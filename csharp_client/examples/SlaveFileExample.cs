// ============================================================================
// FastDFS C# Client - Slave File Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates slave file operations in FastDFS, including
// uploading master files, uploading slave files (thumbnails, previews),
// downloading slave files, and best practices for working with slave files
// in various use cases such as image thumbnails, video transcodes, and
// document previews.
//
// Slave files are associated files linked to a master file, commonly used
// for storing different versions or variants of the same content. They are
// stored on the same storage server as the master file and share the same
// storage group, making them ideal for content delivery scenarios where
// you need multiple representations of the same source material.
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
    /// Example demonstrating FastDFS slave file operations.
    /// 
    /// This example shows:
    /// - How to upload master files to FastDFS storage
    /// - How to upload slave files associated with master files
    /// - How to download slave files from FastDFS storage
    /// - Use cases for slave files (image thumbnails, video transcodes, document previews)
    /// - Best practices for slave file operations
    /// - Error handling and validation
    /// 
    /// Slave files are particularly useful for:
    /// 1. Image thumbnails: Generate multiple sizes (small, medium, large) from original images
    /// 2. Video transcodes: Store different resolutions and formats (720p, 1080p, 4K)
    /// 3. Document previews: Generate preview images or PDFs from source documents
    /// 4. Processed versions: Store edited or filtered versions of original files
    /// 5. Format conversions: Store files in different formats (JPG, PNG, WebP)
    /// </summary>
    class SlaveFileExample
    {
        /// <summary>
        /// Main entry point for the slave file example.
        /// 
        /// This method demonstrates various slave file operations through
        /// a series of examples, each showing different aspects of working
        /// with master and slave files in FastDFS.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Slave File Example");
            Console.WriteLine("========================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates slave file operations,");
            Console.WriteLine("including master file upload, slave file upload,");
            Console.WriteLine("slave file download, and best practices.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For slave file operations, we typically want standard timeouts
            // since slave files are usually smaller than master files.
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
                // For slave file operations, we may need more connections if
                // uploading multiple slave files concurrently
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                // Standard timeout is usually sufficient for slave file operations
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // Slave files are typically smaller, so standard timeout is adequate
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Idle timeout: time before idle connections are closed
                // Standard idle timeout works well for slave file operations
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                // Slave file operations should have retry logic to handle transient
                // network errors, especially important for critical content delivery
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations including slave file operations.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Upload Master File (Image Use Case)
                    // ============================================================
                    // 
                    // This example demonstrates uploading a master file, which
                    // is the first step in working with slave files. Master files
                    // are uploaded using the standard UploadFileAsync method.
                    // 
                    // Use case: Original image that will have thumbnails generated
                    // ============================================================

                    Console.WriteLine("Example 1: Upload Master File (Image Use Case)");
                    Console.WriteLine("=================================================");
                    Console.WriteLine();

                    // Create a sample image file
                    // In real scenarios, this would be an actual image file
                    // (JPG, PNG, etc.) that you want to store and generate
                    // thumbnails or other variants from
                    var masterImagePath = "original_image.jpg";

                    if (!File.Exists(masterImagePath))
                    {
                        // Create a sample image file
                        // In production, this would be your actual image file
                        // For demonstration, we'll create a simple text file
                        // that represents an image file
                        var imageMetadata = "JPEG Image Data - Original High Resolution Image";
                        await File.WriteAllTextAsync(masterImagePath, imageMetadata);
                        Console.WriteLine($"Created sample master image file: {masterImagePath}");
                        Console.WriteLine($"Master image size: {new FileInfo(masterImagePath).Length} bytes");
                        Console.WriteLine();
                    }

                    // Define metadata for the master image file
                    // Metadata helps identify and categorize master files
                    // This is especially useful when managing multiple master files
                    // and their associated slave files
                    var masterImageMetadata = new Dictionary<string, string>
                    {
                        { "type", "image" },
                        { "format", "jpeg" },
                        { "category", "photography" },
                        { "width", "1920" },
                        { "height", "1080" },
                        { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") },
                        { "source", "camera" }
                    };

                    // Upload the master image file
                    // This is the critical first step: uploading the master file
                    // using the standard UploadFileAsync method. The master file
                    // must be uploaded before any slave files can be associated
                    // with it.
                    Console.WriteLine("Uploading master image file...");
                    var masterImageFileId = await client.UploadFileAsync(masterImagePath, masterImageMetadata);
                    
                    Console.WriteLine($"Master image file uploaded successfully!");
                    Console.WriteLine($"Master File ID: {masterImageFileId}");
                    Console.WriteLine();

                    // Get master file information to verify upload
                    // This confirms the master file was uploaded correctly and
                    // provides file size and other metadata information
                    var masterImageInfo = await client.GetFileInfoAsync(masterImageFileId);
                    Console.WriteLine("Master image file information:");
                    Console.WriteLine($"  File Size: {masterImageInfo.FileSize} bytes");
                    Console.WriteLine($"  Create Time: {masterImageInfo.CreateTime}");
                    Console.WriteLine($"  CRC32: {masterImageInfo.CRC32:X8}");
                    Console.WriteLine($"  Source IP: {masterImageInfo.SourceIPAddr}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Upload Slave File (Image Thumbnail Use Case)
                    // ============================================================
                    // 
                    // This example demonstrates uploading a slave file associated
                    // with the master file. Slave files are uploaded using the
                    // UploadSlaveFileAsync method, which requires the master file ID,
                    // a prefix name, file extension, and the slave file data.
                    // 
                    // Use case: Image thumbnails in different sizes
                    // ============================================================

                    Console.WriteLine("Example 2: Upload Slave File (Image Thumbnail Use Case)");
                    Console.WriteLine("==========================================================");
                    Console.WriteLine();

                    // Create thumbnail images of different sizes
                    // In a real scenario, these would be actual thumbnail images
                    // generated from the master image using image processing libraries
                    // For demonstration, we'll create simple text representations
                    var thumbnailSizes = new[]
                    {
                        new { Prefix = "_thumb_small", Size = "150x150", Description = "Small thumbnail" },
                        new { Prefix = "_thumb_medium", Size = "300x300", Description = "Medium thumbnail" },
                        new { Prefix = "_thumb_large", Size = "600x600", Description = "Large thumbnail" }
                    };

                    Console.WriteLine("Uploading thumbnail slave files...");
                    Console.WriteLine();

                    // Dictionary to store slave file IDs for later use
                    // This allows us to track all slave files associated with
                    // the master file and download them later
                    var slaveFileIds = new Dictionary<string, string>();

                    // Upload each thumbnail as a slave file
                    // Each thumbnail is uploaded with a unique prefix name that
                    // identifies its size and purpose. The prefix is used to
                    // generate the slave file ID from the master file ID.
                    foreach (var thumbnail in thumbnailSizes)
                    {
                        // Create thumbnail data
                        // In production, this would be actual image data generated
                        // from the master image using image processing libraries
                        var thumbnailData = Encoding.UTF8.GetBytes(
                            $"JPEG Thumbnail Data - {thumbnail.Description} ({thumbnail.Size})");

                        // Define metadata for the thumbnail
                        // Slave files can have their own metadata, which is useful
                        // for tracking thumbnail dimensions, quality settings, etc.
                        var thumbnailMetadata = new Dictionary<string, string>
                        {
                            { "type", "thumbnail" },
                            { "format", "jpeg" },
                            { "size", thumbnail.Size },
                            { "prefix", thumbnail.Prefix },
                            { "master_file_id", masterImageFileId },
                            { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                        };

                        // Upload the thumbnail as a slave file
                        // The UploadSlaveFileAsync method requires:
                        // 1. Master file ID (the file this slave is associated with)
                        // 2. Prefix name (e.g., "_thumb_small" - must start with underscore or hyphen)
                        // 3. File extension (e.g., "jpg")
                        // 4. Slave file data (the actual thumbnail image bytes)
                        // 5. Optional metadata
                        Console.WriteLine($"  Uploading {thumbnail.Description} ({thumbnail.Size})...");
                        var slaveFileId = await client.UploadSlaveFileAsync(
                            masterImageFileId,        // Master file ID
                            thumbnail.Prefix,         // Prefix name (e.g., "_thumb_small")
                            "jpg",                    // File extension
                            thumbnailData,            // Thumbnail data
                            thumbnailMetadata);        // Optional metadata

                        // Store the slave file ID for later use
                        // Slave file IDs are generated from the master file ID
                        // by appending the prefix and extension
                        slaveFileIds[thumbnail.Prefix] = slaveFileId;

                        Console.WriteLine($"    Slave File ID: {slaveFileId}");
                        Console.WriteLine($"    Uploaded successfully!");
                        Console.WriteLine();
                    }

                    Console.WriteLine("All thumbnail slave files uploaded successfully!");
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Download Slave Files
                    // ============================================================
                    // 
                    // This example demonstrates downloading slave files from
                    // FastDFS storage. Slave files are downloaded using the
                    // standard DownloadFileAsync method with the slave file ID.
                    // 
                    // Best practices:
                    // - Use slave file IDs for efficient content delivery
                    // - Cache slave files when appropriate
                    // - Handle missing slave files gracefully
                    // ============================================================

                    Console.WriteLine("Example 3: Download Slave Files");
                    Console.WriteLine("=================================");
                    Console.WriteLine();

                    // Download each thumbnail slave file
                    // This demonstrates how to retrieve slave files using their
                    // file IDs, which can be used in web applications for
                    // efficient content delivery
                    foreach (var kvp in slaveFileIds)
                    {
                        var prefix = kvp.Key;
                        var slaveFileId = kvp.Value;

                        Console.WriteLine($"Downloading slave file with prefix: {prefix}");

                        // Download the slave file
                        // Slave files are downloaded just like regular files,
                        // using the DownloadFileAsync method with the slave file ID
                        var slaveFileData = await client.DownloadFileAsync(slaveFileId);
                        var slaveFileText = Encoding.UTF8.GetString(slaveFileData);

                        Console.WriteLine($"  Slave File ID: {slaveFileId}");
                        Console.WriteLine($"  File Size: {slaveFileData.Length} bytes");
                        Console.WriteLine($"  Content Preview: {slaveFileText.Substring(0, Math.Min(50, slaveFileText.Length))}...");
                        Console.WriteLine();

                        // Get slave file information
                        // This provides details about the slave file, including
                        // size, creation time, and other metadata
                        var slaveFileInfo = await client.GetFileInfoAsync(slaveFileId);
                        Console.WriteLine($"  File Information:");
                        Console.WriteLine($"    Size: {slaveFileInfo.FileSize} bytes");
                        Console.WriteLine($"    Create Time: {slaveFileInfo.CreateTime}");
                        Console.WriteLine($"    CRC32: {slaveFileInfo.CRC32:X8}");
                        Console.WriteLine();
                    }

                    Console.WriteLine("All slave files downloaded successfully!");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Video Transcode Use Case
                    // ============================================================
                    // 
                    // This example demonstrates using slave files for video
                    // transcodes, where the master file is the original video
                    // and slave files are different resolutions or formats.
                    // 
                    // Use case: Video transcodes in different resolutions
                    // ============================================================

                    Console.WriteLine("Example 4: Video Transcode Use Case");
                    Console.WriteLine("====================================");
                    Console.WriteLine();

                    // Create a sample master video file
                    // In real scenarios, this would be an actual video file
                    // (MP4, AVI, etc.) that you want to transcode into different
                    // resolutions or formats
                    var masterVideoPath = "original_video.mp4";

                    if (!File.Exists(masterVideoPath))
                    {
                        // Create a sample video file
                        // In production, this would be your actual video file
                        var videoMetadata = "MP4 Video Data - Original High Resolution Video (1080p)";
                        await File.WriteAllTextAsync(masterVideoPath, videoMetadata);
                        Console.WriteLine($"Created sample master video file: {masterVideoPath}");
                        Console.WriteLine($"Master video size: {new FileInfo(masterVideoPath).Length} bytes");
                        Console.WriteLine();
                    }

                    // Define metadata for the master video file
                    var masterVideoMetadata = new Dictionary<string, string>
                    {
                        { "type", "video" },
                        { "format", "mp4" },
                        { "resolution", "1920x1080" },
                        { "duration", "120" },
                        { "codec", "h264" },
                        { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                    };

                    // Upload the master video file
                    Console.WriteLine("Uploading master video file...");
                    var masterVideoFileId = await client.UploadFileAsync(masterVideoPath, masterVideoMetadata);
                    
                    Console.WriteLine($"Master video file uploaded successfully!");
                    Console.WriteLine($"Master File ID: {masterVideoFileId}");
                    Console.WriteLine();

                    // Create video transcodes in different resolutions
                    // In a real scenario, these would be actual transcoded video
                    // files generated from the master video using video processing
                    // libraries or services
                    var videoTranscodes = new[]
                    {
                        new { Prefix = "_720p", Resolution = "1280x720", Description = "720p HD" },
                        new { Prefix = "_480p", Resolution = "854x480", Description = "480p SD" },
                        new { Prefix = "_360p", Resolution = "640x360", Description = "360p" }
                    };

                    Console.WriteLine("Uploading video transcode slave files...");
                    Console.WriteLine();

                    var videoSlaveFileIds = new Dictionary<string, string>();

                    // Upload each transcode as a slave file
                    // Each transcode is uploaded with a unique prefix that
                    // identifies its resolution. This allows clients to request
                    // the appropriate resolution based on their bandwidth and
                    // device capabilities.
                    foreach (var transcode in videoTranscodes)
                    {
                        // Create transcode data
                        // In production, this would be actual transcoded video data
                        var transcodeData = Encoding.UTF8.GetBytes(
                            $"MP4 Video Data - {transcode.Description} ({transcode.Resolution})");

                        // Define metadata for the transcode
                        var transcodeMetadata = new Dictionary<string, string>
                        {
                            { "type", "video_transcode" },
                            { "format", "mp4" },
                            { "resolution", transcode.Resolution },
                            { "prefix", transcode.Prefix },
                            { "master_file_id", masterVideoFileId },
                            { "codec", "h264" },
                            { "bitrate", "2000" },
                            { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                        };

                        // Upload the transcode as a slave file
                        Console.WriteLine($"  Uploading {transcode.Description} ({transcode.Resolution})...");
                        var slaveFileId = await client.UploadSlaveFileAsync(
                            masterVideoFileId,        // Master file ID
                            transcode.Prefix,          // Prefix name (e.g., "_720p")
                            "mp4",                     // File extension
                            transcodeData,             // Transcode data
                            transcodeMetadata);        // Optional metadata

                        videoSlaveFileIds[transcode.Prefix] = slaveFileId;

                        Console.WriteLine($"    Slave File ID: {slaveFileId}");
                        Console.WriteLine($"    Uploaded successfully!");
                        Console.WriteLine();
                    }

                    Console.WriteLine("All video transcode slave files uploaded successfully!");
                    Console.WriteLine();

                    // Download a video transcode to demonstrate retrieval
                    // In a real video streaming application, clients would
                    // request the appropriate resolution based on their
                    // bandwidth and device capabilities
                    Console.WriteLine("Downloading video transcode slave file (720p)...");
                    var video720pFileId = videoSlaveFileIds["_720p"];
                    var video720pData = await client.DownloadFileAsync(video720pFileId);
                    var video720pText = Encoding.UTF8.GetString(video720pData);

                    Console.WriteLine($"  Slave File ID: {video720pFileId}");
                    Console.WriteLine($"  File Size: {video720pData.Length} bytes");
                    Console.WriteLine($"  Content Preview: {video720pText.Substring(0, Math.Min(50, video720pText.Length))}...");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Document Preview Use Case
                    // ============================================================
                    // 
                    // This example demonstrates using slave files for document
                    // previews, where the master file is the original document
                    // and slave files are preview images or PDFs.
                    // 
                    // Use case: Document previews in different formats
                    // ============================================================

                    Console.WriteLine("Example 5: Document Preview Use Case");
                    Console.WriteLine("=====================================");
                    Console.WriteLine();

                    // Create a sample master document file
                    // In real scenarios, this would be an actual document file
                    // (PDF, DOCX, etc.) that you want to generate previews from
                    var masterDocumentPath = "original_document.pdf";

                    if (!File.Exists(masterDocumentPath))
                    {
                        // Create a sample document file
                        // In production, this would be your actual document file
                        var documentMetadata = "PDF Document Data - Original Document";
                        await File.WriteAllTextAsync(masterDocumentPath, documentMetadata);
                        Console.WriteLine($"Created sample master document file: {masterDocumentPath}");
                        Console.WriteLine($"Master document size: {new FileInfo(masterDocumentPath).Length} bytes");
                        Console.WriteLine();
                    }

                    // Define metadata for the master document file
                    var masterDocumentMetadata = new Dictionary<string, string>
                    {
                        { "type", "document" },
                        { "format", "pdf" },
                        { "title", "Sample Document" },
                        { "pages", "10" },
                        { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                    };

                    // Upload the master document file
                    Console.WriteLine("Uploading master document file...");
                    var masterDocumentFileId = await client.UploadFileAsync(masterDocumentPath, masterDocumentMetadata);
                    
                    Console.WriteLine($"Master document file uploaded successfully!");
                    Console.WriteLine($"Master File ID: {masterDocumentFileId}");
                    Console.WriteLine();

                    // Create document previews in different formats
                    // In a real scenario, these would be actual preview images
                    // or PDFs generated from the master document using document
                    // processing libraries or services
                    var documentPreviews = new[]
                    {
                        new { Prefix = "_preview_thumb", Format = "jpg", Description = "Thumbnail preview" },
                        new { Prefix = "_preview_page1", Format = "jpg", Description = "First page preview" },
                        new { Prefix = "_preview_pdf", Format = "pdf", Description = "Preview PDF" }
                    };

                    Console.WriteLine("Uploading document preview slave files...");
                    Console.WriteLine();

                    var documentSlaveFileIds = new Dictionary<string, string>();

                    // Upload each preview as a slave file
                    // Each preview is uploaded with a unique prefix that identifies
                    // its format and purpose. This allows clients to request
                    // the appropriate preview format for their needs.
                    foreach (var preview in documentPreviews)
                    {
                        // Create preview data
                        // In production, this would be actual preview image or PDF data
                        var previewData = Encoding.UTF8.GetBytes(
                            $"{preview.Format.ToUpper()} Preview Data - {preview.Description}");

                        // Define metadata for the preview
                        var previewMetadata = new Dictionary<string, string>
                        {
                            { "type", "document_preview" },
                            { "format", preview.Format },
                            { "prefix", preview.Prefix },
                            { "master_file_id", masterDocumentFileId },
                            { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                        };

                        // Upload the preview as a slave file
                        Console.WriteLine($"  Uploading {preview.Description} ({preview.Format})...");
                        var slaveFileId = await client.UploadSlaveFileAsync(
                            masterDocumentFileId,      // Master file ID
                            preview.Prefix,            // Prefix name (e.g., "_preview_thumb")
                            preview.Format,             // File extension
                            previewData,                // Preview data
                            previewMetadata);            // Optional metadata

                        documentSlaveFileIds[preview.Prefix] = slaveFileId;

                        Console.WriteLine($"    Slave File ID: {slaveFileId}");
                        Console.WriteLine($"    Uploaded successfully!");
                        Console.WriteLine();
                    }

                    Console.WriteLine("All document preview slave files uploaded successfully!");
                    Console.WriteLine();

                    // Download a document preview to demonstrate retrieval
                    // In a real document management application, clients would
                    // request the appropriate preview format based on their needs
                    Console.WriteLine("Downloading document preview slave file (thumbnail)...");
                    var documentThumbFileId = documentSlaveFileIds["_preview_thumb"];
                    var documentThumbData = await client.DownloadFileAsync(documentThumbFileId);
                    var documentThumbText = Encoding.UTF8.GetString(documentThumbData);

                    Console.WriteLine($"  Slave File ID: {documentThumbFileId}");
                    Console.WriteLine($"  File Size: {documentThumbData.Length} bytes");
                    Console.WriteLine($"  Content Preview: {documentThumbText.Substring(0, Math.Min(50, documentThumbText.Length))}...");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for working with
                    // slave files in FastDFS, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for Slave File Operations");
                    Console.WriteLine("=========================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Master file requirements:");
                    Console.WriteLine("   - Always upload master file before slave files");
                    Console.WriteLine("   - Master file must exist on the storage server");
                    Console.WriteLine("   - Slave files are stored on the same server as master");
                    Console.WriteLine("   - Master and slave files share the same storage group");
                    Console.WriteLine();
                    Console.WriteLine("2. Prefix naming conventions:");
                    Console.WriteLine("   - Prefix names must start with underscore (_) or hyphen (-)");
                    Console.WriteLine("   - Use descriptive prefixes (e.g., _thumb_small, _720p, _preview)");
                    Console.WriteLine("   - Keep prefixes short but meaningful");
                    Console.WriteLine("   - Each slave file must have a unique prefix");
                    Console.WriteLine("   - Common prefixes: _thumb, _small, _medium, _large, _preview");
                    Console.WriteLine();
                    Console.WriteLine("3. Slave file upload:");
                    Console.WriteLine("   - Use UploadSlaveFileAsync for all slave file uploads");
                    Console.WriteLine("   - Provide master file ID, prefix, extension, and data");
                    Console.WriteLine("   - Slave files can have different extensions than master");
                    Console.WriteLine("   - Add metadata to track slave file characteristics");
                    Console.WriteLine("   - Handle errors appropriately, especially for critical content");
                    Console.WriteLine();
                    Console.WriteLine("4. Use cases and patterns:");
                    Console.WriteLine("   - Image thumbnails: Generate multiple sizes from original");
                    Console.WriteLine("   - Video transcodes: Store different resolutions/formats");
                    Console.WriteLine("   - Document previews: Generate preview images or PDFs");
                    Console.WriteLine("   - Format conversions: Store files in different formats");
                    Console.WriteLine("   - Processed versions: Store edited or filtered versions");
                    Console.WriteLine();
                    Console.WriteLine("5. Performance considerations:");
                    Console.WriteLine("   - Upload slave files in parallel when possible");
                    Console.WriteLine("   - Use connection pooling effectively");
                    Console.WriteLine("   - Consider caching slave files for frequently accessed content");
                    Console.WriteLine("   - Monitor slave file sizes and optimize as needed");
                    Console.WriteLine("   - Use appropriate metadata for efficient content delivery");
                    Console.WriteLine();
                    Console.WriteLine("6. Error handling:");
                    Console.WriteLine("   - Validate master file ID before uploading slave files");
                    Console.WriteLine("   - Implement retry logic for transient failures");
                    Console.WriteLine("   - Handle missing master files gracefully");
                    Console.WriteLine("   - Log slave file upload failures for critical content");
                    Console.WriteLine("   - Consider fallback strategies for missing slave files");
                    Console.WriteLine();
                    Console.WriteLine("7. Metadata management:");
                    Console.WriteLine("   - Use metadata to identify slave file types and characteristics");
                    Console.WriteLine("   - Include master file ID in slave file metadata");
                    Console.WriteLine("   - Store resolution, format, and other relevant information");
                    Console.WriteLine("   - Update metadata if slave file characteristics change");
                    Console.WriteLine();
                    Console.WriteLine("8. Content delivery:");
                    Console.WriteLine("   - Use slave file IDs for efficient content delivery");
                    Console.WriteLine("   - Implement client-side logic to select appropriate slave files");
                    Console.WriteLine("   - Consider bandwidth and device capabilities when selecting slaves");
                    Console.WriteLine("   - Cache slave files when appropriate for performance");
                    Console.WriteLine();
                    Console.WriteLine("9. File management:");
                    Console.WriteLine("   - Keep track of master and slave file relationships");
                    Console.WriteLine("   - Consider cleanup strategies for orphaned slave files");
                    Console.WriteLine("   - Monitor storage usage for slave files");
                    Console.WriteLine("   - Implement versioning strategies if needed");
                    Console.WriteLine();
                    Console.WriteLine("10. Security and access control:");
                    Console.WriteLine("    - Apply appropriate access controls to slave files");
                    Console.WriteLine("    - Consider watermarking for preview images");
                    Console.WriteLine("    - Validate slave file content before serving to clients");
                    Console.WriteLine("    - Implement rate limiting for slave file downloads");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up uploaded files and local test files
                    // Note: Deleting master files does not automatically delete
                    // associated slave files, so we need to delete them separately
                    // ============================================================

                    Console.WriteLine("Cleaning up...");
                    Console.WriteLine();

                    // Delete slave files from FastDFS
                    // It's important to delete slave files before deleting
                    // master files, or at least track them for cleanup
                    Console.WriteLine("Deleting slave files from FastDFS...");
                    Console.WriteLine();

                    // Delete image thumbnail slave files
                    foreach (var kvp in slaveFileIds)
                    {
                        await client.DeleteFileAsync(kvp.Value);
                        Console.WriteLine($"Deleted image thumbnail slave file: {kvp.Key}");
                    }

                    Console.WriteLine();

                    // Delete video transcode slave files
                    foreach (var kvp in videoSlaveFileIds)
                    {
                        await client.DeleteFileAsync(kvp.Value);
                        Console.WriteLine($"Deleted video transcode slave file: {kvp.Prefix}");
                    }

                    Console.WriteLine();

                    // Delete document preview slave files
                    foreach (var kvp in documentSlaveFileIds)
                    {
                        await client.DeleteFileAsync(kvp.Value);
                        Console.WriteLine($"Deleted document preview slave file: {kvp.Key}");
                    }

                    Console.WriteLine();

                    // Delete master files from FastDFS
                    // Master files should be deleted after slave files
                    // to maintain referential integrity
                    await client.DeleteFileAsync(masterImageFileId);
                    Console.WriteLine("Deleted master image file from FastDFS");

                    await client.DeleteFileAsync(masterVideoFileId);
                    Console.WriteLine("Deleted master video file from FastDFS");

                    await client.DeleteFileAsync(masterDocumentFileId);
                    Console.WriteLine("Deleted master document file from FastDFS");

                    Console.WriteLine();

                    // Delete local test files
                    if (File.Exists(masterImagePath))
                    {
                        File.Delete(masterImagePath);
                        Console.WriteLine($"Deleted local file: {masterImagePath}");
                    }

                    if (File.Exists(masterVideoPath))
                    {
                        File.Delete(masterVideoPath);
                        Console.WriteLine($"Deleted local file: {masterVideoPath}");
                    }

                    if (File.Exists(masterDocumentPath))
                    {
                        File.Delete(masterDocumentPath);
                        Console.WriteLine($"Deleted local file: {masterDocumentPath}");
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
                    Console.WriteLine("  - Invalid master file ID or file not found");
                    Console.WriteLine("  - Slave file prefix naming issues");
                    Console.WriteLine("  - File size limits exceeded");
                    Console.WriteLine("  - Storage server configuration issues");
                }
                catch (NotImplementedException ex)
                {
                    // Handle case where slave file operations are not yet implemented
                    Console.WriteLine($"Operation not implemented: {ex.Message}");
                    Console.WriteLine();
                    Console.WriteLine("Note: Slave file operations may not be fully");
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

