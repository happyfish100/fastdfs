# FastDFS Examples

This directory contains comprehensive examples demonstrating how to use FastDFS in various programming languages and deployment scenarios.

## Directory Structure

```
examples/
├── README.md                    # This file
├── c_examples/                  # C language examples
├── php_examples/                # PHP examples (coming soon)
├── docker_examples/             # Docker deployment examples (coming soon)
└── integration_examples/        # Integration examples (coming soon)
```

## Available Examples

### C Examples (`c_examples/`)

Complete C examples demonstrating core FastDFS client functionality:

- **01_basic_upload.c** - Upload files to FastDFS storage
- **02_basic_download.c** - Download files from FastDFS storage
- **03_metadata_operations.c** - Set and retrieve file metadata

Each example includes:
- ✅ Clear inline comments
- ✅ Comprehensive error handling
- ✅ Best practices
- ✅ Expected output documentation
- ✅ Common pitfalls and solutions

See [c_examples/README.md](c_examples/README.md) for detailed usage instructions.

## Prerequisites

- FastDFS installed and configured
- Tracker server running
- Storage server running
- Client configuration file (`client.conf`)

## Quick Start

1. **Configure your client**:
   ```bash
   cp conf/client.conf examples/c_examples/
   # Edit client.conf with your tracker server addresses
   ```

2. **Build the C examples**:
   ```bash
   cd examples/c_examples
   make
   ```

3. **Run an example**:
   ```bash
   ./01_basic_upload client.conf test_file.txt
   ```

## Configuration

All examples require a valid `client.conf` file. Key settings:

```ini
# Tracker server addresses
tracker_server = 192.168.0.196:22122

# Connection timeouts
connect_timeout = 5
network_timeout = 60

# Base path for logs
base_path = /opt/fastdfs
```

## Contributing

When adding new examples:
1. Follow the existing structure and naming conventions
2. Include comprehensive comments and error handling
3. Document expected output and common issues
4. Update this README with your example

## Resources

- [FastDFS Documentation](https://github.com/happyfish100/fastdfs/wiki)
- [Client API Reference](../client/README.md)
- [Configuration Guide](../conf/README.md)

## License

These examples follow the same license as FastDFS (GPL v3).
