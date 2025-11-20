# FastDFS C Examples

This directory contains comprehensive C examples demonstrating core FastDFS client functionality. Each example is fully documented with inline comments, error handling, and best practices.

## Examples Overview

| Example | Description | Difficulty |
|---------|-------------|------------|
| `01_basic_upload.c` | Upload files to FastDFS storage | Beginner |
| `02_basic_download.c` | Download files from FastDFS storage | Beginner |
| `03_metadata_operations.c` | Set and retrieve file metadata | Intermediate |

## Prerequisites

### System Requirements
- GCC compiler
- FastDFS client library installed
- FastDFS tracker and storage servers running
- Network access to FastDFS servers

### FastDFS Setup
1. **Tracker server** must be running on configured address
2. **Storage server** must be running and registered with tracker
3. **Client configuration** file must be properly configured

## Quick Start

### 1. Prepare Configuration

Copy and edit the client configuration file:

```bash
cp ../../conf/client.conf .
```

Edit `client.conf` with your tracker server addresses:

```ini
# Tracker server addresses (can specify multiple)
tracker_server = 192.168.0.196:22122
tracker_server = 192.168.0.197:22122

# Connection timeouts
connect_timeout = 5
network_timeout = 60

# Base path for logs
base_path = /opt/fastdfs
```

### 2. Build Examples

Build all examples:

```bash
make
```

Or build individual examples:

```bash
make 01_basic_upload
make 02_basic_download
make 03_metadata_operations
```

### 3. Run Examples

#### Upload a File

```bash
./01_basic_upload client.conf /path/to/your/file.jpg
```

**Expected Output:**
```
=== FastDFS Basic Upload Example ===
Config file: client.conf
Local file: /path/to/your/file.jpg

Initializing FastDFS client...
✓ Client initialized successfully

Connecting to tracker server...
✓ Connected to tracker server: 192.168.0.196:22122

...

✓ Upload successful!

=== Upload Results ===
Group name: group1
Remote filename: M00/00/00/wKgBcGXxxx.jpg
File ID: group1/M00/00/00/wKgBcGXxxx.jpg
```

#### Download a File

```bash
# Download to auto-named file
./02_basic_download client.conf group1/M00/00/00/wKgBcGXxxx.jpg

# Download to specific file
./02_basic_download client.conf group1/M00/00/00/wKgBcGXxxx.jpg output.jpg
```

**Expected Output:**
```
=== FastDFS Basic Download Example ===
...
✓ Download successful!

=== Download Results ===
File size: 12345 bytes (12.06 KB)
Saved to: output.jpg
✓ File size verified
```

#### Metadata Operations

```bash
# Set metadata (overwrites existing)
./03_metadata_operations client.conf set group1/M00/00/00/wKgBcGXxxx.jpg \
    width=1920 height=1080 author=John format=JPEG

# Merge metadata (preserves existing)
./03_metadata_operations client.conf merge group1/M00/00/00/wKgBcGXxxx.jpg \
    tags=landscape camera=Canon

# Get all metadata
./03_metadata_operations client.conf get group1/M00/00/00/wKgBcGXxxx.jpg
```

**Expected Output:**
```
=== Metadata (4 items) ===
  [ 1] width                = 1920
  [ 2] height               = 1080
  [ 3] author               = John
  [ 4] format               = JPEG
```

## Example Details

### 01_basic_upload.c

**Purpose:** Demonstrates the complete file upload workflow.

**Key Features:**
- ✅ Configuration file validation
- ✅ File existence and permission checks
- ✅ Tracker server connection
- ✅ Storage server query and connection
- ✅ File extension extraction
- ✅ Upload with progress tracking
- ✅ File information retrieval
- ✅ Comprehensive error handling

**What You'll Learn:**
- How to initialize the FastDFS client
- How to connect to tracker and storage servers
- How to upload files using `storage_upload_by_filename()`
- How to retrieve file information after upload
- Proper resource cleanup

**Common Issues:**
- **Tracker connection failed**: Check if tracker server is running and accessible
- **Storage server not available**: Verify storage server is registered with tracker
- **File not found**: Use absolute path or verify relative path is correct
- **Permission denied**: Ensure read access to the file being uploaded

---

### 02_basic_download.c

**Purpose:** Demonstrates three different methods to download files.

**Key Features:**
- ✅ Three download methods: direct to file, to buffer, callback
- ✅ File ID parsing and validation
- ✅ Progress tracking for large files
- ✅ File size verification
- ✅ Human-readable size display
- ✅ Automatic filename extraction

**Download Methods:**

1. **Direct to File** (Most Efficient)
   - Best for large files
   - Minimal memory usage
   - Direct streaming to disk

2. **To Buffer** (Most Flexible)
   - Good for small files
   - Allows data processing before saving
   - Requires memory for entire file

3. **Callback** (Most Control)
   - Streaming with progress tracking
   - Process data as it arrives
   - Good for very large files

**What You'll Learn:**
- How to parse FastDFS file IDs
- How to query storage servers for download
- Different download strategies and when to use them
- How to implement download callbacks
- File verification techniques

**Common Issues:**
- **Invalid file ID**: Must be in format `group_name/path/filename`
- **File not found**: Verify the file exists using the correct file ID
- **Download timeout**: Increase `network_timeout` for large files
- **Disk full**: Ensure sufficient space before downloading

---

### 03_metadata_operations.c

**Purpose:** Demonstrates how to manage file metadata (key-value pairs).

**Key Features:**
- ✅ Set metadata (overwrite mode)
- ✅ Merge metadata (update/add mode)
- ✅ Get all metadata
- ✅ Metadata validation (key/value length checks)
- ✅ Automatic verification after set/merge
- ✅ Pretty-printed metadata display

**Metadata Operations:**

1. **Set (Overwrite)**
   - Deletes all existing metadata
   - Sets new metadata
   - Use when replacing all metadata

2. **Merge (Update/Add)**
   - Preserves existing metadata
   - Updates existing keys
   - Adds new keys
   - Use when adding to existing metadata

3. **Get (Retrieve)**
   - Retrieves all metadata
   - Returns key-value pairs
   - Use to view current metadata

**What You'll Learn:**
- How to structure metadata as key-value pairs
- Difference between overwrite and merge operations
- How to validate metadata before sending
- How to retrieve and display metadata
- Metadata size limitations

**Common Issues:**
- **Key too long**: Keys limited to `FDFS_MAX_META_NAME_LEN` characters
- **Value too long**: Values limited to `FDFS_MAX_META_VALUE_LEN` characters
- **Invalid format**: Must use `key=value` format
- **Overwrite accident**: Use 'merge' to preserve existing metadata

## Building and Compilation

### Makefile Targets

```bash
make                      # Build all examples
make 01_basic_upload      # Build upload example
make 02_basic_download    # Build download example
make 03_metadata_operations  # Build metadata example
make clean                # Remove built executables
make help                 # Show help
```

### Manual Compilation

If you need to compile manually:

```bash
gcc -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE \
    -I../../common -I../../client -I../../tracker \
    -o 01_basic_upload 01_basic_upload.c \
    -L../../client -L../../common -L../../tracker \
    -lfdfsclient -lfastcommon -lpthread -lm
```

### Compiler Flags Explained

- `-Wall`: Enable all warnings
- `-D_FILE_OFFSET_BITS=64`: Support large files (>2GB)
- `-D_GNU_SOURCE`: Enable GNU extensions
- `-I../../common`: Include common headers
- `-I../../client`: Include client headers
- `-I../../tracker`: Include tracker headers
- `-lfdfsclient`: Link FastDFS client library
- `-lfastcommon`: Link FastCommon library
- `-lpthread`: Link pthread library
- `-lm`: Link math library

## Troubleshooting

### Connection Issues

**Problem:** Cannot connect to tracker server

**Solutions:**
1. Verify tracker server is running: `ps aux | grep fdfs_trackerd`
2. Check tracker address in `client.conf`
3. Test network connectivity: `telnet <tracker_ip> 22122`
4. Check firewall rules
5. Review tracker logs: `/opt/fastdfs/logs/trackerd.log`

---

**Problem:** Cannot connect to storage server

**Solutions:**
1. Verify storage server is running: `ps aux | grep fdfs_storaged`
2. Check storage server registration with tracker
3. Review storage logs: `/opt/fastdfs/logs/storaged.log`
4. Ensure storage server has available disk space

### Upload Issues

**Problem:** Upload fails with timeout

**Solutions:**
1. Increase `network_timeout` in `client.conf`
2. Check network bandwidth
3. Verify storage server disk I/O performance
4. Check storage server disk space

---

**Problem:** File too large error

**Solutions:**
1. Check `max_file_size` setting on storage server
2. Split large files into chunks
3. Use appender files for very large files

### Download Issues

**Problem:** Download incomplete or corrupted

**Solutions:**
1. Verify file integrity using CRC32 checksum
2. Check network stability
3. Increase timeout settings
4. Use callback method for better error handling

### Metadata Issues

**Problem:** Metadata not saved

**Solutions:**
1. Verify file exists before setting metadata
2. Check key/value length limits
3. Ensure storage server has write permissions
4. Use 'get' operation to verify metadata was saved

## Best Practices

### 1. Error Handling
- Always check return codes
- Provide meaningful error messages
- Clean up resources on error
- Log errors for debugging

### 2. Resource Management
- Close connections properly
- Free allocated memory
- Use connection pooling for multiple operations
- Destroy client on exit

### 3. Performance
- Reuse connections when possible
- Use appropriate download method for file size
- Enable connection pooling in config
- Batch operations when possible

### 4. Security
- Validate all input parameters
- Use secure configuration file permissions
- Implement anti-steal token for HTTP access
- Validate file types before upload

### 5. Metadata
- Use descriptive key names
- Keep values concise
- Use 'merge' to preserve existing metadata
- Document metadata schema

## Advanced Topics

### Connection Pooling

Enable in `client.conf`:
```ini
use_connection_pool = true
connection_pool_max_idle_time = 3600
```

Benefits:
- Faster operations (no reconnection overhead)
- Better resource utilization
- Improved performance for multiple operations

### Large File Handling

For files > 100MB:
- Use callback download method
- Implement progress tracking
- Increase network timeout
- Consider chunked upload/download

### Error Recovery

Implement retry logic:
```c
int max_retries = 3;
int retry_count = 0;
int result;

while (retry_count < max_retries) {
    result = storage_upload_by_filename(...);
    if (result == 0) break;
    
    retry_count++;
    sleep(1);  // Wait before retry
}
```

## Additional Resources

### Documentation
- [FastDFS Wiki](https://github.com/happyfish100/fastdfs/wiki)
- [Client API Reference](../../client/README.md)
- [Configuration Guide](../../conf/README.md)

### Source Code
- Client implementation: `../../client/`
- Storage client: `../../client/storage_client.c`
- Tracker client: `../../tracker/tracker_client.c`

### Community
- [GitHub Issues](https://github.com/happyfish100/fastdfs/issues)
- [FastDFS Forum](http://bbs.chinaunix.net/forum-240-1.html)

## Contributing

To add new examples:

1. Follow the existing code style and structure
2. Include comprehensive inline comments
3. Add error handling for all operations
4. Document expected output and common issues
5. Update this README with your example
6. Test thoroughly before submitting

## License

These examples are provided under the GPL v3 license, same as FastDFS.

---

**Need Help?**

If you encounter issues:
1. Check the troubleshooting section above
2. Review the inline comments in the source code
3. Check FastDFS logs for detailed error messages
4. Consult the FastDFS documentation
5. Ask on the FastDFS community forums
