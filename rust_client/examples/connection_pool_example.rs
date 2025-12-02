//! FastDFS Connection Pool Example
//!
//! This example demonstrates connection pool management with the FastDFS client.
//! It covers configuration, monitoring, performance impact, and best practices
//! for managing connections efficiently in production applications.
//!
//! Key Topics Covered:
//! - Connection pool configuration
//! - Connection reuse patterns
//! - Pool monitoring and metrics
//! - Performance impact analysis
//! - Best practices for pool management
//! - Connection lifecycle management
//! - Pool size tuning and optimization
//!
//! Run this example with:
//! ```bash
//! cargo run --example connection_pool_example
//! ```

use fastdfs::{Client, ClientConfig};
use std::time::{Duration, Instant};
use tokio::time::sleep;

/// Main entry point for the connection pool example
/// 
/// This function demonstrates various aspects of connection pool management
/// including configuration, monitoring, and performance optimization.
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Connection Pool Example");
    println!("{}", "=".repeat(50));
    println!();

    /// Example 1: Basic Connection Pool Configuration
    /// 
    /// This example shows how to configure the connection pool with different
    /// settings and explains the impact of each configuration option.
    println!("\n1. Basic Connection Pool Configuration");
    println!("---------------------------------------");
    println!();
    println!("   Connection pool configuration affects performance and resource usage.");
    println!("   This example demonstrates different configuration options.");
    println!();

    /// Configuration Option 1: Small Connection Pool
    /// 
    /// A small connection pool uses fewer resources but may limit concurrency.
    /// Suitable for applications with low to moderate traffic.
    println!("   Configuration 1: Small Connection Pool");
    println!("   → max_conns: 10");
    println!("   → Suitable for: Low to moderate traffic");
    println!("   → Resource usage: Low");
    println!("   → Concurrency limit: Moderate");
    println!();

    let small_pool_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let small_pool_client = Client::new(small_pool_config)?;

    /// Test the small connection pool with a few operations
    /// 
    /// This demonstrates how a small pool handles concurrent operations.
    /// Operations may queue if the pool is exhausted.
    println!("   Testing small pool with 5 concurrent operations...");
    let start = Instant::now();
    
    let small_pool_tasks: Vec<_> = (0..5)
        .map(|i| {
            let data = format!("Small pool test {}", i);
            small_pool_client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    let small_results = futures::future::join_all(small_pool_tasks).await;
    let small_elapsed = start.elapsed();

    let small_successful: Vec<_> = small_results.iter()
        .filter_map(|r| r.as_ref().ok())
        .collect();

    println!("   → Completed in: {:?}", small_elapsed);
    println!("   → Successful: {}/5", small_successful.len());
    println!();

    /// Clean up test files from small pool
    for result in small_results {
        if let Ok(file_id) = result {
            let _ = small_pool_client.delete_file(&file_id).await;
        }
    }

    /// Configuration Option 2: Medium Connection Pool
    /// 
    /// A medium connection pool balances resource usage and concurrency.
    /// Suitable for most production applications.
    println!("   Configuration 2: Medium Connection Pool");
    println!("   → max_conns: 50");
    println!("   → Suitable for: Most production applications");
    println!("   → Resource usage: Moderate");
    println!("   → Concurrency limit: High");
    println!();

    let medium_pool_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(50)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let medium_pool_client = Client::new(medium_pool_config)?;

    /// Test the medium connection pool
    /// 
    /// This demonstrates better concurrency with a larger pool.
    println!("   Testing medium pool with 20 concurrent operations...");
    let start = Instant::now();
    
    let medium_pool_tasks: Vec<_> = (0..20)
        .map(|i| {
            let data = format!("Medium pool test {}", i);
            medium_pool_client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    let medium_results = futures::future::join_all(medium_pool_tasks).await;
    let medium_elapsed = start.elapsed();

    let medium_successful: Vec<_> = medium_results.iter()
        .filter_map(|r| r.as_ref().ok())
        .collect();

    println!("   → Completed in: {:?}", medium_elapsed);
    println!("   → Successful: {}/20", medium_successful.len());
    println!();

    /// Clean up test files from medium pool
    for result in medium_results {
        if let Ok(file_id) = result {
            let _ = medium_pool_client.delete_file(&file_id).await;
        }
    }

    /// Configuration Option 3: Large Connection Pool
    /// 
    /// A large connection pool maximizes concurrency but uses more resources.
    /// Suitable for high-traffic applications or batch processing.
    println!("   Configuration 3: Large Connection Pool");
    println!("   → max_conns: 100");
    println!("   → Suitable for: High-traffic applications, batch processing");
    println!("   → Resource usage: High");
    println!("   → Concurrency limit: Very high");
    println!();

    let large_pool_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(100)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let large_pool_client = Client::new(large_pool_config)?;

    /// Test the large connection pool
    /// 
    /// This demonstrates maximum concurrency with a large pool.
    println!("   Testing large pool with 50 concurrent operations...");
    let start = Instant::now();
    
    let large_pool_tasks: Vec<_> = (0..50)
        .map(|i| {
            let data = format!("Large pool test {}", i);
            large_pool_client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    let large_results = futures::future::join_all(large_pool_tasks).await;
    let large_elapsed = start.elapsed();

    let large_successful: Vec<_> = large_results.iter()
        .filter_map(|r| r.as_ref().ok())
        .collect();

    println!("   → Completed in: {:?}", large_elapsed);
    println!("   → Successful: {}/50", large_successful.len());
    println!();

    /// Clean up test files from large pool
    for result in large_results {
        if let Ok(file_id) = result {
            let _ = large_pool_client.delete_file(&file_id).await;
        }
    }

    /// Close all test clients
    small_pool_client.close().await;
    medium_pool_client.close().await;
    large_pool_client.close().await;

    /// Example 2: Connection Reuse Patterns
    /// 
    /// This example demonstrates how the connection pool reuses connections
    /// across multiple operations, reducing overhead and improving performance.
    println!("\n2. Connection Reuse Patterns");
    println!("---------------------------");
    println!();
    println!("   Connection reuse is key to pool efficiency.");
    println!("   This example shows how connections are reused across operations.");
    println!();

    /// Create a client for reuse pattern testing
    /// 
    /// We'll use a moderate pool size to observe reuse behavior.
    let reuse_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let reuse_client = Client::new(reuse_config)?;

    /// Pattern 1: Sequential Operations Reusing Connections
    /// 
    /// Sequential operations can reuse the same connection from the pool.
    /// This is efficient because connections don't need to be re-established.
    println!("   Pattern 1: Sequential Operations");
    println!("   → Multiple operations reuse the same connection");
    println!("   → No connection overhead between operations");
    println!();

    let sequential_start = Instant::now();

    /// Perform multiple sequential operations
    /// 
    /// Each operation may reuse a connection from the pool, avoiding
    /// the overhead of establishing new connections.
    for i in 0..5 {
        let data = format!("Sequential operation {}", i);
        match reuse_client.upload_buffer(data.as_bytes(), "txt", None).await {
            Ok(file_id) => {
                println!("   → Operation {} completed, connection reused", i + 1);
                let _ = reuse_client.delete_file(&file_id).await;
            }
            Err(e) => {
                println!("   → Operation {} failed: {}", i + 1, e);
            }
        }
    }

    let sequential_elapsed = sequential_start.elapsed();
    println!("   → Sequential operations completed in: {:?}", sequential_elapsed);
    println!();

    /// Pattern 2: Concurrent Operations Sharing Pool
    /// 
    /// Concurrent operations share the connection pool, with connections
    /// being reused across different operations as they complete.
    println!("   Pattern 2: Concurrent Operations");
    println!("   → Multiple operations share the connection pool");
    println!("   → Connections are reused as operations complete");
    println!();

    let concurrent_start = Instant::now();

    /// Perform concurrent operations that share the pool
    /// 
    /// The pool manages connections efficiently, reusing them across
    /// different concurrent operations.
    let concurrent_tasks: Vec<_> = (0..10)
        .map(|i| {
            let data = format!("Concurrent operation {}", i);
            reuse_client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    let concurrent_results = futures::future::join_all(concurrent_tasks).await;
    let concurrent_elapsed = concurrent_start.elapsed();

    let concurrent_successful: Vec<_> = concurrent_results.iter()
        .filter_map(|r| r.as_ref().ok())
        .collect();

    println!("   → Concurrent operations completed in: {:?}", concurrent_elapsed);
    println!("   → Successful: {}/10", concurrent_successful.len());
    println!("   → Pool efficiently managed connections across operations");
    println!();

    /// Clean up concurrent test files
    for result in concurrent_results {
        if let Ok(file_id) = result {
            let _ = reuse_client.delete_file(&file_id).await;
        }
    }

    reuse_client.close().await;

    /// Example 3: Pool Monitoring and Metrics
    /// 
    /// This example demonstrates how to monitor connection pool behavior
    /// and gather metrics for performance analysis and tuning.
    println!("\n3. Pool Monitoring and Metrics");
    println!("-----------------------------");
    println!();
    println!("   Monitoring pool behavior helps optimize configuration.");
    println!("   This example shows how to measure pool performance.");
    println!();

    /// Create a client for monitoring
    /// 
    /// We'll use a known pool size to observe behavior.
    let monitor_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(20)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let monitor_client = Client::new(monitor_config)?;

    /// Metric 1: Operation Throughput
    /// 
    /// Measure how many operations can be completed per second.
    /// This helps understand pool capacity and efficiency.
    println!("   Metric 1: Operation Throughput");
    println!("   → Measuring operations per second");
    println!();

    let throughput_ops = 30;
    let throughput_start = Instant::now();

    let throughput_tasks: Vec<_> = (0..throughput_ops)
        .map(|i| {
            let data = format!("Throughput test {}", i);
            monitor_client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    let throughput_results = futures::future::join_all(throughput_tasks).await;
    let throughput_elapsed = throughput_start.elapsed();

    let throughput_successful = throughput_results.iter()
        .filter(|r| r.is_ok())
        .count();

    let ops_per_second = throughput_successful as f64 / throughput_elapsed.as_secs_f64();

    println!("   → Operations: {}", throughput_ops);
    println!("   → Successful: {}", throughput_successful);
    println!("   → Time: {:?}", throughput_elapsed);
    println!("   → Throughput: {:.2} ops/second", ops_per_second);
    println!();

    /// Clean up throughput test files
    for result in throughput_results {
        if let Ok(file_id) = result {
            let _ = monitor_client.delete_file(&file_id).await;
        }
    }

    /// Metric 2: Average Operation Time
    /// 
    /// Measure average time per operation to understand pool efficiency.
    /// Lower times indicate better connection reuse.
    println!("   Metric 2: Average Operation Time");
    println!("   → Measuring average time per operation");
    println!();

    let avg_ops = 15;
    let mut operation_times = Vec::new();

    for i in 0..avg_ops {
        let op_start = Instant::now();
        let data = format!("Avg time test {}", i);
        
        match monitor_client.upload_buffer(data.as_bytes(), "txt", None).await {
            Ok(file_id) => {
                let op_elapsed = op_start.elapsed();
                operation_times.push(op_elapsed);
                println!("   → Operation {}: {:?}", i + 1, op_elapsed);
                let _ = monitor_client.delete_file(&file_id).await;
            }
            Err(e) => {
                println!("   → Operation {} failed: {}", i + 1, e);
            }
        }
    }

    if !operation_times.is_empty() {
        let total_time: Duration = operation_times.iter().sum();
        let avg_time = total_time / operation_times.len() as u32;
        
        println!();
        println!("   → Total operations: {}", operation_times.len());
        println!("   → Average time: {:?}", avg_time);
        println!("   → Total time: {:?}", total_time);
        println!();
    }

    /// Metric 3: Pool Utilization Under Load
    /// 
    /// Measure how the pool performs under different load levels.
    /// This helps identify optimal pool size.
    println!("   Metric 3: Pool Utilization Under Load");
    println!("   → Testing pool with different load levels");
    println!();

    /// Test with load equal to pool size
    /// 
    /// When load equals pool size, all connections should be utilized.
    let load_levels = vec![10, 20, 30, 40];

    for load in load_levels {
        println!("   → Testing with {} concurrent operations (pool size: 20)", load);
        
        let load_start = Instant::now();
        let load_tasks: Vec<_> = (0..load)
            .map(|i| {
                let data = format!("Load test {} - {}", load, i);
                monitor_client.upload_buffer(data.as_bytes(), "txt", None)
            })
            .collect();

        let load_results = futures::future::join_all(load_tasks).await;
        let load_elapsed = load_start.elapsed();

        let load_successful = load_results.iter()
            .filter(|r| r.is_ok())
            .count();

        println!("     → Completed in: {:?}", load_elapsed);
        println!("     → Successful: {}/{}", load_successful, load);
        println!("     → Throughput: {:.2} ops/second", 
                 load_successful as f64 / load_elapsed.as_secs_f64());
        println!();

        /// Clean up load test files
        for result in load_results {
            if let Ok(file_id) = result {
                let _ = monitor_client.delete_file(&file_id).await;
            }
        }
    }

    monitor_client.close().await;

    /// Example 4: Performance Impact Analysis
    /// 
    /// This example compares performance with different pool configurations
    /// to demonstrate the impact of pool size on overall performance.
    println!("\n4. Performance Impact Analysis");
    println!("-----------------------------");
    println!();
    println!("   Pool size significantly affects performance.");
    println!("   This example compares different pool configurations.");
    println!();

    /// Test Configuration: Small Pool
    /// 
    /// Small pools may limit concurrency but use fewer resources.
    let perf_small_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(5)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let perf_small_client = Client::new(perf_small_config)?;

    println!("   Testing Small Pool (max_conns: 5)...");
    let perf_small_start = Instant::now();

    let perf_small_tasks: Vec<_> = (0..20)
        .map(|i| {
            let data = format!("Perf small {}", i);
            perf_small_client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    let perf_small_results = futures::future::join_all(perf_small_tasks).await;
    let perf_small_elapsed = perf_small_start.elapsed();

    let perf_small_successful = perf_small_results.iter()
        .filter(|r| r.is_ok())
        .count();

    println!("   → Time: {:?}", perf_small_elapsed);
    println!("   → Successful: {}/20", perf_small_successful);
    println!("   → Throughput: {:.2} ops/second", 
             perf_small_successful as f64 / perf_small_elapsed.as_secs_f64());
    println!();

    /// Clean up small pool test files
    for result in perf_small_results {
        if let Ok(file_id) = result {
            let _ = perf_small_client.delete_file(&file_id).await;
        }
    }

    /// Test Configuration: Large Pool
    /// 
    /// Large pools allow higher concurrency but use more resources.
    let perf_large_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(50)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let perf_large_client = Client::new(perf_large_config)?;

    println!("   Testing Large Pool (max_conns: 50)...");
    let perf_large_start = Instant::now();

    let perf_large_tasks: Vec<_> = (0..20)
        .map(|i| {
            let data = format!("Perf large {}", i);
            perf_large_client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    let perf_large_results = futures::future::join_all(perf_large_tasks).await;
    let perf_large_elapsed = perf_large_start.elapsed();

    let perf_large_successful = perf_large_results.iter()
        .filter(|r| r.is_ok())
        .count();

    println!("   → Time: {:?}", perf_large_elapsed);
    println!("   → Successful: {}/20", perf_large_successful);
    println!("   → Throughput: {:.2} ops/second", 
             perf_large_successful as f64 / perf_large_elapsed.as_secs_f64());
    println!();

    /// Performance Comparison
    /// 
    /// Compare the results to understand the impact of pool size.
    println!("   Performance Comparison:");
    if perf_small_elapsed > perf_large_elapsed {
        let speedup = perf_small_elapsed.as_secs_f64() / perf_large_elapsed.as_secs_f64();
        println!("   → Large pool is {:.2}x faster", speedup);
        println!("   → Time saved: {:?}", perf_small_elapsed - perf_large_elapsed);
    } else {
        println!("   → Small pool performed better (unusual for concurrent ops)");
    }
    println!();

    /// Clean up large pool test files
    for result in perf_large_results {
        if let Ok(file_id) = result {
            let _ = perf_large_client.delete_file(&file_id).await;
        }
    }

    perf_small_client.close().await;
    perf_large_client.close().await;

    /// Example 5: Best Practices for Pool Management
    /// 
    /// This example demonstrates best practices for configuring and managing
    /// connection pools in production applications.
    println!("\n5. Best Practices for Pool Management");
    println!("-----------------------------------");
    println!();
    println!("   Following best practices ensures optimal pool performance.");
    println!();

    /// Best Practice 1: Right-Size Your Pool
    /// 
    /// Pool size should match your application's concurrency needs.
    /// Too small: operations queue, poor performance
    /// Too large: wasted resources, no performance gain
    println!("   Best Practice 1: Right-Size Your Pool");
    println!("   → Match pool size to expected concurrency");
    println!("   → Too small: operations queue unnecessarily");
    println!("   → Too large: wastes resources without benefit");
    println!("   → Recommended: Start with 20-50, tune based on metrics");
    println!();

    /// Best Practice 2: Monitor Pool Metrics
    /// 
    /// Regular monitoring helps identify when pool tuning is needed.
    /// Track throughput, average operation time, and error rates.
    println!("   Best Practice 2: Monitor Pool Metrics");
    println!("   → Track throughput (operations per second)");
    println!("   → Monitor average operation time");
    println!("   → Watch for connection timeouts or errors");
    println!("   → Adjust pool size based on metrics");
    println!();

    /// Best Practice 3: Use Appropriate Timeouts
    /// 
    /// Timeouts should be set based on network conditions and operation types.
    /// Too short: unnecessary failures
    /// Too long: operations hang, wasting resources
    println!("   Best Practice 3: Use Appropriate Timeouts");
    println!("   → Connect timeout: 5-10 seconds (connection establishment)");
    println!("   → Network timeout: 30-60 seconds (I/O operations)");
    println!("   → Adjust based on network latency and file sizes");
    println!();

    /// Best Practice 4: Handle Pool Exhaustion
    /// 
    /// When pool is exhausted, operations may need to wait or fail.
    /// Implement appropriate retry logic or error handling.
    println!("   Best Practice 4: Handle Pool Exhaustion");
    println!("   → Implement retry logic for transient failures");
    println!("   → Consider increasing pool size if exhaustion is frequent");
    println!("   → Use exponential backoff for retries");
    println!();

    /// Best Practice 5: Clean Up Resources
    /// 
    /// Always close clients properly to release pool resources.
    /// Use RAII patterns or explicit cleanup in async contexts.
    println!("   Best Practice 5: Clean Up Resources");
    println!("   → Always call client.close().await when done");
    println!("   → Use RAII patterns where possible");
    println!("   → Ensure cleanup in error paths");
    println!();

    /// Example 6: Connection Lifecycle Management
    /// 
    /// This example demonstrates how connections are created, reused,
    /// and cleaned up throughout their lifecycle.
    println!("\n6. Connection Lifecycle Management");
    println!("---------------------------------");
    println!();
    println!("   Understanding connection lifecycle helps optimize pool usage.");
    println!();

    /// Create a client for lifecycle demonstration
    /// 
    /// We'll observe how connections are managed throughout operations.
    let lifecycle_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(5)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let lifecycle_client = Client::new(lifecycle_config)?;

    /// Lifecycle Stage 1: Connection Creation
    /// 
    /// Connections are created on-demand when needed and added to the pool.
    /// Initial connection creation has overhead.
    println!("   Lifecycle Stage 1: Connection Creation");
    println!("   → Connections created on-demand when pool has capacity");
    println!("   → Initial creation has connection establishment overhead");
    println!("   → Subsequent operations reuse existing connections");
    println!();

    /// Lifecycle Stage 2: Connection Reuse
    /// 
    /// Once created, connections are reused across multiple operations.
    /// This avoids the overhead of creating new connections.
    println!("   Lifecycle Stage 2: Connection Reuse");
    println!("   → Connections are reused for multiple operations");
    println!("   → Reuse avoids connection establishment overhead");
    println!("   → Pool manages connection availability");
    println!();

    /// Demonstrate connection reuse with multiple operations
    /// 
    /// Multiple operations will reuse connections from the pool.
    for i in 0..10 {
        let data = format!("Lifecycle test {}", i);
        match lifecycle_client.upload_buffer(data.as_bytes(), "txt", None).await {
            Ok(file_id) => {
                println!("   → Operation {}: Connection reused from pool", i + 1);
                let _ = lifecycle_client.delete_file(&file_id).await;
            }
            Err(e) => {
                println!("   → Operation {} failed: {}", i + 1, e);
            }
        }
    }

    println!();

    /// Lifecycle Stage 3: Connection Cleanup
    /// 
    /// Connections are cleaned up when the client is closed or when
    /// they become idle for too long (if idle timeout is configured).
    println!("   Lifecycle Stage 3: Connection Cleanup");
    println!("   → Connections closed when client.close() is called");
    println!("   → Idle connections may be closed after idle timeout");
    println!("   → Cleanup releases network resources");
    println!();

    lifecycle_client.close().await;
    println!("   → Client closed, all connections cleaned up");
    println!();

    /// Example 7: Pool Size Tuning
    /// 
    /// This example demonstrates how to tune pool size based on
    /// application requirements and performance characteristics.
    println!("\n7. Pool Size Tuning");
    println!("------------------");
    println!();
    println!("   Tuning pool size is crucial for optimal performance.");
    println!("   This example shows how to find the right pool size.");
    println!();

    /// Tuning Strategy: Test Different Pool Sizes
    /// 
    /// Test different pool sizes with your typical workload to find
    /// the optimal configuration.
    let tuning_sizes = vec![5, 10, 20, 30, 50];
    let tuning_ops = 25;

    println!("   Testing different pool sizes with {} operations each...", tuning_ops);
    println!();

    for pool_size in tuning_sizes {
        let tuning_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
            .with_max_conns(pool_size)
            .with_connect_timeout(5000)
            .with_network_timeout(30000);

        let tuning_client = Client::new(tuning_config)?;

        println!("   → Testing pool size: {}", pool_size);
        let tuning_start = Instant::now();

        let tuning_tasks: Vec<_> = (0..tuning_ops)
            .map(|i| {
                let data = format!("Tuning test {} - pool {}", i, pool_size);
                tuning_client.upload_buffer(data.as_bytes(), "txt", None)
            })
            .collect();

        let tuning_results = futures::future::join_all(tuning_tasks).await;
        let tuning_elapsed = tuning_start.elapsed();

        let tuning_successful = tuning_results.iter()
            .filter(|r| r.is_ok())
            .count();

        let tuning_throughput = tuning_successful as f64 / tuning_elapsed.as_secs_f64();

        println!("     → Time: {:?}", tuning_elapsed);
        println!("     → Successful: {}/{}", tuning_successful, tuning_ops);
        println!("     → Throughput: {:.2} ops/second", tuning_throughput);
        println!();

        /// Clean up tuning test files
        for result in tuning_results {
            if let Ok(file_id) = result {
                let _ = tuning_client.delete_file(&file_id).await;
            }
        }

        tuning_client.close().await;
    }

    /// Tuning Recommendations
    /// 
    /// Based on testing, provide recommendations for pool sizing.
    println!("   Tuning Recommendations:");
    println!("   → Start with pool size equal to expected concurrent operations");
    println!("   → Test with your typical workload");
    println!("   → Increase if operations are queuing");
    println!("   → Decrease if throughput doesn't improve");
    println!("   → Monitor resource usage (memory, file descriptors)");
    println!("   → Consider server capacity when setting pool size");
    println!();

    /// Summary and Key Takeaways
    /// 
    /// Provide a comprehensive summary of connection pool management.
    println!("\n{}", "=".repeat(50));
    println!("Connection pool example completed!");
    println!("{}", "=".repeat(50));
    println!();

    println!("Key Takeaways:");
    println!();
    println!("  • Connection pool size significantly affects performance");
    println!("    → Too small: operations queue, poor throughput");
    println!("    → Too large: wastes resources without benefit");
    println!("    → Right size: optimal balance of performance and resources");
    println!();
    println!("  • Connection reuse is key to efficiency");
    println!("    → Reusing connections avoids establishment overhead");
    println!("    → Pool manages connection availability automatically");
    println!("    → Sequential and concurrent operations both benefit");
    println!();
    println!("  • Monitor pool metrics for optimization");
    println!("    → Track throughput (operations per second)");
    println!("    → Measure average operation time");
    println!("    → Watch for connection timeouts or errors");
    println!("    → Use metrics to tune pool size");
    println!();
    println!("  • Follow best practices for production");
    println!("    → Right-size pool for your workload");
    println!("    → Set appropriate timeouts");
    println!("    → Handle pool exhaustion gracefully");
    println!("    → Always clean up resources");
    println!();
    println!("  • Tune pool size based on testing");
    println!("    → Test different sizes with your workload");
    println!("    → Find the sweet spot for performance");
    println!("    → Consider server capacity and resources");
    println!("    → Monitor and adjust as workload changes");
    println!();

    Ok(())
}

