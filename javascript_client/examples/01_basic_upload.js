/**
 * FastDFS JavaScript Client - Basic Upload Example
 * 
 * This example demonstrates the basic file upload operations:
 * - Uploading files from filesystem
 * - Uploading data from buffers
 * - Getting file information
 * - Checking file existence
 * - Deleting files
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 */

'use strict';

const { Client } = require('../src');
const fs = require('fs').promises;

/**
 * Main example function
 */
async function main() {
  console.log('FastDFS JavaScript Client - Basic Upload Example');
  console.log('='.repeat(60));

  // Configure client
  // Replace with your actual tracker server address
  const config = {
    trackerAddrs: ['192.168.1.100:22122'],
    maxConns: 10,
    connectTimeout: 5000,
    networkTimeout: 30000,
  };

  // Create client instance
  const client = new Client(config);

  try {
    // ========================================================================
    // Example 1: Upload from buffer
    // ========================================================================
    console.log('\n1. Uploading data from buffer...');
    const testData = Buffer.from('Hello, FastDFS! This is a test file from JavaScript client.');
    const fileId1 = await client.uploadBuffer(testData, 'txt');
    console.log('   ✓ Uploaded successfully!');
    console.log(`   File ID: ${fileId1}`);

    // ========================================================================
    // Example 2: Upload with metadata
    // ========================================================================
    console.log('\n2. Uploading with metadata...');
    const metadata = {
      author: 'John Doe',
      date: new Date().toISOString(),
      description: 'Test file with metadata',
    };
    const fileId2 = await client.uploadBuffer(
      Buffer.from('File with metadata'),
      'txt',
      metadata
    );
    console.log('   ✓ Uploaded successfully!');
    console.log(`   File ID: ${fileId2}`);

    // ========================================================================
    // Example 3: Get file information
    // ========================================================================
    console.log('\n3. Getting file information...');
    const fileInfo = await client.getFileInfo(fileId1);
    console.log(`   File size: ${fileInfo.fileSize} bytes`);
    console.log(`   Create time: ${fileInfo.createTime.toISOString()}`);
    console.log(`   CRC32: 0x${fileInfo.crc32.toString(16).toUpperCase()}`);
    console.log(`   Source IP: ${fileInfo.sourceIpAddr}`);

    // ========================================================================
    // Example 4: Download file
    // ========================================================================
    console.log('\n4. Downloading file...');
    const downloadedData = await client.downloadFile(fileId1);
    console.log(`   ✓ Downloaded ${downloadedData.length} bytes`);
    console.log(`   Content: ${downloadedData.toString()}`);

    // ========================================================================
    // Example 5: Check if file exists
    // ========================================================================
    console.log('\n5. Checking file existence...');
    let exists = await client.fileExists(fileId1);
    console.log(`   File exists: ${exists ? '✓ Yes' : '✗ No'}`);

    // ========================================================================
    // Example 6: Delete file
    // ========================================================================
    console.log('\n6. Deleting file...');
    await client.deleteFile(fileId1);
    console.log('   ✓ File deleted successfully!');

    // Verify deletion
    exists = await client.fileExists(fileId1);
    console.log(`   File exists after deletion: ${exists ? '✓ Yes' : '✗ No'}`);

    // ========================================================================
    // Example 7: Clean up second file
    // ========================================================================
    console.log('\n7. Cleaning up...');
    await client.deleteFile(fileId2);
    console.log('   ✓ All test files deleted');

    console.log('\n' + '='.repeat(60));
    console.log('✓ Example completed successfully!');
  } catch (error) {
    console.error('\n✗ Error:', error.message);
    console.error('Stack trace:', error.stack);
    process.exit(1);
  } finally {
    // Always close the client to release resources
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
