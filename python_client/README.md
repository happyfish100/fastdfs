# FastDFS Python Client

Official Python client library for FastDFS - A high-performance distributed file system.

## Features

- ✅ File upload (normal, appender, slave files)
- ✅ File download (full and partial)
- ✅ File deletion
- ✅ Metadata operations (set, get)
- ✅ Connection pooling
- ✅ Automatic failover
- ✅ Thread-safe operations
- ✅ Comprehensive error handling
- ✅ Pure Python implementation (no C dependencies)

## Installation

```bash
pip install fastdfs-client
```

Or install from source:
```bash
cd python_client
python setup.py install
```

## Quick Start

### Basic Usage

```python
from fdfs import Client, ClientConfig

# Create client configuration
config = ClientConfig(
    tracker_addrs=['192.168.1.100:22122', '192.168.1.101:22122'],
    max_conns=10,
    connect_timeout=5.0,
    network_timeout=30.0
)

# Initialize client
client = Client(config)

# Upload a file
file_id = client.upload_file('test.jpg')
print(f"File uploaded: {file_id}")

# Download the file
data = client.download_file(file_id)
print(f"Downloaded {len(data)} bytes")

# Delete the file
client.delete_file(file_id)
print("File deleted")

# Close client
client.close()
```

### Using Context Manager

```python
from fdfs import Client, ClientConfig

config = ClientConfig(tracker_addrs=['192.168.1.100:22122'])

with Client(config) as client:
    file_id = client.upload_buffer(b'Hello, FastDFS!', 'txt')
    data = client.download_file(file_id)
    client.delete_file(file_id)
```

### Upload from Buffer

```python
data = b'Hello, FastDFS!'
file_id = client.upload_buffer(data, 'txt')
```

### Upload with Metadata

```python
metadata = {
    'author': 'John Doe',
    'date': '2025-01-15',
    'version': '1.0'
}
file_id = client.upload_file('document.pdf', metadata)
```

### Download to File

```python
client.download_to_file(file_id, '/path/to/save/file.jpg')
```

### Partial Download

```python
# Download bytes from offset 100, length 1024
data = client.download_file_range(file_id, offset=100, length=1024)
```

### Appender File Operations

```python
# Upload appender file
file_id = client.upload_appender_file('log.txt')

# Note: Append, modify, and truncate operations require
# storage server configuration to support appender files
```

### Metadata Operations

```python
from fdfs.types import MetadataFlag

# Set metadata (overwrite)
metadata = {
    'width': '1920',
    'height': '1080'
}
client.set_metadata(file_id, metadata, MetadataFlag.OVERWRITE)

# Get metadata
meta = client.get_metadata(file_id)
print(meta)

# Merge metadata
new_meta = {'author': 'Jane Doe'}
client.set_metadata(file_id, new_meta, MetadataFlag.MERGE)
```

### File Information

```python
info = client.get_file_info(file_id)
print(f"Size: {info.file_size}")
print(f"Create time: {info.create_time}")
print(f"CRC32: {info.crc32}")
print(f"Source IP: {info.source_ip_addr}")
```

### Check File Existence

```python
exists = client.file_exists(file_id)
print(f"File exists: {exists}")
```

## Configuration

### ClientConfig Options

```python
config = ClientConfig(
    # Tracker server addresses (required)
    tracker_addrs=['192.168.1.100:22122'],
    
    # Maximum connections per tracker (default: 10)
    max_conns=10,
    
    # Connection timeout in seconds (default: 5.0)
    connect_timeout=5.0,
    
    # Network I/O timeout in seconds (default: 30.0)
    network_timeout=30.0,
    
    # Connection pool idle timeout in seconds (default: 60.0)
    idle_timeout=60.0,
    
    # Retry count for failed operations (default: 3)
    retry_count=3
)
```

## Error Handling

The client provides detailed error types:

```python
from fdfs.errors import (
    FileNotFoundError,
    NoStorageServerError,
    ConnectionTimeoutError,
    NetworkError,
    InvalidFileIDError
)

try:
    file_id = client.upload_file('test.txt')
except FileNotFoundError:
    print("Local file not found")
except NoStorageServerError:
    print("No storage server available")
except ConnectionTimeoutError:
    print("Connection timeout")
except NetworkError as e:
    print(f"Network error: {e}")
```

## Thread Safety

The client is fully thread-safe and can be used concurrently from multiple threads:

```python
import threading

def upload_file(client, filename):
    file_id = client.upload_file(filename)
    print(f"Uploaded: {file_id}")

config = ClientConfig(tracker_addrs=['192.168.1.100:22122'])
client = Client(config)

threads = []
for i in range(10):
    t = threading.Thread(target=upload_file, args=(client, f'file{i}.txt'))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

client.close()
```

## Examples

See the examples directory for complete usage examples:

- basic_usage.py - File upload, download, and deletion
- metadata_example.py - Working with file metadata
- appender_example.py - Appender file operations
  
## Testing

Run the test suite:

```python
# Unit tests
python -m pytest tests/

# With coverage
python -m pytest tests/ --cov=fdfs --cov-report=html

# Integration tests (requires running FastDFS cluster)
export FASTDFS_TRACKER_ADDR=192.168.1.100:22122
python -m pytest tests/test_integration.py
```

## Development

### Setup Development Environment

```bash
# Clone repository
git clone https://github.com/happyfish100/fastdfs.git
cd fastdfs/python_client

# Create virtual environment
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install development dependencies
pip install -e ".[dev]"
```

## Code Formatting

```bash
# Format code
black fdfs tests examples

# Sort imports
isort fdfs tests examples

# Lint
flake8 fdfs tests examples

# Type checking
mypy fdfs
```

## Requirements

- Python 3.7 or later
- No external dependencies (uses only Python standard library)

## Compatibility

- Python Version: 3.7+
- FastDFS Version: 6.x (tested with 6.15.1)
- Platforms: Linux, macOS, Windows, FreeBSD

## Performance

The client uses connection pooling and efficient buffer management for optimal performance:

- Connection reuse minimizes overhead
- Thread-safe for parallel operations
- Automatic retry with exponential backoff
- Efficient memory usage
  
## Contributing

Contributions are welcome! Please see CONTRIBUTING.md for details.

## License

GNU General Public License V3 - see LICENSE for details.

## Support

- GitHub Issues: https://github.com/happyfish100/fastdfs/issues
- Email: 384681@qq.com
- WeChat: fastdfs

## Related Projects

- FastDFS - Main FastDFS project
- FastDFS Go Client - Official Go client
- FastCFS - Distributed file system with strong consistency