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
// Key Concepts:
// - Buffer uploads eliminate the need for temporary disk files
// - Ideal for data that already exists in memory
// - Faster for small to medium files (no disk I/O overhead)
// - Supports all data types that can be converted to byte arrays
// - Can include metadata for better file organization
//
// Use Cases:
// - API responses that need to be stored
// - Dynamically generated content (reports, documents)
// - Data from databases or in-memory caches
// - Real-time data processing results
// - Web service responses
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
    /// 
    /// The UploadBufferAsync method is particularly useful when:
    /// 1. Data is already in memory (no need to write to disk first)
    /// 2. Data is generated dynamically (reports, API responses)
    /// 3. Performance is critical (avoiding disk I/O overhead)
    /// 4. Working with temporary data that shouldn't persist locally
    /// </summary>
    class UploadBufferExample
    {
        /// <summary>
        /// Main entry point for the upload buffer example.
        /// 
        /// This method demonstrates various scenarios for uploading data
        /// from memory buffers to FastDFS storage servers.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// In a production scenario, these might include:
        /// - Configuration file path
        /// - Tracker server addresses
        /// - Operation mode (test, production, etc.)
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task completes when all examples have finished executing.
        /// </returns>
        static async Task Main(string[] args)
        {
            // Display header information
            Console.WriteLine("FastDFS C# Client - Upload Buffer Example");
            Console.WriteLine("===========================================");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create client configuration
            // ====================================================================
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // 
            // Important configuration considerations:
            // - TrackerAddresses: List of tracker servers for redundancy
            // - MaxConnections: Higher values allow more concurrency
            // - ConnectTimeout: How long to wait for connection establishment
            // - NetworkTimeout: How long to wait for network I/O operations
            // - IdleTimeout: When to close idle connections
            // - RetryCount: How many times to retry failed operations
            var config = new FastDFSClientConfig
            {
                // Specify tracker server addresses
                // Tracker servers coordinate file storage and retrieval operations.
                // Multiple trackers provide redundancy and load balancing.
                // Format: "IP:PORT" or "hostname:PORT"
                TrackerAddresses = new[]
                {
                    "192.168.1.100:22122",  // Primary tracker server
                    "192.168.1.101:22122"   // Secondary tracker server (for redundancy)
                },

                // Maximum number of connections per server
                // Higher values allow more concurrent operations but consume more resources.
                // Recommended: 50-200 for most applications
                // For high-throughput scenarios, consider 200-500
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                // This applies to both tracker and storage server connections.
                // Too short: may fail during network congestion
                // Too long: may hang on unreachable servers
                // Recommended: 5-10 seconds
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // This applies to all network I/O operations (upload, download, etc.)
                // Should be adjusted based on expected file sizes and network conditions.
                // For large files, consider increasing this value.
                // Recommended: 30-60 seconds for most scenarios
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Idle timeout: time before idle connections are closed
                // Idle connections are automatically closed to free resources.
                // This helps manage connection pool size dynamically.
                // Recommended: 5-10 minutes
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                // FastDFS client automatically retries failed operations.
                // Retries use exponential backoff to avoid overwhelming servers.
                // Recommended: 3-5 retries for most scenarios
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS client
            // ====================================================================
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations.
            //
            // Using 'using' statement ensures proper disposal of resources:
            // - Closes all connections
            // - Releases connection pool resources
            // - Cleans up any pending operations
            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Upload a simple string
                    // ============================================================
                    // This is the most basic use case: uploading a text string
                    // that exists in memory. No file system operations required.
                    Console.WriteLine("Example 1: Upload a simple string");
                    Console.WriteLine("-----------------------------------");
                    Console.WriteLine();

                    // Create a sample text string
                    // In real scenarios, this might come from:
                    // - User input
                    // - API responses
                    // - Generated reports
                    // - Log entries
                    // - Configuration data
                    string textContent = "Hello, FastDFS! This is a test string uploaded from memory.";

                    // Convert string to byte array using UTF-8 encoding
                    // UTF-8 is the most common encoding for text data.
                    // Other encodings (UTF-16, ASCII) can also be used if needed.
                    byte[] textBytes = Encoding.UTF8.GetBytes(textContent);

                    // Upload the string as a text file
                    // Parameters:
                    //   1. textBytes: The byte array containing the data
                    //   2. "txt": File extension (without dot) - used by FastDFS for file type identification
                    //   3. null: Optional metadata dictionary (null means no metadata)
                    //
                    // The method returns a file ID in the format: "group/path/filename"
                    // This file ID can be used for subsequent operations (download, delete, etc.)
                    var textFileId = await client.UploadBufferAsync(textBytes, "txt", null);

                    // Display upload results
                    Console.WriteLine($"✓ String uploaded successfully!");
                    Console.WriteLine($"  File ID: {textFileId}");
                    Console.WriteLine($"  Content length: {textBytes.Length} bytes");
                    Console.WriteLine();

                    // Verify by downloading the uploaded content
                    // This demonstrates that the upload was successful and the data
                    // can be retrieved correctly.
                    var downloadedText = await client.DownloadFileAsync(textFileId);
                    var downloadedString = Encoding.UTF8.GetString(downloadedText);

                    // Display verification results
                    Console.WriteLine($"  Downloaded content: {downloadedString}");
                    Console.WriteLine($"  Content matches: {textContent == downloadedString}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Upload JSON data
                    // ============================================================
                    // JSON is a common format for structured data.
                    // This example shows how to upload JSON with metadata.
                    Console.WriteLine("Example 2: Upload JSON data");
                    Console.WriteLine("----------------------------");
                    Console.WriteLine();

                    // Create a sample JSON object
                    // In real scenarios, this might represent:
                    // - API response data
                    // - Configuration objects
                    // - Database query results
                    // - Application state
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

                    // Serialize the object to JSON string
                    // JsonSerializerOptions allows customization of serialization:
                    // - WriteIndented: Makes JSON human-readable (adds indentation)
                    // - PropertyNamingPolicy: Controls property name casing
                    // - IgnoreNullValues: Excludes null properties
                    string jsonString = JsonSerializer.Serialize(jsonObject, new JsonSerializerOptions
                    {
                        WriteIndented = true  // Makes JSON readable (adds newlines and indentation)
                    });

                    // Convert JSON string to byte array
                    // UTF-8 encoding is standard for JSON
                    byte[] jsonBytes = Encoding.UTF8.GetBytes(jsonString);

                    // Create metadata dictionary for the JSON file
                    // Metadata provides additional information about the file:
                    // - Content type: Helps identify file format
                    // - Source: Tracks where the data came from
                    // - Created-by: Identifies the application/component that created it
                    //
                    // Metadata can be retrieved later using GetMetadataAsync
                    var jsonMetadata = new Dictionary<string, string>
                    {
                        { "content-type", "application/json" },
                        { "source", "api-response" },
                        { "created-by", "UploadBufferExample" }
                    };

                    // Upload JSON with metadata
                    // The metadata will be stored with the file and can be retrieved later
                    var jsonFileId = await client.UploadBufferAsync(jsonBytes, "json", jsonMetadata);

                    // Display upload results
                    Console.WriteLine($"✓ JSON uploaded successfully!");
                    Console.WriteLine($"  File ID: {jsonFileId}");
                    Console.WriteLine($"  JSON size: {jsonBytes.Length} bytes");

                    // Show a preview of the JSON content (first 100 characters)
                    // This helps verify the content without displaying the entire JSON
                    int previewLength = Math.Min(100, jsonString.Length);
                    Console.WriteLine($"  JSON preview: {jsonString.Substring(0, previewLength)}...");
                    Console.WriteLine();

                    // Verify metadata was set correctly
                    // Retrieve the metadata we just set to confirm it was stored properly
                    var retrievedMetadata = await client.GetMetadataAsync(jsonFileId);

                    // Display all metadata key-value pairs
                    Console.WriteLine("  Metadata:");
                    foreach (var kvp in retrievedMetadata)
                    {
                        Console.WriteLine($"    {kvp.Key}: {kvp.Value}");
                    }
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Upload XML data
                    // ============================================================
                    // XML is another common format for structured data.
                    // This example demonstrates uploading XML documents.
                    Console.WriteLine("Example 3: Upload XML data");
                    Console.WriteLine("--------------------------");
                    Console.WriteLine();

                    // Create a sample XML document
                    // In real scenarios, XML might come from:
                    // - SOAP web services
                    // - Configuration files
                    // - Data exchange formats
                    // - Document formats (Office Open XML, etc.)
                    //
                    // Using verbatim string (@) allows multi-line strings without escaping
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

                    // Convert XML string to byte array
                    // UTF-8 encoding is standard for XML
                    byte[] xmlBytes = Encoding.UTF8.GetBytes(xmlContent);

                    // Upload XML without metadata (third parameter is null)
                    // In some cases, you may not need metadata if the file extension
                    // and content are sufficient for identification
                    var xmlFileId = await client.UploadBufferAsync(xmlBytes, "xml", null);

                    // Display upload results
                    Console.WriteLine($"✓ XML uploaded successfully!");
                    Console.WriteLine($"  File ID: {xmlFileId}");
                    Console.WriteLine($"  XML size: {xmlBytes.Length} bytes");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Upload binary data (image simulation)
                    // ============================================================
                    // Binary data includes images, PDFs, executables, etc.
                    // This example shows how to upload binary data from memory.
                    Console.WriteLine("Example 4: Upload binary data");
                    Console.WriteLine("-------------------------------");
                    Console.WriteLine();

                    // Simulate binary data (e.g., image, PDF, etc.)
                    // In a real scenario, this might come from:
                    // - Image processing libraries (System.Drawing, ImageSharp, etc.)
                    // - PDF generators (iTextSharp, PdfSharp, etc.)
                    // - Encrypted data (AES, RSA encrypted bytes)
                    // - Compressed archives (ZIP, GZIP compressed data)
                    // - Serialized objects (protobuf, MessagePack, etc.)
                    //
                    // For this example, we create random binary data to simulate
                    // real binary content
                    byte[] binaryData = new byte[1024];  // 1 KB of binary data
                    new Random().NextBytes(binaryData);   // Fill with random data

                    // Upload binary data
                    // File extension "bin" indicates binary data
                    // In real scenarios, use appropriate extensions:
                    // - "jpg", "png" for images
                    // - "pdf" for PDF documents
                    // - "zip" for compressed archives
                    var binaryFileId = await client.UploadBufferAsync(binaryData, "bin", null);

                    // Display upload results
                    Console.WriteLine($"✓ Binary data uploaded successfully!");
                    Console.WriteLine($"  File ID: {binaryFileId}");
                    Console.WriteLine($"  Binary size: {binaryData.Length} bytes");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Compare Buffer Upload vs File Upload
                    // ============================================================
                    // This example demonstrates the performance difference between
                    // uploading from a buffer vs uploading from a file.
                    //
                    // Buffer upload advantages:
                    // - No disk I/O required
                    // - Faster for small to medium files
                    // - No temporary files to clean up
                    //
                    // File upload advantages:
                    // - Better for very large files (streaming)
                    // - Can leverage OS file system caching
                    Console.WriteLine("Example 5: Compare Buffer vs File Upload");
                    Console.WriteLine("-------------------------------------------");
                    Console.WriteLine();

                    // Create sample text for comparison
                    string comparisonText = "This is a comparison between buffer and file upload methods.";

                    // ============================================================
                    // Method 1: Upload from buffer (no disk I/O)
                    // ============================================================
                    Console.WriteLine("Method 1: Upload from buffer (no disk I/O)");
                    Console.WriteLine();

                    // Record start time for performance measurement
                    var startTime = DateTime.UtcNow;

                    // Convert text to byte array
                    byte[] bufferData = Encoding.UTF8.GetBytes(comparisonText);

                    // Upload from buffer
                    // This method directly uploads from memory without any disk operations
                    var bufferFileId = await client.UploadBufferAsync(bufferData, "txt", null);

                    // Calculate elapsed time
                    var bufferTime = DateTime.UtcNow - startTime;

                    // Display results
                    Console.WriteLine($"  ✓ Uploaded in {bufferTime.TotalMilliseconds:F2} ms");
                    Console.WriteLine($"  File ID: {bufferFileId}");
                    Console.WriteLine();

                    // ============================================================
                    // Method 2: Upload from file (requires disk I/O)
                    // ============================================================
                    Console.WriteLine("Method 2: Upload from file (requires disk I/O)");
                    Console.WriteLine();

                    // Create a temporary file for comparison
                    // Path.GetTempFileName() creates a unique temporary file
                    var tempFile = Path.GetTempFileName();

                    try
                    {
                        // Write text to temporary file
                        // This simulates the disk I/O overhead of file-based uploads
                        await File.WriteAllTextAsync(tempFile, comparisonText);

                        // Record start time for performance measurement
                        startTime = DateTime.UtcNow;

                        // Upload from file
                        // This method reads from disk, which adds I/O overhead
                        var fileFileId = await client.UploadFileAsync(tempFile, null);

                        // Calculate elapsed time
                        var fileTime = DateTime.UtcNow - startTime;

                        // Display results
                        Console.WriteLine($"  ✓ Uploaded in {fileTime.TotalMilliseconds:F2} ms");
                        Console.WriteLine($"  File ID: {fileFileId}");
                        Console.WriteLine();

                        // ============================================================
                        // Performance Comparison
                        // ============================================================
                        Console.WriteLine("Performance Comparison:");
                        Console.WriteLine($"  Buffer upload: {bufferTime.TotalMilliseconds:F2} ms");
                        Console.WriteLine($"  File upload:   {fileTime.TotalMilliseconds:F2} ms");
                        Console.WriteLine($"  Difference:    {Math.Abs((bufferTime - fileTime).TotalMilliseconds):F2} ms");
                        Console.WriteLine($"  Buffer is {(bufferTime < fileTime ? "faster" : "slower")}");
                        Console.WriteLine();

                        // Note: Performance differences will vary based on:
                        // - File size (larger files may show less difference)
                        // - Disk speed (SSD vs HDD)
                        // - System load
                        // - Network conditions

                        // Clean up the test file from FastDFS
                        await client.DeleteFileAsync(fileFileId);
                    }
                    finally
                    {
                        // Always clean up the temporary file
                        // This ensures no temporary files are left behind
                        if (File.Exists(tempFile))
                        {
                            File.Delete(tempFile);
                        }
                    }

                    // ============================================================
                    // Example 6: Upload with different file extensions
                    // ============================================================
                    // FastDFS uses file extensions to identify file types.
                    // This example demonstrates uploading the same content with
                    // different extensions to show how FastDFS handles them.
                    Console.WriteLine("Example 6: Upload with different file extensions");
                    Console.WriteLine("--------------------------------------------------");
                    Console.WriteLine();

                    // List of common file extensions to test
                    // Each extension will be used to upload the same content
                    var extensions = new[] { "txt", "json", "xml", "csv", "log", "dat" };

                    // List to store uploaded file IDs for cleanup
                    var uploadedFiles = new List<string>();

                    // Upload the same content with different extensions
                    foreach (var ext in extensions)
                    {
                        // Create content specific to each extension
                        // In real scenarios, content would match the extension type
                        string content = $"Sample content for .{ext} file";

                        // Convert to byte array
                        byte[] data = Encoding.UTF8.GetBytes(content);

                        // Upload with specific extension
                        var fileId = await client.UploadBufferAsync(data, ext, null);

                        // Store file ID for later cleanup
                        uploadedFiles.Add(fileId);

                        // Display upload result
                        Console.WriteLine($"  ✓ Uploaded .{ext} file: {fileId}");
                    }

                    Console.WriteLine();

                    // ============================================================
                    // Example 7: Upload large buffer (chunked data simulation)
                    // ============================================================
                    // This example demonstrates uploading larger datasets.
                    // In real scenarios, large data might come from:
                    // - Database query results
                    // - API responses with many records
                    // - Generated reports
                    // - Log file contents
                    Console.WriteLine("Example 7: Upload large buffer");
                    Console.WriteLine("-------------------------------");
                    Console.WriteLine();

                    // Simulate uploading a larger dataset
                    // StringBuilder is efficient for building large strings
                    var largeData = new StringBuilder();

                    // Generate 1000 lines of data
                    // This simulates a real-world scenario with substantial data
                    for (int i = 0; i < 1000; i++)
                    {
                        largeData.AppendLine($"Line {i}: This is line number {i} in a large dataset.");
                    }

                    // Convert StringBuilder to byte array
                    byte[] largeBytes = Encoding.UTF8.GetBytes(largeData.ToString());

                    // Record start time for performance measurement
                    startTime = DateTime.UtcNow;

                    // Upload large buffer
                    // For very large files, consider:
                    // - Using streaming uploads (if available)
                    // - Chunking the data
                    // - Monitoring memory usage
                    var largeFileId = await client.UploadBufferAsync(largeBytes, "txt", null);

                    // Calculate elapsed time
                    var largeTime = DateTime.UtcNow - startTime;

                    // Display results with detailed metrics
                    Console.WriteLine($"✓ Large buffer uploaded successfully!");
                    Console.WriteLine($"  File ID: {largeFileId}");
                    Console.WriteLine($"  Size: {largeBytes.Length:N0} bytes ({largeBytes.Length / 1024.0:F2} KB)");
                    Console.WriteLine($"  Upload time: {largeTime.TotalMilliseconds:F2} ms");
                    Console.WriteLine($"  Upload speed: {largeBytes.Length / 1024.0 / largeTime.TotalSeconds:F2} KB/s");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup: Delete all uploaded files
                    // ============================================================
                    // It's good practice to clean up test files after examples
                    // This prevents accumulating test data in the storage system
                    Console.WriteLine("Cleaning up uploaded files...");
                    Console.WriteLine("-----------------------------");
                    Console.WriteLine();

                    // Collect all file IDs that need to be deleted
                    var filesToDelete = new List<string>
                    {
                        textFileId,      // From Example 1
                        jsonFileId,      // From Example 2
                        xmlFileId,       // From Example 3
                        binaryFileId,     // From Example 4
                        bufferFileId,    // From Example 5
                        largeFileId      // From Example 7
                    };

                    // Add files from Example 6 (multiple extensions)
                    filesToDelete.AddRange(uploadedFiles);

                    // Track successful deletions
                    int deletedCount = 0;

                    // Delete each file
                    // Using try-catch to handle any deletion errors gracefully
                    foreach (var fileId in filesToDelete)
                    {
                        try
                        {
                            // Delete the file from FastDFS storage
                            await client.DeleteFileAsync(fileId);
                            deletedCount++;
                        }
                        catch (Exception ex)
                        {
                            // Log deletion failures but continue with other files
                            // In production, you might want to log this to a logging system
                            Console.WriteLine($"  ✗ Failed to delete {fileId}: {ex.Message}");
                        }
                    }

                    // Display cleanup results
                    Console.WriteLine($"✓ Deleted {deletedCount} files");
                    Console.WriteLine();

                    // ============================================================
                    // Summary and Key Takeaways
                    // ============================================================
                    Console.WriteLine("===========================================");
                    Console.WriteLine("All examples completed successfully!");
                    Console.WriteLine("===========================================");
                    Console.WriteLine();

                    // Display key takeaways for developers
                    Console.WriteLine("Key Takeaways:");
                    Console.WriteLine("  • Buffer upload is ideal for in-memory data");
                    Console.WriteLine("  • No disk I/O required, faster for small files");
                    Console.WriteLine("  • Perfect for API responses, generated content");
                    Console.WriteLine("  • Supports all data types: text, JSON, XML, binary");
                    Console.WriteLine("  • Can include metadata for better organization");
                    Console.WriteLine();

                    // Additional tips for developers
                    Console.WriteLine("Best Practices:");
                    Console.WriteLine("  • Use buffer upload for data already in memory");
                    Console.WriteLine("  • Use file upload for very large files (streaming)");
                    Console.WriteLine("  • Always include appropriate file extensions");
                    Console.WriteLine("  • Add metadata for better file organization");
                    Console.WriteLine("  • Handle errors appropriately in production code");
                    Console.WriteLine("  • Clean up test files after examples");
                    Console.WriteLine();
                }
                catch (FastDFSException ex)
                {
                    // Handle FastDFS-specific errors
                    // FastDFSException is thrown for FastDFS protocol errors,
                    // network errors, and server-side errors
                    Console.WriteLine($"FastDFS Error: {ex.Message}");

                    // Display inner exception if available
                    // Inner exceptions often contain more detailed error information
                    if (ex.InnerException != null)
                    {
                        Console.WriteLine($"Inner Exception: {ex.InnerException.Message}");
                    }

                    // In production, you might want to:
                    // - Log the full exception details
                    // - Retry the operation
                    // - Notify monitoring systems
                    // - Return appropriate error codes to callers
                }
                catch (Exception ex)
                {
                    // Handle other unexpected errors
                    // This catches any other exceptions not specifically handled above
                    Console.WriteLine($"Error: {ex.Message}");
                    Console.WriteLine($"Stack Trace: {ex.StackTrace}");

                    // In production, you might want to:
                    // - Log the full stack trace
                    // - Send error reports to error tracking services
                    // - Provide user-friendly error messages
                }
            }

            // Wait for user input before exiting
            // This allows users to review the output before the console closes
            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}
