/**
 * FastDFS Metadata Operations Example
 * 
 * This example demonstrates metadata operations:
 * - Uploading files with metadata
 * - Setting metadata
 * - Getting metadata
 * - Merging metadata
 */

import { Client, ClientConfig, MetadataFlag } from '../src';

async function main() {
  console.log('FastDFS TypeScript Client - Metadata Example');
  console.log('='.repeat(50));

  // Configure client
  const config: ClientConfig = {
    trackerAddrs: ['192.168.1.100:22122'], // Replace with your tracker address
  };

  // Create client
  const client = new Client(config);

  try {
    // Example 1: Upload with metadata
    console.log('\n1. Uploading file with metadata...');
    const testData = Buffer.from('Document content with metadata');
    const metadata = {
      author: 'John Doe',
      date: '2025-01-15',
      version: '1.0',
      department: 'Engineering',
    };

    const fileId = await client.uploadBuffer(testData, 'txt', metadata);
    console.log('   Uploaded successfully!');
    console.log(`   File ID: ${fileId}`);

    // Example 2: Get metadata
    console.log('\n2. Getting metadata...');
    let retrievedMetadata = await client.getMetadata(fileId);
    console.log('   Metadata:');
    Object.entries(retrievedMetadata).forEach(([key, value]) => {
      console.log(`     ${key}: ${value}`);
    });

    // Example 3: Update metadata (overwrite)
    console.log('\n3. Updating metadata (overwrite mode)...');
    const newMetadata = {
      author: 'Jane Smith',
      date: '2025-01-16',
      status: 'reviewed',
    };
    await client.setMetadata(fileId, newMetadata, MetadataFlag.OVERWRITE);

    retrievedMetadata = await client.getMetadata(fileId);
    console.log('   Updated metadata:');
    Object.entries(retrievedMetadata).forEach(([key, value]) => {
      console.log(`     ${key}: ${value}`);
    });

    // Example 4: Merge metadata
    console.log('\n4. Merging metadata...');
    const mergeMetadata = {
      reviewer: 'Bob Johnson',
      comments: 'Approved',
    };
    await client.setMetadata(fileId, mergeMetadata, MetadataFlag.MERGE);

    retrievedMetadata = await client.getMetadata(fileId);
    console.log('   Merged metadata:');
    Object.entries(retrievedMetadata).forEach(([key, value]) => {
      console.log(`     ${key}: ${value}`);
    });

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