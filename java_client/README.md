# FastDFS Java Client

Official Java client library for FastDFS - A high-performance distributed file system.

## Overview

This Java client provides a complete implementation of the FastDFS protocol, allowing Java applications
to interact with FastDFS tracker and storage servers. The client supports all major FastDFS operations
including file upload, download, deletion, metadata management, and appender file operations.

## Features

- ✅ File upload (normal, appender, slave files)
- ✅ File download (full and partial range downloads)
- ✅ File deletion
- ✅ Metadata operations (set, get, merge)
- ✅ Connection pooling for optimal performance
- ✅ Automatic failover between tracker servers
- ✅ Thread-safe operations
- ✅ Comprehensive error handling
- ✅ Support for large file operations
- ✅ Configurable timeouts and retry mechanisms

## Installation

### Maven

Add the following dependency to your `pom.xml`:

```xml
<dependency>
    <groupId>com.github.happyfish100</groupId>
    <artifactId>fastdfs-java-client</artifactId>
    <version>1.0.0</version>
</dependency>
```

### Gradle

Add the following to your `build.gradle`:

```gradle
dependencies {
    implementation 'com.github.happyfish100:fastdfs-java-client:1.0.0'
}
```

## Quick Start

### Basic Usage

```java
import com.fastdfs.client.FastDFSClient;
import com.fastdfs.client.FastDFSConfig;
import java.util.HashMap;
import java.util.Map;

public class Example {
    public static void main(String[] args) throws Exception {
        // Create client configuration
        FastDFSConfig config = new FastDFSConfig.Builder()
            .addTrackerServer("192.168.1.100", 22122)
            .addTrackerServer("192.168.1.101", 22122)
            .maxConnectionsPerServer(100)
            .connectTimeout(5000)
            .networkTimeout(30000)
            .idleTimeout(60000)
            .retryCount(3)
            .build();
        
        // Initialize client
        FastDFSClient client = new FastDFSClient(config);
        
        try {
            // Upload a file
            String fileId = client.uploadFile("test.jpg", null);
            System.out.println("File uploaded: " + fileId);
            
            // Download the file
            byte[] data = client.downloadFile(fileId);
            System.out.println("Downloaded " + data.length + " bytes");
            
            // Delete the file
            client.deleteFile(fileId);
            System.out.println("File deleted");
        } finally {
            client.close();
        }
    }
}
```

### Upload from Byte Array

```java
byte[] fileData = "Hello, FastDFS!".getBytes();
String fileId = client.uploadBuffer(fileData, "txt", null);
```

### Upload with Metadata

```java
Map<String, String> metadata = new HashMap<>();
metadata.put("author", "John Doe");
metadata.put("description", "Test file");
String fileId = client.uploadFile("test.jpg", metadata);
```

## API Documentation

See the [API Documentation](docs/api.md) for complete API reference.

## License

This project is licensed under the GNU General Public License v3.0.

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Support

For issues and questions, please open an issue on GitHub.

