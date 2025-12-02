/*! FastDFS Cancellation Support Example
 *
 * This comprehensive example demonstrates cancellation support for FastDFS
 * operations using Tokio's cancellation tokens and async cancellation patterns.
 * It covers graceful shutdown, timeout handling, resource cleanup, and
 * proper cancellation of long-running operations.
 *
 * Cancellation topics covered:
 * - Cancellation token usage (tokio::sync::CancellationToken)
 * - Cancelling long-running operations gracefully
 * - Graceful shutdown patterns
 * - Timeout handling with cancellation
 * - Resource cleanup on cancellation
 * - Async cancellation patterns
 * - Using tokio::select! for cancellation
 *
 * Understanding cancellation is crucial for:
 * - Building responsive applications
 * - Implementing graceful shutdown
 * - Handling timeouts properly
 * - Preventing resource leaks
 * - Managing long-running operations
 * - Creating user-cancellable operations
 *
 * Run this example with:
 * ```bash
 * cargo run --example cancellation_example
 * ```
 */

/* Import FastDFS client components */
/* Client provides the main API for FastDFS operations */
/* ClientConfig allows configuration of connection parameters */
use fastdfs::{Client, ClientConfig};
/* Import Tokio cancellation token */
/* CancellationToken allows cooperative cancellation of async operations */
use tokio::sync::CancellationToken;
/* Import Tokio time utilities */
/* For timeouts, delays, and time measurement */
use tokio::time::{sleep, Duration, Instant, timeout};

/* ====================================================================
 * HELPER FUNCTIONS FOR CANCELLATION DEMONSTRATIONS
 * ====================================================================
 * Utility functions that demonstrate different cancellation patterns.
 */

/* Helper function to check cancellation and return error if cancelled */
/* This converts cancellation into an error that can be propagated */
fn check_cancellation(token: &CancellationToken) -> Result<(), Box<dyn std::error::Error>> {
    if token.is_cancelled() {
        return Err("Operation was cancelled".into());
    }
    Ok(())
}

/* Simulate a long-running upload operation */
/* This function demonstrates how to check for cancellation during operations */
async fn simulate_long_upload(
    client: &Client,
    data: &[u8],
    cancel_token: CancellationToken,
) -> Result<String, Box<dyn std::error::Error>> {
    /* Check for cancellation before starting */
    /* Always check cancellation token at the start of operations */
    check_cancellation(&cancel_token)?;
    
    /* Simulate upload in chunks with cancellation checks */
    /* In real operations, check cancellation between chunks */
    let chunk_size = data.len() / 10;
    for i in 0..10 {
        /* Check for cancellation between chunks */
        /* This allows the operation to be cancelled mid-way */
        check_cancellation(&cancel_token)?;
        
        /* Simulate processing a chunk */
        /* In real code, this would be actual upload progress */
        let start = i * chunk_size;
        let end = std::cmp::min((i + 1) * chunk_size, data.len());
        let _chunk = &data[start..end];
        
        /* Small delay to simulate network I/O */
        /* In real operations, this is actual network time */
        sleep(Duration::from_millis(100)).await;
    }
    
    /* Final cancellation check before completing */
    /* Ensure we're still not cancelled before returning success */
    check_cancellation(&cancel_token)?;
    
    /* Perform actual upload */
    /* Only reach here if not cancelled */
    let file_id = client.upload_buffer(data, "txt", None).await?;
    Ok(file_id)
}

/* Simulate a long-running download operation */
/* Demonstrates cancellation during download operations */
async fn simulate_long_download(
    client: &Client,
    file_id: &str,
    cancel_token: CancellationToken,
) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    /* Check for cancellation before starting */
    check_cancellation(&cancel_token)?;
    
    /* Simulate download in chunks */
    /* Check cancellation between chunks */
    for i in 0..5 {
        check_cancellation(&cancel_token)?;
        
        /* Simulate processing a chunk */
        sleep(Duration::from_millis(200)).await;
    }
    
    /* Final cancellation check */
    check_cancellation(&cancel_token)?;
    
    /* Perform actual download */
    let data = client.download_file(file_id).await?;
    Ok(data.to_vec())
}

/* ====================================================================
 * MAIN EXAMPLE FUNCTION
 * ====================================================================
 * Demonstrates all cancellation patterns and techniques.
 */

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    /* Print header for better output readability */
    println!("FastDFS Rust Client - Cancellation Support Example");
    println!("{}", "=".repeat(70));

    /* ====================================================================
     * STEP 1: Initialize Client
     * ====================================================================
     * Set up the FastDFS client for cancellation demonstrations.
     */
    
    println!("\n1. Initializing FastDFS Client...");
    /* Configure client with appropriate settings */
    /* For cancellation examples, we use standard configuration */
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    /* Create the client instance */
    /* The client will be used in cancellation demonstrations */
    let client = Client::new(config)?;
    println!("   ✓ Client initialized successfully");

    /* ====================================================================
     * EXAMPLE 1: Basic Cancellation Token Usage
     * ====================================================================
     * Demonstrate creating and using cancellation tokens.
     */
    
    println!("\n2. Basic Cancellation Token Usage...");
    
    println!("\n   Example 1.1: Creating a cancellation token");
    /* Create a new cancellation token */
    /* This token can be used to signal cancellation to multiple tasks */
    let cancel_token = CancellationToken::new();
    println!("     ✓ Cancellation token created");
    
    println!("\n   Example 1.2: Checking if token is cancelled");
    /* Check if the token is already cancelled */
    /* Initially, tokens are not cancelled */
    if cancel_token.is_cancelled() {
        println!("     Token is cancelled");
    } else {
        println!("     ✓ Token is not cancelled (normal state)");
    }
    
    println!("\n   Example 1.3: Cancelling the token");
    /* Cancel the token */
    /* This will cause all operations waiting on this token to be cancelled */
    cancel_token.cancel();
    println!("     ✓ Token cancelled");
    
    /* Check cancellation status again */
    if cancel_token.is_cancelled() {
        println!("     ✓ Token is now cancelled");
    }
    
    /* Create a new token for subsequent examples */
    /* We need a fresh token that's not cancelled */
    let cancel_token = CancellationToken::new();
    println!("\n   Example 1.4: Created fresh token for examples");

    /* ====================================================================
     * EXAMPLE 2: Cancelling Long-Running Operations
     * ====================================================================
     * Demonstrate cancelling operations that take a long time.
     */
    
    println!("\n3. Cancelling Long-Running Operations...");
    
    println!("\n   Example 2.1: Starting a long-running operation");
    /* Create a cancellation token for this operation */
    let operation_token = CancellationToken::new();
    /* Clone the token for the cancellation task */
    /* Tokens can be cloned and shared across tasks */
    let cancel_clone = operation_token.clone();
    
    /* Spawn a task that will cancel the operation after a delay */
    /* This simulates a user cancelling or a timeout */
    let cancel_task = tokio::spawn(async move {
        /* Wait for 2 seconds before cancelling */
        /* In real scenarios, this could be user input, timeout, etc. */
        sleep(Duration::from_secs(2)).await;
        println!("     → Cancellation signal sent (after 2 seconds)");
        /* Cancel the operation */
        cancel_clone.cancel();
    });
    
    /* Start a long-running operation */
    /* This operation will be cancelled mid-way */
    println!("     Starting long-running upload operation...");
    let start_time = Instant::now();
    
    /* Attempt the operation with cancellation support */
    /* The operation will check for cancellation periodically */
    let large_data = vec![0u8; 10000]; /* Simulate large file */
    let result = simulate_long_upload(&client, &large_data, operation_token.clone()).await;
    
    let elapsed = start_time.elapsed();
    
    /* Check the result */
    match result {
        Ok(file_id) => {
            println!("     ✓ Operation completed successfully: {}", file_id);
            /* Clean up the file if it was created */
            let _ = client.delete_file(&file_id).await;
        }
        Err(e) => {
            /* Check if error is due to cancellation */
            if operation_token.is_cancelled() {
                println!("     ✓ Operation was cancelled gracefully");
                println!("     Elapsed time: {:?}", elapsed);
            } else {
                println!("     ✗ Operation failed: {}", e);
            }
        }
    }
    
    /* Wait for cancellation task to complete */
    /* Ensure the cancellation task finishes */
    let _ = cancel_task.await;

    /* ====================================================================
     * EXAMPLE 3: Using tokio::select! for Cancellation
     * ====================================================================
     * Demonstrate using select! to handle cancellation alongside operations.
     */
    
    println!("\n4. Using tokio::select! for Cancellation...");
    
    println!("\n   Example 3.1: select! with cancellation token");
    /* Create a cancellation token */
    let select_token = CancellationToken::new();
    let select_clone = select_token.clone();
    
    /* Spawn task to cancel after delay */
    tokio::spawn(async move {
        sleep(Duration::from_secs(1)).await;
        select_clone.cancel();
    });
    
    /* Use select! to race between operation and cancellation */
    /* select! allows handling multiple async operations simultaneously */
    println!("     Starting operation with select! cancellation...");
    let start_time = Instant::now();
    
    tokio::select! {
        /* Branch 1: The actual operation */
        /* This branch runs the FastDFS operation */
        result = async {
            /* Simulate operation with cancellation checks */
            for i in 0..10 {
                check_cancellation(&select_token)?;
                sleep(Duration::from_millis(200)).await;
            }
            /* Perform actual operation */
            let data = b"Test data for select! example";
            client.upload_buffer(data, "txt", None).await
        } => {
            match result {
                Ok(file_id) => {
                    println!("     ✓ Operation completed: {}", file_id);
                    let _ = client.delete_file(&file_id).await;
                }
                Err(e) => {
                    println!("     ✗ Operation error: {}", e);
                }
            }
        }
        /* Branch 2: Wait for cancellation */
        /* This branch triggers when the token is cancelled */
        _ = select_token.cancelled() => {
            let elapsed = start_time.elapsed();
            println!("     ✓ Operation cancelled via select!");
            println!("     Elapsed time: {:?}", elapsed);
        }
    }
    
    println!("\n   Example 3.2: select! with timeout and cancellation");
    /* Demonstrate select! with both timeout and cancellation */
    /* This is a common pattern for operations with time limits */
    let timeout_token = CancellationToken::new();
    let timeout_clone = timeout_token.clone();
    
    /* Spawn cancellation task */
    tokio::spawn(async move {
        sleep(Duration::from_secs(3)).await;
        timeout_clone.cancel();
    });
    
    println!("     Starting operation with timeout and cancellation...");
    let start_time = Instant::now();
    
    tokio::select! {
        /* Branch 1: Operation with timeout */
        /* Combine timeout with the operation */
        result = timeout(Duration::from_secs(2), async {
            /* Simulate operation */
            for i in 0..20 {
                check_cancellation(&timeout_token)?;
                sleep(Duration::from_millis(150)).await;
            }
            let data = b"Timeout and cancellation test";
            client.upload_buffer(data, "txt", None).await
        }) => {
            match result {
                Ok(Ok(file_id)) => {
                    println!("     ✓ Operation completed within timeout: {}", file_id);
                    let _ = client.delete_file(&file_id).await;
                }
                Ok(Err(e)) => {
                    println!("     ✗ Operation error: {}", e);
                }
                Err(_) => {
                    println!("     ⏱ Operation timed out");
                }
            }
        }
        /* Branch 2: Cancellation */
        _ = timeout_token.cancelled() => {
            let elapsed = start_time.elapsed();
            println!("     ✓ Operation cancelled");
            println!("     Elapsed time: {:?}", elapsed);
        }
    }

    /* ====================================================================
     * EXAMPLE 4: Graceful Shutdown Pattern
     * ====================================================================
     * Demonstrate graceful shutdown of multiple operations.
     */
    
    println!("\n5. Graceful Shutdown Pattern...");
    
    println!("\n   Example 4.1: Shutting down multiple operations");
    /* Create a cancellation token for shutdown */
    /* This token will be used to signal shutdown to all operations */
    let shutdown_token = CancellationToken::new();
    
    /* Spawn multiple operations that can be shut down */
    /* Each operation listens to the same shutdown token */
    let mut tasks = Vec::new();
    for i in 0..5 {
        let task_token = shutdown_token.clone();
        let task_client = Client::new(ClientConfig::new(vec!["192.168.1.100:22122".to_string()]))?;
        
        /* Spawn a task that runs until shutdown */
        let task = tokio::spawn(async move {
            let mut operation_count = 0;
            /* Run operations until shutdown is requested */
            while !task_token.is_cancelled() {
                /* Check for cancellation before each operation */
                if let Err(_) = check_cancellation(&task_token) {
                    break;
                }
                
                /* Simulate an operation */
                /* In real code, this would be actual FastDFS operations */
                sleep(Duration::from_millis(500)).await;
                operation_count += 1;
                
                /* Limit operations for demonstration */
                if operation_count >= 10 {
                    break;
                }
            }
            println!("     Task {} completed {} operations before shutdown", i, operation_count);
            operation_count
        });
        
        tasks.push(task);
    }
    
    /* Wait a bit, then initiate graceful shutdown */
    /* This simulates receiving a shutdown signal */
    sleep(Duration::from_secs(2)).await;
    println!("     → Initiating graceful shutdown...");
    shutdown_token.cancel();
    
    /* Wait for all tasks to complete */
    /* Tasks should handle cancellation gracefully */
    let mut total_operations = 0;
    for task in tasks {
        if let Ok(count) = task.await {
            total_operations += count;
        }
    }
    
    println!("     ✓ Graceful shutdown completed");
    println!("     Total operations completed: {}", total_operations);

    /* ====================================================================
     * EXAMPLE 5: Timeout Handling with Cancellation
     * ====================================================================
     * Combine timeouts with cancellation for robust operation handling.
     */
    
    println!("\n6. Timeout Handling with Cancellation...");
    
    println!("\n   Example 5.1: Operation with timeout");
    /* Demonstrate timeout without cancellation token */
    /* Simple timeout pattern */
    let timeout_result = timeout(Duration::from_secs(1), async {
        /* Simulate a slow operation */
        sleep(Duration::from_secs(2)).await;
        "Operation completed"
    }).await;
    
    match timeout_result {
        Ok(result) => {
            println!("     ✓ Operation completed: {}", result);
        }
        Err(_) => {
            println!("     ⏱ Operation timed out (expected)");
        }
    }
    
    println!("\n   Example 5.2: Operation with timeout and cancellation token");
    /* Combine timeout with cancellation token */
    /* This provides both timeout and manual cancellation */
    let combined_token = CancellationToken::new();
    let combined_clone = combined_token.clone();
    
    /* Spawn task to cancel after delay */
    tokio::spawn(async move {
        sleep(Duration::from_secs(3)).await;
        combined_clone.cancel();
    });
    
    /* Use timeout with cancellation checks */
    let combined_        result = timeout(Duration::from_secs(2), async {
            /* Operation with cancellation checks */
            for i in 0..10 {
                check_cancellation(&combined_token)?;
            sleep(Duration::from_millis(300)).await;
        }
        Ok::<&str, Box<dyn std::error::Error>>("Completed")
    }).await;
    
    match combined_result {
        Ok(Ok(result)) => {
            println!("     ✓ Operation completed: {}", result);
        }
        Ok(Err(e)) => {
            if combined_token.is_cancelled() {
                println!("     ✓ Operation cancelled");
            } else {
                println!("     ✗ Operation error: {}", e);
            }
        }
        Err(_) => {
            println!("     ⏱ Operation timed out");
        }
    }
    
    println!("\n   Example 5.3: FastDFS operation with timeout");
    /* Apply timeout to actual FastDFS operations */
    /* This is useful for preventing operations from hanging */
    let upload_data = b"Timeout test data";
    let upload_result = timeout(Duration::from_secs(5), 
        client.upload_buffer(upload_data, "txt", None)
    ).await;
    
    match upload_result {
        Ok(Ok(file_id)) => {
            println!("     ✓ Upload completed within timeout: {}", file_id);
            /* Clean up */
            let _ = client.delete_file(&file_id).await;
        }
        Ok(Err(e)) => {
            println!("     ✗ Upload error: {}", e);
        }
        Err(_) => {
            println!("     ⏱ Upload timed out");
        }
    }

    /* ====================================================================
     * EXAMPLE 6: Resource Cleanup on Cancellation
     * ====================================================================
     * Ensure resources are properly cleaned up when operations are cancelled.
     */
    
    println!("\n7. Resource Cleanup on Cancellation...");
    
    println!("\n   Example 6.1: Cleanup in cancellation handler");
    /* Demonstrate proper resource cleanup */
    /* Resources should be cleaned up even when operations are cancelled */
    let cleanup_token = CancellationToken::new();
    let cleanup_clone = cleanup_token.clone();
    
    /* Track resources that need cleanup */
    let mut uploaded_files: Vec<String> = Vec::new();
    
    /* Spawn cancellation task */
    tokio::spawn(async move {
        sleep(Duration::from_secs(1)).await;
        cleanup_clone.cancel();
    });
    
    /* Perform operations with cleanup tracking */
    println!("     Performing operations with cleanup tracking...");
    for i in 0..5 {
        /* Check for cancellation */
        if cleanup_token.is_cancelled() {
            println!("     → Cancellation detected, cleaning up resources...");
            break;
        }
        
        /* Attempt upload */
        let data = format!("Cleanup test {}", i).into_bytes();
        match client.upload_buffer(&data, "txt", None).await {
            Ok(file_id) => {
                println!("     Uploaded file {}: {}", i, file_id);
                uploaded_files.push(file_id);
            }
            Err(e) => {
                println!("     Upload error: {}", e);
            }
        }
        
        /* Small delay */
        sleep(Duration::from_millis(200)).await;
    }
    
    /* Cleanup: Delete all uploaded files */
    /* Always clean up resources, even after cancellation */
    println!("     Cleaning up {} uploaded files...", uploaded_files.len());
    for file_id in &uploaded_files {
        match client.delete_file(file_id).await {
            Ok(_) => {
                println!("     ✓ Deleted: {}", file_id);
            }
            Err(e) => {
                println!("     ✗ Error deleting {}: {}", file_id, e);
            }
        }
    }
    println!("     ✓ Resource cleanup completed");

    /* ====================================================================
     * EXAMPLE 7: Async Cancellation Patterns
     * ====================================================================
     * Demonstrate various patterns for handling cancellation in async code.
     */
    
    println!("\n8. Async Cancellation Patterns...");
    
    println!("\n   Example 7.1: Cancellation in loop pattern");
    /* Pattern: Check cancellation in loops */
    /* This is the most common cancellation pattern */
    let loop_token = CancellationToken::new();
    let loop_clone = loop_token.clone();
    
    tokio::spawn(async move {
        sleep(Duration::from_secs(1)).await;
        loop_clone.cancel();
    });
    
    let mut iterations = 0;
    /* Loop with cancellation check */
    while !loop_token.is_cancelled() {
        /* Check cancellation at start of loop iteration */
        if let Err(_) = check_cancellation(&loop_token) {
            break;
        }
        
        /* Do work */
        iterations += 1;
        sleep(Duration::from_millis(200)).await;
        
        /* Limit for demonstration */
        if iterations >= 20 {
            break;
        }
    }
    println!("     ✓ Loop pattern: {} iterations before cancellation", iterations);
    
    println!("\n   Example 7.2: Cancellation in async function pattern");
    /* Pattern: Pass cancellation token to async functions */
    /* Functions check cancellation at appropriate points */
    async fn cancellable_operation(
        token: CancellationToken,
    ) -> Result<String, Box<dyn std::error::Error>> {
        /* Check at start */
        check_cancellation(&token)?;
        
        /* Do work with periodic checks */
        for i in 0..5 {
            check_cancellation(&token)?;
            sleep(Duration::from_millis(300)).await;
        }
        
        /* Check before returning */
        check_cancellation(&token)?;
        Ok("Operation completed".to_string())
    }
    
    let func_token = CancellationToken::new();
    let func_clone = func_token.clone();
    
    tokio::spawn(async move {
        sleep(Duration::from_secs(1)).await;
        func_clone.cancel();
    });
    
    match cancellable_operation(func_token).await {
        Ok(result) => {
            println!("     ✓ Function pattern: {}", result);
        }
        Err(_) => {
            println!("     ✓ Function pattern: Operation cancelled");
        }
    }
    
    println!("\n   Example 7.3: Cancellation with select! pattern");
    /* Pattern: Use select! to handle cancellation */
    /* This allows cancellation to interrupt operations */
    let select_pattern_token = CancellationToken::new();
    let select_pattern_clone = select_pattern_token.clone();
    
    tokio::spawn(async move {
        sleep(Duration::from_secs(1)).await;
        select_pattern_clone.cancel();
    });
    
    tokio::select! {
        result = async {
            /* Long-running operation */
            for i in 0..10 {
                check_cancellation(&select_pattern_token)?;
                sleep(Duration::from_millis(200)).await;
            }
            Ok::<String, Box<dyn std::error::Error>>("Done".to_string())
        } => {
            match result {
                Ok(msg) => println!("     ✓ Select pattern: {}", msg),
                Err(_) => println!("     ✓ Select pattern: Cancelled"),
            }
        }
        _ = select_pattern_token.cancelled() => {
            println!("     ✓ Select pattern: Cancellation received");
        }
    }

    /* ====================================================================
     * EXAMPLE 8: Real-World Cancellation Scenarios
     * ====================================================================
     * Demonstrate cancellation in realistic scenarios.
     */
    
    println!("\n9. Real-World Cancellation Scenarios...");
    
    println!("\n   Scenario 1: User-initiated cancellation");
    /* Simulate user cancelling an upload */
    let user_token = CancellationToken::new();
    let user_clone = user_token.clone();
    
    /* Simulate user pressing cancel after 1 second */
    tokio::spawn(async move {
        sleep(Duration::from_secs(1)).await;
        println!("     → User pressed cancel button");
        user_clone.cancel();
    });
    
    println!("     Starting upload (user can cancel)...");
    let upload_data = b"User cancellable upload";
    match simulate_long_upload(&client, upload_data, user_token).await {
        Ok(file_id) => {
            println!("     ✓ Upload completed: {}", file_id);
            let _ = client.delete_file(&file_id).await;
        }
        Err(_) => {
            println!("     ✓ Upload cancelled by user");
        }
    }
    
    println!("\n   Scenario 2: Server shutdown cancellation");
    /* Simulate server shutdown requiring operation cancellation */
    let server_token = CancellationToken::new();
    let server_clone = server_token.clone();
    
    /* Simulate shutdown signal */
    tokio::spawn(async move {
        sleep(Duration::from_secs(1)).await;
        println!("     → Server shutdown signal received");
        server_clone.cancel();
    });
    
    println!("     Processing operations (shutdown in progress)...");
    /* Process operations until shutdown */
    let mut processed = 0;
    while !server_token.is_cancelled() {
        check_cancellation(&server_token)?;
        /* Simulate processing */
        sleep(Duration::from_millis(300)).await;
        processed += 1;
        if processed >= 10 {
            break;
        }
    }
    println!("     ✓ Processed {} operations before shutdown", processed);
    
    println!("\n   Scenario 3: Timeout-based cancellation");
    /* Operations that should complete within a time limit */
    let timeout_scenario_token = CancellationToken::new();
    
    /* Create a timeout that cancels the token */
    let timeout_task = {
        let token = timeout_scenario_token.clone();
        tokio::spawn(async move {
            sleep(Duration::from_secs(2)).await;
            token.cancel();
        })
    };
    
    println!("     Starting operation with 2-second timeout...");
    let start = Instant::now();
    
    /* Operation that might take longer than timeout */
    let mut completed = false;
    for i in 0..20 {
        if timeout_scenario_token.is_cancelled() {
            println!("     ⏱ Operation timed out after {:?}", start.elapsed());
            break;
        }
        sleep(Duration::from_millis(200)).await;
        if i == 19 {
            completed = true;
        }
    }
    
    if completed {
        println!("     ✓ Operation completed within timeout");
    }
    
    let _ = timeout_task.await;

    /* ====================================================================
     * EXAMPLE 9: Best Practices for Cancellation
     * ====================================================================
     * Learn best practices for implementing cancellation.
     */
    
    println!("\n10. Cancellation Best Practices...");
    
    println!("\n   Best Practice 1: Always check cancellation at operation start");
    println!("     ✓ Check token.is_cancelled() or use helper function");
    println!("     ✗ Starting operations without checking cancellation");
    
    println!("\n   Best Practice 2: Check cancellation in loops");
    println!("     ✓ Check cancellation at the start of each loop iteration");
    println!("     ✗ Long loops without cancellation checks");
    
    println!("\n   Best Practice 3: Check cancellation before expensive operations");
    println!("     ✓ Check before network I/O, file operations, etc.");
    println!("     ✗ Performing expensive operations when already cancelled");
    
    println!("\n   Best Practice 4: Use select! for cancellation-aware operations");
    println!("     ✓ tokio::select! allows cancellation to interrupt operations");
    println!("     ✗ Blocking operations that can't be cancelled");
    
    println!("\n   Best Practice 5: Clean up resources on cancellation");
    println!("     ✓ Always clean up resources, even when cancelled");
    println!("     ✗ Leaving resources allocated after cancellation");
    
    println!("\n   Best Practice 6: Combine timeout with cancellation");
    println!("     ✓ Use both timeout() and cancellation tokens");
    println!("     ✗ Relying on only one cancellation mechanism");
    
    println!("\n   Best Practice 7: Use CancellationToken for graceful shutdown");
    println!("     ✓ Single token can coordinate shutdown of multiple operations");
    println!("     ✗ Complex shutdown coordination logic");
    
    println!("\n   Best Practice 8: Clone tokens for sharing across tasks");
    println!("     ✓ CancellationToken::clone() is cheap and safe");
    println!("     ✗ Creating new tokens for each task");

    /* ====================================================================
     * CLEANUP
     * ====================================================================
     * Clean up any remaining resources.
     */
    
    println!("\n11. Final Cleanup...");
    /* Close the client to release all resources */
    /* This is important even when operations are cancelled */
    client.close().await;
    println!("   ✓ Client closed, all resources released");

    /* ====================================================================
     * SUMMARY
     * ====================================================================
     * Print summary of cancellation concepts demonstrated.
     */
    
    println!("\n{}", "=".repeat(70));
    println!("Cancellation Support Example Completed Successfully!");
    println!("\nSummary of demonstrated features:");
    println!("  ✓ Basic cancellation token usage");
    println!("  ✓ Cancelling long-running operations");
    println!("  ✓ Using tokio::select! for cancellation");
    println!("  ✓ Graceful shutdown patterns");
    println!("  ✓ Timeout handling with cancellation");
    println!("  ✓ Resource cleanup on cancellation");
    println!("  ✓ Async cancellation patterns");
    println!("  ✓ Real-world cancellation scenarios");
    println!("  ✓ Cancellation best practices");
    println!("\nAll cancellation concepts demonstrated with extensive comments.");

    /* Return success */
    Ok(())
}

