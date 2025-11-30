// ============================================================================
// FastDFS C# Client - File Information Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates file information operations in FastDFS, including
// getting detailed file information, accessing file size, creation time,
// CRC32 checksum, source server information, and use cases for validation,
// monitoring, and auditing. It shows how to retrieve and utilize file
// metadata for various application scenarios.
//
// File information is essential for applications that need to validate files,
// monitor file storage, audit file operations, or make decisions based on
// file characteristics. Understanding file information helps build robust
// and reliable applications.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating file information operations in FastDFS.
    /// 
    /// This example shows:
    /// - How to get detailed file information from FastDFS
    /// - How to access file size, creation time, and CRC32 checksum
    /// - How to retrieve source server information
    /// - Use cases for file information (validation, monitoring, auditing)
    /// - Best practices for file information operations
    /// 
    /// File information patterns demonstrated:
    /// 1. Basic file information retrieval
    /// 2. File size validation and checking
    /// 3. Creation time analysis and usage
    /// 4. CRC32 checksum verification
    /// 5. Source server information usage
    /// 6. Validation use cases
    /// 7. Monitoring use cases
    /// 8. Auditing use cases
    /// </summary>
    class FileInfoExample
    {
        /// <summary>
        /// Main entry point for the file information example.
        /// 
        /// This method demonstrates various file information patterns through
        /// a series of examples, each showing different aspects of file
        /// information operations and use cases in FastDFS.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - File Information Example");
            Console.WriteLine("==============================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates file information operations,");
            Console.WriteLine("including file size, creation time, CRC32, and source server info.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For file information operations, standard configuration is sufficient.
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
                // Standard connection pool size is sufficient for file info operations
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                // Standard timeout is usually sufficient
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // File info operations are typically fast, so standard timeout works
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Idle timeout: time before idle connections are closed
                // Standard idle timeout works well for file info operations
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                // Retry logic is important for file info operations to handle
                // transient network errors
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations including file information retrieval.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Get Detailed File Information
                    // ============================================================
                    // 
                    // This example demonstrates how to retrieve detailed file
                    // information from FastDFS. File information includes file
                    // size, creation time, CRC32 checksum, and source server
                    // information, which are essential for file validation,
                    // monitoring, and auditing.
                    // 
                    // File information components:
                    // - FileSize: Size of the file in bytes
                    // - CreateTime: When the file was created
                    // - CRC32: Checksum for integrity verification
                    // - SourceIPAddr: IP address of the source storage server
                    // ============================================================

                    Console.WriteLine("Example 1: Get Detailed File Information");
                    Console.WriteLine("===========================================");
                    Console.WriteLine();

                    // Create a test file for file information examples
                    // In a real scenario, this would be an existing file in FastDFS
                    var testFilePath = "fileinfo_test.txt";

                    // Create a test file with known content
                    // This allows us to verify file information correctly
                    var testFileContent = "This is a test file for file information examples. " +
                                         "It contains sample content to demonstrate file info operations. " +
                                         "File information includes size, creation time, CRC32, and source server.";

                    await File.WriteAllTextAsync(testFilePath, testFileContent);
                    Console.WriteLine($"Created test file: {testFilePath}");
                    Console.WriteLine($"Local file size: {new FileInfo(testFilePath).Length} bytes");
                    Console.WriteLine();

                    // Upload the test file to FastDFS
                    // After upload, we can retrieve file information
                    Console.WriteLine("Uploading test file to FastDFS...");
                    var testFileId = await client.UploadFileAsync(testFilePath, null);
                    Console.WriteLine($"File uploaded: {testFileId}");
                    Console.WriteLine();

                    // Get detailed file information
                    // GetFileInfoAsync retrieves comprehensive information about the file
                    // including size, creation time, CRC32 checksum, and source server
                    Console.WriteLine("Retrieving detailed file information...");
                    Console.WriteLine();

                    var fileInfo = await client.GetFileInfoAsync(testFileId);

                    // Display all file information components
                    // Each component provides valuable information for different use cases
                    Console.WriteLine("File Information Details:");
                    Console.WriteLine("==========================");
                    Console.WriteLine();

                    // File Size Information
                    // File size is useful for validation, storage planning, and monitoring
                    Console.WriteLine("1. File Size:");
                    Console.WriteLine($"   Size: {fileInfo.FileSize} bytes");
                    Console.WriteLine($"   Size (KB): {fileInfo.FileSize / 1024.0:F2} KB");
                    Console.WriteLine($"   Size (MB): {fileInfo.FileSize / (1024.0 * 1024.0):F2} MB");
                    Console.WriteLine($"   Use cases: Storage planning, quota management, size validation");
                    Console.WriteLine();

                    // Creation Time Information
                    // Creation time is useful for auditing, lifecycle management, and monitoring
                    Console.WriteLine("2. Creation Time:");
                    Console.WriteLine($"   Created: {fileInfo.CreateTime}");
                    Console.WriteLine($"   Created (UTC): {fileInfo.CreateTime.ToUniversalTime()}");
                    Console.WriteLine($"   Created (Local): {fileInfo.CreateTime.ToLocalTime()}");
                    Console.WriteLine($"   Age: {DateTime.UtcNow - fileInfo.CreateTime.ToUniversalTime()}");
                    Console.WriteLine($"   Use cases: Auditing, lifecycle management, retention policies");
                    Console.WriteLine();

                    // CRC32 Checksum Information
                    // CRC32 is useful for integrity verification and corruption detection
                    Console.WriteLine("3. CRC32 Checksum:");
                    Console.WriteLine($"   CRC32: 0x{fileInfo.CRC32:X8}");
                    Console.WriteLine($"   CRC32 (decimal): {fileInfo.CRC32}");
                    Console.WriteLine($"   Use cases: Integrity verification, corruption detection, validation");
                    Console.WriteLine();

                    // Source Server Information
                    // Source server information is useful for monitoring, troubleshooting, and auditing
                    Console.WriteLine("4. Source Server Information:");
                    Console.WriteLine($"   Source IP: {fileInfo.SourceIPAddr}");
                    Console.WriteLine($"   Use cases: Monitoring, troubleshooting, server tracking");
                    Console.WriteLine();

                    // Complete file information summary
                    // The ToString method provides a convenient summary
                    Console.WriteLine("Complete File Information Summary:");
                    Console.WriteLine($"   {fileInfo}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: File Size Validation and Checking
                    // ============================================================
                    // 
                    // This example demonstrates using file size information for
                    // validation and checking purposes. File size validation is
                    // important for ensuring files meet size requirements, managing
                    // storage quotas, and preventing oversized file uploads.
                    // 
                    // File size validation use cases:
                    // - Size limit enforcement
                    // - Storage quota management
                    // - File size verification
                    // - Storage planning
                    // ============================================================

                    Console.WriteLine("Example 2: File Size Validation and Checking");
                    Console.WriteLine("==============================================");
                    Console.WriteLine();

                    // Create files of different sizes for validation examples
                    Console.WriteLine("Creating test files of different sizes...");
                    Console.WriteLine();

                    var smallFilePath = "small_file.txt";
                    var mediumFilePath = "medium_file.txt";
                    var largeFilePath = "large_file.txt";

                    // Small file (under 1KB)
                    var smallContent = "Small file content";
                    await File.WriteAllTextAsync(smallFilePath, smallContent);
                    Console.WriteLine($"Created small file: {smallFilePath} ({new FileInfo(smallFilePath).Length} bytes)");

                    // Medium file (1KB - 100KB)
                    var mediumContent = new StringBuilder();
                    for (int i = 0; i < 1000; i++)
                    {
                        mediumContent.AppendLine($"Medium file line {i + 1}: Content for medium file testing.");
                    }
                    await File.WriteAllTextAsync(mediumFilePath, mediumContent.ToString());
                    Console.WriteLine($"Created medium file: {mediumFilePath} ({new FileInfo(mediumFilePath).Length:N0} bytes)");

                    // Large file (over 100KB)
                    var largeContent = new StringBuilder();
                    for (int i = 0; i < 10000; i++)
                    {
                        largeContent.AppendLine($"Large file line {i + 1}: Content for large file testing.");
                    }
                    await File.WriteAllTextAsync(largeFilePath, largeContent.ToString());
                    Console.WriteLine($"Created large file: {largeFilePath} ({new FileInfo(largeFilePath).Length:N0} bytes)");
                    Console.WriteLine();

                    // Upload files and get their information
                    Console.WriteLine("Uploading files and retrieving file information...");
                    Console.WriteLine();

                    var smallFileId = await client.UploadFileAsync(smallFilePath, null);
                    var mediumFileId = await client.UploadFileAsync(mediumFilePath, null);
                    var largeFileId = await client.UploadFileAsync(largeFilePath, null);

                    var smallFileInfo = await client.GetFileInfoAsync(smallFileId);
                    var mediumFileInfo = await client.GetFileInfoAsync(mediumFileId);
                    var largeFileInfo = await client.GetFileInfoAsync(largeFileId);

                    // File size validation examples
                    Console.WriteLine("File Size Validation Examples:");
                    Console.WriteLine("=================================");
                    Console.WriteLine();

                    // Validate small file size
                    const long smallFileMaxSize = 1024;  // 1KB
                    Console.WriteLine($"Small file validation (max size: {smallFileMaxSize} bytes):");
                    Console.WriteLine($"  File size: {smallFileInfo.FileSize} bytes");
                    Console.WriteLine($"  Within limit: {smallFileInfo.FileSize <= smallFileMaxSize}");
                    Console.WriteLine($"  Validation result: {(smallFileInfo.FileSize <= smallFileMaxSize ? "PASS" : "FAIL")}");
                    Console.WriteLine();

                    // Validate medium file size
                    const long mediumFileMinSize = 1024;   // 1KB
                    const long mediumFileMaxSize = 102400;  // 100KB
                    Console.WriteLine($"Medium file validation (range: {mediumFileMinSize}-{mediumFileMaxSize} bytes):");
                    Console.WriteLine($"  File size: {mediumFileInfo.FileSize:N0} bytes");
                    Console.WriteLine($"  Within range: {mediumFileInfo.FileSize >= mediumFileMinSize && mediumFileInfo.FileSize <= mediumFileMaxSize}");
                    Console.WriteLine($"  Validation result: {(mediumFileInfo.FileSize >= mediumFileMinSize && mediumFileInfo.FileSize <= mediumFileMaxSize ? "PASS" : "FAIL")}");
                    Console.WriteLine();

                    // Validate large file size
                    const long largeFileMinSize = 102400;  // 100KB
                    Console.WriteLine($"Large file validation (min size: {largeFileMinSize} bytes):");
                    Console.WriteLine($"  File size: {largeFileInfo.FileSize:N0} bytes");
                    Console.WriteLine($"  Meets minimum: {largeFileInfo.FileSize >= largeFileMinSize}");
                    Console.WriteLine($"  Validation result: {(largeFileInfo.FileSize >= largeFileMinSize ? "PASS" : "FAIL")}");
                    Console.WriteLine();

                    // Storage quota management example
                    // Calculate total storage used by files
                    var totalStorageUsed = smallFileInfo.FileSize + mediumFileInfo.FileSize + largeFileInfo.FileSize;
                    const long storageQuota = 1024 * 1024;  // 1MB quota

                    Console.WriteLine("Storage Quota Management:");
                    Console.WriteLine($"  Small file: {smallFileInfo.FileSize:N0} bytes");
                    Console.WriteLine($"  Medium file: {mediumFileInfo.FileSize:N0} bytes");
                    Console.WriteLine($"  Large file: {largeFileInfo.FileSize:N0} bytes");
                    Console.WriteLine($"  Total used: {totalStorageUsed:N0} bytes ({totalStorageUsed / (1024.0 * 1024.0):F2} MB)");
                    Console.WriteLine($"  Quota limit: {storageQuota:N0} bytes ({storageQuota / (1024.0 * 1024.0):F2} MB)");
                    Console.WriteLine($"  Quota remaining: {storageQuota - totalStorageUsed:N0} bytes");
                    Console.WriteLine($"  Quota usage: {(totalStorageUsed * 100.0 / storageQuota):F1}%");
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Creation Time Analysis and Usage
                    // ============================================================
                    // 
                    // This example demonstrates using creation time information
                    // for various purposes such as auditing, lifecycle management,
                    // retention policies, and file age analysis.
                    // 
                    // Creation time use cases:
                    // - File age calculation
                    // - Retention policy enforcement
                    // - Lifecycle management
                    // - Auditing and compliance
                    // - File expiration tracking
                    // ============================================================

                    Console.WriteLine("Example 3: Creation Time Analysis and Usage");
                    Console.WriteLine("=============================================");
                    Console.WriteLine();

                    // Create files at different times for time analysis
                    Console.WriteLine("Creating files for time analysis...");
                    Console.WriteLine();

                    var timeTestFilePath = "time_test_file.txt";
                    var timeTestContent = "File for creation time analysis";
                    await File.WriteAllTextAsync(timeTestFilePath, timeTestContent);

                    // Upload file and get creation time
                    var timeTestFileId = await client.UploadFileAsync(timeTestFilePath, null);
                    var timeTestFileInfo = await client.GetFileInfoAsync(timeTestFileId);

                    // Creation time analysis
                    Console.WriteLine("Creation Time Analysis:");
                    Console.WriteLine("=======================");
                    Console.WriteLine();

                    var creationTime = timeTestFileInfo.CreateTime;
                    var currentTime = DateTime.UtcNow;
                    var fileAge = currentTime - creationTime.ToUniversalTime();

                    Console.WriteLine($"File creation time: {creationTime}");
                    Console.WriteLine($"Current time (UTC): {currentTime}");
                    Console.WriteLine($"File age: {fileAge}");
                    Console.WriteLine($"File age (days): {fileAge.TotalDays:F2} days");
                    Console.WriteLine($"File age (hours): {fileAge.TotalHours:F2} hours");
                    Console.WriteLine($"File age (minutes): {fileAge.TotalMinutes:F2} minutes");
                    Console.WriteLine();

                    // Retention policy example
                    // Check if file should be retained based on age
                    var retentionPeriod = TimeSpan.FromDays(30);  // 30-day retention
                    var shouldRetain = fileAge < retentionPeriod;

                    Console.WriteLine("Retention Policy Check:");
                    Console.WriteLine($"  Retention period: {retentionPeriod.Days} days");
                    Console.WriteLine($"  File age: {fileAge.TotalDays:F2} days");
                    Console.WriteLine($"  Should retain: {shouldRetain}");
                    Console.WriteLine($"  Action: {(shouldRetain ? "Keep file" : "Archive or delete file")}");
                    Console.WriteLine();

                    // Lifecycle management example
                    // Categorize files by age
                    Console.WriteLine("File Lifecycle Categorization:");
                    var ageInDays = fileAge.TotalDays;
                    string lifecycleStage;

                    if (ageInDays < 7)
                    {
                        lifecycleStage = "Recent (less than 7 days)";
                    }
                    else if (ageInDays < 30)
                    {
                        lifecycleStage = "Active (7-30 days)";
                    }
                    else if (ageInDays < 90)
                    {
                        lifecycleStage = "Mature (30-90 days)";
                    }
                    else
                    {
                        lifecycleStage = "Archive (over 90 days)";
                    }

                    Console.WriteLine($"  File age: {ageInDays:F1} days");
                    Console.WriteLine($"  Lifecycle stage: {lifecycleStage}");
                    Console.WriteLine();

                    // File expiration tracking example
                    // Check if file has expired based on creation time
                    var expirationPeriod = TimeSpan.FromDays(60);  // 60-day expiration
                    var expirationDate = creationTime.Add(expirationPeriod);
                    var isExpired = currentTime > expirationDate;

                    Console.WriteLine("File Expiration Tracking:");
                    Console.WriteLine($"  Creation date: {creationTime}");
                    Console.WriteLine($"  Expiration period: {expirationPeriod.Days} days");
                    Console.WriteLine($"  Expiration date: {expirationDate}");
                    Console.WriteLine($"  Is expired: {isExpired}");
                    Console.WriteLine($"  Days until expiration: {(isExpired ? 0 : (expirationDate - currentTime).TotalDays):F1} days");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: CRC32 Checksum Verification
                    // ============================================================
                    // 
                    // This example demonstrates using CRC32 checksum for file
                    // integrity verification. CRC32 checksums can detect file
                    // corruption, transmission errors, or unauthorized modifications.
                    // 
                    // CRC32 verification use cases:
                    // - File integrity verification
                    // - Corruption detection
                    // - Data validation
                    // - Transmission error detection
                    // ============================================================

                    Console.WriteLine("Example 4: CRC32 Checksum Verification");
                    Console.WriteLine("========================================");
                    Console.WriteLine();

                    // Create a test file for CRC32 verification
                    Console.WriteLine("Creating test file for CRC32 verification...");
                    Console.WriteLine();

                    var crc32TestFilePath = "crc32_test_file.txt";
                    var crc32TestContent = "This is a test file for CRC32 checksum verification. " +
                                          "The CRC32 checksum can be used to verify file integrity.";

                    await File.WriteAllTextAsync(crc32TestFilePath, crc32TestContent);

                    // Calculate local file CRC32
                    // This allows us to compare with FastDFS CRC32
                    var localCrc32 = CalculateCRC32(File.ReadAllBytes(crc32TestFilePath));
                    Console.WriteLine($"Local file CRC32: 0x{localCrc32:X8}");
                    Console.WriteLine();

                    // Upload file and get CRC32 from FastDFS
                    Console.WriteLine("Uploading file and retrieving CRC32 from FastDFS...");
                    var crc32TestFileId = await client.UploadFileAsync(crc32TestFilePath, null);
                    var crc32TestFileInfo = await client.GetFileInfoAsync(crc32TestFileId);

                    Console.WriteLine($"FastDFS file CRC32: 0x{crc32TestFileInfo.CRC32:X8}");
                    Console.WriteLine();

                    // Compare CRC32 values
                    // Matching CRC32 values indicate file integrity
                    var crc32Match = localCrc32 == crc32TestFileInfo.CRC32;

                    Console.WriteLine("CRC32 Verification:");
                    Console.WriteLine("====================");
                    Console.WriteLine($"  Local CRC32: 0x{localCrc32:X8}");
                    Console.WriteLine($"  FastDFS CRC32: 0x{crc32TestFileInfo.CRC32:X8}");
                    Console.WriteLine($"  Match: {crc32Match}");
                    Console.WriteLine($"  Verification result: {(crc32Match ? "PASS - File integrity verified" : "FAIL - File integrity check failed")}");
                    Console.WriteLine();

                    // Download file and verify CRC32 again
                    // This demonstrates ongoing integrity verification
                    Console.WriteLine("Downloading file and verifying CRC32 again...");
                    var downloadedData = await client.DownloadFileAsync(crc32TestFileId);
                    var downloadedCrc32 = CalculateCRC32(downloadedData);

                    var downloadCrc32Match = downloadedCrc32 == crc32TestFileInfo.CRC32;

                    Console.WriteLine($"  Downloaded file CRC32: 0x{downloadedCrc32:X8}");
                    Console.WriteLine($"  FastDFS CRC32: 0x{crc32TestFileInfo.CRC32:X8}");
                    Console.WriteLine($"  Match: {downloadCrc32Match}");
                    Console.WriteLine($"  Verification result: {(downloadCrc32Match ? "PASS - Download integrity verified" : "FAIL - Download integrity check failed")}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Source Server Information Usage
                    // ============================================================
                    // 
                    // This example demonstrates using source server information
                    // for monitoring, troubleshooting, and auditing purposes.
                    // Source server information helps track where files are
                    // stored and can be useful for load balancing and server
                    // management.
                    // 
                    // Source server use cases:
                    // - Server tracking and monitoring
                    // - Troubleshooting file access issues
                    // - Load balancing analysis
                    // - Server health monitoring
                    // - Audit trail maintenance
                    // ============================================================

                    Console.WriteLine("Example 5: Source Server Information Usage");
                    Console.WriteLine("============================================");
                    Console.WriteLine();

                    // Create multiple test files to demonstrate server tracking
                    Console.WriteLine("Creating multiple test files for server tracking...");
                    Console.WriteLine();

                    var serverTestFiles = new List<string>();
                    for (int i = 1; i <= 5; i++)
                    {
                        var serverTestFilePath = $"server_test_{i}.txt";
                        var serverTestContent = $"Server test file {i} for source server tracking";
                        await File.WriteAllTextAsync(serverTestFilePath, serverTestContent);
                        serverTestFiles.Add(serverTestFilePath);
                    }

                    Console.WriteLine($"Created {serverTestFiles.Count} test files");
                    Console.WriteLine();

                    // Upload files and track source servers
                    Console.WriteLine("Uploading files and tracking source servers...");
                    Console.WriteLine();

                    var fileServerMap = new Dictionary<string, string>();

                    foreach (var serverTestFile in serverTestFiles)
                    {
                        var serverTestFileId = await client.UploadFileAsync(serverTestFile, null);
                        var serverTestFileInfo = await client.GetFileInfoAsync(serverTestFileId);
                        
                        fileServerMap[serverTestFileId] = serverTestFileInfo.SourceIPAddr;
                        
                        Console.WriteLine($"  File: {serverTestFile}");
                        Console.WriteLine($"    File ID: {serverTestFileId}");
                        Console.WriteLine($"    Source server: {serverTestFileInfo.SourceIPAddr}");
                        Console.WriteLine();
                    }

                    // Analyze source server distribution
                    // This helps understand load distribution across servers
                    Console.WriteLine("Source Server Distribution Analysis:");
                    Console.WriteLine("======================================");
                    Console.WriteLine();

                    var serverDistribution = fileServerMap.Values
                        .GroupBy(ip => ip)
                        .Select(g => new { Server = g.Key, FileCount = g.Count() })
                        .OrderByDescending(x => x.FileCount)
                        .ToList();

                    foreach (var server in serverDistribution)
                    {
                        var percentage = (server.FileCount * 100.0 / fileServerMap.Count);
                        Console.WriteLine($"  Server {server.Server}: {server.FileCount} files ({percentage:F1}%)");
                    }

                    Console.WriteLine();
                    Console.WriteLine($"  Total files: {fileServerMap.Count}");
                    Console.WriteLine($"  Unique servers: {serverDistribution.Count}");
                    Console.WriteLine();

                    // Server health monitoring example
                    // Track files per server for health monitoring
                    Console.WriteLine("Server Health Monitoring:");
                    Console.WriteLine("==========================");
                    Console.WriteLine();

                    foreach (var server in serverDistribution)
                    {
                        // In a real scenario, you would check server health
                        // For demonstration, we'll just show the file count
                        var serverHealth = "Healthy";  // Would be determined by actual health checks
                        
                        Console.WriteLine($"  Server: {server.Server}");
                        Console.WriteLine($"    Files stored: {server.FileCount}");
                        Console.WriteLine($"    Health status: {serverHealth}");
                        Console.WriteLine($"    Monitoring: Active");
                        Console.WriteLine();
                    }

                    // ============================================================
                    // Example 6: Validation Use Cases
                    // ============================================================
                    // 
                    // This example demonstrates using file information for
                    // validation purposes. File validation ensures files meet
                    // requirements and are suitable for their intended use.
                    // 
                    // Validation use cases:
                    // - File size validation
                    // - File age validation
                    // - Integrity validation
                    // - Format validation
                    // - Compliance validation
                    // ============================================================

                    Console.WriteLine("Example 6: Validation Use Cases");
                    Console.WriteLine("=================================");
                    Console.WriteLine();

                    // Create a file for validation examples
                    Console.WriteLine("Creating file for validation examples...");
                    Console.WriteLine();

                    var validationTestFilePath = "validation_test_file.txt";
                    var validationTestContent = "This is a test file for validation use cases. " +
                                               "It demonstrates how file information can be used for validation.";

                    await File.WriteAllTextAsync(validationTestFilePath, validationTestContent);

                    // Upload and get file information
                    var validationTestFileId = await client.UploadFileAsync(validationTestFilePath, null);
                    var validationTestFileInfo = await client.GetFileInfoAsync(validationTestFileId);

                    // Comprehensive file validation
                    Console.WriteLine("Comprehensive File Validation:");
                    Console.WriteLine("===============================");
                    Console.WriteLine();

                    var validationResults = new List<ValidationResult>();

                    // Size validation
                    const long minSize = 100;   // Minimum 100 bytes
                    const long maxSize = 10000; // Maximum 10KB
                    var sizeValid = validationTestFileInfo.FileSize >= minSize && 
                                   validationTestFileInfo.FileSize <= maxSize;
                    
                    validationResults.Add(new ValidationResult
                    {
                        Check = "Size Validation",
                        Passed = sizeValid,
                        Details = $"Size: {validationTestFileInfo.FileSize} bytes (range: {minSize}-{maxSize})"
                    });

                    // Age validation (file should be recent)
                    var maxAge = TimeSpan.FromDays(1);  // File should be less than 1 day old
                    var fileAge = DateTime.UtcNow - validationTestFileInfo.CreateTime.ToUniversalTime();
                    var ageValid = fileAge < maxAge;
                    
                    validationResults.Add(new ValidationResult
                    {
                        Check = "Age Validation",
                        Passed = ageValid,
                        Details = $"Age: {fileAge.TotalHours:F2} hours (max: {maxAge.TotalHours} hours)"
                    });

                    // Integrity validation (CRC32 check)
                    var downloadedValidationData = await client.DownloadFileAsync(validationTestFileId);
                    var downloadedValidationCrc32 = CalculateCRC32(downloadedValidationData);
                    var integrityValid = downloadedValidationCrc32 == validationTestFileInfo.CRC32;
                    
                    validationResults.Add(new ValidationResult
                    {
                        Check = "Integrity Validation",
                        Passed = integrityValid,
                        Details = $"CRC32 match: {integrityValid}"
                    });

                    // Source server validation
                    var serverValid = !string.IsNullOrEmpty(validationTestFileInfo.SourceIPAddr);
                    
                    validationResults.Add(new ValidationResult
                    {
                        Check = "Source Server Validation",
                        Passed = serverValid,
                        Details = $"Source server: {validationTestFileInfo.SourceIPAddr ?? "Unknown"}"
                    });

                    // Display validation results
                    Console.WriteLine("Validation Results:");
                    foreach (var result in validationResults)
                    {
                        var status = result.Passed ? "PASS" : "FAIL";
                        Console.WriteLine($"  {result.Check}: {status}");
                        Console.WriteLine($"    {result.Details}");
                    }

                    Console.WriteLine();
                    var allValid = validationResults.All(r => r.Passed);
                    Console.WriteLine($"Overall Validation: {(allValid ? "PASS" : "FAIL")}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 7: Monitoring Use Cases
                    // ============================================================
                    // 
                    // This example demonstrates using file information for
                    // monitoring purposes. File monitoring helps track file
                    // storage, usage patterns, and system health.
                    // 
                    // Monitoring use cases:
                    // - Storage usage monitoring
                    // - File count monitoring
                    // - Server distribution monitoring
                    // - File age monitoring
                    // - Health monitoring
                    // ============================================================

                    Console.WriteLine("Example 7: Monitoring Use Cases");
                    Console.WriteLine("==================================");
                    Console.WriteLine();

                    // Create multiple files for monitoring examples
                    Console.WriteLine("Creating files for monitoring examples...");
                    Console.WriteLine();

                    var monitoringFiles = new List<string>();
                    for (int i = 1; i <= 10; i++)
                    {
                        var monitoringFilePath = $"monitoring_file_{i}.txt";
                        var monitoringContent = $"Monitoring test file {i}";
                        await File.WriteAllTextAsync(monitoringFilePath, monitoringContent);
                        monitoringFiles.Add(monitoringFilePath);
                    }

                    // Upload files and collect monitoring data
                    Console.WriteLine("Uploading files and collecting monitoring data...");
                    Console.WriteLine();

                    var monitoringData = new List<FileInfo>();

                    foreach (var monitoringFile in monitoringFiles)
                    {
                        var monitoringFileId = await client.UploadFileAsync(monitoringFile, null);
                        var monitoringFileInfo = await client.GetFileInfoAsync(monitoringFileId);
                        monitoringData.Add(monitoringFileInfo);
                    }

                    // Storage usage monitoring
                    Console.WriteLine("Storage Usage Monitoring:");
                    Console.WriteLine("=========================");
                    Console.WriteLine();

                    var totalStorage = monitoringData.Sum(fi => fi.FileSize);
                    var averageFileSize = monitoringData.Average(fi => fi.FileSize);
                    var largestFile = monitoringData.OrderByDescending(fi => fi.FileSize).First();
                    var smallestFile = monitoringData.OrderBy(fi => fi.FileSize).First();

                    Console.WriteLine($"  Total files: {monitoringData.Count}");
                    Console.WriteLine($"  Total storage: {totalStorage:N0} bytes ({totalStorage / 1024.0:F2} KB)");
                    Console.WriteLine($"  Average file size: {averageFileSize:F0} bytes");
                    Console.WriteLine($"  Largest file: {largestFile.FileSize} bytes");
                    Console.WriteLine($"  Smallest file: {smallestFile.FileSize} bytes");
                    Console.WriteLine();

                    // File age monitoring
                    Console.WriteLine("File Age Monitoring:");
                    Console.WriteLine("=====================");
                    Console.WriteLine();

                    var currentTime = DateTime.UtcNow;
                    var oldestFile = monitoringData.OrderBy(fi => fi.CreateTime).First();
                    var newestFile = monitoringData.OrderByDescending(fi => fi.CreateTime).First();
                    var averageAge = monitoringData.Average(fi => (currentTime - fi.CreateTime.ToUniversalTime()).TotalDays);

                    Console.WriteLine($"  Oldest file age: {(currentTime - oldestFile.CreateTime.ToUniversalTime()).TotalDays:F2} days");
                    Console.WriteLine($"  Newest file age: {(currentTime - newestFile.CreateTime.ToUniversalTime()).TotalDays:F2} days");
                    Console.WriteLine($"  Average file age: {averageAge:F2} days");
                    Console.WriteLine();

                    // Server distribution monitoring
                    Console.WriteLine("Server Distribution Monitoring:");
                    Console.WriteLine("===============================");
                    Console.WriteLine();

                    var serverCounts = monitoringData
                        .GroupBy(fi => fi.SourceIPAddr)
                        .Select(g => new { Server = g.Key, Count = g.Count(), TotalSize = g.Sum(fi => fi.FileSize) })
                        .OrderByDescending(x => x.Count)
                        .ToList();

                    foreach (var server in serverCounts)
                    {
                        var percentage = (server.Count * 100.0 / monitoringData.Count);
                        Console.WriteLine($"  Server {server.Server}:");
                        Console.WriteLine($"    Files: {server.Count} ({percentage:F1}%)");
                        Console.WriteLine($"    Storage: {server.TotalSize:N0} bytes");
                    }

                    Console.WriteLine();

                    // ============================================================
                    // Example 8: Auditing Use Cases
                    // ============================================================
                    // 
                    // This example demonstrates using file information for
                    // auditing purposes. File auditing helps maintain compliance,
                    // track file operations, and provide audit trails.
                    // 
                    // Auditing use cases:
                    // - File operation audit trails
                    // - Compliance reporting
                    // - Access tracking
                    // - Change tracking
                    // - Retention compliance
                    // ============================================================

                    Console.WriteLine("Example 8: Auditing Use Cases");
                    Console.WriteLine("===============================");
                    Console.WriteLine();

                    // Create files for auditing examples
                    Console.WriteLine("Creating files for auditing examples...");
                    Console.WriteLine();

                    var auditTestFilePath = "audit_test_file.txt";
                    var auditTestContent = "This is a test file for auditing use cases. " +
                                         "It demonstrates how file information can be used for auditing.";

                    await File.WriteAllTextAsync(auditTestFilePath, auditTestContent);

                    // Upload and get file information for audit
                    var auditTestFileId = await client.UploadFileAsync(auditTestFilePath, null);
                    var auditTestFileInfo = await client.GetFileInfoAsync(auditTestFileId);

                    // Create audit record
                    Console.WriteLine("File Operation Audit Record:");
                    Console.WriteLine("============================");
                    Console.WriteLine();

                    var auditRecord = new AuditRecord
                    {
                        Operation = "File Upload",
                        FileId = auditTestFileId,
                        Timestamp = DateTime.UtcNow,
                        FileSize = auditTestFileInfo.FileSize,
                        CreationTime = auditTestFileInfo.CreateTime,
                        CRC32 = auditTestFileInfo.CRC32,
                        SourceServer = auditTestFileInfo.SourceIPAddr,
                        User = "system",  // In real scenario, this would be actual user
                        Details = "File uploaded for auditing example"
                    };

                    Console.WriteLine("Audit Record Details:");
                    Console.WriteLine($"  Operation: {auditRecord.Operation}");
                    Console.WriteLine($"  Timestamp: {auditRecord.Timestamp}");
                    Console.WriteLine($"  File ID: {auditRecord.FileId}");
                    Console.WriteLine($"  File Size: {auditRecord.FileSize} bytes");
                    Console.WriteLine($"  Creation Time: {auditRecord.CreationTime}");
                    Console.WriteLine($"  CRC32: 0x{auditRecord.CRC32:X8}");
                    Console.WriteLine($"  Source Server: {auditRecord.SourceServer}");
                    Console.WriteLine($"  User: {auditRecord.User}");
                    Console.WriteLine($"  Details: {auditRecord.Details}");
                    Console.WriteLine();

                    // Compliance reporting example
                    Console.WriteLine("Compliance Reporting:");
                    Console.WriteLine("======================");
                    Console.WriteLine();

                    // Check compliance with various policies
                    var complianceChecks = new List<ComplianceCheck>();

                    // Size compliance
                    var sizeCompliant = auditTestFileInfo.FileSize <= 10485760;  // 10MB limit
                    complianceChecks.Add(new ComplianceCheck
                    {
                        Policy = "File Size Limit",
                        Compliant = sizeCompliant,
                        Details = $"File size: {auditTestFileInfo.FileSize} bytes (limit: 10MB)"
                    });

                    // Age compliance (retention policy)
                    var retentionCompliant = (DateTime.UtcNow - auditTestFileInfo.CreateTime.ToUniversalTime()) < TimeSpan.FromDays(365);
                    complianceChecks.Add(new ComplianceCheck
                    {
                        Policy = "Retention Policy",
                        Compliant = retentionCompliant,
                        Details = $"File age: {(DateTime.UtcNow - auditTestFileInfo.CreateTime.ToUniversalTime()).TotalDays:F1} days (limit: 365 days)"
                    });

                    // Integrity compliance
                    var integrityCompliant = auditTestFileInfo.CRC32 != 0;  // CRC32 should be non-zero
                    complianceChecks.Add(new ComplianceCheck
                    {
                        Policy = "Integrity Check",
                        Compliant = integrityCompliant,
                        Details = $"CRC32: 0x{auditTestFileInfo.CRC32:X8}"
                    });

                    // Display compliance results
                    Console.WriteLine("Compliance Check Results:");
                    foreach (var check in complianceChecks)
                    {
                        var status = check.Compliant ? "COMPLIANT" : "NON-COMPLIANT";
                        Console.WriteLine($"  {check.Policy}: {status}");
                        Console.WriteLine($"    {check.Details}");
                    }

                    Console.WriteLine();
                    var allCompliant = complianceChecks.All(c => c.Compliant);
                    Console.WriteLine($"Overall Compliance: {(allCompliant ? "COMPLIANT" : "NON-COMPLIANT")}");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for file information
                    // operations in FastDFS applications, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for File Information Operations");
                    Console.WriteLine("==============================================");
                    Console.WriteLine();
                    Console.WriteLine("1. File Information Retrieval:");
                    Console.WriteLine("   - Use GetFileInfoAsync for comprehensive file information");
                    Console.WriteLine("   - Cache file information when appropriate");
                    Console.WriteLine("   - Handle file not found errors gracefully");
                    Console.WriteLine("   - Validate file IDs before querying information");
                    Console.WriteLine();
                    Console.WriteLine("2. File Size Usage:");
                    Console.WriteLine("   - Use file size for storage planning and quotas");
                    Console.WriteLine("   - Validate file sizes against limits");
                    Console.WriteLine("   - Monitor total storage usage");
                    Console.WriteLine("   - Track file size changes over time");
                    Console.WriteLine();
                    Console.WriteLine("3. Creation Time Usage:");
                    Console.WriteLine("   - Use creation time for lifecycle management");
                    Console.WriteLine("   - Implement retention policies based on age");
                    Console.WriteLine("   - Track file age for expiration");
                    Console.WriteLine("   - Use for auditing and compliance");
                    Console.WriteLine();
                    Console.WriteLine("4. CRC32 Checksum Usage:");
                    Console.WriteLine("   - Verify file integrity using CRC32");
                    Console.WriteLine("   - Compare CRC32 before and after operations");
                    Console.WriteLine("   - Detect corruption and transmission errors");
                    Console.WriteLine("   - Use for data validation");
                    Console.WriteLine();
                    Console.WriteLine("5. Source Server Information:");
                    Console.WriteLine("   - Track files per server for monitoring");
                    Console.WriteLine("   - Use for load balancing analysis");
                    Console.WriteLine("   - Troubleshoot server-specific issues");
                    Console.WriteLine("   - Monitor server health and distribution");
                    Console.WriteLine();
                    Console.WriteLine("6. Validation Patterns:");
                    Console.WriteLine("   - Implement comprehensive file validation");
                    Console.WriteLine("   - Check size, age, and integrity");
                    Console.WriteLine("   - Validate against business rules");
                    Console.WriteLine("   - Provide clear validation feedback");
                    Console.WriteLine();
                    Console.WriteLine("7. Monitoring Patterns:");
                    Console.WriteLine("   - Track storage usage and file counts");
                    Console.WriteLine("   - Monitor file age distribution");
                    Console.WriteLine("   - Track server distribution");
                    Console.WriteLine("   - Set up alerts for anomalies");
                    Console.WriteLine();
                    Console.WriteLine("8. Auditing Patterns:");
                    Console.WriteLine("   - Maintain audit trails for file operations");
                    Console.WriteLine("   - Record all relevant file information");
                    Console.WriteLine("   - Implement compliance checking");
                    Console.WriteLine("   - Store audit records securely");
                    Console.WriteLine();
                    Console.WriteLine("9. Performance Considerations:");
                    Console.WriteLine("   - File info operations are typically fast");
                    Console.WriteLine("   - Cache file information when possible");
                    Console.WriteLine("   - Batch file info queries when appropriate");
                    Console.WriteLine("   - Monitor query performance");
                    Console.WriteLine();
                    Console.WriteLine("10. Best Practices Summary:");
                    Console.WriteLine("    - Use file information for validation and monitoring");
                    Console.WriteLine("    - Implement proper error handling");
                    Console.WriteLine("    - Cache information when appropriate");
                    Console.WriteLine("    - Use for auditing and compliance");
                    Console.WriteLine("    - Monitor and optimize based on usage");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up uploaded files and local test files
                    // ============================================================

                    Console.WriteLine("Cleaning up...");
                    Console.WriteLine();

                    // Collect all uploaded file IDs
                    var allUploadedFileIds = new List<string>
                    {
                        testFileId,
                        smallFileId,
                        mediumFileId,
                        largeFileId,
                        timeTestFileId,
                        crc32TestFileId,
                        validationTestFileId,
                        auditTestFileId
                    };

                    // Add server test file IDs
                    foreach (var serverTestFile in serverTestFiles)
                    {
                        try
                        {
                            // Find file ID (in real scenario, you'd track these)
                            // For cleanup, we'll skip files we can't identify
                        }
                        catch
                        {
                            // Ignore
                        }
                    }

                    // Add monitoring file IDs
                    // (In real scenario, you'd track these during upload)

                    Console.WriteLine("Deleting uploaded files from FastDFS...");
                    foreach (var fileId in allUploadedFileIds)
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
                    var allLocalFiles = new List<string>
                    {
                        testFilePath,
                        smallFilePath,
                        mediumFilePath,
                        largeFilePath,
                        timeTestFilePath,
                        crc32TestFilePath,
                        validationTestFilePath,
                        auditTestFilePath
                    };

                    allLocalFiles.AddRange(serverTestFiles);
                    allLocalFiles.AddRange(monitoringFiles);

                    Console.WriteLine("Deleting local test files...");
                    foreach (var fileName in allLocalFiles.Distinct())
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

        // ====================================================================
        // Helper Methods
        // ====================================================================

        /// <summary>
        /// Calculates CRC32 checksum for a byte array.
        /// 
        /// This method computes the CRC32 checksum using the standard
        /// CRC32 algorithm, which matches the FastDFS CRC32 implementation.
        /// </summary>
        /// <param name="data">
        /// The byte array to calculate CRC32 for.
        /// </param>
        /// <returns>
        /// The CRC32 checksum as a 32-bit unsigned integer.
        /// </returns>
        static uint CalculateCRC32(byte[] data)
        {
            // CRC32 polynomial (standard)
            const uint polynomial = 0xEDB88320;
            var crcTable = new uint[256];

            // Build CRC32 lookup table
            for (uint i = 0; i < 256; i++)
            {
                uint crc = i;
                for (int j = 0; j < 8; j++)
                {
                    if ((crc & 1) != 0)
                    {
                        crc = (crc >> 1) ^ polynomial;
                    }
                    else
                    {
                        crc >>= 1;
                    }
                }
                crcTable[i] = crc;
            }

            // Calculate CRC32
            uint crc32 = 0xFFFFFFFF;
            foreach (byte b in data)
            {
                crc32 = (crc32 >> 8) ^ crcTable[(crc32 ^ b) & 0xFF];
            }

            return crc32 ^ 0xFFFFFFFF;
        }
    }

    // ====================================================================
    // Helper Classes
    // ====================================================================

    /// <summary>
    /// Represents a validation result for file validation operations.
    /// 
    /// This class contains information about a single validation check,
    /// including the check name, whether it passed, and details about
    /// the validation result.
    /// </summary>
    class ValidationResult
    {
        /// <summary>
        /// Gets or sets the name of the validation check.
        /// </summary>
        public string Check { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether the validation check passed.
        /// </summary>
        public bool Passed { get; set; }

        /// <summary>
        /// Gets or sets details about the validation result.
        /// </summary>
        public string Details { get; set; }
    }

    /// <summary>
    /// Represents an audit record for file operations.
    /// 
    /// This class contains comprehensive information about a file operation
    /// for auditing purposes, including file information, operation details,
    /// and user information.
    /// </summary>
    class AuditRecord
    {
        /// <summary>
        /// Gets or sets the operation type (e.g., "File Upload", "File Download").
        /// </summary>
        public string Operation { get; set; }

        /// <summary>
        /// Gets or sets the file ID that was operated on.
        /// </summary>
        public string FileId { get; set; }

        /// <summary>
        /// Gets or sets the timestamp when the operation occurred.
        /// </summary>
        public DateTime Timestamp { get; set; }

        /// <summary>
        /// Gets or sets the file size in bytes.
        /// </summary>
        public long FileSize { get; set; }

        /// <summary>
        /// Gets or sets the file creation time.
        /// </summary>
        public DateTime CreationTime { get; set; }

        /// <summary>
        /// Gets or sets the CRC32 checksum of the file.
        /// </summary>
        public uint CRC32 { get; set; }

        /// <summary>
        /// Gets or sets the source server IP address.
        /// </summary>
        public string SourceServer { get; set; }

        /// <summary>
        /// Gets or sets the user who performed the operation.
        /// </summary>
        public string User { get; set; }

        /// <summary>
        /// Gets or sets additional details about the operation.
        /// </summary>
        public string Details { get; set; }
    }

    /// <summary>
    /// Represents a compliance check result.
    /// 
    /// This class contains information about a compliance check, including
    /// the policy being checked, whether the file is compliant, and details
    /// about the compliance status.
    /// </summary>
    class ComplianceCheck
    {
        /// <summary>
        /// Gets or sets the name of the compliance policy.
        /// </summary>
        public string Policy { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether the file is compliant with the policy.
        /// </summary>
        public bool Compliant { get; set; }

        /// <summary>
        /// Gets or sets details about the compliance check result.
        /// </summary>
        public string Details { get; set; }
    }
}

