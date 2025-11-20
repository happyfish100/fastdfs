/**
 * Basic FastDFS Client Usage Example
 * 
 * This example demonstrates the basic operations:
 * - Uploading files
 * - Downloading files
 * - Deleting files
 */

import { Client, ClientConfig } from '../src';

async function main() {
  console.log('FastDFS TypeScript Client - Basic Usage Example');
  console.log('='.repeat(50));

  // Configure client
  const config: ClientConfig = {
    trackerAddrs: ['192.168.1.100:22122'], // Replace with your tracker address
    maxConns: 10,
    connectTimeout: 5000,
    networkTimeout: 30000,
  };

  // Create client
  const client = new Client(config);

  try {
    // Example 1: Upload from buffer
    console.log('\n1. Uploading data from buffer...');
    const testData = Buffer.from('Hello, FastDFS! This is a test file.');
    const fileId = await client.uploadBuffer(testData, 'txt');
    console.log('   Uploaded successfully!');
    console.log(`   File ID: ${fileId}`);

    // Example 2: Download file
    console.log('\n2. Downloading file...');
    const downloadedData = await client.downloadFile(fileId);
    console.log(`   Downloaded ${downloadedData.length} bytes`);
    console.log(`   Content: ${downloadedData.toString()}`);

    // Example 3: Get file information
    console.log('\n3. Getting file information...');
    const fileInfo = await client.getFileInfo(fileId);
    console.log(`   File size: ${fileInfo.fileSize} bytes`);
    console.log(`   Create time: ${fileInfo.createTime}`);
    console.log(`   CRC32: ${fileInfo.crc32}`);
    console.log(`   Source IP: ${fileInfo.sourceIpAddr}`);

    // Example 4: Check if file exists
    console.log('\n4. Checking file existence...');
    let exists = await client.fileExists(fileId);
    console.log(`   File exists: ${exists}`);

    // Example 5: Delete file
    console.log('\n5. Deleting file...');
    await client.deleteFile(fileId);
    console.log('   File deleted successfully!');

    // Verify deletion
    exists = await client.fileExists(fileId);
    console.log(`   File exists after deletion: ${exists}`);

    console.log('\n' + '='.repeat(50));
    console.log('Example completed successfully!');
  } catch (error) {
    console.error('\nError:', error);
  } finally {
    // Always close the client
    await client.close();
    console.log('\nClient closed.');
  }
}

main();