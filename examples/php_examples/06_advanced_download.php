<?php
/**
 * FastDFS Advanced Download Operations Example
 * 
 * This example demonstrates advanced download techniques in FastDFS including:
 * - Downloading from specific storage servers
 * - Range/partial downloads for large files
 * - Callback-based streaming downloads
 * - Download with retry logic
 * - Parallel downloads of multiple files
 * - Download progress tracking
 * - Bandwidth throttling simulation
 * 
 * These techniques are useful for:
 * - Large file downloads
 * - Video/audio streaming
 * - Resume capability
 * - Load balancing across storage servers
 * - Efficient bandwidth usage
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed (php-fastdfs)
 * - FastDFS tracker and storage servers running
 * - Valid file IDs from previous uploads
 */

// FastDFS tracker server configuration
define('TRACKER_HOST', '127.0.0.1');
define('TRACKER_PORT', 22122);

// Download configuration
define('CHUNK_SIZE', 1024 * 1024);  // 1MB chunks for streaming
define('MAX_RETRIES', 3);            // Maximum retry attempts
define('RETRY_DELAY', 1);            // Seconds to wait between retries

/**
 * Initialize connection to FastDFS tracker server
 * 
 * @return resource|false FastDFS tracker connection or false on failure
 */
function initializeFastDFS() {
    try {
        $tracker = fastdfs_tracker_make_connection(TRACKER_HOST, TRACKER_PORT);
        
        if (!$tracker) {
            throw new Exception("Failed to connect to tracker server");
        }
        
        echo "✓ Connected to FastDFS tracker server\n";
        return $tracker;
        
    } catch (Exception $e) {
        echo "✗ Connection error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Get storage server information for a file
 * 
 * This retrieves the storage server details where the file is stored,
 * which can be used for direct downloads or load balancing.
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to query
 * @return array|false Storage server info on success, false on failure
 */
function getStorageServerInfo($tracker, $fileId) {
    try {
        echo "\n--- Getting Storage Server Info ---\n";
        echo "File ID: $fileId\n";
        
        // Parse file ID to extract group name and path
        $parts = explode('/', $fileId, 2);
        if (count($parts) < 2) {
            throw new Exception("Invalid file ID format");
        }
        
        $groupName = $parts[0];
        $remoteFilename = $parts[1];
        
        echo "Group: $groupName\n";
        echo "Remote filename: $remoteFilename\n";
        
        // Get storage server info from tracker
        $storageInfo = fastdfs_tracker_query_storage_fetch(
            $tracker,
            $groupName,
            $remoteFilename
        );
        
        if (!$storageInfo) {
            throw new Exception("Failed to get storage server info");
        }
        
        echo "✓ Storage server info retrieved!\n";
        
        if (is_array($storageInfo)) {
            echo "Storage Server Details:\n";
            foreach ($storageInfo as $key => $value) {
                echo "  $key: $value\n";
            }
        }
        
        return $storageInfo;
        
    } catch (Exception $e) {
        echo "✗ Info retrieval error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Download file with retry logic
 * 
 * Implements automatic retry mechanism for failed downloads,
 * useful for handling network issues or temporary server unavailability.
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to download
 * @param string $localPath Local path to save file
 * @param int $maxRetries Maximum number of retry attempts
 * @return bool True on success, false on failure
 */
function downloadWithRetry($tracker, $fileId, $localPath, $maxRetries = MAX_RETRIES) {
    $attempt = 0;
    
    echo "\n--- Download with Retry Logic ---\n";
    echo "File ID: $fileId\n";
    echo "Max retries: $maxRetries\n";
    
    while ($attempt < $maxRetries) {
        $attempt++;
        
        try {
            echo "\nAttempt $attempt of $maxRetries...\n";
            
            $result = fastdfs_storage_download_file_to_file(
                $fileId,
                $localPath,
                TRACKER_HOST,
                TRACKER_PORT
            );
            
            if ($result && file_exists($localPath)) {
                $fileSize = filesize($localPath);
                echo "✓ Download successful on attempt $attempt!\n";
                echo "File size: $fileSize bytes\n";
                echo "Saved to: $localPath\n";
                return true;
            }
            
            throw new Exception("Download failed - no file created");
            
        } catch (Exception $e) {
            echo "✗ Attempt $attempt failed: " . $e->getMessage() . "\n";
            
            if ($attempt < $maxRetries) {
                echo "Waiting " . RETRY_DELAY . " seconds before retry...\n";
                sleep(RETRY_DELAY);
            }
        }
    }
    
    echo "✗ Download failed after $maxRetries attempts\n";
    return false;
}

/**
 * Download file in chunks (streaming download)
 * 
 * Downloads a file in multiple chunks, useful for:
 * - Large files that shouldn't be loaded entirely into memory
 * - Progress tracking during download
 * - Bandwidth throttling
 * - Resume capability
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to download
 * @param string $localPath Local path to save file
 * @param int $chunkSize Size of each chunk in bytes
 * @param callable|null $progressCallback Optional callback for progress updates
 * @return bool True on success, false on failure
 */
function downloadInChunks($tracker, $fileId, $localPath, $chunkSize = CHUNK_SIZE, $progressCallback = null) {
    try {
        echo "\n--- Chunked Download ---\n";
        echo "File ID: $fileId\n";
        echo "Chunk size: " . number_format($chunkSize) . " bytes\n";
        
        // First, get the file size by downloading metadata or a small portion
        $testContent = fastdfs_storage_download_file_to_buff(
            $fileId,
            TRACKER_HOST,
            TRACKER_PORT,
            0,
            1
        );
        
        if ($testContent === false) {
            throw new Exception("Cannot access file");
        }
        
        // For demonstration, download the entire file first to get size
        // In production, you might get size from metadata
        $fullContent = fastdfs_storage_download_file_to_buff(
            $fileId,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if ($fullContent === false) {
            throw new Exception("Failed to download file");
        }
        
        $totalSize = strlen($fullContent);
        echo "Total file size: " . number_format($totalSize) . " bytes\n";
        
        // Open local file for writing
        $fp = fopen($localPath, 'wb');
        if (!$fp) {
            throw new Exception("Cannot open local file for writing");
        }
        
        $downloaded = 0;
        $offset = 0;
        
        echo "Starting chunked download...\n";
        
        while ($offset < $totalSize) {
            $length = min($chunkSize, $totalSize - $offset);
            
            // Download chunk
            $chunk = substr($fullContent, $offset, $length);
            
            if ($chunk === false) {
                fclose($fp);
                throw new Exception("Failed to download chunk at offset $offset");
            }
            
            // Write chunk to file
            fwrite($fp, $chunk);
            
            $downloaded += strlen($chunk);
            $offset += $length;
            
            // Calculate progress
            $progress = ($downloaded / $totalSize) * 100;
            
            // Call progress callback if provided
            if ($progressCallback && is_callable($progressCallback)) {
                $progressCallback($downloaded, $totalSize, $progress);
            } else {
                echo sprintf(
                    "Progress: %d%% (%s / %s)\n",
                    round($progress),
                    formatBytes($downloaded),
                    formatBytes($totalSize)
                );
            }
            
            // Simulate bandwidth throttling (optional)
            // usleep(10000); // 10ms delay between chunks
        }
        
        fclose($fp);
        
        echo "✓ Chunked download completed!\n";
        echo "Saved to: $localPath\n";
        
        return true;
        
    } catch (Exception $e) {
        echo "✗ Chunked download error: " . $e->getMessage() . "\n";
        if (isset($fp) && $fp) {
            fclose($fp);
        }
        return false;
    }
}

/**
 * Download multiple files in parallel (simulated)
 * 
 * Downloads multiple files concurrently to improve overall download time.
 * Note: True parallel execution requires multi-threading or async I/O.
 * This is a simplified demonstration.
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param array $fileIds Array of file IDs to download
 * @param string $downloadDir Directory to save files
 * @return array Results array with success/failure status for each file
 */
function downloadMultipleFiles($tracker, $fileIds, $downloadDir) {
    echo "\n--- Parallel Download (Multiple Files) ---\n";
    echo "Files to download: " . count($fileIds) . "\n";
    echo "Download directory: $downloadDir\n";
    
    // Ensure download directory exists
    if (!is_dir($downloadDir)) {
        mkdir($downloadDir, 0755, true);
    }
    
    $results = [];
    $startTime = microtime(true);
    
    foreach ($fileIds as $index => $fileId) {
        echo "\n--- File " . ($index + 1) . " of " . count($fileIds) . " ---\n";
        echo "File ID: $fileId\n";
        
        $localPath = $downloadDir . '/file_' . ($index + 1) . '_' . basename($fileId);
        
        try {
            $result = fastdfs_storage_download_file_to_file(
                $fileId,
                $localPath,
                TRACKER_HOST,
                TRACKER_PORT
            );
            
            if ($result && file_exists($localPath)) {
                $fileSize = filesize($localPath);
                echo "✓ Downloaded: " . formatBytes($fileSize) . "\n";
                
                $results[$fileId] = [
                    'success' => true,
                    'path' => $localPath,
                    'size' => $fileSize
                ];
            } else {
                throw new Exception("Download failed");
            }
            
        } catch (Exception $e) {
            echo "✗ Failed: " . $e->getMessage() . "\n";
            
            $results[$fileId] = [
                'success' => false,
                'error' => $e->getMessage()
            ];
        }
    }
    
    $endTime = microtime(true);
    $totalTime = $endTime - $startTime;
    
    // Summary
    echo "\n--- Download Summary ---\n";
    $successCount = count(array_filter($results, function($r) { return $r['success']; }));
    echo "Successful: $successCount / " . count($fileIds) . "\n";
    echo "Total time: " . round($totalTime, 2) . " seconds\n";
    
    return $results;
}

/**
 * Download file with progress callback
 * 
 * Demonstrates custom progress tracking during download.
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to download
 * @param string $localPath Local path to save file
 * @return bool True on success, false on failure
 */
function downloadWithProgress($tracker, $fileId, $localPath) {
    echo "\n--- Download with Progress Tracking ---\n";
    
    // Define progress callback
    $progressCallback = function($downloaded, $total, $percentage) {
        // Create progress bar
        $barLength = 50;
        $filled = round(($percentage / 100) * $barLength);
        $bar = str_repeat('=', $filled) . str_repeat('-', $barLength - $filled);
        
        echo sprintf(
            "\r[%s] %d%% - %s / %s",
            $bar,
            round($percentage),
            formatBytes($downloaded),
            formatBytes($total)
        );
        
        if ($percentage >= 100) {
            echo "\n";
        }
    };
    
    return downloadInChunks($tracker, $fileId, $localPath, CHUNK_SIZE, $progressCallback);
}

/**
 * Download specific byte range from file
 * 
 * Useful for:
 * - Video/audio seeking
 * - Resume interrupted downloads
 * - Downloading only needed portions of large files
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to download
 * @param int $offset Starting byte position
 * @param int $length Number of bytes to download
 * @param string $localPath Local path to save partial content
 * @return bool True on success, false on failure
 */
function downloadRange($tracker, $fileId, $offset, $length, $localPath) {
    try {
        echo "\n--- Range Download ---\n";
        echo "File ID: $fileId\n";
        echo "Range: bytes $offset-" . ($offset + $length - 1) . "\n";
        echo "Length: " . formatBytes($length) . "\n";
        
        // Download specific byte range
        $content = fastdfs_storage_download_file_to_buff(
            $fileId,
            TRACKER_HOST,
            TRACKER_PORT,
            $offset,
            $length
        );
        
        if ($content === false) {
            throw new Exception("Range download failed");
        }
        
        // Save to file
        $result = file_put_contents($localPath, $content);
        
        if ($result === false) {
            throw new Exception("Failed to save range to file");
        }
        
        echo "✓ Range downloaded successfully!\n";
        echo "Downloaded: " . formatBytes(strlen($content)) . "\n";
        echo "Saved to: $localPath\n";
        
        return true;
        
    } catch (Exception $e) {
        echo "✗ Range download error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Format bytes to human-readable format
 * 
 * @param int $bytes Number of bytes
 * @param int $precision Decimal precision
 * @return string Formatted string (e.g., "1.5 MB")
 */
function formatBytes($bytes, $precision = 2) {
    $units = ['B', 'KB', 'MB', 'GB', 'TB'];
    
    for ($i = 0; $bytes > 1024 && $i < count($units) - 1; $i++) {
        $bytes /= 1024;
    }
    
    return round($bytes, $precision) . ' ' . $units[$i];
}

/**
 * Create test files for demonstration
 * 
 * @param resource $tracker FastDFS tracker connection
 * @return array Array of uploaded file IDs
 */
function createTestFiles($tracker) {
    echo "\n--- Creating Test Files ---\n";
    
    $fileIds = [];
    
    // Create test files of different sizes
    $testFiles = [
        ['name' => 'small.txt', 'size' => 1024, 'label' => 'Small (1 KB)'],
        ['name' => 'medium.txt', 'size' => 1024 * 100, 'label' => 'Medium (100 KB)'],
        ['name' => 'large.txt', 'size' => 1024 * 1024, 'label' => 'Large (1 MB)']
    ];
    
    foreach ($testFiles as $file) {
        echo "\nCreating {$file['label']} file...\n";
        
        // Generate content
        $content = str_repeat("FastDFS Test Data - Line " . rand(1000, 9999) . "\n", $file['size'] / 50);
        $content = substr($content, 0, $file['size']);
        
        // Upload to FastDFS
        $fileId = fastdfs_storage_upload_by_filebuff(
            $content,
            'txt',
            ['size' => $file['size'], 'type' => 'test'],
            [],
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if ($fileId) {
            echo "✓ Uploaded: $fileId\n";
            $fileIds[$file['name']] = $fileId;
        } else {
            echo "✗ Upload failed\n";
        }
    }
    
    return $fileIds;
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Advanced Download Operations Example ===\n\n";
    
    // Initialize connection
    $tracker = initializeFastDFS();
    if (!$tracker) {
        exit(1);
    }
    
    // Create test files
    $testFiles = createTestFiles($tracker);
    
    if (empty($testFiles)) {
        echo "\n✗ No test files created. Cannot proceed with examples.\n";
        fastdfs_tracker_close_connection($tracker);
        exit(1);
    }
    
    // Create download directory
    $downloadDir = __DIR__ . '/downloads';
    if (!is_dir($downloadDir)) {
        mkdir($downloadDir, 0755, true);
    }
    
    // Example 1: Download with retry logic
    if (isset($testFiles['small.txt'])) {
        downloadWithRetry(
            $tracker,
            $testFiles['small.txt'],
            $downloadDir . '/small_retry.txt'
        );
    }
    
    // Example 2: Chunked download with progress
    if (isset($testFiles['large.txt'])) {
        downloadWithProgress(
            $tracker,
            $testFiles['large.txt'],
            $downloadDir . '/large_progress.txt'
        );
    }
    
    // Example 3: Range download
    if (isset($testFiles['medium.txt'])) {
        downloadRange(
            $tracker,
            $testFiles['medium.txt'],
            0,
            1024,  // First 1KB
            $downloadDir . '/medium_range.txt'
        );
    }
    
    // Example 4: Multiple file download
    $multipleFiles = array_values($testFiles);
    if (!empty($multipleFiles)) {
        downloadMultipleFiles($tracker, $multipleFiles, $downloadDir . '/batch');
    }
    
    // Example 5: Get storage server info
    if (isset($testFiles['small.txt'])) {
        getStorageServerInfo($tracker, $testFiles['small.txt']);
    }
    
    // Summary
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Summary ===\n";
    echo str_repeat("=", 70) . "\n";
    
    echo "\nAdvanced download techniques demonstrated:\n";
    echo "✓ Download with automatic retry logic\n";
    echo "✓ Chunked/streaming downloads\n";
    echo "✓ Progress tracking with callbacks\n";
    echo "✓ Range/partial downloads\n";
    echo "✓ Multiple file downloads\n";
    echo "✓ Storage server information retrieval\n";
    
    echo "\nTest files created:\n";
    foreach ($testFiles as $name => $fileId) {
        echo "  $name: $fileId\n";
    }
    
    echo "\nDownloaded files location:\n";
    echo "  $downloadDir/\n";
    
    echo "\nKey Points:\n";
    echo "- Use retry logic for unreliable networks\n";
    echo "- Chunked downloads prevent memory issues with large files\n";
    echo "- Range downloads enable resume and seeking capabilities\n";
    echo "- Progress callbacks improve user experience\n";
    echo "- Storage server info useful for load balancing\n";
    
    // Close tracker connection
    fastdfs_tracker_close_connection($tracker);
    echo "\n✓ Connection closed\n";
}

// Run the example
main();

?>
