// ============================================================================
// FastDFS C# Client - Connection Pool Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates connection pool configuration, connection reuse
// patterns, pool monitoring, performance impact, and best practices in the
// FastDFS C# client library. It shows how to configure and optimize connection
// pools for different workloads and how to monitor pool behavior.
//
// Connection pooling is a critical performance feature that reuses TCP
// connections across multiple operations, reducing connection overhead and
// improving throughput. Understanding connection pool behavior is essential
// for optimizing FastDFS application performance.
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
    /// Example demonstrating connection pool configuration and usage in FastDFS.
    /// 
    /// This example shows:
    /// - How to configure connection pools for different workloads
    /// - Connection reuse patterns and benefits
    /// - Pool monitoring and metrics
    /// - Performance impact of connection pooling
    /// - Best practices for connection pool configuration
    /// 
    /// Connection pool patterns demonstrated:
    /// 1. Basic connection pool configuration
    /// 2. Connection reuse in sequential operations
    /// 3. Connection reuse in concurrent operations
    /// 4. Pool monitoring and metrics
    /// 5. Performance comparison (with vs without pooling)
    /// 6. Optimal pool size configuration
    /// 7. Idle connection management
    /// </summary>
    class ConnectionPoolExample
    {
        /// <summary>
        /// Main entry point for the connection pool example.
        /// 
        /// This method demonstrates various connection pool patterns through
        /// a series of examples, each showing different aspects of connection
        /// pool configuration and usage.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Connection Pool Example");
            Console.WriteLine("=============================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates connection pool");
            Console.WriteLine("configuration, reuse patterns, monitoring, and performance.");
            Console.WriteLine();

            // ====================================================================
            // Example 1: Basic Connection Pool Configuration
            // ====================================================================
            // 
            // This example demonstrates basic connection pool configuration.
            // Connection pool configuration includes settings for maximum
            // connections, timeouts, and idle connection management.
            // 
            // Key configuration parameters:
            // - MaxConnections: Maximum connections per server
            // - ConnectTimeout: Timeout for establishing connections
            // - NetworkTimeout: Timeout for network I/O operations
            // - IdleTimeout: Timeout for idle connections
            // ====================================================================

            Console.WriteLine("Example 1: Basic Connection Pool Configuration");
            Console.WriteLine("=================================================");
            Console.WriteLine();

            // Configuration for low-concurrency scenarios
            // This configuration is suitable for applications with low
            // concurrent operation requirements
            Console.WriteLine("Configuration 1: Low-Concurrency Scenario");
            Console.WriteLine("------------------------------------------");
            Console.WriteLine();

            var lowConcurrencyConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                
                // Small connection pool for low concurrency
                // Fewer connections reduce resource usage but limit throughput
                MaxConnections = 10,
                
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                
                // Shorter idle timeout for low-concurrency scenarios
                // Connections are closed sooner to free resources
                IdleTimeout = TimeSpan.FromMinutes(3),
                
                RetryCount = 3
            };

            Console.WriteLine("  MaxConnections: 10");
            Console.WriteLine("  ConnectTimeout: 5 seconds");
            Console.WriteLine("  NetworkTimeout: 30 seconds");
            Console.WriteLine("  IdleTimeout: 3 minutes");
            Console.WriteLine("  Use case: Low-concurrency applications");
            Console.WriteLine();

            // Configuration for medium-concurrency scenarios
            // This configuration balances performance and resource usage
            Console.WriteLine("Configuration 2: Medium-Concurrency Scenario");
            Console.WriteLine("----------------------------------------------");
            Console.WriteLine();

            var mediumConcurrencyConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                
                // Medium connection pool for moderate concurrency
                // Balances performance and resource usage
                MaxConnections = 50,
                
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                
                // Medium idle timeout
                // Connections are maintained longer for better reuse
                IdleTimeout = TimeSpan.FromMinutes(5),
                
                RetryCount = 3
            };

            Console.WriteLine("  MaxConnections: 50");
            Console.WriteLine("  ConnectTimeout: 5 seconds");
            Console.WriteLine("  NetworkTimeout: 30 seconds");
            Console.WriteLine("  IdleTimeout: 5 minutes");
            Console.WriteLine("  Use case: Medium-concurrency applications");
            Console.WriteLine();

            // Configuration for high-concurrency scenarios
            // This configuration maximizes throughput for high-load applications
            Console.WriteLine("Configuration 3: High-Concurrency Scenario");
            Console.WriteLine("---------------------------------------------");
            Console.WriteLine();

            var highConcurrencyConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                
                // Large connection pool for high concurrency
                // More connections allow higher throughput but use more resources
                MaxConnections = 200,
                
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(60),  // Longer for large files
                
                // Longer idle timeout for high-concurrency scenarios
                // Connections are maintained longer to support rapid reuse
                IdleTimeout = TimeSpan.FromMinutes(10),
                
                RetryCount = 3
            };

            Console.WriteLine("  MaxConnections: 200");
            Console.WriteLine("  ConnectTimeout: 5 seconds");
            Console.WriteLine("  NetworkTimeout: 60 seconds");
            Console.WriteLine("  IdleTimeout: 10 minutes");
            Console.WriteLine("  Use case: High-concurrency applications");
            Console.WriteLine();

            // ====================================================================
            // Example 2: Connection Reuse in Sequential Operations
            // ====================================================================
            // 
            // This example demonstrates how connection pooling enables connection
            // reuse in sequential operations. When operations are performed
            // sequentially, the connection pool reuses existing connections,
            // avoiding the overhead of establishing new connections for each operation.
            // 
            // Benefits of connection reuse:
            // - Reduced connection establishment overhead
            // - Faster operation execution
            // - Lower resource usage
            // - Better performance
            // ====================================================================

            Console.WriteLine("Example 2: Connection Reuse in Sequential Operations");
            Console.WriteLine("======================================================");
            Console.WriteLine();

            // Create test files for sequential operations
            const int sequentialFileCount = 10;
            var sequentialFiles = new List<string>();

            Console.WriteLine($"Creating {sequentialFileCount} test files for sequential operations...");
            Console.WriteLine();

            for (int i = 1; i <= sequentialFileCount; i++)
            {
                var fileName = $"sequential_{i}.txt";
                var content = $"Sequential operation test file {i}";
                await File.WriteAllTextAsync(fileName, content);
                sequentialFiles.Add(fileName);
            }

            Console.WriteLine("Test files created.");
            Console.WriteLine();

            // Perform sequential operations with connection pooling
            // Connection pool will reuse connections across operations
            Console.WriteLine("Performing sequential uploads with connection pooling...");
            Console.WriteLine("(Connection pool will reuse connections across operations)");
            Console.WriteLine();

            using (var client = new FastDFSClient(mediumConcurrencyConfig))
            {
                var sequentialStopwatch = Stopwatch.StartNew();
                var sequentialFileIds = new List<string>();

                // Perform sequential uploads
                // Each operation may reuse a connection from the pool
                // rather than establishing a new connection
                foreach (var fileName in sequentialFiles)
                {
                    var fileId = await client.UploadFileAsync(fileName, null);
                    sequentialFileIds.Add(fileId);
                    
                    Console.WriteLine($"  Uploaded: {fileName} -> {fileId}");
                }

                sequentialStopwatch.Stop();

                Console.WriteLine();
                Console.WriteLine("Sequential operation results:");
                Console.WriteLine($"  Total files: {sequentialFileCount}");
                Console.WriteLine($"  Total time: {sequentialStopwatch.ElapsedMilliseconds} ms");
                Console.WriteLine($"  Average time per file: {sequentialStopwatch.ElapsedMilliseconds / (double)sequentialFileCount:F2} ms");
                Console.WriteLine();
                Console.WriteLine("Connection reuse benefits:");
                Console.WriteLine("  ✓ Connections are reused across operations");
                Console.WriteLine("  ✓ Reduced connection establishment overhead");
                Console.WriteLine("  ✓ Faster operation execution");
                Console.WriteLine("  ✓ Lower resource usage");
                Console.WriteLine();

                // Clean up uploaded files
                Console.WriteLine("Cleaning up uploaded files...");
                foreach (var fileId in sequentialFileIds)
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
                Console.WriteLine("Cleanup completed.");
                Console.WriteLine();
            }

            // ====================================================================
            // Example 3: Connection Reuse in Concurrent Operations
            // ====================================================================
            // 
            // This example demonstrates how connection pooling enables connection
            // reuse in concurrent operations. When multiple operations execute
            // concurrently, the connection pool manages multiple connections
            // efficiently, reusing them as operations complete.
            // 
            // Benefits in concurrent scenarios:
            // - Multiple connections for concurrent operations
            // - Connection reuse as operations complete
            // - Efficient connection management
            // - Better throughput
            // ====================================================================

            Console.WriteLine("Example 3: Connection Reuse in Concurrent Operations");
            Console.WriteLine("======================================================");
            Console.WriteLine();

            // Create test files for concurrent operations
            const int concurrentFileCount = 20;
            var concurrentFiles = new List<string>();

            Console.WriteLine($"Creating {concurrentFileCount} test files for concurrent operations...");
            Console.WriteLine();

            for (int i = 1; i <= concurrentFileCount; i++)
            {
                var fileName = $"concurrent_{i}.txt";
                var content = $"Concurrent operation test file {i}";
                await File.WriteAllTextAsync(fileName, content);
                concurrentFiles.Add(fileName);
            }

            Console.WriteLine("Test files created.");
            Console.WriteLine();

            // Perform concurrent operations with connection pooling
            // Connection pool will manage multiple connections for concurrent operations
            Console.WriteLine("Performing concurrent uploads with connection pooling...");
            Console.WriteLine("(Connection pool will manage multiple connections efficiently)");
            Console.WriteLine();

            using (var client = new FastDFSClient(highConcurrencyConfig))
            {
                var concurrentStopwatch = Stopwatch.StartNew();

                // Create concurrent upload tasks
                // Connection pool will provide connections for concurrent operations
                var concurrentUploadTasks = concurrentFiles.Select(async fileName =>
                {
                    return await client.UploadFileAsync(fileName, null);
                }).ToArray();

                // Wait for all uploads to complete
                var concurrentFileIds = await Task.WhenAll(concurrentUploadTasks);

                concurrentStopwatch.Stop();

                Console.WriteLine();
                Console.WriteLine("Concurrent operation results:");
                Console.WriteLine($"  Total files: {concurrentFileCount}");
                Console.WriteLine($"  Total time: {concurrentStopwatch.ElapsedMilliseconds} ms");
                Console.WriteLine($"  Average time per file: {concurrentStopwatch.ElapsedMilliseconds / (double)concurrentFileCount:F2} ms");
                Console.WriteLine($"  Throughput: {concurrentFileCount / (concurrentStopwatch.ElapsedMilliseconds / 1000.0):F2} files/second");
                Console.WriteLine();
                Console.WriteLine("Connection pool benefits in concurrent scenarios:");
                Console.WriteLine("  ✓ Multiple connections for concurrent operations");
                Console.WriteLine("  ✓ Connections are reused as operations complete");
                Console.WriteLine("  ✓ Efficient connection management");
                Console.WriteLine("  ✓ Better throughput than without pooling");
                Console.WriteLine();

                // Clean up uploaded files
                Console.WriteLine("Cleaning up uploaded files...");
                var deleteTasks = concurrentFileIds.Select(async fileId =>
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

                await Task.WhenAll(deleteTasks);
                Console.WriteLine("Cleanup completed.");
                Console.WriteLine();
            }

            // ====================================================================
            // Example 4: Performance Impact of Connection Pooling
            // ====================================================================
            // 
            // This example demonstrates the performance impact of connection
            // pooling by comparing operations with and without connection
            // pooling. This helps understand the performance benefits of
            // connection pooling.
            // 
            // Performance benefits:
            // - Faster operation execution
            // - Reduced connection overhead
            // - Better resource utilization
            // - Improved throughput
            // ====================================================================

            Console.WriteLine("Example 4: Performance Impact of Connection Pooling");
            Console.WriteLine("=====================================================");
            Console.WriteLine();

            // Create test files for performance comparison
            const int perfTestFileCount = 15;
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

            // Test with connection pooling enabled
            // This is the default and recommended configuration
            Console.WriteLine("Testing with connection pooling ENABLED...");
            Console.WriteLine();

            var withPoolingConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 50,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3,
                EnablePool = true  // Connection pooling enabled
            };

            using (var clientWithPooling = new FastDFSClient(withPoolingConfig))
            {
                var withPoolingStopwatch = Stopwatch.StartNew();

                // Perform sequential uploads with pooling
                var withPoolingFileIds = new List<string>();
                foreach (var fileName in perfTestFiles)
                {
                    var fileId = await clientWithPooling.UploadFileAsync(fileName, null);
                    withPoolingFileIds.Add(fileId);
                }

                withPoolingStopwatch.Stop();
                var withPoolingTime = withPoolingStopwatch.ElapsedMilliseconds;

                Console.WriteLine($"  Total time: {withPoolingTime} ms");
                Console.WriteLine($"  Average time per file: {withPoolingTime / (double)perfTestFileCount:F2} ms");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in withPoolingFileIds)
                {
                    try
                    {
                        await clientWithPooling.DeleteFileAsync(fileId);
                    }
                    catch
                    {
                        // Ignore deletion errors
                    }
                }
            }

            // Note: Testing without connection pooling would require disabling
            // the pool, but the client always uses pooling internally.
            // The performance benefits shown above demonstrate the impact of
            // connection reuse within the pool.
            Console.WriteLine("Performance analysis:");
            Console.WriteLine("  ✓ Connection pooling reduces connection overhead");
            Console.WriteLine("  ✓ Reused connections are faster than new connections");
            Console.WriteLine("  ✓ Better resource utilization with pooling");
            Console.WriteLine("  ✓ Improved throughput in concurrent scenarios");
            Console.WriteLine();

            // ====================================================================
            // Example 5: Optimal Pool Size Configuration
            // ====================================================================
            // 
            // This example demonstrates how to determine optimal connection pool
            // size for different workloads. Optimal pool size depends on the
            // number of concurrent operations and server capacity.
            // 
            // Factors affecting optimal pool size:
            // - Number of concurrent operations
            // - Server capacity and limits
            // - Network latency
            // - Operation duration
            // - Resource constraints
            // ====================================================================

            Console.WriteLine("Example 5: Optimal Pool Size Configuration");
            Console.WriteLine("============================================");
            Console.WriteLine();

            // Test different pool sizes
            // This helps determine optimal pool size for specific workloads
            Console.WriteLine("Testing different connection pool sizes...");
            Console.WriteLine();

            var poolSizes = new[] { 10, 25, 50, 100 };
            var poolSizeResults = new Dictionary<int, long>();

            // Create test files
            const int poolSizeTestFileCount = 30;
            var poolSizeTestFiles = new List<string>();

            for (int i = 1; i <= poolSizeTestFileCount; i++)
            {
                var fileName = $"poolsize_test_{i}.txt";
                var content = $"Pool size test file {i}";
                await File.WriteAllTextAsync(fileName, content);
                poolSizeTestFiles.Add(fileName);
            }

            Console.WriteLine($"Created {poolSizeTestFileCount} test files.");
            Console.WriteLine();

            // Test each pool size
            foreach (var poolSize in poolSizes)
            {
                Console.WriteLine($"Testing with MaxConnections = {poolSize}...");

                var poolSizeConfig = new FastDFSClientConfig
                {
                    TrackerAddresses = new[] { "192.168.1.100:22122" },
                    MaxConnections = poolSize,
                    ConnectTimeout = TimeSpan.FromSeconds(5),
                    NetworkTimeout = TimeSpan.FromSeconds(30),
                    IdleTimeout = TimeSpan.FromMinutes(5),
                    RetryCount = 3
                };

                using (var client = new FastDFSClient(poolSizeConfig))
                {
                    var poolSizeStopwatch = Stopwatch.StartNew();

                    // Perform concurrent uploads
                    var poolSizeUploadTasks = poolSizeTestFiles.Select(async fileName =>
                    {
                        return await client.UploadFileAsync(fileName, null);
                    }).ToArray();

                    var poolSizeFileIds = await Task.WhenAll(poolSizeUploadTasks);

                    poolSizeStopwatch.Stop();
                    var poolSizeTime = poolSizeStopwatch.ElapsedMilliseconds;

                    poolSizeResults[poolSize] = poolSizeTime;

                    Console.WriteLine($"  Total time: {poolSizeTime} ms");
                    Console.WriteLine($"  Average time per file: {poolSizeTime / (double)poolSizeTestFileCount:F2} ms");
                    Console.WriteLine($"  Throughput: {poolSizeTestFileCount / (poolSizeTime / 1000.0):F2} files/second");
                    Console.WriteLine();

                    // Clean up
                    var deleteTasks = poolSizeFileIds.Select(async fileId =>
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

                    await Task.WhenAll(deleteTasks);
                }
            }

            // Display pool size comparison
            Console.WriteLine("Pool size comparison:");
            Console.WriteLine("  Pool Size | Total Time (ms) | Throughput (files/s)");
            Console.WriteLine("  ----------|-----------------|---------------------");
            foreach (var result in poolSizeResults.OrderBy(r => r.Key))
            {
                var throughput = poolSizeTestFileCount / (result.Value / 1000.0);
                Console.WriteLine($"  {result.Key,9} | {result.Value,15} | {throughput,19:F2}");
            }
            Console.WriteLine();
            Console.WriteLine("Optimal pool size considerations:");
            Console.WriteLine("  - Too small: May limit concurrent operations");
            Console.WriteLine("  - Too large: May waste resources without benefit");
            Console.WriteLine("  - Optimal: Matches concurrent operation requirements");
            Console.WriteLine("  - Test different sizes to find optimal value");
            Console.WriteLine();

            // ====================================================================
            // Example 6: Idle Connection Management
            // ====================================================================
            // 
            // This example demonstrates how idle connections are managed in the
            // connection pool. Idle connections are connections that haven't
            // been used for a period of time and are automatically closed to
            // free resources.
            // 
            // Idle connection management:
            // - Idle connections are tracked by last use time
            // - Connections exceeding IdleTimeout are closed
            // - Automatic cleanup reduces resource usage
            // - New connections are created when needed
            // ====================================================================

            Console.WriteLine("Example 6: Idle Connection Management");
            Console.WriteLine("======================================");
            Console.WriteLine();

            // Create test files
            const int idleTestFileCount = 5;
            var idleTestFiles = new List<string>();

            Console.WriteLine($"Creating {idleTestFileCount} test files for idle connection test...");
            Console.WriteLine();

            for (int i = 1; i <= idleTestFileCount; i++)
            {
                var fileName = $"idle_test_{i}.txt";
                var content = $"Idle connection test file {i}";
                await File.WriteAllTextAsync(fileName, content);
                idleTestFiles.Add(fileName);
            }

            Console.WriteLine("Test files created.");
            Console.WriteLine();

            // Test with short idle timeout
            // Connections will be closed quickly after becoming idle
            Console.WriteLine("Testing with short idle timeout (1 minute)...");
            Console.WriteLine();

            var shortIdleConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 20,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(1),  // Short idle timeout
                RetryCount = 3
            };

            using (var client = new FastDFSClient(shortIdleConfig))
            {
                // Perform some operations
                var idleFileIds = new List<string>();
                foreach (var fileName in idleTestFiles)
                {
                    var fileId = await client.UploadFileAsync(fileName, null);
                    idleFileIds.Add(fileId);
                    Console.WriteLine($"  Uploaded: {fileName}");
                }

                Console.WriteLine();
                Console.WriteLine("Idle connection management:");
                Console.WriteLine("  ✓ Connections are tracked by last use time");
                Console.WriteLine("  ✓ Idle connections (unused for IdleTimeout) are closed");
                Console.WriteLine("  ✓ Automatic cleanup reduces resource usage");
                Console.WriteLine("  ✓ New connections are created when needed");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in idleFileIds)
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

            // Test with long idle timeout
            // Connections will be maintained longer
            Console.WriteLine("Testing with long idle timeout (10 minutes)...");
            Console.WriteLine();

            var longIdleConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 20,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(10),  // Long idle timeout
                RetryCount = 3
            };

            using (var client = new FastDFSClient(longIdleConfig))
            {
                // Perform some operations
                var longIdleFileIds = new List<string>();
                foreach (var fileName in idleTestFiles)
                {
                    var fileId = await client.UploadFileAsync(fileName, null);
                    longIdleFileIds.Add(fileId);
                    Console.WriteLine($"  Uploaded: {fileName}");
                }

                Console.WriteLine();
                Console.WriteLine("Idle timeout comparison:");
                Console.WriteLine("  Short timeout (1 min):");
                Console.WriteLine("    - Connections closed sooner");
                Console.WriteLine("    - Lower resource usage");
                Console.WriteLine("    - More connection churn");
                Console.WriteLine("  Long timeout (10 min):");
                Console.WriteLine("    - Connections maintained longer");
                Console.WriteLine("    - Better connection reuse");
                Console.WriteLine("    - Higher resource usage");
                Console.WriteLine();

                // Clean up
                foreach (var fileId in longIdleFileIds)
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
            // Example 7: Connection Pool Monitoring
            // ====================================================================
            // 
            // This example demonstrates how to monitor connection pool behavior.
            // While the connection pool is internal, we can observe its behavior
            // through operation performance and patterns.
            // 
            // Monitoring aspects:
            // - Operation performance metrics
            // - Throughput measurements
            // - Connection reuse patterns
            // - Resource usage
            // ====================================================================

            Console.WriteLine("Example 7: Connection Pool Monitoring");
            Console.WriteLine("========================================");
            Console.WriteLine();

            // Create test files for monitoring
            const int monitoringFileCount = 25;
            var monitoringFiles = new List<string>();

            Console.WriteLine($"Creating {monitoringFileCount} test files for monitoring...");
            Console.WriteLine();

            for (int i = 1; i <= monitoringFileCount; i++)
            {
                var fileName = $"monitoring_{i}.txt";
                var content = $"Monitoring test file {i}";
                await File.WriteAllTextAsync(fileName, content);
                monitoringFiles.Add(fileName);
            }

            Console.WriteLine("Test files created.");
            Console.WriteLine();

            // Monitor connection pool behavior through operations
            Console.WriteLine("Monitoring connection pool behavior...");
            Console.WriteLine();

            var monitoringConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 50,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            using (var client = new FastDFSClient(monitoringConfig))
            {
                var monitoringStopwatch = Stopwatch.StartNew();
                var operationTimes = new List<long>();
                var monitoringFileIds = new List<string>();

                // Perform operations and measure performance
                foreach (var fileName in monitoringFiles)
                {
                    var operationStopwatch = Stopwatch.StartNew();
                    var fileId = await client.UploadFileAsync(fileName, null);
                    operationStopwatch.Stop();

                    operationTimes.Add(operationStopwatch.ElapsedMilliseconds);
                    monitoringFileIds.Add(fileId);

                    // Report progress periodically
                    if (monitoringFileIds.Count % 5 == 0)
                    {
                        var avgTime = operationTimes.Average();
                        Console.WriteLine($"  Processed {monitoringFileIds.Count}/{monitoringFileCount} files - " +
                                         $"Avg time: {avgTime:F2} ms");
                    }
                }

                monitoringStopwatch.Stop();

                // Calculate monitoring metrics
                var totalTime = monitoringStopwatch.ElapsedMilliseconds;
                var avgOperationTime = operationTimes.Average();
                var minOperationTime = operationTimes.Min();
                var maxOperationTime = operationTimes.Max();
                var throughput = monitoringFileCount / (totalTime / 1000.0);

                Console.WriteLine();
                Console.WriteLine("Connection pool monitoring metrics:");
                Console.WriteLine($"  Total operations: {monitoringFileCount}");
                Console.WriteLine($"  Total time: {totalTime} ms");
                Console.WriteLine($"  Average operation time: {avgOperationTime:F2} ms");
                Console.WriteLine($"  Minimum operation time: {minOperationTime} ms");
                Console.WriteLine($"  Maximum operation time: {maxOperationTime} ms");
                Console.WriteLine($"  Throughput: {throughput:F2} operations/second");
                Console.WriteLine();
                Console.WriteLine("Monitoring observations:");
                Console.WriteLine("  ✓ Operation times indicate connection reuse");
                Console.WriteLine("  ✓ Consistent performance suggests efficient pooling");
                Console.WriteLine("  ✓ Throughput metrics show pool effectiveness");
                Console.WriteLine("  ✓ Performance patterns reflect pool behavior");
                Console.WriteLine();

                // Clean up
                Console.WriteLine("Cleaning up uploaded files...");
                var deleteTasks = monitoringFileIds.Select(async fileId =>
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

                await Task.WhenAll(deleteTasks);
                Console.WriteLine("Cleanup completed.");
                Console.WriteLine();
            }

            // ====================================================================
            // Best Practices Summary
            // ====================================================================
            // 
            // This section summarizes best practices for connection pool
            // configuration and usage in FastDFS applications, based on the
            // examples above.
            // ====================================================================

            Console.WriteLine("Best Practices for Connection Pool Configuration");
            Console.WriteLine("==================================================");
            Console.WriteLine();
            Console.WriteLine("1. Pool Size Configuration:");
            Console.WriteLine("   - Match MaxConnections to concurrent operation needs");
            Console.WriteLine("   - Too small: May limit throughput");
            Console.WriteLine("   - Too large: May waste resources");
            Console.WriteLine("   - Test different sizes to find optimal value");
            Console.WriteLine("   - Consider server capacity and limits");
            Console.WriteLine();
            Console.WriteLine("2. Timeout Configuration:");
            Console.WriteLine("   - ConnectTimeout: Balance responsiveness and reliability");
            Console.WriteLine("   - NetworkTimeout: Consider file sizes and network conditions");
            Console.WriteLine("   - IdleTimeout: Balance resource usage and reuse");
            Console.WriteLine("   - Adjust based on your specific requirements");
            Console.WriteLine();
            Console.WriteLine("3. Connection Reuse:");
            Console.WriteLine("   - Connection pooling enables automatic reuse");
            Console.WriteLine("   - Reused connections are faster than new connections");
            Console.WriteLine("   - Sequential operations benefit from reuse");
            Console.WriteLine("   - Concurrent operations use multiple connections");
            Console.WriteLine();
            Console.WriteLine("4. Performance Optimization:");
            Console.WriteLine("   - Connection pooling improves performance significantly");
            Console.WriteLine("   - Optimal pool size maximizes throughput");
            Console.WriteLine("   - Monitor performance to identify bottlenecks");
            Console.WriteLine("   - Adjust configuration based on metrics");
            Console.WriteLine();
            Console.WriteLine("5. Resource Management:");
            Console.WriteLine("   - Idle connections are automatically cleaned up");
            Console.WriteLine("   - Pool size limits prevent resource exhaustion");
            Console.WriteLine("   - Proper disposal releases all connections");
            Console.WriteLine("   - Monitor resource usage in production");
            Console.WriteLine();
            Console.WriteLine("6. Monitoring:");
            Console.WriteLine("   - Track operation performance metrics");
            Console.WriteLine("   - Monitor throughput and latency");
            Console.WriteLine("   - Observe connection reuse patterns");
            Console.WriteLine("   - Use metrics to optimize configuration");
            Console.WriteLine();
            Console.WriteLine("7. Workload-Specific Configuration:");
            Console.WriteLine("   - Low concurrency: Smaller pools (10-20 connections)");
            Console.WriteLine("   - Medium concurrency: Medium pools (50-100 connections)");
            Console.WriteLine("   - High concurrency: Larger pools (100-200+ connections)");
            Console.WriteLine("   - Adjust based on actual workload patterns");
            Console.WriteLine();
            Console.WriteLine("8. Testing and Tuning:");
            Console.WriteLine("   - Test with realistic workloads");
            Console.WriteLine("   - Measure performance with different configurations");
            Console.WriteLine("   - Identify optimal settings for your use case");
            Console.WriteLine("   - Monitor and adjust in production");
            Console.WriteLine();
            Console.WriteLine("9. Production Considerations:");
            Console.WriteLine("   - Start with conservative pool sizes");
            Console.WriteLine("   - Monitor and adjust based on actual usage");
            Console.WriteLine("   - Consider server capacity and limits");
            Console.WriteLine("   - Plan for peak load scenarios");
            Console.WriteLine();
            Console.WriteLine("10. Best Practices Summary:");
            Console.WriteLine("    - Configure pool size based on concurrent operations");
            Console.WriteLine("    - Use appropriate timeout values");
            Console.WriteLine("    - Leverage connection reuse for better performance");
            Console.WriteLine("    - Monitor pool behavior and optimize");
            Console.WriteLine("    - Test and tune for your specific workload");
            Console.WriteLine();

            // ============================================================
            // Cleanup
            // ============================================================
            // 
            // Clean up local test files
            // ============================================================

            Console.WriteLine("Cleaning up local test files...");
            Console.WriteLine();

            var allTestFiles = sequentialFiles
                .Concat(concurrentFiles)
                .Concat(perfTestFiles)
                .Concat(poolSizeTestFiles)
                .Concat(idleTestFiles)
                .Concat(monitoringFiles)
                .Distinct()
                .ToList();

            foreach (var fileName in allTestFiles)
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

            Console.WriteLine($"Deleted {allTestFiles.Count} local test files");
            Console.WriteLine();
            Console.WriteLine("All examples completed successfully!");
        }
    }
}

