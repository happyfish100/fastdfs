/**
 * FastDFS JavaScript Client - Appender File Example
 * 
 * This example demonstrates appender file operations:
 * - Uploading appender files
 * - Appending data to files
 * - Modifying file content
 * - Truncating files
 * 
 * Appender files are useful for log files or files that grow over time.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 */

'use strict';

const { Client } = require('../src');

/**
 * Main example function
 */
async function main() {
  console.log('FastDFS JavaScript Client - Appender File Example');
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
    // Example 1: Upload an appender file
    // ========================================================================
    console.log('\n1. Uploading appender file...');
    const initialContent = Buffer.from('Log Entry 1: Application started\n');
    const fileId = await client.uploadAppenderBuffer(initialContent, 'log');
    console.log('   ✓ Appender file uploaded successfully!');
    console.log(`   File ID: ${fileId}`);

    // Check initial file info
    let fileInfo = await client.getFileInfo(fileId);
    console.log(`   Initial file size: ${fileInfo.fileSize} bytes`);

    // ========================================================================
    // Example 2: Append data to the file
    // ========================================================================
    console.log('\n2. Appending data to file...');
    
    const entries = [
      'Log Entry 2: User logged in\n',
      'Log Entry 3: Data processed successfully\n',
      'Log Entry 4: Cache updated\n',
      'Log Entry 5: Request completed\n',
    ];

    for (let i = 0; i < entries.length; i++) {
      await client.appendFile(fileId, Buffer.from(entries[i]));
      console.log(`   ✓ Appended entry ${i + 2}`);
    }

    // Check file size after appending
    fileInfo = await client.getFileInfo(fileId);
    console.log(`   File size after appending: ${fileInfo.fileSize} bytes`);

    // ========================================================================
    // Example 3: Download and display file content
    // ========================================================================
    console.log('\n3. Downloading file content...');
    let content = await client.downloadFile(fileId);
    console.log('   Current file content:');
    console.log('   ' + '-'.repeat(56));
    console.log(content.toString().split('\n').map(line => '   ' + line).join('\n'));
    console.log('   ' + '-'.repeat(56));

    // ========================================================================
    // Example 4: Modify file content
    // ========================================================================
    console.log('\n4. Modifying file content...');
    
    // Replace "Log Entry 1" with "Log Entry 0" (modify at offset 0)
    const modifiedHeader = Buffer.from('Log Entry 0: Application started\n');
    await client.modifyFile(fileId, 0, modifiedHeader);
    console.log('   ✓ Modified first line');

    // Download and display modified content
    content = await client.downloadFile(fileId);
    console.log('   Modified file content:');
    console.log('   ' + '-'.repeat(56));
    console.log(content.toString().split('\n').map(line => '   ' + line).join('\n'));
    console.log('   ' + '-'.repeat(56));

    // ========================================================================
    // Example 5: Truncate file
    // ========================================================================
    console.log('\n5. Truncating file...');
    
    // Get current size
    fileInfo = await client.getFileInfo(fileId);
    console.log(`   Current file size: ${fileInfo.fileSize} bytes`);
    
    // Truncate to keep only first 100 bytes
    const newSize = 100;
    await client.truncateFile(fileId, newSize);
    console.log(`   ✓ File truncated to ${newSize} bytes`);

    // Verify truncation
    fileInfo = await client.getFileInfo(fileId);
    console.log(`   New file size: ${fileInfo.fileSize} bytes`);

    // Download and display truncated content
    content = await client.downloadFile(fileId);
    console.log('   Truncated file content:');
    console.log('   ' + '-'.repeat(56));
    console.log(content.toString().split('\n').map(line => '   ' + line).join('\n'));
    console.log('   ' + '-'.repeat(56));

    // ========================================================================
    // Example 6: Append more data after truncation
    // ========================================================================
    console.log('\n6. Appending data after truncation...');
    await client.appendFile(fileId, Buffer.from('\nLog Entry 6: File resumed\n'));
    console.log('   ✓ Data appended after truncation');

    // Final content
    content = await client.downloadFile(fileId);
    console.log('   Final file content:');
    console.log('   ' + '-'.repeat(56));
    console.log(content.toString().split('\n').map(line => '   ' + line).join('\n'));
    console.log('   ' + '-'.repeat(56));

    // ========================================================================
    // Example 7: Clean up
    // ========================================================================
    console.log('\n7. Cleaning up...');
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
