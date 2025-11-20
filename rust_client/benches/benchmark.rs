//! Performance benchmarks for FastDFS Rust client
//!
//! This benchmark suite measures the performance of various client operations
//! including uploads, downloads, and metadata operations under different conditions.
//!
//! Run benchmarks with:
//! ```bash
//! cargo bench
//! ```

use criterion::{black_box, criterion_group, criterion_main, Criterion, BenchmarkId};
use fastdfs::{Client, ClientConfig};
use std::collections::HashMap;

/// Benchmark for uploading small files (< 1KB)
///
/// This benchmark measures the throughput of uploading small files,
/// which is a common use case for thumbnail images, icons, or small documents.
fn bench_upload_small_file(c: &mut Criterion) {
    // Create runtime for async operations
    let rt = tokio::runtime::Runtime::new().unwrap();

    // Set up client (skip if no tracker available)
    let tracker_addr = std::env::var("FASTDFS_TRACKER_ADDR")
        .unwrap_or_else(|_| "127.0.0.1:22122".to_string());
    let config = ClientConfig::new(vec![tracker_addr]);
    let client = Client::new(config).unwrap();

    // Create test data - 512 bytes
    let test_data = vec![0u8; 512];

    c.bench_function("upload_small_file_512b", |b| {
        b.to_async(&rt).iter(|| async {
            // Upload the file
            let file_id = client
                .upload_buffer(black_box(&test_data), "bin", None)
                .await
                .unwrap();

            // Clean up - delete the file
            client.delete_file(&file_id).await.ok();
        });
    });

    // Clean up
    rt.block_on(client.close());
}

/// Benchmark for uploading medium files (1KB - 100KB)
///
/// This benchmark measures the throughput of uploading medium-sized files,
/// typical for documents, small images, or configuration files.
fn bench_upload_medium_file(c: &mut Criterion) {
    // Create runtime for async operations
    let rt = tokio::runtime::Runtime::new().unwrap();

    // Set up client
    let tracker_addr = std::env::var("FASTDFS_TRACKER_ADDR")
        .unwrap_or_else(|_| "127.0.0.1:22122".to_string());
    let config = ClientConfig::new(vec![tracker_addr]);
    let client = Client::new(config).unwrap();

    // Test different file sizes
    let sizes = vec![1024, 10240, 102400]; // 1KB, 10KB, 100KB

    for size in sizes {
        let test_data = vec![0u8; size];

        c.bench_with_input(
            BenchmarkId::new("upload_medium_file", size),
            &test_data,
            |b, data| {
                b.to_async(&rt).iter(|| async {
                    // Upload the file
                    let file_id = client
                        .upload_buffer(black_box(data), "bin", None)
                        .await
                        .unwrap();

                    // Clean up
                    client.delete_file(&file_id).await.ok();
                });
            },
        );
    }

    // Clean up
    rt.block_on(client.close());
}

/// Benchmark for downloading files
///
/// This benchmark measures the throughput of downloading files of various sizes,
/// which is critical for read-heavy workloads.
fn bench_download_file(c: &mut Criterion) {
    // Create runtime for async operations
    let rt = tokio::runtime::Runtime::new().unwrap();

    // Set up client
    let tracker_addr = std::env::var("FASTDFS_TRACKER_ADDR")
        .unwrap_or_else(|_| "127.0.0.1:22122".to_string());
    let config = ClientConfig::new(vec![tracker_addr]);
    let client = Client::new(config).unwrap();

    // Upload a test file first
    let test_data = vec![0u8; 10240]; // 10KB
    let file_id = rt
        .block_on(client.upload_buffer(&test_data, "bin", None))
        .unwrap();

    c.bench_function("download_file_10kb", |b| {
        b.to_async(&rt).iter(|| async {
            // Download the file
            let _data = client.download_file(black_box(&file_id)).await.unwrap();
        });
    });

    // Clean up
    rt.block_on(client.delete_file(&file_id)).ok();
    rt.block_on(client.close());
}

/// Benchmark for metadata operations
///
/// This benchmark measures the performance of setting and getting metadata,
/// which is important for applications that heavily use file attributes.
fn bench_metadata_operations(c: &mut Criterion) {
    // Create runtime for async operations
    let rt = tokio::runtime::Runtime::new().unwrap();

    // Set up client
    let tracker_addr = std::env::var("FASTDFS_TRACKER_ADDR")
        .unwrap_or_else(|_| "127.0.0.1:22122".to_string());
    let config = ClientConfig::new(vec![tracker_addr]);
    let client = Client::new(config).unwrap();

    // Upload a test file
    let test_data = b"Test file for metadata benchmarks";
    let file_id = rt
        .block_on(client.upload_buffer(test_data, "txt", None))
        .unwrap();

    // Create test metadata
    let mut metadata = HashMap::new();
    metadata.insert("author".to_string(), "Benchmark User".to_string());
    metadata.insert("date".to_string(), "2025-01-15".to_string());

    // Benchmark setting metadata
    c.bench_function("set_metadata", |b| {
        b.to_async(&rt).iter(|| async {
            client
                .set_metadata(
                    black_box(&file_id),
                    black_box(&metadata),
                    MetadataFlag::Overwrite,
                )
                .await
                .unwrap();
        });
    });

    // Benchmark getting metadata
    c.bench_function("get_metadata", |b| {
        b.to_async(&rt).iter(|| async {
            let _meta = client.get_metadata(black_box(&file_id)).await.unwrap();
        });
    });

    // Clean up
    rt.block_on(client.delete_file(&file_id)).ok();
    rt.block_on(client.close());
}

/// Benchmark for concurrent operations
///
/// This benchmark measures the client's performance when handling
/// multiple concurrent upload operations, testing the connection pool
/// and async runtime efficiency.
fn bench_concurrent_uploads(c: &mut Criterion) {
    // Create runtime for async operations
    let rt = tokio::runtime::Runtime::new().unwrap();

    // Set up client
    let tracker_addr = std::env::var("FASTDFS_TRACKER_ADDR")
        .unwrap_or_else(|_| "127.0.0.1:22122".to_string());
    let config = ClientConfig::new(vec![tracker_addr]).with_max_conns(20);
    let client = Client::new(config).unwrap();

    // Create test data
    let test_data = vec![0u8; 1024]; // 1KB

    c.bench_function("concurrent_uploads_10", |b| {
        b.to_async(&rt).iter(|| async {
            // Launch 10 concurrent uploads
            let mut handles = vec![];
            for _ in 0..10 {
                let client_ref = &client;
                let data_ref = &test_data;
                let handle = tokio::spawn(async move {
                    client_ref.upload_buffer(data_ref, "bin", None).await
                });
                handles.push(handle);
            }

            // Wait for all uploads to complete
            let file_ids: Vec<_> = futures::future::join_all(handles)
                .await
                .into_iter()
                .filter_map(|r| r.ok())
                .filter_map(|r| r.ok())
                .collect();

            // Clean up all uploaded files
            for file_id in file_ids {
                client.delete_file(&file_id).await.ok();
            }
        });
    });

    // Clean up
    rt.block_on(client.close());
}

// Register all benchmark functions
criterion_group!(
    benches,
    bench_upload_small_file,
    bench_upload_medium_file,
    bench_download_file,
    bench_metadata_operations,
    bench_concurrent_uploads
);

// Main entry point for criterion
criterion_main!(benches);