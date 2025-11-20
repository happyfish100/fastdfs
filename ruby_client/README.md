# FastDFS Ruby Client

Official Ruby client library for FastDFS - A high-performance distributed file system.

[![Ruby Version](https://img.shields.io/badge/ruby-2.7%2B-blue.svg)](https://www.ruby-lang.org/)
[![License](https://img.shields.io/badge/license-GPL--3.0-green.svg)](LICENSE)

## Features

- ✅ File upload (normal, appender, slave files)
- ✅ File download (full and partial)
- ✅ File deletion
- ✅ Metadata operations (set, get)
- ✅ Connection pooling
- ✅ Automatic failover
- ✅ Retry logic for transient failures
- ✅ Thread-safe operations
- ✅ Comprehensive error handling
- ✅ Ruby-like API

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'fastdfs'
```

And then execute:

```bash
bundle install
```

Or install it directly:

```bash
gem install fastdfs
```

## Quick Start

### Basic Usage

```ruby
require 'fastdfs'

# Create client configuration
config = FastDFS::ClientConfig.new(
  tracker_addrs: [
    '192.168.1.100:22122',
    '192.168.1.101:22122'
  ],
  max_conns: 10,
  connect_timeout: 5.0,
  network_timeout: 30.0
)

# Initialize client
client = FastDFS::Client.new(config)

begin
  # Upload a file
  file_id = client.upload_file('test.jpg')
  puts "File uploaded: #{file_id}"
  
  # Download the file
  data = client.download_file(file_id)
  puts "Downloaded #{data.bytesize} bytes"
  
  # Delete the file
  client.delete_file(file_id)
  puts "File deleted"
ensure
  # Always close the client
  client.close
end
```

### Upload from Buffer

```ruby
# Upload data from memory
data = "Hello, FastDFS!"
file_id = client.upload_buffer(data, 'txt')
```

### Upload with Metadata

```ruby
# Upload with metadata
metadata = {
  'author' => 'John Doe',
  'date' => '2025-01-01'
}
file_id = client.upload_file('document.pdf', metadata)
```

### Download to File

```ruby
# Download and save to local file
client.download_to_file(file_id, '/path/to/save/image.jpg')
```

### Partial Download

```ruby
# Download specific byte range
data = client.download_file_range(file_id, 0, 1024)  # First 1024 bytes
```

### File Information

```ruby
# Check if file exists
if client.file_exists?(file_id)
  puts "File exists"
  
  # Get file information
  info = client.get_file_info(file_id)
  puts "Size: #{info.file_size} bytes"
  puts "Created: #{info.create_time}"
  puts "CRC32: #{info.crc32}"
end
```

## Configuration

### ClientConfig Options

```ruby
config = FastDFS::ClientConfig.new(
  # Tracker server addresses (required)
  tracker_addrs: ['192.168.1.100:22122'],
  
  # Maximum connections per server (default: 10)
  max_conns: 10,
  
  # Connection timeout in seconds (default: 5.0)
  connect_timeout: 5.0,
  
  # Network I/O timeout in seconds (default: 30.0)
  network_timeout: 30.0,
  
  # Idle connection timeout in seconds (default: 60.0)
  idle_timeout: 60.0,
  
  # Retry count for failed operations (default: 3)
  retry_count: 3
)
```

## Error Handling

The client provides detailed error types:

```ruby
begin
  client.upload_file('test.jpg')
rescue FastDFS::FileNotFoundError => e
  puts "File not found: #{e.message}"
rescue FastDFS::NetworkError => e
  puts "Network error: #{e.message}"
rescue FastDFS::ClientClosedError => e
  puts "Client is closed: #{e.message}"
rescue FastDFS::Error => e
  puts "FastDFS error: #{e.message}"
end
```

### Error Types

- `ClientClosedError` - Client has been closed
- `FileNotFoundError` - File does not exist
- `NoStorageServerError` - No storage server available
- `ConnectionTimeoutError` - Connection timeout
- `NetworkTimeoutError` - Network I/O timeout
- `InvalidFileIDError` - Invalid file ID format
- `InvalidResponseError` - Invalid server response
- `StorageServerOfflineError` - Storage server is offline
- `TrackerServerOfflineError` - Tracker server is offline
- `InsufficientSpaceError` - Insufficient storage space
- `FileAlreadyExistsError` - File already exists
- `InvalidMetadataError` - Invalid metadata format
- `OperationNotSupportedError` - Operation not supported
- `InvalidArgumentError` - Invalid argument
- `ProtocolError` - Protocol-level error
- `NetworkError` - Network-related error
- `StorageError` - Storage server error
- `TrackerError` - Tracker server error

## Connection Pooling

The client automatically manages connection pools for optimal performance:

- Connections are reused across requests
- Idle connections are cleaned up automatically
- Failed connections trigger automatic failover
- Thread-safe for concurrent operations

## Thread Safety

The client is fully thread-safe and can be used concurrently from multiple threads:

```ruby
require 'thread'

threads = []
10.times do |i|
  threads << Thread.new do
    file_id = client.upload_file("file#{i}.txt")
    puts "Uploaded: #{file_id}"
  end
end

threads.each(&:join)
```

## Examples

See the [examples](examples/) directory for complete usage examples:

- [Basic Usage](examples/basic_usage.rb) - File upload, download, and deletion
- [Upload Buffer](examples/upload_buffer.rb) - Upload data from memory

## Requirements

- Ruby 2.7 or higher
- FastDFS server (tracker and storage servers)

## Testing

Run the test suite:

```bash
# Unit tests
bundle exec rake test

# Integration tests (requires running FastDFS cluster)
bundle exec rake test:integration
```

## Performance

The client is optimized for performance:

- Connection pooling reduces connection overhead
- Automatic retries handle transient failures
- Efficient binary protocol implementation
- Thread-safe for concurrent operations

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## License

GNU General Public License V3 - see [LICENSE](LICENSE) for details.

## Support

- GitHub Issues: https://github.com/happyfish100/fastdfs/issues
- Email: 384681@qq.com
- WeChat: fastdfs

## Related Projects

- [FastDFS](https://github.com/happyfish100/fastdfs) - Main FastDFS project
- [FastCFS](https://github.com/happyfish100/FastCFS) - Distributed file system with strong consistency

## Changelog

### Version 1.0.0

- Initial release
- Basic file operations (upload, download, delete)
- Connection pooling
- Retry logic
- Thread safety
- Comprehensive error handling

