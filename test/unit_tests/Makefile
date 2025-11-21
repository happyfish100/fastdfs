# ==============================================================================
# Makefile for FastDFS Client API Unit Tests
# Copyright (C) 2008 Happy Fish / YuQing
#
# Purpose: Build and run unit tests for FastDFS client API
# Usage:
#   make              - Build all test programs
#   make test         - Build and run all tests with default config
#   make test-config CONFIG=/path/to/client.conf - Run with custom config
#   make clean        - Remove build artifacts
#   make help         - Show detailed help
# ==============================================================================

.SUFFIXES: .c .o

# ------------------------------------------------------------------------------
# Compiler Configuration
# ------------------------------------------------------------------------------
CC = gcc                                    # C compiler
CFLAGS = -g                                 # Debug symbols
CFLAGS += -Wall                             # Enable all warnings
CFLAGS += -O2                               # Optimization level 2
CFLAGS += -D_GNU_SOURCE                     # GNU extensions
CFLAGS += -D_FILE_OFFSET_BITS=64           # Large file support (>2GB)
CFLAGS += -DDEBUG                          # Enable debug mode

# ------------------------------------------------------------------------------
# Include Paths
# ------------------------------------------------------------------------------
# Add paths where FastDFS header files are located
INC_PATH = -I/usr/local/include            # System-wide headers
INC_PATH += -I../../client                 # FastDFS client headers
INC_PATH += -I../../common                 # FastDFS common headers

# ------------------------------------------------------------------------------
# Library Configuration
# ------------------------------------------------------------------------------
LIB_PATH = -L/usr/local/lib                # Library search path
LIBS = -lfdfsclient                        # FastDFS client library
LIBS += -lfastcommon                       # FastCommon utility library
LIBS += -lserverframe                      # Server framework library
LIBS += -lpthread                          # POSIX threads
LIBS += -lm                                # Math library

# ------------------------------------------------------------------------------
# Installation Configuration
# ------------------------------------------------------------------------------
TARGET_PATH = $(TARGET_PREFIX)/bin         # Installation directory

# ------------------------------------------------------------------------------
# Build Targets
# ------------------------------------------------------------------------------
TEST_PRGS = test_client_api                # List of test programs to build

# Object files for shared test utilities (currently none, but extensible)
OBJS = 

# ==============================================================================
# Build Rules
# ==============================================================================

# Default target: build all test programs
all: $(TEST_PRGS)

# Build test_client_api executable
# Links test source with required FastDFS libraries
test_client_api: test_client_api.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(INC_PATH) $(LIB_PATH) $(LIBS)

# Generic rule for compiling C source files to object files
# Used if shared test utilities are added in the future
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< $(INC_PATH)

# ==============================================================================
# Test Execution Targets
# ==============================================================================

# Run all tests with default configuration (/etc/fdfs/client.conf)
# Exits with error code if any test fails
test: $(TEST_PRGS)
	@echo "=========================================="
	@echo "Running FastDFS Client API Unit Tests"
	@echo "=========================================="
	@for test in $(TEST_PRGS); do \
		echo ""; \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo ""
	@echo "=========================================="
	@echo "All tests completed successfully!"
	@echo "=========================================="

# Run tests with custom configuration file
# Usage: make test-config CONFIG=/path/to/client.conf
# This allows testing against different FastDFS server configurations
test-config: $(TEST_PRGS)
	@if [ -z "$(CONFIG)" ]; then \
		echo "Error: CONFIG variable not set"; \
		echo "Usage: make test-config CONFIG=/path/to/client.conf"; \
		exit 1; \
	fi
	@echo "=========================================="
	@echo "Running tests with config: $(CONFIG)"
	@echo "=========================================="
	@for test in $(TEST_PRGS); do \
		echo ""; \
		echo "Running $$test..."; \
		./$$test $(CONFIG) || exit 1; \
	done

# Run individual test program (useful for debugging specific tests)
run-client-api: test_client_api
	@echo "Running test_client_api..."
	./test_client_api

# ==============================================================================
# Installation Target
# ==============================================================================

# Install test programs to system directory
# Respects TARGET_PATH environment variable or uses /usr/local/bin as default
install: $(TEST_PRGS)
	@if [ -z "$(TARGET_PATH)" ]; then \
		echo "Warning: TARGET_PATH not set, using /usr/local/bin"; \
		mkdir -p /usr/local/bin; \
		cp -f $(TEST_PRGS) /usr/local/bin/; \
	else \
		mkdir -p $(TARGET_PATH); \
		cp -f $(TEST_PRGS) $(TARGET_PATH)/; \
	fi
	@echo "Test programs installed successfully"

# ==============================================================================
# Maintenance Targets
# ==============================================================================

# Clean all build artifacts (executables, object files, core dumps)
clean:
	rm -f $(TEST_PRGS) $(OBJS) *.o core core.*
	@echo "Clean completed"

# Clean and rebuild everything from scratch
rebuild: clean all

# ==============================================================================
# Help Target
# ==============================================================================

# Display comprehensive help information about available targets
help:
	@echo "FastDFS Client API Unit Tests Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build all test programs (default)"
	@echo "  test             - Build and run all tests"
	@echo "  test-config      - Run tests with custom config file"
	@echo "                     Usage: make test-config CONFIG=/path/to/client.conf"
	@echo "  run-client-api   - Run test_client_api only"
	@echo "  install          - Install test programs to TARGET_PATH"
	@echo "  clean            - Remove build artifacts"
	@echo "  rebuild          - Clean and rebuild all"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make                                    # Build all tests"
	@echo "  make test                               # Build and run all tests"
	@echo "  make test-config CONFIG=/etc/fdfs/client.conf"
	@echo "  make run-client-api                     # Run specific test"
	@echo "  make clean                              # Clean build files"
	@echo ""
	@echo "Environment variables:"
	@echo "  CC           - C compiler (default: gcc)"
	@echo "  CFLAGS       - Compiler flags"
	@echo "  TARGET_PATH  - Installation directory"

# ==============================================================================
# Phony Targets Declaration
# ==============================================================================
# Declare targets that don't represent actual files
# This prevents conflicts with files of the same name and improves performance
.PHONY: all test test-config run-client-api install clean rebuild help
