//! FastDFS Batch Operations Example
//!
//! This example demonstrates how to perform batch operations with the FastDFS client.
//! It covers efficient patterns for processing multiple files in batches, including
//! progress tracking, error handling, and performance optimization.
//!
//! Key Topics Covered:
//! - Batch upload multiple files
//! - Batch download multiple files
//! - Progress tracking for batches
//! - Error handling in batches
//! - Performance optimization techniques
//! - Bulk operations patterns
//! - Using futures::stream for batch processing
//!
//! Run this example with:
//! ```bash
//! cargo run --example batch_operations_example
//! ```

use fastdfs::{Client, ClientConfig};
use futures::stream::{self, StreamExt};
use std::time::{Duration, Instant};
use tokio::time::sleep;

// ============================================================================
// Main Entry Point
// ============================================================================

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Print header information
    println!("FastDFS Rust Client - Batch Operations Example");
    println!("{}", "=".repeat(50));
    println!();

    // ====================================================================
    // Step 1: Configure and Create Client
    // ====================================================================
    // For batch operations, we may want to configure the client with
    // higher connection limits to handle many concurrent operations.
    
    println!("Initializing FastDFS client...");
    println!();
    
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(50)  // Higher limit for batch operations
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    // Create the client instance
    let client = Client::new(config)?;

    // ====================================================================
    // Example 1: Simple Batch Upload
    // ====================================================================
    // This example demonstrates the most basic batch upload pattern.
    // Multiple files are uploaded concurrently using join_all.
    
    println!("\n1. Simple Batch Upload");
    println!("----------------------");
    println!();
    println!("   This example shows how to upload multiple files in a batch.");
    println!("   All files are uploaded concurrently for maximum efficiency.");
    println!();

    // Prepare file data for batch upload
    // In a real application, this might come from a directory, database, etc.
    let file_data: Vec<(&str, &[u8])> = vec![
        ("file1.txt", b"Content of file 1"),
        ("file2.txt", b"Content of file 2"),
        ("file3.txt", b"Content of file 3"),
        ("file4.txt", b"Content of file 4"),
        ("file5.txt", b"Content of file 5"),
    ];

    println!("   Preparing to upload {} files...", file_data.len());
    println!();

    // Record start time for performance measurement
    let start = Instant::now();

    // Create upload tasks for all files
    // Each file gets its own upload task that will run concurrently
    let upload_tasks: Vec<_> = file_data
        .iter()
        .map(|(name, data)| {
            println!("   → Queuing upload for: {}", name);
            client.upload_buffer(data, "txt", None)
        })
        .collect();

    // Execute all uploads concurrently
    // join_all runs all futures concurrently and waits for all to complete
    let results = futures::future::join_all(upload_tasks).await;

    let elapsed = start.elapsed();

    // Process results
    let mut successful_uploads = Vec::new();
    let mut failed_uploads = Vec::new();

    for (index, result) in results.iter().enumerate() {
        match result {
            Ok(file_id) => {
                successful_uploads.push((index, file_id.clone()));
                println!("   ✓ File {} uploaded: {}", index + 1, file_id);
            }
            Err(e) => {
                failed_uploads.push((index, e));
                println!("   ✗ File {} failed: {}", index + 1, e);
            }
        }
    }

    println!();
    println!("   Batch Upload Summary:");
    println!("   - Total files: {}", file_data.len());
    println!("   - Successful: {}", successful_uploads.len());
    println!("   - Failed: {}", failed_uploads.len());
    println!("   - Total time: {:?}", elapsed);
    println!("   - Average time per file: {:?}", elapsed / file_data.len() as u32);
    println!();

    // Store file IDs for later examples
    let uploaded_file_ids: Vec<String> = successful_uploads
        .into_iter()
        .map(|(_, file_id)| file_id)
        .collect();

    // ====================================================================
    // Example 2: Batch Upload with Progress Tracking
    // ====================================================================
    // Progress tracking is important for batch operations, especially
    // when processing large numbers of files. This example shows how to
    // track progress during batch operations.
    
    println!("\n2. Batch Upload with Progress Tracking");
    println!("---------------------------------------");
    println!();
    println!("   Progress tracking helps users understand batch operation status.");
    println!("   This example demonstrates how to track progress during uploads.");
    println!();

    // Prepare another batch of files
    let batch_size = 10;
    let file_data_batch: Vec<(&str, &[u8])> = (1..=batch_size)
        .map(|i| {
            let name = format!("progress_file_{}.txt", i);
            let content = format!("Content of progress file {}", i);
            (name.as_str(), content.as_bytes())
        })
        .collect();

    println!("   Uploading {} files with progress tracking...", batch_size);
    println!();

    let start = Instant::now();
    let mut completed = 0;
    let mut successful = 0;
    let mut failed = 0;

    // Create upload tasks
    let upload_tasks: Vec<_> = file_data_batch
        .iter()
        .enumerate()
        .map(|(index, (name, data))| {
            let client_ref = &client;
            async move {
                let result = client_ref.upload_buffer(data, "txt", None).await;
                (index + 1, name, result)
            }
        })
        .collect();

    // Process results as they complete
    // Using join_all and then processing results
    let results = futures::future::join_all(upload_tasks).await;

    let mut progress_file_ids = Vec::new();

    for (task_num, name, result) in results {
        completed += 1;
        
        match result {
            Ok(file_id) => {
                successful += 1;
                progress_file_ids.push(file_id.clone());
                println!("   [{}/{}] ✓ {} uploaded: {}", completed, batch_size, name, file_id);
            }
            Err(e) => {
                failed += 1;
                println!("   [{}/{}] ✗ {} failed: {}", completed, batch_size, name, e);
            }
        }

        // Calculate and display progress percentage
        let progress = (completed as f64 / batch_size as f64) * 100.0;
        println!("   Progress: {:.1}% ({} completed, {} successful, {} failed)", 
                 progress, completed, successful, failed);
        println!();
    }

    let elapsed = start.elapsed();

    println!("   Final Summary:");
    println!("   - Total: {}", batch_size);
    println!("   - Successful: {}", successful);
    println!("   - Failed: {}", failed);
    println!("   - Total time: {:?}", elapsed);
    println!("   - Throughput: {:.2} files/second", 
             batch_size as f64 / elapsed.as_secs_f64());
    println!();

    // Clean up progress test files
    println!("   Cleaning up progress test files...");
    for file_id in &progress_file_ids {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 3: Batch Download
    // ====================================================================
    // Batch download is useful when you need to retrieve multiple files.
    // This example shows how to download multiple files efficiently.
    
    println!("\n3. Batch Download");
    println!("----------------");
    println!();
    println!("   Batch download allows retrieving multiple files concurrently.");
    println!("   This is efficient when you need to process multiple files.");
    println!();

    // Use the file IDs from the first example
    if uploaded_file_ids.is_empty() {
        println!("   No files available for download test.");
        println!("   → Skipping batch download example");
    } else {
        println!("   Downloading {} files...", uploaded_file_ids.len());
        println!();

        let start = Instant::now();

        // Create download tasks for all files
        let download_tasks: Vec<_> = uploaded_file_ids
            .iter()
            .enumerate()
            .map(|(index, file_id)| {
                println!("   → Queuing download for file {}", index + 1);
                client.download_file(file_id)
            })
            .collect();

        // Execute all downloads concurrently
        let results = futures::future::join_all(download_tasks).await;

        let elapsed = start.elapsed();

        // Process download results
        let mut successful_downloads = 0;
        let mut total_bytes = 0;

        for (index, result) in results.iter().enumerate() {
            match result {
                Ok(data) => {
                    successful_downloads += 1;
                    total_bytes += data.len();
                    println!("   ✓ File {} downloaded: {} bytes", index + 1, data.len());
                }
                Err(e) => {
                    println!("   ✗ File {} download failed: {}", index + 1, e);
                }
            }
        }

        println!();
        println!("   Batch Download Summary:");
        println!("   - Total files: {}", uploaded_file_ids.len());
        println!("   - Successful: {}", successful_downloads);
        println!("   - Total bytes: {} ({:.2} KB)", total_bytes, total_bytes as f64 / 1024.0);
        println!("   - Total time: {:?}", elapsed);
        println!("   - Download speed: {:.2} KB/s", 
                 (total_bytes as f64 / 1024.0) / elapsed.as_secs_f64());
        println!();
    }

    // ====================================================================
    // Example 4: Batch Operations with Error Handling
    // ====================================================================
    // Error handling in batch operations is crucial. Some files may fail
    // while others succeed. This example shows how to handle errors gracefully
    // and continue processing the remaining files.
    
    println!("\n4. Batch Operations with Error Handling");
    println!("----------------------------------------");
    println!();
    println!("   Batch operations may have partial failures.");
    println!("   This example demonstrates robust error handling.");
    println!();

    // Prepare a mix of valid and potentially invalid operations
    let mixed_operations: Vec<(&str, Result<Vec<u8>, String>)> = vec![
        ("valid_file_1.txt", Ok(b"Valid content 1".to_vec())),
        ("valid_file_2.txt", Ok(b"Valid content 2".to_vec())),
        ("valid_file_3.txt", Ok(b"Valid content 3".to_vec())),
    ];

    println!("   Processing batch with error handling...");
    println!();

    let start = Instant::now();
    let mut upload_tasks = Vec::new();

    // Create upload tasks
    for (name, content_result) in mixed_operations {
        match content_result {
            Ok(content) => {
                println!("   → Queuing upload for: {}", name);
                upload_tasks.push((name, Some(client.upload_buffer(&content, "txt", None))));
            }
            Err(e) => {
                println!("   → Skipping {}: {}", name, e);
                upload_tasks.push((name, None));
            }
        }
    }

    // Execute uploads
    let mut results = Vec::new();
    for (name, task_opt) in upload_tasks {
        match task_opt {
            Some(task) => {
                match task.await {
                    Ok(file_id) => {
                        println!("   ✓ {} uploaded: {}", name, file_id);
                        results.push(Ok((name, file_id)));
                    }
                    Err(e) => {
                        println!("   ✗ {} failed: {}", name, e);
                        results.push(Err((name, e.to_string())));
                    }
                }
            }
            None => {
                println!("   ⊘ {} skipped", name);
                results.push(Err((name, "Skipped".to_string())));
            }
        }
    }

    let elapsed = start.elapsed();

    // Analyze results
    let successful: Vec<_> = results.iter()
        .filter_map(|r| r.as_ref().ok())
        .collect();
    let failed: Vec<_> = results.iter()
        .filter_map(|r| r.as_ref().err())
        .collect();

    println!();
    println!("   Error Handling Summary:");
    println!("   - Total operations: {}", results.len());
    println!("   - Successful: {}", successful.len());
    println!("   - Failed/Skipped: {}", failed.len());
    println!("   - Total time: {:?}", elapsed);
    println!();
    println!("   → Errors were handled gracefully");
    println!("   → Successful operations were not affected by failures");
    println!("   → Processing continued despite individual failures");
    println!();

    // Clean up successful uploads
    println!("   Cleaning up successful uploads...");
    for (_, file_id) in successful {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 5: Batch Processing with Streams
    // ====================================================================
    // Using futures::stream allows for more control over batch processing,
    // including backpressure handling and streaming results. This example
    // demonstrates stream-based batch processing.
    
    println!("\n5. Batch Processing with Streams");
    println!("---------------------------------");
    println!();
    println!("   Streams provide more control over batch processing.");
    println!("   They allow processing items as they complete, with backpressure.");
    println!();

    // Prepare file data for stream processing
    let stream_files: Vec<(&str, &[u8])> = (1..=8)
        .map(|i| {
            let name = format!("stream_file_{}.txt", i);
            let content = format!("Stream file content {}", i);
            (name.as_str(), content.as_bytes())
        })
        .collect();

    println!("   Processing {} files using streams...", stream_files.len());
    println!();

    let start = Instant::now();

    // Create a stream of upload operations
    // buffer_unordered allows controlling concurrency level
    let upload_stream = stream::iter(stream_files.iter())
        .map(|(name, data)| {
            let client_ref = &client;
            async move {
                let result = client_ref.upload_buffer(data, "txt", None).await;
                (name, result)
            }
        })
        .buffer_unordered(4);  // Process 4 files concurrently

    // Process results as they complete
    let mut stream_results = Vec::new();
    let mut completed_count = 0;

    upload_stream
        .for_each(|(name, result)| {
            completed_count += 1;
            match &result {
                Ok(file_id) => {
                    println!("   [{}/{}] ✓ {} uploaded: {}", 
                            completed_count, stream_files.len(), name, file_id);
                    stream_results.push(result);
                }
                Err(e) => {
                    println!("   [{}/{}] ✗ {} failed: {}", 
                            completed_count, stream_files.len(), name, e);
                    stream_results.push(result);
                }
            }
            futures::future::ready(())
        })
        .await;

    let elapsed = start.elapsed();

    // Analyze stream results
    let stream_successful: Vec<_> = stream_results.iter()
        .filter_map(|r| r.as_ref().ok())
        .collect();

    println!();
    println!("   Stream Processing Summary:");
    println!("   - Total files: {}", stream_files.len());
    println!("   - Successful: {}", stream_successful.len());
    println!("   - Total time: {:?}", elapsed);
    println!("   - Concurrency level: 4 (controlled by buffer_unordered)");
    println!();
    println!("   → Streams allow processing results as they complete");
    println!("   → buffer_unordered controls concurrency level");
    println!("   → Useful for large batches with memory constraints");
    println!();

    // Clean up stream test files
    println!("   Cleaning up stream test files...");
    for result in stream_results {
        if let Ok(file_id) = result {
            let _ = client.delete_file(&file_id).await;
        }
    }

    // ====================================================================
    // Example 6: Performance Optimization - Chunked Batch Processing
    // ====================================================================
    // For very large batches, processing all files at once may not be optimal.
    // Chunked processing allows processing files in smaller batches, which
    // can be more memory-efficient and provide better progress tracking.
    
    println!("\n6. Performance Optimization - Chunked Batch Processing");
    println!("------------------------------------------------------");
    println!();
    println!("   Chunked processing is useful for very large batches.");
    println!("   It processes files in smaller chunks for better resource management.");
    println!();

    // Prepare a larger batch
    let large_batch_size = 20;
    let chunk_size = 5;  // Process 5 files at a time

    println!("   Processing {} files in chunks of {}...", large_batch_size, chunk_size);
    println!();

    let start = Instant::now();
    let mut all_file_ids = Vec::new();
    let mut total_successful = 0;
    let mut total_failed = 0;

    // Process in chunks
    for chunk_start in (0..large_batch_size).step_by(chunk_size) {
        let chunk_end = (chunk_start + chunk_size).min(large_batch_size);
        let chunk_num = (chunk_start / chunk_size) + 1;
        let total_chunks = (large_batch_size + chunk_size - 1) / chunk_size;

        println!("   Processing chunk {}/{} (files {}-{})...", 
                chunk_num, total_chunks, chunk_start + 1, chunk_end);

        // Create tasks for this chunk
        let chunk_tasks: Vec<_> = (chunk_start..chunk_end)
            .map(|i| {
                let data = format!("Chunk file {}", i + 1);
                client.upload_buffer(data.as_bytes(), "txt", None)
            })
            .collect();

        // Process chunk
        let chunk_results = futures::future::join_all(chunk_tasks).await;

        // Process chunk results
        for (index, result) in chunk_results.iter().enumerate() {
            match result {
                Ok(file_id) => {
                    total_successful += 1;
                    all_file_ids.push(file_id.clone());
                    println!("     ✓ File {} uploaded", chunk_start + index + 1);
                }
                Err(e) => {
                    total_failed += 1;
                    println!("     ✗ File {} failed: {}", chunk_start + index + 1, e);
                }
            }
        }

        println!("   Chunk {}/{} completed ({} successful, {} failed)", 
                chunk_num, total_chunks, 
                chunk_results.iter().filter(|r| r.is_ok()).count(),
                chunk_results.iter().filter(|r| r.is_err()).count());
        println!();

        // Small delay between chunks (optional, for demonstration)
        if chunk_end < large_batch_size {
            sleep(Duration::from_millis(100)).await;
        }
    }

    let elapsed = start.elapsed();

    println!("   Chunked Processing Summary:");
    println!("   - Total files: {}", large_batch_size);
    println!("   - Successful: {}", total_successful);
    println!("   - Failed: {}", total_failed);
    println!("   - Chunk size: {}", chunk_size);
    println!("   - Total chunks: {}", (large_batch_size + chunk_size - 1) / chunk_size);
    println!("   - Total time: {:?}", elapsed);
    println!();
    println!("   → Chunked processing provides better resource control");
    println!("   → Memory usage is more predictable");
    println!("   → Progress tracking is more granular");
    println!("   → Can handle very large batches efficiently");
    println!();

    // Clean up chunked test files
    println!("   Cleaning up chunked test files...");
    for file_id in &all_file_ids {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 7: Bulk Operations Pattern
    // ====================================================================
    // Bulk operations pattern is useful when you need to perform the same
    // operation on many items. This example shows a reusable pattern for
    // bulk operations with progress tracking and error handling.
    
    println!("\n7. Bulk Operations Pattern");
    println!("--------------------------");
    println!();
    println!("   Bulk operations pattern provides a reusable approach");
    println!("   for processing large numbers of items efficiently.");
    println!();

    // Define a bulk operation function
    async fn bulk_upload(
        client: &Client,
        files: &[(&str, &[u8])],
    ) -> (Vec<String>, Vec<(usize, String)>) {
        let mut successful = Vec::new();
        let mut failed = Vec::new();

        println!("   Starting bulk upload of {} files...", files.len());

        let tasks: Vec<_> = files
            .iter()
            .enumerate()
            .map(|(index, (_, data))| {
                (index, client.upload_buffer(data, "txt", None))
            })
            .collect();

        let results = futures::future::join_all(tasks).await;

        for (original_index, result) in results {
            match result {
                Ok(file_id) => {
                    successful.push(file_id);
                    println!("   ✓ File {} uploaded", original_index + 1);
                }
                Err(e) => {
                    failed.push((original_index, e.to_string()));
                    println!("   ✗ File {} failed: {}", original_index + 1, e);
                }
            }
        }

        (successful, failed)
    }

    // Use the bulk operation function
    let bulk_files: Vec<(&str, &[u8])> = (1..=6)
        .map(|i| {
            let name = format!("bulk_file_{}.txt", i);
            let content = format!("Bulk file content {}", i);
            (name.as_str(), content.as_bytes())
        })
        .collect();

    println!("   Using bulk operations pattern...");
    println!();

    let start = Instant::now();
    let (bulk_successful, bulk_failed) = bulk_upload(&client, &bulk_files).await;
    let elapsed = start.elapsed();

    println!();
    println!("   Bulk Operations Summary:");
    println!("   - Total files: {}", bulk_files.len());
    println!("   - Successful: {}", bulk_successful.len());
    println!("   - Failed: {}", bulk_failed.len());
    println!("   - Total time: {:?}", elapsed);
    println!();
    println!("   → Bulk operations pattern is reusable");
    println!("   → Provides consistent error handling");
    println!("   → Easy to extend for different operation types");
    println!();

    // Clean up bulk test files
    println!("   Cleaning up bulk test files...");
    for file_id in &bulk_successful {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 8: Batch Delete Operations
    // ====================================================================
    // Batch delete is useful for cleanup operations. This example shows
    // how to efficiently delete multiple files in a batch.
    
    println!("\n8. Batch Delete Operations");
    println!("--------------------------");
    println!();
    println!("   Batch delete allows efficient cleanup of multiple files.");
    println!("   This is useful for maintenance and cleanup operations.");
    println!();

    // First, upload some files to delete
    println!("   Preparing files for batch delete test...");
    
    let delete_test_files: Vec<&[u8]> = (1..=5)
        .map(|i| {
            format!("Delete test file {}", i).as_bytes()
        })
        .collect();

    let upload_results: Vec<_> = delete_test_files
        .iter()
        .map(|data| client.upload_buffer(data, "txt", None))
        .collect();

    let upload_results = futures::future::join_all(upload_results).await;

    let files_to_delete: Vec<String> = upload_results
        .into_iter()
        .filter_map(|r| r.ok())
        .collect();

    println!("   ✓ {} files uploaded for delete test", files_to_delete.len());
    println!();
    println!("   Deleting {} files in batch...", files_to_delete.len());
    println!();

    let start = Instant::now();

    // Create delete tasks
    let delete_tasks: Vec<_> = files_to_delete
        .iter()
        .map(|file_id| client.delete_file(file_id))
        .collect();

    // Execute all deletes concurrently
    let delete_results = futures::future::join_all(delete_tasks).await;

    let elapsed = start.elapsed();

    // Process delete results
    let mut successful_deletes = 0;
    let mut failed_deletes = 0;

    for (index, result) in delete_results.iter().enumerate() {
        match result {
            Ok(_) => {
                successful_deletes += 1;
                println!("   ✓ File {} deleted", index + 1);
            }
            Err(e) => {
                failed_deletes += 1;
                println!("   ✗ File {} delete failed: {}", index + 1, e);
            }
        }
    }

    println!();
    println!("   Batch Delete Summary:");
    println!("   - Total files: {}", files_to_delete.len());
    println!("   - Successful: {}", successful_deletes);
    println!("   - Failed: {}", failed_deletes);
    println!("   - Total time: {:?}", elapsed);
    println!();

    // ====================================================================
    // Summary and Key Takeaways
    // ====================================================================
    
    println!("\n{}", "=".repeat(50));
    println!("Batch operations example completed!");
    println!("{}", "=".repeat(50));
    println!();

    println!("Key Takeaways:");
    println!();
    println!("  • Use join_all for simple batch operations");
    println!("    → Simple and efficient for small to medium batches");
    println!("    → All operations run concurrently");
    println!();
    println!("  • Implement progress tracking for user feedback");
    println!("    → Important for long-running batch operations");
    println!("    → Helps users understand operation status");
    println!();
    println!("  • Handle errors gracefully in batch operations");
    println!("    → Some operations may fail while others succeed");
    println!("    → Continue processing remaining items");
    println!("    → Report both successes and failures");
    println!();
    println!("  • Use streams for large batches with backpressure");
    println!("    → buffer_unordered controls concurrency level");
    println!("    → Processes results as they complete");
    println!("    → More memory-efficient for very large batches");
    println!();
    println!("  • Consider chunked processing for very large batches");
    println!("    → Better resource management");
    println!("    → More predictable memory usage");
    println!("    → Better progress tracking");
    println!();
    println!("  • Create reusable bulk operation patterns");
    println!("    → Consistent error handling");
    println!("    → Easy to extend and maintain");
    println!("    → Reusable across different operation types");
    println!();
    println!("  • Batch operations significantly improve performance");
    println!("    → Concurrent execution reduces total time");
    println!("    → Connection pool efficiently manages connections");
    println!("    → Throughput increases with batch size (up to limits)");
    println!();

    // Close the client
    println!("Closing client and releasing resources...");
    client.close().await;
    println!("Client closed.");
    println!();

    Ok(())
}

