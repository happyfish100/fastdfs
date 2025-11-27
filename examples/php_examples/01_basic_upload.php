<?php
/**
 * FastDFS Basic Upload Example
 * 
 * This example demonstrates how to upload a file to FastDFS storage system.
 * It covers:
 * - Connecting to FastDFS tracker server
 * - Uploading a file and receiving a file ID
 * - Proper error handling and connection cleanup
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed (php-fastdfs)
 * - FastDFS tracker server running and accessible
 * - Proper configuration in client.conf or connection parameters
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
        // Create a new FastDFS tracker connection
        $tracker = fastdfs_tracker_make_connection(TRACKER_HOST, TRACKER_PORT);
        
        if (!$tracker) {
            throw new Exception("Failed to connect to tracker server at " . TRACKER_HOST . ":" . TRACKER_PORT);
        }
        
        echo "✓ Successfully connected to FastDFS tracker server\n";
        return $tracker;
        
    } catch (Exception $e) {
        echo "✗ Connection error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Upload a file to FastDFS storage
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $localFilePath Path to the local file to upload
 * @param string $fileExtension File extension (e.g., 'jpg', 'pdf', 'txt')
 * @return string|false File ID on success, false on failure
 */
function uploadFile($tracker, $localFilePath, $fileExtension = '') {
    try {
        // Verify file exists
        if (!file_exists($localFilePath)) {
            throw new Exception("File not found: " . $localFilePath);
        }
        
        // Get file extension if not provided
        if (empty($fileExtension)) {
            $fileExtension = pathinfo($localFilePath, PATHINFO_EXTENSION);
        }
        
        echo "\n--- Uploading File ---\n";
        echo "Local path: $localFilePath\n";
        echo "File size: " . filesize($localFilePath) . " bytes\n";
        echo "Extension: $fileExtension\n";
        
        // Upload file to FastDFS
        // Returns a file ID in format: group1/M00/00/00/wKgBcFxxx.jpg
        $fileId = fastdfs_storage_upload_by_filename($localFilePath, $fileExtension, [], [], TRACKER_HOST, TRACKER_PORT);
        
        if (!$fileId) {
            throw new Exception("Upload failed - no file ID returned");
        }
        
        echo "✓ Upload successful!\n";
        echo "File ID: $fileId\n";
        
        return $fileId;
        
    } catch (Exception $e) {
        echo "✗ Upload error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Upload file from memory buffer
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileContent File content as string
 * @param string $fileExtension File extension
 * @return string|false File ID on success, false on failure
 */
function uploadFromBuffer($tracker, $fileContent, $fileExtension) {
    try {
        echo "\n--- Uploading from Buffer ---\n";
        echo "Content size: " . strlen($fileContent) . " bytes\n";
        echo "Extension: $fileExtension\n";
        
        // Upload file content directly from memory
        $fileId = fastdfs_storage_upload_by_filebuff($fileContent, $fileExtension, [], [], TRACKER_HOST, TRACKER_PORT);
        
        if (!$fileId) {
            throw new Exception("Buffer upload failed");
        }
        
        echo "✓ Buffer upload successful!\n";
        echo "File ID: $fileId\n";
        
        return $fileId;
        
    } catch (Exception $e) {
        echo "✗ Buffer upload error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Basic Upload Example ===\n\n";
    
    // Initialize connection
    $tracker = initializeFastDFS();
    if (!$tracker) {
        exit(1);
    }
    
    // Example 1: Upload a file from disk
    // Replace with your actual file path
    $testFile = __DIR__ . '/test_upload.txt';
    
    // Create a test file if it doesn't exist
    if (!file_exists($testFile)) {
        file_put_contents($testFile, "Hello FastDFS! This is a test file.\nTimestamp: " . date('Y-m-d H:i:s'));
        echo "Created test file: $testFile\n";
    }
    
    $fileId = uploadFile($tracker, $testFile, 'txt');
    
    if ($fileId) {
        echo "\n--- Upload Summary ---\n";
        echo "You can now access this file using the File ID: $fileId\n";
        echo "Store this ID in your database to retrieve the file later.\n";
    }
    
    // Example 2: Upload content from memory
    $content = "This is content uploaded directly from memory.\nNo temporary file needed!";
    $bufferId = uploadFromBuffer($tracker, $content, 'txt');
    
    if ($bufferId) {
        echo "\nBuffer File ID: $bufferId\n";
    }
    
    // Close tracker connection
    fastdfs_tracker_close_connection($tracker);
    echo "\n✓ Connection closed\n";
}

// Run the example
main();

?>
