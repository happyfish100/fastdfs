// ============================================================================
// FastDFS C# Client - Partial Download Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates partial download operations in FastDFS, including
// downloading specific byte ranges, resuming interrupted downloads, extracting
// portions of files, streaming large files, and memory-efficient downloads.
// It shows how to efficiently work with large files without loading them
// entirely into memory.
//
// Partial downloads are essential for working with large files efficiently,
// enabling applications to access only the data they need without downloading
// entire files. This is particularly important for memory-constrained
// applications and scenarios where only portions of files are required.
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
    /// Example demonstrating partial download operations in FastDFS.
    /// 
    /// This example shows:
    /// - How to download specific byte ranges from files
    /// - How to resume interrupted downloads
    /// - How to extract portions of files
    /// - How to stream large files efficiently
    /// - How to perform memory-efficient downloads
    /// - Best practices for partial download operations
    /// 
    /// Partial download patterns demonstrated:
    /// 1. Download specific byte ranges
    /// 2. Resume interrupted downloads with checkpoint tracking
    /// 3. Extract file portions (headers, footers, middle sections)
    /// 4. Streaming large files in chunks
    /// 5. Memory-efficient download strategies
    /// </summary>
    class PartialDownloadExample
    {
        /// <summary>
        /// Main entry point for the partial download example.
        /// 
        /// This method demonstrates various partial download patterns through
        /// a series of examples, each showing different aspects of partial
        /// download operations in FastDFS.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Partial Download Example");
            Console.WriteLine("==============================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates partial downloads,");
            Console.WriteLine("range requests, resume capabilities, and memory-efficient operations.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For partial downloads, we configure appropriate timeouts for
            // large file operations.
            // ====================================================================

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
                // For partial downloads, standard connection pool size is sufficient
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                // Standard timeout is usually sufficient
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // For large file partial downloads, longer timeout may be needed
                NetworkTimeout = TimeSpan.FromSeconds(60),  // Longer for large files

                // Idle timeout: time before idle connections are closed
                // Standard idle timeout works well for partial downloads
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                // Retry logic is important for partial downloads to handle
                // transient network errors
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations including partial download operations.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Download Specific Byte Ranges
                    // ============================================================
                    // 
                    // This example demonstrates downloading specific byte ranges
                    // from files. Range downloads are useful when you only need
                    // portions of files, such as file headers, specific sections,
                    // or file metadata.
                    // 
                    // Benefits of range downloads:
                    // - Download only needed data
                    // - Reduce network bandwidth usage
                    // - Faster access to specific file portions
                    // - Lower memory usage
                    // ============================================================

                    Console.WriteLine("Example 1: Download Specific Byte Ranges");
                    Console.WriteLine("==========================================");
                    Console.WriteLine();

                    // Create a test file for range download examples
                    // In a real scenario, this would be an existing file in FastDFS
                    var testFilePath = "range_test_file.txt";

                    // Create a test file with known content
                    // This allows us to verify range downloads correctly
                    var testFileContent = new StringBuilder();
                    for (int i = 0; i < 100; i++)
                    {
                        testFileContent.AppendLine($"Line {i + 1}: This is line number {i + 1} in the test file.");
                    }

                    await File.WriteAllTextAsync(testFilePath, testFileContent.ToString());
                    Console.WriteLine($"Created test file: {testFilePath}");
                    Console.WriteLine($"File size: {new FileInfo(testFilePath).Length} bytes");
                    Console.WriteLine();

                    // Upload the test file to FastDFS
                    Console.WriteLine("Uploading test file to FastDFS...");
                    var testFileId = await client.UploadFileAsync(testFilePath, null);
                    Console.WriteLine($"File uploaded: {testFileId}");
                    Console.WriteLine();

                    // Get file information to know the file size
                    // This is useful for determining valid byte ranges
                    var fileInfo = await client.GetFileInfoAsync(testFileId);
                    Console.WriteLine("File information:");
                    Console.WriteLine($"  File size: {fileInfo.FileSize} bytes");
                    Console.WriteLine($"  Created: {fileInfo.CreateTime}");
                    Console.WriteLine();

                    // Download first 100 bytes (file header)
                    // This is useful for reading file headers, metadata, or
                    // initial content without downloading the entire file
                    Console.WriteLine("Downloading first 100 bytes (file header)...");
                    var headerData = await client.DownloadFileRangeAsync(testFileId, 0, 100);
                    var headerText = Encoding.UTF8.GetString(headerData);
                    Console.WriteLine($"  Downloaded {headerData.Length} bytes");
                    Console.WriteLine($"  Content preview: {headerText.Substring(0, Math.Min(80, headerText.Length))}...");
                    Console.WriteLine();

                    // Download bytes 500-600 (middle section)
                    // This demonstrates downloading a specific section from
                    // the middle of a file
                    Console.WriteLine("Downloading bytes 500-600 (middle section)...");
                    var middleData = await client.DownloadFileRangeAsync(testFileId, 500, 100);
                    var middleText = Encoding.UTF8.GetString(middleData);
                    Console.WriteLine($"  Downloaded {middleData.Length} bytes");
                    Console.WriteLine($"  Content preview: {middleText.Substring(0, Math.Min(80, middleText.Length))}...");
                    Console.WriteLine();

                    // Download last 100 bytes (file footer)
                    // This is useful for reading file footers, end markers,
                    // or final content
                    var footerOffset = fileInfo.FileSize - 100;
                    if (footerOffset > 0)
                    {
                        Console.WriteLine($"Downloading last 100 bytes (file footer, offset {footerOffset})...");
                        var footerData = await client.DownloadFileRangeAsync(testFileId, footerOffset, 100);
                        var footerText = Encoding.UTF8.GetString(footerData);
                        Console.WriteLine($"  Downloaded {footerData.Length} bytes");
                        Console.WriteLine($"  Content preview: {footerText.Substring(0, Math.Min(80, footerText.Length))}...");
                        Console.WriteLine();
                    }

                    // Download from offset to end of file
                    // When length is 0, downloads from offset to end
                    Console.WriteLine("Downloading from offset 1000 to end of file...");
                    var tailData = await client.DownloadFileRangeAsync(testFileId, 1000, 0);
                    Console.WriteLine($"  Downloaded {tailData.Length} bytes (from offset 1000 to end)");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Resume Interrupted Downloads
                    // ============================================================
                    // 
                    // This example demonstrates resuming interrupted downloads
                    // by tracking download progress and continuing from the last
                    // downloaded position. This is essential for large file
                    // downloads that may be interrupted by network issues or
                    // application restarts.
                    // 
                    // Resume download features:
                    // - Checkpoint tracking
                    // - Progress persistence
                    // - Automatic resume on restart
                    // - Partial file handling
                    // ============================================================

                    Console.WriteLine("Example 2: Resume Interrupted Downloads");
                    Console.WriteLine("==========================================");
                    Console.WriteLine();

                    // Create a larger test file for resume download example
                    var resumeTestFilePath = "resume_test_file.txt";
                    var resumeTestContent = new StringBuilder();
                    for (int i = 0; i < 500; i++)
                    {
                        resumeTestContent.AppendLine($"Resume test line {i + 1}: Content for resume download testing.");
                    }

                    await File.WriteAllTextAsync(resumeTestFilePath, resumeTestContent.ToString());
                    Console.WriteLine($"Created resume test file: {resumeTestFilePath}");
                    Console.WriteLine($"File size: {new FileInfo(resumeTestFilePath).Length} bytes");
                    Console.WriteLine();

                    // Upload the resume test file
                    Console.WriteLine("Uploading resume test file to FastDFS...");
                    var resumeTestFileId = await client.UploadFileAsync(resumeTestFilePath, null);
                    Console.WriteLine($"File uploaded: {resumeTestFileId}");
                    Console.WriteLine();

                    // Simulate interrupted download with checkpoint
                    // In a real scenario, the checkpoint would be persisted
                    // to disk or database
                    Console.WriteLine("Simulating interrupted download with resume capability...");
                    Console.WriteLine();

                    var checkpointFile = "download_checkpoint.txt";
                    var outputFile = "resumed_download.txt";
                    long downloadedBytes = 0;
                    const int chunkSize = 1024;  // Download in 1KB chunks

                    // Check if there's an existing checkpoint
                    // This simulates resuming after an interruption
                    if (File.Exists(checkpointFile))
                    {
                        // Resume from checkpoint
                        var checkpointContent = await File.ReadAllTextAsync(checkpointFile);
                        if (long.TryParse(checkpointContent, out downloadedBytes))
                        {
                            Console.WriteLine($"  Resuming download from checkpoint: {downloadedBytes} bytes");
                        }
                    }
                    else
                    {
                        Console.WriteLine("  Starting new download...");
                    }

                    // Get file size
                    var resumeFileInfo = await client.GetFileInfoAsync(resumeTestFileId);
                    var totalFileSize = resumeFileInfo.FileSize;

                    Console.WriteLine($"  Total file size: {totalFileSize} bytes");
                    Console.WriteLine($"  Already downloaded: {downloadedBytes} bytes");
                    Console.WriteLine($"  Remaining: {totalFileSize - downloadedBytes} bytes");
                    Console.WriteLine();

                    // Download remaining data in chunks
                    // This allows resuming from any point in the file
                    using (var outputStream = new FileStream(outputFile, FileMode.Append, FileAccess.Write))
                    {
                        while (downloadedBytes < totalFileSize)
                        {
                            // Calculate chunk size for this iteration
                            var remainingBytes = totalFileSize - downloadedBytes;
                            var currentChunkSize = (int)Math.Min(chunkSize, remainingBytes);

                            Console.WriteLine($"  Downloading chunk: offset {downloadedBytes}, size {currentChunkSize} bytes");

                            try
                            {
                                // Download chunk
                                var chunkData = await client.DownloadFileRangeAsync(
                                    resumeTestFileId,
                                    downloadedBytes,
                                    currentChunkSize);

                                // Write chunk to output file
                                await outputStream.WriteAsync(chunkData, 0, chunkData.Length);
                                downloadedBytes += chunkData.Length;

                                // Update checkpoint
                                // In production, persist checkpoint to reliable storage
                                await File.WriteAllTextAsync(checkpointFile, downloadedBytes.ToString());

                                Console.WriteLine($"    Downloaded {chunkData.Length} bytes, total: {downloadedBytes}/{totalFileSize} bytes " +
                                                 $"({(downloadedBytes * 100.0 / totalFileSize):F1}%)");

                                // Simulate interruption after first chunk for demonstration
                                // In real scenario, interruption would be due to network error, etc.
                                if (downloadedBytes == currentChunkSize)
                                {
                                    Console.WriteLine("  Simulating download interruption...");
                                    break;  // Simulate interruption
                                }
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"  Error downloading chunk: {ex.Message}");
                                Console.WriteLine($"  Checkpoint saved at {downloadedBytes} bytes");
                                throw;
                            }
                        }
                    }

                    // Resume download after interruption
                    Console.WriteLine();
                    Console.WriteLine("Resuming download after interruption...");
                    Console.WriteLine();

                    // Read checkpoint
                    if (File.Exists(checkpointFile))
                    {
                        var checkpointContent = await File.ReadAllTextAsync(checkpointFile);
                        downloadedBytes = long.Parse(checkpointContent);
                        Console.WriteLine($"  Resuming from checkpoint: {downloadedBytes} bytes");
                    }

                    // Continue downloading remaining data
                    using (var outputStream = new FileStream(outputFile, FileMode.Append, FileAccess.Write))
                    {
                        while (downloadedBytes < totalFileSize)
                        {
                            var remainingBytes = totalFileSize - downloadedBytes;
                            var currentChunkSize = (int)Math.Min(chunkSize, remainingBytes);

                            Console.WriteLine($"  Downloading chunk: offset {downloadedBytes}, size {currentChunkSize} bytes");

                            var chunkData = await client.DownloadFileRangeAsync(
                                resumeTestFileId,
                                downloadedBytes,
                                currentChunkSize);

                            await outputStream.WriteAsync(chunkData, 0, chunkData.Length);
                            downloadedBytes += chunkData.Length;

                            // Update checkpoint
                            await File.WriteAllTextAsync(checkpointFile, downloadedBytes.ToString());

                            Console.WriteLine($"    Downloaded {chunkData.Length} bytes, total: {downloadedBytes}/{totalFileSize} bytes " +
                                             $"({(downloadedBytes * 100.0 / totalFileSize):F1}%)");
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine("Download completed successfully!");
                    Console.WriteLine($"  Total downloaded: {downloadedBytes} bytes");
                    Console.WriteLine();

                    // Verify downloaded file
                    if (File.Exists(outputFile))
                    {
                        var downloadedFileSize = new FileInfo(outputFile).Length;
                        Console.WriteLine($"  Downloaded file size: {downloadedFileSize} bytes");
                        Console.WriteLine($"  Original file size: {totalFileSize} bytes");
                        Console.WriteLine($"  Match: {downloadedFileSize == totalFileSize}");
                        Console.WriteLine();
                    }

                    // Clean up checkpoint file
                    if (File.Exists(checkpointFile))
                    {
                        File.Delete(checkpointFile);
                        Console.WriteLine("  Checkpoint file cleaned up");
                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 3: Extract Portions of Files
                    // ============================================================
                    // 
                    // This example demonstrates extracting specific portions
                    // of files, such as headers, footers, or middle sections.
                    // This is useful for file format analysis, metadata extraction,
                    // or processing specific file regions.
                    // 
                    // Extraction patterns:
                    // - File header extraction
                    // - File footer extraction
                    // - Middle section extraction
                    // - Multiple range extraction
                    // ============================================================

                    Console.WriteLine("Example 3: Extract Portions of Files");
                    Console.WriteLine("====================================");
                    Console.WriteLine();

                    // Create a structured test file for extraction
                    var extractTestFilePath = "extract_test_file.bin";
                    var extractTestContent = new byte[2048];

                    // Create structured content: header (256 bytes) + body (1536 bytes) + footer (256 bytes)
                    Encoding.UTF8.GetBytes("FILE_HEADER_START").CopyTo(extractTestContent, 0);
                    for (int i = 16; i < 240; i++)
                    {
                        extractTestContent[i] = (byte)(i % 256);
                    }
                    Encoding.UTF8.GetBytes("FILE_HEADER_END").CopyTo(extractTestContent, 240);

                    // Body content
                    for (int i = 256; i < 1792; i++)
                    {
                        extractTestContent[i] = (byte)((i * 7) % 256);
                    }

                    // Footer content
                    Encoding.UTF8.GetBytes("FILE_FOOTER_START").CopyTo(extractTestContent, 1792);
                    for (int i = 1808; i < 2032; i++)
                    {
                        extractTestContent[i] = (byte)(i % 256);
                    }
                    Encoding.UTF8.GetBytes("FILE_FOOTER_END").CopyTo(extractTestContent, 2032);

                    await File.WriteAllBytesAsync(extractTestFilePath, extractTestContent);
                    Console.WriteLine($"Created extraction test file: {extractTestFilePath}");
                    Console.WriteLine($"File size: {extractTestContent.Length} bytes");
                    Console.WriteLine();

                    // Upload extraction test file
                    Console.WriteLine("Uploading extraction test file to FastDFS...");
                    var extractTestFileId = await client.UploadFileAsync(extractTestFilePath, null);
                    Console.WriteLine($"File uploaded: {extractTestFileId}");
                    Console.WriteLine();

                    // Extract file header (first 256 bytes)
                    Console.WriteLine("Extracting file header (first 256 bytes)...");
                    var extractedHeader = await client.DownloadFileRangeAsync(extractTestFileId, 0, 256);
                    var headerString = Encoding.UTF8.GetString(extractedHeader.Take(16).ToArray());
                    Console.WriteLine($"  Extracted {extractedHeader.Length} bytes");
                    Console.WriteLine($"  Header marker: {headerString}");
                    Console.WriteLine();

                    // Extract file body (middle 1536 bytes)
                    Console.WriteLine("Extracting file body (bytes 256-1792)...");
                    var extractedBody = await client.DownloadFileRangeAsync(extractTestFileId, 256, 1536);
                    Console.WriteLine($"  Extracted {extractedBody.Length} bytes");
                    Console.WriteLine($"  Body content range: {extractedBody[0]}-{extractedBody[extractedBody.Length - 1]}");
                    Console.WriteLine();

                    // Extract file footer (last 256 bytes)
                    Console.WriteLine("Extracting file footer (last 256 bytes)...");
                    var footerOffset = extractTestContent.Length - 256;
                    var extractedFooter = await client.DownloadFileRangeAsync(extractTestFileId, footerOffset, 256);
                    var footerString = Encoding.UTF8.GetString(extractedFooter.Take(16).ToArray());
                    Console.WriteLine($"  Extracted {extractedFooter.Length} bytes");
                    Console.WriteLine($"  Footer marker: {footerString}");
                    Console.WriteLine();

                    // Extract multiple non-contiguous ranges
                    // This demonstrates extracting multiple separate portions
                    Console.WriteLine("Extracting multiple non-contiguous ranges...");
                    var range1 = await client.DownloadFileRangeAsync(extractTestFileId, 0, 100);
                    var range2 = await client.DownloadFileRangeAsync(extractTestFileId, 500, 100);
                    var range3 = await client.DownloadFileRangeAsync(extractTestFileId, 1000, 100);
                    Console.WriteLine($"  Extracted range 1: {range1.Length} bytes (offset 0)");
                    Console.WriteLine($"  Extracted range 2: {range2.Length} bytes (offset 500)");
                    Console.WriteLine($"  Extracted range 3: {range3.Length} bytes (offset 1000)");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Streaming Large Files
                    // ============================================================
                    // 
                    // This example demonstrates streaming large files in chunks
                    // to avoid loading entire files into memory. Streaming is
                    // essential for processing large files efficiently without
                    // exhausting available memory.
                    // 
                    // Streaming benefits:
                    // - Memory-efficient processing
                    // - Constant memory usage regardless of file size
                    // - Ability to process files larger than available memory
                    // - Real-time processing capabilities
                    // ============================================================

                    Console.WriteLine("Example 4: Streaming Large Files");
                    Console.WriteLine("==================================");
                    Console.WriteLine();

                    // Create a large test file for streaming
                    var streamTestFilePath = "stream_test_file.txt";
                    var streamTestContent = new StringBuilder();
                    
                    // Create a file with many lines to simulate large file
                    for (int i = 0; i < 1000; i++)
                    {
                        streamTestContent.AppendLine($"Stream test line {i + 1}: " +
                                                     $"This is a line in a large file for streaming demonstration. " +
                                                     $"Line number {i + 1} contains data for testing streaming operations.");
                    }

                    await File.WriteAllTextAsync(streamTestFilePath, streamTestContent.ToString());
                    Console.WriteLine($"Created streaming test file: {streamTestFilePath}");
                    Console.WriteLine($"File size: {new FileInfo(streamTestFilePath).Length:N0} bytes");
                    Console.WriteLine();

                    // Upload streaming test file
                    Console.WriteLine("Uploading streaming test file to FastDFS...");
                    var streamTestFileId = await client.UploadFileAsync(streamTestFilePath, null);
                    Console.WriteLine($"File uploaded: {streamTestFileId}");
                    Console.WriteLine();

                    // Stream file in chunks
                    // This processes the file in small chunks without loading
                    // the entire file into memory
                    Console.WriteLine("Streaming file in chunks (processing without loading entire file)...");
                    Console.WriteLine();

                    var streamFileInfo = await client.GetFileInfoAsync(streamTestFileId);
                    var streamTotalSize = streamFileInfo.FileSize;
                    const int streamChunkSize = 2048;  // 2KB chunks
                    var streamOffset = 0L;
                    var streamChunkCount = 0;
                    var totalProcessedBytes = 0L;

                    // Process file in streaming chunks
                    while (streamOffset < streamTotalSize)
                    {
                        var remainingBytes = streamTotalSize - streamOffset;
                        var currentChunkSize = (int)Math.Min(streamChunkSize, remainingBytes);

                        // Download chunk
                        var streamChunk = await client.DownloadFileRangeAsync(
                            streamTestFileId,
                            streamOffset,
                            currentChunkSize);

                        // Process chunk (in real scenario, this would be actual processing)
                        // For demonstration, we'll just count lines in the chunk
                        var chunkText = Encoding.UTF8.GetString(streamChunk);
                        var lineCount = chunkText.Split('\n').Length - 1;

                        streamChunkCount++;
                        totalProcessedBytes += streamChunk.Length;
                        streamOffset += streamChunk.Length;

                        // Report progress
                        if (streamChunkCount % 10 == 0 || streamOffset >= streamTotalSize)
                        {
                            var progress = (totalProcessedBytes * 100.0 / streamTotalSize);
                            Console.WriteLine($"  Processed chunk {streamChunkCount}: " +
                                             $"{totalProcessedBytes:N0}/{streamTotalSize:N0} bytes ({progress:F1}%) - " +
                                             $"{lineCount} lines in chunk");
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine("Streaming completed!");
                    Console.WriteLine($"  Total chunks processed: {streamChunkCount}");
                    Console.WriteLine($"  Total bytes processed: {totalProcessedBytes:N0}");
                    Console.WriteLine($"  Memory-efficient: Processed without loading entire file");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Memory-Efficient Downloads
                    // ============================================================
                    // 
                    // This example demonstrates memory-efficient download
                    // strategies for large files. Memory efficiency is crucial
                    // for applications that need to handle large files without
                    // exhausting available memory.
                    // 
                    // Memory-efficient strategies:
                    // - Chunked downloads
                    // - Streaming to disk
                    // - Processing while downloading
                    // - Avoiding full file loading
                    // ============================================================

                    Console.WriteLine("Example 5: Memory-Efficient Downloads");
                    Console.WriteLine("=====================================");
                    Console.WriteLine();

                    // Create a test file for memory-efficient download
                    var memoryTestFilePath = "memory_test_file.txt";
                    var memoryTestContent = new StringBuilder();
                    
                    // Create a moderately large file
                    for (int i = 0; i < 2000; i++)
                    {
                        memoryTestContent.AppendLine($"Memory efficiency test line {i + 1}: " +
                                                     $"This line contains data for testing memory-efficient downloads.");
                    }

                    await File.WriteAllTextAsync(memoryTestFilePath, memoryTestContent.ToString());
                    Console.WriteLine($"Created memory test file: {memoryTestFilePath}");
                    Console.WriteLine($"File size: {new FileInfo(memoryTestFilePath).Length:N0} bytes");
                    Console.WriteLine();

                    // Upload memory test file
                    Console.WriteLine("Uploading memory test file to FastDFS...");
                    var memoryTestFileId = await client.UploadFileAsync(memoryTestFilePath, null);
                    Console.WriteLine($"File uploaded: {memoryTestFileId}");
                    Console.WriteLine();

                    // Memory-efficient download: Stream directly to file
                    // This avoids loading the entire file into memory
                    Console.WriteLine("Memory-efficient download: Streaming directly to file...");
                    Console.WriteLine();

                    var memoryFileInfo = await client.GetFileInfoAsync(memoryTestFileId);
                    var memoryTotalSize = memoryFileInfo.FileSize;
                    const int memoryChunkSize = 4096;  // 4KB chunks
                    var memoryOffset = 0L;
                    var memoryOutputFile = "memory_efficient_download.txt";

                    // Delete output file if it exists
                    if (File.Exists(memoryOutputFile))
                    {
                        File.Delete(memoryOutputFile);
                    }

                    // Download in chunks and write directly to file
                    // This keeps memory usage constant regardless of file size
                    using (var memoryOutputStream = new FileStream(memoryOutputFile, FileMode.Create, FileAccess.Write))
                    {
                        var memoryChunkCount = 0;

                        while (memoryOffset < memoryTotalSize)
                        {
                            var remainingBytes = memoryTotalSize - memoryOffset;
                            var currentChunkSize = (int)Math.Min(memoryChunkSize, remainingBytes);

                            // Download chunk
                            var memoryChunk = await client.DownloadFileRangeAsync(
                                memoryTestFileId,
                                memoryOffset,
                                currentChunkSize);

                            // Write chunk directly to file
                            // This avoids accumulating data in memory
                            await memoryOutputStream.WriteAsync(memoryChunk, 0, memoryChunk.Length);

                            memoryOffset += memoryChunk.Length;
                            memoryChunkCount++;

                            // Report progress periodically
                            if (memoryChunkCount % 50 == 0 || memoryOffset >= memoryTotalSize)
                            {
                                var progress = (memoryOffset * 100.0 / memoryTotalSize);
                                Console.WriteLine($"  Downloaded chunk {memoryChunkCount}: " +
                                                 $"{memoryOffset:N0}/{memoryTotalSize:N0} bytes ({progress:F1}%)");
                            }
                        }
                    }

                    Console.WriteLine();
                    Console.WriteLine("Memory-efficient download completed!");
                    Console.WriteLine($"  Output file: {memoryOutputFile}");
                    Console.WriteLine($"  File size: {new FileInfo(memoryOutputFile).Length:N0} bytes");
                    Console.WriteLine($"  Memory usage: Constant (chunk-based processing)");
                    Console.WriteLine();

                    // Compare with full file download (memory-intensive)
                    // This demonstrates the memory difference
                    Console.WriteLine("Comparison: Full file download (memory-intensive)...");
                    Console.WriteLine();

                    var fullDownloadStopwatch = System.Diagnostics.Stopwatch.StartNew();
                    var fullFileData = await client.DownloadFileAsync(memoryTestFileId);
                    fullDownloadStopwatch.Stop();

                    Console.WriteLine($"  Full download time: {fullDownloadStopwatch.ElapsedMilliseconds} ms");
                    Console.WriteLine($"  Memory usage: {fullFileData.Length:N0} bytes in memory");
                    Console.WriteLine($"  Memory-efficient method: Constant memory usage");
                    Console.WriteLine($"  Full download method: {fullFileData.Length:N0} bytes in memory");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for partial download
                    // operations in FastDFS applications, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for Partial Downloads");
                    Console.WriteLine("======================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Byte Range Downloads:");
                    Console.WriteLine("   - Use DownloadFileRangeAsync for specific byte ranges");
                    Console.WriteLine("   - Validate offset and length before downloading");
                    Console.WriteLine("   - Handle range errors gracefully");
                    Console.WriteLine("   - Consider file size limits when calculating ranges");
                    Console.WriteLine();
                    Console.WriteLine("2. Resume Interrupted Downloads:");
                    Console.WriteLine("   - Implement checkpoint tracking for large files");
                    Console.WriteLine("   - Persist checkpoints to reliable storage");
                    Console.WriteLine("   - Resume from last successful position");
                    Console.WriteLine("   - Verify downloaded data integrity");
                    Console.WriteLine("   - Handle checkpoint corruption scenarios");
                    Console.WriteLine();
                    Console.WriteLine("3. File Portion Extraction:");
                    Console.WriteLine("   - Extract only needed portions of files");
                    Console.WriteLine("   - Use appropriate chunk sizes for extraction");
                    Console.WriteLine("   - Combine multiple ranges when needed");
                    Console.WriteLine("   - Validate extracted data");
                    Console.WriteLine();
                    Console.WriteLine("4. Streaming Large Files:");
                    Console.WriteLine("   - Use chunked downloads for large files");
                    Console.WriteLine("   - Process data while downloading");
                    Console.WriteLine("   - Maintain constant memory usage");
                    Console.WriteLine("   - Choose appropriate chunk sizes");
                    Console.WriteLine("   - Monitor memory usage during streaming");
                    Console.WriteLine();
                    Console.WriteLine("5. Memory Efficiency:");
                    Console.WriteLine("   - Avoid loading entire large files into memory");
                    Console.WriteLine("   - Stream directly to disk when possible");
                    Console.WriteLine("   - Use chunk-based processing");
                    Console.WriteLine("   - Monitor memory usage in production");
                    Console.WriteLine("   - Consider file size vs available memory");
                    Console.WriteLine();
                    Console.WriteLine("6. Chunk Size Selection:");
                    Console.WriteLine("   - Balance between network efficiency and memory usage");
                    Console.WriteLine("   - Smaller chunks: Lower memory, more requests");
                    Console.WriteLine("   - Larger chunks: Higher memory, fewer requests");
                    Console.WriteLine("   - Typical chunk sizes: 1KB - 64KB");
                    Console.WriteLine("   - Test different sizes for your use case");
                    Console.WriteLine();
                    Console.WriteLine("7. Error Handling:");
                    Console.WriteLine("   - Handle range errors appropriately");
                    Console.WriteLine("   - Retry failed chunk downloads");
                    Console.WriteLine("   - Validate downloaded data");
                    Console.WriteLine("   - Handle network interruptions gracefully");
                    Console.WriteLine("   - Implement proper checkpoint recovery");
                    Console.WriteLine();
                    Console.WriteLine("8. Performance Optimization:");
                    Console.WriteLine("   - Use appropriate chunk sizes for your network");
                    Console.WriteLine("   - Consider parallel chunk downloads for large files");
                    Console.WriteLine("   - Cache frequently accessed ranges");
                    Console.WriteLine("   - Monitor download performance");
                    Console.WriteLine("   - Optimize based on actual usage patterns");
                    Console.WriteLine();
                    Console.WriteLine("9. Progress Tracking:");
                    Console.WriteLine("   - Track download progress for large files");
                    Console.WriteLine("   - Provide user feedback during downloads");
                    Console.WriteLine("   - Calculate estimated time remaining");
                    Console.WriteLine("   - Persist progress for resume capability");
                    Console.WriteLine();
                    Console.WriteLine("10. Best Practices Summary:");
                    Console.WriteLine("    - Use range downloads for partial file access");
                    Console.WriteLine("    - Implement resume capability for large files");
                    Console.WriteLine("    - Stream large files to avoid memory issues");
                    Console.WriteLine("    - Choose appropriate chunk sizes");
                    Console.WriteLine("    - Monitor memory usage and optimize");
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
                    var uploadedFileIds = new List<string>
                    {
                        testFileId,
                        resumeTestFileId,
                        extractTestFileId,
                        streamTestFileId,
                        memoryTestFileId
                    };

                    Console.WriteLine("Deleting uploaded files from FastDFS...");
                    foreach (var fileId in uploadedFileIds)
                    {
                        try
                        {
                            await client.DeleteFileAsync(fileId);
                            Console.WriteLine($"  Deleted: {fileId}");
                        }
                        catch
                        {
                            // Ignore deletion errors
                        }
                    }

                    Console.WriteLine();

                    // Delete local test files
                    var localTestFiles = new List<string>
                    {
                        testFilePath,
                        resumeTestFilePath,
                        outputFile,
                        extractTestFilePath,
                        streamTestFilePath,
                        memoryTestFilePath,
                        memoryOutputFile
                    };

                    Console.WriteLine("Deleting local test files...");
                    foreach (var fileName in localTestFiles)
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

