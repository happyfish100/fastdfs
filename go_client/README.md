# FastDFS Go Client

Official Go client library for FastDFS - A high-performance distributed file system.

## Features

- ✅ File upload (normal, appender, slave files)
- ✅ File download (full and partial)
- ✅ File deletion
- ✅ Metadata operations (set, get)
- ✅ Connection pooling
- ✅ Automatic failover
- ✅ Context support for cancellation and timeouts
- ✅ Thread-safe operations
- ✅ Comprehensive error handling

## Installation

```bash
go get github.com/happyfish100/fastdfs/go_client
```

## Quick Start

### Basic Usage

```go
package main

import (
    "context"
    "fmt"
    "log"
    
    fdfs "github.com/happyfish100/fastdfs/go_client"
)

func main() {
    // Create client configuration
    config := &fdfs.ClientConfig{
        TrackerAddrs: []string{
            "192.168.1.100:22122",
            "192.168.1.101:22122",
        },
        MaxConns:        100,
        ConnectTimeout:  5 * time.Second,
        NetworkTimeout:  30 * time.Second,
    }
    
    // Initialize client
    client, err := fdfs.NewClient(config)
    if err != nil {
        log.Fatal(err)
    }
    defer client.Close()
    
    // Upload a file
    fileID, err := client.UploadFile(context.Background(), "test.jpg", nil)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("File uploaded: %s\n", fileID)
    
    // Download the file
    data, err := client.DownloadFile(context.Background(), fileID)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Downloaded %d bytes\n", len(data))
    
    // Delete the file
    err = client.DeleteFile(context.Background(), fileID)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Println("File deleted")
}
```

### Upload from Buffer

```go
data := []byte("Hello, FastDFS!")
fileID, err := client.UploadBuffer(ctx, data, "txt", nil)
```

### Upload with Metadata

```go
metadata := map[string]string{
    "author": "John Doe",
    "date":   "2025-01-01",
}
fileID, err := client.UploadFile(ctx, "document.pdf", metadata)
```

### Download to File

```go
err := client.DownloadToFile(ctx, fileID, "/path/to/save/file.jpg")
```

### Partial Download

```go
// Download bytes from offset 100, length 1024
data, err := client.DownloadFileRange(ctx, fileID, 100, 1024)
```

### Appender File Operations

```go
// Upload appender file
fileID, err := client.UploadAppenderFile(ctx, "log.txt", nil)

// Append data
err = client.AppendFile(ctx, fileID, []byte("New log entry\n"))

// Modify file content
err = client.ModifyFile(ctx, fileID, 0, []byte("Modified content"))

// Truncate file
err = client.TruncateFile(ctx, fileID, 1024)
```

### Slave File Operations

```go
// Upload slave file with prefix
slaveFileID, err := client.UploadSlaveFile(ctx, masterFileID, "thumb", "jpg", slaveData, nil)
```

### Metadata Operations

```go
// Set metadata
metadata := map[string]string{
    "width":  "1920",
    "height": "1080",
}
err := client.SetMetadata(ctx, fileID, metadata, fdfs.MetadataOverwrite)

// Get metadata
meta, err := client.GetMetadata(ctx, fileID)
```

### File Information

```go
info, err := client.GetFileInfo(ctx, fileID)
fmt.Printf("Size: %d, CreateTime: %v, CRC32: %d\n", 
    info.FileSize, info.CreateTime, info.CRC32)
```

## Configuration

### ClientConfig Options

```go
type ClientConfig struct {
    // Tracker server addresses (required)
    TrackerAddrs []string
    
    // Maximum connections per tracker (default: 10)
    MaxConns int
    
    // Connection timeout (default: 5s)
    ConnectTimeout time.Duration
    
    // Network I/O timeout (default: 30s)
    NetworkTimeout time.Duration
    
    // Connection pool idle timeout (default: 60s)
    IdleTimeout time.Duration
    
    // Enable connection pool (default: true)
    EnablePool bool
    
    // Retry count for failed operations (default: 3)
    RetryCount int
}
```

## Error Handling

The client provides detailed error types:

```go
err := client.UploadFile(ctx, "file.txt", nil)
if err != nil {
    switch {
    case errors.Is(err, fdfs.ErrFileNotFound):
        // Handle file not found
    case errors.Is(err, fdfs.ErrNoStorageServer):
        // Handle no available storage server
    case errors.Is(err, fdfs.ErrConnectionTimeout):
        // Handle connection timeout
    default:
        // Handle other errors
    }
}
```

## Connection Pooling

The client automatically manages connection pools for optimal performance:

- Connections are reused across requests
- Idle connections are cleaned up automatically
- Failed connections trigger automatic failover
- Thread-safe for concurrent operations

## Context Support

All operations support context for cancellation and timeouts:

```go
// With timeout
ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
defer cancel()

fileID, err := client.UploadFile(ctx, "large-file.bin", nil)

// With cancellation
ctx, cancel := context.WithCancel(context.Background())
go func() {
    time.Sleep(5 * time.Second)
    cancel() // Cancel the operation
}()

data, err := client.DownloadFile(ctx, fileID)
```

## Thread Safety

The client is fully thread-safe and can be used concurrently from multiple goroutines:

```go
var wg sync.WaitGroup
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func(n int) {
        defer wg.Done()
        fileID, err := client.UploadFile(ctx, fmt.Sprintf("file%d.txt", n), nil)
        // Handle error...
    }(i)
}
wg.Wait()
```

## Examples

See the [examples](examples/) directory for more usage examples:

- [Basic Upload/Download](examples/basic/main.go)
- [Batch Operations](examples/batch/main.go)
- [Appender Files](examples/appender/main.go)
- [Metadata Management](examples/metadata/main.go)
- [Error Handling](examples/errors/main.go)

## Testing

Run the test suite:

```bash
# Unit tests
go test ./...

# Integration tests (requires running FastDFS cluster)
go test -tags=integration ./...

# Benchmarks
go test -bench=. ./...
```

## Performance

Benchmark results on a typical setup:

```
BenchmarkUploadSmallFile-8     5000    250000 ns/op    4000 B/op    50 allocs/op
BenchmarkUploadLargeFile-8      100  10000000 ns/op  100000 B/op   100 allocs/op
BenchmarkDownload-8            3000    400000 ns/op    8000 B/op    60 allocs/op
```

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
