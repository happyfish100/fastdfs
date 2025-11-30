// ============================================================================
// FastDFS C# Client - Streaming Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates streaming large files efficiently, memory-efficient
// operations, chunked uploads/downloads, progress reporting, and backpressure
// handling in the FastDFS C# client library. It shows how to process large
// files without loading them entirely into memory.
//
// Streaming operations are essential for handling large files efficiently,
// minimizing memory usage, and providing responsive user experiences through
// progress reporting. This example provides comprehensive patterns for
// streaming operations in FastDFS applications.
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
    /// Example demonstrating streaming operations in FastDFS.
    /// 
    /// This example shows:
    /// - How to stream large files efficiently
    /// - How to perform memory-efficient operations
    /// - How to implement chunked uploads and downloads
    /// - How to report progress during streaming operations
    /// - How to handle backpressure in streaming scenarios
    /// 
    /// Streaming patterns demonstrated:
    /// 1. Chunked file uploads
    /// 2. Chunked file downloads
    /// 3. Memory-efficient operations
    /// 4. Progress reporting
    /// 5. Backpressure handling
    /// 6. Streaming with cancellation
    /// 7. Large file processing
    /// </summary>
    class StreamingExample
    {
        /// <summary>
        /// Default chunk size for streaming operations (1MB).
        /// 
        /// This size balances memory usage and network efficiency.
        /// Larger chunks reduce network overhead but use more memory.
        /// Smaller chunks use less memory but may have more network overhead.
        /// </summary>
        private const int DefaultChunkSize = 1024 * 1024; // 1MB

        /// <summary>
        /// Main entry point for the streaming example.
        /// 
        /// This method demonstrates various streaming patterns through
        /// a series of examples, each showing different aspects of
        /// streaming operations in FastDFS.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Streaming Example");
            Console.WriteLine("======================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates streaming large files efficiently,");
            Console.WriteLine("memory-efficient operations, chunked uploads/downloads,");
            Console.WriteLine("progress reporting, and backpressure handling.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For streaming operations, consider longer network timeouts for
            // large file transfers.
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
                // For streaming, more connections can help with concurrent operations
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // For large file streaming, use longer timeouts
                NetworkTimeout = TimeSpan.FromSeconds(300), // 5 minutes for large files

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
            // file operations including streaming support.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Stream Large Files Efficiently
                    // ============================================================
                    // 
                    // This example demonstrates streaming large files efficiently
                    // by processing them in chunks rather than loading entire
                    // files into memory. This is essential for handling files
                    // larger than available memory.
                    // 
                    // Streaming benefits:
                    // - Constant memory usage regardless of file size
                    // - Ability to process files larger than available memory
                    // - Better resource utilization
                    // - Improved responsiveness
                    // ============================================================

                    Console.WriteLine("Example 1: Stream Large Files Efficiently");
                    Console.WriteLine("==========================================");
                    Console.WriteLine();

                    // Create a large test file for streaming examples
                    Console.WriteLine("Creating large test file for streaming examples...");
                    Console.WriteLine();

                    var largeTestFile = "large_streaming_test.txt";
                    var fileSize = 10 * 1024 * 1024; // 10MB test file

                    await CreateLargeTestFileAsync(largeTestFile, fileSize);
                    var actualFileSize = new FileInfo(largeTestFile).Length;
                    Console.WriteLine($"Large test file created: {largeTestFile}");
                    Console.WriteLine($"  File size: {actualFileSize:N0} bytes ({actualFileSize / (1024.0 * 1024.0):F2} MB)");
                    Console.WriteLine();

                    // Pattern 1: Streaming upload with chunked processing
                    Console.WriteLine("Pattern 1: Streaming Upload with Chunked Processing");
                    Console.WriteLine("---------------------------------------------------");
                    Console.WriteLine();

                    Console.WriteLine("Uploading large file using chunked streaming...");
                    Console.WriteLine();

                    // For FastDFS, we'll demonstrate chunked reading and processing
                    // Note: FastDFS uploads are typically done in one operation,
                    // but we can demonstrate memory-efficient reading patterns
                    var chunkSize = DefaultChunkSize;
                    var totalChunks = (int)Math.Ceiling((double)actualFileSize / chunkSize);

                    Console.WriteLine($"  Chunk size: {chunkSize:N0} bytes ({chunkSize / 1024.0:F2} KB)");
                    Console.WriteLine($"  Total chunks: {totalChunks}");
                    Console.WriteLine();

                    // Read file in chunks to demonstrate memory efficiency
                    // In a real scenario, you might process chunks before uploading
                    var chunksProcessed = 0;
                    using (var fileStream = new FileStream(largeTestFile, FileMode.Open, FileAccess.Read, FileShare.Read, chunkSize, useAsync: true))
                    {
                        var buffer = new byte[chunkSize];
                        int bytesRead;

                        while ((bytesRead = await fileStream.ReadAsync(buffer, 0, chunkSize)) > 0)
                        {
                            chunksProcessed++;
                            var chunkData = new byte[bytesRead];
                            Array.Copy(buffer, chunkData, bytesRead);

                            // In a real scenario, you would process or upload this chunk
                            // For demonstration, we'll just track progress
                            var progress = (chunksProcessed * 100.0 / totalChunks);
                            Console.WriteLine($"  Processed chunk {chunksProcessed}/{totalChunks} ({progress:F1}%) - {bytesRead:N0} bytes");
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine($"  ✓ Processed {chunksProcessed} chunks");
                    Console.WriteLine($"  ✓ Memory usage: ~{chunkSize / 1024.0:F2} KB (constant)");
                    Console.WriteLine($"  ✓ File size: {actualFileSize / (1024.0 * 1024.0):F2} MB");
                    Console.WriteLine();

                    // Upload the file (FastDFS handles this efficiently)
                    Console.WriteLine("Uploading file to FastDFS...");
                    var uploadedFileId = await client.UploadFileAsync(largeTestFile, null);
                    Console.WriteLine($"  File uploaded: {uploadedFileId}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Memory-Efficient Operations
                    // ============================================================
                    // 
                    // This example demonstrates memory-efficient operations by
                    // processing files in chunks and avoiding loading entire
                    // files into memory. This is crucial for applications with
                    // limited memory or when processing very large files.
                    // 
                    // Memory efficiency patterns:
                    // - Chunked file reading
                    // - Chunked file writing
                    // - Constant memory usage
                    // - Streaming processing
                    // ============================================================

                    Console.WriteLine("Example 2: Memory-Efficient Operations");
                    Console.WriteLine("=======================================");
                    Console.WriteLine();

                    // Pattern 1: Memory-efficient file processing
                    Console.WriteLine("Pattern 1: Memory-Efficient File Processing");
                    Console.WriteLine("---------------------------------------------");
                    Console.WriteLine();

                    var processingChunkSize = 512 * 1024; // 512KB chunks for processing
                    Console.WriteLine($"Processing file in {processingChunkSize / 1024.0:F2} KB chunks...");
                    Console.WriteLine();

                    var processedBytes = 0L;
                    var maxMemoryUsage = 0L;

                    using (var fileStream = new FileStream(largeTestFile, FileMode.Open, FileAccess.Read, FileShare.Read, processingChunkSize, useAsync: true))
                    {
                        var buffer = new byte[processingChunkSize];
                        int bytesRead;

                        while ((bytesRead = await fileStream.ReadAsync(buffer, 0, processingChunkSize)) > 0)
                        {
                            // Process chunk (e.g., calculate checksum, transform data, etc.)
                            // For demonstration, we'll just track memory usage
                            var currentMemory = GC.GetTotalMemory(false);
                            if (currentMemory > maxMemoryUsage)
                            {
                                maxMemoryUsage = currentMemory;
                            }

                            processedBytes += bytesRead;
                            var progress = (processedBytes * 100.0 / actualFileSize);
                            Console.WriteLine($"  Processed: {processedBytes:N0} bytes ({progress:F1}%) - Memory: {currentMemory / 1024.0:F2} KB");
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine($"  ✓ Total processed: {processedBytes:N0} bytes");
                    Console.WriteLine($"  ✓ Peak memory usage: {maxMemoryUsage / 1024.0:F2} KB");
                    Console.WriteLine($"  ✓ Memory efficiency: Constant memory usage regardless of file size");
                    Console.WriteLine();

                    // Pattern 2: Memory-efficient download
                    Console.WriteLine("Pattern 2: Memory-Efficient Download");
                    Console.WriteLine("-------------------------------------");
                    Console.WriteLine();

                    if (uploadedFileId != null)
                    {
                        Console.WriteLine("Downloading file using memory-efficient streaming...");
                        Console.WriteLine();

                        var downloadFilePath = "downloaded_streaming_file.txt";
                        var downloadChunkSize = 1024 * 1024; // 1MB chunks

                        // Use DownloadToFileAsync for memory-efficient download
                        // This streams directly to disk without loading into memory
                        await client.DownloadToFileAsync(uploadedFileId, downloadFilePath);

                        var downloadedFileSize = new FileInfo(downloadFilePath).Length;
                        Console.WriteLine($"  ✓ File downloaded: {downloadFilePath}");
                        Console.WriteLine($"  ✓ Downloaded size: {downloadedFileSize:N0} bytes");
                        Console.WriteLine($"  ✓ Memory usage: Minimal (streamed directly to disk)");
                        Console.WriteLine();

                        // Clean up downloaded file
                        if (File.Exists(downloadFilePath))
                        {
                            File.Delete(downloadFilePath);
                        }
                    }

                    // ============================================================
                    // Example 3: Chunked Uploads
                    // ============================================================
                    // 
                    // This example demonstrates chunked upload patterns for
                    // large files. While FastDFS handles uploads in single
                    // operations, we can demonstrate chunked reading and
                    // processing patterns that can be used for pre-processing
                    // or validation before upload.
                    // 
                    // Chunked upload patterns:
                    // - Reading file in chunks
                    // - Processing chunks before upload
                    // - Progress tracking during chunked processing
                    // - Memory-efficient chunk handling
                    // ============================================================

                    Console.WriteLine("Example 3: Chunked Uploads");
                    Console.WriteLine("===========================");
                    Console.WriteLine();

                    // Pattern 1: Chunked file reading for upload preparation
                    Console.WriteLine("Pattern 1: Chunked File Reading for Upload Preparation");
                    Console.WriteLine("----------------------------------------------------------");
                    Console.WriteLine();

                    var uploadChunkSize = 2 * 1024 * 1024; // 2MB chunks
                    Console.WriteLine($"Preparing file for upload using {uploadChunkSize / 1024.0 / 1024.0:F2} MB chunks...");
                    Console.WriteLine();

                    var uploadChunks = new List<byte[]>();
                    var totalUploadBytes = 0L;

                    using (var fileStream = new FileStream(largeTestFile, FileMode.Open, FileAccess.Read, FileShare.Read, uploadChunkSize, useAsync: true))
                    {
                        var buffer = new byte[uploadChunkSize];
                        int bytesRead;
                        int chunkIndex = 0;

                        while ((bytesRead = await fileStream.ReadAsync(buffer, 0, uploadChunkSize)) > 0)
                        {
                            chunkIndex++;
                            var chunk = new byte[bytesRead];
                            Array.Copy(buffer, chunk, bytesRead);

                            // In a real scenario, you might:
                            // - Validate chunk data
                            // - Calculate chunk checksum
                            // - Transform chunk data
                            // - Upload chunk to temporary storage
                            // For demonstration, we'll just track chunks

                            uploadChunks.Add(chunk);
                            totalUploadBytes += bytesRead;

                            var progress = (totalUploadBytes * 100.0 / actualFileSize);
                            Console.WriteLine($"  Chunk {chunkIndex}: {bytesRead:N0} bytes ({progress:F1}%)");
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine($"  ✓ Prepared {uploadChunks.Count} chunks");
                    Console.WriteLine($"  ✓ Total size: {totalUploadBytes:N0} bytes");
                    Console.WriteLine($"  ✓ Chunk size: {uploadChunkSize / 1024.0 / 1024.0:F2} MB");
                    Console.WriteLine();

                    // Pattern 2: Upload with chunk validation
                    Console.WriteLine("Pattern 2: Upload with Chunk Validation");
                    Console.WriteLine("---------------------------------------");
                    Console.WriteLine();

                    Console.WriteLine("Validating chunks before upload...");
                    Console.WriteLine();

                    var validChunks = 0;
                    foreach (var chunk in uploadChunks)
                    {
                        // Validate chunk (e.g., check size, checksum, format, etc.)
                        var isValid = chunk.Length > 0; // Simple validation
                        if (isValid)
                        {
                            validChunks++;
                        }
                    }

                    Console.WriteLine($"  ✓ Validated {validChunks}/{uploadChunks.Count} chunks");
                    Console.WriteLine();

                    // Upload file (FastDFS handles the actual upload)
                    Console.WriteLine("Uploading validated file...");
                    var validatedFileId = await client.UploadFileAsync(largeTestFile, null);
                    Console.WriteLine($"  ✓ File uploaded: {validatedFileId}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Chunked Downloads
                    // ============================================================
                    // 
                    // This example demonstrates chunked download patterns using
                    // DownloadFileRangeAsync to download files in chunks. This
                    // is useful for large files, resuming downloads, or
                    // processing files as they are downloaded.
                    // 
                    // Chunked download patterns:
                    // - Downloading files in chunks
                    // - Processing chunks as they are downloaded
                    // - Resuming interrupted downloads
                    // - Memory-efficient chunk handling
                    // ============================================================

                    Console.WriteLine("Example 4: Chunked Downloads");
                    Console.WriteLine("==============================");
                    Console.WriteLine();

                    if (uploadedFileId != null)
                    {
                        // Get file info to determine size
                        var fileInfo = await client.GetFileInfoAsync(uploadedFileId);
                        var fileSize = fileInfo.FileSize;

                        Console.WriteLine($"Downloading file in chunks (size: {fileSize:N0} bytes)...");
                        Console.WriteLine();

                        // Pattern 1: Download file in chunks
                        Console.WriteLine("Pattern 1: Download File in Chunks");
                        Console.WriteLine("------------------------------------");
                        Console.WriteLine();

                        var downloadChunkSize = 1024 * 1024; // 1MB chunks
                        var totalDownloadChunks = (int)Math.Ceiling((double)fileSize / downloadChunkSize);

                        Console.WriteLine($"  Chunk size: {downloadChunkSize:N0} bytes ({downloadChunkSize / 1024.0:F2} KB)");
                        Console.WriteLine($"  Total chunks: {totalDownloadChunks}");
                        Console.WriteLine();

                        var downloadedChunks = new List<byte[]>();
                        var downloadedBytes = 0L;

                        for (int chunkIndex = 0; chunkIndex < totalDownloadChunks; chunkIndex++)
                        {
                            var offset = chunkIndex * downloadChunkSize;
                            var length = Math.Min(downloadChunkSize, (int)(fileSize - offset));

                            Console.WriteLine($"  Downloading chunk {chunkIndex + 1}/{totalDownloadChunks} (offset: {offset:N0}, length: {length:N0})...");

                            // Download chunk using DownloadFileRangeAsync
                            var chunkData = await client.DownloadFileRangeAsync(uploadedFileId, offset, length);

                            downloadedChunks.Add(chunkData);
                            downloadedBytes += chunkData.Length;

                            var progress = (downloadedBytes * 100.0 / fileSize);
                            Console.WriteLine($"    ✓ Chunk {chunkIndex + 1} downloaded: {chunkData.Length:N0} bytes ({progress:F1}%)");
                        }

                        Console.WriteLine();
                        Console.WriteLine($"  ✓ Downloaded {downloadedChunks.Count} chunks");
                        Console.WriteLine($"  ✓ Total downloaded: {downloadedBytes:N0} bytes");
                        Console.WriteLine($"  ✓ Memory usage: ~{downloadChunkSize / 1024.0:F2} KB per chunk");
                        Console.WriteLine();

                        // Pattern 2: Stream chunks to file
                        Console.WriteLine("Pattern 2: Stream Chunks to File");
                        Console.WriteLine("----------------------------------");
                        Console.WriteLine();

                        var streamedFilePath = "streamed_download.txt";
                        Console.WriteLine($"Streaming chunks to file: {streamedFilePath}...");
                        Console.WriteLine();

                        using (var fileStream = new FileStream(streamedFilePath, FileMode.Create, FileAccess.Write, FileShare.None, downloadChunkSize, useAsync: true))
                        {
                            var streamedBytes = 0L;

                            for (int chunkIndex = 0; chunkIndex < totalDownloadChunks; chunkIndex++)
                            {
                                var offset = chunkIndex * downloadChunkSize;
                                var length = Math.Min(downloadChunkSize, (int)(fileSize - offset));

                                // Download chunk
                                var chunkData = await client.DownloadFileRangeAsync(uploadedFileId, offset, length);

                                // Write chunk to file immediately
                                await fileStream.WriteAsync(chunkData, 0, chunkData.Length);

                                streamedBytes += chunkData.Length;
                                var progress = (streamedBytes * 100.0 / fileSize);
                                Console.WriteLine($"  Streamed chunk {chunkIndex + 1}/{totalDownloadChunks}: {chunkData.Length:N0} bytes ({progress:F1}%)");
                            }

                            await fileStream.FlushAsync();
                        }

                        var streamedFileSize = new FileInfo(streamedFilePath).Length;
                        Console.WriteLine();
                        Console.WriteLine($"  ✓ File streamed: {streamedFilePath}");
                        Console.WriteLine($"  ✓ Streamed size: {streamedFileSize:N0} bytes");
                        Console.WriteLine($"  ✓ Memory efficiency: Chunks processed and discarded immediately");
                        Console.WriteLine();

                        // Clean up streamed file
                        if (File.Exists(streamedFilePath))
                        {
                            File.Delete(streamedFilePath);
                        }
                    }

                    // ============================================================
                    // Example 5: Progress Reporting
                    // ============================================================
                    // 
                    // This example demonstrates progress reporting during
                    // streaming operations. Progress reporting provides user
                    // feedback and improves user experience for long-running
                    // operations.
                    // 
                    // Progress reporting patterns:
                    // - Progress callbacks
                    // - Progress events
                    // - Percentage calculation
                    // - Throughput calculation
                    // ============================================================

                    Console.WriteLine("Example 5: Progress Reporting");
                    Console.WriteLine("==============================");
                    Console.WriteLine();

                    // Pattern 1: Progress reporting during chunked upload
                    Console.WriteLine("Pattern 1: Progress Reporting During Chunked Upload");
                    Console.WriteLine("-----------------------------------------------------");
                    Console.WriteLine();

                    var progressChunkSize = 1024 * 1024; // 1MB chunks
                    var progressFileSize = actualFileSize;

                    Console.WriteLine($"Processing file with progress reporting ({progressFileSize / 1024.0 / 1024.0:F2} MB)...");
                    Console.WriteLine();

                    var processedBytesForProgress = 0L;
                    var startTime = DateTime.UtcNow;

                    using (var fileStream = new FileStream(largeTestFile, FileMode.Open, FileAccess.Read, FileShare.Read, progressChunkSize, useAsync: true))
                    {
                        var buffer = new byte[progressChunkSize];
                        int bytesRead;
                        int chunkNumber = 0;

                        while ((bytesRead = await fileStream.ReadAsync(buffer, 0, progressChunkSize)) > 0)
                        {
                            chunkNumber++;
                            processedBytesForProgress += bytesRead;

                            // Calculate progress
                            var progress = (processedBytesForProgress * 100.0 / progressFileSize);
                            var elapsed = DateTime.UtcNow - startTime;
                            var throughput = processedBytesForProgress / elapsed.TotalSeconds;
                            var remainingBytes = progressFileSize - processedBytesForProgress;
                            var estimatedRemaining = remainingBytes / throughput;

                            Console.WriteLine($"  Chunk {chunkNumber}: {bytesRead:N0} bytes");
                            Console.WriteLine($"    Progress: {progress:F1}%");
                            Console.WriteLine($"    Throughput: {throughput / 1024.0 / 1024.0:F2} MB/s");
                            Console.WriteLine($"    Elapsed: {elapsed.TotalSeconds:F1}s");
                            Console.WriteLine($"    Estimated remaining: {estimatedRemaining:F1}s");
                            Console.WriteLine();
                        }
                    }

                    var totalElapsed = DateTime.UtcNow - startTime;
                    var totalThroughput = processedBytesForProgress / totalElapsed.TotalSeconds;

                    Console.WriteLine($"  ✓ Processing completed");
                    Console.WriteLine($"  ✓ Total time: {totalElapsed.TotalSeconds:F1}s");
                    Console.WriteLine($"  ✓ Average throughput: {totalThroughput / 1024.0 / 1024.0:F2} MB/s");
                    Console.WriteLine();

                    // Pattern 2: Progress reporting during chunked download
                    Console.WriteLine("Pattern 2: Progress Reporting During Chunked Download");
                    Console.WriteLine("--------------------------------------------------------");
                    Console.WriteLine();

                    if (uploadedFileId != null)
                    {
                        var downloadFileInfo = await client.GetFileInfoAsync(uploadedFileId);
                        var downloadFileSize = downloadFileInfo.FileSize;

                        Console.WriteLine($"Downloading file with progress reporting ({downloadFileSize / 1024.0 / 1024.0:F2} MB)...");
                        Console.WriteLine();

                        var downloadProgressChunkSize = 1024 * 1024; // 1MB
                        var totalDownloadProgressChunks = (int)Math.Ceiling((double)downloadFileSize / downloadProgressChunkSize);

                        var downloadedBytesForProgress = 0L;
                        var downloadStartTime = DateTime.UtcNow;

                        for (int chunkIndex = 0; chunkIndex < totalDownloadProgressChunks; chunkIndex++)
                        {
                            var offset = chunkIndex * downloadProgressChunkSize;
                            var length = Math.Min(downloadProgressChunkSize, (int)(downloadFileSize - offset));

                            // Download chunk
                            var chunkData = await client.DownloadFileRangeAsync(uploadedFileId, offset, length);

                            downloadedBytesForProgress += chunkData.Length;

                            // Calculate progress
                            var downloadProgress = (downloadedBytesForProgress * 100.0 / downloadFileSize);
                            var downloadElapsed = DateTime.UtcNow - downloadStartTime;
                            var downloadThroughput = downloadedBytesForProgress / downloadElapsed.TotalSeconds;
                            var downloadRemainingBytes = downloadFileSize - downloadedBytesForProgress;
                            var downloadEstimatedRemaining = downloadRemainingBytes / downloadThroughput;

                            Console.WriteLine($"  Chunk {chunkIndex + 1}/{totalDownloadProgressChunks}: {chunkData.Length:N0} bytes");
                            Console.WriteLine($"    Progress: {downloadProgress:F1}%");
                            Console.WriteLine($"    Throughput: {downloadThroughput / 1024.0 / 1024.0:F2} MB/s");
                            Console.WriteLine($"    Elapsed: {downloadElapsed.TotalSeconds:F1}s");
                            Console.WriteLine($"    Estimated remaining: {downloadEstimatedRemaining:F1}s");
                            Console.WriteLine();
                        }

                        var totalDownloadElapsed = DateTime.UtcNow - downloadStartTime;
                        var totalDownloadThroughput = downloadedBytesForProgress / totalDownloadElapsed.TotalSeconds;

                        Console.WriteLine($"  ✓ Download completed");
                        Console.WriteLine($"  ✓ Total time: {totalDownloadElapsed.TotalSeconds:F1}s");
                        Console.WriteLine($"  ✓ Average throughput: {totalDownloadThroughput / 1024.0 / 1024.0:F2} MB/s");
                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 6: Backpressure Handling
                    // ============================================================
                    // 
                    // This example demonstrates backpressure handling in
                    // streaming scenarios. Backpressure occurs when data
                    // production exceeds consumption capacity, and needs to
                    // be managed to prevent memory issues.
                    // 
                    // Backpressure patterns:
                    // - Rate limiting
                    // - Buffering with limits
                    // - Flow control
                    // - Producer-consumer coordination
                    // ============================================================

                    Console.WriteLine("Example 6: Backpressure Handling");
                    Console.WriteLine("==================================");
                    Console.WriteLine();

                    // Pattern 1: Rate limiting to prevent backpressure
                    Console.WriteLine("Pattern 1: Rate Limiting to Prevent Backpressure");
                    Console.WriteLine("--------------------------------------------------");
                    Console.WriteLine();

                    var maxBytesPerSecond = 5 * 1024 * 1024; // 5MB/s rate limit
                    Console.WriteLine($"Processing with rate limit: {maxBytesPerSecond / 1024.0 / 1024.0:F2} MB/s");
                    Console.WriteLine();

                    var rateLimitedBytes = 0L;
                    var rateLimitStartTime = DateTime.UtcNow;

                    using (var fileStream = new FileStream(largeTestFile, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultChunkSize, useAsync: true))
                    {
                        var buffer = new byte[DefaultChunkSize];
                        int bytesRead;

                        while ((bytesRead = await fileStream.ReadAsync(buffer, 0, DefaultChunkSize)) > 0)
                        {
                            // Process chunk
                            rateLimitedBytes += bytesRead;

                            // Calculate current rate
                            var elapsed = DateTime.UtcNow - rateLimitStartTime;
                            var currentRate = rateLimitedBytes / elapsed.TotalSeconds;

                            // Apply rate limiting if needed
                            if (currentRate > maxBytesPerSecond)
                            {
                                var delayNeeded = (rateLimitedBytes / (double)maxBytesPerSecond) - elapsed.TotalSeconds;
                                if (delayNeeded > 0)
                                {
                                    Console.WriteLine($"  Rate limit: {currentRate / 1024.0 / 1024.0:F2} MB/s > {maxBytesPerSecond / 1024.0 / 1024.0:F2} MB/s, delaying {delayNeeded:F2}s");
                                    await Task.Delay(TimeSpan.FromSeconds(delayNeeded));
                                }
                            }

                            var progress = (rateLimitedBytes * 100.0 / actualFileSize);
                            Console.WriteLine($"  Processed: {rateLimitedBytes:N0} bytes ({progress:F1}%) - Rate: {currentRate / 1024.0 / 1024.0:F2} MB/s");
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine($"  ✓ Rate-limited processing completed");
                    Console.WriteLine($"  ✓ Backpressure prevented through rate limiting");
                    Console.WriteLine();

                    // Pattern 2: Buffering with limits
                    Console.WriteLine("Pattern 2: Buffering with Limits");
                    Console.WriteLine("----------------------------------");
                    Console.WriteLine();

                    var maxBufferSize = 10 * 1024 * 1024; // 10MB buffer limit
                    Console.WriteLine($"Processing with buffer limit: {maxBufferSize / 1024.0 / 1024.0:F2} MB");
                    Console.WriteLine();

                    var bufferedChunks = new Queue<byte[]>();
                    var currentBufferSize = 0L;

                    using (var fileStream = new FileStream(largeTestFile, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultChunkSize, useAsync: true))
                    {
                        var buffer = new byte[DefaultChunkSize];
                        int bytesRead;

                        while ((bytesRead = await fileStream.ReadAsync(buffer, 0, DefaultChunkSize)) > 0 || bufferedChunks.Count > 0)
                        {
                            // Read chunk if available
                            if (bytesRead > 0)
                            {
                                var chunk = new byte[bytesRead];
                                Array.Copy(buffer, chunk, bytesRead);

                                // Check buffer limit (backpressure)
                                while (currentBufferSize + chunk.Length > maxBufferSize && bufferedChunks.Count > 0)
                                {
                                    var processedChunk = bufferedChunks.Dequeue();
                                    currentBufferSize -= processedChunk.Length;
                                    Console.WriteLine($"  Buffer full, processing chunk: {processedChunk.Length:N0} bytes (buffer: {currentBufferSize / 1024.0 / 1024.0:F2} MB)");
                                }

                                // Add chunk to buffer
                                bufferedChunks.Enqueue(chunk);
                                currentBufferSize += chunk.Length;
                                Console.WriteLine($"  Buffered chunk: {chunk.Length:N0} bytes (buffer: {currentBufferSize / 1024.0 / 1024.0:F2} MB)");
                            }

                            // Process chunks from buffer
                            while (bufferedChunks.Count > 0)
                            {
                                var chunkToProcess = bufferedChunks.Dequeue();
                                currentBufferSize -= chunkToProcess.Length;

                                // Process chunk (simulated)
                                await Task.Delay(10); // Simulate processing time

                                Console.WriteLine($"  Processed chunk: {chunkToProcess.Length:N0} bytes (buffer: {currentBufferSize / 1024.0 / 1024.0:F2} MB)");
                            }

                            // Continue reading if buffer has space
                            if (bytesRead == 0 && bufferedChunks.Count == 0)
                            {
                                break;
                            }
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine($"  ✓ Buffered processing completed");
                    Console.WriteLine($"  ✓ Backpressure handled through buffer limits");
                    Console.WriteLine();

                    // ============================================================
                    // Example 7: Streaming with Cancellation
                    // ============================================================
                    // 
                    // This example demonstrates streaming operations with
                    // cancellation support. Cancellation is important for
                    // allowing users to stop long-running streaming operations.
                    // 
                    // Cancellation patterns:
                    // - Cancellation token in streaming loops
                    // - Graceful cancellation handling
                    // - Resource cleanup on cancellation
                    // ============================================================

                    Console.WriteLine("Example 7: Streaming with Cancellation");
                    Console.WriteLine("========================================");
                    Console.WriteLine();

                    using (var cancellationCts = new CancellationTokenSource())
                    {
                        try
                        {
                            Console.WriteLine("Starting streaming operation with cancellation support...");
                            Console.WriteLine();

                            var cancelledBytes = 0L;
                            var cancellationChunkSize = 1024 * 1024; // 1MB

                            using (var fileStream = new FileStream(largeTestFile, FileMode.Open, FileAccess.Read, FileShare.Read, cancellationChunkSize, useAsync: true))
                            {
                                var buffer = new byte[cancellationChunkSize];
                                int bytesRead;
                                int chunkNumber = 0;

                                while ((bytesRead = await fileStream.ReadAsync(buffer, 0, cancellationChunkSize, cancellationCts.Token)) > 0)
                                {
                                    chunkNumber++;
                                    cancelledBytes += bytesRead;

                                    // Check cancellation
                                    cancellationCts.Token.ThrowIfCancellationRequested();

                                    var progress = (cancelledBytes * 100.0 / actualFileSize);
                                    Console.WriteLine($"  Chunk {chunkNumber}: {bytesRead:N0} bytes ({progress:F1}%)");

                                    // Simulate cancellation after some progress
                                    if (chunkNumber >= 3)
                                    {
                                        Console.WriteLine("  Cancelling operation...");
                                        cancellationCts.Cancel();
                                        break;
                                    }
                                }
                            }
                        }
                        catch (OperationCanceledException)
                        {
                            Console.WriteLine();
                            Console.WriteLine($"  ✓ Streaming operation cancelled gracefully");
                            Console.WriteLine($"  ✓ Processed {cancelledBytes:N0} bytes before cancellation");
                            Console.WriteLine($"  ✓ Resources cleaned up");
                        }
                    }

                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for streaming
                    // operations in FastDFS applications.
                    // ============================================================

                    Console.WriteLine("Best Practices for Streaming Operations");
                    Console.WriteLine("==========================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Stream Large Files Efficiently:");
                    Console.WriteLine("   - Process files in chunks rather than loading entirely into memory");
                    Console.WriteLine("   - Use appropriate chunk sizes (typically 1-2MB)");
                    Console.WriteLine("   - Stream directly to/from disk when possible");
                    Console.WriteLine("   - Avoid loading entire files into memory");
                    Console.WriteLine();
                    Console.WriteLine("2. Memory-Efficient Operations:");
                    Console.WriteLine("   - Use constant memory patterns for large files");
                    Console.WriteLine("   - Process and discard chunks immediately when possible");
                    Console.WriteLine("   - Monitor memory usage during streaming");
                    Console.WriteLine("   - Use streaming APIs (DownloadToFileAsync) when available");
                    Console.WriteLine();
                    Console.WriteLine("3. Chunked Uploads/Downloads:");
                    Console.WriteLine("   - Use DownloadFileRangeAsync for chunked downloads");
                    Console.WriteLine("   - Process chunks as they are read/written");
                    Console.WriteLine("   - Validate chunks before processing");
                    Console.WriteLine("   - Handle chunk boundaries correctly");
                    Console.WriteLine();
                    Console.WriteLine("4. Progress Reporting:");
                    Console.WriteLine("   - Report progress regularly during streaming");
                    Console.WriteLine("   - Calculate and display throughput");
                    Console.WriteLine("   - Estimate remaining time");
                    Console.WriteLine("   - Provide meaningful progress updates");
                    Console.WriteLine();
                    Console.WriteLine("5. Backpressure Handling:");
                    Console.WriteLine("   - Implement rate limiting when needed");
                    Console.WriteLine("   - Use bounded buffers to prevent memory issues");
                    Console.WriteLine("   - Coordinate producer and consumer rates");
                    Console.WriteLine("   - Monitor buffer sizes and adjust accordingly");
                    Console.WriteLine();
                    Console.WriteLine("6. Streaming with Cancellation:");
                    Console.WriteLine("   - Support cancellation in streaming operations");
                    Console.WriteLine("   - Check cancellation tokens in loops");
                    Console.WriteLine("   - Clean up resources on cancellation");
                    Console.WriteLine("   - Handle OperationCanceledException gracefully");
                    Console.WriteLine();
                    Console.WriteLine("7. Chunk Size Selection:");
                    Console.WriteLine("   - Balance memory usage and network efficiency");
                    Console.WriteLine("   - Typical chunk sizes: 512KB - 2MB");
                    Console.WriteLine("   - Adjust based on available memory and network");
                    Console.WriteLine("   - Test different chunk sizes for optimal performance");
                    Console.WriteLine();
                    Console.WriteLine("8. Error Handling:");
                    Console.WriteLine("   - Handle errors in chunk processing");
                    Console.WriteLine("   - Implement retry logic for failed chunks");
                    Console.WriteLine("   - Clean up partial operations on errors");
                    Console.WriteLine("   - Provide meaningful error messages");
                    Console.WriteLine();
                    Console.WriteLine("9. Performance Optimization:");
                    Console.WriteLine("   - Use async I/O for streaming operations");
                    Console.WriteLine("   - Process chunks in parallel when appropriate");
                    Console.WriteLine("   - Optimize chunk sizes for your use case");
                    Console.WriteLine("   - Monitor and tune streaming performance");
                    Console.WriteLine();
                    Console.WriteLine("10. Best Practices Summary:");
                    Console.WriteLine("    - Stream large files in chunks");
                    Console.WriteLine("    - Maintain constant memory usage");
                    Console.WriteLine("    - Report progress during streaming");
                    Console.WriteLine("    - Handle backpressure appropriately");
                    Console.WriteLine("    - Support cancellation in streaming operations");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up test files and uploaded files
                    // ============================================================

                    Console.WriteLine("Cleaning up...");
                    Console.WriteLine();

                    // Delete local test files
                    var testFiles = new[] { largeTestFile };
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

                    // Delete uploaded files
                    var uploadedFiles = new[] { uploadedFileId, validatedFileId };
                    foreach (var fileId in uploadedFiles)
                    {
                        if (fileId != null)
                        {
                            try
                            {
                                await client.DeleteFileAsync(fileId);
                                Console.WriteLine($"  Deleted uploaded file: {fileId}");
                            }
                            catch
                            {
                                // Ignore deletion errors
                            }
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

        // ====================================================================
        // Helper Methods
        // ====================================================================

        /// <summary>
        /// Creates a large test file for streaming examples.
        /// 
        /// This method creates a file of the specified size by writing
        /// data in chunks to avoid loading the entire file into memory.
        /// </summary>
        /// <param name="filePath">
        /// The path where the test file should be created.
        /// </param>
        /// <param name="fileSize">
        /// The desired size of the test file in bytes.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous file creation operation.
        /// </returns>
        static async Task CreateLargeTestFileAsync(string filePath, long fileSize)
        {
            const int writeChunkSize = 1024 * 1024; // 1MB write chunks
            var writtenBytes = 0L;

            using (var fileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None, writeChunkSize, useAsync: true))
            {
                var buffer = new byte[writeChunkSize];
                var random = new Random();

                while (writtenBytes < fileSize)
                {
                    var remainingBytes = fileSize - writtenBytes;
                    var chunkSize = (int)Math.Min(writeChunkSize, remainingBytes);

                    // Fill buffer with test data
                    random.NextBytes(buffer);

                    await fileStream.WriteAsync(buffer, 0, chunkSize);
                    writtenBytes += chunkSize;
                }

                await fileStream.FlushAsync();
            }
        }
    }
}

