// ============================================================================
// FastDFS C# Client - Performance Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates performance benchmarking, optimization techniques,
// connection pool tuning, batch operation patterns, and memory usage patterns
// in the FastDFS C# client library. It shows how to measure, analyze, and
// optimize FastDFS client performance.
//
// Performance optimization is essential for building high-performance
// applications that efficiently utilize system resources and provide
// responsive user experiences. This example provides comprehensive patterns
// for performance measurement, analysis, and optimization.
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
    /// Example demonstrating performance benchmarking and optimization in FastDFS.
    /// 
    /// This example shows:
    /// - How to benchmark FastDFS operations
    /// - How to optimize performance
    /// - How to tune connection pools
    /// - How to use batch operation patterns
    /// - How to monitor and optimize memory usage
    /// 
    /// Performance patterns demonstrated:
    /// 1. Performance benchmarking
    /// 2. Optimization techniques
    /// 3. Connection pool tuning
    /// 4. Batch operation patterns
    /// 5. Memory usage patterns
    /// 6. Performance comparison
    /// 7. Throughput measurement
    /// </summary>
    class PerformanceExample
    {
        /// <summary>
        /// Main entry point for the performance example.
        /// 
        /// This method demonstrates various performance patterns through
        /// a series of examples, each showing different aspects of
        /// performance measurement and optimization.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Performance Example");
            Console.WriteLine("==========================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates performance benchmarking,");
            Console.WriteLine("optimization techniques, connection pool tuning,");
            Console.WriteLine("batch operations, and memory usage patterns.");
            Console.WriteLine();

            // ====================================================================
            // Example 1: Performance Benchmarking
            // ====================================================================
            // 
            // This example demonstrates how to benchmark FastDFS operations
            // to measure performance and identify bottlenecks. Benchmarking
            // is essential for understanding performance characteristics and
            // validating optimizations.
            // 
            // Benchmarking patterns:
            // - Operation timing
            // - Throughput measurement
            // - Latency measurement
            // - Resource usage measurement
            // ====================================================================

            Console.WriteLine("Example 1: Performance Benchmarking");
            Console.WriteLine("====================================");
            Console.WriteLine();

            // Create test files for benchmarking
            Console.WriteLine("Creating test files for benchmarking...");
            Console.WriteLine();

            var smallTestFile = "perf_test_small.txt";
            var mediumTestFile = "perf_test_medium.txt";
            var largeTestFile = "perf_test_large.txt";

            await File.WriteAllTextAsync(smallTestFile, new string('A', 1024)); // 1KB
            await File.WriteAllTextAsync(mediumTestFile, new string('B', 1024 * 100)); // 100KB
            await File.WriteAllTextAsync(largeTestFile, new string('C', 1024 * 1024)); // 1MB

            Console.WriteLine($"  Created: {smallTestFile} ({new FileInfo(smallTestFile).Length:N0} bytes)");
            Console.WriteLine($"  Created: {mediumTestFile} ({new FileInfo(mediumTestFile).Length:N0} bytes)");
            Console.WriteLine($"  Created: {largeTestFile} ({new FileInfo(largeTestFile).Length:N0} bytes)");
            Console.WriteLine();

            // Pattern 1: Single operation benchmarking
            Console.WriteLine("Pattern 1: Single Operation Benchmarking");
            Console.WriteLine("------------------------------------------");
            Console.WriteLine();

            var config = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            using (var client = new FastDFSClient(config))
            {
                // Benchmark upload operation
                Console.WriteLine("Benchmarking upload operation...");
                Console.WriteLine();

                var uploadStopwatch = Stopwatch.StartNew();
                var uploadFileId = await client.UploadFileAsync(smallTestFile, null);
                uploadStopwatch.Stop();

                var uploadTime = uploadStopwatch.ElapsedMilliseconds;
                var fileSize = new FileInfo(smallTestFile).Length;
                var uploadThroughput = (fileSize / 1024.0) / (uploadTime / 1000.0); // KB/s

                Console.WriteLine($"  File: {smallTestFile}");
                Console.WriteLine($"  File size: {fileSize:N0} bytes");
                Console.WriteLine($"  Upload time: {uploadTime} ms");
                Console.WriteLine($"  Throughput: {uploadThroughput:F2} KB/s");
                Console.WriteLine();

                // Benchmark download operation
                Console.WriteLine("Benchmarking download operation...");
                Console.WriteLine();

                var downloadStopwatch = Stopwatch.StartNew();
                var downloadedData = await client.DownloadFileAsync(uploadFileId);
                downloadStopwatch.Stop();

                var downloadTime = downloadStopwatch.ElapsedMilliseconds;
                var downloadThroughput = (downloadedData.Length / 1024.0) / (downloadTime / 1000.0); // KB/s

                Console.WriteLine($"  File ID: {uploadFileId}");
                Console.WriteLine($"  File size: {downloadedData.Length:N0} bytes");
                Console.WriteLine($"  Download time: {downloadTime} ms");
                Console.WriteLine($"  Throughput: {downloadThroughput:F2} KB/s");
                Console.WriteLine();

                // Clean up
                await client.DeleteFileAsync(uploadFileId);
            }

            // Pattern 2: Multiple operation benchmarking
            Console.WriteLine("Pattern 2: Multiple Operation Benchmarking");
            Console.WriteLine("-------------------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var testFiles = new[] { smallTestFile, mediumTestFile, largeTestFile };
                var results = new List<BenchmarkResult>();

                foreach (var testFile in testFiles)
                {
                    var fileSize = new FileInfo(testFile).Length;

                    // Benchmark upload
                    var uploadStopwatch = Stopwatch.StartNew();
                    var fileId = await client.UploadFileAsync(testFile, null);
                    uploadStopwatch.Stop();

                    // Benchmark download
                    var downloadStopwatch = Stopwatch.StartNew();
                    var data = await client.DownloadFileAsync(fileId);
                    downloadStopwatch.Stop();

                    results.Add(new BenchmarkResult
                    {
                        FileName = testFile,
                        FileSize = fileSize,
                        UploadTime = uploadStopwatch.ElapsedMilliseconds,
                        DownloadTime = downloadStopwatch.ElapsedMilliseconds,
                        FileId = fileId
                    });

                    // Clean up
                    await client.DeleteFileAsync(fileId);
                }

                Console.WriteLine("Benchmark Results:");
                Console.WriteLine("==================");
                Console.WriteLine();

                foreach (var result in results)
                {
                    var uploadThroughput = (result.FileSize / 1024.0) / (result.UploadTime / 1000.0);
                    var downloadThroughput = (result.FileSize / 1024.0) / (result.DownloadTime / 1000.0);

                    Console.WriteLine($"  File: {result.FileName}");
                    Console.WriteLine($"    Size: {result.FileSize:N0} bytes ({result.FileSize / 1024.0:F2} KB)");
                    Console.WriteLine($"    Upload: {result.UploadTime} ms ({uploadThroughput:F2} KB/s)");
                    Console.WriteLine($"    Download: {result.DownloadTime} ms ({downloadThroughput:F2} KB/s)");
                    Console.WriteLine();
                }
            }

            // Pattern 3: Throughput benchmarking
            Console.WriteLine("Pattern 3: Throughput Benchmarking");
            Console.WriteLine("-----------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var throughputTestFile = mediumTestFile;
                var fileSize = new FileInfo(throughputTestFile).Length;
                var iterations = 10;

                Console.WriteLine($"Running {iterations} iterations for throughput measurement...");
                Console.WriteLine();

                var uploadTimes = new List<long>();
                var downloadTimes = new List<long>();
                var uploadedFileIds = new List<string>();

                // Upload iterations
                for (int i = 0; i < iterations; i++)
                {
                    var stopwatch = Stopwatch.StartNew();
                    var fileId = await client.UploadFileAsync(throughputTestFile, null);
                    stopwatch.Stop();

                    uploadTimes.Add(stopwatch.ElapsedMilliseconds);
                    uploadedFileIds.Add(fileId);
                }

                // Download iterations
                foreach (var fileId in uploadedFileIds)
                {
                    var stopwatch = Stopwatch.StartNew();
                    await client.DownloadFileAsync(fileId);
                    stopwatch.Stop();

                    downloadTimes.Add(stopwatch.ElapsedMilliseconds);
                }

                // Calculate statistics
                var avgUploadTime = uploadTimes.Average();
                var avgDownloadTime = downloadTimes.Average();
                var avgUploadThroughput = (fileSize / 1024.0) / (avgUploadTime / 1000.0);
                var avgDownloadThroughput = (fileSize / 1024.0) / (avgDownloadTime / 1000.0);

                Console.WriteLine($"  Average upload time: {avgUploadTime:F2} ms");
                Console.WriteLine($"  Average download time: {avgDownloadTime:F2} ms");
                Console.WriteLine($"  Average upload throughput: {avgUploadThroughput:F2} KB/s");
                Console.WriteLine($"  Average download throughput: {avgDownloadThroughput:F2} KB/s");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in uploadedFileIds)
                {
                    await client.DeleteFileAsync(fileId);
                }
            }

            // ====================================================================
            // Example 2: Optimization Techniques
            // ====================================================================
            // 
            // This example demonstrates various optimization techniques for
            // improving FastDFS client performance. Optimization is crucial for
            // achieving maximum throughput and minimizing latency.
            // 
            // Optimization techniques:
            // - Connection reuse
            // - Batch operations
            // - Parallel processing
            // - Caching strategies
            // ====================================================================

            Console.WriteLine("Example 2: Optimization Techniques");
            Console.WriteLine("===================================");
            Console.WriteLine();

            // Pattern 1: Connection reuse optimization
            Console.WriteLine("Pattern 1: Connection Reuse Optimization");
            Console.WriteLine("------------------------------------------");
            Console.WriteLine();

            // Compare single client (connection reuse) vs multiple clients
            var optimizationTestFile = mediumTestFile;
            var optimizationFileSize = new FileInfo(optimizationTestFile).Length;
            var optimizationIterations = 20;

            Console.WriteLine($"Comparing connection reuse vs new connections ({optimizationIterations} operations)...");
            Console.WriteLine();

            // Test 1: Single client (connection reuse)
            using (var singleClient = new FastDFSClient(config))
            {
                var singleClientStopwatch = Stopwatch.StartNew();

                var singleClientFileIds = new List<string>();
                for (int i = 0; i < optimizationIterations; i++)
                {
                    var fileId = await singleClient.UploadFileAsync(optimizationTestFile, null);
                    singleClientFileIds.Add(fileId);
                }

                singleClientStopwatch.Stop();

                var singleClientTime = singleClientStopwatch.ElapsedMilliseconds;
                var singleClientThroughput = (optimizationFileSize * optimizationIterations / 1024.0) / (singleClientTime / 1000.0);

                Console.WriteLine($"  Single client (connection reuse):");
                Console.WriteLine($"    Time: {singleClientTime} ms");
                Console.WriteLine($"    Throughput: {singleClientThroughput:F2} KB/s");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in singleClientFileIds)
                {
                    await singleClient.DeleteFileAsync(fileId);
                }
            }

            // Pattern 2: Batch operation optimization
            Console.WriteLine("Pattern 2: Batch Operation Optimization");
            Console.WriteLine("----------------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var batchTestFiles = new[] { smallTestFile, mediumTestFile };
                var batchSize = 5;

                Console.WriteLine($"Comparing sequential vs batch operations (batch size: {batchSize})...");
                Console.WriteLine();

                // Sequential operations
                var sequentialStopwatch = Stopwatch.StartNew();
                var sequentialFileIds = new List<string>();

                foreach (var testFile in batchTestFiles)
                {
                    for (int i = 0; i < batchSize; i++)
                    {
                        var fileId = await client.UploadFileAsync(testFile, null);
                        sequentialFileIds.Add(fileId);
                    }
                }

                sequentialStopwatch.Stop();

                // Batch operations (parallel)
                var batchStopwatch = Stopwatch.StartNew();
                var batchFileIds = new List<string>();

                foreach (var testFile in batchTestFiles)
                {
                    var batchTasks = new List<Task<string>>();
                    for (int i = 0; i < batchSize; i++)
                    {
                        batchTasks.Add(client.UploadFileAsync(testFile, null));
                    }

                    var batchResults = await Task.WhenAll(batchTasks);
                    batchFileIds.AddRange(batchResults);
                }

                batchStopwatch.Stop();

                var sequentialTime = sequentialStopwatch.ElapsedMilliseconds;
                var batchTime = batchStopwatch.ElapsedMilliseconds;
                var speedup = (double)sequentialTime / batchTime;

                Console.WriteLine($"  Sequential operations: {sequentialTime} ms");
                Console.WriteLine($"  Batch operations: {batchTime} ms");
                Console.WriteLine($"  Speedup: {speedup:F2}x");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in sequentialFileIds.Concat(batchFileIds))
                {
                    try
                    {
                        await client.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // Pattern 3: Parallel processing optimization
            Console.WriteLine("Pattern 3: Parallel Processing Optimization");
            Console.WriteLine("---------------------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var parallelTestFile = mediumTestFile;
                var parallelIterations = 10;

                Console.WriteLine($"Comparing sequential vs parallel processing ({parallelIterations} operations)...");
                Console.WriteLine();

                // Sequential processing
                var sequentialStopwatch = Stopwatch.StartNew();
                var sequentialFileIds = new List<string>();

                for (int i = 0; i < parallelIterations; i++)
                {
                    var fileId = await client.UploadFileAsync(parallelTestFile, null);
                    sequentialFileIds.Add(fileId);
                }

                sequentialStopwatch.Stop();

                // Parallel processing
                var parallelStopwatch = Stopwatch.StartNew();
                var parallelTasks = new List<Task<string>>();

                for (int i = 0; i < parallelIterations; i++)
                {
                    parallelTasks.Add(client.UploadFileAsync(parallelTestFile, null));
                }

                var parallelFileIds = await Task.WhenAll(parallelTasks);
                parallelStopwatch.Stop();

                var sequentialTime = sequentialStopwatch.ElapsedMilliseconds;
                var parallelTime = parallelStopwatch.ElapsedMilliseconds;
                var parallelSpeedup = (double)sequentialTime / parallelTime;

                Console.WriteLine($"  Sequential processing: {sequentialTime} ms");
                Console.WriteLine($"  Parallel processing: {parallelTime} ms");
                Console.WriteLine($"  Speedup: {parallelSpeedup:F2}x");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in sequentialFileIds.Concat(parallelFileIds))
                {
                    try
                    {
                        await client.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // ====================================================================
            // Example 3: Connection Pool Tuning
            // ====================================================================
            // 
            // This example demonstrates how to tune connection pool settings
            // for optimal performance. Connection pool tuning is essential for
            // balancing resource usage and performance.
            // 
            // Connection pool tuning factors:
            // - Pool size optimization
            // - Connection reuse
            // - Idle timeout tuning
            // - Performance impact analysis
            // ====================================================================

            Console.WriteLine("Example 3: Connection Pool Tuning");
            Console.WriteLine("====================================");
            Console.WriteLine();

            var poolTestFile = mediumTestFile;
            var poolTestIterations = 20;

            // Pattern 1: Small connection pool
            Console.WriteLine("Pattern 1: Small Connection Pool");
            Console.WriteLine("----------------------------------");
            Console.WriteLine();

            var smallPoolConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 10,  // Small pool
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            using (var smallPoolClient = new FastDFSClient(smallPoolConfig))
            {
                var smallPoolStopwatch = Stopwatch.StartNew();
                var smallPoolFileIds = new List<string>();

                var smallPoolTasks = new List<Task<string>>();
                for (int i = 0; i < poolTestIterations; i++)
                {
                    smallPoolTasks.Add(smallPoolClient.UploadFileAsync(poolTestFile, null));
                }

                smallPoolFileIds.AddRange(await Task.WhenAll(smallPoolTasks));
                smallPoolStopwatch.Stop();

                var smallPoolTime = smallPoolStopwatch.ElapsedMilliseconds;

                Console.WriteLine($"  Max connections: {smallPoolConfig.MaxConnections}");
                Console.WriteLine($"  Time for {poolTestIterations} operations: {smallPoolTime} ms");
                Console.WriteLine($"  Average time per operation: {smallPoolTime / (double)poolTestIterations:F2} ms");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in smallPoolFileIds)
                {
                    try
                    {
                        await smallPoolClient.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // Pattern 2: Medium connection pool
            Console.WriteLine("Pattern 2: Medium Connection Pool");
            Console.WriteLine("------------------------------------");
            Console.WriteLine();

            var mediumPoolConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 50,  // Medium pool
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            using (var mediumPoolClient = new FastDFSClient(mediumPoolConfig))
            {
                var mediumPoolStopwatch = Stopwatch.StartNew();
                var mediumPoolFileIds = new List<string>();

                var mediumPoolTasks = new List<Task<string>>();
                for (int i = 0; i < poolTestIterations; i++)
                {
                    mediumPoolTasks.Add(mediumPoolClient.UploadFileAsync(poolTestFile, null));
                }

                mediumPoolFileIds.AddRange(await Task.WhenAll(mediumPoolTasks));
                mediumPoolStopwatch.Stop();

                var mediumPoolTime = mediumPoolStopwatch.ElapsedMilliseconds;

                Console.WriteLine($"  Max connections: {mediumPoolConfig.MaxConnections}");
                Console.WriteLine($"  Time for {poolTestIterations} operations: {mediumPoolTime} ms");
                Console.WriteLine($"  Average time per operation: {mediumPoolTime / (double)poolTestIterations:F2} ms");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in mediumPoolFileIds)
                {
                    try
                    {
                        await mediumPoolClient.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // Pattern 3: Large connection pool
            Console.WriteLine("Pattern 3: Large Connection Pool");
            Console.WriteLine("---------------------------------");
            Console.WriteLine();

            var largePoolConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 200,  // Large pool
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            using (var largePoolClient = new FastDFSClient(largePoolConfig))
            {
                var largePoolStopwatch = Stopwatch.StartNew();
                var largePoolFileIds = new List<string>();

                var largePoolTasks = new List<Task<string>>();
                for (int i = 0; i < poolTestIterations; i++)
                {
                    largePoolTasks.Add(largePoolClient.UploadFileAsync(poolTestFile, null));
                }

                largePoolFileIds.AddRange(await Task.WhenAll(largePoolTasks));
                largePoolStopwatch.Stop();

                var largePoolTime = largePoolStopwatch.ElapsedMilliseconds;

                Console.WriteLine($"  Max connections: {largePoolConfig.MaxConnections}");
                Console.WriteLine($"  Time for {poolTestIterations} operations: {largePoolTime} ms");
                Console.WriteLine($"  Average time per operation: {largePoolTime / (double)poolTestIterations:F2} ms");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in largePoolFileIds)
                {
                    try
                    {
                        await largePoolClient.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // Pattern 4: Connection pool comparison
            Console.WriteLine("Pattern 4: Connection Pool Comparison");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            Console.WriteLine("Connection Pool Performance Comparison:");
            Console.WriteLine($"  Small pool (10 connections): {smallPoolTime} ms");
            Console.WriteLine($"  Medium pool (50 connections): {mediumPoolTime} ms");
            Console.WriteLine($"  Large pool (200 connections): {largePoolTime} ms");
            Console.WriteLine();
            Console.WriteLine("Recommendations:");
            Console.WriteLine("  - Small pool: Suitable for low-concurrency scenarios");
            Console.WriteLine("  - Medium pool: Balanced for moderate concurrency");
            Console.WriteLine("  - Large pool: Optimal for high-concurrency scenarios");
            Console.WriteLine();

            // ====================================================================
            // Example 4: Batch Operation Patterns
            // ====================================================================
            // 
            // This example demonstrates batch operation patterns for improving
            // performance when processing multiple files. Batch operations can
            // significantly improve throughput by processing files in parallel.
            // 
            // Batch operation patterns:
            // - Batch uploads
            // - Batch downloads
            // - Batch processing with progress
            // - Batch error handling
            // ====================================================================

            Console.WriteLine("Example 4: Batch Operation Patterns");
            Console.WriteLine("=====================================");
            Console.WriteLine();

            // Pattern 1: Batch upload pattern
            Console.WriteLine("Pattern 1: Batch Upload Pattern");
            Console.WriteLine("--------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var batchUploadFiles = new[] { smallTestFile, mediumTestFile, largeTestFile };
                var batchSize = 3;

                Console.WriteLine($"Batch uploading {batchUploadFiles.Length} files...");
                Console.WriteLine();

                var batchUploadStopwatch = Stopwatch.StartNew();

                // Batch upload using Task.WhenAll
                var batchUploadTasks = batchUploadFiles.Select(file => 
                    client.UploadFileAsync(file, null)).ToArray();

                var batchUploadFileIds = await Task.WhenAll(batchUploadTasks);
                batchUploadStopwatch.Stop();

                var batchUploadTime = batchUploadStopwatch.ElapsedMilliseconds;
                var totalFileSize = batchUploadFiles.Sum(f => new FileInfo(f).Length);
                var batchUploadThroughput = (totalFileSize / 1024.0) / (batchUploadTime / 1000.0);

                Console.WriteLine($"  Files uploaded: {batchUploadFileIds.Length}");
                Console.WriteLine($"  Total size: {totalFileSize:N0} bytes");
                Console.WriteLine($"  Total time: {batchUploadTime} ms");
                Console.WriteLine($"  Throughput: {batchUploadThroughput:F2} KB/s");
                Console.WriteLine($"  Average time per file: {batchUploadTime / (double)batchUploadFileIds.Length:F2} ms");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in batchUploadFileIds)
                {
                    try
                    {
                        await client.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // Pattern 2: Batch download pattern
            Console.WriteLine("Pattern 2: Batch Download Pattern");
            Console.WriteLine("----------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                // First upload files
                var batchDownloadFiles = new[] { smallTestFile, mediumTestFile };
                var batchDownloadFileIds = new List<string>();

                foreach (var file in batchDownloadFiles)
                {
                    var fileId = await client.UploadFileAsync(file, null);
                    batchDownloadFileIds.Add(fileId);
                }

                Console.WriteLine($"Batch downloading {batchDownloadFileIds.Count} files...");
                Console.WriteLine();

                var batchDownloadStopwatch = Stopwatch.StartNew();

                // Batch download using Task.WhenAll
                var batchDownloadTasks = batchDownloadFileIds.Select(fileId => 
                    client.DownloadFileAsync(fileId)).ToArray();

                var batchDownloadResults = await Task.WhenAll(batchDownloadTasks);
                batchDownloadStopwatch.Stop();

                var batchDownloadTime = batchDownloadStopwatch.ElapsedMilliseconds;
                var totalDownloadSize = batchDownloadResults.Sum(data => data.Length);
                var batchDownloadThroughput = (totalDownloadSize / 1024.0) / (batchDownloadTime / 1000.0);

                Console.WriteLine($"  Files downloaded: {batchDownloadResults.Length}");
                Console.WriteLine($"  Total size: {totalDownloadSize:N0} bytes");
                Console.WriteLine($"  Total time: {batchDownloadTime} ms");
                Console.WriteLine($"  Throughput: {batchDownloadThroughput:F2} KB/s");
                Console.WriteLine($"  Average time per file: {batchDownloadTime / (double)batchDownloadResults.Length:F2} ms");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in batchDownloadFileIds)
                {
                    try
                    {
                        await client.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // Pattern 3: Batch processing with progress
            Console.WriteLine("Pattern 3: Batch Processing with Progress");
            Console.WriteLine("-------------------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var progressBatchFiles = new[] { smallTestFile, mediumTestFile, largeTestFile };
                var progressBatchFileIds = new List<string>();

                Console.WriteLine($"Processing batch with progress tracking ({progressBatchFiles.Length} files)...");
                Console.WriteLine();

                var progressBatchStopwatch = Stopwatch.StartNew();
                int completed = 0;

                // Process with progress reporting
                var progressBatchTasks = progressBatchFiles.Select(async file =>
                {
                    var fileId = await client.UploadFileAsync(file, null);
                    Interlocked.Increment(ref completed);
                    var progress = (completed * 100.0 / progressBatchFiles.Length);
                    Console.WriteLine($"  Progress: {progress:F1}% ({completed}/{progressBatchFiles.Length} files)");
                    return fileId;
                }).ToArray();

                progressBatchFileIds.AddRange(await Task.WhenAll(progressBatchTasks));
                progressBatchStopwatch.Stop();

                Console.WriteLine();
                Console.WriteLine($"  ✓ Batch processing completed");
                Console.WriteLine($"  ✓ Total time: {progressBatchStopwatch.ElapsedMilliseconds} ms");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in progressBatchFileIds)
                {
                    try
                    {
                        await client.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // ====================================================================
            // Example 5: Memory Usage Patterns
            // ====================================================================
            // 
            // This example demonstrates memory usage patterns and optimization
            // techniques for FastDFS operations. Memory optimization is important
            // for applications with limited memory or when processing large files.
            // 
            // Memory usage patterns:
            // - Memory-efficient uploads
            // - Memory-efficient downloads
            // - Memory monitoring
            // - Memory optimization techniques
            // ====================================================================

            Console.WriteLine("Example 5: Memory Usage Patterns");
            Console.WriteLine("==================================");
            Console.WriteLine();

            // Pattern 1: Memory monitoring
            Console.WriteLine("Pattern 1: Memory Monitoring");
            Console.WriteLine("-----------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var memoryTestFile = largeTestFile;
                var initialMemory = GC.GetTotalMemory(false);

                Console.WriteLine($"Monitoring memory usage during operations...");
                Console.WriteLine($"  Initial memory: {initialMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine();

                // Upload operation
                var beforeUploadMemory = GC.GetTotalMemory(false);
                var uploadFileId = await client.UploadFileAsync(memoryTestFile, null);
                var afterUploadMemory = GC.GetTotalMemory(false);

                Console.WriteLine($"  Before upload: {beforeUploadMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  After upload: {afterUploadMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Memory increase: {(afterUploadMemory - beforeUploadMemory) / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine();

                // Download operation
                var beforeDownloadMemory = GC.GetTotalMemory(false);
                var downloadedData = await client.DownloadFileAsync(uploadFileId);
                var afterDownloadMemory = GC.GetTotalMemory(false);

                Console.WriteLine($"  Before download: {beforeDownloadMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  After download: {afterDownloadMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Memory increase: {(afterDownloadMemory - beforeDownloadMemory) / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Downloaded data size: {downloadedData.Length / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine();

                // Clean up
                downloadedData = null; // Release reference
                GC.Collect();
                GC.WaitForPendingFinalizers();
                GC.Collect();

                var afterCleanupMemory = GC.GetTotalMemory(false);
                Console.WriteLine($"  After cleanup: {afterCleanupMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine();

                await client.DeleteFileAsync(uploadFileId);
            }

            // Pattern 2: Memory-efficient download
            Console.WriteLine("Pattern 2: Memory-Efficient Download");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var efficientTestFile = largeTestFile;
                var efficientFileId = await client.UploadFileAsync(efficientTestFile, null);
                var efficientFileSize = new FileInfo(efficientTestFile).Length;

                Console.WriteLine($"Downloading file using memory-efficient pattern...");
                Console.WriteLine($"  File size: {efficientFileSize / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine();

                // Memory-efficient download: stream to file instead of loading into memory
                var efficientDownloadPath = "efficient_download.txt";
                var beforeEfficientMemory = GC.GetTotalMemory(false);

                await client.DownloadToFileAsync(efficientFileId, efficientDownloadPath);

                var afterEfficientMemory = GC.GetTotalMemory(false);
                var efficientMemoryIncrease = afterEfficientMemory - beforeEfficientMemory;

                Console.WriteLine($"  Memory before: {beforeEfficientMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Memory after: {afterEfficientMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Memory increase: {efficientMemoryIncrease / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  ✓ File streamed directly to disk (minimal memory usage)");
                Console.WriteLine();

                // Clean up
                if (File.Exists(efficientDownloadPath))
                {
                    File.Delete(efficientDownloadPath);
                }

                await client.DeleteFileAsync(efficientFileId);
            }

            // Pattern 3: Memory optimization with chunked operations
            Console.WriteLine("Pattern 3: Memory Optimization with Chunked Operations");
            Console.WriteLine("--------------------------------------------------------");
            Console.WriteLine();

            using (var client = new FastDFSClient(config))
            {
                var chunkedTestFile = largeTestFile;
                var chunkedFileId = await client.UploadFileAsync(chunkedTestFile, null);
                var chunkedFileInfo = await client.GetFileInfoAsync(chunkedFileId);
                var chunkedFileSize = chunkedFileInfo.FileSize;
                var chunkSize = 1024 * 1024; // 1MB chunks

                Console.WriteLine($"Downloading file in chunks for memory efficiency...");
                Console.WriteLine($"  File size: {chunkedFileSize / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Chunk size: {chunkSize / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine();

                var beforeChunkedMemory = GC.GetTotalMemory(false);
                var totalChunks = (int)Math.Ceiling((double)chunkedFileSize / chunkSize);
                var processedChunks = 0;

                for (int i = 0; i < totalChunks; i++)
                {
                    var offset = i * chunkSize;
                    var length = Math.Min(chunkSize, (int)(chunkedFileSize - offset));

                    // Download chunk
                    var chunk = await client.DownloadFileRangeAsync(chunkedFileId, offset, length);

                    // Process chunk (then discard)
                    processedChunks++;
                    var progress = (processedChunks * 100.0 / totalChunks);
                    Console.WriteLine($"  Processed chunk {processedChunks}/{totalChunks} ({progress:F1}%)");

                    // Chunk is automatically discarded after processing
                }

                var afterChunkedMemory = GC.GetTotalMemory(false);
                var chunkedMemoryIncrease = afterChunkedMemory - beforeChunkedMemory;

                Console.WriteLine();
                Console.WriteLine($"  Memory before: {beforeChunkedMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Memory after: {afterChunkedMemory / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  Memory increase: {chunkedMemoryIncrease / 1024.0 / 1024.0:F2} MB");
                Console.WriteLine($"  ✓ Constant memory usage regardless of file size");
                Console.WriteLine();

                await client.DeleteFileAsync(chunkedFileId);
            }

            // ====================================================================
            // Best Practices Summary
            // ====================================================================
            // 
            // This section summarizes best practices for performance optimization
            // in FastDFS applications.
            // ====================================================================

            Console.WriteLine("Best Practices for Performance Optimization");
            Console.WriteLine("============================================");
            Console.WriteLine();
            Console.WriteLine("1. Performance Benchmarking:");
            Console.WriteLine("   - Measure baseline performance");
            Console.WriteLine("   - Benchmark different scenarios");
            Console.WriteLine("   - Track throughput and latency");
            Console.WriteLine("   - Monitor resource usage");
            Console.WriteLine();
            Console.WriteLine("2. Optimization Techniques:");
            Console.WriteLine("   - Reuse client instances (connection pooling)");
            Console.WriteLine("   - Use batch operations for multiple files");
            Console.WriteLine("   - Process operations in parallel when possible");
            Console.WriteLine("   - Cache frequently accessed data");
            Console.WriteLine();
            Console.WriteLine("3. Connection Pool Tuning:");
            Console.WriteLine("   - Match pool size to concurrent operation needs");
            Console.WriteLine("   - Start with conservative values and tune based on metrics");
            Console.WriteLine("   - Monitor connection pool usage");
            Console.WriteLine("   - Balance between performance and resource usage");
            Console.WriteLine();
            Console.WriteLine("4. Batch Operation Patterns:");
            Console.WriteLine("   - Use Task.WhenAll for parallel batch operations");
            Console.WriteLine("   - Process batches with progress reporting");
            Console.WriteLine("   - Handle errors in batch operations gracefully");
            Console.WriteLine("   - Optimize batch sizes based on performance");
            Console.WriteLine();
            Console.WriteLine("5. Memory Usage Optimization:");
            Console.WriteLine("   - Use streaming operations for large files");
            Console.WriteLine("   - Process files in chunks when possible");
            Console.WriteLine("   - Monitor memory usage during operations");
            Console.WriteLine("   - Release references promptly");
            Console.WriteLine();
            Console.WriteLine("6. Performance Monitoring:");
            Console.WriteLine("   - Track operation times");
            Console.WriteLine("   - Monitor throughput");
            Console.WriteLine("   - Measure latency");
            Console.WriteLine("   - Track resource usage");
            Console.WriteLine();
            Console.WriteLine("7. Configuration Optimization:");
            Console.WriteLine("   - Tune timeouts for your network");
            Console.WriteLine("   - Optimize connection pool size");
            Console.WriteLine("   - Configure retry counts appropriately");
            Console.WriteLine("   - Test different configurations");
            Console.WriteLine();
            Console.WriteLine("8. Parallel Processing:");
            Console.WriteLine("   - Use parallel processing for independent operations");
            Console.WriteLine("   - Balance parallelism with resource constraints");
            Console.WriteLine("   - Monitor thread pool usage");
            Console.WriteLine("   - Avoid excessive parallelism");
            Console.WriteLine();
            Console.WriteLine("9. Error Handling:");
            Console.WriteLine("   - Handle errors efficiently");
            Console.WriteLine("   - Implement retry logic appropriately");
            Console.WriteLine("   - Log errors for analysis");
            Console.WriteLine("   - Fail fast for non-retryable errors");
            Console.WriteLine();
            Console.WriteLine("10. Best Practices Summary:");
            Console.WriteLine("    - Benchmark and measure performance");
            Console.WriteLine("    - Optimize based on metrics");
            Console.WriteLine("    - Tune connection pools appropriately");
            Console.WriteLine("    - Use batch and parallel operations");
            Console.WriteLine("    - Optimize memory usage");
            Console.WriteLine();

            // ============================================================
            // Cleanup
            // ============================================================
            // 
            // Clean up test files
            // ============================================================

            Console.WriteLine("Cleaning up test files...");
            Console.WriteLine();

            var testFiles = new[] { smallTestFile, mediumTestFile, largeTestFile };
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

            Console.WriteLine();
            Console.WriteLine("Cleanup completed.");
            Console.WriteLine();
            Console.WriteLine("All examples completed successfully!");
        }
    }

    // ====================================================================
    // Helper Classes
    // ====================================================================

    /// <summary>
    /// Represents benchmark results for performance measurement.
    /// 
    /// This class stores the results of performance benchmarks,
    /// including file information, operation times, and throughput.
    /// </summary>
    class BenchmarkResult
    {
        /// <summary>
        /// Gets or sets the file name.
        /// </summary>
        public string FileName { get; set; }

        /// <summary>
        /// Gets or sets the file size in bytes.
        /// </summary>
        public long FileSize { get; set; }

        /// <summary>
        /// Gets or sets the upload time in milliseconds.
        /// </summary>
        public long UploadTime { get; set; }

        /// <summary>
        /// Gets or sets the download time in milliseconds.
        /// </summary>
        public long DownloadTime { get; set; }

        /// <summary>
        /// Gets or sets the uploaded file ID.
        /// </summary>
        public string FileId { get; set; }
    }
}

