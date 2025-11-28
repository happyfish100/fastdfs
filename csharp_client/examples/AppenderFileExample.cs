// ============================================================================
// FastDFS C# Client - Appender File Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates appender file operations in FastDFS, including
// uploading appender files, appending data to existing appender files, and
// best practices for working with appender files in various use cases.
//
// Appender files are special files that support modification operations
// (append, modify, truncate) after initial upload, making them ideal for
// log files, growing datasets, streaming data, and other scenarios where
// files need to be updated incrementally.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating FastDFS appender file operations.
    /// 
    /// This example shows:
    /// - How to upload files as appender files
    /// - How to append data to existing appender files
    /// - Use cases for appender files (log files, growing datasets, streaming data)
    /// - Best practices for appender file operations
    /// - Error handling and validation
    /// 
    /// Appender files are particularly useful for:
    /// 1. Log files: Continuously append log entries without re-uploading
    /// 2. Growing datasets: Incrementally add data to large files
    /// 3. Streaming data: Append data as it becomes available
    /// 4. Time-series data: Append measurements over time
    /// 5. Audit trails: Append audit records sequentially
    /// </summary>
    class AppenderFileExample
    {
        /// <summary>
        /// Main entry point for the appender file example.
        /// 
        /// This method demonstrates various appender file operations through
        /// a series of examples, each showing different aspects of working
        /// with appender files in FastDFS.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Appender File Example");
            Console.WriteLine("==========================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates appender file operations,");
            Console.WriteLine("including upload, append, and best practices.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For appender file operations, we typically want longer network
            // timeouts to accommodate potentially large append operations.
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
                // For appender operations, we may need more connections if
                // performing concurrent appends to multiple files
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                // Standard timeout is usually sufficient for appender operations
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // For large append operations, consider increasing this value
                // to accommodate longer network transfers
                NetworkTimeout = TimeSpan.FromSeconds(60),  // Longer timeout for appends

                // Idle timeout: time before idle connections are closed
                // Appender operations may have longer gaps between operations,
                // so a reasonable idle timeout helps maintain connections
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                // Appender operations should have retry logic to handle transient
                // network errors, especially important for critical log files
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations including appender file operations.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Upload Appender File (Log File Use Case)
                    // ============================================================
                    // 
                    // This example demonstrates uploading a file as an appender
                    // file, which is the first step in working with appender
                    // files. Appender files must be uploaded using the special
                    // UploadAppenderFileAsync method, not the regular upload method.
                    // 
                    // Use case: Log files that need to be continuously appended
                    // ============================================================

                    Console.WriteLine("Example 1: Upload Appender File (Log File Use Case)");
                    Console.WriteLine("====================================================");
                    Console.WriteLine();

                    // Create a sample log file with initial content
                    // In real scenarios, this might be an existing log file
                    // that you want to continue appending to in FastDFS
                    var logFilePath = "application.log";

                    if (!File.Exists(logFilePath))
                    {
                        // Create initial log content
                        // In production, this would be your existing log file
                        var initialLogContent = new StringBuilder();
                        initialLogContent.AppendLine("[2025-01-01 10:00:00] INFO: Application started");
                        initialLogContent.AppendLine("[2025-01-01 10:00:01] INFO: Database connection established");
                        initialLogContent.AppendLine("[2025-01-01 10:00:02] INFO: Server listening on port 8080");

                        await File.WriteAllTextAsync(logFilePath, initialLogContent.ToString());
                        Console.WriteLine($"Created initial log file: {logFilePath}");
                        Console.WriteLine($"Initial log size: {new FileInfo(logFilePath).Length} bytes");
                        Console.WriteLine();
                    }

                    // Define metadata for the log file
                    // Metadata helps identify and categorize appender files
                    // This is especially useful when managing multiple log files
                    var logMetadata = new Dictionary<string, string>
                    {
                        { "type", "application_log" },
                        { "application", "MyApp" },
                        { "environment", "production" },
                        { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") },
                        { "format", "text/plain" }
                    };

                    // Upload the file as an appender file
                    // This is the critical step: using UploadAppenderFileAsync
                    // instead of UploadFileAsync marks the file as an appender
                    // file, enabling subsequent append operations
                    Console.WriteLine("Uploading log file as appender file...");
                    var appenderFileId = await client.UploadAppenderFileAsync(logFilePath, logMetadata);
                    
                    Console.WriteLine($"Appender file uploaded successfully!");
                    Console.WriteLine($"File ID: {appenderFileId}");
                    Console.WriteLine();

                    // Get file information to verify upload
                    // This confirms the file was uploaded correctly and
                    // provides initial size information
                    var initialFileInfo = await client.GetFileInfoAsync(appenderFileId);
                    Console.WriteLine("Initial file information:");
                    Console.WriteLine($"  File Size: {initialFileInfo.FileSize} bytes");
                    Console.WriteLine($"  Create Time: {initialFileInfo.CreateTime}");
                    Console.WriteLine($"  CRC32: {initialFileInfo.CRC32:X8}");
                    Console.WriteLine($"  Source IP: {initialFileInfo.SourceIPAddr}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Append Data to Appender File
                    // ============================================================
                    // 
                    // This example demonstrates appending data to an existing
                    // appender file. This is the core operation that makes
                    // appender files useful for log files and growing datasets.
                    // 
                    // Best practices:
                    // - Append data in reasonable chunks (not too small, not too large)
                    // - Consider batching multiple log entries together
                    // - Handle errors appropriately, especially for critical logs
                    // ============================================================

                    Console.WriteLine("Example 2: Append Data to Appender File");
                    Console.WriteLine("========================================");
                    Console.WriteLine();

                    // Simulate appending log entries over time
                    // In a real application, these would be generated by your
                    // application as events occur
                    var logEntries = new[]
                    {
                        "[2025-01-01 10:05:00] INFO: User login successful (user_id: 12345)",
                        "[2025-01-01 10:05:15] INFO: Request processed (endpoint: /api/users, duration: 45ms)",
                        "[2025-01-01 10:05:30] WARN: High memory usage detected (85%)",
                        "[2025-01-01 10:05:45] INFO: Cache refreshed (entries: 1250)"
                    };

                    Console.WriteLine("Appending log entries to appender file...");
                    Console.WriteLine();

                    // Append each log entry individually
                    // In production, you might batch multiple entries together
                    // for better performance, but individual appends provide
                    // better durability guarantees
                    for (int i = 0; i < logEntries.Length; i++)
                    {
                        // Convert log entry to bytes
                        // Use UTF-8 encoding to ensure proper character handling
                        var logEntryBytes = Encoding.UTF8.GetBytes(logEntries[i] + Environment.NewLine);

                        // Append the log entry to the appender file
                        // This operation adds data to the end of the file
                        // without requiring a full file re-upload
                        await client.AppendFileAsync(appenderFileId, logEntryBytes);

                        Console.WriteLine($"  Appended entry {i + 1}/{logEntries.Length}: {logEntries[i]}");

                        // Get updated file information after each append
                        // This demonstrates how the file size grows with each append
                        var updatedFileInfo = await client.GetFileInfoAsync(appenderFileId);
                        Console.WriteLine($"    Current file size: {updatedFileInfo.FileSize} bytes");
                    }

                    Console.WriteLine();
                    Console.WriteLine("All log entries appended successfully!");
                    Console.WriteLine();

                    // Verify final file size
                    // The file should now be larger than the initial upload
                    var finalFileInfo = await client.GetFileInfoAsync(appenderFileId);
                    Console.WriteLine("Final file information:");
                    Console.WriteLine($"  File Size: {finalFileInfo.FileSize} bytes");
                    Console.WriteLine($"  Size increase: {finalFileInfo.FileSize - initialFileInfo.FileSize} bytes");
                    Console.WriteLine();

                    // Download and display the complete log file
                    // This verifies that all appended data is correctly stored
                    Console.WriteLine("Downloading complete log file to verify content...");
                    var completeLogData = await client.DownloadFileAsync(appenderFileId);
                    var completeLogText = Encoding.UTF8.GetString(completeLogData);
                    
                    Console.WriteLine("Complete log file content:");
                    Console.WriteLine("-------------------------");
                    Console.WriteLine(completeLogText);
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Growing Dataset Use Case
                    // ============================================================
                    // 
                    // This example demonstrates using appender files for
                    // growing datasets, such as time-series data, sensor readings,
                    // or incremental backups.
                    // 
                    // Best practices for growing datasets:
                    // - Use structured data formats (JSON, CSV, etc.)
                    // - Append data in batches for better performance
                    // - Consider data compression for large datasets
                    // - Monitor file size and consider splitting large files
                    // ============================================================

                    Console.WriteLine("Example 3: Growing Dataset Use Case");
                    Console.WriteLine("=====================================");
                    Console.WriteLine();

                    // Create initial dataset file
                    // This represents the initial state of a growing dataset
                    var datasetFilePath = "sensor_data.csv";

                    if (!File.Exists(datasetFilePath))
                    {
                        // Create CSV header and initial data
                        var csvContent = new StringBuilder();
                        csvContent.AppendLine("timestamp,temperature,humidity,pressure");
                        csvContent.AppendLine("2025-01-01 10:00:00,22.5,65.0,1013.25");
                        csvContent.AppendLine("2025-01-01 10:01:00,22.6,64.8,1013.30");

                        await File.WriteAllTextAsync(datasetFilePath, csvContent.ToString());
                        Console.WriteLine($"Created initial dataset file: {datasetFilePath}");
                        Console.WriteLine();
                    }

                    // Upload as appender file with appropriate metadata
                    var datasetMetadata = new Dictionary<string, string>
                    {
                        { "type", "sensor_data" },
                        { "format", "csv" },
                        { "source", "weather_station_01" },
                        { "frequency", "1_minute" }
                    };

                    Console.WriteLine("Uploading dataset as appender file...");
                    var datasetFileId = await client.UploadAppenderFileAsync(datasetFilePath, datasetMetadata);
                    Console.WriteLine($"Dataset file uploaded: {datasetFileId}");
                    Console.WriteLine();

                    // Simulate appending new sensor readings
                    // In a real scenario, these would come from actual sensors
                    // at regular intervals
                    var newReadings = new[]
                    {
                        "2025-01-01 10:02:00,22.7,64.5,1013.28",
                        "2025-01-01 10:03:00,22.8,64.3,1013.32",
                        "2025-01-01 10:04:00,22.9,64.1,1013.35",
                        "2025-01-01 10:05:00,23.0,63.9,1013.38"
                    };

                    Console.WriteLine("Appending new sensor readings...");
                    Console.WriteLine();

                    // Batch append multiple readings together
                    // Batching improves performance by reducing the number of
                    // network round trips, but individual appends provide
                    // better durability
                    var batchData = new StringBuilder();
                    foreach (var reading in newReadings)
                    {
                        batchData.AppendLine(reading);
                    }

                    var batchBytes = Encoding.UTF8.GetBytes(batchData.ToString());
                    await client.AppendFileAsync(datasetFileId, batchBytes);

                    Console.WriteLine($"Appended {newReadings.Length} new readings in batch");
                    Console.WriteLine();

                    // Verify the dataset
                    var datasetInfo = await client.GetFileInfoAsync(datasetFileId);
                    Console.WriteLine("Dataset file information:");
                    Console.WriteLine($"  File Size: {datasetInfo.FileSize} bytes");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Streaming Data Use Case
                    // ============================================================
                    // 
                    // This example demonstrates using appender files for
                    // streaming data scenarios, where data arrives continuously
                    // and needs to be appended as it becomes available.
                    // 
                    // Best practices for streaming data:
                    // - Use buffering to batch small writes
                    // - Implement proper error handling and retry logic
                    // - Consider using async/await for non-blocking operations
                    // - Monitor append performance and adjust batch sizes
                    // ============================================================

                    Console.WriteLine("Example 4: Streaming Data Use Case");
                    Console.WriteLine("===================================");
                    Console.WriteLine();

                    // Create initial streaming data file
                    var streamFilePath = "stream_data.txt";

                    if (!File.Exists(streamFilePath))
                    {
                        await File.WriteAllTextAsync(streamFilePath, "Stream started at " + DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") + Environment.NewLine);
                        Console.WriteLine($"Created initial stream file: {streamFilePath}");
                        Console.WriteLine();
                    }

                    // Upload as appender file
                    var streamMetadata = new Dictionary<string, string>
                    {
                        { "type", "stream_data" },
                        { "stream_id", "stream_001" },
                        { "format", "text" }
                    };

                    Console.WriteLine("Uploading stream file as appender file...");
                    var streamFileId = await client.UploadAppenderFileAsync(streamFilePath, streamMetadata);
                    Console.WriteLine($"Stream file uploaded: {streamFileId}");
                    Console.WriteLine();

                    // Simulate streaming data arrival
                    // In a real scenario, this would be triggered by events
                    // or data arrival from external sources
                    Console.WriteLine("Simulating streaming data arrival...");
                    Console.WriteLine();

                    // Simulate data arriving at different intervals
                    // This demonstrates how appender files can handle
                    // irregular data arrival patterns
                    var streamChunks = new[]
                    {
                        "Chunk 1: Data received at " + DateTime.UtcNow.AddSeconds(1).ToString("HH:mm:ss") + Environment.NewLine,
                        "Chunk 2: Data received at " + DateTime.UtcNow.AddSeconds(2).ToString("HH:mm:ss") + Environment.NewLine,
                        "Chunk 3: Data received at " + DateTime.UtcNow.AddSeconds(3).ToString("HH:mm:ss") + Environment.NewLine
                    };

                    foreach (var chunk in streamChunks)
                    {
                        // Append each chunk as it arrives
                        // In production, you might buffer multiple chunks
                        // before appending to improve performance
                        var chunkBytes = Encoding.UTF8.GetBytes(chunk);
                        await client.AppendFileAsync(streamFileId, chunkBytes);

                        Console.WriteLine($"  Appended: {chunk.Trim()}");
                        
                        // Small delay to simulate real-time streaming
                        await Task.Delay(100);
                    }

                    Console.WriteLine();
                    Console.WriteLine("Streaming data appended successfully!");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for working with
                    // appender files in FastDFS, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for Appender File Operations");
                    Console.WriteLine("===========================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Use appender files for:");
                    Console.WriteLine("   - Log files that need continuous appending");
                    Console.WriteLine("   - Growing datasets (time-series, sensor data)");
                    Console.WriteLine("   - Streaming data that arrives incrementally");
                    Console.WriteLine("   - Audit trails and event logs");
                    Console.WriteLine("   - Any file that needs to grow over time");
                    Console.WriteLine();
                    Console.WriteLine("2. Upload files as appender files from the start:");
                    Console.WriteLine("   - Use UploadAppenderFileAsync, not UploadFileAsync");
                    Console.WriteLine("   - Regular files cannot be converted to appender files");
                    Console.WriteLine("   - Plan ahead if you might need to append later");
                    Console.WriteLine();
                    Console.WriteLine("3. Append operations:");
                    Console.WriteLine("   - Append data in reasonable chunks (not too small/large)");
                    Console.WriteLine("   - Batch multiple small appends for better performance");
                    Console.WriteLine("   - Use individual appends for critical data (better durability)");
                    Console.WriteLine("   - Handle errors appropriately, especially for logs");
                    Console.WriteLine();
                    Console.WriteLine("4. Performance considerations:");
                    Console.WriteLine("   - Increase NetworkTimeout for large append operations");
                    Console.WriteLine("   - Use connection pooling effectively");
                    Console.WriteLine("   - Consider batching for high-frequency appends");
                    Console.WriteLine("   - Monitor file sizes and consider splitting large files");
                    Console.WriteLine();
                    Console.WriteLine("5. Error handling:");
                    Console.WriteLine("   - Implement retry logic for transient failures");
                    Console.WriteLine("   - Log append failures for critical operations");
                    Console.WriteLine("   - Consider local buffering for offline scenarios");
                    Console.WriteLine("   - Validate file IDs before append operations");
                    Console.WriteLine();
                    Console.WriteLine("6. Metadata:");
                    Console.WriteLine("   - Use metadata to identify appender file types");
                    Console.WriteLine("   - Include source, format, and other relevant information");
                    Console.WriteLine("   - Update metadata if file characteristics change");
                    Console.WriteLine();
                    Console.WriteLine("7. Monitoring:");
                    Console.WriteLine("   - Monitor file sizes to prevent unbounded growth");
                    Console.WriteLine("   - Track append operation performance");
                    Console.WriteLine("   - Set up alerts for append failures");
                    Console.WriteLine("   - Consider file rotation for very large files");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up uploaded files and local test files
                    // ============================================================

                    Console.WriteLine("Cleaning up...");
                    Console.WriteLine();

                    // Delete appender files from FastDFS
                    await client.DeleteFileAsync(appenderFileId);
                    Console.WriteLine("Deleted appender log file from FastDFS");

                    await client.DeleteFileAsync(datasetFileId);
                    Console.WriteLine("Deleted dataset file from FastDFS");

                    await client.DeleteFileAsync(streamFileId);
                    Console.WriteLine("Deleted stream file from FastDFS");

                    Console.WriteLine();

                    // Delete local test files
                    if (File.Exists(logFilePath))
                    {
                        File.Delete(logFilePath);
                        Console.WriteLine($"Deleted local file: {logFilePath}");
                    }

                    if (File.Exists(datasetFilePath))
                    {
                        File.Delete(datasetFilePath);
                        Console.WriteLine($"Deleted local file: {datasetFilePath}");
                    }

                    if (File.Exists(streamFilePath))
                    {
                        File.Delete(streamFilePath);
                        Console.WriteLine($"Deleted local file: {streamFilePath}");
                    }

                    Console.WriteLine();
                    Console.WriteLine("Example completed successfully!");
                }
                catch (FastDFSException ex)
                {
                    // Handle FastDFS-specific errors
                    // These might include network errors, server errors,
                    // protocol errors, or file operation errors
                    Console.WriteLine($"FastDFS Error: {ex.Message}");
                    
                    if (ex.InnerException != null)
                    {
                        Console.WriteLine($"Inner Exception: {ex.InnerException.Message}");
                    }

                    Console.WriteLine();
                    Console.WriteLine("Common causes:");
                    Console.WriteLine("  - Network connectivity issues");
                    Console.WriteLine("  - Tracker or storage server unavailable");
                    Console.WriteLine("  - Invalid file ID or file not found");
                    Console.WriteLine("  - File size limits exceeded");
                    Console.WriteLine("  - Storage server configuration issues");
                }
                catch (NotImplementedException ex)
                {
                    // Handle case where appender operations are not yet implemented
                    Console.WriteLine($"Operation not implemented: {ex.Message}");
                    Console.WriteLine();
                    Console.WriteLine("Note: Appender file operations may not be fully");
                    Console.WriteLine("implemented in this version of the client.");
                }
                catch (Exception ex)
                {
                    // Handle other unexpected errors
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

