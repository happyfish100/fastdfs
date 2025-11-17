.PHONY: all build test test-race test-cover lint fmt clean examples help

# Default target
all: fmt lint test

# Build the project
build:
	@echo "Building..."
	@go build ./...

# Run tests
test:
	@echo "Running tests..."
	@go test -v ./...

# Run tests with race detector
test-race:
	@echo "Running tests with race detector..."
	@go test -race -v ./...

# Run tests with coverage
test-cover:
	@echo "Running tests with coverage..."
	@go test -cover -coverprofile=coverage.txt ./...
	@go tool cover -html=coverage.txt -o coverage.html
	@echo "Coverage report generated: coverage.html"

# Run integration tests
test-integration:
	@echo "Running integration tests..."
	@go test -tags=integration -v ./...

# Run linters
lint:
	@echo "Running linters..."
	@go vet ./...
	@test -z "$$(gofmt -l .)" || (echo "Code is not formatted. Run 'make fmt'" && exit 1)

# Format code
fmt:
	@echo "Formatting code..."
	@gofmt -w .

# Clean build artifacts
clean:
	@echo "Cleaning..."
	@rm -f coverage.txt coverage.html
	@go clean ./...

# Build examples
examples:
	@echo "Building examples..."
	@cd examples/basic && go build
	@cd examples/metadata && go build
	@cd examples/appender && go build
	@echo "Examples built successfully"

# Run example
run-example-basic:
	@cd examples/basic && go run main.go

run-example-metadata:
	@cd examples/metadata && go run main.go

run-example-appender:
	@cd examples/appender && go run main.go

# Install dependencies
deps:
	@echo "Installing dependencies..."
	@go mod download
	@go mod tidy

# Update dependencies
update-deps:
	@echo "Updating dependencies..."
	@go get -u ./...
	@go mod tidy

# Show help
help:
	@echo "FastDFS Go Client - Makefile targets:"
	@echo ""
	@echo "  make              - Format, lint, and test"
	@echo "  make build        - Build the project"
	@echo "  make test         - Run tests"
	@echo "  make test-race    - Run tests with race detector"
	@echo "  make test-cover   - Run tests with coverage report"
	@echo "  make test-integration - Run integration tests"
	@echo "  make lint         - Run linters"
	@echo "  make fmt          - Format code"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make examples     - Build all examples"
	@echo "  make run-example-basic - Run basic example"
	@echo "  make run-example-metadata - Run metadata example"
	@echo "  make run-example-appender - Run appender example"
	@echo "  make deps         - Install dependencies"
	@echo "  make update-deps  - Update dependencies"
	@echo "  make help         - Show this help message"
