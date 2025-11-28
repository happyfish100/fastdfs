// ============================================================================
// FastDFS C# Client - Error Handling Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates comprehensive error handling in FastDFS, including
// handling FastDFS exceptions, network errors, file not found errors, retry
// logic patterns, and error recovery strategies. It shows how to properly
// handle various error scenarios that can occur during FastDFS operations
// and implement robust error handling patterns for production applications.
//
// Error handling is a critical aspect of any distributed system application.
// This example provides comprehensive patterns and best practices for handling
// errors gracefully, implementing retry logic, and recovering from failures
// in FastDFS operations.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating comprehensive error handling in FastDFS operations.
    /// 
    /// This example shows:
    /// - How to handle FastDFS exceptions (base and specific types)
    /// - How to handle network errors and timeouts
    /// - How to handle file not found errors
    /// - Retry logic patterns (exponential backoff, circuit breaker, etc.)
    /// - Error recovery strategies
    /// - Best practices for error handling in production applications
    /// 
    /// Error handling patterns demonstrated:
    /// 1. Specific exception handling for different error types
    /// 2. Retry logic with exponential backoff
    /// 3. Circuit breaker pattern for repeated failures
    /// 4. Fallback strategies for critical operations
    /// 5. Error logging and monitoring
    /// 6. Graceful degradation
    /// </summary>
    class ErrorHandlingExample
    {
        /// <summary>
        /// Main entry point for the error handling example.
        /// 
        /// This method demonstrates various error handling patterns through
        /// a series of examples, each showing different aspects of error
        /// handling in FastDFS operations.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Error Handling Example");
            Console.WriteLine("=============================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates comprehensive error handling,");
            Console.WriteLine("including exception handling, retry logic, and recovery strategies.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For error handling examples, we configure appropriate timeouts
            // and retry counts to demonstrate various error scenarios.
            // ====================================================================

            var config = new FastDFSClientConfig
            {
                // Specify tracker server addresses
                // Tracker servers coordinate file storage and retrieval operations
                // Multiple trackers provide redundancy and load balancing
                TrackerAddresses = new[]
                {
                    "192.168.1.100:22122",  // Primary tracker server
                    "192.168.1.101:22122"   // Secondary tracker server (for redundancy)
                },

                // Maximum number of connections per server
                // Connection pool settings affect error handling behavior
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                // Shorter timeouts help detect connection issues faster
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // Appropriate timeouts help balance responsiveness and reliability
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Idle timeout: time before idle connections are closed
                // Idle timeout helps manage connection pool resources
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                // Retry count is important for handling transient errors
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations. Error handling is crucial when working with
            // distributed systems like FastDFS.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Handle FastDFS Exceptions (Base Exception)
                    // ============================================================
                    // 
                    // This example demonstrates handling the base FastDFSException,
                    // which is the parent class for all FastDFS-specific exceptions.
                    // Catching the base exception provides a catch-all for any
                    // FastDFS-related errors that aren't handled by more specific
                    // exception handlers.
                    // 
                    // Best practice: Always catch specific exceptions first, then
                    // fall back to the base exception for unhandled cases.
                    // ============================================================

                    Console.WriteLine("Example 1: Handle FastDFS Exceptions (Base Exception)");
                    Console.WriteLine("=====================================================");
                    Console.WriteLine();

                    // Attempt a file operation that might fail
                    // In this example, we'll try to download a file that may not exist
                    // to demonstrate error handling
                    Console.WriteLine("Attempting to download a potentially non-existent file...");
                    Console.WriteLine();

                    try
                    {
                        // Attempt to download a file that may not exist
                        // This operation might throw a FastDFSException or one of
                        // its derived exceptions, depending on the specific error
                        var nonExistentFileId = "group1/M00/00/00/nonexistent_file.txt";
                        var fileData = await client.DownloadFileAsync(nonExistentFileId);

                        // If we reach here, the file exists and was downloaded successfully
                        Console.WriteLine("File downloaded successfully (unexpected in this example)");
                        Console.WriteLine($"File size: {fileData.Length} bytes");
                    }
                    catch (FastDFSException ex)
                    {
                        // Handle FastDFS-specific exceptions
                        // FastDFSException is the base class for all FastDFS-related
                        // exceptions. Catching it here provides a catch-all for
                        // any FastDFS errors that aren't handled by more specific
                        // exception handlers.
                        Console.WriteLine("FastDFS Exception caught:");
                        Console.WriteLine($"  Message: {ex.Message}");
                        Console.WriteLine($"  Error Code: {ex.ErrorCode?.ToString() ?? "N/A"}");
                        Console.WriteLine();

                        // Check for inner exception
                        // Inner exceptions often contain more detailed error information
                        // from the underlying network or system operations
                        if (ex.InnerException != null)
                        {
                            Console.WriteLine("  Inner Exception:");
                            Console.WriteLine($"    Type: {ex.InnerException.GetType().Name}");
                            Console.WriteLine($"    Message: {ex.InnerException.Message}");
                            Console.WriteLine();
                        }

                        // Log the exception for monitoring and debugging
                        // In production, you would log to your logging framework
                        // (e.g., Serilog, NLog, Application Insights, etc.)
                        Console.WriteLine("  Logging exception for monitoring...");
                        Console.WriteLine($"  Exception Type: {ex.GetType().Name}");
                        Console.WriteLine($"  Stack Trace: {ex.StackTrace?.Substring(0, Math.Min(200, ex.StackTrace.Length ?? 0))}...");
                        Console.WriteLine();
                    }

                    Console.WriteLine("Example 1 completed.");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Handle Network Errors
                    // ============================================================
                    // 
                    // This example demonstrates handling network-related errors,
                    // including connection timeouts, network timeouts, and other
                    // network communication failures. Network errors are common
                    // in distributed systems and should be handled gracefully
                    // with appropriate retry logic.
                    // 
                    // Network errors can occur due to:
                    // - Network connectivity issues
                    // - Server unavailability
                    // - Timeout conditions
                    // - Connection pool exhaustion
                    // ============================================================

                    Console.WriteLine("Example 2: Handle Network Errors");
                    Console.WriteLine("==================================");
                    Console.WriteLine();

                    // Attempt a file operation that might encounter network errors
                    // In a real scenario, network errors might occur due to
                    // server unavailability, network partitions, or timeout conditions
                    Console.WriteLine("Attempting file operation that might encounter network errors...");
                    Console.WriteLine();

                    try
                    {
                        // Attempt to upload a file
                        // This operation involves network communication and might
                        // encounter network errors if the server is unavailable
                        // or if there are connectivity issues
                        var testFilePath = "test_network_error.txt";

                        if (!File.Exists(testFilePath))
                        {
                            await File.WriteAllTextAsync(testFilePath, "Test file for network error handling");
                            Console.WriteLine($"Created test file: {testFilePath}");
                            Console.WriteLine();
                        }

                        Console.WriteLine("Uploading file (may encounter network errors)...");
                        var fileId = await client.UploadFileAsync(testFilePath, null);
                        Console.WriteLine($"File uploaded successfully: {fileId}");
                        Console.WriteLine();
                    }
                    catch (FastDFSNetworkException ex)
                    {
                        // Handle network-specific errors
                        // FastDFSNetworkException is thrown when network communication
                        // fails, such as connection timeouts, connection refused,
                        // network unreachable, or other network-related errors
                        Console.WriteLine("Network Exception caught:");
                        Console.WriteLine($"  Message: {ex.Message}");
                        Console.WriteLine($"  Operation: {ex.Operation}");
                        Console.WriteLine($"  Address: {ex.Address}");
                        Console.WriteLine();

                        // Check for inner exception
                        // The inner exception typically contains the underlying
                        // network exception (e.g., SocketException, TimeoutException)
                        if (ex.InnerException != null)
                        {
                            Console.WriteLine("  Inner Exception:");
                            Console.WriteLine($"    Type: {ex.InnerException.GetType().Name}");
                            Console.WriteLine($"    Message: {ex.InnerException.Message}");
                            Console.WriteLine();
                        }

                        // Determine recovery strategy based on error type
                        // Different network errors may require different recovery
                        // strategies, such as retry, failover, or graceful degradation
                        Console.WriteLine("  Recovery Strategy:");
                        Console.WriteLine("    - Check network connectivity");
                        Console.WriteLine("    - Verify server availability");
                        Console.WriteLine("    - Consider retry with exponential backoff");
                        Console.WriteLine("    - Implement circuit breaker pattern for repeated failures");
                        Console.WriteLine();
                    }
                    catch (FastDFSConnectionTimeoutException ex)
                    {
                        // Handle connection timeout errors specifically
                        // Connection timeouts occur when establishing a connection
                        // to a server exceeds the configured timeout duration
                        Console.WriteLine("Connection Timeout Exception caught:");
                        Console.WriteLine($"  Message: {ex.Message}");
                        Console.WriteLine($"  Address: {ex.Address}");
                        Console.WriteLine($"  Timeout: {ex.Timeout.TotalSeconds} seconds");
                        Console.WriteLine();

                        // Recovery strategy for connection timeouts
                        Console.WriteLine("  Recovery Strategy:");
                        Console.WriteLine("    - Verify server is running and accessible");
                        Console.WriteLine("    - Check network connectivity");
                        Console.WriteLine("    - Consider increasing connection timeout");
                        Console.WriteLine("    - Try alternative tracker/storage servers");
                        Console.WriteLine();
                    }
                    catch (FastDFSNetworkTimeoutException ex)
                    {
                        // Handle network I/O timeout errors specifically
                        // Network timeouts occur when read/write operations on
                        // an established connection exceed the configured timeout
                        Console.WriteLine("Network Timeout Exception caught:");
                        Console.WriteLine($"  Message: {ex.Message}");
                        Console.WriteLine($"  Operation: {ex.Operation}");
                        Console.WriteLine($"  Address: {ex.Address}");
                        Console.WriteLine($"  Timeout: {ex.Timeout.TotalSeconds} seconds");
                        Console.WriteLine();

                        // Recovery strategy for network timeouts
                        Console.WriteLine("  Recovery Strategy:");
                        Console.WriteLine("    - Server may be overloaded or unresponsive");
                        Console.WriteLine("    - Consider increasing network timeout for large files");
                        Console.WriteLine("    - Implement retry logic with exponential backoff");
                        Console.WriteLine("    - Monitor server performance and capacity");
                        Console.WriteLine();
                    }
                    catch (FastDFSException ex)
                    {
                        // Catch other FastDFS exceptions
                        // This provides a fallback for any other FastDFS-related
                        // errors that aren't specifically network-related
                        Console.WriteLine($"Other FastDFS Exception: {ex.GetType().Name}");
                        Console.WriteLine($"  Message: {ex.Message}");
                        Console.WriteLine();
                    }

                    Console.WriteLine("Example 2 completed.");
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Handle File Not Found Errors
                    // ============================================================
                    // 
                    // This example demonstrates handling file not found errors,
                    // which occur when attempting to access files that don't
                    // exist in FastDFS storage. File not found errors are common
                    // and should be handled gracefully with appropriate user
                    // feedback and recovery strategies.
                    // 
                    // File not found errors can occur when:
                    // - File ID is invalid or malformed
                    // - File has been deleted
                    // - File was never uploaded
                    // - File is on a different storage server that's unavailable
                    // ============================================================

                    Console.WriteLine("Example 3: Handle File Not Found Errors");
                    Console.WriteLine("========================================");
                    Console.WriteLine();

                    // Attempt to download a file that doesn't exist
                    // This demonstrates how to handle file not found errors
                    Console.WriteLine("Attempting to download a non-existent file...");
                    Console.WriteLine();

                    try
                    {
                        // Attempt to download a file that doesn't exist
                        // This will throw a FastDFSFileNotFoundException
                        var invalidFileId = "group1/M00/00/00/invalid_file_that_does_not_exist.txt";
                        var fileData = await client.DownloadFileAsync(invalidFileId);

                        // If we reach here, the file exists (unexpected in this example)
                        Console.WriteLine("File downloaded successfully (unexpected)");
                    }
                    catch (FastDFSFileNotFoundException ex)
                    {
                        // Handle file not found errors specifically
                        // FastDFSFileNotFoundException is thrown when attempting
                        // to download, delete, or query information about a file
                        // that does not exist in the FastDFS cluster
                        Console.WriteLine("File Not Found Exception caught:");
                        Console.WriteLine($"  Message: {ex.Message}");
                        Console.WriteLine($"  File ID: {ex.FileId}");
                        Console.WriteLine();

                        // Recovery strategies for file not found errors
                        Console.WriteLine("  Recovery Strategy:");
                        Console.WriteLine("    - Verify file ID is correct");
                        Console.WriteLine("    - Check if file was deleted");
                        Console.WriteLine("    - Provide user-friendly error message");
                        Console.WriteLine("    - Consider fallback to default/placeholder content");
                        Console.WriteLine("    - Log for monitoring and debugging");
                        Console.WriteLine();

                        // Example: Provide user-friendly error message
                        // In a real application, you would provide a user-friendly
                        // message instead of technical error details
                        var userFriendlyMessage = $"The requested file could not be found. " +
                                                 $"Please verify the file ID and try again.";
                        Console.WriteLine($"  User-friendly message: {userFriendlyMessage}");
                        Console.WriteLine();
                    }
                    catch (FastDFSException ex)
                    {
                        // Catch other FastDFS exceptions
                        Console.WriteLine($"Other FastDFS Exception: {ex.GetType().Name}");
                        Console.WriteLine($"  Message: {ex.Message}");
                        Console.WriteLine();
                    }

                    // Example: Check if file exists before downloading
                    // This demonstrates a proactive approach to handling file
                    // not found errors by checking file existence first
                    Console.WriteLine("Attempting to check file existence before downloading...");
                    Console.WriteLine();

                    try
                    {
                        var fileIdToCheck = "group1/M00/00/00/another_nonexistent_file.txt";

                        // Try to get file information
                        // This will throw FastDFSFileNotFoundException if the file doesn't exist
                        var fileInfo = await client.GetFileInfoAsync(fileIdToCheck);

                        // If we reach here, the file exists
                        Console.WriteLine($"File exists: {fileIdToCheck}");
                        Console.WriteLine($"  Size: {fileInfo.FileSize} bytes");
                        Console.WriteLine($"  Created: {fileInfo.CreateTime}");
                    }
                    catch (FastDFSFileNotFoundException ex)
                    {
                        // File doesn't exist - handle gracefully
                        Console.WriteLine($"File does not exist: {ex.FileId}");
                        Console.WriteLine("  Proceeding with alternative logic (e.g., use placeholder)");
                        Console.WriteLine();
                    }

                    Console.WriteLine("Example 3 completed.");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Retry Logic Patterns
                    // ============================================================
                    // 
                    // This example demonstrates various retry logic patterns for
                    // handling transient errors. Retry logic is essential for
                    // building resilient applications that can recover from
                    // temporary failures.
                    // 
                    // Common retry patterns:
                    // - Simple retry with fixed delay
                    // - Exponential backoff
                    // - Linear backoff
                    // - Jittered backoff
                    // - Circuit breaker pattern
                    // ============================================================

                    Console.WriteLine("Example 4: Retry Logic Patterns");
                    Console.WriteLine("=================================");
                    Console.WriteLine();

                    // Pattern 1: Simple Retry with Fixed Delay
                    // This is the simplest retry pattern, where we retry a
                    // fixed number of times with a fixed delay between attempts
                    Console.WriteLine("Pattern 1: Simple Retry with Fixed Delay");
                    Console.WriteLine("------------------------------------------");
                    Console.WriteLine();

                    await SimpleRetryWithFixedDelay(client, "test_simple_retry.txt");
                    Console.WriteLine();

                    // Pattern 2: Exponential Backoff Retry
                    // Exponential backoff increases the delay between retries
                    // exponentially, which helps reduce load on the server
                    // during transient failures
                    Console.WriteLine("Pattern 2: Exponential Backoff Retry");
                    Console.WriteLine("--------------------------------------");
                    Console.WriteLine();

                    await ExponentialBackoffRetry(client, "test_exponential_retry.txt");
                    Console.WriteLine();

                    // Pattern 3: Retry with Maximum Attempts
                    // This pattern retries up to a maximum number of attempts
                    // and provides detailed logging for each attempt
                    Console.WriteLine("Pattern 3: Retry with Maximum Attempts");
                    Console.WriteLine("----------------------------------------");
                    Console.WriteLine();

                    await RetryWithMaxAttempts(client, "test_max_attempts.txt");
                    Console.WriteLine();

                    // Pattern 4: Retry with Cancellation Support
                    // This pattern supports cancellation tokens, allowing
                    // retry operations to be cancelled if needed
                    Console.WriteLine("Pattern 4: Retry with Cancellation Support");
                    Console.WriteLine("---------------------------------------------");
                    Console.WriteLine();

                    var cancellationTokenSource = new CancellationTokenSource();
                    await RetryWithCancellation(client, "test_cancellation.txt", cancellationTokenSource.Token);
                    Console.WriteLine();

                    Console.WriteLine("Example 4 completed.");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Error Recovery Strategies
                    // ============================================================
                    // 
                    // This example demonstrates various error recovery strategies
                    // for handling failures in FastDFS operations. Recovery
                    // strategies help applications continue operating even when
                    // some operations fail.
                    // 
                    // Common recovery strategies:
                    // - Fallback to alternative content
                    // - Graceful degradation
                    // - Retry with different parameters
                    // - Circuit breaker pattern
                    // - Bulkhead pattern
                    // ============================================================

                    Console.WriteLine("Example 5: Error Recovery Strategies");
                    Console.WriteLine("=====================================");
                    Console.WriteLine();

                    // Strategy 1: Fallback to Alternative Content
                    // When a file cannot be retrieved, fall back to alternative
                    // content such as a placeholder or cached version
                    Console.WriteLine("Strategy 1: Fallback to Alternative Content");
                    Console.WriteLine("--------------------------------------------");
                    Console.WriteLine();

                    await FallbackToAlternativeContent(client, "nonexistent_file.txt");
                    Console.WriteLine();

                    // Strategy 2: Graceful Degradation
                    // When some operations fail, continue with available
                    // functionality rather than failing completely
                    Console.WriteLine("Strategy 2: Graceful Degradation");
                    Console.WriteLine("---------------------------------");
                    Console.WriteLine();

                    await GracefulDegradation(client);
                    Console.WriteLine();

                    // Strategy 3: Retry with Different Parameters
                    // When an operation fails, retry with different parameters
                    // such as different timeout values or server addresses
                    Console.WriteLine("Strategy 3: Retry with Different Parameters");
                    Console.WriteLine("-------------------------------------------");
                    Console.WriteLine();

                    await RetryWithDifferentParameters(client, "test_different_params.txt");
                    Console.WriteLine();

                    Console.WriteLine("Example 5 completed.");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for error handling
                    // in FastDFS applications, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for Error Handling");
                    Console.WriteLine("===================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Exception Handling Hierarchy:");
                    Console.WriteLine("   - Always catch specific exceptions first");
                    Console.WriteLine("   - Use base exceptions as fallback");
                    Console.WriteLine("   - Handle FastDFSFileNotFoundException separately");
                    Console.WriteLine("   - Handle network exceptions with retry logic");
                    Console.WriteLine("   - Handle protocol exceptions without retry");
                    Console.WriteLine();
                    Console.WriteLine("2. Retry Logic:");
                    Console.WriteLine("   - Use exponential backoff for transient errors");
                    Console.WriteLine("   - Set maximum retry attempts to avoid infinite loops");
                    Console.WriteLine("   - Don't retry on non-transient errors (e.g., invalid arguments)");
                    Console.WriteLine("   - Implement jitter to avoid thundering herd problem");
                    Console.WriteLine("   - Use cancellation tokens for long-running retries");
                    Console.WriteLine();
                    Console.WriteLine("3. Error Logging:");
                    Console.WriteLine("   - Log all exceptions with sufficient context");
                    Console.WriteLine("   - Include error codes, file IDs, and operation details");
                    Console.WriteLine("   - Log inner exceptions for debugging");
                    Console.WriteLine("   - Use structured logging for better analysis");
                    Console.WriteLine("   - Monitor error rates and patterns");
                    Console.WriteLine();
                    Console.WriteLine("4. Recovery Strategies:");
                    Console.WriteLine("   - Implement fallback mechanisms for critical operations");
                    Console.WriteLine("   - Use graceful degradation when possible");
                    Console.WriteLine("   - Cache frequently accessed files locally");
                    Console.WriteLine("   - Implement circuit breaker for repeated failures");
                    Console.WriteLine("   - Provide user-friendly error messages");
                    Console.WriteLine();
                    Console.WriteLine("5. Network Error Handling:");
                    Console.WriteLine("   - Distinguish between transient and permanent failures");
                    Console.WriteLine("   - Implement timeout handling appropriately");
                    Console.WriteLine("   - Use connection pooling effectively");
                    Console.WriteLine("   - Monitor network health and server availability");
                    Console.WriteLine("   - Consider failover to alternative servers");
                    Console.WriteLine();
                    Console.WriteLine("6. File Not Found Handling:");
                    Console.WriteLine("   - Validate file IDs before operations");
                    Console.WriteLine("   - Check file existence when appropriate");
                    Console.WriteLine("   - Provide meaningful error messages to users");
                    Console.WriteLine("   - Consider fallback to default/placeholder content");
                    Console.WriteLine("   - Log missing file requests for analysis");
                    Console.WriteLine();
                    Console.WriteLine("7. Performance Considerations:");
                    Console.WriteLine("   - Balance retry attempts with response time");
                    Console.WriteLine("   - Use appropriate timeout values");
                    Console.WriteLine("   - Implement connection pooling");
                    Console.WriteLine("   - Monitor and optimize retry delays");
                    Console.WriteLine("   - Consider async/await for non-blocking operations");
                    Console.WriteLine();
                    Console.WriteLine("8. Monitoring and Alerting:");
                    Console.WriteLine("   - Track error rates and patterns");
                    Console.WriteLine("   - Set up alerts for critical failures");
                    Console.WriteLine("   - Monitor retry success rates");
                    Console.WriteLine("   - Track network health metrics");
                    Console.WriteLine("   - Analyze error logs for patterns");
                    Console.WriteLine();
                    Console.WriteLine("9. Testing Error Scenarios:");
                    Console.WriteLine("   - Test with invalid file IDs");
                    Console.WriteLine("   - Test with network failures");
                    Console.WriteLine("   - Test with server unavailability");
                    Console.WriteLine("   - Test retry logic under various conditions");
                    Console.WriteLine("   - Test recovery strategies");
                    Console.WriteLine();
                    Console.WriteLine("10. User Experience:");
                    Console.WriteLine("    - Provide clear, actionable error messages");
                    Console.WriteLine("    - Avoid exposing technical details to end users");
                    Console.WriteLine("    - Implement progress indicators for retries");
                    Console.WriteLine("    - Consider offline mode for critical applications");
                    Console.WriteLine("    - Implement proper error state management in UI");
                    Console.WriteLine();

                    Console.WriteLine("All examples completed successfully!");
                }
                catch (Exception ex)
                {
                    // Final catch-all for any unexpected errors
                    Console.WriteLine($"Unexpected Error: {ex.Message}");
                    Console.WriteLine($"Stack Trace: {ex.StackTrace}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }

        // ====================================================================
        // Retry Logic Pattern Implementations
        // ====================================================================

        /// <summary>
        /// Demonstrates simple retry logic with fixed delay between attempts.
        /// 
        /// This pattern retries a fixed number of times with a constant delay
        /// between attempts. It's simple but may not be optimal for all scenarios.
        /// </summary>
        /// <param name="client">FastDFS client instance.</param>
        /// <param name="filePath">Path to the file to upload.</param>
        /// <returns>A task that represents the asynchronous operation.</returns>
        static async Task SimpleRetryWithFixedDelay(FastDFSClient client, string filePath)
        {
            const int maxRetries = 3;
            const int delaySeconds = 2;

            Console.WriteLine($"Attempting to upload file with simple retry (max {maxRetries} attempts, {delaySeconds}s delay)...");

            for (int attempt = 1; attempt <= maxRetries; attempt++)
            {
                try
                {
                    // Attempt the operation
                    if (!File.Exists(filePath))
                    {
                        await File.WriteAllTextAsync(filePath, "Test file for simple retry");
                    }

                    var fileId = await client.UploadFileAsync(filePath, null);
                    Console.WriteLine($"  Attempt {attempt}: Success! File ID: {fileId}");
                    return; // Success - exit retry loop
                }
                catch (FastDFSNetworkException ex)
                {
                    // Retry on network errors
                    Console.WriteLine($"  Attempt {attempt}: Network error - {ex.Message}");

                    if (attempt < maxRetries)
                    {
                        Console.WriteLine($"    Waiting {delaySeconds} seconds before retry...");
                        await Task.Delay(TimeSpan.FromSeconds(delaySeconds));
                    }
                    else
                    {
                        Console.WriteLine($"    All {maxRetries} attempts failed. Giving up.");
                        throw;
                    }
                }
                catch (FastDFSException ex)
                {
                    // Don't retry on other FastDFS errors
                    Console.WriteLine($"  Attempt {attempt}: FastDFS error - {ex.Message}");
                    throw;
                }
            }
        }

        /// <summary>
        /// Demonstrates exponential backoff retry logic.
        /// 
        /// Exponential backoff increases the delay between retries exponentially,
        /// which helps reduce load on the server during transient failures.
        /// </summary>
        /// <param name="client">FastDFS client instance.</param>
        /// <param name="filePath">Path to the file to upload.</param>
        /// <returns>A task that represents the asynchronous operation.</returns>
        static async Task ExponentialBackoffRetry(FastDFSClient client, string filePath)
        {
            const int maxRetries = 5;
            const int baseDelaySeconds = 1;

            Console.WriteLine($"Attempting to upload file with exponential backoff (max {maxRetries} attempts)...");

            for (int attempt = 1; attempt <= maxRetries; attempt++)
            {
                try
                {
                    // Attempt the operation
                    if (!File.Exists(filePath))
                    {
                        await File.WriteAllTextAsync(filePath, "Test file for exponential backoff retry");
                    }

                    var fileId = await client.UploadFileAsync(filePath, null);
                    Console.WriteLine($"  Attempt {attempt}: Success! File ID: {fileId}");
                    return; // Success - exit retry loop
                }
                catch (FastDFSNetworkException ex)
                {
                    // Retry on network errors with exponential backoff
                    Console.WriteLine($"  Attempt {attempt}: Network error - {ex.Message}");

                    if (attempt < maxRetries)
                    {
                        // Calculate exponential backoff delay
                        // Delay = baseDelay * 2^(attempt-1)
                        var delaySeconds = baseDelaySeconds * Math.Pow(2, attempt - 1);
                        Console.WriteLine($"    Waiting {delaySeconds} seconds before retry (exponential backoff)...");
                        await Task.Delay(TimeSpan.FromSeconds(delaySeconds));
                    }
                    else
                    {
                        Console.WriteLine($"    All {maxRetries} attempts failed. Giving up.");
                        throw;
                    }
                }
                catch (FastDFSException ex)
                {
                    // Don't retry on other FastDFS errors
                    Console.WriteLine($"  Attempt {attempt}: FastDFS error - {ex.Message}");
                    throw;
                }
            }
        }

        /// <summary>
        /// Demonstrates retry logic with maximum attempts and detailed logging.
        /// 
        /// This pattern provides comprehensive logging for each retry attempt
        /// and handles various error types appropriately.
        /// </summary>
        /// <param name="client">FastDFS client instance.</param>
        /// <param name="filePath">Path to the file to upload.</param>
        /// <returns>A task that represents the asynchronous operation.</returns>
        static async Task RetryWithMaxAttempts(FastDFSClient client, string filePath)
        {
            const int maxRetries = 3;

            Console.WriteLine($"Attempting operation with retry (max {maxRetries} attempts)...");

            Exception lastException = null;

            for (int attempt = 1; attempt <= maxRetries; attempt++)
            {
                try
                {
                    Console.WriteLine($"  Attempt {attempt}/{maxRetries}...");

                    // Attempt the operation
                    if (!File.Exists(filePath))
                    {
                        await File.WriteAllTextAsync(filePath, "Test file for max attempts retry");
                    }

                    var fileId = await client.UploadFileAsync(filePath, null);
                    Console.WriteLine($"  Success on attempt {attempt}! File ID: {fileId}");
                    return; // Success - exit retry loop
                }
                catch (FastDFSNetworkException ex)
                {
                    // Retry on network errors
                    lastException = ex;
                    Console.WriteLine($"  Attempt {attempt} failed: Network error");
                    Console.WriteLine($"    Error: {ex.Message}");
                    Console.WriteLine($"    Operation: {ex.Operation}");
                    Console.WriteLine($"    Address: {ex.Address}");

                    if (attempt < maxRetries)
                    {
                        var delaySeconds = Math.Pow(2, attempt - 1);
                        Console.WriteLine($"    Retrying in {delaySeconds} seconds...");
                        await Task.Delay(TimeSpan.FromSeconds(delaySeconds));
                    }
                }
                catch (FastDFSFileNotFoundException ex)
                {
                    // Don't retry on file not found
                    Console.WriteLine($"  Attempt {attempt} failed: File not found");
                    Console.WriteLine($"    File ID: {ex.FileId}");
                    throw;
                }
                catch (FastDFSProtocolException ex)
                {
                    // Don't retry on protocol errors
                    Console.WriteLine($"  Attempt {attempt} failed: Protocol error");
                    Console.WriteLine($"    Error: {ex.Message}");
                    throw;
                }
                catch (FastDFSException ex)
                {
                    // Retry on other FastDFS errors
                    lastException = ex;
                    Console.WriteLine($"  Attempt {attempt} failed: FastDFS error");
                    Console.WriteLine($"    Error: {ex.Message}");

                    if (attempt < maxRetries)
                    {
                        var delaySeconds = Math.Pow(2, attempt - 1);
                        Console.WriteLine($"    Retrying in {delaySeconds} seconds...");
                        await Task.Delay(TimeSpan.FromSeconds(delaySeconds));
                    }
                }
            }

            // All retries failed
            Console.WriteLine($"  All {maxRetries} attempts failed.");
            throw new FastDFSException($"Operation failed after {maxRetries} attempts.", lastException);
        }

        /// <summary>
        /// Demonstrates retry logic with cancellation token support.
        /// 
        /// This pattern allows retry operations to be cancelled if needed,
        /// which is important for responsive applications.
        /// </summary>
        /// <param name="client">FastDFS client instance.</param>
        /// <param name="filePath">Path to the file to upload.</param>
        /// <param name="cancellationToken">Cancellation token.</param>
        /// <returns>A task that represents the asynchronous operation.</returns>
        static async Task RetryWithCancellation(FastDFSClient client, string filePath, CancellationToken cancellationToken)
        {
            const int maxRetries = 5;

            Console.WriteLine($"Attempting operation with retry and cancellation support (max {maxRetries} attempts)...");

            for (int attempt = 1; attempt <= maxRetries; attempt++)
            {
                // Check for cancellation before each attempt
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    Console.WriteLine($"  Attempt {attempt}/{maxRetries}...");

                    // Attempt the operation with cancellation token
                    if (!File.Exists(filePath))
                    {
                        await File.WriteAllTextAsync(filePath, "Test file for cancellation retry", cancellationToken);
                    }

                    var fileId = await client.UploadFileAsync(filePath, null, cancellationToken);
                    Console.WriteLine($"  Success on attempt {attempt}! File ID: {fileId}");
                    return; // Success - exit retry loop
                }
                catch (OperationCanceledException)
                {
                    // Operation was cancelled
                    Console.WriteLine($"  Operation cancelled on attempt {attempt}");
                    throw;
                }
                catch (FastDFSNetworkException ex)
                {
                    // Retry on network errors
                    Console.WriteLine($"  Attempt {attempt} failed: Network error - {ex.Message}");

                    if (attempt < maxRetries)
                    {
                        var delaySeconds = Math.Pow(2, attempt - 1);
                        Console.WriteLine($"    Waiting {delaySeconds} seconds before retry...");
                        await Task.Delay(TimeSpan.FromSeconds(delaySeconds), cancellationToken);
                    }
                    else
                    {
                        Console.WriteLine($"    All {maxRetries} attempts failed.");
                        throw;
                    }
                }
            }
        }

        // ====================================================================
        // Error Recovery Strategy Implementations
        // ====================================================================

        /// <summary>
        /// Demonstrates fallback to alternative content when file cannot be retrieved.
        /// 
        /// This strategy provides a fallback mechanism when the primary content
        /// is unavailable, improving user experience.
        /// </summary>
        /// <param name="client">FastDFS client instance.</param>
        /// <param name="fileId">File ID to retrieve.</param>
        /// <returns>A task that represents the asynchronous operation.</returns>
        static async Task FallbackToAlternativeContent(FastDFSClient client, string fileId)
        {
            Console.WriteLine($"Attempting to retrieve file: {fileId}");
            Console.WriteLine();

            try
            {
                // Try to download the file
                var fileData = await client.DownloadFileAsync(fileId);
                Console.WriteLine("  Primary file retrieved successfully");
                Console.WriteLine($"  File size: {fileData.Length} bytes");
            }
            catch (FastDFSFileNotFoundException)
            {
                // File not found - fall back to alternative content
                Console.WriteLine("  Primary file not found, falling back to alternative content...");

                // Fallback options:
                // 1. Use placeholder/default content
                // 2. Use cached version if available
                // 3. Use alternative file ID
                // 4. Generate content on the fly

                var fallbackContent = Encoding.UTF8.GetBytes("This is fallback/placeholder content.");
                Console.WriteLine("  Using fallback content:");
                Console.WriteLine($"    Size: {fallbackContent.Length} bytes");
                Console.WriteLine($"    Content: {Encoding.UTF8.GetString(fallbackContent)}");
            }
            catch (FastDFSNetworkException)
            {
                // Network error - try fallback
                Console.WriteLine("  Network error, trying fallback content...");
                var fallbackContent = Encoding.UTF8.GetBytes("Fallback content due to network error.");
                Console.WriteLine($"  Using fallback content: {Encoding.UTF8.GetString(fallbackContent)}");
            }
        }

        /// <summary>
        /// Demonstrates graceful degradation when some operations fail.
        /// 
        /// This strategy allows the application to continue operating with
        /// reduced functionality when some operations fail.
        /// </summary>
        /// <param name="client">FastDFS client instance.</param>
        /// <returns>A task that represents the asynchronous operation.</returns>
        static async Task GracefulDegradation(FastDFSClient client)
        {
            Console.WriteLine("Attempting multiple operations with graceful degradation...");
            Console.WriteLine();

            var results = new Dictionary<string, string>();

            // Operation 1: Upload file
            try
            {
                var testFile = "test_graceful_1.txt";
                if (!File.Exists(testFile))
                {
                    await File.WriteAllTextAsync(testFile, "Test file 1");
                }

                var fileId1 = await client.UploadFileAsync(testFile, null);
                results["file1"] = fileId1;
                Console.WriteLine("  Operation 1: Upload successful");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  Operation 1: Failed - {ex.Message}");
                results["file1"] = "failed";
            }

            // Operation 2: Upload another file
            try
            {
                var testFile2 = "test_graceful_2.txt";
                if (!File.Exists(testFile2))
                {
                    await File.WriteAllTextAsync(testFile2, "Test file 2");
                }

                var fileId2 = await client.UploadFileAsync(testFile2, null);
                results["file2"] = fileId2;
                Console.WriteLine("  Operation 2: Upload successful");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  Operation 2: Failed - {ex.Message}");
                results["file2"] = "failed";
            }

            // Continue with available results
            Console.WriteLine();
            Console.WriteLine("  Summary:");
            foreach (var kvp in results)
            {
                Console.WriteLine($"    {kvp.Key}: {kvp.Value}");
            }

            Console.WriteLine("  Application continues with available functionality");
        }

        /// <summary>
        /// Demonstrates retry with different parameters when initial attempt fails.
        /// 
        /// This strategy retries operations with modified parameters such as
        /// different timeout values or server addresses.
        /// </summary>
        /// <param name="client">FastDFS client instance.</param>
        /// <param name="filePath">Path to the file to upload.</param>
        /// <returns>A task that represents the asynchronous operation.</returns>
        static async Task RetryWithDifferentParameters(FastDFSClient client, string filePath)
        {
            Console.WriteLine("Attempting operation with retry using different parameters...");
            Console.WriteLine();

            // Initial attempt with default parameters
            try
            {
                if (!File.Exists(filePath))
                {
                    await File.WriteAllTextAsync(filePath, "Test file for different parameters");
                }

                Console.WriteLine("  Attempt 1: Using default parameters...");
                var fileId = await client.UploadFileAsync(filePath, null);
                Console.WriteLine($"  Success! File ID: {fileId}");
                return;
            }
            catch (FastDFSNetworkTimeoutException ex)
            {
                Console.WriteLine($"  Attempt 1 failed: Network timeout - {ex.Message}");
                Console.WriteLine("  Retrying with increased timeout...");

                // Note: In a real scenario, you would create a new client
                // with increased timeout. For this example, we'll just
                // demonstrate the concept
                Console.WriteLine("  (In production, retry with increased NetworkTimeout)");
            }
            catch (FastDFSConnectionTimeoutException ex)
            {
                Console.WriteLine($"  Attempt 1 failed: Connection timeout - {ex.Message}");
                Console.WriteLine("  Retrying with increased connection timeout...");
                Console.WriteLine("  (In production, retry with increased ConnectTimeout)");
            }
        }
    }
}

