/*! FastDFS Performance Benchmarking Example
 *
 * This comprehensive example demonstrates performance benchmarking, optimization
 * techniques, connection pool tuning, batch operations, memory usage patterns,
 * and performance metrics collection for the FastDFS Rust client.
 *
 * Key Topics Covered:
 * - Performance benchmarking and measurement
 * - Optimization techniques for throughput and latency
 * - Connection pool tuning and sizing
 * - Batch operation patterns for efficiency
 * - Memory usage patterns and optimization
 * - Using criterion for benchmarks
 * - Performance metrics collection and analysis
 *
 * Run this example with:
 * ```bash
 * cargo run --example performance_example
 * ```
 *
 * For detailed criterion benchmarks, run:
 * ```bash
 * cargo bench
 * ```
 */

use fastdfs::{Client, ClientConfig};
use std::time::{Duration, Instant};
use std::sync::Arc;
use tokio::time::sleep;

// ============================================================================
// SECTION 1: Performance Metrics Collection
// ============================================================================

/// Performance metrics for a single operation
#[derive(Debug, Clone)]
pub struct OperationMetrics {
    /// Operation name
    pub operation: String,
    /// Duration of the operation
    pub duration: Duration,
    /// Number of bytes processed (if applicable)
    pub bytes_processed: Option<usize>,
    /// Success status
    pub success: bool,
    /// Error message if failed
    pub error: Option<String>,
}

/// Aggregated performance statistics
#[derive(Debug, Clone)]
pub struct PerformanceStats {
    /// Operation name
    pub operation: String,
    /// Total number of operations
    pub count: usize,
    /// Total duration
    pub total_duration: Duration,
    /// Average duration per operation
    pub avg_duration: Duration,
    /// Minimum duration
    pub min_duration: Duration,
    /// Maximum duration
    pub max_duration: Duration,
    /// Total bytes processed
    pub total_bytes: usize,
    /// Operations per second (throughput)
    pub ops_per_second: f64,
    /// Throughput in MB/s
    pub throughput_mbps: f64,
    /// Success rate (0.0 to 1.0)
    pub success_rate: f64,
}

impl PerformanceStats {
    /// Calculate statistics from a collection of metrics
    pub fn from_metrics(metrics: &[OperationMetrics]) -> Self {
        if metrics.is_empty() {
            return PerformanceStats {
                operation: "unknown".to_string(),
                count: 0,
                total_duration: Duration::ZERO,
                avg_duration: Duration::ZERO,
                min_duration: Duration::ZERO,
                max_duration: Duration::ZERO,
                total_bytes: 0,
                ops_per_second: 0.0,
                throughput_mbps: 0.0,
                success_rate: 0.0,
            };
        }

        let count = metrics.len();
        let total_duration: Duration = metrics.iter().map(|m| m.duration).sum();
        let avg_duration = total_duration / count as u32;
        
        let durations: Vec<Duration> = metrics.iter().map(|m| m.duration).collect();
        let min_duration = *durations.iter().min().unwrap();
        let max_duration = *durations.iter().max().unwrap();
        
        let total_bytes: usize = metrics.iter()
            .filter_map(|m| m.bytes_processed)
            .sum();
        
        let success_count = metrics.iter().filter(|m| m.success).count();
        let success_rate = success_count as f64 / count as f64;
        
        let total_secs = total_duration.as_secs_f64();
        let ops_per_second = if total_secs > 0.0 {
            count as f64 / total_secs
        } else {
            0.0
        };
        
        let throughput_mbps = if total_secs > 0.0 {
            (total_bytes as f64 / (1024.0 * 1024.0)) / total_secs
        } else {
            0.0
        };

        PerformanceStats {
            operation: metrics[0].operation.clone(),
            count,
            total_duration,
            avg_duration,
            min_duration,
            max_duration,
            total_bytes,
            ops_per_second,
            throughput_mbps,
            success_rate,
        }
    }

    /// Print formatted statistics
    pub fn print(&self) {
        println!("\n{} Performance Statistics", self.operation);
        println!("{}", "=".repeat(60));
        println!("  Operations:           {}", self.count);
        println!("  Total Duration:       {:.2?}", self.total_duration);
        println!("  Average Duration:     {:.2?}", self.avg_duration);
        println!("  Min Duration:         {:.2?}", self.min_duration);
        println!("  Max Duration:         {:.2?}", self.max_duration);
        println!("  Throughput:           {:.2} ops/sec", self.ops_per_second);
        if self.total_bytes > 0 {
            println!("  Total Bytes:          {} bytes ({:.2} MB)", 
                     self.total_bytes, self.total_bytes as f64 / (1024.0 * 1024.0));
            println!("  Data Throughput:      {:.2} MB/s", self.throughput_mbps);
        }
        println!("  Success Rate:         {:.1}%", self.success_rate * 100.0);
        println!();
    }
}

// ============================================================================
// SECTION 2: Connection Pool Tuning
// ============================================================================

/// Test different connection pool configurations
async fn benchmark_connection_pool_sizes(
    tracker_addr: &str,
    file_size: usize,
    num_operations: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n2. Connection Pool Tuning");
    println!("{}", "-".repeat(70));
    println!("Testing different connection pool sizes...");
    println!("File size: {} bytes, Operations: {}", file_size, num_operations);
    println!();

    let pool_sizes = vec![1, 5, 10, 20, 50];
    let test_data = vec![0u8; file_size];

    for max_conns in pool_sizes {
        let config = ClientConfig::new(vec![tracker_addr.to_string()])
            .with_max_conns(max_conns)
            .with_connect_timeout(5000)
            .with_network_timeout(30000);
        
        let client = Client::new(config)?;
        let start = Instant::now();
        let mut metrics = Vec::new();

        // Perform concurrent operations
        let mut handles = Vec::new();
        for i in 0..num_operations {
            let client_ref = &client;
            let data = test_data.clone();
            let handle = tokio::spawn(async move {
                let op_start = Instant::now();
                let result = client_ref.upload_buffer(&data, "bin", None).await;
                let duration = op_start.elapsed();
                
                match result {
                    Ok(file_id) => {
                        let _ = client_ref.delete_file(&file_id).await;
                        OperationMetrics {
                            operation: format!("upload_pool_{}", max_conns),
                            duration,
                            bytes_processed: Some(data.len()),
                            success: true,
                            error: None,
                        }
                    }
                    Err(e) => OperationMetrics {
                        operation: format!("upload_pool_{}", max_conns),
                        duration,
                        bytes_processed: Some(data.len()),
                        success: false,
                        error: Some(e.to_string()),
                    },
                }
            });
            handles.push(handle);
        }

        // Collect results
        for handle in handles {
            if let Ok(metric) = handle.await {
                metrics.push(metric);
            }
        }

        let stats = PerformanceStats::from_metrics(&metrics);
        let total_time = start.elapsed();
        
        println!("  Max Connections: {}", max_conns);
        println!("    Total Time: {:.2?}", total_time);
        println!("    Avg Duration: {:.2?}", stats.avg_duration);
        println!("    Throughput: {:.2} ops/sec", stats.ops_per_second);
        println!("    Success Rate: {:.1}%", stats.success_rate * 100.0);
        println!();

        client.close().await;
    }

    Ok(())
}

// ============================================================================
// SECTION 3: Batch Operation Patterns
// ============================================================================

/// Benchmark batch upload operations
async fn benchmark_batch_operations(
    client: &Client,
    file_size: usize,
    batch_sizes: &[usize],
) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n3. Batch Operation Patterns");
    println!("{}", "-".repeat(70));
    println!("Testing different batch sizes for upload operations...");
    println!("File size: {} bytes", file_size);
    println!();

    let test_data = vec![0u8; file_size];

    for &batch_size in batch_sizes {
        let start = Instant::now();
        let mut metrics = Vec::new();

        // Perform batch operations
        for _ in 0..batch_size {
            let op_start = Instant::now();
            match client.upload_buffer(&test_data, "bin", None).await {
                Ok(file_id) => {
                    let duration = op_start.elapsed();
                    metrics.push(OperationMetrics {
                        operation: "batch_upload".to_string(),
                        duration,
                        bytes_processed: Some(test_data.len()),
                        success: true,
                        error: None,
                    });
                    // Clean up
                    let _ = client.delete_file(&file_id).await;
                }
                Err(e) => {
                    let duration = op_start.elapsed();
                    metrics.push(OperationMetrics {
                        operation: "batch_upload".to_string(),
                        duration,
                        bytes_processed: Some(test_data.len()),
                        success: false,
                        error: Some(e.to_string()),
                    });
                }
            }
        }

        let stats = PerformanceStats::from_metrics(&metrics);
        let total_time = start.elapsed();
        
        println!("  Batch Size: {}", batch_size);
        println!("    Total Time: {:.2?}", total_time);
        println!("    Avg Duration: {:.2?}", stats.avg_duration);
        println!("    Throughput: {:.2} ops/sec", stats.ops_per_second);
        println!("    Data Throughput: {:.2} MB/s", stats.throughput_mbps);
        println!();
    }

    Ok(())
}

/// Benchmark concurrent batch operations
async fn benchmark_concurrent_batch(
    client: &Client,
    file_size: usize,
    batch_size: usize,
    concurrency: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n4. Concurrent Batch Operations");
    println!("{}", "-".repeat(70));
    println!("Testing concurrent batch operations...");
    println!("File size: {} bytes, Batch size: {}, Concurrency: {}", 
             file_size, batch_size, concurrency);
    println!();

    let test_data = vec![0u8; file_size];
    let start = Instant::now();
    let mut all_metrics = Vec::new();

    // Create concurrent batches
    let mut handles = Vec::new();
    for _ in 0..concurrency {
        let client_ref = &client;
        let data = test_data.clone();
        let handle = tokio::spawn(async move {
            let mut batch_metrics = Vec::new();
            for _ in 0..batch_size {
                let op_start = Instant::now();
                match client_ref.upload_buffer(&data, "bin", None).await {
                    Ok(file_id) => {
                        let duration = op_start.elapsed();
                        batch_metrics.push(OperationMetrics {
                            operation: "concurrent_batch".to_string(),
                            duration,
                            bytes_processed: Some(data.len()),
                            success: true,
                            error: None,
                        });
                        let _ = client_ref.delete_file(&file_id).await;
                    }
                    Err(e) => {
                        let duration = op_start.elapsed();
                        batch_metrics.push(OperationMetrics {
                            operation: "concurrent_batch".to_string(),
                            duration,
                            bytes_processed: Some(data.len()),
                            success: false,
                            error: Some(e.to_string()),
                        });
                    }
                }
            }
            batch_metrics
        });
        handles.push(handle);
    }

    // Collect all metrics
    for handle in handles {
        if let Ok(mut batch_metrics) = handle.await {
            all_metrics.append(&mut batch_metrics);
        }
    }

    let stats = PerformanceStats::from_metrics(&all_metrics);
    let total_time = start.elapsed();
    
    println!("  Total Operations: {}", stats.count);
    println!("  Total Time: {:.2?}", total_time);
    println!("  Avg Duration: {:.2?}", stats.avg_duration);
    println!("  Throughput: {:.2} ops/sec", stats.ops_per_second);
    println!("  Data Throughput: {:.2} MB/s", stats.throughput_mbps);
    println!("  Success Rate: {:.1}%", stats.success_rate * 100.0);
    println!();

    Ok(())
}

// ============================================================================
// SECTION 4: Memory Usage Patterns
// ============================================================================

/// Benchmark memory usage patterns with different file sizes
async fn benchmark_memory_patterns(
    client: &Client,
    file_sizes: &[usize],
    num_operations: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n5. Memory Usage Patterns");
    println!("{}", "-".repeat(70));
    println!("Testing memory usage with different file sizes...");
    println!("Operations per size: {}", num_operations);
    println!();

    for &file_size in file_sizes {
        let test_data = vec![0u8; file_size];
        let start = Instant::now();
        let mut metrics = Vec::new();

        for _ in 0..num_operations {
            let op_start = Instant::now();
            match client.upload_buffer(&test_data, "bin", None).await {
                Ok(file_id) => {
                    let duration = op_start.elapsed();
                    metrics.push(OperationMetrics {
                        operation: format!("memory_test_{}b", file_size),
                        duration,
                        bytes_processed: Some(file_size),
                        success: true,
                        error: None,
                    });
                    let _ = client.delete_file(&file_id).await;
                }
                Err(e) => {
                    let duration = op_start.elapsed();
                    metrics.push(OperationMetrics {
                        operation: format!("memory_test_{}b", file_size),
                        duration,
                        bytes_processed: Some(file_size),
                        success: false,
                        error: Some(e.to_string()),
                    });
                }
            }
        }

        let stats = PerformanceStats::from_metrics(&metrics);
        let total_time = start.elapsed();
        
        let size_mb = file_size as f64 / (1024.0 * 1024.0);
        println!("  File Size: {} bytes ({:.2} MB)", file_size, size_mb);
        println!("    Total Time: {:.2?}", total_time);
        println!("    Avg Duration: {:.2?}", stats.avg_duration);
        println!("    Throughput: {:.2} ops/sec", stats.ops_per_second);
        println!("    Data Throughput: {:.2} MB/s", stats.throughput_mbps);
        println!();
    }

    Ok(())
}

// ============================================================================
// SECTION 5: Optimization Techniques
// ============================================================================

/// Compare sequential vs concurrent operations
async fn benchmark_optimization_techniques(
    client: &Client,
    file_size: usize,
    num_operations: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n6. Optimization Techniques: Sequential vs Concurrent");
    println!("{}", "-".repeat(70));
    println!("File size: {} bytes, Operations: {}", file_size, num_operations);
    println!();

    let test_data = vec![0u8; file_size];

    // Sequential operations
    println!("  Sequential Operations:");
    let start = Instant::now();
    let mut metrics = Vec::new();
    
    for _ in 0..num_operations {
        let op_start = Instant::now();
        match client.upload_buffer(&test_data, "bin", None).await {
            Ok(file_id) => {
                let duration = op_start.elapsed();
                metrics.push(OperationMetrics {
                    operation: "sequential".to_string(),
                    duration,
                    bytes_processed: Some(file_size),
                    success: true,
                    error: None,
                });
                let _ = client.delete_file(&file_id).await;
            }
            Err(e) => {
                let duration = op_start.elapsed();
                metrics.push(OperationMetrics {
                    operation: "sequential".to_string(),
                    duration,
                    bytes_processed: Some(file_size),
                    success: false,
                    error: Some(e.to_string()),
                });
            }
        }
    }
    
    let seq_stats = PerformanceStats::from_metrics(&metrics);
    let seq_time = start.elapsed();
    println!("    Total Time: {:.2?}", seq_time);
    println!("    Throughput: {:.2} ops/sec", seq_stats.ops_per_second);
    println!();

    // Concurrent operations
    println!("  Concurrent Operations:");
    let start = Instant::now();
    let mut handles = Vec::new();
    
    for _ in 0..num_operations {
        let client_ref = &client;
        let data = test_data.clone();
        let handle = tokio::spawn(async move {
            let op_start = Instant::now();
            let result = client_ref.upload_buffer(&data, "bin", None).await;
            let duration = op_start.elapsed();
            
            match result {
                Ok(file_id) => {
                    let _ = client_ref.delete_file(&file_id).await;
                    OperationMetrics {
                        operation: "concurrent".to_string(),
                        duration,
                        bytes_processed: Some(data.len()),
                        success: true,
                        error: None,
                    }
                }
                Err(e) => OperationMetrics {
                    operation: "concurrent".to_string(),
                    duration,
                    bytes_processed: Some(data.len()),
                    success: false,
                    error: Some(e.to_string()),
                },
            }
        });
        handles.push(handle);
    }

    let mut metrics = Vec::new();
    for handle in handles {
        if let Ok(metric) = handle.await {
            metrics.push(metric);
        }
    }

    let conc_stats = PerformanceStats::from_metrics(&metrics);
    let conc_time = start.elapsed();
    println!("    Total Time: {:.2?}", conc_time);
    println!("    Throughput: {:.2} ops/sec", conc_stats.ops_per_second);
    println!();

    // Comparison
    let speedup = seq_time.as_secs_f64() / conc_time.as_secs_f64();
    println!("  Performance Improvement:");
    println!("    Speedup: {:.2}x", speedup);
    println!("    Time Saved: {:.2?}", seq_time.saturating_sub(conc_time));
    println!();

    Ok(())
}

// ============================================================================
// SECTION 6: Criterion Benchmark Integration
// ============================================================================

/// Demonstrate how to use criterion for benchmarks
/// This shows the pattern for creating criterion benchmarks
fn demonstrate_criterion_usage() {
    println!("\n7. Using Criterion for Benchmarks");
    println!("{}", "-".repeat(70));
    println!("Criterion is a statistical benchmarking framework for Rust.");
    println!("It provides detailed performance analysis with statistical significance.");
    println!();
    println!("Example criterion benchmark code:");
    println!("```rust");
    println!("use criterion::{{black_box, criterion_group, criterion_main, Criterion}};");
    println!("");
    println!("fn bench_upload_small_file(c: &mut Criterion) {{");
    println!("    let rt = tokio::runtime::Runtime::new().unwrap();");
    println!("    let config = ClientConfig::new(vec![\"127.0.0.1:22122\".to_string()]);");
    println!("    let client = Client::new(config).unwrap();");
    println!("    let test_data = vec![0u8; 1024];");
    println!("    ");
    println!("    c.bench_function(\"upload_1kb\", |b| {{");
    println!("        b.to_async(&rt).iter(|| async {{");
    println!("            let file_id = client");
    println!("                .upload_buffer(black_box(&test_data), \"bin\", None)");
    println!("                .await");
    println!("                .unwrap();");
    println!("            client.delete_file(&file_id).await.ok();");
    println!("        }});");
    println!("    }});");
    println!("    ");
    println!("    rt.block_on(client.close());");
    println!("}}");
    println!("");
    println!("criterion_group!(benches, bench_upload_small_file);");
    println!("criterion_main!(benches);");
    println!("```");
    println!();
    println!("Run criterion benchmarks with:");
    println!("  cargo bench");
    println!();
    println!("Criterion provides:");
    println!("  - Statistical analysis of performance");
    println!("  - Detection of performance regressions");
    println!("  - HTML reports with graphs");
    println!("  - Comparison between benchmark runs");
    println!();
}

// ============================================================================
// Main Entry Point
// ============================================================================

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Performance Benchmarking Example");
    println!("{}", "=".repeat(70));
    println!();

    // Configuration
    let tracker_addr = std::env::var("FASTDFS_TRACKER_ADDR")
        .unwrap_or_else(|_| "192.168.1.100:22122".to_string());
    
    println!("Tracker address: {}", tracker_addr);
    println!("Note: Adjust FASTDFS_TRACKER_ADDR environment variable if needed");
    println!();

    // ====================================================================
    // Example 1: Basic Performance Measurement
    // ====================================================================
    println!("1. Basic Performance Measurement");
    println!("{}", "-".repeat(70));
    
    let config = ClientConfig::new(vec![tracker_addr.clone()])
        .with_max_conns(20)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    let client = Client::new(config)?;
    
    let test_data = b"Performance test data for benchmarking";
    let num_iterations = 10;
    let mut metrics = Vec::new();

    println!("Running {} upload operations...", num_iterations);
    
    for i in 0..num_iterations {
        let start = Instant::now();
        match client.upload_buffer(test_data, "txt", None).await {
            Ok(file_id) => {
                let duration = start.elapsed();
                metrics.push(OperationMetrics {
                    operation: "upload".to_string(),
                    duration,
                    bytes_processed: Some(test_data.len()),
                    success: true,
                    error: None,
                });
                let _ = client.delete_file(&file_id).await;
                println!("  Operation {}: {:.2?}", i + 1, duration);
            }
            Err(e) => {
                let duration = start.elapsed();
                metrics.push(OperationMetrics {
                    operation: "upload".to_string(),
                    duration,
                    bytes_processed: Some(test_data.len()),
                    success: false,
                    error: Some(e.to_string()),
                });
                eprintln!("  Operation {} failed: {}", i + 1, e);
            }
        }
    }

    let stats = PerformanceStats::from_metrics(&metrics);
    stats.print();

    // ====================================================================
    // Connection Pool Tuning
    // ====================================================================
    benchmark_connection_pool_sizes(&tracker_addr, 1024, 20).await?;

    // ====================================================================
    // Batch Operations
    // ====================================================================
    let batch_sizes = vec![5, 10, 20, 50];
    benchmark_batch_operations(&client, 1024, &batch_sizes).await?;

    // ====================================================================
    // Concurrent Batch Operations
    // ====================================================================
    benchmark_concurrent_batch(&client, 1024, 10, 5).await?;

    // ====================================================================
    // Memory Usage Patterns
    // ====================================================================
    let file_sizes = vec![1024, 10 * 1024, 100 * 1024, 1024 * 1024]; // 1KB, 10KB, 100KB, 1MB
    benchmark_memory_patterns(&client, &file_sizes, 5).await?;

    // ====================================================================
    // Optimization Techniques
    // ====================================================================
    benchmark_optimization_techniques(&client, 1024, 20).await?;

    // ====================================================================
    // Criterion Usage
    // ====================================================================
    demonstrate_criterion_usage();

    // ====================================================================
    // Summary and Recommendations
    // ====================================================================
    println!("\n8. Performance Optimization Recommendations");
    println!("{}", "-".repeat(70));
    println!("Based on the benchmarks above, consider:");
    println!();
    println!("1. Connection Pool Sizing:");
    println!("   - Start with 10-20 connections per server");
    println!("   - Increase for high-concurrency workloads");
    println!("   - Monitor connection pool utilization");
    println!();
    println!("2. Batch Operations:");
    println!("   - Use batch operations for multiple files");
    println!("   - Process batches concurrently when possible");
    println!("   - Balance batch size with memory constraints");
    println!();
    println!("3. Concurrent Operations:");
    println!("   - Use concurrent operations for better throughput");
    println!("   - Match concurrency to connection pool size");
    println!("   - Consider async/await patterns for I/O-bound tasks");
    println!();
    println!("4. Memory Management:");
    println!("   - Stream large files instead of loading into memory");
    println!("   - Reuse buffers when possible");
    println!("   - Monitor memory usage patterns");
    println!();
    println!("5. Performance Monitoring:");
    println!("   - Collect metrics for all operations");
    println!("   - Track throughput, latency, and error rates");
    println!("   - Use criterion for statistical benchmarking");
    println!("   - Set up performance regression tests");
    println!();

    // Cleanup
    client.close().await;
    
    println!("{}", "=".repeat(70));
    println!("Performance benchmarking example completed!");
    println!();
    println!("For detailed statistical benchmarks, run:");
    println!("  cargo bench");
    println!();

    Ok(())
}

