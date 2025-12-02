# FastDFS C++ Client

Official C++ client library for FastDFS - A high-performance distributed file system.

## Features

- ✅ File upload (normal, appender, slave files)
- ✅ File download (full and partial)
- ✅ File deletion
- ✅ Metadata operations (set, get)
- ✅ Connection pooling
- ✅ Automatic failover
- ✅ Thread-safe operations
- ✅ Comprehensive error handling
- ✅ Cross-platform (Windows, Linux, macOS)

## Requirements

- C++17 or later
- CMake 3.12 or later
- FastDFS tracker and storage servers

## Building

### Using CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Build Options

- `BUILD_SHARED_LIBS`: Build shared library (default: ON)
- `BUILD_EXAMPLES`: Build example programs (default: ON)
- `BUILD_TESTS`: Build test programs (default: OFF)

### Installation

```bash
cmake --install .
```

## Quick Start

### Basic Usage

```cpp
#include "fastdfs/client.hpp"
#include <iostream>

int main() {
    // Create client configuration
    fastdfs::ClientConfig config;
    config.tracker_addrs = {"192.168.1.100:22122"};
    config.max_conns = 10;
    config.connect_timeout = std::chrono::milliseconds(5000);
    config.network_timeout = std::chrono::milliseconds(30000);
    
    // Initialize client
    fastdfs::Client client(config);
    
    // Upload a file
    std::string file_id = client.upload_file("test.jpg", nullptr);
    std::cout << "File uploaded: " << file_id << std::endl;
    
    // Download the file
    std::vector<uint8_t> data = client.download_file(file_id);
    std::cout << "Downloaded " << data.size() << " bytes" << std::endl;
    
    // Delete the file
    client.delete_file(file_id);
    
    // Close client
    client.close();
    
    return 0;
}
```

### Upload from Buffer

```cpp
std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'F', 'a', 's', 't', 'D', 'F', 'S', '!'};
std::string file_id = client.upload_buffer(data, "txt", nullptr);
```

### Upload with Metadata

```cpp
fastdfs::Metadata metadata;
metadata["author"] = "John Doe";
metadata["date"] = "2025-01-01";
metadata["description"] = "Test file";

std::string file_id = client.upload_file("document.pdf", &metadata);
```

### Download to File

```cpp
client.download_to_file(file_id, "/path/to/save/file.jpg");
```

### Partial Download

```cpp
// Download bytes from offset 100, length 1024
std::vector<uint8_t> data = client.download_file_range(file_id, 100, 1024);
```

### Appender File Operations

```cpp
// Upload appender file
std::string file_id = client.upload_appender_file("log.txt", nullptr);

// Append data
std::vector<uint8_t> new_data = {'N', 'e', 'w', ' ', 'l', 'o', 'g', ' ', 'e', 'n', 't', 'r', 'y', '\n'};
client.append_file(file_id, new_data);

// Modify file content
std::vector<uint8_t> modified_data = {'M', 'o', 'd', 'i', 'f', 'i', 'e', 'd'};
client.modify_file(file_id, 0, modified_data);

// Truncate file
client.truncate_file(file_id, 1024);
```

### Slave File Operations

```cpp
// Upload slave file with prefix
std::vector<uint8_t> slave_data = {/* thumbnail data */};
std::string slave_file_id = client.upload_slave_file(
    master_file_id, "thumb", "jpg", slave_data, nullptr);
```

### Metadata Operations

```cpp
// Set metadata
fastdfs::Metadata metadata;
metadata["width"] = "1920";
metadata["height"] = "1080";
client.set_metadata(file_id, metadata, fastdfs::MetadataFlag::OVERWRITE);

// Get metadata
fastdfs::Metadata retrieved = client.get_metadata(file_id);
for (const auto& pair : retrieved) {
    std::cout << pair.first << " = " << pair.second << std::endl;
}
```

### File Information

```cpp
fastdfs::FileInfo info = client.get_file_info(file_id);
std::cout << "Size: " << info.file_size << std::endl;
std::cout << "CreateTime: " << info.create_time << std::endl;
std::cout << "CRC32: " << info.crc32 << std::endl;
```

## Configuration

### ClientConfig Options

```cpp
struct ClientConfig {
    std::vector<std::string> tracker_addrs;  // Required: Tracker server addresses
    int max_conns = 10;                       // Maximum connections per tracker
    std::chrono::milliseconds connect_timeout{5000};   // Connection timeout
    std::chrono::milliseconds network_timeout{30000};  // Network I/O timeout
    std::chrono::milliseconds idle_timeout{60000};   // Connection pool idle timeout
    bool enable_pool = true;                 // Enable connection pooling
    int retry_count = 3;                     // Retry count for failed operations
};
```

## Error Handling

The client provides detailed exception types:

```cpp
try {
    std::string file_id = client.upload_file("test.jpg", nullptr);
} catch (const fastdfs::FileNotFoundException& e) {
    // Handle file not found
} catch (const fastdfs::ConnectionException& e) {
    // Handle connection error
} catch (const fastdfs::TimeoutException& e) {
    // Handle timeout
} catch (const fastdfs::FastDFSException& e) {
    // Handle other FastDFS errors
} catch (const std::exception& e) {
    // Handle other errors
}
```

## Thread Safety

The client is thread-safe and can be used concurrently from multiple threads:

```cpp
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&client, i]() {
        std::string file_id = client.upload_file("file" + std::to_string(i) + ".txt", nullptr);
        // Handle result...
    });
}

for (auto& t : threads) {
    t.join();
}
```

## Examples

See the [examples](examples/) directory for complete usage examples:

- [Basic Usage](examples/basic_usage.cpp) - File upload, download, and deletion
- [Metadata Management](examples/metadata_example.cpp) - Working with file metadata
- [Appender Files](examples/appender_example.cpp) - Appender file operations

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

