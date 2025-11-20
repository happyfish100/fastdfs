// ============================================================================
// FastDFS C# Client - Basic Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates basic FastDFS operations including file upload,
// download, and deletion. It shows how to initialize the client, perform
// simple file operations, and handle errors.
//
// ============================================================================

using System;
using System.IO;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Basic example demonstrating FastDFS file operations.
    /// 
    /// This example shows:
    /// - How to configure and initialize the FastDFS client
    /// - How to upload files to FastDFS storage
    /// - How to download files from FastDFS storage
    /// - How to delete files from FastDFS storage
    /// - How to handle errors and exceptions
    /// </summary>
    class BasicExample
    {
        /// <summary>
        /// Main entry point for the basic example.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Basic Example");
            Console.WriteLine("==================================");
            Console.WriteLine();

            // Step 1: Create client configuration
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            var config = new FastDFSClientConfig
            {
                // Specify tracker server addresses
                // Tracker servers coordinate file storage and retrieval operations
                TrackerAddresses = new[]
                {
                    "192.168.1.100:22122",  // Primary tracker server
                    "192.168.1.101:22122"   // Secondary tracker server (for redundancy)
                },

                // Maximum number of connections per server
                // Higher values allow more concurrent operations but consume more resources
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

            // Step 2: Initialize the FastDFS client
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations.
            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // Step 3: Upload a file
                    // This example uploads a local file to FastDFS storage.
                    // The method returns a file ID that uniquely identifies
                    // the file in the FastDFS cluster.
                    Console.WriteLine("Step 1: Uploading file...");
                    var localFilePath = "test.txt";

                    // Create a test file if it doesn't exist
                    if (!File.Exists(localFilePath))
                    {
                        await File.WriteAllTextAsync(localFilePath, "Hello, FastDFS!");
                        Console.WriteLine($"Created test file: {localFilePath}");
                    }

                    // Upload the file
                    // The second parameter is metadata (null means no metadata)
                    var fileId = await client.UploadFileAsync(localFilePath, null);
                    Console.WriteLine($"File uploaded successfully!");
                    Console.WriteLine($"File ID: {fileId}");
                    Console.WriteLine();

                    // Step 4: Get file information
                    // Retrieve detailed information about the uploaded file,
                    // including size, creation time, CRC32 checksum, etc.
                    Console.WriteLine("Step 2: Getting file information...");
                    var fileInfo = await client.GetFileInfoAsync(fileId);
                    Console.WriteLine($"File Size: {fileInfo.FileSize} bytes");
                    Console.WriteLine($"Create Time: {fileInfo.CreateTime}");
                    Console.WriteLine($"CRC32: {fileInfo.CRC32:X8}");
                    Console.WriteLine($"Source IP: {fileInfo.SourceIPAddr}");
                    Console.WriteLine();

                    // Step 5: Download the file
                    // Download the file content as a byte array.
                    // For large files, consider using DownloadToFileAsync
                    // to stream directly to disk.
                    Console.WriteLine("Step 3: Downloading file...");
                    var downloadedData = await client.DownloadFileAsync(fileId);
                    var downloadedText = System.Text.Encoding.UTF8.GetString(downloadedData);
                    Console.WriteLine($"File downloaded successfully!");
                    Console.WriteLine($"Downloaded content: {downloadedText}");
                    Console.WriteLine($"Downloaded size: {downloadedData.Length} bytes");
                    Console.WriteLine();

                    // Step 6: Download to a local file
                    // This method streams the file directly to disk, which is
                    // more memory-efficient for large files.
                    Console.WriteLine("Step 4: Downloading file to disk...");
                    var downloadPath = "downloaded_test.txt";
                    await client.DownloadToFileAsync(fileId, downloadPath);
                    Console.WriteLine($"File downloaded to: {downloadPath}");
                    Console.WriteLine();

                    // Step 7: Download a partial file range
                    // Download only a specific byte range from the file.
                    // This is useful for large files where you only need
                    // a portion of the data.
                    Console.WriteLine("Step 5: Downloading file range...");
                    var rangeData = await client.DownloadFileRangeAsync(fileId, 0, 5);
                    var rangeText = System.Text.Encoding.UTF8.GetString(rangeData);
                    Console.WriteLine($"Downloaded range (0-5): {rangeText}");
                    Console.WriteLine();

                    // Step 8: Delete the file
                    // Permanently delete the file from FastDFS storage.
                    // This operation cannot be undone.
                    Console.WriteLine("Step 6: Deleting file...");
                    await client.DeleteFileAsync(fileId);
                    Console.WriteLine("File deleted successfully!");
                    Console.WriteLine();

                    // Clean up local files
                    if (File.Exists(localFilePath))
                    {
                        File.Delete(localFilePath);
                    }
                    if (File.Exists(downloadPath))
                    {
                        File.Delete(downloadPath);
                    }

                    Console.WriteLine("Example completed successfully!");
                }
                catch (FastDFSException ex)
                {
                    // Handle FastDFS-specific errors
                    Console.WriteLine($"FastDFS Error: {ex.Message}");
                    if (ex.InnerException != null)
                    {
                        Console.WriteLine($"Inner Exception: {ex.InnerException.Message}");
                    }
                }
                catch (Exception ex)
                {
                    // Handle other errors
                    Console.WriteLine($"Error: {ex.Message}");
                    Console.WriteLine($"Stack Trace: {ex.StackTrace}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}

