# Changelog

All notable changes to the FastDFS Java Client will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-01-01

### Added
- Initial release of FastDFS Java Client
- File upload operations (normal and appender files)
- File download operations (full and partial)
- File deletion
- Metadata operations (set and get)
- File information queries
- Connection pooling with automatic cleanup
- Automatic retry mechanism for failed operations
- Comprehensive exception hierarchy
- Thread-safe operations
- Java 8+ compatibility
- Maven support
- Complete documentation and examples
- Basic usage examples
- Metadata operations examples
- Concurrent upload examples

### Features
- Support for multiple tracker servers
- Configurable connection pool size
- Configurable timeouts (connect, network, idle)
- Configurable retry count
- Automatic failover
- Protocol-compliant implementation
- SLF4J logging integration

### Documentation
- Comprehensive README with usage examples
- JavaDoc for all public APIs
- Contributing guidelines
- Example code for common use cases

## [Unreleased]

### Planned Features
- Slave file upload support
- Appender file operations (append, modify, truncate)
- Batch operations
- Async API support
- Connection health checks
- Metrics and monitoring
- Spring Boot integration
- More comprehensive test suite

---

For more information, see the [README](README.md).
