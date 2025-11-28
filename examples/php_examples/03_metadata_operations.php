<?php
/**
 * FastDFS Metadata Operations Example
 * 
 * This example demonstrates how to work with file metadata in FastDFS.
 * Metadata allows you to store additional information about files such as:
 * - Original filename
 * - Upload timestamp
 * - File description
 * - Author/owner information
 * - Custom tags and attributes
 * 
 * Operations covered:
 * - Setting metadata during upload
 * - Retrieving metadata
 * - Updating existing metadata
 * - Deleting metadata
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed (php-fastdfs)
 * - FastDFS tracker server running and accessible
 */

// FastDFS tracker server configuration
define('TRACKER_HOST', '127.0.0.1');
define('TRACKER_PORT', 22122);

// Metadata operation flags
define('METADATA_OVERWRITE', 'O');  // Overwrite all existing metadata
define('METADATA_MERGE', 'M');      // Merge with existing metadata

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
 * Upload a file with metadata
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $localFilePath Path to local file
 * @param array $metadata Associative array of metadata key-value pairs
 * @return string|false File ID on success, false on failure
 */
function uploadWithMetadata($tracker, $localFilePath, $metadata = []) {
    try {
        echo "\n--- Uploading File with Metadata ---\n";
        echo "File: $localFilePath\n";
        
        if (!file_exists($localFilePath)) {
            throw new Exception("File not found: $localFilePath");
        }
        
        // Display metadata being uploaded
        if (!empty($metadata)) {
            echo "Metadata:\n";
            foreach ($metadata as $key => $value) {
                echo "  $key = $value\n";
            }
        }
        
        $fileExtension = pathinfo($localFilePath, PATHINFO_EXTENSION);
        
        // Upload file with metadata
        // Metadata is passed as an associative array
        $fileId = fastdfs_storage_upload_by_filename(
            $localFilePath,
            $fileExtension,
            $metadata,  // Metadata array
            [],         // Group name (empty for auto-selection)
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$fileId) {
            throw new Exception("Upload failed");
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
 * Retrieve metadata for a file
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to query
 * @return array|false Metadata array on success, false on failure
 */
function getMetadata($tracker, $fileId) {
    try {
        echo "\n--- Retrieving Metadata ---\n";
        echo "File ID: $fileId\n";
        
        // Get metadata from FastDFS
        $metadata = fastdfs_storage_get_metadata($fileId, TRACKER_HOST, TRACKER_PORT);
        
        if ($metadata === false) {
            throw new Exception("Failed to retrieve metadata");
        }
        
        echo "✓ Metadata retrieved!\n";
        
        if (is_array($metadata) && !empty($metadata)) {
            echo "Metadata entries:\n";
            foreach ($metadata as $key => $value) {
                echo "  $key = $value\n";
            }
        } else {
            echo "No metadata found for this file\n";
        }
        
        return $metadata;
        
    } catch (Exception $e) {
        echo "✗ Retrieval error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Set or update metadata for an existing file
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to update
 * @param array $metadata New metadata to set
 * @param string $flag Operation flag: 'O' for overwrite, 'M' for merge
 * @return bool True on success, false on failure
 */
function setMetadata($tracker, $fileId, $metadata, $flag = METADATA_MERGE) {
    try {
        echo "\n--- Setting Metadata ---\n";
        echo "File ID: $fileId\n";
        echo "Operation: " . ($flag === METADATA_OVERWRITE ? "OVERWRITE" : "MERGE") . "\n";
        
        if (empty($metadata)) {
            throw new Exception("Metadata array is empty");
        }
        
        echo "New metadata:\n";
        foreach ($metadata as $key => $value) {
            echo "  $key = $value\n";
        }
        
        // Set metadata on existing file
        // Flag 'O' overwrites all existing metadata
        // Flag 'M' merges with existing metadata
        $result = fastdfs_storage_set_metadata(
            $fileId,
            $metadata,
            $flag,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Failed to set metadata");
        }
        
        echo "✓ Metadata updated successfully!\n";
        return true;
        
    } catch (Exception $e) {
        echo "✗ Set metadata error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Delete all metadata from a file
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to clear metadata from
 * @return bool True on success, false on failure
 */
function deleteMetadata($tracker, $fileId) {
    try {
        echo "\n--- Deleting Metadata ---\n";
        echo "File ID: $fileId\n";
        
        // Delete metadata by setting empty array with overwrite flag
        $result = fastdfs_storage_set_metadata(
            $fileId,
            [],  // Empty array
            METADATA_OVERWRITE,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Failed to delete metadata");
        }
        
        echo "✓ Metadata deleted successfully!\n";
        return true;
        
    } catch (Exception $e) {
        echo "✗ Delete metadata error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Demonstrate metadata merge vs overwrite
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $fileId File ID to demonstrate on
 */
function demonstrateMergeVsOverwrite($tracker, $fileId) {
    echo "\n=== Demonstrating Merge vs Overwrite ===\n";
    
    // Set initial metadata
    $initialMetadata = [
        'author' => 'John Doe',
        'category' => 'documents',
        'version' => '1.0'
    ];
    
    echo "\n1. Setting initial metadata...\n";
    setMetadata($tracker, $fileId, $initialMetadata, METADATA_OVERWRITE);
    getMetadata($tracker, $fileId);
    
    // Merge new metadata (keeps existing)
    $mergeMetadata = [
        'description' => 'Important document',
        'tags' => 'urgent,review'
    ];
    
    echo "\n2. Merging additional metadata...\n";
    setMetadata($tracker, $fileId, $mergeMetadata, METADATA_MERGE);
    getMetadata($tracker, $fileId);
    
    // Overwrite with new metadata (removes existing)
    $overwriteMetadata = [
        'status' => 'archived',
        'date' => date('Y-m-d')
    ];
    
    echo "\n3. Overwriting all metadata...\n";
    setMetadata($tracker, $fileId, $overwriteMetadata, METADATA_OVERWRITE);
    getMetadata($tracker, $fileId);
}

/**
 * Create a sample file for testing
 * 
 * @return string Path to created test file
 */
function createTestFile() {
    $testFile = __DIR__ . '/metadata_test.txt';
    $content = "FastDFS Metadata Test File\n";
    $content .= "Created: " . date('Y-m-d H:i:s') . "\n";
    $content .= "This file is used to demonstrate metadata operations.\n";
    
    file_put_contents($testFile, $content);
    return $testFile;
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Metadata Operations Example ===\n\n";
    
    // Initialize connection
    $tracker = initializeFastDFS();
    if (!$tracker) {
        exit(1);
    }
    
    // Create test file
    $testFile = createTestFile();
    echo "\nCreated test file: $testFile\n";
    
    // Define metadata for the file
    $metadata = [
        'filename' => basename($testFile),
        'upload_date' => date('Y-m-d H:i:s'),
        'author' => 'FastDFS Example',
        'description' => 'Test file for metadata operations',
        'content_type' => 'text/plain',
        'tags' => 'test,example,metadata'
    ];
    
    // Upload file with metadata
    $fileId = uploadWithMetadata($tracker, $testFile, $metadata);
    
    if ($fileId) {
        // Retrieve and display metadata
        getMetadata($tracker, $fileId);
        
        // Update metadata (merge)
        $additionalMetadata = [
            'last_modified' => date('Y-m-d H:i:s'),
            'version' => '1.1',
            'status' => 'active'
        ];
        
        setMetadata($tracker, $fileId, $additionalMetadata, METADATA_MERGE);
        
        // Retrieve updated metadata
        getMetadata($tracker, $fileId);
        
        // Demonstrate merge vs overwrite
        demonstrateMergeVsOverwrite($tracker, $fileId);
        
        // Final metadata state
        echo "\n--- Final Metadata State ---\n";
        getMetadata($tracker, $fileId);
        
        echo "\n--- Summary ---\n";
        echo "File ID: $fileId\n";
        echo "You can use this file ID to test other operations\n";
        
    } else {
        echo "\n✗ Failed to upload file with metadata\n";
    }
    
    // Close tracker connection
    fastdfs_tracker_close_connection($tracker);
    echo "\n✓ Connection closed\n";
}

// Run the example
main();

?>
