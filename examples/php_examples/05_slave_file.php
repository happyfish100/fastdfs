<?php
/**
 * FastDFS Slave File Operations Example
 * 
 * This example demonstrates how to work with slave files in FastDFS.
 * Slave files are derived files (like thumbnails, compressed versions, or 
 * different formats) that are associated with a master file.
 * 
 * Key concepts:
 * - Master file: The original uploaded file
 * - Slave file: A derived/related file linked to the master
 * - Prefix/Suffix: Used to identify the relationship between files
 * 
 * Common use cases:
 * - Image thumbnails (small, medium, large versions)
 * - Video transcoding (different resolutions/formats)
 * - Document conversions (PDF to images, etc.)
 * - Compressed versions of files
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed (php-fastdfs)
 * - FastDFS tracker server running and accessible
 * - Master file already uploaded to FastDFS
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
 * Upload a master file to FastDFS
 * 
 * This is the primary/original file that slave files will be associated with.
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $localFilePath Path to the local file
 * @param array $metadata Optional metadata
 * @return string|false Master file ID on success, false on failure
 */
function uploadMasterFile($tracker, $localFilePath, $metadata = []) {
    try {
        echo "\n--- Uploading Master File ---\n";
        echo "File: $localFilePath\n";
        
        if (!file_exists($localFilePath)) {
            throw new Exception("File not found: $localFilePath");
        }
        
        $fileSize = filesize($localFilePath);
        $fileExtension = pathinfo($localFilePath, PATHINFO_EXTENSION);
        
        echo "Size: $fileSize bytes\n";
        echo "Extension: $fileExtension\n";
        
        // Upload the master file
        $fileId = fastdfs_storage_upload_by_filename(
            $localFilePath,
            $fileExtension,
            $metadata,
            [],
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$fileId) {
            throw new Exception("Master file upload failed");
        }
        
        echo "✓ Master file uploaded successfully!\n";
        echo "Master File ID: $fileId\n";
        
        return $fileId;
        
    } catch (Exception $e) {
        echo "✗ Upload error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Upload a slave file associated with a master file
 * 
 * Slave files are linked to master files using a prefix/suffix naming convention.
 * The slave file is stored in the same group as the master file.
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $masterFileId Master file ID
 * @param string $localFilePath Path to the slave file
 * @param string $prefixName Prefix to identify the slave file (e.g., "_thumb", "_small", "_150x150")
 * @param string $fileExtension File extension for slave file
 * @return string|false Slave file ID on success, false on failure
 */
function uploadSlaveFile($tracker, $masterFileId, $localFilePath, $prefixName, $fileExtension = '') {
    try {
        echo "\n--- Uploading Slave File ---\n";
        echo "Master File ID: $masterFileId\n";
        echo "Slave File: $localFilePath\n";
        echo "Prefix: $prefixName\n";
        
        if (!file_exists($localFilePath)) {
            throw new Exception("Slave file not found: $localFilePath");
        }
        
        if (empty($fileExtension)) {
            $fileExtension = pathinfo($localFilePath, PATHINFO_EXTENSION);
        }
        
        $fileSize = filesize($localFilePath);
        echo "Size: $fileSize bytes\n";
        echo "Extension: $fileExtension\n";
        
        // Upload slave file linked to master file
        // The slave file will be stored with a reference to the master file
        $slaveFileId = fastdfs_storage_upload_slave_by_filename(
            $localFilePath,
            $masterFileId,
            $prefixName,
            $fileExtension,
            [],  // Metadata
            [],  // Group name
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$slaveFileId) {
            throw new Exception("Slave file upload failed");
        }
        
        echo "✓ Slave file uploaded successfully!\n";
        echo "Slave File ID: $slaveFileId\n";
        
        return $slaveFileId;
        
    } catch (Exception $e) {
        echo "✗ Slave upload error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Upload a slave file from memory buffer
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $masterFileId Master file ID
 * @param string $content Slave file content
 * @param string $prefixName Prefix to identify the slave file
 * @param string $fileExtension File extension
 * @return string|false Slave file ID on success, false on failure
 */
function uploadSlaveFromBuffer($tracker, $masterFileId, $content, $prefixName, $fileExtension) {
    try {
        echo "\n--- Uploading Slave File from Buffer ---\n";
        echo "Master File ID: $masterFileId\n";
        echo "Content size: " . strlen($content) . " bytes\n";
        echo "Prefix: $prefixName\n";
        echo "Extension: $fileExtension\n";
        
        // Upload slave file content from memory
        $slaveFileId = fastdfs_storage_upload_slave_by_filebuff(
            $content,
            $masterFileId,
            $prefixName,
            $fileExtension,
            [],  // Metadata
            [],  // Group name
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$slaveFileId) {
            throw new Exception("Slave buffer upload failed");
        }
        
        echo "✓ Slave file uploaded from buffer!\n";
        echo "Slave File ID: $slaveFileId\n";
        
        return $slaveFileId;
        
    } catch (Exception $e) {
        echo "✗ Slave buffer upload error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Download a slave file to local disk
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $slaveFileId Slave file ID
 * @param string $localPath Local path to save the file
 * @return bool True on success, false on failure
 */
function downloadSlaveFile($tracker, $slaveFileId, $localPath) {
    try {
        echo "\n--- Downloading Slave File ---\n";
        echo "Slave File ID: $slaveFileId\n";
        echo "Save to: $localPath\n";
        
        // Download slave file (same as regular file download)
        $result = fastdfs_storage_download_file_to_file(
            $slaveFileId,
            $localPath,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Download failed");
        }
        
        if (!file_exists($localPath)) {
            throw new Exception("File was not saved to disk");
        }
        
        $fileSize = filesize($localPath);
        echo "✓ Download successful!\n";
        echo "File size: $fileSize bytes\n";
        
        return true;
        
    } catch (Exception $e) {
        echo "✗ Download error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Download slave file to memory buffer
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $slaveFileId Slave file ID
 * @return string|false File content on success, false on failure
 */
function downloadSlaveToBuffer($tracker, $slaveFileId) {
    try {
        echo "\n--- Downloading Slave File to Buffer ---\n";
        echo "Slave File ID: $slaveFileId\n";
        
        $content = fastdfs_storage_download_file_to_buff(
            $slaveFileId,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if ($content === false) {
            throw new Exception("Download to buffer failed");
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
 * Delete a slave file
 * 
 * Note: Deleting a master file does NOT automatically delete its slave files.
 * You must delete slave files explicitly.
 * 
 * @param resource $tracker FastDFS tracker connection
 * @param string $slaveFileId Slave file ID to delete
 * @return bool True on success, false on failure
 */
function deleteSlaveFile($tracker, $slaveFileId) {
    try {
        echo "\n--- Deleting Slave File ---\n";
        echo "Slave File ID: $slaveFileId\n";
        
        // Delete the slave file
        $result = fastdfs_storage_delete_file(
            $slaveFileId,
            TRACKER_HOST,
            TRACKER_PORT
        );
        
        if (!$result) {
            throw new Exception("Delete operation failed");
        }
        
        echo "✓ Slave file deleted successfully!\n";
        return true;
        
    } catch (Exception $e) {
        echo "✗ Delete error: " . $e->getMessage() . "\n";
        return false;
    }
}

/**
 * Create a test image file for demonstration
 * 
 * @param string $filename Filename to create
 * @param int $width Image width
 * @param int $height Image height
 * @param string $text Text to display on image
 * @return string Path to created file
 */
function createTestImage($filename, $width, $height, $text) {
    $filepath = __DIR__ . '/' . $filename;
    
    // Create a simple image using GD library if available
    if (function_exists('imagecreate')) {
        $image = imagecreate($width, $height);
        $bgColor = imagecolorallocate($image, 200, 200, 200);
        $textColor = imagecolorallocate($image, 0, 0, 0);
        
        imagestring($image, 5, 10, $height / 2 - 10, $text, $textColor);
        imagejpeg($image, $filepath);
        imagedestroy($image);
    } else {
        // Fallback: create a text file if GD is not available
        file_put_contents($filepath, "Image placeholder: $text\nSize: {$width}x{$height}");
    }
    
    return $filepath;
}

/**
 * Demonstrate image thumbnail scenario
 * 
 * This shows a common use case: uploading an original image and multiple
 * thumbnail versions as slave files.
 * 
 * @param resource $tracker FastDFS tracker connection
 */
function demonstrateImageThumbnails($tracker) {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Image Thumbnail Scenario ===\n";
    echo str_repeat("=", 70) . "\n";
    
    // Create original image
    $originalImage = createTestImage('original.jpg', 800, 600, 'Original Image');
    echo "\nCreated test image: $originalImage\n";
    
    // Upload master file (original image)
    $metadata = [
        'type' => 'image',
        'original_name' => 'photo.jpg',
        'upload_date' => date('Y-m-d H:i:s')
    ];
    
    $masterFileId = uploadMasterFile($tracker, $originalImage, $metadata);
    
    if (!$masterFileId) {
        return;
    }
    
    // Create and upload thumbnail versions as slave files
    $thumbnails = [
        ['prefix' => '_thumb_small', 'width' => 150, 'height' => 150, 'label' => 'Small Thumb'],
        ['prefix' => '_thumb_medium', 'width' => 300, 'height' => 300, 'label' => 'Medium Thumb'],
        ['prefix' => '_thumb_large', 'width' => 600, 'height' => 600, 'label' => 'Large Thumb']
    ];
    
    $slaveFileIds = [];
    
    foreach ($thumbnails as $thumb) {
        // Create thumbnail image
        $thumbFile = createTestImage(
            "thumb_{$thumb['width']}x{$thumb['height']}.jpg",
            $thumb['width'],
            $thumb['height'],
            $thumb['label']
        );
        
        // Upload as slave file
        $slaveId = uploadSlaveFile(
            $tracker,
            $masterFileId,
            $thumbFile,
            $thumb['prefix'],
            'jpg'
        );
        
        if ($slaveId) {
            $slaveFileIds[$thumb['prefix']] = $slaveId;
        }
        
        // Clean up local thumbnail file
        @unlink($thumbFile);
    }
    
    // Summary
    echo "\n--- Image Upload Summary ---\n";
    echo "Master File ID: $masterFileId\n";
    echo "\nSlave Files (Thumbnails):\n";
    foreach ($slaveFileIds as $prefix => $fileId) {
        echo "  $prefix: $fileId\n";
    }
    
    // Clean up original image
    @unlink($originalImage);
    
    return [
        'master' => $masterFileId,
        'slaves' => $slaveFileIds
    ];
}

/**
 * Demonstrate document conversion scenario
 * 
 * Shows uploading a document and its converted versions (e.g., PDF and images)
 * 
 * @param resource $tracker FastDFS tracker connection
 */
function demonstrateDocumentConversion($tracker) {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Document Conversion Scenario ===\n";
    echo str_repeat("=", 70) . "\n";
    
    // Create master document
    $docContent = "FastDFS Slave File Example\n\n";
    $docContent .= "This is a sample document that demonstrates slave file operations.\n";
    $docContent .= "In a real scenario, this could be a Word document, PDF, or other format.\n";
    $docContent .= "\nCreated: " . date('Y-m-d H:i:s') . "\n";
    
    $docFile = __DIR__ . '/document.txt';
    file_put_contents($docFile, $docContent);
    
    // Upload master document
    $masterFileId = uploadMasterFile($tracker, $docFile, [
        'type' => 'document',
        'format' => 'text'
    ]);
    
    if (!$masterFileId) {
        @unlink($docFile);
        return;
    }
    
    // Create and upload converted versions as slave files
    
    // 1. Compressed version
    $compressedContent = gzcompress($docContent);
    $compressedId = uploadSlaveFromBuffer(
        $tracker,
        $masterFileId,
        $compressedContent,
        '_compressed',
        'gz'
    );
    
    // 2. HTML version
    $htmlContent = "<html><body><pre>" . htmlspecialchars($docContent) . "</pre></body></html>";
    $htmlId = uploadSlaveFromBuffer(
        $tracker,
        $masterFileId,
        $htmlContent,
        '_html',
        'html'
    );
    
    // 3. JSON metadata version
    $jsonContent = json_encode([
        'title' => 'FastDFS Example Document',
        'content' => $docContent,
        'created' => date('Y-m-d H:i:s'),
        'length' => strlen($docContent)
    ], JSON_PRETTY_PRINT);
    
    $jsonId = uploadSlaveFromBuffer(
        $tracker,
        $masterFileId,
        $jsonContent,
        '_metadata',
        'json'
    );
    
    // Summary
    echo "\n--- Document Conversion Summary ---\n";
    echo "Master Document ID: $masterFileId\n";
    echo "\nConverted Versions (Slave Files):\n";
    if ($compressedId) echo "  Compressed (_compressed.gz): $compressedId\n";
    if ($htmlId) echo "  HTML (_html.html): $htmlId\n";
    if ($jsonId) echo "  JSON Metadata (_metadata.json): $jsonId\n";
    
    // Test downloading a slave file
    if ($htmlId) {
        $content = downloadSlaveToBuffer($tracker, $htmlId);
        if ($content) {
            echo "\n--- Downloaded HTML Version (first 200 chars) ---\n";
            echo substr($content, 0, 200) . "...\n";
        }
    }
    
    // Clean up
    @unlink($docFile);
    
    return [
        'master' => $masterFileId,
        'slaves' => [
            'compressed' => $compressedId,
            'html' => $htmlId,
            'json' => $jsonId
        ]
    ];
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Slave File Operations Example ===\n\n";
    
    echo "Slave files allow you to store related/derived files linked to a master file.\n";
    echo "Common use cases:\n";
    echo "  - Image thumbnails of different sizes\n";
    echo "  - Video files in different resolutions\n";
    echo "  - Document format conversions\n";
    echo "  - Compressed versions of files\n\n";
    
    // Initialize connection
    $tracker = initializeFastDFS();
    if (!$tracker) {
        exit(1);
    }
    
    // Scenario 1: Image thumbnails
    $imageResult = demonstrateImageThumbnails($tracker);
    
    // Scenario 2: Document conversion
    $docResult = demonstrateDocumentConversion($tracker);
    
    // Final summary
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Summary ===\n";
    echo str_repeat("=", 70) . "\n";
    
    echo "\nOperations demonstrated:\n";
    echo "✓ Upload master file\n";
    echo "✓ Upload slave files from disk\n";
    echo "✓ Upload slave files from buffer\n";
    echo "✓ Download slave files\n";
    echo "✓ Multiple slave files per master\n";
    
    echo "\nKey Points:\n";
    echo "- Slave files are linked to master files using prefix/suffix\n";
    echo "- Multiple slave files can be associated with one master file\n";
    echo "- Slave files are stored in the same group as the master file\n";
    echo "- Deleting master file does NOT auto-delete slave files\n";
    echo "- Slave files can have different extensions than master file\n";
    echo "- Use descriptive prefixes to identify slave file purpose\n";
    
    echo "\nFile IDs for testing:\n";
    if ($imageResult) {
        echo "\nImage Example:\n";
        echo "  Master: {$imageResult['master']}\n";
        foreach ($imageResult['slaves'] as $prefix => $id) {
            echo "  Slave $prefix: $id\n";
        }
    }
    
    if ($docResult) {
        echo "\nDocument Example:\n";
        echo "  Master: {$docResult['master']}\n";
        foreach ($docResult['slaves'] as $type => $id) {
            echo "  Slave $type: $id\n";
        }
    }
    
    // Close tracker connection
    fastdfs_tracker_close_connection($tracker);
    echo "\n✓ Connection closed\n";
}

// Run the example
main();

?>
