<?php
/**
 * FastDFS Basic Download Example
 * 
 * This example demonstrates how to download files from FastDFS storage system.
 * It covers:
 * - Downloading files to disk
 * - Downloading files to memory buffer
 * - Retrieving file information
 * - Error handling for missing or corrupted files
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed (php-fastdfs)
 * - FastDFS tracker server running and accessible
 * - Valid file ID from a previous upload
 */

// FastDFS tracker server configuration
define('TRACKER_HOST', '127.0.0.1');
define('TRACKER_PORT', 22122);

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
 * Download file from FastDFS to local disk
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID returned from upload (e.g., "group1/M00/00/00/xxx.jpg")
 * @param string $localPath Local path where file should be saved
 * @return bool True on success, false on failure
 */
function downloadToFile($tracker, $fileId, $localPath) {
    try {
        echo "\n--- Downloading to File ---\n";
        echo "File ID: $fileId\n";
        echo "Save to: $localPath\n";
        
        // Download file from FastDFS storage to local disk
        $result = fastdfs_storage_download_file_to_file($fileId, $localPath, TRACKER_HOST, TRACKER_PORT);
        
        if (!$result) {
            throw new Exception("Download failed - file may not exist or connection error");
        }
        
        // Verify the downloaded file
        if (!file_exists($localPath)) {
            throw new Exception("File was not saved to disk");
        }
        
        $fileSize = filesize($localPath);
        echo "✓ Download successful!\n";
        echo "File size: $fileSize bytes\n";
        echo "Saved to: $localPath\n";
        
        return true;
        
    } catch (Exception $e) {
        echo "✗ Download error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Download file from FastDFS to memory buffer
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to download
 * @return string|false File content on success, false on failure
 */
function downloadToBuffer($tracker, $fileId) {
    try {
        echo "\n--- Downloading to Buffer ---\n";
        echo "File ID: $fileId\n";
        
        // Download file content directly to memory
        $content = fastdfs_storage_download_file_to_buff($fileId, TRACKER_HOST, TRACKER_PORT);
        
        if ($content === false) {
            throw new Exception("Failed to download file to buffer");
        }
        
        echo "✓ Download successful!\n";
        echo "Content size: " . strlen($content) . " bytes\n";
        
        return $content;
        
    } catch (Exception $e) {
        echo "✗ Download error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Download a specific portion of a file (partial download)
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to download
 * @param int $offset Starting byte position
 * @param int $length Number of bytes to download
 * @return string|false Partial content on success, false on failure
 */
function downloadPartial($tracker, $fileId, $offset, $length) {
    try {
        echo "\n--- Partial Download ---\n";
        echo "File ID: $fileId\n";
        echo "Offset: $offset bytes\n";
        echo "Length: $length bytes\n";
        
        // Download specific byte range from file
        $content = fastdfs_storage_download_file_to_buff($fileId, TRACKER_HOST, TRACKER_PORT, $offset, $length);
        
        if ($content === false) {
            throw new Exception("Partial download failed");
        }
        
        echo "✓ Partial download successful!\n";
        echo "Downloaded: " . strlen($content) . " bytes\n";
        
        return $content;
        
    } catch (Exception $e) {
        echo "✗ Partial download error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Get file information without downloading
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to query
 * @return array|false File info array on success, false on failure
 */
function getFileInfo($tracker, $fileId) {
    try {
        echo "\n--- Getting File Info ---\n";
        echo "File ID: $fileId\n";
        
        // Retrieve file metadata
        $info = fastdfs_storage_get_metadata($fileId, TRACKER_HOST, TRACKER_PORT);
        
        if ($info === false) {
            throw new Exception("Failed to retrieve file info");
        }
        
        echo "✓ File info retrieved!\n";
        
        // Display file information
        if (is_array($info) && !empty($info)) {
            echo "Metadata:\n";
            foreach ($info as $key => $value) {
                echo "  $key: $value\n";
            }
        } else {
            echo "No metadata available for this file\n";
        }
        
        return $info;
        
    } catch (Exception $e) {
        echo "✗ Info retrieval error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Check if a file exists in FastDFS
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to check
 * @return bool True if file exists, false otherwise
 */
function fileExists($tracker, $fileId) {
    try {
        echo "\n--- Checking File Existence ---\n";
        echo "File ID: $fileId\n";
        
        // Try to get file info to verify existence
        $info = fastdfs_storage_get_metadata($fileId, TRACKER_HOST, TRACKER_PORT);
        
        $exists = ($info !== false);
        echo ($exists ? "✓ File exists\n" : "✗ File does not exist\n");
        
        return $exists;
        
    } catch (Exception $e) {
        echo "✗ Check error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Basic Download Example ===\n\n";
    
    // Initialize connection
    $tracker = initializeFastDFS();
    if (!$tracker) {
        exit(1);
    }
    
    // Example file ID - replace with your actual file ID from upload
    // Format: group1/M00/00/00/wKgBcFxxx.jpg
    $fileId = "group1/M00/00/00/example.txt";
    
    echo "\n⚠ NOTE: Replace the \$fileId variable with a valid file ID from your FastDFS storage\n";
    echo "Current file ID: $fileId\n";
    
    // Check if file exists
    if (fileExists($tracker, $fileId)) {
        
        // Example 1: Download to local file
        $downloadPath = __DIR__ . '/downloaded_file.txt';
        downloadToFile($tracker, $fileId, $downloadPath);
        
        // Example 2: Download to memory buffer
        $content = downloadToBuffer($tracker, $fileId);
        if ($content !== false) {
            echo "\nFirst 100 characters of content:\n";
            echo substr($content, 0, 100) . "\n";
        }
        
        // Example 3: Partial download (first 50 bytes)
        $partialContent = downloadPartial($tracker, $fileId, 0, 50);
        if ($partialContent !== false) {
            echo "\nPartial content:\n";
            echo $partialContent . "\n";
        }
        
        // Example 4: Get file metadata
        getFileInfo($tracker, $fileId);
        
    } else {
        echo "\n⚠ File does not exist. Please upload a file first using 01_basic_upload.php\n";
    }
    
    // Close tracker connection
    fastdfs_tracker_close_connection($tracker);
    echo "\n✓ Connection closed\n";
}

// Run the example
main();

?>
