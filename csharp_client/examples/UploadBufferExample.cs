// ============================================================================
// FastDFS C# Client - Upload Buffer Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates uploading data from memory buffers (byte arrays)
// to FastDFS storage. It shows how to upload generated content such as strings,
// JSON, XML, and other in-memory data without writing to disk first.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating FastDFS buffer upload operations.
    /// 
    /// This example shows:
    /// - How to upload data from memory buffers (byte arrays)
    /// - How to upload generated content (strings, JSON, XML)
    /// - How to compare buffer upload vs file upload
    /// - Use cases: API responses, generated content, in-memory data
    /// - How to handle different data types and formats
    /// </summary>
    class UploadBufferExample
    {
        /// <summary>
        /// Main entry point for the upload buffer example.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Upload Buffer Example");
            Console.WriteLine("===========================================");
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
                    // ============================================================
                    // Example 1: Upload a simple string
                    // ============================================================
                    Console.WriteLine("Example 1: Upload a simple string");
                    Console.WriteLine("-----------------------------------");

                    string textContent = "Hello, FastDFS! This is a test string uploaded from memory.";
                    byte[] textBytes = Encoding.UTF8.GetBytes(textContent);

                    // Upload the string as a text file
                    // The second parameter is the file extension (without dot)
                    // The third parameter is optional metadata
                    var textFileId = await client.UploadBufferAsync(textBytes, "txt", null);
                    Console.WriteLine($"✓ String uploaded successfully!");
                    Console.WriteLine($"  File ID: {textFileId}");
                    Console.WriteLine($"  Content length: {textBytes.Length} bytes");
                    Console.WriteLine();

                    // Verify by downloading
                    var downloadedText = await client.DownloadFileAsync(textFileId);
                    var downloadedString = Encoding.UTF8.GetString(downloadedText);
                    Console.WriteLine($"  Downloaded content: {downloadedString}");
                    Console.WriteLine($"  Content matches: {textContent == downloadedString}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Upload JSON data
                    // ============================================================
                    Console.WriteLine("Example 2: Upload JSON data");
                    Console.WriteLine("----------------------------");

                    // Create a sample JSON object
                    var jsonObject = new
                    {
                        id = 12345,
                        name = "FastDFS Example",
                        timestamp = DateTime.UtcNow,
                        tags = new[] { "storage", "distributed", "csharp" },
                        metadata = new Dictionary<string, string>
                        {
                            { "version", "1.0" },
                            { "author", "FastDFS Team" }
                        }
                    };

                    // Serialize to JSON string
                    string jsonString = JsonSerializer.Serialize(jsonObject, new JsonSerializerOptions
                    {
                        WriteIndented = true
                    });
                    byte[] jsonBytes = Encoding.UTF8.GetBytes(jsonString);

                    // Upload JSON with metadata
                    var jsonMetadata = new Dictionary<string, string>
                    {
                        { "content-type", "application/json" },
                        { "source", "api-response" },
                        { "created-by", "UploadBufferExample" }
                    };

                    var jsonFileId = await client.UploadBufferAsync(jsonBytes, "json", jsonMetadata);
                    Console.WriteLine($"✓ JSON uploaded successfully!");
                    Console.WriteLine($"  File ID: {jsonFileId}");
                    Console.WriteLine($"  JSON size: {jsonBytes.Length} bytes");
                    Console.WriteLine($"  JSON preview: {jsonString.Substring(0, Math.Min(100, jsonString.Length))}...");
                    Console.WriteLine();

                    // Verify metadata was set correctly
                    var retrievedMetadata = await client.GetMetadataAsync(jsonFileId);
                    Console.WriteLine("  Metadata:");
                    foreach (var kvp in retrievedMetadata)
                    {
                        Console.WriteLine($"    {kvp.Key}: {kvp.Value}");
                    }
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Upload XML data
                    // ============================================================
                    Console.WriteLine("Example 3: Upload XML data");
                    Console.WriteLine("--------------------------");

                    // Create a sample XML document
                    string xmlContent = @"<?xml version=""1.0"" encoding=""UTF-8""?>
<document>
    <title>FastDFS Upload Example</title>
    <author>FastDFS Team</author>
    <date>" + DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ") + @"</date>
    <content>
        <paragraph>This is an example XML document uploaded from memory.</paragraph>
        <paragraph>It demonstrates how to upload structured data without writing to disk first.</paragraph>
    </content>
    <metadata>
        <version>1.0</version>
        <format>XML</format>
    </metadata>
</document>";

                    byte[] xmlBytes = Encoding.UTF8.GetBytes(xmlContent);

                    var xmlFileId = await client.UploadBufferAsync(xmlBytes, "xml", null);
                    Console.WriteLine($"✓ XML uploaded successfully!");
                    Console.WriteLine($"  File ID: {xmlFileId}");
                    Console.WriteLine($"  XML size: {xmlBytes.Length} bytes");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Upload binary data (image simulation)
                    // ============================================================
                    Console.WriteLine("Example 4: Upload binary data");
                    Console.WriteLine("-------------------------------");

                    // Simulate binary data (e.g., image, PDF, etc.)
                    // In a real scenario, this might come from:
                    // - Image processing libraries
                    // - PDF generators
                    // - Encrypted data
                    // - Compressed archives
                    byte[] binaryData = new byte[1024];
                    new Random().NextBytes(binaryData); // Fill with random data

                    var binaryFileId = await client.UploadBufferAsync(binaryData, "bin", null);
                    Console.WriteLine($"✓ Binary data uploaded successfully!");
                    Console.WriteLine($"  File ID: {binaryFileId}");
                    Console.WriteLine($"  Binary size: {binaryData.Length} bytes");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Compare Buffer Upload vs File Upload
                    // ============================================================
                    Console.WriteLine("Example 5: Compare Buffer vs File Upload");
                    Console.WriteLine("-------------------------------------------");

                    string comparisonText = "This is a comparison between buffer and file upload methods.";

                    // Method 1: Upload from buffer (no disk I/O)
                    Console.WriteLine("Method 1: Upload from buffer (no disk I/O)");
                    var startTime = DateTime.UtcNow;
                    byte[] bufferData = Encoding.UTF8.GetBytes(comparisonText);
                    var bufferFileId = await client.UploadBufferAsync(bufferData, "txt", null);
                    var bufferTime = DateTime.UtcNow - startTime;
                    Console.WriteLine($"  ✓ Uploaded in {bufferTime.TotalMilliseconds:F2} ms");
                    Console.WriteLine($"  File ID: {bufferFileId}");
                    Console.WriteLine();

                    // Method 2: Upload from file (requires disk I/O)
                    Console.WriteLine("Method 2: Upload from file (requires disk I/O)");
                    var tempFile = Path.GetTempFileName();
                    try
                    {
                        await File.WriteAllTextAsync(tempFile, comparisonText);
                        startTime = DateTime.UtcNow;
                        var fileFileId = await client.UploadFileAsync(tempFile, null);
                        var fileTime = DateTime.UtcNow - startTime;
                        Console.WriteLine($"  ✓ Uploaded in {fileTime.TotalMilliseconds:F2} ms");
                        Console.WriteLine($"  File ID: {fileFileId}");
                        Console.WriteLine();

                        // Compare performance
                        Console.WriteLine("Performance Comparison:");
                        Console.WriteLine($"  Buffer upload: {bufferTime.TotalMilliseconds:F2} ms");
                        Console.WriteLine($"  File upload:   {fileTime.TotalMilliseconds:F2} ms");
                        Console.WriteLine($"  Difference:    {Math.Abs((bufferTime - fileTime).TotalMilliseconds):F2} ms");
                        Console.WriteLine($"  Buffer is {(bufferTime < fileTime ? "faster" : "slower")}");
                        Console.WriteLine();

                        // Clean up
                        await client.DeleteFileAsync(fileFileId);
                    }
                    finally
                    {
                        if (File.Exists(tempFile))
                        {
                            File.Delete(tempFile);
                        }
                    }

                    // ============================================================
                    // Example 6: Upload with different file extensions
                    // ============================================================
                    Console.WriteLine("Example 6: Upload with different file extensions");
                    Console.WriteLine("--------------------------------------------------");

                    var extensions = new[] { "txt", "json", "xml", "csv", "log", "dat" };
                    var uploadedFiles = new List<string>();

                    foreach (var ext in extensions)
                    {
                        string content = $"Sample content for .{ext} file";
                        byte[] data = Encoding.UTF8.GetBytes(content);
                        var fileId = await client.UploadBufferAsync(data, ext, null);
                        uploadedFiles.Add(fileId);
                        Console.WriteLine($"  ✓ Uploaded .{ext} file: {fileId}");
                    }
                    Console.WriteLine();

                    // ============================================================
                    // Example 7: Upload large buffer (chunked data simulation)
                    // ============================================================
                    Console.WriteLine("Example 7: Upload large buffer");
                    Console.WriteLine("-------------------------------");

                    // Simulate uploading a larger dataset (e.g., from database, API, etc.)
                    var largeData = new StringBuilder();
                    for (int i = 0; i < 1000; i++)
                    {
                        largeData.AppendLine($"Line {i}: This is line number {i} in a large dataset.");
                    }
                    byte[] largeBytes = Encoding.UTF8.GetBytes(largeData.ToString());

                    startTime = DateTime.UtcNow;
                    var largeFileId = await client.UploadBufferAsync(largeBytes, "txt", null);
                    var largeTime = DateTime.UtcNow - startTime;

                    Console.WriteLine($"✓ Large buffer uploaded successfully!");
                    Console.WriteLine($"  File ID: {largeFileId}");
                    Console.WriteLine($"  Size: {largeBytes.Length:N0} bytes ({largeBytes.Length / 1024.0:F2} KB)");
                    Console.WriteLine($"  Upload time: {largeTime.TotalMilliseconds:F2} ms");
                    Console.WriteLine($"  Upload speed: {largeBytes.Length / 1024.0 / largeTime.TotalSeconds:F2} KB/s");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup: Delete all uploaded files
                    // ============================================================
                    Console.WriteLine("Cleaning up uploaded files...");
                    Console.WriteLine("-----------------------------");

                    var filesToDelete = new List<string>
                    {
                        textFileId,
                        jsonFileId,
                        xmlFileId,
                        binaryFileId,
                        bufferFileId,
                        largeFileId
                    };
                    filesToDelete.AddRange(uploadedFiles);

                    int deletedCount = 0;
                    foreach (var fileId in filesToDelete)
                    {
                        try
                        {
                            await client.DeleteFileAsync(fileId);
                            deletedCount++;
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"  ✗ Failed to delete {fileId}: {ex.Message}");
                        }
                    }

                    Console.WriteLine($"✓ Deleted {deletedCount} files");
                    Console.WriteLine();

                    Console.WriteLine("===========================================");
                    Console.WriteLine("All examples completed successfully!");
                    Console.WriteLine("===========================================");
                    Console.WriteLine();
                    Console.WriteLine("Key Takeaways:");
                    Console.WriteLine("  • Buffer upload is ideal for in-memory data");
                    Console.WriteLine("  • No disk I/O required, faster for small files");
                    Console.WriteLine("  • Perfect for API responses, generated content");
                    Console.WriteLine("  • Supports all data types: text, JSON, XML, binary");
                    Console.WriteLine("  • Can include metadata for better organization");
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

