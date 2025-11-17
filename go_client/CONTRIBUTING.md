# Contributing to FastDFS Go Client

Thank you for your interest in contributing to the FastDFS Go Client!

## Development Setup

### Prerequisites

- Go 1.21 or later
- Access to a FastDFS cluster for integration testing
- Git

### Clone and Build

```bash
git clone https://github.com/happyfish100/fastdfs.git
cd fastdfs/go_client
go mod download
go build ./...
```

### Running Tests

```bash
# Run unit tests
go test ./...

# Run tests with coverage
go test -cover ./...

# Run tests with race detector
go test -race ./...

# Run integration tests (requires FastDFS cluster)
go test -tags=integration ./...
```

## Code Style

- Follow standard Go conventions and idioms
- Use `gofmt` to format code
- Use `golint` and `go vet` to check for issues
- Write clear, descriptive comments
- Keep functions focused and testable

### Formatting

```bash
# Format code
gofmt -w .

# Check for issues
go vet ./...
golint ./...
```

## Pull Request Process

1. **Fork the repository** and create your branch from `master`

2. **Make your changes**:
   - Write clear, concise commit messages
   - Add tests for new functionality
   - Update documentation as needed
   - Ensure all tests pass

3. **Test your changes**:
   ```bash
   go test ./...
   go test -race ./...
   ```

4. **Submit a pull request**:
   - Provide a clear description of the changes
   - Reference any related issues
   - Ensure CI checks pass

## Adding New Features

When adding new features:

1. **Design first**: Discuss the feature in an issue before implementing
2. **Write tests**: Add unit tests and integration tests
3. **Document**: Update README.md and add code comments
4. **Examples**: Add usage examples if appropriate
5. **Backward compatibility**: Maintain compatibility with existing code

## Testing Guidelines

### Unit Tests

- Test all public functions
- Test error conditions
- Use table-driven tests where appropriate
- Mock external dependencies

Example:
```go
func TestNewClient(t *testing.T) {
    tests := []struct {
        name    string
        config  *ClientConfig
        wantErr bool
    }{
        // test cases...
    }
    
    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            // test implementation...
        })
    }
}
```

### Integration Tests

- Tag integration tests with `//go:build integration`
- Require a running FastDFS cluster
- Clean up resources after tests
- Test real-world scenarios

Example:
```go
//go:build integration

package fdfs

import "testing"

func TestIntegrationUpload(t *testing.T) {
    // integration test implementation...
}
```

## Documentation

- Keep README.md up to date
- Document all exported functions and types
- Include usage examples
- Update CHANGELOG.md for significant changes

### Documentation Style

```go
// UploadFile uploads a file from the local filesystem to FastDFS.
//
// Parameters:
//   - ctx: context for cancellation and timeout
//   - localFilename: path to the local file
//   - metadata: optional metadata key-value pairs
//
// Returns the file ID on success.
//
// Example:
//   fileID, err := client.UploadFile(ctx, "test.jpg", nil)
func (c *Client) UploadFile(ctx context.Context, localFilename string, metadata map[string]string) (string, error) {
    // implementation...
}
```

## Reporting Issues

When reporting issues, please include:

- Go version
- FastDFS version
- Operating system
- Steps to reproduce
- Expected behavior
- Actual behavior
- Error messages and stack traces

## Code of Conduct

- Be respectful and inclusive
- Welcome newcomers
- Focus on constructive feedback
- Help others learn and grow

## Questions?

- Open an issue for questions
- Check existing issues and pull requests
- Contact the maintainers

## License

By contributing, you agree that your contributions will be licensed under the GNU General Public License V3.
