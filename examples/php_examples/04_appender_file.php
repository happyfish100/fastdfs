<?php
/**
 * FastDFS Appender File Example
 * 
 * This example demonstrates how to work with appender files in FastDFS.
 * Appender files are special files that can be modified after upload by:
 * - Appending data to the end of the file
 * - Modifying existing content at specific positions
 * 
 * Use cases:
 * - Log files that grow over time
 * - Streaming data collection
 * - Files that need incremental updates
 * - Append-only data structures
 * 
 * Note: Regular files in FastDFS are immutable. Appender files provide
 * the ability to modify files after initial upload.
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed (php-fastdfs)
 * - FastDFS tracker server running and accessible
 * - Storage server configured to support appender files
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
 * Upload an appender file from local disk
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $localFilePath Path to local file
 * @param array $metadata Optional metadata for the file
 * @return string|false Appender file ID on success, false on failure
 */
function uploadAppenderFile($tracker, $localFilePath, $metadata = []) {
    try {
        echo "\n--- Uploading Appender File ---\n";
        echo "File: $localFilePath\n";
        
        if (!file_exists($localFilePath)) {
            throw new Exception("File not found: $localFilePath");
        }
        
        $fileSize = filesize($localFilePath);
        $fileExtension = pathinfo($localFilePath, PATHINFO_EXTENSION);
        
        echo "Size: $fileSize bytes\n";
        echo "Extension: $fileExtension\n";
        
        // Upload as appender file - this file can be modified later
        $fileId = fastdfs_storage_upload_appender_by_filename(
            $localFilePath,
            $fileExtension,
            $metadata,
            [],  // Group name (empty for auto-selection)
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$fileId) {
            throw new Exception("Appender file upload failed");
        }
        
        echo "✓ Appender file uploaded successfully!\n";
        echo "File ID: $fileId\n";
        echo "This file can now be appended or modified\n";
        
        return $fileId;
        
    } catch (Exception $e) {
        echo "✗ Upload error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Upload an appender file from memory buffer
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $content File content to upload
 * @param string $fileExtension File extension
 * @param array $metadata Optional metadata
 * @return string|false Appender file ID on success, false on failure
 */
function uploadAppenderFromBuffer($tracker, $content, $fileExtension, $metadata = []) {
    try {
        echo "\n--- Uploading Appender File from Buffer ---\n";
        echo "Content size: " . strlen($content) . " bytes\n";
        echo "Extension: $fileExtension\n";
        
        // Upload content as appender file
        $fileId = fastdfs_storage_upload_appender_by_filebuff(
            $content,
            $fileExtension,
            $metadata,
            [],
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$fileId) {
            throw new Exception("Buffer upload failed");
        }
        
        echo "✓ Appender file uploaded from buffer!\n";
        echo "File ID: $fileId\n";
        
        return $fileId;
        
    } catch (Exception $e) {
        echo "✗ Upload error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Append data to an existing appender file from local file
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId Appender file ID
 * @param string $localFilePath Path to file containing data to append
 * @return bool True on success, false on failure
 */
function appendFromFile($tracker, $fileId, $localFilePath) {
    try {
        echo "\n--- Appending from File ---\n";
        echo "Appender file ID: $fileId\n";
        echo "Append from: $localFilePath\n";
        
        if (!file_exists($localFilePath)) {
            throw new Exception("File not found: $localFilePath");
        }
        
        $appendSize = filesize($localFilePath);
        echo "Appending: $appendSize bytes\n";
        
        // Append content from file to existing appender file
        $result = fastdfs_storage_append_by_filename(
            $fileId,
            $localFilePath,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Append operation failed");
        }
        
        echo "✓ Data appended successfully!\n";
        return true;
        
    } catch (Exception $e) {
        echo "✗ Append error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Append data to an existing appender file from memory buffer
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId Appender file ID
 * @param string $content Content to append
 * @return bool True on success, false on failure
 */
function appendFromBuffer($tracker, $fileId, $content) {
    try {
        echo "\n--- Appending from Buffer ---\n";
        echo "Appender file ID: $fileId\n";
        echo "Content size: " . strlen($content) . " bytes\n";
        
        // Append content from buffer to existing appender file
        $result = fastdfs_storage_append_by_filebuff(
            $fileId,
            $content,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Append operation failed");
        }
        
        echo "✓ Data appended successfully!\n";
        return true;
        
    } catch (Exception $e) {
        echo "✗ Append error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Modify content in an appender file at a specific offset
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId Appender file ID
 * @param int $offset Byte offset where modification should start
 * @param string $content New content to write at offset
 * @return bool True on success, false on failure
 */
function modifyFile($tracker, $fileId, $offset, $content) {
    try {
        echo "\n--- Modifying Appender File ---\n";
        echo "File ID: $fileId\n";
        echo "Offset: $offset bytes\n";
        echo "Content size: " . strlen($content) . " bytes\n";
        
        // Modify content at specific offset in appender file
        $result = fastdfs_storage_modify_by_filebuff(
            $fileId,
            $offset,
            $content,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Modify operation failed");
        }
        
        echo "✓ File modified successfully!\n";
        return true;
        
    } catch (Exception $e) {
        echo "✗ Modify error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Truncate an appender file to a specific size
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId Appender file ID
 * @param int $truncateSize New file size in bytes
 * @return bool True on success, false on failure
 */
function truncateFile($tracker, $fileId, $truncateSize) {
    try {
        echo "\n--- Truncating Appender File ---\n";
        echo "File ID: $fileId\n";
        echo "Truncate to: $truncateSize bytes\n";
        
        // Truncate file to specified size
        $result = fastdfs_storage_truncate_file(
            $fileId,
            $truncateSize,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Truncate operation failed");
        }
        
        echo "✓ File truncated successfully!\n";
        return true;
        
    } catch (Exception $e) {
        echo "✗ Truncate error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Download and display appender file content
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to download
 * @return string|false File content on success, false on failure
 */
function downloadAndDisplay($tracker, $fileId) {
    try {
        echo "\n--- Downloading File Content ---\n";
        echo "File ID: $fileId\n";
        
        $content = fastdfs_storage_download_file_to_buff($fileId, TRACKER_HOST, TRACKER_PORT);
        
        if ($content === false) {
            throw new Exception("Download failed");
        }
        
        echo "✓ Downloaded: " . strlen($content) . " bytes\n";
        echo "\nContent:\n";
        echo str_repeat("-", 50) . "\n";
        echo $content . "\n";
        echo str_repeat("-", 50) . "\n";
        
        return $content;
        
    } catch (Exception $e) {
        echo "✗ Download error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Demonstrate log file scenario using appender file
 * 
 * @param resource $tracker FastDFS tracker connection
 */
function demonstrateLogFile($tracker) {
    echo "\n" . str_repeat("=", 60) . "\n";
    echo "=== Log File Scenario ===\n";
    echo str_repeat("=", 60) . "\n";
    
    // Create initial log content
    $initialLog = "[" . date('Y-m-d H:i:s') . "] Application started\n";
    $initialLog .= "[" . date('Y-m-d H:i:s') . "] Initializing components...\n";
    
    // Upload as appender file
    $fileId = uploadAppenderFromBuffer($tracker, $initialLog, 'log');
    
    if (!$fileId) {
        return;
    }
    
    // Display initial content
    downloadAndDisplay($tracker, $fileId);
    
    // Simulate adding log entries over time
    sleep(1);
    $newEntry1 = "[" . date('Y-m-d H:i:s') . "] User login: admin\n";
    appendFromBuffer($tracker, $fileId, $newEntry1);
    
    sleep(1);
    $newEntry2 = "[" . date('Y-m-d H:i:s') . "] Processing request #12345\n";
    appendFromBuffer($tracker, $fileId, $newEntry2);
    
    sleep(1);
    $newEntry3 = "[" . date('Y-m-d H:i:s') . "] Request completed successfully\n";
    appendFromBuffer($tracker, $fileId, $newEntry3);
    
    // Display final log content
    echo "\n--- Final Log Content ---\n";
    downloadAndDisplay($tracker, $fileId);
    
    return $fileId;
}

/**
 * Demonstrate data modification scenario
 * 
 * @param resource $tracker FastDFS tracker connection
 */
function demonstrateModification($tracker) {
    echo "\n" . str_repeat("=", 60) . "\n";
    echo "=== Data Modification Scenario ===\n";
    echo str_repeat("=", 60) . "\n";
    
    // Create initial data
    $initialData = "Status: PENDING\n";
    $initialData .= "Progress: 0%\n";
    $initialData .= "Message: Waiting to start...\n";
    
    $fileId = uploadAppenderFromBuffer($tracker, $initialData, 'txt');
    
    if (!$fileId) {
        return;
    }
    
    echo "\n--- Initial State ---\n";
    downloadAndDisplay($tracker, $fileId);
    
    // Modify status (overwrite "PENDING" with "RUNNING")
    sleep(1);
    modifyFile($tracker, $fileId, 8, "RUNNING");
    
    echo "\n--- After Status Update ---\n";
    downloadAndDisplay($tracker, $fileId);
    
    // Modify progress
    sleep(1);
    modifyFile($tracker, $fileId, 26, "50%");
    
    // Modify message
    modifyFile($tracker, $fileId, 40, "Processing data...     ");
    
    echo "\n--- After Progress Update ---\n";
    downloadAndDisplay($tracker, $fileId);
    
    // Final update
    sleep(1);
    modifyFile($tracker, $fileId, 8, "COMPLETE");
    modifyFile($tracker, $fileId, 26, "100%");
    modifyFile($tracker, $fileId, 40, "Task completed!        ");
    
    echo "\n--- Final State ---\n";
    downloadAndDisplay($tracker, $fileId);
    
    return $fileId;
}

/**
 * Demonstrate truncate operation
 * 
 * @param resource $tracker FastDFS tracker connection
 */
function demonstrateTruncate($tracker) {
    echo "\n" . str_repeat("=", 60) . "\n";
    echo "=== Truncate Operation Scenario ===\n";
    echo str_repeat("=", 60) . "\n";
    
    // Create a file with some content
    $content = "Line 1: This is the first line\n";
    $content .= "Line 2: This is the second line\n";
    $content .= "Line 3: This is the third line\n";
    $content .= "Line 4: This is the fourth line\n";
    
    $fileId = uploadAppenderFromBuffer($tracker, $content, 'txt');
    
    if (!$fileId) {
        return;
    }
    
    echo "\n--- Original Content ---\n";
    $originalContent = downloadAndDisplay($tracker, $fileId);
    $originalSize = strlen($originalContent);
    
    // Truncate to keep only first 2 lines (approximately 62 bytes)
    $truncateSize = 62;
    truncateFile($tracker, $fileId, $truncateSize);
    
    echo "\n--- After Truncate to $truncateSize bytes ---\n";
    downloadAndDisplay($tracker, $fileId);
    
    // Append new content after truncate
    $newContent = "Line 3: New content after truncate\n";
    appendFromBuffer($tracker, $fileId, $newContent);
    
    echo "\n--- After Appending New Content ---\n";
    downloadAndDisplay($tracker, $fileId);
    
    return $fileId;
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Appender File Example ===\n\n";
    
    // Initialize connection
    $tracker = initializeFastDFS();
    if (!$tracker) {
        exit(1);
    }
    
    // Scenario 1: Log file that grows over time
    $logFileId = demonstrateLogFile($tracker);
    
    // Scenario 2: Modifying existing content
    $dataFileId = demonstrateModification($tracker);
    
    // Scenario 3: Truncate operation
    $truncateFileId = demonstrateTruncate($tracker);
    
    // Summary
    echo "\n" . str_repeat("=", 60) . "\n";
    echo "=== Summary ===\n";
    echo str_repeat("=", 60) . "\n";
    echo "\nAppender file operations demonstrated:\n";
    echo "✓ Upload appender file from buffer\n";
    echo "✓ Append data to existing file\n";
    echo "✓ Modify content at specific offset\n";
    echo "✓ Truncate file to specific size\n";
    echo "✓ Download and verify content\n";
    
    if ($logFileId) {
        echo "\nLog file ID: $logFileId\n";
    }
    if ($dataFileId) {
        echo "Data file ID: $dataFileId\n";
    }
    if ($truncateFileId) {
        echo "Truncate demo file ID: $truncateFileId\n";
    }
    
    echo "\nKey Points:\n";
    echo "- Appender files can be modified after upload\n";
    echo "- Use append operations for log files and streaming data\n";
    echo "- Use modify operations to update specific portions\n";
    echo "- Truncate can reduce file size when needed\n";
    echo "- Regular files are immutable; use appender files for mutable content\n";
    
    // Close tracker connection
    fastdfs_tracker_close_connection($tracker);
    echo "\n✓ Connection closed\n";
}

// Run the example
main();

?>
