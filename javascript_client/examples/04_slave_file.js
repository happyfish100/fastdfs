/**
 * FastDFS JavaScript Client - Slave File Example
 * 
 * This example demonstrates slave file operations:
 * - Uploading master files
 * - Uploading slave files (thumbnails, previews, etc.)
 * - Managing relationships between master and slave files
 * 
 * Slave files are typically used for thumbnails or different versions
 * of the same content (e.g., different image sizes).
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 */

'use strict';

const { Client } = require('../src');

/**
 * Simulates creating a thumbnail from image data
 * In a real application, you would use an image processing library
 * 
 * @param {Buffer} imageData - Original image data
 * @returns {Buffer} Thumbnail data (simulated)
 */
function createThumbnail(imageData) {
  // In a real application, you would resize the image here
  // For this example, we just create a smaller buffer with metadata
  return Buffer.from(`[THUMBNAIL] Original size: ${imageData.length} bytes`);
}

/**
 * Simulates creating a preview from image data
 * 
 * @param {Buffer} imageData - Original image data
 * @returns {Buffer} Preview data (simulated)
 */
function createPreview(imageData) {
  // In a real application, you would create a medium-sized version
  return Buffer.from(`[PREVIEW] Original size: ${imageData.length} bytes`);
}

/**
 * Main example function
 */
async function main() {
  console.log('FastDFS JavaScript Client - Slave File Example');
  console.log('='.repeat(60));

  // Configure client
  const config = {
    trackerAddrs: ['192.168.1.100:22122'],
    maxConns: 10,
    connectTimeout: 5000,
    networkTimeout: 30000,
  };

  const client = new Client(config);

  try {
    // ========================================================================
    // Example 1: Upload master file (original image)
    // ========================================================================
    console.log('\n1. Uploading master file (original image)...');
    
    // Simulate original image data
    const originalImage = Buffer.from('This is the original high-resolution image data. ' +
                                     'In a real application, this would be actual image bytes.');
    
    const masterMetadata = {
      type: 'image',
      format: 'jpg',
      width: '1920',
      height: '1080',
      quality: 'high',
    };

    const masterFileId = await client.uploadBuffer(originalImage, 'jpg', masterMetadata);
    console.log('   ✓ Master file uploaded successfully!');
    console.log(`   Master File ID: ${masterFileId}`);

    // ========================================================================
    // Example 2: Upload thumbnail (slave file)
    // ========================================================================
    console.log('\n2. Uploading thumbnail (slave file)...');
    
    const thumbnailData = createThumbnail(originalImage);
    const thumbnailMetadata = {
      type: 'thumbnail',
      width: '150',
      height: '150',
    };

    const thumbnailFileId = await client.uploadSlaveFile(
      masterFileId,
      'thumb',      // Prefix name
      'jpg',        // File extension
      thumbnailData,
      thumbnailMetadata
    );
    console.log('   ✓ Thumbnail uploaded successfully!');
    console.log(`   Thumbnail File ID: ${thumbnailFileId}`);

    // ========================================================================
    // Example 3: Upload preview (another slave file)
    // ========================================================================
    console.log('\n3. Uploading preview (another slave file)...');
    
    const previewData = createPreview(originalImage);
    const previewMetadata = {
      type: 'preview',
      width: '800',
      height: '600',
    };

    const previewFileId = await client.uploadSlaveFile(
      masterFileId,
      'preview',    // Prefix name
      'jpg',
      previewData,
      previewMetadata
    );
    console.log('   ✓ Preview uploaded successfully!');
    console.log(`   Preview File ID: ${previewFileId}`);

    // ========================================================================
    // Example 4: Upload small version (yet another slave file)
    // ========================================================================
    console.log('\n4. Uploading small version (slave file)...');
    
    const smallData = Buffer.from('[SMALL] Optimized for mobile devices');
    const smallMetadata = {
      type: 'small',
      width: '320',
      height: '240',
      optimized: 'mobile',
    };

    const smallFileId = await client.uploadSlaveFile(
      masterFileId,
      'small',
      'jpg',
      smallData,
      smallMetadata
    );
    console.log('   ✓ Small version uploaded successfully!');
    console.log(`   Small File ID: ${smallFileId}`);

    // ========================================================================
    // Example 5: Display file information
    // ========================================================================
    console.log('\n5. Displaying file information...');
    
    const files = [
      { id: masterFileId, name: 'Master (Original)' },
      { id: thumbnailFileId, name: 'Thumbnail' },
      { id: previewFileId, name: 'Preview' },
      { id: smallFileId, name: 'Small' },
    ];

    for (const file of files) {
      const info = await client.getFileInfo(file.id);
      const metadata = await client.getMetadata(file.id);
      
      console.log(`\n   ${file.name}:`);
      console.log(`     File ID: ${file.id}`);
      console.log(`     Size: ${info.fileSize} bytes`);
      console.log(`     Created: ${info.createTime.toISOString()}`);
      console.log(`     Metadata:`);
      for (const [key, value] of Object.entries(metadata)) {
        console.log(`       - ${key}: ${value}`);
      }
    }

    // ========================================================================
    // Example 6: Download and verify files
    // ========================================================================
    console.log('\n6. Downloading and verifying files...');
    
    for (const file of files) {
      const data = await client.downloadFile(file.id);
      console.log(`   ✓ ${file.name}: ${data.length} bytes`);
      console.log(`     Content preview: ${data.toString().substring(0, 50)}...`);
    }

    // ========================================================================
    // Example 7: Clean up - Delete all files
    // ========================================================================
    console.log('\n7. Cleaning up...');
    
    // Delete slave files first
    await client.deleteFile(thumbnailFileId);
    console.log('   ✓ Thumbnail deleted');
    
    await client.deleteFile(previewFileId);
    console.log('   ✓ Preview deleted');
    
    await client.deleteFile(smallFileId);
    console.log('   ✓ Small version deleted');
    
    // Delete master file last
    await client.deleteFile(masterFileId);
    console.log('   ✓ Master file deleted');

    console.log('\n' + '='.repeat(60));
    console.log('✓ Example completed successfully!');
    console.log('\nNote: In a real application, you would:');
    console.log('  - Use actual image processing libraries (sharp, jimp, etc.)');
    console.log('  - Generate real thumbnails and previews');
    console.log('  - Store file relationships in your database');
    console.log('  - Implement proper error handling for file operations');
  } catch (error) {
    console.error('\n✗ Error:', error.message);
    console.error('Stack trace:', error.stack);
    process.exit(1);
  } finally {
    await client.close();
    console.log('\nClient closed.');
  }
}

// Run the example
if (require.main === module) {
  main().catch(error => {
    console.error('Fatal error:', error);
    process.exit(1);
  });
}

module.exports = { main };
