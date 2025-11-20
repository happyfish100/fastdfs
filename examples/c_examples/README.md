# FastDFS C Examples

Simple, well-documented C examples for FastDFS client operations.

## Examples

| File | Description |
|------|-------------|
| `01_basic_upload.c` | Upload files to FastDFS |
| `02_basic_download.c` | Download files (3 methods) |
| `03_metadata_operations.c` | Manage file metadata |

## Quick Start

### 1. Setup Configuration

```bash
cp ../../conf/client.conf .
# Edit client.conf with your tracker server address
```

### 2. Build

```bash
make
```

### 3. Run

**Upload:**
```bash
./01_basic_upload client.conf /path/to/file.jpg
# Output: File ID like group1/M00/00/00/wKgBcGXxxx.jpg
```

**Download:**
```bash
./02_basic_download client.conf group1/M00/00/00/wKgBcGXxxx.jpg
./02_basic_download client.conf group1/M00/00/00/wKgBcGXxxx.jpg output.jpg
```

**Metadata:**
```bash
# Set (overwrites all)
./03_metadata_operations client.conf set group1/M00/00/00/xxx.jpg width=1920 height=1080

# Merge (add/update)
./03_metadata_operations client.conf merge group1/M00/00/00/xxx.jpg author=John

# Get
./03_metadata_operations client.conf get group1/M00/00/00/xxx.jpg
```

## What Each Example Does

### 01_basic_upload.c
- Uploads a file to FastDFS storage
- Returns file ID for later retrieval
- Shows file info (size, CRC32, timestamp)

### 02_basic_download.c
- Downloads files using 3 methods:
  - Direct to file (efficient for large files)
  - To buffer (flexible for processing)
  - Callback (streaming with progress)
- Verifies file size after download

### 03_metadata_operations.c
- **set**: Replace all metadata (overwrites)
- **merge**: Add/update metadata (preserves existing)
- **get**: Retrieve all metadata
- Format: `key=value` pairs

## Build Commands

```bash
make                         # Build all
make 01_basic_upload         # Build specific example
make clean                   # Clean build files
```

## Common Issues

| Problem | Solution |
|---------|----------|
| Cannot connect to tracker | Check tracker is running, verify `tracker_server` in config |
| Upload timeout | Increase `network_timeout` in `client.conf` |
| File not found | Verify file ID format: `group_name/path/filename` |
| Metadata not saved | Check key/value length limits, verify file exists |

## Tips

- All examples include detailed error messages
- Check return codes (0 = success)
- Use `merge` to preserve existing metadata
- Enable connection pooling for better performance:
  ```ini
  use_connection_pool = true
  ```

## Resources

- [FastDFS Wiki](https://github.com/happyfish100/fastdfs/wiki)
- Source code: `../../client/`
- Each example has extensive inline comments
