// ============================================================================
// FastDFS C# Client - Cancellation Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates cancellation token usage, long-running operations,
// graceful shutdown, timeout handling, and resource cleanup in the FastDFS
// C# client library. It shows how to properly handle cancellation, timeouts,
// and resource management in FastDFS applications.
//
// Proper cancellation handling is essential for building responsive applications
// that can gracefully handle user cancellation, timeouts, and shutdown scenarios.
// This example provides comprehensive patterns for cancellation and resource
// management in FastDFS operations.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating cancellation token usage and resource management in FastDFS.
    /// 
    /// This example shows:
    /// - How to use cancellation tokens with FastDFS operations
    /// - How to handle long-running operations with cancellation
    /// - How to implement graceful shutdown
    /// - How to handle timeouts and cancellation
    /// - How to properly clean up resources
    /// 
    /// Cancellation patterns demonstrated:
    /// 1. Basic cancellation token usage
    /// 2. Long-running operation cancellation
    /// 3. Graceful shutdown patterns
    /// 4. Timeout handling with cancellation
    /// 5. Resource cleanup on cancellation
    /// 6. Multiple operation cancellation
    /// 7. Cancellation propagation
    /// </summary>
    class CancellationExample
    {
        /// <summary>
        /// Main entry point for the cancellation example.
        /// 
        /// This method demonstrates various cancellation patterns through
        /// a series of examples, each showing different aspects of cancellation
        /// and resource management in FastDFS operations.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Cancellation Example");
            Console.WriteLine("==========================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates cancellation token usage,");
            Console.WriteLine("long-running operations, graceful shutdown, and resource cleanup.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For cancellation examples, standard configuration is sufficient.
            // ====================================================================

            var config = new FastDFSClientConfig
            {
                // Specify tracker server addresses
                TrackerAddresses = new[]
                {
                    "192.168.1.100:22122",  // Primary tracker server
                    "192.168.1.101:22122"   // Secondary tracker server (for redundancy)
                },

                // Maximum number of connections per server
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Idle timeout: time before idle connections are closed
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations with cancellation token support.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Basic Cancellation Token Usage
                    // ============================================================
                    // 
                    // This example demonstrates basic usage of cancellation
                    // tokens with FastDFS operations. Cancellation tokens allow
                    // operations to be cancelled gracefully.
                    // 
                    // Basic cancellation patterns:
                    // - Creating cancellation token sources
                    // - Passing cancellation tokens to operations
                    // - Handling OperationCanceledException
                    // ============================================================

                    Console.WriteLine("Example 1: Basic Cancellation Token Usage");
                    Console.WriteLine("==========================================");
                    Console.WriteLine();

                    // Create a test file for cancellation examples
                    Console.WriteLine("Creating test file for cancellation examples...");
                    Console.WriteLine();

                    var testFile = "cancellation_test.txt";
                    var testContent = "This is a test file for cancellation examples. " +
                                     "It demonstrates how to use cancellation tokens with FastDFS operations.";

                    await File.WriteAllTextAsync(testFile, testContent);
                    Console.WriteLine($"Test file created: {testFile}");
                    Console.WriteLine();

                    // Pattern 1: Normal operation without cancellation
                    Console.WriteLine("Pattern 1: Normal Operation Without Cancellation");
                    Console.WriteLine("--------------------------------------------------");
                    Console.WriteLine();

                    try
                    {
                        Console.WriteLine("Uploading file without cancellation...");
                        var fileId = await client.UploadFileAsync(testFile, null);
                        Console.WriteLine($"  File uploaded successfully: {fileId}");
                        Console.WriteLine();
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"  Upload failed: {ex.Message}");
                        Console.WriteLine();
                    }

                    // Pattern 2: Operation with cancellation token
                    Console.WriteLine("Pattern 2: Operation With Cancellation Token");
                    Console.WriteLine("---------------------------------------------");
                    Console.WriteLine();

                    // Create a cancellation token source
                    // CancellationTokenSource allows creating and controlling cancellation tokens
                    using (var cts = new CancellationTokenSource())
                    {
                        try
                        {
                            Console.WriteLine("Uploading file with cancellation token...");
                            var fileId = await client.UploadFileAsync(testFile, null, cts.Token);
                            Console.WriteLine($"  File uploaded successfully: {fileId}");
                            Console.WriteLine();
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  Operation was cancelled");
                            Console.WriteLine();
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"  Upload failed: {ex.Message}");
                            Console.WriteLine();
                        }
                    }

                    // Pattern 3: Cancelling an operation
                    Console.WriteLine("Pattern 3: Cancelling an Operation");
                    Console.WriteLine("------------------------------------");
                    Console.WriteLine();

                    using (var cts = new CancellationTokenSource())
                    {
                        // Start operation in background
                        var uploadTask = Task.Run(async () =>
                        {
                            try
                            {
                                Console.WriteLine("  Starting upload operation...");
                                await Task.Delay(100);  // Simulate some work
                                return await client.UploadFileAsync(testFile, null, cts.Token);
                            }
                            catch (OperationCanceledException)
                            {
                                Console.WriteLine("  Upload operation was cancelled");
                                return null;
                            }
                        });

                        // Cancel after a short delay
                        await Task.Delay(50);
                        Console.WriteLine("  Cancelling operation...");
                        cts.Cancel();

                        try
                        {
                            await uploadTask;
                            Console.WriteLine("  Operation completed (may have been cancelled)");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  Operation cancellation confirmed");
                        }

                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 2: Long-Running Operations with Cancellation
                    // ============================================================
                    // 
                    // This example demonstrates handling long-running operations
                    // with cancellation support. Long-running operations benefit
                    // from cancellation to allow users to stop operations that
                    // take too long.
                    // 
                    // Long-running operation patterns:
                    // - Cancellation during long uploads
                    // - Cancellation during long downloads
                    // - Progress reporting with cancellation
                    // - Batch operation cancellation
                    // ============================================================

                    Console.WriteLine("Example 2: Long-Running Operations with Cancellation");
                    Console.WriteLine("======================================================");
                    Console.WriteLine();

                    // Create a larger test file for long-running operations
                    Console.WriteLine("Creating larger test file for long-running operations...");
                    Console.WriteLine();

                    var largeTestFile = "large_cancellation_test.txt";
                    var largeContent = new StringBuilder();
                    for (int i = 0; i < 10000; i++)
                    {
                        largeContent.AppendLine($"Line {i + 1}: This is a test line for long-running operation cancellation examples.");
                    }

                    await File.WriteAllTextAsync(largeTestFile, largeContent.ToString());
                    var fileInfo = new FileInfo(largeTestFile);
                    Console.WriteLine($"Large test file created: {largeTestFile} ({fileInfo.Length:N0} bytes)");
                    Console.WriteLine();

                    // Pattern 1: Cancelling a long upload
                    Console.WriteLine("Pattern 1: Cancelling a Long Upload");
                    Console.WriteLine("------------------------------------");
                    Console.WriteLine();

                    using (var cts = new CancellationTokenSource())
                    {
                        try
                        {
                            Console.WriteLine("Starting long upload operation...");
                            Console.WriteLine("  (In a real scenario, this would be a large file upload)");

                            // Simulate long-running upload with cancellation support
                            var uploadTask = client.UploadFileAsync(largeTestFile, null, cts.Token);

                            // Cancel after a delay (simulating user cancellation)
                            await Task.Delay(200);
                            Console.WriteLine("  User requested cancellation...");
                            cts.Cancel();

                            await uploadTask;
                            Console.WriteLine("  Upload completed");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  ✓ Upload was successfully cancelled");
                            Console.WriteLine("  ✓ Resources were cleaned up");
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"  Upload error: {ex.Message}");
                        }

                        Console.WriteLine();
                    }

                    // Pattern 2: Long download with cancellation
                    Console.WriteLine("Pattern 2: Long Download with Cancellation");
                    Console.WriteLine("--------------------------------------------");
                    Console.WriteLine();

                    // First upload a file to download
                    string downloadFileId = null;
                    try
                    {
                        Console.WriteLine("Uploading file for download example...");
                        downloadFileId = await client.UploadFileAsync(largeTestFile, null);
                        Console.WriteLine($"  File uploaded: {downloadFileId}");
                        Console.WriteLine();
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"  Upload failed: {ex.Message}");
                        Console.WriteLine();
                    }

                    if (downloadFileId != null)
                    {
                        using (var cts = new CancellationTokenSource())
                        {
                            try
                            {
                                Console.WriteLine("Starting long download operation...");
                                Console.WriteLine("  (In a real scenario, this would be a large file download)");

                                // Simulate long-running download with cancellation support
                                var downloadTask = client.DownloadFileAsync(downloadFileId, cts.Token);

                                // Cancel after a delay (simulating user cancellation)
                                await Task.Delay(200);
                                Console.WriteLine("  User requested cancellation...");
                                cts.Cancel();

                                await downloadTask;
                                Console.WriteLine("  Download completed");
                            }
                            catch (OperationCanceledException)
                            {
                                Console.WriteLine("  ✓ Download was successfully cancelled");
                                Console.WriteLine("  ✓ Resources were cleaned up");
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"  Download error: {ex.Message}");
                            }

                            Console.WriteLine();
                        }
                    }

                    // Pattern 3: Progress reporting with cancellation
                    Console.WriteLine("Pattern 3: Progress Reporting with Cancellation");
                    Console.WriteLine("--------------------------------------------------");
                    Console.WriteLine();

                    using (var cts = new CancellationTokenSource())
                    {
                        try
                        {
                            Console.WriteLine("Starting operation with progress reporting...");

                            // Simulate operation with progress updates
                            var progressTask = Task.Run(async () =>
                            {
                                for (int i = 0; i < 10; i++)
                                {
                                    cts.Token.ThrowIfCancellationRequested();
                                    Console.WriteLine($"  Progress: {(i + 1) * 10}%");
                                    await Task.Delay(100, cts.Token);
                                }
                            }, cts.Token);

                            // Cancel after some progress
                            await Task.Delay(500);
                            Console.WriteLine("  Cancelling operation...");
                            cts.Cancel();

                            await progressTask;
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  ✓ Operation cancelled with progress tracking");
                        }

                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 3: Graceful Shutdown
                    // ============================================================
                    // 
                    // This example demonstrates implementing graceful shutdown
                    // patterns for FastDFS applications. Graceful shutdown ensures
                    // that operations complete or are cancelled cleanly when
                    // the application is shutting down.
                    // 
                    // Graceful shutdown patterns:
                    // - Shutdown signal handling
                    // - Completing in-progress operations
                    // - Cancelling pending operations
                    // - Resource cleanup
                    // ============================================================

                    Console.WriteLine("Example 3: Graceful Shutdown");
                    Console.WriteLine("============================");
                    Console.WriteLine();

                    // Pattern 1: Shutdown signal handling
                    Console.WriteLine("Pattern 1: Shutdown Signal Handling");
                    Console.WriteLine("------------------------------------");
                    Console.WriteLine();

                    // Create a cancellation token source for shutdown
                    using (var shutdownCts = new CancellationTokenSource())
                    {
                        // Simulate shutdown signal (e.g., from Console.CancelKeyPress)
                        Console.WriteLine("Simulating graceful shutdown scenario...");
                        Console.WriteLine();

                        // Start some operations
                        var operations = new List<Task<string>>();
                        for (int i = 0; i < 5; i++)
                        {
                            var operationIndex = i;
                            var operation = Task.Run(async () =>
                            {
                                try
                                {
                                    Console.WriteLine($"  Operation {operationIndex + 1} started");
                                    await Task.Delay(1000, shutdownCts.Token);
                                    Console.WriteLine($"  Operation {operationIndex + 1} completed");
                                    return $"result_{operationIndex + 1}";
                                }
                                catch (OperationCanceledException)
                                {
                                    Console.WriteLine($"  Operation {operationIndex + 1} cancelled during shutdown");
                                    return null;
                                }
                            }, shutdownCts.Token);

                            operations.Add(operation);
                        }

                        // Simulate shutdown signal after a delay
                        await Task.Delay(500);
                        Console.WriteLine();
                        Console.WriteLine("Shutdown signal received...");
                        Console.WriteLine("  Initiating graceful shutdown...");
                        Console.WriteLine();

                        // Request cancellation for all operations
                        shutdownCts.Cancel();

                        // Wait for operations to complete or cancel
                        try
                        {
                            await Task.WhenAll(operations);
                            Console.WriteLine("  All operations handled during shutdown");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  Operations were cancelled during shutdown");
                        }

                        Console.WriteLine();
                    }

                    // Pattern 2: Completing in-progress operations
                    Console.WriteLine("Pattern 2: Completing In-Progress Operations");
                    Console.WriteLine("----------------------------------------------");
                    Console.WriteLine();

                    using (var shutdownCts = new CancellationTokenSource())
                    {
                        Console.WriteLine("Starting operations that should complete during shutdown...");
                        Console.WriteLine();

                        var inProgressOperations = new List<Task>();

                        // Start operations that should complete
                        for (int i = 0; i < 3; i++)
                        {
                            var operationIndex = i;
                            var operation = Task.Run(async () =>
                            {
                                try
                                {
                                    Console.WriteLine($"  Operation {operationIndex + 1} started");
                                    // Short operation that should complete
                                    await Task.Delay(100, shutdownCts.Token);
                                    Console.WriteLine($"  ✓ Operation {operationIndex + 1} completed");
                                }
                                catch (OperationCanceledException)
                                {
                                    Console.WriteLine($"  ✗ Operation {operationIndex + 1} was cancelled");
                                }
                            }, shutdownCts.Token);

                            inProgressOperations.Add(operation);
                        }

                        // Wait a bit, then initiate shutdown
                        await Task.Delay(50);
                        Console.WriteLine();
                        Console.WriteLine("Initiating shutdown (allowing in-progress operations to complete)...");
                        Console.WriteLine();

                        // Give operations time to complete before cancelling
                        await Task.Delay(200);
                        shutdownCts.Cancel();

                        try
                        {
                            await Task.WhenAll(inProgressOperations);
                            Console.WriteLine("  All in-progress operations completed");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  Some operations were cancelled");
                        }

                        Console.WriteLine();
                    }

                    // Pattern 3: Resource cleanup during shutdown
                    Console.WriteLine("Pattern 3: Resource Cleanup During Shutdown");
                    Console.WriteLine("---------------------------------------------");
                    Console.WriteLine();

                    using (var shutdownCts = new CancellationTokenSource())
                    {
                        var resources = new List<string> { "resource1", "resource2", "resource3" };

                        Console.WriteLine("Managing resources during shutdown...");
                        Console.WriteLine($"  Active resources: {resources.Count}");
                        Console.WriteLine();

                        // Simulate resource usage
                        var resourceTask = Task.Run(async () =>
                        {
                            try
                            {
                                foreach (var resource in resources)
                                {
                                    shutdownCts.Token.ThrowIfCancellationRequested();
                                    Console.WriteLine($"  Using resource: {resource}");
                                    await Task.Delay(200, shutdownCts.Token);
                                }
                            }
                            catch (OperationCanceledException)
                            {
                                Console.WriteLine("  Shutdown detected, cleaning up resources...");
                                
                                // Clean up resources
                                foreach (var resource in resources)
                                {
                                    Console.WriteLine($"    Cleaning up: {resource}");
                                }

                                resources.Clear();
                                Console.WriteLine("  ✓ All resources cleaned up");
                            }
                        }, shutdownCts.Token);

                        // Initiate shutdown
                        await Task.Delay(400);
                        Console.WriteLine();
                        Console.WriteLine("Initiating shutdown...");
                        shutdownCts.Cancel();

                        try
                        {
                            await resourceTask;
                        }
                        catch (OperationCanceledException)
                        {
                            // Expected during shutdown
                        }

                        Console.WriteLine($"  Remaining resources: {resources.Count}");
                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 4: Timeout Handling
                    // ============================================================
                    // 
                    // This example demonstrates handling timeouts using cancellation
                    // tokens. Timeouts are important for preventing operations from
                    // running indefinitely and for providing better user experience.
                    // 
                    // Timeout patterns:
                    // - Operation timeout with cancellation
                    // - Per-operation timeout
                    // - Global timeout for multiple operations
                    // - Timeout with retry
                    // ============================================================

                    Console.WriteLine("Example 4: Timeout Handling");
                    Console.WriteLine("===========================");
                    Console.WriteLine();

                    // Pattern 1: Operation timeout
                    Console.WriteLine("Pattern 1: Operation Timeout");
                    Console.WriteLine("------------------------------");
                    Console.WriteLine();

                    using (var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(2)))
                    {
                        try
                        {
                            Console.WriteLine("Starting operation with 2-second timeout...");

                            // Simulate operation that might take too long
                            var operation = Task.Run(async () =>
                            {
                                await Task.Delay(5000, timeoutCts.Token);  // 5 seconds (will timeout)
                                return "Operation completed";
                            }, timeoutCts.Token);

                            await operation;
                            Console.WriteLine("  Operation completed");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  ✓ Operation timed out after 2 seconds");
                            Console.WriteLine("  ✓ Timeout handled gracefully");
                        }

                        Console.WriteLine();
                    }

                    // Pattern 2: Per-operation timeout
                    Console.WriteLine("Pattern 2: Per-Operation Timeout");
                    Console.WriteLine("----------------------------------");
                    Console.WriteLine();

                    var operationsWithTimeouts = new[]
                    {
                        (Name: "Operation 1", Timeout: TimeSpan.FromSeconds(1)),
                        (Name: "Operation 2", Timeout: TimeSpan.FromSeconds(2)),
                        (Name: "Operation 3", Timeout: TimeSpan.FromSeconds(3))
                    };

                    foreach (var (name, timeout) in operationsWithTimeouts)
                    {
                        using (var operationCts = new CancellationTokenSource(timeout))
                        {
                            try
                            {
                                Console.WriteLine($"Starting {name} with {timeout.TotalSeconds}s timeout...");

                                var operation = Task.Run(async () =>
                                {
                                    await Task.Delay(5000, operationCts.Token);  // Will timeout
                                    return $"{name} completed";
                                }, operationCts.Token);

                                await operation;
                                Console.WriteLine($"  {name} completed");
                            }
                            catch (OperationCanceledException)
                            {
                                Console.WriteLine($"  ✓ {name} timed out after {timeout.TotalSeconds} seconds");
                            }

                            Console.WriteLine();
                        }
                    }

                    // Pattern 3: Global timeout for multiple operations
                    Console.WriteLine("Pattern 3: Global Timeout for Multiple Operations");
                    Console.WriteLine("---------------------------------------------------");
                    Console.WriteLine();

                    using (var globalTimeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(3)))
                    {
                        try
                        {
                            Console.WriteLine("Starting multiple operations with 3-second global timeout...");
                            Console.WriteLine();

                            var multipleOperations = new List<Task>();
                            for (int i = 0; i < 5; i++)
                            {
                                var operationIndex = i;
                                var operation = Task.Run(async () =>
                                {
                                    try
                                    {
                                        Console.WriteLine($"  Operation {operationIndex + 1} started");
                                        await Task.Delay(2000, globalTimeoutCts.Token);
                                        Console.WriteLine($"  Operation {operationIndex + 1} completed");
                                    }
                                    catch (OperationCanceledException)
                                    {
                                        Console.WriteLine($"  Operation {operationIndex + 1} cancelled (timeout)");
                                    }
                                }, globalTimeoutCts.Token);

                                multipleOperations.Add(operation);
                            }

                            await Task.WhenAll(multipleOperations);
                            Console.WriteLine();
                            Console.WriteLine("  All operations completed");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine();
                            Console.WriteLine("  ✓ Global timeout reached, operations cancelled");
                        }

                        Console.WriteLine();
                    }

                    // Pattern 4: Timeout with retry
                    Console.WriteLine("Pattern 4: Timeout with Retry");
                    Console.WriteLine("-------------------------------");
                    Console.WriteLine();

                    var maxRetries = 3;
                    var operationTimeout = TimeSpan.FromSeconds(1);

                    for (int attempt = 1; attempt <= maxRetries; attempt++)
                    {
                        using (var retryCts = new CancellationTokenSource(operationTimeout))
                        {
                            try
                            {
                                Console.WriteLine($"Attempt {attempt} of {maxRetries} (timeout: {operationTimeout.TotalSeconds}s)...");

                                var operation = Task.Run(async () =>
                                {
                                    await Task.Delay(2000, retryCts.Token);  // Will timeout
                                    return "Operation completed";
                                }, retryCts.Token);

                                await operation;
                                Console.WriteLine($"  ✓ Operation succeeded on attempt {attempt}");
                                break;
                            }
                            catch (OperationCanceledException)
                            {
                                Console.WriteLine($"  ✗ Attempt {attempt} timed out");
                                if (attempt < maxRetries)
                                {
                                    Console.WriteLine($"  Retrying...");
                                    await Task.Delay(500);  // Brief delay before retry
                                }
                                else
                                {
                                    Console.WriteLine($"  ✗ All {maxRetries} attempts timed out");
                                }
                            }

                            Console.WriteLine();
                        }
                    }

                    // ============================================================
                    // Example 5: Resource Cleanup
                    // ============================================================
                    // 
                    // This example demonstrates proper resource cleanup when
                    // operations are cancelled. Resource cleanup ensures that
                    // resources are properly released even when operations are
                    // cancelled or fail.
                    // 
                    // Resource cleanup patterns:
                    // - Using statements for automatic cleanup
                    // - Finally blocks for cleanup
                    // - Cleanup on cancellation
                    // - Cleanup in exception handlers
                    // ============================================================

                    Console.WriteLine("Example 5: Resource Cleanup");
                    Console.WriteLine("============================");
                    Console.WriteLine();

                    // Pattern 1: Using statements for automatic cleanup
                    Console.WriteLine("Pattern 1: Using Statements for Automatic Cleanup");
                    Console.WriteLine("---------------------------------------------------");
                    Console.WriteLine();

                    // The FastDFS client implements IDisposable and should be used with 'using'
                    Console.WriteLine("Using 'using' statement for automatic client disposal...");
                    Console.WriteLine();

                    // Client is already in a using statement, demonstrating the pattern
                    Console.WriteLine("  ✓ Client is in a 'using' statement");
                    Console.WriteLine("  ✓ Resources will be automatically cleaned up");
                    Console.WriteLine("  ✓ Connections will be properly closed");
                    Console.WriteLine();

                    // Pattern 2: Finally blocks for cleanup
                    Console.WriteLine("Pattern 2: Finally Blocks for Cleanup");
                    Console.WriteLine("--------------------------------------");
                    Console.WriteLine();

                    var tempFile = "temp_cleanup_test.txt";
                    try
                    {
                        Console.WriteLine("Creating temporary file...");
                        await File.WriteAllTextAsync(tempFile, "Temporary file content");
                        Console.WriteLine($"  Temporary file created: {tempFile}");

                        // Simulate operation that might fail or be cancelled
                        using (var cleanupCts = new CancellationTokenSource())
                        {
                            var operation = Task.Run(async () =>
                            {
                                await Task.Delay(100, cleanupCts.Token);
                                return "Operation completed";
                            }, cleanupCts.Token);

                            await operation;
                        }
                    }
                    catch (OperationCanceledException)
                    {
                        Console.WriteLine("  Operation was cancelled");
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"  Operation failed: {ex.Message}");
                    }
                    finally
                    {
                        // Cleanup in finally block ensures it always runs
                        if (File.Exists(tempFile))
                        {
                            File.Delete(tempFile);
                            Console.WriteLine($"  ✓ Temporary file cleaned up: {tempFile}");
                        }
                    }

                    Console.WriteLine();

                    // Pattern 3: Cleanup on cancellation
                    Console.WriteLine("Pattern 3: Cleanup on Cancellation");
                    Console.WriteLine("------------------------------------");
                    Console.WriteLine();

                    var resourcesToCleanup = new List<string> { "resource_a", "resource_b", "resource_c" };

                    using (var cleanupCts = new CancellationTokenSource())
                    {
                        try
                        {
                            Console.WriteLine("Using resources...");
                            foreach (var resource in resourcesToCleanup)
                            {
                                cleanupCts.Token.ThrowIfCancellationRequested();
                                Console.WriteLine($"  Using: {resource}");
                                await Task.Delay(200, cleanupCts.Token);
                            }

                            Console.WriteLine("  All resources used successfully");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  Operation cancelled, cleaning up resources...");

                            // Cleanup on cancellation
                            foreach (var resource in resourcesToCleanup)
                            {
                                Console.WriteLine($"    Cleaning up: {resource}");
                            }

                            resourcesToCleanup.Clear();
                            Console.WriteLine("  ✓ All resources cleaned up");
                        }
                    }

                    Console.WriteLine();

                    // Pattern 4: Cleanup in exception handlers
                    Console.WriteLine("Pattern 4: Cleanup in Exception Handlers");
                    Console.WriteLine("------------------------------------------");
                    Console.WriteLine();

                    var operationResources = new List<string> { "op_resource_1", "op_resource_2" };

                    try
                    {
                        Console.WriteLine("Starting operation with resources...");

                        // Simulate operation that might throw
                        throw new InvalidOperationException("Simulated operation failure");
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"  Operation failed: {ex.Message}");
                        Console.WriteLine("  Cleaning up resources in exception handler...");

                        // Cleanup in exception handler
                        foreach (var resource in operationResources)
                        {
                            Console.WriteLine($"    Cleaning up: {resource}");
                        }

                        operationResources.Clear();
                        Console.WriteLine("  ✓ Resources cleaned up in exception handler");
                    }

                    Console.WriteLine();

                    // ============================================================
                    // Example 6: Multiple Operation Cancellation
                    // ============================================================
                    // 
                    // This example demonstrates cancelling multiple operations
                    // simultaneously using a shared cancellation token. This is
                    // useful for batch operations and coordinated cancellation.
                    // 
                    // Multiple operation patterns:
                    // - Shared cancellation token
                    // - Coordinated cancellation
                    // - Partial operation cancellation
                    // ============================================================

                    Console.WriteLine("Example 6: Multiple Operation Cancellation");
                    Console.WriteLine("==========================================");
                    Console.WriteLine();

                    // Pattern 1: Shared cancellation token
                    Console.WriteLine("Pattern 1: Shared Cancellation Token");
                    Console.WriteLine("--------------------------------------");
                    Console.WriteLine();

                    using (var sharedCts = new CancellationTokenSource())
                    {
                        Console.WriteLine("Starting multiple operations with shared cancellation token...");
                        Console.WriteLine();

                        var sharedOperations = new List<Task>();
                        for (int i = 0; i < 5; i++)
                        {
                            var operationIndex = i;
                            var operation = Task.Run(async () =>
                            {
                                try
                                {
                                    Console.WriteLine($"  Operation {operationIndex + 1} started");
                                    await Task.Delay(1000, sharedCts.Token);
                                    Console.WriteLine($"  Operation {operationIndex + 1} completed");
                                }
                                catch (OperationCanceledException)
                                {
                                    Console.WriteLine($"  Operation {operationIndex + 1} cancelled");
                                }
                            }, sharedCts.Token);

                            sharedOperations.Add(operation);
                        }

                        // Cancel all operations after a delay
                        await Task.Delay(500);
                        Console.WriteLine();
                        Console.WriteLine("Cancelling all operations with shared token...");
                        sharedCts.Cancel();

                        try
                        {
                            await Task.WhenAll(sharedOperations);
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  ✓ All operations cancelled via shared token");
                        }

                        Console.WriteLine();
                    }

                    // Pattern 2: Coordinated cancellation
                    Console.WriteLine("Pattern 2: Coordinated Cancellation");
                    Console.WriteLine("------------------------------------");
                    Console.WriteLine();

                    using (var coordinatorCts = new CancellationTokenSource())
                    {
                        Console.WriteLine("Starting coordinated operations...");
                        Console.WriteLine();

                        var coordinatedOperations = new List<Task>();
                        var operationGroups = new[]
                        {
                            new[] { "Operation A1", "Operation A2" },
                            new[] { "Operation B1", "Operation B2", "Operation B3" }
                        };

                        foreach (var group in operationGroups)
                        {
                            foreach (var opName in group)
                            {
                                var operation = Task.Run(async () =>
                                {
                                    try
                                    {
                                        Console.WriteLine($"  {opName} started");
                                        await Task.Delay(800, coordinatorCts.Token);
                                        Console.WriteLine($"  {opName} completed");
                                    }
                                    catch (OperationCanceledException)
                                    {
                                        Console.WriteLine($"  {opName} cancelled");
                                    }
                                }, coordinatorCts.Token);

                                coordinatedOperations.Add(operation);
                            }
                        }

                        // Cancel all coordinated operations
                        await Task.Delay(400);
                        Console.WriteLine();
                        Console.WriteLine("Coordinating cancellation of all operations...");
                        coordinatorCts.Cancel();

                        try
                        {
                            await Task.WhenAll(coordinatedOperations);
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  ✓ All coordinated operations cancelled");
                        }

                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 7: Cancellation Propagation
                    // ============================================================
                    // 
                    // This example demonstrates how cancellation propagates
                    // through operation chains and nested operations. Understanding
                    // cancellation propagation is important for building robust
                    // cancellation-aware applications.
                    // 
                    // Cancellation propagation patterns:
                    // - Propagation through operation chains
                    // - Nested operation cancellation
                    // - Cancellation token linking
                    // ============================================================

                    Console.WriteLine("Example 7: Cancellation Propagation");
                    Console.WriteLine("======================================");
                    Console.WriteLine();

                    // Pattern 1: Propagation through operation chains
                    Console.WriteLine("Pattern 1: Propagation Through Operation Chains");
                    Console.WriteLine("--------------------------------------------------");
                    Console.WriteLine();

                    using (var chainCts = new CancellationTokenSource())
                    {
                        try
                        {
                            Console.WriteLine("Starting operation chain...");

                            // Chain of operations
                            var result1 = await Task.Run(async () =>
                            {
                                Console.WriteLine("  Step 1: Starting");
                                await Task.Delay(200, chainCts.Token);
                                Console.WriteLine("  Step 1: Completed");
                                return "Step1Result";
                            }, chainCts.Token);

                            var result2 = await Task.Run(async () =>
                            {
                                Console.WriteLine("  Step 2: Starting");
                                chainCts.Token.ThrowIfCancellationRequested();
                                await Task.Delay(200, chainCts.Token);
                                Console.WriteLine("  Step 2: Completed");
                                return "Step2Result";
                            }, chainCts.Token);

                            var result3 = await Task.Run(async () =>
                            {
                                Console.WriteLine("  Step 3: Starting");
                                await Task.Delay(200, chainCts.Token);
                                Console.WriteLine("  Step 3: Completed");
                                return "Step3Result";
                            }, chainCts.Token);

                            Console.WriteLine("  ✓ Operation chain completed");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  ✓ Cancellation propagated through operation chain");
                        }

                        Console.WriteLine();
                    }

                    // Pattern 2: Nested operation cancellation
                    Console.WriteLine("Pattern 2: Nested Operation Cancellation");
                    Console.WriteLine("------------------------------------------");
                    Console.WriteLine();

                    using (var parentCts = new CancellationTokenSource())
                    {
                        try
                        {
                            Console.WriteLine("Starting nested operations...");

                            await Task.Run(async () =>
                            {
                                Console.WriteLine("  Parent operation started");

                                await Task.Run(async () =>
                                {
                                    Console.WriteLine("    Child operation 1 started");
                                    await Task.Delay(300, parentCts.Token);
                                    Console.WriteLine("    Child operation 1 completed");
                                }, parentCts.Token);

                                await Task.Run(async () =>
                                {
                                    Console.WriteLine("    Child operation 2 started");
                                    await Task.Delay(300, parentCts.Token);
                                    Console.WriteLine("    Child operation 2 completed");
                                }, parentCts.Token);

                                Console.WriteLine("  Parent operation completed");
                            }, parentCts.Token);

                            Console.WriteLine("  ✓ Nested operations completed");
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine("  ✓ Cancellation propagated to nested operations");
                        }

                        Console.WriteLine();
                    }

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for cancellation
                    // and resource management in FastDFS applications.
                    // ============================================================

                    Console.WriteLine("Best Practices for Cancellation and Resource Management");
                    Console.WriteLine("=========================================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Cancellation Token Usage:");
                    Console.WriteLine("   - Always pass cancellation tokens to async operations");
                    Console.WriteLine("   - Check cancellation tokens in loops and long operations");
                    Console.WriteLine("   - Handle OperationCanceledException appropriately");
                    Console.WriteLine("   - Use CancellationTokenSource for creating tokens");
                    Console.WriteLine();
                    Console.WriteLine("2. Long-Running Operations:");
                    Console.WriteLine("   - Support cancellation in long-running operations");
                    Console.WriteLine("   - Provide progress updates during long operations");
                    Console.WriteLine("   - Allow users to cancel long operations");
                    Console.WriteLine("   - Clean up resources when operations are cancelled");
                    Console.WriteLine();
                    Console.WriteLine("3. Graceful Shutdown:");
                    Console.WriteLine("   - Use cancellation tokens for shutdown signals");
                    Console.WriteLine("   - Allow in-progress operations to complete when possible");
                    Console.WriteLine("   - Cancel pending operations during shutdown");
                    Console.WriteLine("   - Clean up all resources during shutdown");
                    Console.WriteLine();
                    Console.WriteLine("4. Timeout Handling:");
                    Console.WriteLine("   - Set appropriate timeouts for operations");
                    Console.WriteLine("   - Use CancellationTokenSource with timeout");
                    Console.WriteLine("   - Handle timeout exceptions gracefully");
                    Console.WriteLine("   - Consider retry with timeout for transient failures");
                    Console.WriteLine();
                    Console.WriteLine("5. Resource Cleanup:");
                    Console.WriteLine("   - Use 'using' statements for IDisposable resources");
                    Console.WriteLine("   - Clean up in finally blocks");
                    Console.WriteLine("   - Clean up on cancellation");
                    Console.WriteLine("   - Clean up in exception handlers");
                    Console.WriteLine();
                    Console.WriteLine("6. Multiple Operation Cancellation:");
                    Console.WriteLine("   - Use shared cancellation tokens for related operations");
                    Console.WriteLine("   - Coordinate cancellation of multiple operations");
                    Console.WriteLine("   - Handle partial operation completion");
                    Console.WriteLine();
                    Console.WriteLine("7. Cancellation Propagation:");
                    Console.WriteLine("   - Pass cancellation tokens through operation chains");
                    Console.WriteLine("   - Ensure nested operations respect cancellation");
                    Console.WriteLine("   - Link cancellation tokens when needed");
                    Console.WriteLine();
                    Console.WriteLine("8. Error Handling:");
                    Console.WriteLine("   - Distinguish between cancellation and other errors");
                    Console.WriteLine("   - Handle OperationCanceledException separately");
                    Console.WriteLine("   - Clean up resources in all error scenarios");
                    Console.WriteLine();
                    Console.WriteLine("9. Performance Considerations:");
                    Console.WriteLine("   - Check cancellation tokens frequently in loops");
                    Console.WriteLine("   - Avoid expensive operations after cancellation");
                    Console.WriteLine("   - Minimize overhead of cancellation checks");
                    Console.WriteLine();
                    Console.WriteLine("10. Best Practices Summary:");
                    Console.WriteLine("    - Always support cancellation in async operations");
                    Console.WriteLine("    - Implement graceful shutdown patterns");
                    Console.WriteLine("    - Use timeouts appropriately");
                    Console.WriteLine("    - Clean up resources properly");
                    Console.WriteLine("    - Handle cancellation exceptions correctly");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up test files
                    // ============================================================

                    Console.WriteLine("Cleaning up test files...");
                    Console.WriteLine();

                    var testFiles = new[] { testFile, largeTestFile };
                    foreach (var fileName in testFiles)
                    {
                        try
                        {
                            if (File.Exists(fileName))
                            {
                                File.Delete(fileName);
                                Console.WriteLine($"  Deleted: {fileName}");
                            }
                        }
                        catch
                        {
                            // Ignore deletion errors
                        }
                    }

                    // Clean up uploaded files if any
                    if (downloadFileId != null)
                    {
                        try
                        {
                            await client.DeleteFileAsync(downloadFileId);
                            Console.WriteLine($"  Deleted uploaded file: {downloadFileId}");
                        }
                        catch
                        {
                            // Ignore deletion errors
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine("Cleanup completed.");
                    Console.WriteLine();
                    Console.WriteLine("All examples completed successfully!");
                }
                catch (Exception ex)
                {
                    // Handle unexpected errors
                    Console.WriteLine($"Unexpected Error: {ex.Message}");
                    Console.WriteLine($"Stack Trace: {ex.StackTrace}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}

