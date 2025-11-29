// ============================================================================
// FastDFS C# Client - Batch Operations Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates batch operations in FastDFS, including batch
// upload of multiple files, batch download of multiple files, progress
// tracking for batch operations, error handling in batch scenarios, and
// performance optimization techniques. It shows how to efficiently process
// multiple files in batches while providing progress feedback and handling
// errors gracefully.
//
// Batch operations are essential for applications that need to process
// large numbers of files efficiently. This example provides comprehensive
// patterns and best practices for implementing batch operations with
// progress tracking, error handling, and performance optimization.
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
    /// Example demonstrating batch operations in FastDFS.
    /// 
    /// This example shows:
    /// - How to batch upload multiple files efficiently
    /// - How to batch download multiple files efficiently
    /// - How to track progress for batch operations
    /// - How to handle errors in batch scenarios
    /// - How to optimize batch operation performance
    /// - Best practices for batch processing
    /// 
    /// Batch operation patterns demonstrated:
    /// 1. Simple batch upload with progress tracking
    /// 2. Batch download with progress tracking
    /// 3. Batch operations with error handling
    /// 4. Performance-optimized batch processing
    /// 5. Large-scale batch operations
    /// 6. Batch operations with cancellation support
    /// </summary>
    class BatchOperationsExample
    {
        /// <summary>
        /// Main entry point for the batch operations example.
        /// 
        /// This method demonstrates various batch operation patterns through
        /// a series of examples, each showing different aspects of batch
        /// processing in FastDFS operations.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Batch Operations Example");
            Console.WriteLine("==============================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates batch operations,");
            Console.WriteLine("progress tracking, error handling, and performance optimization.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For batch operations, we configure appropriate connection pools
            // and timeouts to handle multiple files efficiently.
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
                // For batch operations, we need sufficient connections to handle
                // multiple simultaneous file operations. Higher values allow more
                // concurrent batch operations but consume more system resources.
                MaxConnections = 150,  // Sufficient for batch operations

                // Connection timeout: maximum time to wait when establishing connections
                // Standard timeout is usually sufficient for batch operations
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // For batch operations with large files, consider increasing this
                // to accommodate longer network transfers
                NetworkTimeout = TimeSpan.FromSeconds(60),  // Longer for batch ops

                // Idle timeout: time before idle connections are closed
                // Longer idle timeout helps maintain connections during batch
                // operations, reducing connection churn
                IdleTimeout = TimeSpan.FromMinutes(10),

                // Retry count: number of retry attempts for failed operations
                // Retry logic is important for batch operations to handle
                // transient failures gracefully
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations. The client is designed to handle batch operations
            // efficiently through connection pooling and concurrent processing.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Batch Upload Multiple Files
                    // ============================================================
                    // 
                    // This example demonstrates uploading multiple files in a
                    // batch operation. Batch uploads are more efficient than
                    // individual uploads because they can leverage connection
                    // pooling and concurrent processing.
                    // 
                    // Benefits of batch uploads:
                    // - Better resource utilization
                    // - Improved throughput
                    // - Reduced overhead per file
                    // - Easier progress tracking
                    // ============================================================

                    Console.WriteLine("Example 1: Batch Upload Multiple Files");
                    Console.WriteLine("=========================================");
                    Console.WriteLine();

                    // Create multiple test files for batch upload
                    // In a real scenario, these would be actual files that
                    // need to be uploaded to FastDFS storage
                    const int batchSize = 20;
                    var batchFiles = new List<string>();

                    Console.WriteLine($"Creating {batchSize} test files for batch upload...");
                    Console.WriteLine();

                    for (int i = 1; i <= batchSize; i++)
                    {
                        var fileName = $"batch_upload_{i}.txt";
                        var content = $"This is batch upload test file {i}. " +
                                     $"Created at {DateTime.UtcNow:yyyy-MM-dd HH:mm:ss}. " +
                                     $"File content for batch operation testing.";
                        
                        await File.WriteAllTextAsync(fileName, content);
                        batchFiles.Add(fileName);
                        
                        if (i % 5 == 0)
                        {
                            Console.WriteLine($"  Created {i}/{batchSize} files...");
                        }
                    }

                    Console.WriteLine($"All {batchSize} test files created.");
                    Console.WriteLine();

                    // Perform batch upload
                    // Batch upload processes all files together, allowing for
                    // better resource utilization and progress tracking
                    Console.WriteLine("Starting batch upload...");
                    Console.WriteLine();

                    var batchUploadStopwatch = Stopwatch.StartNew();

                    // Create upload tasks for all files in the batch
                    // Each task represents an independent upload operation
                    // that can execute concurrently with others
                    var uploadTasks = batchFiles.Select(async (fileName, index) =>
                    {
                        try
                        {
                            // Upload the file
                            // Each upload operation is independent and can
                            // proceed concurrently with other uploads in the batch
                            var fileId = await client.UploadFileAsync(fileName, null);
                            
                            // Return result with file information
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = fileId,
                                Success = true,
                                Index = index + 1
                            };
                        }
                        catch (Exception ex)
                        {
                            // Handle errors for individual files in the batch
                            // Errors in one file don't affect other files in the batch
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = null,
                                Success = false,
                                ErrorMessage = ex.Message,
                                Index = index + 1
                            };
                        }
                    }).ToArray();

                    // Wait for all uploads in the batch to complete
                    // Task.WhenAll waits for all tasks to complete, allowing
                    // them to execute in parallel for better performance
                    var batchUploadResults = await Task.WhenAll(uploadTasks);

                    batchUploadStopwatch.Stop();

                    // Display batch upload results
                    var successfulUploads = batchUploadResults.Count(r => r.Success);
                    var failedUploads = batchUploadResults.Count(r => !r.Success);

                    Console.WriteLine();
                    Console.WriteLine("Batch upload results:");
                    Console.WriteLine($"  Total files: {batchSize}");
                    Console.WriteLine($"  Successful: {successfulUploads}");
                    Console.WriteLine($"  Failed: {failedUploads}");
                    Console.WriteLine($"  Success rate: {(successfulUploads / (double)batchSize * 100):F1}%");
                    Console.WriteLine($"  Total time: {batchUploadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {batchUploadStopwatch.ElapsedMilliseconds / (double)batchSize:F2} ms");
                    Console.WriteLine($"  Throughput: {batchSize / (batchUploadStopwatch.ElapsedMilliseconds / 1000.0):F2} files/second");
                    Console.WriteLine();

                    // Display file IDs for successful uploads
                    if (successfulUploads > 0)
                    {
                        Console.WriteLine("Successfully uploaded files:");
                        foreach (var result in batchUploadResults.Where(r => r.Success).Take(5))
                        {
                            Console.WriteLine($"  {result.FileName}: {result.FileId}");
                        }
                        if (successfulUploads > 5)
                        {
                            Console.WriteLine($"  ... and {successfulUploads - 5} more files");
                        }
                        Console.WriteLine();
                    }

                    // Display errors for failed uploads
                    if (failedUploads > 0)
                    {
                        Console.WriteLine("Failed uploads:");
                        foreach (var result in batchUploadResults.Where(r => !r.Success))
                        {
                            Console.WriteLine($"  {result.FileName}: {result.ErrorMessage}");
                        }
                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 2: Batch Upload with Progress Tracking
                    // ============================================================
                    // 
                    // This example demonstrates batch upload with progress
                    // tracking. Progress tracking is essential for user experience
                    // and monitoring batch operations, especially for large batches.
                    // 
                    // Progress tracking features:
                    // - Real-time progress updates
                    // - Percentage completion
                    // - Files processed count
                    // - Estimated time remaining
                    // ============================================================

                    Console.WriteLine("Example 2: Batch Upload with Progress Tracking");
                    Console.WriteLine("================================================");
                    Console.WriteLine();

                    // Create test files for progress tracking example
                    const int progressBatchSize = 15;
                    var progressFiles = new List<string>();

                    Console.WriteLine($"Creating {progressBatchSize} test files for progress tracking...");
                    Console.WriteLine();

                    for (int i = 1; i <= progressBatchSize; i++)
                    {
                        var fileName = $"progress_upload_{i}.txt";
                        var content = $"Progress tracking test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        progressFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Perform batch upload with progress tracking
                    Console.WriteLine("Starting batch upload with progress tracking...");
                    Console.WriteLine();

                    var progressStopwatch = Stopwatch.StartNew();
                    var progressResults = new List<BatchUploadResult>();
                    var completedCount = 0;
                    var lockObject = new object();

                    // Create upload tasks with progress tracking
                    // Each task reports progress as it completes
                    var progressUploadTasks = progressFiles.Select(async (fileName, index) =>
                    {
                        try
                        {
                            // Upload the file
                            var fileId = await client.UploadFileAsync(fileName, null);
                            
                            // Update progress
                            // Thread-safe progress tracking using lock
                            lock (lockObject)
                            {
                                completedCount++;
                                var progress = (completedCount / (double)progressBatchSize) * 100;
                                
                                // Report progress
                                Console.WriteLine($"  Progress: {completedCount}/{progressBatchSize} files ({progress:F1}%) - {fileName}");
                                
                                progressResults.Add(new BatchUploadResult
                                {
                                    FileName = fileName,
                                    FileId = fileId,
                                    Success = true,
                                    Index = index + 1
                                });
                            }
                            
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = fileId,
                                Success = true,
                                Index = index + 1
                            };
                        }
                        catch (Exception ex)
                        {
                            // Handle errors and update progress
                            lock (lockObject)
                            {
                                completedCount++;
                                var progress = (completedCount / (double)progressBatchSize) * 100;
                                
                                Console.WriteLine($"  Progress: {completedCount}/{progressBatchSize} files ({progress:F1}%) - {fileName} [FAILED]");
                                
                                progressResults.Add(new BatchUploadResult
                                {
                                    FileName = fileName,
                                    FileId = null,
                                    Success = false,
                                    ErrorMessage = ex.Message,
                                    Index = index + 1
                                });
                            }
                            
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = null,
                                Success = false,
                                ErrorMessage = ex.Message,
                                Index = index + 1
                            };
                        }
                    }).ToArray();

                    // Wait for all uploads to complete
                    await Task.WhenAll(progressUploadTasks);

                    progressStopwatch.Stop();

                    // Display final results
                    var successfulProgressUploads = progressResults.Count(r => r.Success);
                    
                    Console.WriteLine();
                    Console.WriteLine("Progress tracking results:");
                    Console.WriteLine($"  Total files: {progressBatchSize}");
                    Console.WriteLine($"  Successful: {successfulProgressUploads}");
                    Console.WriteLine($"  Failed: {progressBatchSize - successfulProgressUploads}");
                    Console.WriteLine($"  Total time: {progressStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {progressStopwatch.ElapsedMilliseconds / (double)progressBatchSize:F2} ms");
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Batch Download Multiple Files
                    // ============================================================
                    // 
                    // This example demonstrates downloading multiple files in
                    // a batch operation. Batch downloads are more efficient than
                    // individual downloads and allow for better progress tracking.
                    // 
                    // Benefits of batch downloads:
                    // - Faster batch file retrieval
                    // - Better network utilization
                    // - Easier progress tracking
                    // - Improved user experience
                    // ============================================================

                    Console.WriteLine("Example 3: Batch Download Multiple Files");
                    Console.WriteLine("==========================================");
                    Console.WriteLine();

                    // Get file IDs from successful batch uploads
                    // We'll use these file IDs to demonstrate batch downloads
                    var fileIdsToDownload = batchUploadResults
                        .Where(r => r.Success)
                        .Select(r => r.FileId)
                        .Take(10)  // Download first 10 files
                        .ToList();

                    Console.WriteLine($"Downloading {fileIdsToDownload.Count} files in batch...");
                    Console.WriteLine();

                    var batchDownloadStopwatch = Stopwatch.StartNew();

                    // Create download tasks for all files in the batch
                    // Each task represents an independent download operation
                    var downloadTasks = fileIdsToDownload.Select(async (fileId, index) =>
                    {
                        try
                        {
                            // Download the file
                            // Each download operation is independent and can
                            // proceed concurrently with other downloads in the batch
                            var fileData = await client.DownloadFileAsync(fileId);
                            
                            return new BatchDownloadResult
                            {
                                FileId = fileId,
                                Data = fileData,
                                Success = true,
                                Size = fileData.Length,
                                Index = index + 1
                            };
                        }
                        catch (Exception ex)
                        {
                            // Handle errors for individual files in the batch
                            return new BatchDownloadResult
                            {
                                FileId = fileId,
                                Data = null,
                                Success = false,
                                ErrorMessage = ex.Message,
                                Size = 0,
                                Index = index + 1
                            };
                        }
                    }).ToArray();

                    // Wait for all downloads in the batch to complete
                    // Task.WhenAll allows all downloads to proceed in parallel
                    var batchDownloadResults = await Task.WhenAll(downloadTasks);

                    batchDownloadStopwatch.Stop();

                    // Display batch download results
                    var successfulDownloads = batchDownloadResults.Count(r => r.Success);
                    var totalBytesDownloaded = batchDownloadResults.Where(r => r.Success).Sum(r => r.Size);

                    Console.WriteLine();
                    Console.WriteLine("Batch download results:");
                    Console.WriteLine($"  Total files: {fileIdsToDownload.Count}");
                    Console.WriteLine($"  Successful: {successfulDownloads}");
                    Console.WriteLine($"  Failed: {fileIdsToDownload.Count - successfulDownloads}");
                    Console.WriteLine($"  Total bytes downloaded: {totalBytesDownloaded:N0}");
                    Console.WriteLine($"  Total time: {batchDownloadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {batchDownloadStopwatch.ElapsedMilliseconds / (double)fileIdsToDownload.Count:F2} ms");
                    Console.WriteLine($"  Throughput: {totalBytesDownloaded / 1024.0 / (batchDownloadStopwatch.ElapsedMilliseconds / 1000.0):F2} KB/s");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Batch Download with Progress Tracking
                    // ============================================================
                    // 
                    // This example demonstrates batch download with progress
                    // tracking. Progress tracking helps users understand the
                    // status of batch download operations and estimate completion time.
                    // 
                    // Progress tracking features:
                    // - Real-time progress updates
                    // - Percentage completion
                    // - Bytes downloaded tracking
                    // - Estimated time remaining
                    // ============================================================

                    Console.WriteLine("Example 4: Batch Download with Progress Tracking");
                    Console.WriteLine("==================================================");
                    Console.WriteLine();

                    // Get more file IDs for progress tracking example
                    var progressDownloadFileIds = batchUploadResults
                        .Where(r => r.Success)
                        .Select(r => r.FileId)
                        .Take(12)
                        .ToList();

                    Console.WriteLine($"Downloading {progressDownloadFileIds.Count} files with progress tracking...");
                    Console.WriteLine();

                    var progressDownloadStopwatch = Stopwatch.StartNew();
                    var progressDownloadResults = new List<BatchDownloadResult>();
                    var downloadCompletedCount = 0;
                    var totalBytesDownloadedProgress = 0L;
                    var downloadLockObject = new object();

                    // Create download tasks with progress tracking
                    // Each task reports progress as it completes
                    var progressDownloadTasks = progressDownloadFileIds.Select(async (fileId, index) =>
                    {
                        try
                        {
                            // Download the file
                            var fileData = await client.DownloadFileAsync(fileId);
                            
                            // Update progress
                            // Thread-safe progress tracking using lock
                            lock (downloadLockObject)
                            {
                                downloadCompletedCount++;
                                totalBytesDownloadedProgress += fileData.Length;
                                var progress = (downloadCompletedCount / (double)progressDownloadFileIds.Count) * 100;
                                
                                // Report progress with bytes downloaded
                                Console.WriteLine($"  Progress: {downloadCompletedCount}/{progressDownloadFileIds.Count} files ({progress:F1}%) - " +
                                                 $"{fileData.Length:N0} bytes - Total: {totalBytesDownloadedProgress:N0} bytes");
                                
                                progressDownloadResults.Add(new BatchDownloadResult
                                {
                                    FileId = fileId,
                                    Data = fileData,
                                    Success = true,
                                    Size = fileData.Length,
                                    Index = index + 1
                                });
                            }
                            
                            return new BatchDownloadResult
                            {
                                FileId = fileId,
                                Data = fileData,
                                Success = true,
                                Size = fileData.Length,
                                Index = index + 1
                            };
                        }
                        catch (Exception ex)
                        {
                            // Handle errors and update progress
                            lock (downloadLockObject)
                            {
                                downloadCompletedCount++;
                                var progress = (downloadCompletedCount / (double)progressDownloadFileIds.Count) * 100;
                                
                                Console.WriteLine($"  Progress: {downloadCompletedCount}/{progressDownloadFileIds.Count} files ({progress:F1}%) - [FAILED]");
                                
                                progressDownloadResults.Add(new BatchDownloadResult
                                {
                                    FileId = fileId,
                                    Data = null,
                                    Success = false,
                                    ErrorMessage = ex.Message,
                                    Size = 0,
                                    Index = index + 1
                                });
                            }
                            
                            return new BatchDownloadResult
                            {
                                FileId = fileId,
                                Data = null,
                                Success = false,
                                ErrorMessage = ex.Message,
                                Size = 0,
                                Index = index + 1
                            };
                        }
                    }).ToArray();

                    // Wait for all downloads to complete
                    await Task.WhenAll(progressDownloadTasks);

                    progressDownloadStopwatch.Stop();

                    // Display final results
                    var successfulProgressDownloads = progressDownloadResults.Count(r => r.Success);
                    var totalProgressBytes = progressDownloadResults.Where(r => r.Success).Sum(r => r.Size);

                    Console.WriteLine();
                    Console.WriteLine("Progress tracking download results:");
                    Console.WriteLine($"  Total files: {progressDownloadFileIds.Count}");
                    Console.WriteLine($"  Successful: {successfulProgressDownloads}");
                    Console.WriteLine($"  Failed: {progressDownloadFileIds.Count - successfulProgressDownloads}");
                    Console.WriteLine($"  Total bytes downloaded: {totalProgressBytes:N0}");
                    Console.WriteLine($"  Total time: {progressDownloadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {progressDownloadStopwatch.ElapsedMilliseconds / (double)progressDownloadFileIds.Count:F2} ms");
                    Console.WriteLine($"  Throughput: {totalProgressBytes / 1024.0 / (progressDownloadStopwatch.ElapsedMilliseconds / 1000.0):F2} KB/s");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Batch Operations with Error Handling
                    // ============================================================
                    // 
                    // This example demonstrates comprehensive error handling in
                    // batch operations. Error handling is crucial for batch
                    // operations because failures in individual files should not
                    // stop the entire batch from processing.
                    // 
                    // Error handling strategies:
                    // - Individual file error isolation
                    // - Retry logic for transient failures
                    // - Error reporting and logging
                    // - Partial success handling
                    // ============================================================

                    Console.WriteLine("Example 5: Batch Operations with Error Handling");
                    Console.WriteLine("=================================================");
                    Console.WriteLine();

                    // Create test files for error handling example
                    const int errorHandlingBatchSize = 10;
                    var errorHandlingFiles = new List<string>();

                    Console.WriteLine($"Creating {errorHandlingBatchSize} test files for error handling...");
                    Console.WriteLine();

                    for (int i = 1; i <= errorHandlingBatchSize; i++)
                    {
                        var fileName = $"error_handling_{i}.txt";
                        var content = $"Error handling test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        errorHandlingFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Perform batch upload with comprehensive error handling
                    Console.WriteLine("Starting batch upload with error handling...");
                    Console.WriteLine();

                    var errorHandlingStopwatch = Stopwatch.StartNew();
                    var errorHandlingResults = new List<BatchUploadResult>();

                    // Create upload tasks with error handling
                    // Each task handles its own errors and reports results
                    var errorHandlingUploadTasks = errorHandlingFiles.Select(async (fileName, index) =>
                    {
                        const int maxRetries = 3;
                        Exception lastException = null;

                        // Retry logic for transient failures
                        for (int attempt = 1; attempt <= maxRetries; attempt++)
                        {
                            try
                            {
                                // Attempt upload
                                var fileId = await client.UploadFileAsync(fileName, null);
                                
                                // Success - return result
                                return new BatchUploadResult
                                {
                                    FileName = fileName,
                                    FileId = fileId,
                                    Success = true,
                                    Index = index + 1,
                                    Attempts = attempt
                                };
                            }
                            catch (FastDFSNetworkException ex)
                            {
                                // Network error - retry
                                lastException = ex;
                                
                                if (attempt < maxRetries)
                                {
                                    // Wait before retry (exponential backoff)
                                    var delaySeconds = Math.Pow(2, attempt - 1);
                                    await Task.Delay(TimeSpan.FromSeconds(delaySeconds));
                                }
                            }
                            catch (FastDFSFileNotFoundException ex)
                            {
                                // File not found - don't retry
                                return new BatchUploadResult
                                {
                                    FileName = fileName,
                                    FileId = null,
                                    Success = false,
                                    ErrorMessage = $"File not found: {ex.Message}",
                                    Index = index + 1,
                                    Attempts = attempt
                                };
                            }
                            catch (FastDFSProtocolException ex)
                            {
                                // Protocol error - don't retry
                                return new BatchUploadResult
                                {
                                    FileName = fileName,
                                    FileId = null,
                                    Success = false,
                                    ErrorMessage = $"Protocol error: {ex.Message}",
                                    Index = index + 1,
                                    Attempts = attempt
                                };
                            }
                            catch (Exception ex)
                            {
                                // Other errors - retry
                                lastException = ex;
                                
                                if (attempt < maxRetries)
                                {
                                    var delaySeconds = Math.Pow(2, attempt - 1);
                                    await Task.Delay(TimeSpan.FromSeconds(delaySeconds));
                                }
                            }
                        }

                        // All retries failed
                        return new BatchUploadResult
                        {
                            FileName = fileName,
                            FileId = null,
                            Success = false,
                            ErrorMessage = $"Failed after {maxRetries} attempts: {lastException?.Message}",
                            Index = index + 1,
                            Attempts = maxRetries
                        };
                    }).ToArray();

                    // Wait for all uploads to complete
                    var errorHandlingUploadResults = await Task.WhenAll(errorHandlingUploadTasks);

                    errorHandlingStopwatch.Stop();

                    // Display error handling results
                    var successfulErrorHandling = errorHandlingUploadResults.Count(r => r.Success);
                    var failedErrorHandling = errorHandlingUploadResults.Count(r => !r.Success);

                    Console.WriteLine();
                    Console.WriteLine("Error handling batch upload results:");
                    Console.WriteLine($"  Total files: {errorHandlingBatchSize}");
                    Console.WriteLine($"  Successful: {successfulErrorHandling}");
                    Console.WriteLine($"  Failed: {failedErrorHandling}");
                    Console.WriteLine($"  Success rate: {(successfulErrorHandling / (double)errorHandlingBatchSize * 100):F1}%");
                    Console.WriteLine($"  Total time: {errorHandlingStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine();

                    // Display detailed error information
                    if (failedErrorHandling > 0)
                    {
                        Console.WriteLine("Failed uploads with error details:");
                        foreach (var result in errorHandlingUploadResults.Where(r => !r.Success))
                        {
                            Console.WriteLine($"  {result.FileName}:");
                            Console.WriteLine($"    Error: {result.ErrorMessage}");
                            Console.WriteLine($"    Attempts: {result.Attempts}");
                        }
                        Console.WriteLine();
                    }

                    // Display retry statistics
                    var retryStats = errorHandlingUploadResults
                        .GroupBy(r => r.Attempts)
                        .Select(g => new { Attempts = g.Key, Count = g.Count() })
                        .OrderBy(x => x.Attempts);

                    Console.WriteLine("Retry statistics:");
                    foreach (var stat in retryStats)
                    {
                        Console.WriteLine($"  {stat.Attempts} attempt(s): {stat.Count} files");
                    }
                    Console.WriteLine();

                    // ============================================================
                    // Example 6: Performance-Optimized Batch Operations
                    // ============================================================
                    // 
                    // This example demonstrates performance optimization techniques
                    // for batch operations. Performance optimization is important
                    // for processing large batches efficiently.
                    // 
                    // Optimization techniques:
                    // - Batch size optimization
                    // - Concurrent processing limits
                    // - Connection pool tuning
                    // - Resource management
                    // ============================================================

                    Console.WriteLine("Example 6: Performance-Optimized Batch Operations");
                    Console.WriteLine("===================================================");
                    Console.WriteLine();

                    // Create test files for performance optimization
                    const int optimizedBatchSize = 30;
                    var optimizedFiles = new List<string>();

                    Console.WriteLine($"Creating {optimizedBatchSize} test files for performance optimization...");
                    Console.WriteLine();

                    for (int i = 1; i <= optimizedBatchSize; i++)
                    {
                        var fileName = $"optimized_{i}.txt";
                        var content = $"Performance optimized batch test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        optimizedFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Performance-optimized batch upload
                    // Use semaphore to limit concurrent operations
                    // This prevents overwhelming the connection pool
                    const int maxConcurrent = 10;  // Limit concurrent operations
                    var semaphore = new SemaphoreSlim(maxConcurrent);
                    var optimizedResults = new List<BatchUploadResult>();

                    Console.WriteLine($"Starting performance-optimized batch upload (max {maxConcurrent} concurrent)...");
                    Console.WriteLine();

                    var optimizedStopwatch = Stopwatch.StartNew();

                    // Create upload tasks with concurrency limiting
                    // Semaphore limits the number of concurrent operations
                    var optimizedUploadTasks = optimizedFiles.Select(async (fileName, index) =>
                    {
                        // Wait for semaphore slot
                        await semaphore.WaitAsync();
                        
                        try
                        {
                            // Upload the file
                            var fileId = await client.UploadFileAsync(fileName, null);
                            
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = fileId,
                                Success = true,
                                Index = index + 1
                            };
                        }
                        catch (Exception ex)
                        {
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = null,
                                Success = false,
                                ErrorMessage = ex.Message,
                                Index = index + 1
                            };
                        }
                        finally
                        {
                            // Release semaphore slot
                            semaphore.Release();
                        }
                    }).ToArray();

                    // Wait for all uploads to complete
                    var optimizedUploadResults = await Task.WhenAll(optimizedUploadTasks);

                    optimizedStopwatch.Stop();

                    // Display optimization results
                    var successfulOptimized = optimizedUploadResults.Count(r => r.Success);

                    Console.WriteLine();
                    Console.WriteLine("Performance-optimized batch upload results:");
                    Console.WriteLine($"  Total files: {optimizedBatchSize}");
                    Console.WriteLine($"  Successful: {successfulOptimized}");
                    Console.WriteLine($"  Failed: {optimizedBatchSize - successfulOptimized}");
                    Console.WriteLine($"  Max concurrent: {maxConcurrent}");
                    Console.WriteLine($"  Total time: {optimizedStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {optimizedStopwatch.ElapsedMilliseconds / (double)optimizedBatchSize:F2} ms");
                    Console.WriteLine($"  Throughput: {optimizedBatchSize / (optimizedStopwatch.ElapsedMilliseconds / 1000.0):F2} files/second");
                    Console.WriteLine();

                    // ============================================================
                    // Example 7: Large-Scale Batch Operations
                    // ============================================================
                    // 
                    // This example demonstrates handling large-scale batch
                    // operations with many files. Large-scale batches require
                    // special considerations for memory, performance, and error handling.
                    // 
                    // Large-scale batch considerations:
                    // - Memory management
                    // - Progress tracking
                    // - Error handling
                    // - Performance optimization
                    // - Resource cleanup
                    // ============================================================

                    Console.WriteLine("Example 7: Large-Scale Batch Operations");
                    Console.WriteLine("========================================");
                    Console.WriteLine();

                    // Create test files for large-scale batch
                    const int largeScaleBatchSize = 50;
                    var largeScaleFiles = new List<string>();

                    Console.WriteLine($"Creating {largeScaleBatchSize} test files for large-scale batch...");
                    Console.WriteLine();

                    for (int i = 1; i <= largeScaleBatchSize; i++)
                    {
                        var fileName = $"largescale_{i}.txt";
                        var content = $"Large-scale batch test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        largeScaleFiles.Add(fileName);
                        
                        if (i % 10 == 0)
                        {
                            Console.WriteLine($"  Created {i}/{largeScaleBatchSize} files...");
                        }
                    }

                    Console.WriteLine("All test files created.");
                    Console.WriteLine();

                    // Perform large-scale batch upload with progress tracking
                    Console.WriteLine($"Starting large-scale batch upload ({largeScaleBatchSize} files)...");
                    Console.WriteLine();

                    var largeScaleStopwatch = Stopwatch.StartNew();
                    var largeScaleCompleted = 0;
                    var largeScaleLock = new object();

                    // Create upload tasks with progress tracking
                    var largeScaleUploadTasks = largeScaleFiles.Select(async (fileName, index) =>
                    {
                        try
                        {
                            var fileId = await client.UploadFileAsync(fileName, null);
                            
                            // Update progress
                            lock (largeScaleLock)
                            {
                                largeScaleCompleted++;
                                if (largeScaleCompleted % 5 == 0 || largeScaleCompleted == largeScaleBatchSize)
                                {
                                    var progress = (largeScaleCompleted / (double)largeScaleBatchSize) * 100;
                                    Console.WriteLine($"  Progress: {largeScaleCompleted}/{largeScaleBatchSize} files ({progress:F1}%)");
                                }
                            }
                            
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = fileId,
                                Success = true,
                                Index = index + 1
                            };
                        }
                        catch (Exception ex)
                        {
                            lock (largeScaleLock)
                            {
                                largeScaleCompleted++;
                            }
                            
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = null,
                                Success = false,
                                ErrorMessage = ex.Message,
                                Index = index + 1
                            };
                        }
                    }).ToArray();

                    // Wait for all uploads to complete
                    var largeScaleResults = await Task.WhenAll(largeScaleUploadTasks);

                    largeScaleStopwatch.Stop();

                    // Display large-scale results
                    var successfulLargeScale = largeScaleResults.Count(r => r.Success);

                    Console.WriteLine();
                    Console.WriteLine("Large-scale batch upload results:");
                    Console.WriteLine($"  Total files: {largeScaleBatchSize}");
                    Console.WriteLine($"  Successful: {successfulLargeScale}");
                    Console.WriteLine($"  Failed: {largeScaleBatchSize - successfulLargeScale}");
                    Console.WriteLine($"  Success rate: {(successfulLargeScale / (double)largeScaleBatchSize * 100):F1}%");
                    Console.WriteLine($"  Total time: {largeScaleStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Average time per file: {largeScaleStopwatch.ElapsedMilliseconds / (double)largeScaleBatchSize:F2} ms");
                    Console.WriteLine($"  Throughput: {largeScaleBatchSize / (largeScaleStopwatch.ElapsedMilliseconds / 1000.0):F2} files/second");
                    Console.WriteLine();

                    // ============================================================
                    // Example 8: Batch Operations with Cancellation Support
                    // ============================================================
                    // 
                    // This example demonstrates batch operations with cancellation
                    // support. Cancellation is important for long-running batch
                    // operations that users might want to cancel.
                    // 
                    // Cancellation features:
                    // - Cancellation token support
                    // - Graceful cancellation handling
                    // - Partial results on cancellation
                    // ============================================================

                    Console.WriteLine("Example 8: Batch Operations with Cancellation Support");
                    Console.WriteLine("=======================================================");
                    Console.WriteLine();

                    // Create test files for cancellation example
                    const int cancellationBatchSize = 8;
                    var cancellationFiles = new List<string>();

                    Console.WriteLine($"Creating {cancellationBatchSize} test files for cancellation example...");
                    Console.WriteLine();

                    for (int i = 1; i <= cancellationBatchSize; i++)
                    {
                        var fileName = $"cancellation_{i}.txt";
                        var content = $"Cancellation test file {i}";
                        await File.WriteAllTextAsync(fileName, content);
                        cancellationFiles.Add(fileName);
                    }

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Create cancellation token source
                    var cancellationTokenSource = new CancellationTokenSource();
                    var cancellationToken = cancellationTokenSource.Token;

                    Console.WriteLine("Starting batch upload with cancellation support...");
                    Console.WriteLine("(Cancellation will be triggered after 3 files for demonstration)");
                    Console.WriteLine();

                    var cancellationStopwatch = Stopwatch.StartNew();
                    var cancellationCompleted = 0;
                    var cancellationLock = new object();

                    // Create upload tasks with cancellation support
                    var cancellationUploadTasks = cancellationFiles.Select(async (fileName, index) =>
                    {
                        // Check for cancellation before starting
                        cancellationToken.ThrowIfCancellationRequested();

                        try
                        {
                            // Upload with cancellation token
                            var fileId = await client.UploadFileAsync(fileName, null, cancellationToken);
                            
                            lock (cancellationLock)
                            {
                                cancellationCompleted++;
                                
                                // Trigger cancellation after 3 files for demonstration
                                if (cancellationCompleted == 3 && !cancellationTokenSource.IsCancellationRequested)
                                {
                                    Console.WriteLine($"  Cancelling batch operation after {cancellationCompleted} files...");
                                    cancellationTokenSource.Cancel();
                                }
                            }
                            
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = fileId,
                                Success = true,
                                Index = index + 1
                            };
                        }
                        catch (OperationCanceledException)
                        {
                            // Operation was cancelled
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = null,
                                Success = false,
                                ErrorMessage = "Operation cancelled",
                                Index = index + 1
                            };
                        }
                        catch (Exception ex)
                        {
                            return new BatchUploadResult
                            {
                                FileName = fileName,
                                FileId = null,
                                Success = false,
                                ErrorMessage = ex.Message,
                                Index = index + 1
                            };
                        }
                    }).ToArray();

                    // Wait for all tasks (some may be cancelled)
                    try
                    {
                        await Task.WhenAll(cancellationUploadTasks);
                    }
                    catch (OperationCanceledException)
                    {
                        Console.WriteLine("  Batch operation was cancelled.");
                    }

                    cancellationStopwatch.Stop();

                    // Get results (including cancelled operations)
                    var cancellationResults = cancellationUploadTasks
                        .Select(t => t.IsCompletedSuccessfully ? t.Result : new BatchUploadResult
                        {
                            FileName = "unknown",
                            FileId = null,
                            Success = false,
                            ErrorMessage = "Task not completed"
                        })
                        .ToList();

                    // Display cancellation results
                    var successfulCancellation = cancellationResults.Count(r => r.Success);
                    var cancelledCount = cancellationResults.Count(r => r.ErrorMessage == "Operation cancelled");

                    Console.WriteLine();
                    Console.WriteLine("Cancellation batch upload results:");
                    Console.WriteLine($"  Total files: {cancellationBatchSize}");
                    Console.WriteLine($"  Successful: {successfulCancellation}");
                    Console.WriteLine($"  Cancelled: {cancelledCount}");
                    Console.WriteLine($"  Failed: {cancellationBatchSize - successfulCancellation - cancelledCount}");
                    Console.WriteLine($"  Total time: {cancellationStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for batch operations
                    // in FastDFS applications, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for Batch Operations");
                    Console.WriteLine("====================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Batch Size Optimization:");
                    Console.WriteLine("   - Choose appropriate batch sizes based on your workload");
                    Console.WriteLine("   - Balance between throughput and resource usage");
                    Console.WriteLine("   - Consider memory constraints for large batches");
                    Console.WriteLine("   - Test different batch sizes to find optimal value");
                    Console.WriteLine();
                    Console.WriteLine("2. Progress Tracking:");
                    Console.WriteLine("   - Implement progress tracking for user experience");
                    Console.WriteLine("   - Use thread-safe progress updates");
                    Console.WriteLine("   - Provide percentage completion and file counts");
                    Console.WriteLine("   - Consider estimated time remaining");
                    Console.WriteLine();
                    Console.WriteLine("3. Error Handling:");
                    Console.WriteLine("   - Handle errors for individual files in batches");
                    Console.WriteLine("   - Don't let one failure stop the entire batch");
                    Console.WriteLine("   - Implement retry logic for transient failures");
                    Console.WriteLine("   - Log errors appropriately for monitoring");
                    Console.WriteLine("   - Report partial success when appropriate");
                    Console.WriteLine();
                    Console.WriteLine("4. Performance Optimization:");
                    Console.WriteLine("   - Use concurrent processing for batch operations");
                    Console.WriteLine("   - Limit concurrent operations to prevent overload");
                    Console.WriteLine("   - Use semaphores to control concurrency");
                    Console.WriteLine("   - Monitor connection pool usage");
                    Console.WriteLine("   - Optimize based on your specific workload");
                    Console.WriteLine();
                    Console.WriteLine("5. Resource Management:");
                    Console.WriteLine("   - Clean up resources after batch operations");
                    Console.WriteLine("   - Dispose of file streams properly");
                    Console.WriteLine("   - Monitor memory usage for large batches");
                    Console.WriteLine("   - Use cancellation tokens for long-running batches");
                    Console.WriteLine();
                    Console.WriteLine("6. Monitoring and Logging:");
                    Console.WriteLine("   - Track batch operation metrics");
                    Console.WriteLine("   - Log success/failure rates");
                    Console.WriteLine("   - Monitor operation durations");
                    Console.WriteLine("   - Track throughput and performance");
                    Console.WriteLine("   - Set up alerts for batch failures");
                    Console.WriteLine();
                    Console.WriteLine("7. Large-Scale Batches:");
                    Console.WriteLine("   - Process large batches in chunks if needed");
                    Console.WriteLine("   - Implement progress tracking for large batches");
                    Console.WriteLine("   - Consider memory constraints");
                    Console.WriteLine("   - Use streaming for very large files");
                    Console.WriteLine("   - Plan for peak load scenarios");
                    Console.WriteLine();
                    Console.WriteLine("8. Cancellation Support:");
                    Console.WriteLine("   - Support cancellation for long-running batches");
                    Console.WriteLine("   - Use cancellation tokens appropriately");
                    Console.WriteLine("   - Handle cancellation gracefully");
                    Console.WriteLine("   - Provide partial results on cancellation");
                    Console.WriteLine();
                    Console.WriteLine("9. Testing:");
                    Console.WriteLine("   - Test with various batch sizes");
                    Console.WriteLine("   - Test error handling scenarios");
                    Console.WriteLine("   - Test cancellation behavior");
                    Console.WriteLine("   - Test under different load conditions");
                    Console.WriteLine("   - Verify progress tracking accuracy");
                    Console.WriteLine();
                    Console.WriteLine("10. Best Practices Summary:");
                    Console.WriteLine("    - Use batch operations for better performance");
                    Console.WriteLine("    - Implement progress tracking for user experience");
                    Console.WriteLine("    - Handle errors gracefully");
                    Console.WriteLine("    - Optimize batch sizes and concurrency");
                    Console.WriteLine("    - Monitor and log batch operations");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up uploaded files and local test files
                    // ============================================================

                    Console.WriteLine("Cleaning up...");
                    Console.WriteLine();

                    // Collect all file IDs from successful uploads
                    var allUploadedFileIds = batchUploadResults
                        .Where(r => r.Success)
                        .Select(r => r.FileId)
                        .Concat(progressResults.Where(r => r.Success).Select(r => r.FileId))
                        .Concat(errorHandlingUploadResults.Where(r => r.Success).Select(r => r.FileId))
                        .Concat(optimizedUploadResults.Where(r => r.Success).Select(r => r.FileId))
                        .Concat(largeScaleResults.Where(r => r.Success).Select(r => r.FileId))
                        .Concat(cancellationResults.Where(r => r.Success).Select(r => r.FileId))
                        .Distinct()
                        .ToList();

                    Console.WriteLine($"Deleting {allUploadedFileIds.Count} uploaded files...");

                    // Delete files in batches for efficiency
                    const int deleteBatchSize = 20;
                    var deleteBatches = allUploadedFileIds
                        .Select((fileId, index) => new { fileId, index })
                        .GroupBy(x => x.index / deleteBatchSize)
                        .Select(g => g.Select(x => x.fileId).ToList())
                        .ToList();

                    var totalDeleted = 0;
                    foreach (var deleteBatch in deleteBatches)
                    {
                        var deleteTasks = deleteBatch.Select(async fileId =>
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
                        totalDeleted += deleteResults.Count(r => r);
                    }

                    Console.WriteLine($"Deleted {totalDeleted} files");
                    Console.WriteLine();

                    // Delete local test files
                    var allLocalFiles = batchFiles
                        .Concat(progressFiles)
                        .Concat(errorHandlingFiles)
                        .Concat(optimizedFiles)
                        .Concat(largeScaleFiles)
                        .Concat(cancellationFiles)
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

    // ====================================================================
    // Helper Classes for Batch Operations
    // ====================================================================

    /// <summary>
    /// Represents the result of a batch upload operation.
    /// 
    /// This class contains information about the result of uploading
    /// a single file as part of a batch operation, including success
    /// status, file ID, error information, and retry attempts.
    /// </summary>
    class BatchUploadResult
    {
        /// <summary>
        /// Gets or sets the name of the file that was uploaded.
        /// </summary>
        public string FileName { get; set; }

        /// <summary>
        /// Gets or sets the file ID returned from the upload operation.
        /// This is null if the upload failed.
        /// </summary>
        public string FileId { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether the upload was successful.
        /// </summary>
        public bool Success { get; set; }

        /// <summary>
        /// Gets or sets the error message if the upload failed.
        /// This is null if the upload was successful.
        /// </summary>
        public string ErrorMessage { get; set; }

        /// <summary>
        /// Gets or sets the index of this file in the batch (1-based).
        /// </summary>
        public int Index { get; set; }

        /// <summary>
        /// Gets or sets the number of attempts made for this upload.
        /// This is useful for tracking retry behavior.
        /// </summary>
        public int Attempts { get; set; }
    }

    /// <summary>
    /// Represents the result of a batch download operation.
    /// 
    /// This class contains information about the result of downloading
    /// a single file as part of a batch operation, including success
    /// status, file data, error information, and file size.
    /// </summary>
    class BatchDownloadResult
    {
        /// <summary>
        /// Gets or sets the file ID that was downloaded.
        /// </summary>
        public string FileId { get; set; }

        /// <summary>
        /// Gets or sets the file data downloaded from FastDFS.
        /// This is null if the download failed.
        /// </summary>
        public byte[] Data { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether the download was successful.
        /// </summary>
        public bool Success { get; set; }

        /// <summary>
        /// Gets or sets the error message if the download failed.
        /// This is null if the download was successful.
        /// </summary>
        public string ErrorMessage { get; set; }

        /// <summary>
        /// Gets or sets the size of the downloaded file in bytes.
        /// This is 0 if the download failed.
        /// </summary>
        public long Size { get; set; }

        /// <summary>
        /// Gets or sets the index of this file in the batch (1-based).
        /// </summary>
        public int Index { get; set; }
    }
}

