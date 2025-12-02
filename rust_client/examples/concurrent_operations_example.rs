//! FastDFS Concurrent Operations Example
//!
//! This example demonstrates how to perform concurrent operations with the FastDFS client.
//! It covers various patterns for parallel uploads, downloads, and other operations
//! using Rust's async/await and Tokio runtime.
//!
//! Key Topics Covered:
//! - Concurrent uploads and downloads
//! - Thread-safe client usage
//! - Parallel operations with tokio::join! and futures::future::join_all
//! - Performance comparison between sequential and concurrent operations
//! - Connection pool behavior under load
//! - Multi-threaded scenarios
//! - Using tokio::spawn for concurrent tasks
//!
//! Run this example with:
//! ```bash
//! cargo run --example concurrent_operations_example
//! ```

use fastdfs::{Client, ClientConfig};
use std::time::{Duration, Instant};
use tokio::time::sleep;

// ============================================================================
// Main Entry Point
// ============================================================================

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Print header information
    println!("FastDFS Rust Client - Concurrent Operations Example");
    println!("{}", "=".repeat(50));
    println!();

    // ====================================================================
    // Step 1: Configure and Create Client
    // ====================================================================
    // The client is designed to be thread-safe and can be used concurrently
    // from multiple tasks. The connection pool manages connections efficiently
    // across concurrent operations.
    
    println!("Initializing FastDFS client...");
    println!();
    
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(50)  // Higher connection limit for concurrent operations
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    // Create the client instance
    // This client can be safely shared across multiple async tasks
    let client = Client::new(config)?;

    // ====================================================================
    // Example 1: Concurrent Uploads with tokio::join!
    // ====================================================================
    // tokio::join! allows you to run multiple async operations concurrently
    // and wait for all of them to complete. This is useful when you need
    // to perform multiple independent operations in parallel.
    
    println!("\n1. Concurrent Uploads with tokio::join!");
    println!("----------------------------------------");
    println!();
    println!("   tokio::join! runs multiple futures concurrently");
    println!("   and waits for all of them to complete.");
    println!("   This is ideal for independent operations.");
    println!();

    // Prepare test data for concurrent uploads
    let data1 = b"Concurrent upload file 1";
    let data2 = b"Concurrent upload file 2";
    let data3 = b"Concurrent upload file 3";

    println!("   Uploading 3 files concurrently...");
    println!();

    // Record start time for performance measurement
    let start = Instant::now();

    // Use tokio::join! to run all uploads concurrently
    // All three uploads will start at the same time and run in parallel
    let (result1, result2, result3) = tokio::join!(
        client.upload_buffer(data1, "txt", None),
        client.upload_buffer(data2, "txt", None),
        client.upload_buffer(data3, "txt", None),
    );

    // Calculate elapsed time
    let elapsed = start.elapsed();

    // Handle results
    // Each result needs to be checked individually
    let file_ids = match (result1, result2, result3) {
        (Ok(id1), Ok(id2), Ok(id3)) => {
            println!("   ✓ All 3 files uploaded successfully!");
            println!("   File ID 1: {}", id1);
            println!("   File ID 2: {}", id2);
            println!("   File ID 3: {}", id3);
            println!("   Total time: {:?}", elapsed);
            println!("   → All uploads completed concurrently");
            vec![id1, id2, id3]
        }
        _ => {
            println!("   ✗ Some uploads failed");
            println!("   → Check individual results for details");
            vec![]
        }
    };

    println!();

    // Clean up uploaded files
    println!("   Cleaning up uploaded files...");
    for file_id in &file_ids {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 2: Concurrent Downloads with tokio::join!
    // ====================================================================
    // Similar to concurrent uploads, we can download multiple files
    // concurrently. This is especially useful when downloading files
    // that are independent of each other.
    
    println!("\n2. Concurrent Downloads with tokio::join!");
    println!("-------------------------------------------");
    println!();

    // First, upload some files to download
    println!("   Preparing files for concurrent download...");
    
    let upload_results = tokio::join!(
        client.upload_buffer(b"Download file 1", "txt", None),
        client.upload_buffer(b"Download file 2", "txt", None),
        client.upload_buffer(b"Download file 3", "txt", None),
    );

    let download_file_ids = match upload_results {
        (Ok(id1), Ok(id2), Ok(id3)) => {
            println!("   ✓ Files uploaded for download test");
            vec![id1, id2, id3]
        }
        _ => {
            println!("   ✗ Failed to prepare files");
            return Ok(());
        }
    };

    println!();
    println!("   Downloading 3 files concurrently...");
    println!();

    // Record start time
    let start = Instant::now();

    // Download all files concurrently
    let download_results = tokio::join!(
        client.download_file(&download_file_ids[0]),
        client.download_file(&download_file_ids[1]),
        client.download_file(&download_file_ids[2]),
    );

    let elapsed = start.elapsed();

    // Handle download results
    match download_results {
        (Ok(data1), Ok(data2), Ok(data3)) => {
            println!("   ✓ All 3 files downloaded successfully!");
            println!("   File 1 size: {} bytes", data1.len());
            println!("   File 2 size: {} bytes", data2.len());
            println!("   File 3 size: {} bytes", data3.len());
            println!("   Total time: {:?}", elapsed);
            println!("   → All downloads completed concurrently");
        }
        _ => {
            println!("   ✗ Some downloads failed");
        }
    }

    println!();

    // Clean up
    println!("   Cleaning up test files...");
    for file_id in &download_file_ids {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 3: Parallel Operations with futures::future::join_all
    // ====================================================================
    // join_all is useful when you have a variable number of operations
    // to run concurrently. It takes a vector of futures and runs them all
    // in parallel, returning a vector of results.
    
    println!("\n3. Parallel Operations with futures::future::join_all");
    println!("------------------------------------------------------");
    println!();
    println!("   join_all is useful for variable numbers of operations.");
    println!("   It takes a collection of futures and runs them concurrently.");
    println!();

    // Create a vector of upload operations
    let upload_tasks: Vec<_> = (1..=10)
        .map(|i| {
            let data = format!("Batch upload file {}", i);
            client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    println!("   Uploading {} files in parallel...", upload_tasks.len());
    println!();

    // Record start time
    let start = Instant::now();

    // Use join_all to run all uploads concurrently
    // This will run all 10 uploads at the same time
    let results = futures::future::join_all(upload_tasks).await;

    let elapsed = start.elapsed();

    // Process results
    let mut successful_uploads = 0;
    let mut file_ids_to_cleanup = Vec::new();

    for (index, result) in results.iter().enumerate() {
        match result {
            Ok(file_id) => {
                successful_uploads += 1;
                file_ids_to_cleanup.push(file_id.clone());
                if index < 3 {
                    // Show first 3 file IDs
                    println!("   ✓ File {} uploaded: {}", index + 1, file_id);
                }
            }
            Err(e) => {
                println!("   ✗ File {} failed: {}", index + 1, e);
            }
        }
    }

    println!();
    println!("   Summary:");
    println!("   - Total files: {}", results.len());
    println!("   - Successful: {}", successful_uploads);
    println!("   - Failed: {}", results.len() - successful_uploads);
    println!("   - Total time: {:?}", elapsed);
    println!("   → All operations ran concurrently");

    println!();
    println!("   Cleaning up uploaded files...");
    for file_id in &file_ids_to_cleanup {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 4: Performance Comparison: Sequential vs Concurrent
    // ====================================================================
    // This example demonstrates the performance benefits of concurrent
    // operations compared to sequential operations. Concurrent operations
    // can significantly reduce total execution time, especially for I/O-bound
    // operations like file uploads and downloads.
    
    println!("\n4. Performance Comparison: Sequential vs Concurrent");
    println!("----------------------------------------------------");
    println!();
    println!("   This example compares the performance of sequential");
    println!("   vs concurrent operations to demonstrate the benefits");
    println!("   of parallel execution.");
    println!();

    let num_files = 5;
    let file_data = b"Performance test file";

    // Sequential uploads
    println!("   Sequential uploads (one at a time)...");
    println!();

    let start = Instant::now();
    let mut sequential_file_ids = Vec::new();

    for i in 0..num_files {
        match client.upload_buffer(file_data, "txt", None).await {
            Ok(file_id) => {
                sequential_file_ids.push(file_id);
                println!("   ✓ Uploaded file {}/{}", i + 1, num_files);
            }
            Err(e) => {
                println!("   ✗ Failed to upload file {}: {}", i + 1, e);
            }
        }
    }

    let sequential_time = start.elapsed();
    println!("   Sequential time: {:?}", sequential_time);
    println!();

    // Concurrent uploads
    println!("   Concurrent uploads (all at once)...");
    println!();

    let start = Instant::now();
    let concurrent_tasks: Vec<_> = (0..num_files)
        .map(|_| client.upload_buffer(file_data, "txt", None))
        .collect();

    let concurrent_results = futures::future::join_all(concurrent_tasks).await;
    let concurrent_time = start.elapsed();

    let mut concurrent_file_ids = Vec::new();
    for (index, result) in concurrent_results.iter().enumerate() {
        match result {
            Ok(file_id) => {
                concurrent_file_ids.push(file_id.clone());
                println!("   ✓ Uploaded file {}/{}", index + 1, num_files);
            }
            Err(e) => {
                println!("   ✗ Failed to upload file {}: {}", index + 1, e);
            }
        }
    }

    println!("   Concurrent time: {:?}", concurrent_time);
    println!();

    // Performance comparison
    println!("   Performance Comparison:");
    println!("   - Sequential: {:?}", sequential_time);
    println!("   - Concurrent: {:?}", concurrent_time);
    
    if sequential_time > concurrent_time {
        let speedup = sequential_time.as_secs_f64() / concurrent_time.as_secs_f64();
        println!("   - Speedup: {:.2}x faster with concurrent operations", speedup);
        println!("   - Time saved: {:?}", sequential_time - concurrent_time);
    } else {
        println!("   - Concurrent operations were slower (unusual)");
    }

    println!();
    println!("   → Concurrent operations can significantly reduce total time");
    println!("   → The speedup depends on network latency and server capacity");
    println!("   → Connection pool allows multiple concurrent connections");

    println!();
    println!("   Cleaning up test files...");
    for file_id in sequential_file_ids.iter().chain(concurrent_file_ids.iter()) {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 5: Using tokio::spawn for Background Tasks
    // ====================================================================
    // tokio::spawn creates a new task that runs concurrently with the
    // current task. This is useful for fire-and-forget operations or
    // when you need to run operations in the background.
    
    println!("\n5. Using tokio::spawn for Background Tasks");
    println!("---------------------------------------------");
    println!();
    println!("   tokio::spawn creates independent tasks that run concurrently.");
    println!("   Useful for background operations and fire-and-forget tasks.");
    println!();

    // Spawn multiple background upload tasks
    let num_background_tasks = 5;
    let mut handles = Vec::new();

    println!("   Spawning {} background upload tasks...", num_background_tasks);
    println!();

    for i in 0..num_background_tasks {
        // Clone the client for use in the spawned task
        // The client is designed to be shared across tasks
        let client_clone = &client;
        let data = format!("Background task file {}", i + 1);

        // Spawn a new task
        let handle = tokio::spawn(async move {
            let result = client_clone.upload_buffer(data.as_bytes(), "txt", None).await;
            (i + 1, result)
        });

        handles.push(handle);
    }

    println!("   All tasks spawned, waiting for completion...");
    println!();

    // Wait for all tasks to complete
    let start = Instant::now();
    let mut background_file_ids = Vec::new();

    for handle in handles {
        match handle.await {
            Ok((task_num, Ok(file_id))) => {
                println!("   ✓ Background task {} completed: {}", task_num, file_id);
                background_file_ids.push(file_id);
            }
            Ok((task_num, Err(e))) => {
                println!("   ✗ Background task {} failed: {}", task_num, e);
            }
            Err(e) => {
                println!("   ✗ Background task panicked: {}", e);
            }
        }
    }

    let elapsed = start.elapsed();
    println!();
    println!("   All background tasks completed in: {:?}", elapsed);
    println!("   → Tasks ran concurrently in the background");

    println!();
    println!("   Cleaning up background task files...");
    for file_id in &background_file_ids {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 6: Mixed Concurrent Operations
    // ====================================================================
    // In real applications, you often need to mix different types of
    // operations concurrently. This example shows how to run uploads,
    // downloads, and other operations in parallel.
    
    println!("\n6. Mixed Concurrent Operations");
    println!("-------------------------------");
    println!();
    println!("   Real applications often need to mix different operations.");
    println!("   This example shows uploads, downloads, and metadata operations");
    println!("   running concurrently.");
    println!();

    // First, upload a file to use for mixed operations
    let master_file_id = match client.upload_buffer(b"Master file for mixed ops", "txt", None).await {
        Ok(id) => {
            println!("   ✓ Master file uploaded: {}", id);
            id
        }
        Err(e) => {
            println!("   ✗ Failed to upload master file: {}", e);
            return Ok(());
        }
    };

    println!();
    println!("   Running mixed operations concurrently...");
    println!();

    let start = Instant::now();

    // Run different types of operations concurrently
    let mixed_results = tokio::join!(
        // Upload a new file
        client.upload_buffer(b"New file from mixed ops", "txt", None),
        // Download the master file
        client.download_file(&master_file_id),
        // Get file info
        client.get_file_info(&master_file_id),
        // Check if file exists
        async { client.file_exists(&master_file_id).await },
    );

    let elapsed = start.elapsed();

    // Process mixed results
    match mixed_results {
        (Ok(new_file_id), Ok(downloaded_data), Ok(file_info), exists) => {
            println!("   ✓ All mixed operations completed successfully!");
            println!("   - New file uploaded: {}", new_file_id);
            println!("   - Master file downloaded: {} bytes", downloaded_data.len());
            println!("   - File info retrieved: {} bytes", file_info.file_size);
            println!("   - File exists check: {}", exists);
            println!("   - Total time: {:?}", elapsed);
            println!("   → All operations ran concurrently");

            // Clean up
            println!();
            println!("   Cleaning up test files...");
            let _ = client.delete_file(&new_file_id).await;
            let _ = client.delete_file(&master_file_id).await;
        }
        _ => {
            println!("   ✗ Some mixed operations failed");
            let _ = client.delete_file(&master_file_id).await;
        }
    }

    // ====================================================================
    // Example 7: Connection Pool Behavior Under Load
    // ====================================================================
    // This example demonstrates how the connection pool handles many
    // concurrent operations. The connection pool efficiently manages
    // connections to avoid creating too many connections while still
    // allowing high concurrency.
    
    println!("\n7. Connection Pool Behavior Under Load");
    println!("----------------------------------------");
    println!();
    println!("   This example demonstrates connection pool behavior");
    println!("   when handling many concurrent operations.");
    println!("   The pool efficiently manages connections.");
    println!();

    let num_concurrent_ops = 20;
    println!("   Running {} concurrent operations...", num_concurrent_ops);
    println!("   → This tests the connection pool under load");
    println!();

    let start = Instant::now();

    // Create many concurrent operations
    let load_tasks: Vec<_> = (0..num_concurrent_ops)
        .map(|i| {
            let data = format!("Load test file {}", i);
            client.upload_buffer(data.as_bytes(), "txt", None)
        })
        .collect();

    // Run all operations concurrently
    let load_results = futures::future::join_all(load_tasks).await;

    let elapsed = start.elapsed();

    // Analyze results
    let mut success_count = 0;
    let mut file_ids_for_cleanup = Vec::new();

    for result in load_results {
        match result {
            Ok(file_id) => {
                success_count += 1;
                file_ids_for_cleanup.push(file_id);
            }
            Err(_) => {
                // Count failures
            }
        }
    }

    println!("   Results:");
    println!("   - Total operations: {}", num_concurrent_ops);
    println!("   - Successful: {}", success_count);
    println!("   - Failed: {}", num_concurrent_ops - success_count);
    println!("   - Total time: {:?}", elapsed);
    println!("   - Average time per operation: {:?}", elapsed / num_concurrent_ops as u32);
    println!();
    println!("   → Connection pool handled {} concurrent operations", num_concurrent_ops);
    println!("   → Pool efficiently reused connections");
    println!("   → High concurrency achieved with limited connections");

    println!();
    println!("   Cleaning up load test files...");
    for file_id in &file_ids_for_cleanup {
        let _ = client.delete_file(file_id).await;
    }

    // ====================================================================
    // Example 8: Error Handling in Concurrent Operations
    // ====================================================================
    // When running operations concurrently, it's important to handle
    // errors appropriately. Some operations may succeed while others fail.
    // This example shows how to handle partial failures.
    
    println!("\n8. Error Handling in Concurrent Operations");
    println!("--------------------------------------------");
    println!();
    println!("   Concurrent operations may have partial failures.");
    println!("   This example shows how to handle errors gracefully");
    println!("   when some operations succeed and others fail.");
    println!();

    // Create a mix of operations, some will succeed, some might fail
    let error_test_tasks: Vec<_> = vec![
        // These should succeed
        client.upload_buffer(b"Valid file 1", "txt", None),
        client.upload_buffer(b"Valid file 2", "txt", None),
        // This might fail if file doesn't exist (for demonstration)
        client.download_file("group1/M00/00/00/nonexistent.txt"),
        // These should succeed
        client.upload_buffer(b"Valid file 3", "txt", None),
    ];

    println!("   Running operations with potential errors...");
    println!();

    let results = futures::future::join_all(error_test_tasks).await;

    // Process results with error handling
    let mut successful_ops = Vec::new();
    let mut failed_ops = Vec::new();

    for (index, result) in results.iter().enumerate() {
        match result {
            Ok(value) => {
                successful_ops.push((index, value));
                println!("   ✓ Operation {} succeeded", index + 1);
            }
            Err(e) => {
                failed_ops.push((index, e));
                println!("   ✗ Operation {} failed: {}", index + 1, e);
            }
        }
    }

    println!();
    println!("   Summary:");
    println!("   - Successful operations: {}", successful_ops.len());
    println!("   - Failed operations: {}", failed_ops.len());
    println!();
    println!("   → Partial failures are handled gracefully");
    println!("   → Successful operations are not affected by failures");
    println!("   → Each operation's result is independent");

    // Clean up successful uploads
    println!();
    println!("   Cleaning up successful uploads...");
    for (_, result) in successful_ops {
        // Check if it's a file ID (String) from upload
        if let Ok(file_id) = result.downcast::<String>() {
            let _ = client.delete_file(&*file_id).await;
        }
    }

    // ====================================================================
    // Summary and Key Takeaways
    // ====================================================================
    
    println!("\n{}", "=".repeat(50));
    println!("Concurrent operations example completed!");
    println!("{}", "=".repeat(50));
    println!();

    println!("Key Takeaways:");
    println!();
    println!("  • Use tokio::join! for fixed number of concurrent operations");
    println!("    → Simple and efficient for known number of operations");
    println!("    → Returns tuple of results");
    println!();
    println!("  • Use futures::future::join_all for variable number of operations");
    println!("    → Works with collections of futures");
    println!("    → Returns vector of results");
    println!();
    println!("  • Use tokio::spawn for background tasks");
    println!("    → Creates independent concurrent tasks");
    println!("    → Useful for fire-and-forget operations");
    println!();
    println!("  • Concurrent operations can significantly improve performance");
    println!("    → Especially beneficial for I/O-bound operations");
    println!("    → Reduces total execution time");
    println!();
    println!("  • The client is thread-safe and designed for concurrent use");
    println!("    → Can be shared across multiple tasks");
    println!("    → Connection pool manages connections efficiently");
    println!();
    println!("  • Handle errors appropriately in concurrent operations");
    println!("    → Some operations may succeed while others fail");
    println!("    → Process results individually");
    println!("    → Don't let one failure affect other operations");
    println!();
    println!("  • Connection pool efficiently handles high concurrency");
    println!("    → Reuses connections across operations");
    println!("    → Limits total connections while allowing high concurrency");
    println!("    → Configure max_conns based on your needs");
    println!();

    // Close the client
    println!("Closing client and releasing resources...");
    client.close().await;
    println!("Client closed.");
    println!();

    Ok(())
}

