# FastDFS Java Client

Official Java client library for FastDFS - A high-performance distributed file system.

## Features

- ✅ File upload (normal, appender, slave files)
- ✅ File download (full and partial)
- ✅ File deletion
- ✅ Metadata operations (set, get)
- ✅ Connection pooling
- ✅ Automatic failover and retries
- ✅ Thread-safe operations
- ✅ Comprehensive error handling
- ✅ Java 8+ compatible

## Installation

### Maven

```xml
<dependency>
    <groupId>org.fastdfs</groupId>
    <artifactId>fastdfs-java-client</artifactId>
    <version>1.0.0</version>
</dependency>
```

### Gradle

```gradle
implementation 'org.fastdfs:fastdfs-java-client:1.0.0'
```

## Quick Start

### Basic Usage

```java
import org.fastdfs.client.FastDFSClient;
import org.fastdfs.client.config.ClientConfig;

public class Example {
    public static void main(String[] args) {
        // Create client configuration
        ClientConfig config = new ClientConfig("192.168.1.100:22122", "192.168.1.101:22122");
        config.setMaxConns(100);
        config.setConnectTimeout(5000);
        config.setNetworkTimeout(30000);
        
        // Initialize client
        try (FastDFSClient client = new FastDFSClient(config)) {
            // Upload a file
            String fileId = client.uploadFile("test.jpg", null);
            System.out.println("File uploaded: " + fileId);
            
            // Download the file
            byte[] data = client.downloadFile(fileId);
            System.out.println("Downloaded " + data.length + " bytes");
            
            // Delete the file
            client.deleteFile(fileId);
            System.out.println("File deleted");
            
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
```

### Upload from Buffer

```java
byte[] data = "Hello, FastDFS!".getBytes();
String fileId = client.uploadBuffer(data, "txt", null);
```

### Upload with Metadata

```java
Map<String, String> metadata = new HashMap<>();
metadata.put("author", "John Doe");
metadata.put("date", "2025-01-01");

String fileId = client.uploadFile("document.pdf", metadata);
```

### Download to File

```java
client.downloadToFile(fileId, "/path/to/save/file.jpg");
```

### Download Partial Content

```java
// Download 1024 bytes starting from offset 100
byte[] data = client.downloadFileRange(fileId, 100, 1024);
```

### Metadata Operations

```java
// Set metadata
Map<String, String> metadata = new HashMap<>();
metadata.put("description", "Sample file");
client.setMetadata(fileId, metadata, MetadataFlag.OVERWRITE);

// Get metadata
Map<String, String> meta = client.getMetadata(fileId);
System.out.println("Metadata: " + meta);
```

### Get File Information

```java
FileInfo info = client.getFileInfo(fileId);
System.out.println("File size: " + info.getFileSize());
System.out.println("Create time: " + info.getCreateTime());
System.out.println("CRC32: " + info.getCrc32());
System.out.println("Source IP: " + info.getSourceIpAddr());
```

### Check File Existence

```java
boolean exists = client.fileExists(fileId);
if (exists) {
    System.out.println("File exists");
}
```

### Upload Appender File

Appender files can be modified after upload (append, modify, truncate).

```java
String fileId = client.uploadAppenderFile("log.txt", null);
```

## Configuration

The `ClientConfig` class provides various configuration options:

| Option | Description | Default |
|--------|-------------|---------|
| `trackerAddrs` | List of tracker server addresses | Required |
| `maxConns` | Maximum connections per server | 10 |
| `connectTimeout` | Connection timeout (ms) | 5000 |
| `networkTimeout` | Network I/O timeout (ms) | 30000 |
| `idleTimeout` | Idle connection timeout (ms) | 60000 |
| `retryCount` | Number of retries for failed operations | 3 |

Example:

```java
ClientConfig config = new ClientConfig("192.168.1.100:22122");
config.setMaxConns(50);
config.setConnectTimeout(3000);
config.setNetworkTimeout(60000);
config.setRetryCount(5);
```

## Error Handling

The client uses a comprehensive exception hierarchy:

- `FastDFSException` - Base exception for all errors
- `ClientClosedException` - Client has been closed
- `FileNotFoundException` - File does not exist
- `NoStorageServerException` - No storage server available
- `ConnectionTimeoutException` - Connection timeout
- `NetworkTimeoutException` - Network I/O timeout
- `InvalidFileIdException` - Invalid file ID format
- `InvalidResponseException` - Invalid server response
- `InvalidArgumentException` - Invalid argument provided
- `InsufficientSpaceException` - Insufficient storage space
- `FileAlreadyExistsException` - File already exists
- `NetworkException` - Network communication error
- `ProtocolException` - Protocol-level error

Example:

```java
try {
    String fileId = client.uploadFile("test.jpg", null);
} catch (FileNotFoundException e) {
    System.err.println("File not found: " + e.getMessage());
} catch (NetworkException e) {
    System.err.println("Network error: " + e.getMessage());
} catch (FastDFSException e) {
    System.err.println("FastDFS error: " + e.getMessage());
}
```

## Thread Safety

The `FastDFSClient` is thread-safe and can be used concurrently from multiple threads. The client maintains internal connection pools that are shared across threads.

```java
// Create a single client instance
FastDFSClient client = new FastDFSClient(config);

// Use from multiple threads
ExecutorService executor = Executors.newFixedThreadPool(10);
for (int i = 0; i < 100; i++) {
    final int index = i;
    executor.submit(() -> {
        try {
            String fileId = client.uploadBuffer(
                ("File " + index).getBytes(), "txt", null);
            System.out.println("Uploaded: " + fileId);
        } catch (FastDFSException e) {
            e.printStackTrace();
        }
    });
}

executor.shutdown();
executor.awaitTermination(1, TimeUnit.MINUTES);
client.close();
```

## Building from Source

```bash
git clone https://github.com/happyfish100/fastdfs.git
cd fastdfs/java_client
mvn clean install
```

## Running Tests

```bash
mvn test
```

## Examples

See the [examples](examples/) directory for more usage examples:

- [BasicUsage.java](examples/BasicUsage.java) - Basic file operations
- [MetadataExample.java](examples/MetadataExample.java) - Working with metadata
- [ConcurrentUpload.java](examples/ConcurrentUpload.java) - Concurrent file uploads

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](../LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support

For issues and questions:
- GitHub Issues: https://github.com/happyfish100/fastdfs/issues
- FastDFS Forum: http://bbs.chinaunix.net/forum-240-1.html

## Related Projects

- [FastDFS](https://github.com/happyfish100/fastdfs) - FastDFS server
- [Go Client](../go_client) - Go client implementation
- [Python Client](../python_client) - Python client implementation
- [TypeScript Client](../typescript_client) - TypeScript/JavaScript client
- [Rust Client](../rust_client) - Rust client implementation
