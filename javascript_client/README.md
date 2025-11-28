# FastDFS JavaScript Client

Official JavaScript/Node.js client library for [FastDFS](https://github.com/happyfish100/fastdfs) distributed file system.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Node.js Version](https://img.shields.io/badge/node-%3E%3D12.0.0-brightgreen.svg)](https://nodejs.org/)

## Features

- ✅ **Complete Protocol Support** - Full implementation of FastDFS protocol
- ✅ **Connection Pooling** - Efficient connection management with automatic reuse
- ✅ **Automatic Retry** - Built-in retry logic for transient failures
- ✅ **Async/Await** - Modern Promise-based API
- ✅ **TypeScript Ready** - JSDoc annotations for excellent IDE support
- ✅ **Comprehensive Error Handling** - Detailed error types for all scenarios
- ✅ **Metadata Support** - Store and retrieve custom file metadata
- ✅ **Appender Files** - Support for files that can be modified after upload
- ✅ **Slave Files** - Upload thumbnails and related files
- ✅ **Partial Downloads** - Download specific byte ranges
- ✅ **Well Documented** - Extensive comments and examples

## Installation

```bash
npm install fastdfs-client
```

Or with yarn:

```bash
yarn add fastdfs-client
```

## Quick Start

```javascript
const { Client } = require('fastdfs-client');

// Create client
const client = new Client({
  trackerAddrs: ['192.168.1.100:22122']
});

async function example() {
  try {
    // Upload a file
    const fileId = await client.uploadBuffer(
      Buffer.from('Hello, FastDFS!'),
      'txt'
    );
    console.log('Uploaded:', fileId);

    // Download the file
    const data = await client.downloadFile(fileId);
    console.log('Downloaded:', data.toString());

    // Delete the file
    await client.deleteFile(fileId);
    console.log('Deleted');
  } finally {
    await client.close();
  }
}

example();
```

## Configuration

```javascript
const client = new Client({
  // Required: List of tracker server addresses
  trackerAddrs: ['192.168.1.100:22122', '192.168.1.101:22122'],
  
  // Optional: Maximum connections per server (default: 10)
  maxConns: 10,
  
  // Optional: Connection timeout in milliseconds (default: 5000)
  connectTimeout: 5000,
  
  // Optional: Network I/O timeout in milliseconds (default: 30000)
  networkTimeout: 30000,
  
  // Optional: Idle connection timeout in milliseconds (default: 60000)
  idleTimeout: 60000,
  
  // Optional: Number of retries for failed operations (default: 3)
  retryCount: 3
});
```

## API Reference

### File Upload

#### `uploadFile(localFilename, metadata?)`

Upload a file from the local filesystem.

```javascript
const fileId = await client.uploadFile('/path/to/file.jpg');

// With metadata
const fileId = await client.uploadFile('/path/to/file.jpg', {
  author: 'John Doe',
  date: '2025-01-01'
});
```

#### `uploadBuffer(data, fileExtName, metadata?)`

Upload data from a Buffer.

```javascript
const data = Buffer.from('Hello, World!');
const fileId = await client.uploadBuffer(data, 'txt');
```

#### `uploadAppenderFile(localFilename, metadata?)`

Upload an appender file that can be modified later.

```javascript
const fileId = await client.uploadAppenderFile('/path/to/log.txt');
```

#### `uploadAppenderBuffer(data, fileExtName, metadata?)`

Upload an appender file from a Buffer.

```javascript
const data = Buffer.from('Initial log content\n');
const fileId = await client.uploadAppenderBuffer(data, 'log');
```

#### `uploadSlaveFile(masterFileId, prefixName, fileExtName, data, metadata?)`

Upload a slave file (thumbnail, preview, etc.) associated with a master file.

```javascript
const thumbnailData = createThumbnail(imageData);
const thumbId = await client.uploadSlaveFile(
  masterFileId,
  'thumb',
  'jpg',
  thumbnailData
);
```

### File Download

#### `downloadFile(fileId)`

Download a complete file.

```javascript
const data = await client.downloadFile(fileId);
```

#### `downloadFileRange(fileId, offset, length)`

Download a specific byte range.

```javascript
// Download first 1024 bytes
const header = await client.downloadFileRange(fileId, 0, 1024);

// Download from offset 1000 to end
const tail = await client.downloadFileRange(fileId, 1000, 0);
```

#### `downloadToFile(fileId, localFilename)`

Download and save to local filesystem.

```javascript
await client.downloadToFile(fileId, '/path/to/save/file.jpg');
```

### File Management

#### `deleteFile(fileId)`

Delete a file from FastDFS.

```javascript
await client.deleteFile(fileId);
```

#### `getFileInfo(fileId)`

Get file information (size, create time, CRC32, source IP).

```javascript
const info = await client.getFileInfo(fileId);
console.log('Size:', info.fileSize);
console.log('Created:', info.createTime);
console.log('CRC32:', info.crc32);
console.log('Source IP:', info.sourceIpAddr);
```

#### `fileExists(fileId)`

Check if a file exists.

```javascript
const exists = await client.fileExists(fileId);
```

### Metadata Operations

#### `setMetadata(fileId, metadata, flag?)`

Set or update file metadata.

```javascript
// Overwrite all metadata (default)
await client.setMetadata(fileId, {
  author: 'John Doe',
  version: '2.0'
}, 'OVERWRITE');

// Merge with existing metadata
await client.setMetadata(fileId, {
  tags: 'important'
}, 'MERGE');
```

#### `getMetadata(fileId)`

Retrieve file metadata.

```javascript
const metadata = await client.getMetadata(fileId);
console.log('Author:', metadata.author);
```

### Appender File Operations

#### `appendFile(fileId, data)`

Append data to an appender file.

```javascript
await client.appendFile(fileId, Buffer.from('New log entry\n'));
```

#### `modifyFile(fileId, offset, data)`

Modify content at a specific offset.

```javascript
await client.modifyFile(fileId, 0, Buffer.from('Modified header\n'));
```

#### `truncateFile(fileId, size)`

Truncate file to specified size.

```javascript
await client.truncateFile(fileId, 1024); // Truncate to 1KB
```

### Client Management

#### `close()`

Close the client and release all resources.

```javascript
await client.close();
```

## Examples

The `examples/` directory contains comprehensive examples:

- **01_basic_upload.js** - Basic file upload, download, and deletion
- **02_metadata_operations.js** - Working with file metadata
- **03_appender_file.js** - Appender file operations (append, modify, truncate)
- **04_slave_file.js** - Slave file management (thumbnails, previews)

Run an example:

```bash
node examples/01_basic_upload.js
```

## Error Handling

The client provides detailed error types for different scenarios:

```javascript
const {
  Client,
  FileNotFoundError,
  NetworkError,
  InvalidFileIDError,
  ClientClosedError
} = require('fastdfs-client');

try {
  await client.downloadFile(fileId);
} catch (error) {
  if (error instanceof FileNotFoundError) {
    console.log('File does not exist');
  } else if (error instanceof NetworkError) {
    console.log('Network communication failed');
  } else if (error instanceof InvalidFileIDError) {
    console.log('Invalid file ID format');
  } else {
    console.log('Other error:', error.message);
  }
}
```

### Available Error Types

- `FastDFSError` - Base error class
- `ClientClosedError` - Client has been closed
- `FileNotFoundError` - File does not exist
- `NoStorageServerError` - No storage server available
- `ConnectionTimeoutError` - Connection timeout
- `NetworkTimeoutError` - Network I/O timeout
- `InvalidFileIDError` - Invalid file ID format
- `InvalidResponseError` - Invalid server response
- `StorageServerOfflineError` - Storage server offline
- `TrackerServerOfflineError` - Tracker server offline
- `InsufficientSpaceError` - Insufficient storage space
- `FileAlreadyExistsError` - File already exists
- `InvalidMetadataError` - Invalid metadata format
- `OperationNotSupportedError` - Operation not supported
- `InvalidArgumentError` - Invalid argument
- `ProtocolError` - Protocol-level error
- `NetworkError` - Network communication error

## Best Practices

### 1. Always Close the Client

```javascript
const client = new Client(config);
try {
  // Your operations
} finally {
  await client.close();
}
```

### 2. Use Connection Pooling

The client automatically manages connection pooling. Configure `maxConns` based on your workload:

```javascript
const client = new Client({
  trackerAddrs: ['192.168.1.100:22122'],
  maxConns: 50 // For high-concurrency applications
});
```

### 3. Handle Errors Appropriately

```javascript
try {
  const fileId = await client.uploadFile('file.jpg');
} catch (error) {
  if (error instanceof NetworkError) {
    // Retry or log for investigation
  } else if (error instanceof InvalidArgumentError) {
    // Fix the input
  } else {
    // Handle other errors
  }
}
```

### 4. Use Metadata for File Management

```javascript
await client.uploadBuffer(data, 'jpg', {
  userId: '12345',
  uploadTime: new Date().toISOString(),
  originalName: 'photo.jpg'
});
```

### 5. Leverage Slave Files for Variants

```javascript
// Upload original
const originalId = await client.uploadBuffer(imageData, 'jpg');

// Upload thumbnail
const thumbData = await resizeImage(imageData, 150, 150);
const thumbId = await client.uploadSlaveFile(
  originalId, 'thumb', 'jpg', thumbData
);
```

## Performance Tips

1. **Reuse Client Instances** - Create one client and reuse it across your application
2. **Adjust Connection Pool Size** - Increase `maxConns` for high-concurrency scenarios
3. **Use Appropriate Timeouts** - Adjust timeouts based on your network conditions
4. **Batch Operations** - When possible, batch multiple operations together
5. **Monitor Connection Pool** - The pool automatically manages connections, but monitor for leaks

## Requirements

- Node.js >= 12.0.0
- FastDFS server 6.0.0 or later

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Support

- **Issues**: [GitHub Issues](https://github.com/happyfish100/fastdfs/issues)
- **Documentation**: [FastDFS Wiki](https://github.com/happyfish100/fastdfs/wiki)

## Acknowledgments

- FastDFS team for creating this excellent distributed file system
- All contributors to this JavaScript client

## Related Projects

- [FastDFS](https://github.com/happyfish100/fastdfs) - The main FastDFS project
- [fastdfs-nginx-module](https://github.com/happyfish100/fastdfs-nginx-module) - Nginx module for FastDFS
- Other language clients: Python, Go, Ruby, TypeScript, Rust, C#, Groovy

---

Made with ❤️ for the FastDFS community
