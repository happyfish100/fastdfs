//! FastDFS Error Handling Example
//!
//! This example demonstrates comprehensive error handling patterns for the FastDFS client.
//! It covers various error scenarios and how to handle them gracefully in Rust applications.
//!
//! Key Topics Covered:
//! - Handling FastDFS-specific exceptions
//! - Handling network errors and timeouts
//! - Handling file not found errors
//! - Implementing retry logic patterns
//! - Error recovery strategies
//! - Understanding the error hierarchy and error types
//! - Pattern matching on different error types
//! - Custom error handling functions
//!
//! Run this example with:
//! ```bash
//! cargo run --example error_handling_example
//! ```

use fastdfs::{Client, ClientConfig, FastDFSError};
use std::time::Duration;
use tokio::time::sleep;

// ============================================================================
// Main Entry Point
// ============================================================================

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Print header information
    println!("FastDFS Rust Client - Error Handling Example");
    println!("{}", "=".repeat(50));
    println!();

    // ====================================================================
    // Step 1: Configure and Create Client
    // ====================================================================
    // The client configuration determines how the client behaves,
    // including timeouts, connection pool settings, and retry behavior.
    // Proper configuration can help prevent many errors before they occur.
    
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    // Create the client instance
    // This may fail if the configuration is invalid or if initial
    // connections cannot be established.
    let client = Client::new(config)?;

    // ====================================================================
    // Example 1: Basic Error Handling with Pattern Matching
    // ====================================================================
    // Pattern matching is the idiomatic Rust way to handle errors.
    // It allows you to handle different error types explicitly and
    // provides compile-time guarantees that all cases are handled.
    
    println!("\n1. Basic Error Handling with Pattern Matching");
    println!("------------------------------------------------");
    println!();
    println!("   This example demonstrates how to use pattern matching");
    println!("   to handle different types of errors that can occur");
    println!("   during FastDFS operations.");
    println!();

    // Prepare test data for upload
    let test_data = b"Test file for error handling demonstration";
    
    // Attempt to upload a file and handle potential errors
    // The match expression allows us to handle both success and failure cases
    match client.upload_buffer(test_data, "txt", None).await {
        // Success case: file was uploaded successfully
        Ok(file_id) => {
            println!("   ✓ File uploaded successfully!");
            println!("   File ID: {}", file_id);
            println!("   → This is the expected outcome for a successful upload");
            println!();
            
            // Clean up: delete the test file
            // We use let _ = to ignore any errors during cleanup
            // In production, you might want to log cleanup errors
            let _ = client.delete_file(&file_id).await;
        }
        // Error case: something went wrong during upload
        Err(e) => {
            println!("   ✗ Upload failed: {}", e);
            println!("   → The error message provides details about what went wrong");
            println!();
            
            // Handle specific error types using nested pattern matching
            // This allows us to provide specific guidance for each error type
            match e {
                // Connection timeout errors occur when the client cannot
                // establish a connection to the tracker or storage server
                // within the configured timeout period.
                FastDFSError::ConnectionTimeout(addr) => {
                    println!("   → Error Type: Connection Timeout");
                    println!("   → Server Address: {}", addr);
                    println!("   → Possible Causes:");
                    println!("     - Tracker server is not running");
                    println!("     - Network connectivity issues");
                    println!("     - Firewall blocking connections");
                    println!("     - Incorrect server address or port");
                    println!("   → Recommended Actions:");
                    println!("     - Verify tracker server is running");
                    println!("     - Check network connectivity");
                    println!("     - Verify firewall rules");
                    println!("     - Increase connect_timeout if network is slow");
                }
                // Network timeout errors occur when a network I/O operation
                // (read or write) takes longer than the configured timeout.
                FastDFSError::NetworkTimeout(op) => {
                    println!("   → Error Type: Network Timeout");
                    println!("   → Operation: {}", op);
                    println!("   → Possible Causes:");
                    println!("     - Network congestion");
                    println!("     - Server is overloaded");
                    println!("     - File is very large");
                    println!("     - Network latency is high");
                    println!("   → Recommended Actions:");
                    println!("     - Increase network_timeout for large files");
                    println!("     - Check server load");
                    println!("     - Verify network conditions");
                }
                // No storage server available means the tracker could not
                // find any available storage servers to store the file.
                FastDFSError::NoStorageServer => {
                    println!("   → Error Type: No Storage Server Available");
                    println!("   → Possible Causes:");
                    println!("     - All storage servers are offline");
                    println!("     - Storage servers are not registered with tracker");
                    println!("     - Storage servers are in maintenance mode");
                    println!("   → Recommended Actions:");
                    println!("     - Check storage server status");
                    println!("     - Verify storage servers are registered");
                    println!("     - Contact system administrator");
                }
                // Catch-all for other error types
                _ => {
                    println!("   → Error Type: Other");
                    println!("   → Unexpected error occurred");
                    println!("   → Check error message for details");
                }
            }
            println!();
        }
    }

    // ====================================================================
    // Example 2: Handling File Not Found Errors
    // ====================================================================
    // File not found errors are common when trying to access files that
    // don't exist or have been deleted. This example shows how to handle
    // these errors gracefully.
    
    println!("\n2. Handling File Not Found Errors");
    println!("-----------------------------------");
    println!();
    println!("   File not found errors occur when attempting to access");
    println!("   a file that doesn't exist in the FastDFS storage system.");
    println!("   This is a common scenario in many applications.");
    println!();

    // Create a file ID that we know doesn't exist
    // In a real application, this might come from user input,
    // a database, or another system.
    let non_existent_file_id = "group1/M00/00/00/nonexistent_file.txt";

    println!("   Attempting to download non-existent file...");
    println!("   File ID: {}", non_existent_file_id);
    println!();

    // Attempt to download a non-existent file
    // This will fail, but we want to handle the error gracefully
    match client.download_file(non_existent_file_id).await {
        // This should not happen for a non-existent file
        Ok(_) => {
            println!("   ✓ File downloaded (unexpected!)");
            println!("   → This should not happen for a non-existent file");
        }
        // Expected: file not found error
        Err(e) => {
            // Check if it's specifically a file not found error
            // Using if let allows us to extract the file ID from the error
            if let FastDFSError::FileNotFound(file_id) = &e {
                println!("   ✓ Correctly caught file not found error");
                println!("   File ID: {}", file_id);
                println!("   → This is expected behavior for non-existent files");
                println!("   → The error contains the file ID that was not found");
            } else {
                // Unexpected error type
                println!("   ✗ Unexpected error type: {}", e);
                println!("   → Expected FileNotFound, but got different error");
            }
        }
    }

    println!();
    println!("   Using file_exists() for safer checks:");
    println!("   → file_exists() is a lightweight way to check if a file exists");
    println!("   → It returns a boolean instead of an error");
    println!("   → This can be more convenient than catching errors");
    println!();

    // Using file_exists for a safer check
    // This method returns a boolean instead of an error,
    // making it easier to check file existence without error handling
    let exists = client.file_exists(non_existent_file_id).await;
    println!("   File exists: {}", exists);
    
    if !exists {
        println!("   → File does not exist, as expected");
        println!("   → Use file_exists() to check before operations");
        println!("   → This avoids unnecessary error handling");
    }

    // ====================================================================
    // Example 3: Handling Network Errors
    // ====================================================================
    // Network errors can occur for various reasons: connection failures,
    // timeouts, network interruptions, etc. This example shows how to
    // identify and handle different types of network errors.
    
    println!("\n3. Handling Network Errors");
    println!("--------------------------");
    println!();
    println!("   Network errors can occur during any network operation.");
    println!("   They can be transient (temporary) or permanent.");
    println!("   Understanding the error type helps determine if retry is appropriate.");
    println!();

    // Upload a file first to use for network error testing
    let file_id = match client.upload_buffer(b"Network test file", "txt", None).await {
        Ok(id) => {
            println!("   ✓ File uploaded for network error testing");
            println!("   File ID: {}", id);
            id
        }
        Err(e) => {
            println!("   ✗ Failed to upload test file: {}", e);
            println!("   → This might indicate network connectivity issues");
            println!("   → Cannot proceed with network error examples");
            return Ok(());
        }
    };

    println!();
    println!("   Network errors can occur during various operations:");
    println!("   - Connection establishment");
    println!("   - Data transfer (upload/download)");
    println!("   - Server communication");
    println!();

    // Network errors can occur during various operations
    // The client automatically handles retries, but we can catch network errors
    // to provide better error messages or implement custom retry logic
    match client.download_file(&file_id).await {
        Ok(_) => {
            println!("   ✓ Download successful");
            println!("   → No network errors occurred");
        }
        Err(e) => {
            // Pattern match on different network error types
            match e {
                // Network error with detailed information
                // This error type includes the operation, address, and underlying I/O error
                FastDFSError::Network { operation, addr, source } => {
                    println!("   ✗ Network error detected:");
                    println!("     Operation: {}", operation);
                    println!("     Address: {}", addr);
                    println!("     Source Error: {}", source);
                    println!("   → This error provides detailed information about the failure");
                    println!("   → The 'source' field contains the underlying I/O error");
                    println!("   → Possible causes:");
                    println!("     - Network connection was reset");
                    println!("     - Server closed the connection unexpectedly");
                    println!("     - Network interface is down");
                    println!("     - DNS resolution failed");
                    println!("   → Recommended actions:");
                    println!("     - Check network connectivity");
                    println!("     - Verify server addresses are correct");
                    println!("     - Check DNS configuration");
                    println!("     - Retry the operation");
                }
                // Connection timeout
                FastDFSError::ConnectionTimeout(addr) => {
                    println!("   ✗ Connection timeout: {}", addr);
                    println!("   → Server might be unreachable");
                    println!("   → Check firewall settings");
                    println!("   → Verify server is running");
                    println!("   → Consider increasing connect_timeout");
                }
                // Network I/O timeout
                FastDFSError::NetworkTimeout(op) => {
                    println!("   ✗ Network timeout: {}", op);
                    println!("   → Operation took too long");
                    println!("   → Network might be slow or congested");
                    println!("   → Consider increasing network_timeout");
                    println!("   → For large files, use streaming or chunked operations");
                }
                // Other errors
                _ => {
                    println!("   ✗ Other error: {}", e);
                    println!("   → This is not a network error");
                    println!("   → Check error message for details");
                }
            }
        }
    }

    println!();
    println!("   Cleaning up test file...");
    
    // Clean up the test file
    let _ = client.delete_file(&file_id).await;

    // ====================================================================
    // Example 4: Retry Logic Pattern
    // ====================================================================
    // Retry logic is important for handling transient errors.
    // This example demonstrates how to implement retry logic with
    // exponential backoff, which is a common pattern for handling
    // temporary failures.
    
    println!("\n4. Retry Logic Pattern");
    println!("----------------------");
    println!();
    println!("   Retry logic allows operations to recover from transient errors.");
    println!("   Exponential backoff prevents overwhelming the server with retries.");
    println!("   Not all errors should be retried - some are permanent failures.");
    println!();

    // Test data for retry example
    let test_data = b"Retry logic test file";
    
    // Retry configuration
    let max_retries = 3;
    let mut retry_count = 0;
    let mut last_error = None;

    println!("   Configuration:");
    println!("   - Max retries: {}", max_retries);
    println!("   - Backoff strategy: Exponential (2^retry_count seconds)");
    println!();

    // Implement custom retry logic
    // This loop continues until success or max retries is reached
    loop {
        println!("   Attempt {}...", retry_count + 1);
        
        // Attempt the operation
        match client.upload_buffer(test_data, "txt", None).await {
            // Success: break out of retry loop
            Ok(file_id) => {
                println!("   ✓ Upload succeeded after {} retries", retry_count);
                println!("   File ID: {}", file_id);
                println!();
                
                // Clean up
                let _ = client.delete_file(&file_id).await;
                break;
            }
            // Error: determine if retryable
            Err(e) => {
                retry_count += 1;
                last_error = Some(e.clone());
                
                // Determine if the error is retryable
                // Not all errors should be retried - some indicate permanent failures
                let is_retryable = matches!(
                    e,
                    // These errors are typically transient and worth retrying
                    FastDFSError::ConnectionTimeout(_)
                        | FastDFSError::NetworkTimeout(_)
                        | FastDFSError::Network { .. }
                        | FastDFSError::NoStorageServer
                );

                // Check if error is retryable
                if !is_retryable {
                    println!("   ✗ Non-retryable error: {}", e);
                    println!("   → This error type should not be retried");
                    println!("   → Examples: InvalidArgument, FileNotFound, etc.");
                    break;
                }

                // Check if we've exceeded max retries
                if retry_count >= max_retries {
                    println!("   ✗ Max retries ({}) exceeded", max_retries);
                    println!("   → Giving up after {} attempts", retry_count);
                    break;
                }

                // Log retry attempt
                println!("   → Retry {}/{}: {}", retry_count, max_retries, e);
                
                // Calculate exponential backoff delay
                // Formula: 2^retry_count seconds
                // This gives: 1s, 2s, 4s, 8s, etc.
                let delay = Duration::from_secs(2_u64.pow(retry_count));
                println!("   → Waiting {:?} before retry...", delay);
                println!("   → Exponential backoff prevents overwhelming the server");
                
                // Wait before retrying
                sleep(delay).await;
                
                println!();
            }
        }
    }

    // Display final result
    if let Some(err) = last_error {
        if retry_count >= max_retries {
            println!("   Final error after all retries: {}", err);
            println!("   → Consider:");
            println!("     - Checking server status");
            println!("     - Verifying network connectivity");
            println!("     - Increasing retry count");
            println!("     - Investigating the root cause");
        }
    }

    // ====================================================================
    // Example 5: Error Recovery Strategies
    // ====================================================================
    // Error recovery strategies help applications continue operating
    // even when some operations fail. This example demonstrates
    // common recovery patterns.
    
    println!("\n5. Error Recovery Strategies");
    println!("----------------------------");
    println!();
    println!("   Error recovery strategies allow applications to continue");
    println!("   operating even when some operations fail.");
    println!("   Different strategies are appropriate for different scenarios.");
    println!();

    // Strategy 1: Fallback to alternative operation
    println!("   Strategy 1: Fallback to Alternative Operation");
    println!("   → When primary operation fails, try an alternative");
    println!("   → Useful when multiple approaches can achieve the same goal");
    println!();

    let file_id = match client.upload_buffer(b"Primary upload attempt", "txt", None).await {
        // Primary operation succeeded
        Ok(id) => {
            println!("   ✓ Primary upload succeeded");
            println!("   File ID: {}", id);
            id
        }
        // Primary operation failed - try fallback
        Err(e) => {
            println!("   ✗ Primary upload failed: {}", e);
            println!("   → Attempting fallback strategy...");
            println!();
            
            // Fallback: try with different extension or metadata
            // In a real application, you might:
            // - Try a different storage group
            // - Use a different file format
            // - Try a different server
            match client.upload_buffer(b"Fallback upload attempt", "dat", None).await {
                Ok(id) => {
                    println!("   ✓ Fallback upload succeeded");
                    println!("   File ID: {}", id);
                    id
                }
                Err(e2) => {
                    println!("   ✗ Fallback also failed: {}", e2);
                    println!("   → Both primary and fallback operations failed");
                    println!("   → Returning error to caller");
                    return Err(Box::new(e2));
                }
            }
        }
    };

    println!();

    // Strategy 2: Graceful degradation
    println!("   Strategy 2: Graceful Degradation");
    println!("   → When full operation fails, use a simpler alternative");
    println!("   → Provides partial functionality instead of complete failure");
    println!();

    // Try to get full file information
    match client.get_file_info(&file_id).await {
        // Full operation succeeded
        Ok(info) => {
            println!("   ✓ Full file info retrieved:");
            println!("     Size: {} bytes", info.file_size);
            println!("     CRC32: {}", info.crc32);
            println!("     Create Time: {:?}", info.create_time);
            println!("     Source IP: {}", info.source_ip_addr);
        }
        // Full operation failed - degrade to simpler check
        Err(e) => {
            println!("   ✗ Failed to get file info: {}", e);
            println!("   → Degrading: Using file_exists() instead");
            println!("   → This provides less information but still confirms file existence");
            
            // Degrade to simpler operation
            let exists = client.file_exists(&file_id).await;
            println!("   → File exists: {}", exists);
            
            if exists {
                println!("   → Degraded check confirms file exists");
                println!("   → Application can continue with limited information");
            } else {
                println!("   → File does not exist");
                println!("   → This is unexpected if we just uploaded it");
            }
        }
    }

    println!();
    println!("   Cleaning up test file...");
    
    // Clean up
    let _ = client.delete_file(&file_id).await;

    // ====================================================================
    // Example 6: Understanding Error Hierarchy
    // ====================================================================
    // Understanding the error hierarchy helps in writing better error
    // handling code. This example categorizes errors and shows how
    // to handle them appropriately.
    
    println!("\n6. Understanding Error Hierarchy");
    println!("--------------------------------");
    println!();
    println!("   FastDFS errors can be categorized into different groups:");
    println!("   - Client Errors: Issues with client configuration or usage");
    println!("   - Network Errors: Network connectivity or timeout issues");
    println!("   - Server Errors: Issues reported by FastDFS servers");
    println!("   - I/O Errors: Low-level I/O operation failures");
    println!();

    // Helper function to categorize errors
    // This function helps understand which category an error belongs to
    fn categorize_error(error: &FastDFSError) -> &str {
        match error {
            // Client errors: issues with how the client is used
            FastDFSError::ClientClosed
            | FastDFSError::InvalidArgument(_)
            | FastDFSError::InvalidFileId(_) => "Client Error",
            
            // Network errors: connectivity or timeout issues
            FastDFSError::ConnectionTimeout(_)
            | FastDFSError::NetworkTimeout(_)
            | FastDFSError::Network { .. } => "Network Error",
            
            // Server errors: issues reported by FastDFS servers
            FastDFSError::FileNotFound(_)
            | FastDFSError::NoStorageServer
            | FastDFSError::Protocol { .. }
            | FastDFSError::StorageServerOffline(_)
            | FastDFSError::TrackerServerOffline(_)
            | FastDFSError::InsufficientSpace
            | FastDFSError::FileAlreadyExists(_) => "Server Error",
            
            // I/O errors: low-level I/O operation failures
            FastDFSError::Io(_) | FastDFSError::Utf8(_) => "I/O Error",
            
            // Other errors: uncategorized
            _ => "Other Error",
        }
    }

    // Test error categorization with various error types
    println!("   Testing error categorization:");
    println!();
    
    let test_errors = vec![
        FastDFSError::FileNotFound("test.txt".to_string()),
        FastDFSError::ConnectionTimeout("192.168.1.100:22122".to_string()),
        FastDFSError::InvalidArgument("Invalid file extension".to_string()),
        FastDFSError::NoStorageServer,
        FastDFSError::NetworkTimeout("download".to_string()),
    ];

    for error in test_errors {
        let category = categorize_error(&error);
        println!("   {} → {}", category, error);
        println!("     → This error belongs to the '{}' category", category);
        println!("     → Category-specific handling can be applied");
        println!();
    }

    // ====================================================================
    // Example 7: Custom Error Handling Function
    // ====================================================================
    // Custom error handling functions can encapsulate error handling
    // logic and make code more reusable. This example shows how to
    // create a reusable error handler.
    
    println!("\n7. Custom Error Handling Function");
    println!("-----------------------------------");
    println!();
    println!("   Custom error handling functions encapsulate error handling logic.");
    println!("   They make code more reusable and maintainable.");
    println!("   They can provide consistent error handling across the application.");
    println!();

    // Define a custom error handler
    // This function wraps an operation and provides consistent error handling
    async fn handle_operation_with_recovery<F, T>(
        operation: F,
        operation_name: &str,
    ) -> Result<T, FastDFSError>
    where
        F: std::future::Future<Output = Result<T, FastDFSError>>,
    {
        println!("   Executing operation: {}", operation_name);
        
        // Execute the operation
        match operation.await {
            // Success case
            Ok(result) => {
                println!("   ✓ {} succeeded", operation_name);
                Ok(result)
            }
            // Error case: provide detailed error information
            Err(e) => {
                println!("   ✗ {} failed: {}", operation_name, e);
                println!();
                
                // Custom recovery logic based on error type
                // Different error types may require different recovery strategies
                match &e {
                    // File not found might be expected in some cases
                    FastDFSError::FileNotFound(_) => {
                        println!("   → Recovery: File not found is expected in some cases");
                        println!("   → Consider checking file_exists() before operations");
                        println!("   → Or handle this as a normal case, not an error");
                    }
                    // Connection timeout suggests network issues
                    FastDFSError::ConnectionTimeout(_) => {
                        println!("   → Recovery: Connection timeout - check network");
                        println!("   → Verify server is reachable");
                        println!("   → Consider retrying with exponential backoff");
                    }
                    // Network timeout suggests slow network or large file
                    FastDFSError::NetworkTimeout(_) => {
                        println!("   → Recovery: Network timeout - operation may be too slow");
                        println!("   → Consider increasing timeout for large files");
                        println!("   → Or use streaming/chunked operations");
                    }
                    // Other errors
                    _ => {
                        println!("   → Recovery: No specific recovery strategy");
                        println!("   → Check error message for details");
                        println!("   → Consider logging for investigation");
                    }
                }
                println!();
                
                // Return the error to caller
                // Caller can decide whether to retry, log, or propagate
                Err(e)
            }
        }
    }

    // Use the custom error handler
    println!("   Using custom error handler:");
    println!();
    
    let file_id = match handle_operation_with_recovery(
        client.upload_buffer(b"Custom handler test", "txt", None),
        "Upload operation",
    )
    .await
    {
        Ok(id) => {
            println!("   ✓ Operation completed successfully");
            println!("   File ID: {}", id);
            id
        }
        Err(e) => {
            println!("   ✗ Operation failed after recovery attempts");
            println!("   → Returning error to caller");
            return Err(Box::new(e));
        }
    };

    println!();
    println!("   Cleaning up test file...");
    
    // Clean up
    let _ = client.delete_file(&file_id).await;

    // ====================================================================
    // Example 8: Error Propagation and Context
    // ====================================================================
    // Adding context to errors helps with debugging and provides
    // better error messages to users. This example shows how to
    // add context while propagating errors.
    
    println!("\n8. Error Propagation and Context");
    println!("-------------------------------");
    println!();
    println!("   Error propagation allows errors to bubble up the call stack.");
    println!("   Adding context helps identify where and why errors occurred.");
    println!("   Context can be added using error messages or custom error types.");
    println!();

    // Function that adds context to errors
    // This function wraps an operation and adds context to any errors
    async fn upload_with_context(
        client: &Client,
        data: &[u8],
        ext: &str,
    ) -> Result<String, String> {
        // Execute the operation and map errors to include context
        client
            .upload_buffer(data, ext, None)
            .await
            .map_err(|e| {
                // Add context to the error message
                format!("Failed to upload {} file: {}", ext, e)
            })
    }

    println!("   Testing error context:");
    println!();
    
    match upload_with_context(&client, b"Context test file", "txt").await {
        Ok(file_id) => {
            println!("   ✓ Upload with context succeeded");
            println!("   File ID: {}", file_id);
            println!("   → Context helps identify which operation failed");
            
            // Clean up
            let _ = client.delete_file(&file_id).await;
        }
        Err(e) => {
            println!("   ✗ {}", e);
            println!("   → Error context helps identify the operation that failed");
            println!("   → Context makes debugging easier");
            println!("   → Users get more informative error messages");
        }
    }

    // ====================================================================
    // Summary and Key Takeaways
    // ====================================================================
    
    println!("\n{}", "=".repeat(50));
    println!("Error handling example completed!");
    println!("{}", "=".repeat(50));
    println!();
    
    println!("Key Takeaways:");
    println!();
    println!("  • Always handle errors explicitly using match or ? operator");
    println!("    → Rust's type system ensures errors are not ignored");
    println!("    → Pattern matching provides compile-time safety");
    println!();
    println!("  • Use pattern matching to handle specific error types");
    println!("    → Different error types may require different handling");
    println!("    → Pattern matching allows fine-grained error handling");
    println!();
    println!("  • Implement retry logic for transient errors");
    println!("    → Not all errors should be retried");
    println!("    → Use exponential backoff to avoid overwhelming servers");
    println!("    → Set reasonable retry limits");
    println!();
    println!("  • Provide fallback strategies for critical operations");
    println!("    → Fallback operations can maintain functionality");
    println!("    → Graceful degradation provides partial functionality");
    println!("    → Consider user experience when implementing fallbacks");
    println!();
    println!("  • Add context to errors for better debugging");
    println!("    → Context helps identify where errors occurred");
    println!("    → Better error messages improve user experience");
    println!("    → Context aids in troubleshooting production issues");
    println!();
    println!("  • Use file_exists() to check before operations when appropriate");
    println!("    → Avoids unnecessary error handling");
    println!("    → Provides cleaner code flow");
    println!("    → Note: file_exists() may have slight performance overhead");
    println!();
    println!("  • Understand error categories for appropriate handling");
    println!("    → Client errors: fix client code or configuration");
    println!("    → Network errors: check connectivity, consider retry");
    println!("    → Server errors: check server status, may need admin action");
    println!("    → I/O errors: check system resources, file permissions");
    println!();

    // Close the client and release resources
    println!("Closing client and releasing resources...");
    client.close().await;
    println!("Client closed.");
    println!();

    Ok(())
}

