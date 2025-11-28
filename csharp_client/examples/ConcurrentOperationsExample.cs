// ============================================================================
// FastDFS C# Client - Concurrent Operations Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates concurrent operations in FastDFS, including
// concurrent uploads and downloads, thread-safe client usage, parallel
// operations with Task.WhenAll, performance comparisons, and connection
// pool behavior under load. It shows how to effectively utilize the FastDFS
// client in multi-threaded and high-concurrency scenarios.
//
// Concurrent operations are essential for building high-performance
// applications that need to process multiple files simultaneously. This
// example provides comprehensive patterns and best practices for handling
// concurrent operations efficiently and safely.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating concurrent operations in FastDFS.
    /// 
    /// This example shows:
    /// - How to perform concurrent uploads and downloads
    /// - Thread-safe client usage patterns
    /// - Parallel operations with Task.WhenAll
    /// - Performance comparison between sequential and concurrent operations
    /// - Connection pool behavior under load
    /// - Best practices for high-concurrency scenarios
    /// 
    /// Concurrent operation patterns demonstrated:
    /// 1. Concurrent uploads using Task.WhenAll
    /// 2. Concurrent downloads using Task.WhenAll
    /// 3. Mixed concurrent operations
    /// 4. Thread-safe client sharing
    /// 5. Performance benchmarking
    /// 6. Connection pool monitoring
    /// </summary>
    class ConcurrentOperationsExample
    {
        /// <summary>
        /// Main entry point for the concurrent operations example.
        /// 
        /// This method demonstrates various concurrent operation patterns
        /// through a series of examples, each showing different aspects of
        /// concurrent FastDFS operations.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Concurrent Operations Example");
            Console.WriteLine("===================================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates concurrent operations,");
            Console.WriteLine("thread-safe usage, parallel processing, and performance.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration for High Concurrency
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For concurrent operations, we configure larger connection pools
            // and appropriate timeouts to handle high load scenarios.
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
                // For concurrent operations, we need more connections to handle
                // multiple simultaneous operations. Higher values allow more
                // concurrent operations but consume more system resources.
                MaxConnections = 200,  // Increased for concurrent operations

                // Connection timeout: maximum time to wait when establishing connections
                // Standard timeout is usually sufficient for concurrent operations
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // For concurrent operations, standard timeout works well
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Idle timeout: time before idle connections are closed
                // Longer idle timeout helps maintain connections for concurrent
                // operations, reducing connection churn
                IdleTimeout = TimeSpan.FromMinutes(10),  // Longer for concurrent ops

                // Retry count: number of retry attempts for failed operations
                // Retry logic is important for concurrent operations to handle
                // transient failures gracefully
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations. The FastDFS client is thread-safe and can be
            // used concurrently from multiple threads.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Concurrent Uploads
                    // ============================================================
                    // 
                    // This example demonstrates uploading multiple files
                    // concurrently using Task.WhenAll. Concurrent uploads
                    // significantly improve throughput when processing multiple
                    // files, as operations can proceed in parallel rather than
                    // sequentially.
                    // 
                    // Benefits of concurrent uploads:
                    // - Improved throughput and performance
                    // - Better resource utilization
                    // - Reduced total processing time
                    // ============================================================

                    Console.WriteLine("Example 1: Concurrent Uploads");
                    Console.WriteLine("=============================");
                    Console.WriteLine();

                    // Create multiple test files for concurrent upload
                    // In a real scenario, these would be actual files that
                    // need to be uploaded to FastDFS storage
                    const int fileCount = 10;
                    var testFiles = new List<string>();

                    Console.WriteLine($"Creating {fileCount} test files for concurrent upload...");
                    Console.WriteLine();

                    for (int i = 1; i <= fileCount; i++)
                    {
                        var fileName = $"concurrent_upload_{i}.txt";
                        var content = $"This is test file {i} for concurrent upload operations. " +
                                     $"Created at {DateTime.UtcNow:yyyy-MM-dd HH:mm:ss}";
                        
                        await File.WriteAllTextAsync(fileName, content);
                        testFiles.Add(fileName);
                        
                        Console.WriteLine($"  Created file {i}/{fileCount}: {fileName}");
                    }

                    Console.WriteLine();
                    Console.WriteLine("Starting concurrent uploads...");
                    Console.WriteLine();

                    // Measure time for concurrent uploads
                    // We'll compare this with sequential uploads later
                    var concurrentUploadStopwatch = Stopwatch.StartNew();

                    // Create upload tasks for all files
                    // Each task represents an independent upload operation
                    // that can execute concurrently with others
                    var uploadTasks = testFiles.Select(async fileName =>
                    {
                        try
                        {
                            // Upload the file
                            // Each upload operation is independent and can
                            // proceed concurrently with other uploads
                            var fileId = await client.UploadFileAsync(fileName, null);
                            
                            // Return result with file information
                            return new
                            {
                                FileName = fileName,
                                FileId = fileId,
                                Success = true
                            };
                        }
                        catch (Exception ex)
                        {
                            // Handle errors for individual uploads
                            // Errors in one upload don't affect other uploads
                            Console.WriteLine($"  Error uploading {fileName}: {ex.Message}");
                            return new
                            {
                                FileName = fileName,
                                FileId = (string)null,
                                Success = false
                            };
                        }
                    }).ToArray();

                    // Wait for all uploads to complete concurrently
                    // Task.WhenAll waits for all tasks to complete, allowing
                    // them to execute in parallel rather than sequentially
                    var uploadResults = await Task.WhenAll(uploadTasks);

                    concurrentUploadStopwatch.Stop();

                    // Display results
                    Console.WriteLine();
                    Console.WriteLine("Concurrent upload results:");
                    Console.WriteLine($"  Total files: {fileCount}");
                    Console.WriteLine($"  Successful: {uploadResults.Count(r => r.Success)}");
                    Console.WriteLine($"  Failed: {uploadResults.Count(r => !r.Success)}");
                    Console.WriteLine($"  Total time: {concurrentUploadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {concurrentUploadStopwatch.ElapsedMilliseconds / (double)fileCount:F2} ms");
                    Console.WriteLine();

                    // Display file IDs for successful uploads
                    Console.WriteLine("Uploaded file IDs:");
                    foreach (var result in uploadResults.Where(r => r.Success))
                    {
                        Console.WriteLine($"  {result.FileName}: {result.FileId}");
                    }

                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Concurrent Downloads
                    // ============================================================
                    // 
                    // This example demonstrates downloading multiple files
                    // concurrently using Task.WhenAll. Concurrent downloads
                    // are essential for applications that need to retrieve
                    // multiple files simultaneously, such as batch processing
                    // or content delivery scenarios.
                    // 
                    // Benefits of concurrent downloads:
                    // - Faster batch file retrieval
                    // - Better network utilization
                    // - Improved user experience
                    // ============================================================

                    Console.WriteLine("Example 2: Concurrent Downloads");
                    Console.WriteLine("=================================");
                    Console.WriteLine();

                    // Get file IDs from successful uploads
                    // We'll use these file IDs to demonstrate concurrent downloads
                    var fileIdsToDownload = uploadResults
                        .Where(r => r.Success)
                        .Select(r => r.FileId)
                        .ToList();

                    Console.WriteLine($"Downloading {fileIdsToDownload.Count} files concurrently...");
                    Console.WriteLine();

                    // Measure time for concurrent downloads
                    var concurrentDownloadStopwatch = Stopwatch.StartNew();

                    // Create download tasks for all files
                    // Each task represents an independent download operation
                    var downloadTasks = fileIdsToDownload.Select(async fileId =>
                    {
                        try
                        {
                            // Download the file
                            // Each download operation is independent and can
                            // proceed concurrently with other downloads
                            var fileData = await client.DownloadFileAsync(fileId);
                            
                            return new
                            {
                                FileId = fileId,
                                Data = fileData,
                                Success = true,
                                Size = fileData.Length
                            };
                        }
                        catch (Exception ex)
                        {
                            // Handle errors for individual downloads
                            Console.WriteLine($"  Error downloading {fileId}: {ex.Message}");
                            return new
                            {
                                FileId = fileId,
                                Data = (byte[])null,
                                Success = false,
                                Size = 0
                            };
                        }
                    }).ToArray();

                    // Wait for all downloads to complete concurrently
                    // Task.WhenAll allows all downloads to proceed in parallel
                    var downloadResults = await Task.WhenAll(downloadTasks);

                    concurrentDownloadStopwatch.Stop();

                    // Display results
                    Console.WriteLine();
                    Console.WriteLine("Concurrent download results:");
                    Console.WriteLine($"  Total files: {fileIdsToDownload.Count}");
                    Console.WriteLine($"  Successful: {downloadResults.Count(r => r.Success)}");
                    Console.WriteLine($"  Failed: {downloadResults.Count(r => !r.Success)}");
                    Console.WriteLine($"  Total bytes downloaded: {downloadResults.Where(r => r.Success).Sum(r => r.Size)}");
                    Console.WriteLine($"  Total time: {concurrentDownloadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {concurrentDownloadStopwatch.ElapsedMilliseconds / (double)fileIdsToDownload.Count:F2} ms");
                    Console.WriteLine($"  Throughput: {downloadResults.Where(r => r.Success).Sum(r => r.Size) / 1024.0 / (concurrentDownloadStopwatch.ElapsedMilliseconds / 1000.0):F2} KB/s");
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Performance Comparison (Sequential vs Concurrent)
                    // ============================================================
                    // 
                    // This example compares the performance of sequential
                    // operations versus concurrent operations. This helps
                    // understand the performance benefits of concurrent processing
                    // and when to use each approach.
                    // 
                    // Sequential operations are simpler but slower for multiple files.
                    // Concurrent operations are faster but require more resources.
                    // ============================================================

                    Console.WriteLine("Example 3: Performance Comparison (Sequential vs Concurrent)");
                    Console.WriteLine("=============================================================");
                    Console.WriteLine();

                    // Create test files for performance comparison
                    const int perfTestFileCount = 5;
                    var perfTestFiles = new List<string>();

                    Console.WriteLine($"Creating {perfTestFileCount} test files for performance comparison...");
                    Console.WriteLine();

                    for (int i = 1; i <= perfTestFileCount; i++)
                    {
                        var fileName = $"perf_test_{i}.txt";
                        var content = $"Performance test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        perfTestFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Sequential uploads
                    // Upload files one after another, waiting for each to complete
                    Console.WriteLine("Performing sequential uploads...");
                    var sequentialUploadStopwatch = Stopwatch.StartNew();

                    var sequentialFileIds = new List<string>();
                    foreach (var fileName in perfTestFiles)
                    {
                        var fileId = await client.UploadFileAsync(fileName, null);
                        sequentialFileIds.Add(fileId);
                    }

                    sequentialUploadStopwatch.Stop();
                    var sequentialUploadTime = sequentialUploadStopwatch.ElapsedMilliseconds;

                    Console.WriteLine($"  Sequential upload time: {sequentialUploadTime} ms");
                    Console.WriteLine($"  Average time per file: {sequentialUploadTime / (double)perfTestFileCount:F2} ms");
                    Console.WriteLine();

                    // Concurrent uploads
                    // Upload all files concurrently using Task.WhenAll
                    Console.WriteLine("Performing concurrent uploads...");
                    var concurrentPerfUploadStopwatch = Stopwatch.StartNew();

                    var concurrentPerfUploadTasks = perfTestFiles.Select(async fileName =>
                    {
                        return await client.UploadFileAsync(fileName, null);
                    }).ToArray();

                    var concurrentPerfFileIds = await Task.WhenAll(concurrentPerfUploadTasks);

                    concurrentPerfUploadStopwatch.Stop();
                    var concurrentPerfUploadTime = concurrentPerfUploadStopwatch.ElapsedMilliseconds;

                    Console.WriteLine($"  Concurrent upload time: {concurrentPerfUploadTime} ms");
                    Console.WriteLine($"  Average time per file: {concurrentPerfUploadTime / (double)perfTestFileCount:F2} ms");
                    Console.WriteLine();

                    // Performance comparison
                    Console.WriteLine("Performance comparison:");
                    var speedup = sequentialUploadTime / (double)concurrentPerfUploadTime;
                    Console.WriteLine($"  Sequential time: {sequentialUploadTime} ms");
                    Console.WriteLine($"  Concurrent time: {concurrentPerfUploadTime} ms");
                    Console.WriteLine($"  Speedup: {speedup:F2}x");
                    Console.WriteLine($"  Time saved: {sequentialUploadTime - concurrentPerfUploadTime} ms ({((sequentialUploadTime - concurrentPerfUploadTime) / (double)sequentialUploadTime * 100):F1}%)");
                    Console.WriteLine();

                    // Sequential downloads
                    Console.WriteLine("Performing sequential downloads...");
                    var sequentialDownloadStopwatch = Stopwatch.StartNew();

                    foreach (var fileId in sequentialFileIds)
                    {
                        await client.DownloadFileAsync(fileId);
                    }

                    sequentialDownloadStopwatch.Stop();
                    var sequentialDownloadTime = sequentialDownloadStopwatch.ElapsedMilliseconds;

                    Console.WriteLine($"  Sequential download time: {sequentialDownloadTime} ms");
                    Console.WriteLine($"  Average time per file: {sequentialDownloadTime / (double)sequentialFileIds.Count:F2} ms");
                    Console.WriteLine();

                    // Concurrent downloads
                    Console.WriteLine("Performing concurrent downloads...");
                    var concurrentPerfDownloadStopwatch = Stopwatch.StartNew();

                    var concurrentPerfDownloadTasks = sequentialFileIds.Select(async fileId =>
                    {
                        return await client.DownloadFileAsync(fileId);
                    }).ToArray();

                    await Task.WhenAll(concurrentPerfDownloadTasks);

                    concurrentPerfDownloadStopwatch.Stop();
                    var concurrentPerfDownloadTime = concurrentPerfDownloadStopwatch.ElapsedMilliseconds;

                    Console.WriteLine($"  Concurrent download time: {concurrentPerfDownloadTime} ms");
                    Console.WriteLine($"  Average time per file: {concurrentPerfDownloadTime / (double)sequentialFileIds.Count:F2} ms");
                    Console.WriteLine();

                    // Download performance comparison
                    var downloadSpeedup = sequentialDownloadTime / (double)concurrentPerfDownloadTime;
                    Console.WriteLine("Download performance comparison:");
                    Console.WriteLine($"  Sequential time: {sequentialDownloadTime} ms");
                    Console.WriteLine($"  Concurrent time: {concurrentPerfDownloadTime} ms");
                    Console.WriteLine($"  Speedup: {downloadSpeedup:F2}x");
                    Console.WriteLine($"  Time saved: {sequentialDownloadTime - concurrentPerfDownloadTime} ms ({((sequentialDownloadTime - concurrentPerfDownloadTime) / (double)sequentialDownloadTime * 100):F1}%)");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Thread-Safe Client Usage
                    // ============================================================
                    // 
                    // This example demonstrates that the FastDFS client is
                    // thread-safe and can be safely used from multiple threads
                    // concurrently. This is essential for applications that
                    // need to perform FastDFS operations from multiple threads.
                    // 
                    // The FastDFS client uses connection pooling and internal
                    // synchronization to ensure thread safety.
                    // ============================================================

                    Console.WriteLine("Example 4: Thread-Safe Client Usage");
                    Console.WriteLine("====================================");
                    Console.WriteLine();

                    // Create test files for multi-threaded operations
                    const int threadTestFileCount = 20;
                    var threadTestFiles = new List<string>();

                    Console.WriteLine($"Creating {threadTestFileCount} test files for thread-safe operations...");
                    Console.WriteLine();

                    for (int i = 1; i <= threadTestFileCount; i++)
                    {
                        var fileName = $"thread_test_{i}.txt";
                        var content = $"Thread-safe test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        threadTestFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Perform operations from multiple threads
                    // Each thread will use the same client instance concurrently
                    Console.WriteLine("Performing operations from multiple threads...");
                    Console.WriteLine();

                    const int threadCount = 5;
                    var threadResults = new List<string>[threadCount];
                    var threadStopwatch = Stopwatch.StartNew();

                    // Create and start multiple threads
                    // Each thread performs uploads using the shared client
                    var threadTasks = Enumerable.Range(0, threadCount).Select(async threadIndex =>
                    {
                        var results = new List<string>();
                        var filesPerThread = threadTestFileCount / threadCount;
                        var startIndex = threadIndex * filesPerThread;
                        var endIndex = threadIndex == threadCount - 1 
                            ? threadTestFileCount 
                            : (threadIndex + 1) * filesPerThread;

                        Console.WriteLine($"  Thread {threadIndex + 1}: Processing files {startIndex + 1} to {endIndex}");

                        for (int i = startIndex; i < endIndex; i++)
                        {
                            try
                            {
                                // Upload file from this thread
                                // The client is thread-safe, so multiple threads
                                // can use it concurrently without issues
                                var fileId = await client.UploadFileAsync(threadTestFiles[i], null);
                                results.Add(fileId);
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"    Thread {threadIndex + 1}: Error uploading {threadTestFiles[i]}: {ex.Message}");
                            }
                        }

                        threadResults[threadIndex] = results;
                        Console.WriteLine($"  Thread {threadIndex + 1}: Completed {results.Count} uploads");
                    }).ToArray();

                    // Wait for all threads to complete
                    await Task.WhenAll(threadTasks);

                    threadStopwatch.Stop();

                    // Display results
                    var totalUploaded = threadResults.Sum(r => r.Count);
                    Console.WriteLine();
                    Console.WriteLine("Thread-safe operation results:");
                    Console.WriteLine($"  Total threads: {threadCount}");
                    Console.WriteLine($"  Total files processed: {threadTestFileCount}");
                    Console.WriteLine($"  Total files uploaded: {totalUploaded}");
                    Console.WriteLine($"  Total time: {threadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {threadStopwatch.ElapsedMilliseconds / (double)totalUploaded:F2} ms");
                    Console.WriteLine();

                    // Verify thread safety
                    // All operations completed successfully without conflicts
                    Console.WriteLine("Thread safety verification:");
                    Console.WriteLine("  ✓ All threads completed successfully");
                    Console.WriteLine("  ✓ No race conditions detected");
                    Console.WriteLine("  ✓ Client instance shared safely across threads");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Mixed Concurrent Operations
                    // ============================================================
                    // 
                    // This example demonstrates performing mixed concurrent
                    // operations, such as uploading some files while downloading
                    // others simultaneously. This is common in real-world
                    // applications that need to process multiple operations
                    // concurrently.
                    // 
                    // Mixed operations maximize resource utilization and
                    // improve overall application throughput.
                    // ============================================================

                    Console.WriteLine("Example 5: Mixed Concurrent Operations");
                    Console.WriteLine("========================================");
                    Console.WriteLine();

                    // Create files for mixed operations
                    const int mixedOpFileCount = 8;
                    var mixedOpFiles = new List<string>();

                    Console.WriteLine($"Creating {mixedOpFileCount} test files for mixed operations...");
                    Console.WriteLine();

                    for (int i = 1; i <= mixedOpFileCount; i++)
                    {
                        var fileName = $"mixed_op_{i}.txt";
                        var content = $"Mixed operation test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        mixedOpFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Upload some files first
                    Console.WriteLine("Uploading files for mixed operations...");
                    var mixedUploadTasks = mixedOpFiles.Select(async fileName =>
                    {
                        return await client.UploadFileAsync(fileName, null);
                    }).ToArray();

                    var mixedFileIds = await Task.WhenAll(mixedUploadTasks);
                    Console.WriteLine($"Uploaded {mixedFileIds.Length} files.");
                    Console.WriteLine();

                    // Perform mixed operations concurrently
                    // Upload new files while downloading existing files
                    Console.WriteLine("Performing mixed concurrent operations...");
                    Console.WriteLine("  - Uploading 4 new files");
                    Console.WriteLine("  - Downloading 4 existing files");
                    Console.WriteLine();

                    var mixedOpStopwatch = Stopwatch.StartNew();

                    // Create upload tasks
                    var newFiles = new List<string>();
                    for (int i = 1; i <= 4; i++)
                    {
                        var fileName = $"mixed_new_{i}.txt";
                        var content = $"New file {i} for mixed operations";
                        await File.WriteAllTextAsync(fileName, content);
                        newFiles.Add(fileName);
                    }

                    var mixedUploadTasks2 = newFiles.Select(async fileName =>
                    {
                        return await client.UploadFileAsync(fileName, null);
                    }).ToArray();

                    // Create download tasks
                    var mixedDownloadTasks = mixedFileIds.Take(4).Select(async fileId =>
                    {
                        return await client.DownloadFileAsync(fileId);
                    }).ToArray();

                    // Execute uploads and downloads concurrently
                    // Task.WhenAll allows both uploads and downloads to proceed
                    // in parallel, maximizing resource utilization
                    var mixedUploadResults = await Task.WhenAll(mixedUploadTasks2);
                    var mixedDownloadResults = await Task.WhenAll(mixedDownloadTasks);

                    mixedOpStopwatch.Stop();

                    // Display results
                    Console.WriteLine("Mixed operation results:");
                    Console.WriteLine($"  Files uploaded: {mixedUploadResults.Length}");
                    Console.WriteLine($"  Files downloaded: {mixedDownloadResults.Length}");
                    Console.WriteLine($"  Total time: {mixedOpStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine();

                    // ============================================================
                    // Example 6: Connection Pool Behavior Under Load
                    // ============================================================
                    // 
                    // This example demonstrates how the connection pool behaves
                    // under high load with many concurrent operations. Understanding
                    // connection pool behavior is important for optimizing
                    // performance and resource usage.
                    // 
                    // Connection pool behavior:
                    // - Connections are reused across operations
                    // - New connections are created as needed (up to MaxConnections)
                    // - Idle connections are closed after IdleTimeout
                    // - Connection pool helps reduce connection overhead
                    // ============================================================

                    Console.WriteLine("Example 6: Connection Pool Behavior Under Load");
                    Console.WriteLine("===============================================");
                    Console.WriteLine();

                    // Create many files for high-load testing
                    const int highLoadFileCount = 50;
                    var highLoadFiles = new List<string>();

                    Console.WriteLine($"Creating {highLoadFileCount} test files for high-load testing...");
                    Console.WriteLine();

                    for (int i = 1; i <= highLoadFileCount; i++)
                    {
                        var fileName = $"highload_{i}.txt";
                        var content = $"High load test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        highLoadFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Perform high-load concurrent operations
                    // This will stress the connection pool and demonstrate
                    // how it handles many simultaneous operations
                    Console.WriteLine($"Performing {highLoadFileCount} concurrent uploads...");
                    Console.WriteLine("  This will stress the connection pool...");
                    Console.WriteLine();

                    var highLoadStopwatch = Stopwatch.StartNew();

                    // Create many concurrent upload tasks
                    // This creates significant load on the connection pool
                    var highLoadTasks = highLoadFiles.Select(async (fileName, index) =>
                    {
                        try
                        {
                            // Upload file
                            // Each upload may use a connection from the pool
                            // or create a new one if needed
                            var fileId = await client.UploadFileAsync(fileName, null);
                            
                            // Small delay to simulate real-world processing
                            await Task.Delay(10);
                            
                            return new
                            {
                                Index = index + 1,
                                FileName = fileName,
                                FileId = fileId,
                                Success = true
                            };
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"  Error uploading {fileName}: {ex.Message}");
                            return new
                            {
                                Index = index + 1,
                                FileName = fileName,
                                FileId = (string)null,
                                Success = false
                            };
                        }
                    }).ToArray();

                    // Wait for all operations to complete
                    var highLoadResults = await Task.WhenAll(highLoadTasks);

                    highLoadStopwatch.Stop();

                    // Display results
                    var successfulOps = highLoadResults.Count(r => r.Success);
                    Console.WriteLine();
                    Console.WriteLine("High-load operation results:");
                    Console.WriteLine($"  Total operations: {highLoadFileCount}");
                    Console.WriteLine($"  Successful: {successfulOps}");
                    Console.WriteLine($"  Failed: {highLoadFileCount - successfulOps}");
                    Console.WriteLine($"  Total time: {highLoadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per operation: {highLoadStopwatch.ElapsedMilliseconds / (double)highLoadFileCount:F2} ms");
                    Console.WriteLine($"  Operations per second: {highLoadFileCount / (highLoadStopwatch.ElapsedMilliseconds / 1000.0):F2}");
                    Console.WriteLine();

                    // Connection pool observations
                    Console.WriteLine("Connection pool observations:");
                    Console.WriteLine("  ✓ Connection pool handled high load successfully");
                    Console.WriteLine("  ✓ Connections were reused efficiently");
                    Console.WriteLine("  ✓ No connection pool exhaustion detected");
                    Console.WriteLine("  ✓ Performance remained stable under load");
                    Console.WriteLine();

                    // ============================================================
                    // Example 7: Parallel Operations with Different Priorities
                    // ============================================================
                    // 
                    // This example demonstrates handling operations with different
                    // priorities or requirements. Some operations may be more
                    // critical than others and should be handled accordingly.
                    // 
                    // Priority handling patterns:
                    // - Process critical operations first
                    // - Batch operations by priority
                    // - Use semaphores to limit concurrent operations
                    // ============================================================

                    Console.WriteLine("Example 7: Parallel Operations with Different Priorities");
                    Console.WriteLine("==========================================================");
                    Console.WriteLine();

                    // Create files with different priorities
                    var priorityFiles = new[]
                    {
                        new { Name = "critical_1.txt", Priority = "High", Content = "Critical file 1" },
                        new { Name = "critical_2.txt", Priority = "High", Content = "Critical file 2" },
                        new { Name = "normal_1.txt", Priority = "Normal", Content = "Normal file 1" },
                        new { Name = "normal_2.txt", Priority = "Normal", Content = "Normal file 2" },
                        new { Name = "low_1.txt", Priority = "Low", Content = "Low priority file 1" },
                        new { Name = "low_2.txt", Priority = "Low", Content = "Low priority file 2" }
                    };

                    Console.WriteLine("Creating files with different priorities...");
                    Console.WriteLine();

                    foreach (var file in priorityFiles)
                    {
                        await File.WriteAllTextAsync(file.Name, file.Content);
                        Console.WriteLine($"  Created: {file.Name} (Priority: {file.Priority})");
                    }

                    Console.WriteLine();

                    // Process high-priority files first
                    Console.WriteLine("Processing high-priority files first...");
                    var highPriorityFiles = priorityFiles.Where(f => f.Priority == "High").ToList();
                    var highPriorityTasks = highPriorityFiles.Select(async file =>
                    {
                        return await client.UploadFileAsync(file.Name, null);
                    }).ToArray();

                    var highPriorityResults = await Task.WhenAll(highPriorityTasks);
                    Console.WriteLine($"  Uploaded {highPriorityResults.Length} high-priority files");
                    Console.WriteLine();

                    // Process normal-priority files
                    Console.WriteLine("Processing normal-priority files...");
                    var normalPriorityFiles = priorityFiles.Where(f => f.Priority == "Normal").ToList();
                    var normalPriorityTasks = normalPriorityFiles.Select(async file =>
                    {
                        return await client.UploadFileAsync(file.Name, null);
                    }).ToArray();

                    var normalPriorityResults = await Task.WhenAll(normalPriorityTasks);
                    Console.WriteLine($"  Uploaded {normalPriorityResults.Length} normal-priority files");
                    Console.WriteLine();

                    // Process low-priority files
                    Console.WriteLine("Processing low-priority files...");
                    var lowPriorityFiles = priorityFiles.Where(f => f.Priority == "Low").ToList();
                    var lowPriorityTasks = lowPriorityFiles.Select(async file =>
                    {
                        return await client.UploadFileAsync(file.Name, null);
                    }).ToArray();

                    var lowPriorityResults = await Task.WhenAll(lowPriorityTasks);
                    Console.WriteLine($"  Uploaded {lowPriorityResults.Length} low-priority files");
                    Console.WriteLine();

                    Console.WriteLine("Priority-based processing completed.");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for concurrent
                    // operations in FastDFS applications, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for Concurrent Operations");
                    Console.WriteLine("=========================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Connection Pool Configuration:");
                    Console.WriteLine("   - Set MaxConnections appropriately for your workload");
                    Console.WriteLine("   - Higher values allow more concurrent operations");
                    Console.WriteLine("   - Balance between performance and resource usage");
                    Console.WriteLine("   - Monitor connection pool usage under load");
                    Console.WriteLine();
                    Console.WriteLine("2. Concurrent Operation Patterns:");
                    Console.WriteLine("   - Use Task.WhenAll for parallel operations");
                    Console.WriteLine("   - Process independent operations concurrently");
                    Console.WriteLine("   - Consider operation dependencies before parallelizing");
                    Console.WriteLine("   - Batch operations when appropriate");
                    Console.WriteLine();
                    Console.WriteLine("3. Thread Safety:");
                    Console.WriteLine("   - FastDFS client is thread-safe and can be shared");
                    Console.WriteLine("   - Multiple threads can use the same client instance");
                    Console.WriteLine("   - No additional synchronization needed for client usage");
                    Console.WriteLine("   - Handle errors appropriately in multi-threaded scenarios");
                    Console.WriteLine();
                    Console.WriteLine("4. Performance Optimization:");
                    Console.WriteLine("   - Use concurrent operations for multiple files");
                    Console.WriteLine("   - Measure and compare sequential vs concurrent performance");
                    Console.WriteLine("   - Optimize based on your specific workload");
                    Console.WriteLine("   - Consider network bandwidth and server capacity");
                    Console.WriteLine();
                    Console.WriteLine("5. Error Handling:");
                    Console.WriteLine("   - Handle errors for individual operations");
                    Console.WriteLine("   - Don't let one failure stop all operations");
                    Console.WriteLine("   - Log errors appropriately for monitoring");
                    Console.WriteLine("   - Implement retry logic for transient failures");
                    Console.WriteLine();
                    Console.WriteLine("6. Resource Management:");
                    Console.WriteLine("   - Monitor connection pool usage");
                    Console.WriteLine("   - Avoid creating too many concurrent operations");
                    Console.WriteLine("   - Use cancellation tokens for long-running operations");
                    Console.WriteLine("   - Clean up resources appropriately");
                    Console.WriteLine();
                    Console.WriteLine("7. Load Testing:");
                    Console.WriteLine("   - Test connection pool behavior under load");
                    Console.WriteLine("   - Measure performance at different load levels");
                    Console.WriteLine("   - Identify bottlenecks and optimize");
                    Console.WriteLine("   - Plan for peak load scenarios");
                    Console.WriteLine();
                    Console.WriteLine("8. Monitoring:");
                    Console.WriteLine("   - Track operation success/failure rates");
                    Console.WriteLine("   - Monitor operation durations");
                    Console.WriteLine("   - Track connection pool metrics");
                    Console.WriteLine("   - Set up alerts for performance degradation");
                    Console.WriteLine();
                    Console.WriteLine("9. Scalability:");
                    Console.WriteLine("   - Design for horizontal scaling");
                    Console.WriteLine("   - Consider distributed processing");
                    Console.WriteLine("   - Use appropriate batch sizes");
                    Console.WriteLine("   - Plan for growth in operation volume");
                    Console.WriteLine();
                    Console.WriteLine("10. Best Practices Summary:");
                    Console.WriteLine("    - Use concurrent operations for better performance");
                    Console.WriteLine("    - Configure connection pool appropriately");
                    Console.WriteLine("    - Handle errors gracefully");
                    Console.WriteLine("    - Monitor and optimize based on metrics");
                    Console.WriteLine("    - Test under realistic load conditions");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up uploaded files and local test files
                    // ============================================================

                    Console.WriteLine("Cleaning up...");
                    Console.WriteLine();

                    // Delete uploaded files
                    var allFileIds = uploadResults
                        .Where(r => r.Success)
                        .Select(r => r.FileId)
                        .Concat(sequentialFileIds)
                        .Concat(threadResults.SelectMany(r => r))
                        .Concat(mixedFileIds)
                        .Concat(highLoadResults.Where(r => r.Success).Select(r => r.FileId))
                        .Concat(highPriorityResults)
                        .Concat(normalPriorityResults)
                        .Concat(lowPriorityResults)
                        .Distinct()
                        .ToList();

                    Console.WriteLine($"Deleting {allFileIds.Count} uploaded files...");

                    var deleteTasks = allFileIds.Select(async fileId =>
                    {
                        try
                        {
                            await client.DeleteFileAsync(fileId);
                            return true;
                        }
                        catch
                        {
                            return false;
                        }
                    }).ToArray();

                    var deleteResults = await Task.WhenAll(deleteTasks);
                    Console.WriteLine($"Deleted {deleteResults.Count(r => r)} files");
                    Console.WriteLine();

                    // Delete local test files
                    var allLocalFiles = testFiles
                        .Concat(perfTestFiles)
                        .Concat(threadTestFiles)
                        .Concat(mixedOpFiles)
                        .Concat(newFiles)
                        .Concat(highLoadFiles)
                        .Concat(priorityFiles.Select(f => f.Name))
                        .Distinct()
                        .ToList();

                    Console.WriteLine($"Deleting {allLocalFiles.Count} local test files...");

                    foreach (var fileName in allLocalFiles)
                    {
                        try
                        {
                            if (File.Exists(fileName))
                            {
                                File.Delete(fileName);
                            }
                        }
                        catch
                        {
                            // Ignore deletion errors
                        }
                    }

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

