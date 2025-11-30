//! FastDFS Error Handling Example
//!
//! This example demonstrates comprehensive error handling patterns for the FastDFS client:
//! - Handling FastDFS-specific exceptions
//! - Handling network errors
//! - Handling file not found errors
//! - Implementing retry logic patterns
//! - Error recovery strategies
//! - Understanding the error hierarchy and error types
//! - Matching on different error types
//! - Custom error handling
//!
//! Run this example with:
//! ```bash
//! cargo run --example error_handling_example
//! ```

use fastdfs::{Client, ClientConfig, FastDFSError};
use std::time::Duration;
use tokio::time::sleep;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Error Handling Example");
    println!("{}", "=".repeat(50));

    // Step 1: Configure and create client
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    let client = Client::new(config)?;

    // ====================================================================
    // Example 1: Basic Error Handling with Pattern Matching
    // ====================================================================
    println!("\n1. Basic Error Handling with Pattern Matching");
    println!("------------------------------------------------");
    println!();

    let test_data = b"Test file for error handling";
    
    // Upload a file and handle potential errors
    match client.upload_buffer(test_data, "txt", None).await {
        Ok(file_id) => {
            println!("   ✓ File uploaded successfully!");
            println!("   File ID: {}", file_id);
            
            // Clean up
            let _ = client.delete_file(&file_id).await;
        }
        Err(e) => {
            println!("   ✗ Upload failed: {}", e);
            // Handle specific error types
            match e {
                FastDFSError::ConnectionTimeout(addr) => {
                    println!("   → Connection timeout to: {}", addr);
                    println!("   → Check if tracker server is reachable");
                }
                FastDFSError::NetworkTimeout(op) => {
                    println!("   → Network timeout during: {}", op);
                    println!("   → Consider increasing network timeout");
                }
                FastDFSError::NoStorageServer => {
                    println!("   → No storage server available");
                    println!("   → Check tracker server configuration");
                }
                _ => {
                    println!("   → Unexpected error occurred");
                }
            }
        }
    }

    // ====================================================================
    // Example 2: Handling File Not Found Errors
    // ====================================================================
    println!("\n2. Handling File Not Found Errors");
    println!("-----------------------------------");
    println!();

    let non_existent_file_id = "group1/M00/00/00/nonexistent_file.txt";

    // Attempt to download a non-existent file
    match client.download_file(non_existent_file_id).await {
        Ok(_) => {
            println!("   ✓ File downloaded (unexpected!)");
        }
        Err(e) => {
            // Check if it's a file not found error
            if let FastDFSError::FileNotFound(file_id) = &e {
                println!("   ✓ Correctly caught file not found error");
                println!("   File ID: {}", file_id);
                println!("   → This is expected behavior for non-existent files");
            } else {
                println!("   ✗ Unexpected error: {}", e);
            }
        }
    }

    // Using file_exists for a safer check
    println!("\n   Using file_exists() for safer checks:");
    let exists = client.file_exists(non_existent_file_id).await;
    println!("   File exists: {}", exists);
    if !exists {
        println!("   → Use file_exists() to check before operations");
    }

    // ====================================================================
    // Example 3: Handling Network Errors
    // ====================================================================
    println!("\n3. Handling Network Errors");
    println!("--------------------------");
    println!();

    // Upload a file first
    let file_id = match client.upload_buffer(b"Network test", "txt", None).await {
        Ok(id) => {
            println!("   ✓ File uploaded for network error testing");
            id
        }
        Err(e) => {
            println!("   ✗ Failed to upload test file: {}", e);
            println!("   → This might indicate network connectivity issues");
            return Ok(());
        }
    };

    // Network errors can occur during various operations
    // The client automatically handles retries, but we can catch network errors
    match client.download_file(&file_id).await {
        Ok(_) => {
            println!("   ✓ Download successful");
        }
        Err(e) => {
            match e {
                FastDFSError::Network { operation, addr, source } => {
                    println!("   ✗ Network error detected:");
                    println!("     Operation: {}", operation);
                    println!("     Address: {}", addr);
                    println!("     Source: {}", source);
                    println!("   → Check network connectivity");
                    println!("   → Verify server addresses are correct");
                }
                FastDFSError::ConnectionTimeout(addr) => {
                    println!("   ✗ Connection timeout: {}", addr);
                    println!("   → Server might be unreachable");
                    println!("   → Check firewall settings");
                }
                FastDFSError::NetworkTimeout(op) => {
                    println!("   ✗ Network timeout: {}", op);
                    println!("   → Operation took too long");
                    println!("   → Consider increasing timeout values");
                }
                _ => {
                    println!("   ✗ Other error: {}", e);
                }
            }
        }
    }

    // Clean up
    let _ = client.delete_file(&file_id).await;

    // ====================================================================
    // Example 4: Retry Logic Pattern
    // ====================================================================
    println!("\n4. Retry Logic Pattern");
    println!("----------------------");
    println!();

    let test_data = b"Retry logic test";
    let max_retries = 3;
    let mut retry_count = 0;
    let mut last_error = None;

    // Implement custom retry logic
    loop {
        match client.upload_buffer(test_data, "txt", None).await {
            Ok(file_id) => {
                println!("   ✓ Upload succeeded after {} retries", retry_count);
                println!("   File ID: {}", file_id);
                
                // Clean up
                let _ = client.delete_file(&file_id).await;
                break;
            }
            Err(e) => {
                retry_count += 1;
                last_error = Some(e.clone());
                
                // Check if error is retryable
                let is_retryable = matches!(
                    e,
                    FastDFSError::ConnectionTimeout(_)
                        | FastDFSError::NetworkTimeout(_)
                        | FastDFSError::Network { .. }
                        | FastDFSError::NoStorageServer
                );

                if !is_retryable {
                    println!("   ✗ Non-retryable error: {}", e);
                    break;
                }

                if retry_count >= max_retries {
                    println!("   ✗ Max retries ({}) exceeded", max_retries);
                    break;
                }

                println!("   → Retry {}/{}: {}", retry_count, max_retries, e);
                
                // Exponential backoff: wait 2^retry_count seconds
                let delay = Duration::from_secs(2_u64.pow(retry_count));
                println!("   → Waiting {:?} before retry...", delay);
                sleep(delay).await;
            }
        }
    }

    if let Some(err) = last_error {
        if retry_count >= max_retries {
            println!("   Final error: {}", err);
        }
    }

    // ====================================================================
    // Example 5: Error Recovery Strategies
    // ====================================================================
    println!("\n5. Error Recovery Strategies");
    println!("----------------------------");
    println!();

    // Strategy 1: Fallback to alternative operation
    println!("   Strategy 1: Fallback to alternative operation");
    let file_id = match client.upload_buffer(b"Primary upload", "txt", None).await {
        Ok(id) => {
            println!("   ✓ Primary upload succeeded");
            id
        }
        Err(e) => {
            println!("   ✗ Primary upload failed: {}", e);
            println!("   → Attempting fallback...");
            
            // Fallback: try with different extension or metadata
            match client.upload_buffer(b"Fallback upload", "dat", None).await {
                Ok(id) => {
                    println!("   ✓ Fallback upload succeeded");
                    id
                }
                Err(e2) => {
                    println!("   ✗ Fallback also failed: {}", e2);
                    return Err(Box::new(e2));
                }
            }
        }
    };

    // Strategy 2: Graceful degradation
    println!("\n   Strategy 2: Graceful degradation");
    match client.get_file_info(&file_id).await {
        Ok(info) => {
            println!("   ✓ Full file info retrieved:");
            println!("     Size: {} bytes", info.file_size);
            println!("     CRC32: {}", info.crc32);
        }
        Err(e) => {
            println!("   ✗ Failed to get file info: {}", e);
            println!("   → Degrading: Using file_exists() instead");
            let exists = client.file_exists(&file_id).await;
            println!("   → File exists: {}", exists);
        }
    }

    // Clean up
    let _ = client.delete_file(&file_id).await;

    // ====================================================================
    // Example 6: Understanding Error Hierarchy
    // ====================================================================
    println!("\n6. Understanding Error Hierarchy");
    println!("--------------------------------");
    println!();

    // Demonstrate different error categories
    println!("   Error Categories:");
    println!("   - Client Errors: ClientClosed, InvalidArgument, InvalidFileId");
    println!("   - Network Errors: ConnectionTimeout, NetworkTimeout, Network");
    println!("   - Server Errors: FileNotFound, NoStorageServer, Protocol");
    println!("   - I/O Errors: Io, Utf8");
    println!();

    // Helper function to categorize errors
    fn categorize_error(error: &FastDFSError) -> &str {
        match error {
            FastDFSError::ClientClosed
            | FastDFSError::InvalidArgument(_)
            | FastDFSError::InvalidFileId(_) => "Client Error",
            FastDFSError::ConnectionTimeout(_)
            | FastDFSError::NetworkTimeout(_)
            | FastDFSError::Network { .. } => "Network Error",
            FastDFSError::FileNotFound(_)
            | FastDFSError::NoStorageServer
            | FastDFSError::Protocol { .. }
            | FastDFSError::StorageServerOffline(_)
            | FastDFSError::TrackerServerOffline(_) => "Server Error",
            FastDFSError::Io(_) | FastDFSError::Utf8(_) => "I/O Error",
            _ => "Other Error",
        }
    }

    // Test error categorization
    let test_errors = vec![
        FastDFSError::FileNotFound("test.txt".to_string()),
        FastDFSError::ConnectionTimeout("192.168.1.100:22122".to_string()),
        FastDFSError::InvalidArgument("Invalid file extension".to_string()),
    ];

    for error in test_errors {
        println!("   {} → {}", categorize_error(&error), error);
    }

    // ====================================================================
    // Example 7: Custom Error Handling Function
    // ====================================================================
    println!("\n7. Custom Error Handling Function");
    println!("-----------------------------------");
    println!();

    // Define a custom error handler
    async fn handle_operation_with_recovery<F, T>(
        operation: F,
        operation_name: &str,
    ) -> Result<T, FastDFSError>
    where
        F: std::future::Future<Output = Result<T, FastDFSError>>,
    {
        match operation.await {
            Ok(result) => {
                println!("   ✓ {} succeeded", operation_name);
                Ok(result)
            }
            Err(e) => {
                println!("   ✗ {} failed: {}", operation_name, e);
                
                // Custom recovery logic based on error type
                match &e {
                    FastDFSError::FileNotFound(_) => {
                        println!("   → Recovery: File not found is expected in some cases");
                    }
                    FastDFSError::ConnectionTimeout(_) => {
                        println!("   → Recovery: Connection timeout - check network");
                    }
                    FastDFSError::NetworkTimeout(_) => {
                        println!("   → Recovery: Network timeout - operation may be too slow");
                    }
                    _ => {
                        println!("   → Recovery: No specific recovery strategy");
                    }
                }
                
                Err(e)
            }
        }
    }

    // Use the custom error handler
    let file_id = match handle_operation_with_recovery(
        client.upload_buffer(b"Custom handler test", "txt", None),
        "Upload operation",
    )
    .await
    {
        Ok(id) => {
            println!("   ✓ Operation completed successfully");
            id
        }
        Err(e) => {
            println!("   ✗ Operation failed after recovery attempts");
            return Err(Box::new(e));
        }
    };

    // Clean up
    let _ = client.delete_file(&file_id).await;

    // ====================================================================
    // Example 8: Error Propagation and Context
    // ====================================================================
    println!("\n8. Error Propagation and Context");
    println!("-------------------------------");
    println!();

    // Function that adds context to errors
    async fn upload_with_context(
        client: &Client,
        data: &[u8],
        ext: &str,
    ) -> Result<String, String> {
        client
            .upload_buffer(data, ext, None)
            .await
            .map_err(|e| format!("Failed to upload {} file: {}", ext, e))
    }

    match upload_with_context(&client, b"Context test", "txt").await {
        Ok(file_id) => {
            println!("   ✓ Upload with context succeeded");
            println!("   File ID: {}", file_id);
            let _ = client.delete_file(&file_id).await;
        }
        Err(e) => {
            println!("   ✗ {}", e);
            println!("   → Error context helps identify the operation that failed");
        }
    }

    println!("\n{}", "=".repeat(50));
    println!("Error handling example completed!");
    println!("\nKey Takeaways:");
    println!("  • Always handle errors explicitly using match or ? operator");
    println!("  • Use pattern matching to handle specific error types");
    println!("  • Implement retry logic for transient errors");
    println!("  • Provide fallback strategies for critical operations");
    println!("  • Add context to errors for better debugging");
    println!("  • Use file_exists() to check before operations when appropriate");

    // Close the client
    client.close().await;
    println!("\nClient closed.");

    Ok(())
}

