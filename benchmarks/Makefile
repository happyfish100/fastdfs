# FastDFS Benchmark Suite Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -I../client -I../common
LDFLAGS = -L../client -L../common -lfdfsclient -lfastcommon -lpthread -lm

# Source files
UPLOAD_SRC = benchmark_upload.c
DOWNLOAD_SRC = benchmark_download.c
CONCURRENT_SRC = benchmark_concurrent.c
SMALL_FILES_SRC = benchmark_small_files.c
LARGE_FILES_SRC = benchmark_large_files.c
METADATA_SRC = benchmark_metadata.c

# Output binaries
UPLOAD_BIN = benchmark_upload
DOWNLOAD_BIN = benchmark_download
CONCURRENT_BIN = benchmark_concurrent
SMALL_FILES_BIN = benchmark_small_files
LARGE_FILES_BIN = benchmark_large_files
METADATA_BIN = benchmark_metadata

# All targets
ALL_BINS = $(UPLOAD_BIN) $(DOWNLOAD_BIN) $(CONCURRENT_BIN) \
           $(SMALL_FILES_BIN) $(LARGE_FILES_BIN) $(METADATA_BIN)

.PHONY: all clean install help test

# Default target
all: $(ALL_BINS)
	@echo ""
	@echo "✓ All benchmarks compiled successfully!"
	@echo ""
	@echo "Run 'make test' to verify the build"
	@echo "Run 'make install' to install benchmarks"
	@echo "Run './scripts/run_all_benchmarks.sh' to run all benchmarks"

# Individual benchmark targets
$(UPLOAD_BIN): $(UPLOAD_SRC)
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(DOWNLOAD_BIN): $(DOWNLOAD_SRC)
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(CONCURRENT_BIN): $(CONCURRENT_SRC)
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(SMALL_FILES_BIN): $(SMALL_FILES_SRC)
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(LARGE_FILES_BIN): $(LARGE_FILES_SRC)
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(METADATA_BIN): $(METADATA_SRC)
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(ALL_BINS)
	rm -f *.o
	rm -f results/*.json
	@echo "✓ Clean complete"

# Install benchmarks to system
install: all
	@echo "Installing benchmarks to /usr/local/bin..."
	@mkdir -p /usr/local/bin
	@for bin in $(ALL_BINS); do \
		install -m 755 $$bin /usr/local/bin/; \
		echo "  Installed $$bin"; \
	done
	@echo "✓ Installation complete"

# Test that benchmarks can run
test: all
	@echo "Testing benchmarks..."
	@for bin in $(ALL_BINS); do \
		echo "  Testing $$bin..."; \
		./$$bin --help > /dev/null 2>&1 && echo "    ✓ $$bin OK" || echo "    ✗ $$bin FAILED"; \
	done
	@echo "✓ Tests complete"

# Show help
help:
	@echo "FastDFS Benchmark Suite - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build all benchmarks (default)"
	@echo "  clean            - Remove build artifacts"
	@echo "  install          - Install benchmarks to /usr/local/bin"
	@echo "  test             - Test that benchmarks can run"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Individual benchmarks:"
	@echo "  $(UPLOAD_BIN)"
	@echo "  $(DOWNLOAD_BIN)"
	@echo "  $(CONCURRENT_BIN)"
	@echo "  $(SMALL_FILES_BIN)"
	@echo "  $(LARGE_FILES_BIN)"
	@echo "  $(METADATA_BIN)"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build all benchmarks"
	@echo "  make clean        # Clean build artifacts"
	@echo "  make install      # Install to system"
	@echo "  make test         # Test benchmarks"
	@echo ""

# Dependencies (simplified - in real project would use proper dependency tracking)
$(UPLOAD_BIN): $(UPLOAD_SRC)
$(DOWNLOAD_BIN): $(DOWNLOAD_SRC)
$(CONCURRENT_BIN): $(CONCURRENT_SRC)
$(SMALL_FILES_BIN): $(SMALL_FILES_SRC)
$(LARGE_FILES_BIN): $(LARGE_FILES_SRC)
$(METADATA_BIN): $(METADATA_SRC)
