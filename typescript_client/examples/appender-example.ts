/**
 * FastDFS Appender File Operations Example
 * 
 * This example demonstrates appender file operations:
 * - Uploading appender files
 * - Appending data (Note: requires storage server support)
 */

import { Client, ClientConfig } from '../src';

async function main() {
  console.log('FastDFS TypeScript Client - Appender File Example');
  console.log('='.repeat(50));

  // Configure client
  const config: ClientConfig = {
    trackerAddrs: ['192.168.1.100:22122'], // Replace with your tracker address
  };

  const client = new Client(config);

  try {
    // Example 1: Upload appender file
    console.log('\n1. Uploading appender file...');
    const initialData = Buffer.from('Initial log entry\n');
    const fileId = await client.uploadAppenderBuffer(initialData, 'log');
    console.log('   Uploaded successfully!');
    console.log(`   File ID: ${fileId}`);

    // Example 2: Get initial file info
    console.log('\n2. Getting initial file information...');
    const fileInfo = await client.getFileInfo(fileId);
    console.log(`   File size: ${fileInfo.fileSize} bytes`);
    console.log(`   Create time: ${fileInfo.createTime}`);

    // Example 3: Download and display content
    console.log('\n3. Downloading file content...');
    const content = await client.downloadFile(fileId);
    console.log(`   Content:\n${content.toString()}`);

    // Note: Append, modify, and truncate operations require
    // storage server configuration to support appender files.
    console.log('\n4. Appender file operations:');
    console.log('   - Append: Adds data to the end of the file');
    console.log('   - Modify: Changes data at a specific offset');
    console.log('   - Truncate: Reduces file size to specified length');
    console.log('   Note: These operations require storage server support');

    // Clean up
    console.log('\n5. Cleaning up...');
    await client.deleteFile(fileId);
    console.log('   File deleted successfully!');

    console.log('\n' + '='.repeat(50));
    console.log('Example completed successfully!');
  } catch (error) {
    console.error('\nError:', error);
  } finally {
    await client.close();
  }
}

main();