# FastDFS C# Client

Official C# client library for FastDFS - A high-performance distributed file system.

## Features

- ✅ File upload (normal, appender, slave files)
- ✅ File download (full and partial)
- ✅ File deletion
- ✅ Metadata operations (set, get)
- ✅ Connection pooling
- ✅ Automatic failover
- ✅ Async/await support
- ✅ Thread-safe operations
- ✅ Comprehensive error handling
- ✅ Extensive documentation and comments

## Installation

### NuGet Package (Coming Soon)

```bash
dotnet add package FastDFS.Client
```

### Manual Installation

1. Clone the FastDFS repository
2. Copy the `csharp_client` directory to your project
3. Add all `.cs` files to your project
4. Build the project

## Quick Start

### Basic Usage

```csharp
using FastDFS.Client;
using System;
using System.Threading.Tasks;

class Program
{
    static async Task Main(string[] args)
    {
        // Create client configuration
        var config = new FastDFSClientConfig
        {
            TrackerAddresses = new[]
            {
                "192.168.1.100:22122",
                "192.168.1.101:22122"
            },
            MaxConnections = 100,
            ConnectTimeout = TimeSpan.FromSeconds(5),
            NetworkTimeout = TimeSpan.FromSeconds(30),
            IdleTimeout = TimeSpan.FromMinutes(5),
            RetryCount = 3
        };

        // Initialize client
        using (var client = new FastDFSClient(config))
        {
            // Upload a file
            var fileId = await client.UploadFileAsync("test.jpg", null);
            Console.WriteLine($"File uploaded: {fileId}");

            // Download the file
            var data = await client.DownloadFileAsync(fileId);
            Console.WriteLine($"Downloaded {data.Length} bytes");

            // Delete the file
            await client.DeleteFileAsync(fileId);
            Console.WriteLine("File deleted");
        }
    }
}
```

### Upload from Buffer

```csharp
var data = Encoding.UTF8.GetBytes("Hello, FastDFS!");
var fileId = await client.UploadBufferAsync(data, "txt", null);
```

### Upload with Metadata

```csharp
var metadata = new Dictionary<string, string>
{
    { "author", "John Doe" },
    { "date", "2025-01-01" },
    { "width", "1920" },
    { "height", "1080" }
};

var fileId = await client.UploadFileAsync("document.pdf", metadata);
```

### Download to File

```csharp
await client.DownloadToFileAsync(fileId, "/path/to/save/file.jpg");
```

### Partial Download

```csharp
// Download bytes from offset 100, length 1024
var data = await client.DownloadFileRangeAsync(fileId, 100, 1024);
```

### Appender File Operations

```csharp
// Upload appender file
var fileId = await client.UploadAppenderFileAsync("log.txt", null);

// Append data
var newData = Encoding.UTF8.GetBytes("New log entry\n");
await client.AppendFileAsync(fileId, newData);
```

### Slave File Operations

```csharp
// Upload slave file with prefix
var slaveFileId = await client.UploadSlaveFileAsync(
    masterFileId,
    "thumb",
    "jpg",
    slaveData,
    null);
```

### Metadata Operations

```csharp
// Set metadata
var metadata = new Dictionary<string, string>
{
    { "width", "1920" },
    { "height", "1080" }
};

await client.SetMetadataAsync(fileId, metadata, MetadataFlag.Overwrite);

// Get metadata
var retrievedMetadata = await client.GetMetadataAsync(fileId);
foreach (var kvp in retrievedMetadata)
{
    Console.WriteLine($"{kvp.Key}: {kvp.Value}");
}
```

### File Information

```csharp
var fileInfo = await client.GetFileInfoAsync(fileId);
Console.WriteLine($"Size: {fileInfo.FileSize}, " +
                  $"CreateTime: {fileInfo.CreateTime}, " +
                  $"CRC32: {fileInfo.CRC32:X8}");
```

## Configuration

### FastDFSClientConfig Options

```csharp
var config = new FastDFSClientConfig
{
    // Tracker server addresses (required)
    TrackerAddresses = new[] { "192.168.1.100:22122" },
    
    // Maximum connections per tracker (default: 10)
    MaxConnections = 100,
    
    // Connection timeout (default: 5s)
    ConnectTimeout = TimeSpan.FromSeconds(5),
    
    // Network I/O timeout (default: 30s)
    NetworkTimeout = TimeSpan.FromSeconds(30),
    
    // Connection pool idle timeout (default: 5 minutes)
    IdleTimeout = TimeSpan.FromMinutes(5),
    
    // Retry count for failed operations (default: 3)
    RetryCount = 3,
    
    // Enable connection pool (default: true)
    EnablePool = true
};
```

## Error Handling

The client provides detailed exception types:

```csharp
try
{
    var fileId = await client.UploadFileAsync("file.txt", null);
}
catch (FastDFSFileNotFoundException ex)
{
    // Handle file not found
    Console.WriteLine($"File not found: {ex.FileId}");
}
catch (FastDFSNetworkException ex)
{
    // Handle network errors
    Console.WriteLine($"Network error: {ex.Message}");
}
catch (FastDFSException ex)
{
    // Handle other FastDFS errors
    Console.WriteLine($"FastDFS error: {ex.Message}");
}
```

## Connection Pooling

The client automatically manages connection pools for optimal performance:

- Connections are reused across requests
- Idle connections are cleaned up automatically
- Failed connections trigger automatic failover
- Thread-safe for concurrent operations

## Thread Safety

The client is fully thread-safe and can be used concurrently from multiple threads:

```csharp
var tasks = new List<Task<string>>();
for (int i = 0; i < 100; i++)
{
    int fileIndex = i;
    tasks.Add(Task.Run(async () =>
    {
        return await client.UploadFileAsync($"file{fileIndex}.txt", null);
    }));
}

var fileIds = await Task.WhenAll(tasks);
```

## Examples

See the [examples](examples/) directory for complete usage examples:

- [Basic Upload/Download](examples/BasicExample.cs) - File upload, download, and deletion
- [Metadata Management](examples/MetadataExample.cs) - Working with file metadata
- [Appender Files](examples/AppenderExample.cs) - Appender file operations

## Performance

The client is designed for high performance:

- Connection pooling reduces connection overhead
- Async/await provides non-blocking I/O
- Efficient protocol encoding/decoding
- Minimal memory allocations

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](../CONTRIBUTING.md) for details.

## License

GNU General Public License V3 - see [LICENSE](../COPYING-3_0.txt) for details.

## Support

- GitHub Issues: https://github.com/happyfish100/fastdfs/issues
- Email: 384681@qq.com
- WeChat: fastdfs

## Related Projects

- [FastDFS](https://github.com/happyfish100/fastdfs) - Main FastDFS project
- [FastCFS](https://github.com/happyfish100/FastCFS) - Distributed file system with strong consistency

