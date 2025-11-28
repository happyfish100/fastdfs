/**
 * FastDFS JavaScript Client - Metadata Operations Example
 * 
 * This example demonstrates metadata management:
 * - Setting metadata on upload
 * - Getting metadata
 * - Updating metadata (overwrite mode)
 * - Merging metadata
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 */

'use strict';

const { Client } = require('../src');

/**
 * Main example function
 */
async function main() {
  console.log('FastDFS JavaScript Client - Metadata Operations Example');
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
    // Example 1: Upload file with initial metadata
    // ========================================================================
    console.log('\n1. Uploading file with metadata...');
    const initialMetadata = {
      title: 'My Document',
      author: 'John Doe',
      version: '1.0',
      category: 'documentation',
      created: new Date().toISOString(),
    };

    const fileData = Buffer.from('This is a document with metadata.');
    const fileId = await client.uploadBuffer(fileData, 'txt', initialMetadata);
    console.log('   ✓ File uploaded successfully!');
    console.log(`   File ID: ${fileId}`);

    // ========================================================================
    // Example 2: Retrieve metadata
    // ========================================================================
    console.log('\n2. Retrieving metadata...');
    let metadata = await client.getMetadata(fileId);
    console.log('   Current metadata:');
    for (const [key, value] of Object.entries(metadata)) {
      console.log(`     - ${key}: ${value}`);
    }

    // ========================================================================
    // Example 3: Update metadata (OVERWRITE mode)
    // ========================================================================
    console.log('\n3. Updating metadata (OVERWRITE mode)...');
    const newMetadata = {
      title: 'My Updated Document',
      author: 'Jane Smith',
      version: '2.0',
      modified: new Date().toISOString(),
    };

    await client.setMetadata(fileId, newMetadata, 'OVERWRITE');
    console.log('   ✓ Metadata updated (overwrite)');

    // Verify the update
    metadata = await client.getMetadata(fileId);
    console.log('   Updated metadata:');
    for (const [key, value] of Object.entries(metadata)) {
      console.log(`     - ${key}: ${value}`);
    }
    console.log('   Note: Old keys (category, created) were removed');

    // ========================================================================
    // Example 4: Merge metadata
    // ========================================================================
    console.log('\n4. Merging additional metadata...');
    const additionalMetadata = {
      tags: 'important,reviewed',
      status: 'published',
      reviewer: 'Bob Johnson',
    };

    await client.setMetadata(fileId, additionalMetadata, 'MERGE');
    console.log('   ✓ Metadata merged');

    // Verify the merge
    metadata = await client.getMetadata(fileId);
    console.log('   Final metadata (after merge):');
    for (const [key, value] of Object.entries(metadata)) {
      console.log(`     - ${key}: ${value}`);
    }
    console.log('   Note: New keys were added, existing keys were preserved');

    // ========================================================================
    // Example 5: Metadata with special characters
    // ========================================================================
    console.log('\n5. Testing metadata with special characters...');
    const specialMetadata = {
      'unicode-test': '你好世界 Hello World',
      'emoji-test': '🚀 FastDFS',
      'special-chars': 'Test: @#$%^&*()',
    };

    await client.setMetadata(fileId, specialMetadata, 'MERGE');
    metadata = await client.getMetadata(fileId);
    console.log('   ✓ Special characters handled correctly:');
    console.log(`     - unicode-test: ${metadata['unicode-test']}`);
    console.log(`     - emoji-test: ${metadata['emoji-test']}`);
    console.log(`     - special-chars: ${metadata['special-chars']}`);

    // ========================================================================
    // Example 6: Clean up
    // ========================================================================
    console.log('\n6. Cleaning up...');
    await client.deleteFile(fileId);
    console.log('   ✓ Test file deleted');

    console.log('\n' + '='.repeat(60));
    console.log('✓ Example completed successfully!');
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
