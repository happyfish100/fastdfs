# FastDFS Modern CLI Tool

A modern, feature-rich command-line interface for FastDFS with enhanced usability and productivity features.

## Features

### ⭐ Core Capabilities
- **Upload**: Upload files to FastDFS storage
- **Download**: Download files from FastDFS storage  
- **Delete**: Remove files from storage
- **Info**: Get detailed file information
- **Batch Operations**: Process multiple files from a list
- **Interactive Mode**: REPL-style interface for multiple operations

### 🎨 Modern UX Features
- **Colored Output**: Easy-to-read color-coded messages
- **Progress Bars**: Visual feedback for long operations
- **JSON Output**: Machine-readable output for automation
- **Human-Readable Sizes**: Automatic file size formatting (B, KB, MB, GB, TB)
- **Timestamp Formatting**: Human-friendly date/time display

## Installation

The CLI tool is built automatically when you build FastDFS:

```bash
cd fastdfs
./make.sh
./make.sh install
```

The `fdfs_cli` binary will be installed to `/usr/bin` (or your configured `TARGET_PREFIX/bin`).

## Usage

### Basic Syntax

```bash
fdfs_cli [options] <command> [args...]
```

### Options

| Option | Description |
|--------|-------------|
| `-c <config>` | Configuration file path (required) |
| `-j` | Enable JSON output format |
| `-n` | Disable colored output |
| `-v` | Enable verbose mode |
| `-p <index>` | Specify storage path index |
| `-h` | Show help message |

### Commands

#### Upload a File

```bash
# Upload to default group
fdfs_cli -c /etc/fdfs/client.conf upload /path/to/file.jpg

# Upload to specific group
fdfs_cli -c /etc/fdfs/client.conf upload /path/to/file.jpg group1

# With JSON output
fdfs_cli -c /etc/fdfs/client.conf -j upload /path/to/file.jpg
```

**Output:**
```
Uploading: /path/to/file.jpg (2.45 MB)
Progress [==================================================] 100%
✓ Upload successful!
File ID: group1/M00/00/00/wKgBaGFxxx.jpg
```

#### Download a File

```bash
# Download with auto-generated filename
fdfs_cli -c /etc/fdfs/client.conf download group1/M00/00/00/wKgBaGFxxx.jpg

# Download to specific location
fdfs_cli -c /etc/fdfs/client.conf download group1/M00/00/00/wKgBaGFxxx.jpg /tmp/myfile.jpg
```

**Output:**
```
Downloading: group1/M00/00/00/wKgBaGFxxx.jpg
Progress [==================================================] 100%
✓ Download successful!
Saved to: /tmp/myfile.jpg (2.45 MB)
```

#### Delete a File

```bash
fdfs_cli -c /etc/fdfs/client.conf delete group1/M00/00/00/wKgBaGFxxx.jpg
```

**Output:**
```
✓ File deleted: group1/M00/00/00/wKgBaGFxxx.jpg
```

#### Get File Information

```bash
fdfs_cli -c /etc/fdfs/client.conf info group1/M00/00/00/wKgBaGFxxx.jpg
```

**Output:**
```
File Information
================
File ID:   group1/M00/00/00/wKgBaGFxxx.jpg
Size:      2.45 MB (2568192 bytes)
Created:   2025-11-19 22:30:45
CRC32:     0x12345678
Source IP: 192.168.1.100
```

#### Batch Operations

Create a file list (`files.txt`):
```
/path/to/file1.jpg
/path/to/file2.png
/path/to/file3.pdf
# Comments are supported
/path/to/file4.doc
```

Run batch upload:
```bash
fdfs_cli -c /etc/fdfs/client.conf batch upload files.txt
```

**Output:**
```
Batch upload: 4 files
Batch [==================================================] 100%

Summary: Success=4 Failed=0 Total=4
```

Batch operations support:
- `batch upload <file_list>` - Upload multiple files
- `batch download <file_list>` - Download multiple files (list contains file IDs)
- `batch delete <file_list>` - Delete multiple files (list contains file IDs)

#### Interactive Mode

```bash
fdfs_cli -c /etc/fdfs/client.conf interactive
```

**Interactive Session:**
```
FastDFS Interactive CLI
Type 'help' for commands, 'exit' to quit

fdfs> help
Commands: upload <file> [group] | download <fid> [dest] | delete <fid> | info <fid> | batch <op> <list> | exit

fdfs> upload test.jpg
Uploading: test.jpg (1.23 MB)
Progress [==================================================] 100%
✓ Upload successful!
File ID: group1/M00/00/00/wKgBaGFxxx.jpg

fdfs> info group1/M00/00/00/wKgBaGFxxx.jpg
File Information
================
File ID:   group1/M00/00/00/wKgBaGFxxx.jpg
Size:      1.23 MB (1290240 bytes)
Created:   2025-11-19 22:35:12
CRC32:     0xABCDEF01
Source IP: 192.168.1.100

fdfs> exit
Goodbye!
```

## JSON Output Format

Enable JSON output with the `-j` flag for easy integration with scripts and automation tools.

### Upload Response
```json
{
  "operation": "upload",
  "success": true,
  "file_id": "group1/M00/00/00/wKgBaGFxxx.jpg"
}
```

### Download Response
```json
{
  "operation": "download",
  "success": true,
  "file_id": "group1/M00/00/00/wKgBaGFxxx.jpg",
  "local": "/tmp/myfile.jpg",
  "size": 2568192
}
```

### Info Response
```json
{
  "operation": "info",
  "success": true,
  "file_id": "group1/M00/00/00/wKgBaGFxxx.jpg",
  "size": 2568192,
  "timestamp": 1700432445,
  "crc32": 305441401,
  "source_ip": "192.168.1.100"
}
```

### Error Response
```json
{
  "operation": "upload",
  "success": false,
  "error_code": 2,
  "error": "No such file or directory"
}
```

### Batch Response
```json
{
  "operation": "batch_upload",
  "total": 10,
  "success": 9,
  "failed": 1
}
```

## Examples

### Automation Script

```bash
#!/bin/bash

# Upload with error handling
result=$(fdfs_cli -c /etc/fdfs/client.conf -j upload photo.jpg)
if echo "$result" | grep -q '"success":true'; then
    file_id=$(echo "$result" | grep -o '"file_id":"[^"]*"' | cut -d'"' -f4)
    echo "Uploaded successfully: $file_id"
else
    echo "Upload failed"
    exit 1
fi
```

### Batch Processing

```bash
# Generate file list
find /photos -name "*.jpg" > upload_list.txt

# Batch upload
fdfs_cli -c /etc/fdfs/client.conf batch upload upload_list.txt

# With JSON output for logging
fdfs_cli -c /etc/fdfs/client.conf -j batch upload upload_list.txt >> upload_log.json
```

### Pipeline Integration

```bash
# Upload and immediately get info
file_id=$(fdfs_cli -c /etc/fdfs/client.conf upload test.jpg | tail -1)
fdfs_cli -c /etc/fdfs/client.conf info "$file_id"
```

## Configuration

The CLI tool uses the standard FastDFS client configuration file. Example `/etc/fdfs/client.conf`:

```ini
connect_timeout = 30
network_timeout = 60
base_path = /tmp
tracker_server = 192.168.1.100:22122
tracker_server = 192.168.1.101:22122
```

## Tips & Best Practices

1. **Use JSON output for scripts**: The `-j` flag provides consistent, parseable output
2. **Batch operations for efficiency**: Process multiple files in one command
3. **Interactive mode for exploration**: Great for testing and learning
4. **Disable colors in scripts**: Use `-n` flag when piping output
5. **Store file IDs**: Keep track of uploaded file IDs for later retrieval

## Troubleshooting

### Connection Errors
```
Error: Tracker connection failed
```
- Check if tracker server is running
- Verify `tracker_server` in config file
- Check network connectivity

### File Not Found
```
Error: File not found: /path/to/file
```
- Verify file path is correct
- Check file permissions

### Configuration Issues
```
Error: Configuration file required (-c option)
```
- Always specify config file with `-c` option
- Verify config file exists and is readable

## Performance Notes

- **Batch operations** are more efficient than individual commands in loops
- **Progress bars** add minimal overhead and can be disabled with `-n` for maximum performance
- **JSON output** has slightly less overhead than formatted output

## Comparison with Original Tools

| Feature | Original Tools | Modern CLI |
|---------|---------------|------------|
| Upload | `fdfs_upload_file` | `fdfs_cli upload` |
| Download | `fdfs_download_file` | `fdfs_cli download` |
| Delete | `fdfs_delete_file` | `fdfs_cli delete` |
| Info | `fdfs_file_info` | `fdfs_cli info` |
| Batch | ❌ | ✅ |
| Interactive | ❌ | ✅ |
| Progress Bars | ❌ | ✅ |
| Colored Output | ❌ | ✅ |
| JSON Output | ❌ | ✅ |
| Single Binary | ❌ | ✅ |

## License

Copyright (C) 2008 Happy Fish / YuQing

FastDFS may be copied only under the terms of the GNU General Public License V3.

## Contributing

Contributions are welcome! Please see the main FastDFS repository for contribution guidelines.
