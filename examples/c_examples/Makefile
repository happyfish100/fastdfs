# Makefile for FastDFS C Examples
# Copyright (C) 2024

.PHONY: all clean

# Compiler and flags
CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I../../common -I../../client -I../../tracker
LDFLAGS = -L../../client -L../../common -L../../tracker
LIBS = -lfdfsclient -lfastcommon -lpthread -lm

# Output directory
BUILD_DIR = .

# Source files
SOURCES = 01_basic_upload.c 02_basic_download.c 03_metadata_operations.c

# Executable names (without .c extension)
EXECUTABLES = $(SOURCES:.c=)

# Default target
all: $(EXECUTABLES)

# Pattern rule for building executables
%: %.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LDFLAGS) $(LIBS)
	@echo "Built $@ successfully"

# Individual targets for clarity
01_basic_upload: 01_basic_upload.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LDFLAGS) $(LIBS)

02_basic_download: 02_basic_download.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LDFLAGS) $(LIBS)

03_metadata_operations: 03_metadata_operations.c
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LDFLAGS) $(LIBS)

# Clean build artifacts
clean:
	rm -f $(EXECUTABLES)
	@echo "Cleaned build artifacts"

# Help target
help:
	@echo "FastDFS C Examples Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all                      - Build all examples (default)"
	@echo "  01_basic_upload          - Build basic upload example"
	@echo "  02_basic_download        - Build basic download example"
	@echo "  03_metadata_operations   - Build metadata operations example"
	@echo "  clean                    - Remove all built executables"
	@echo "  help                     - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make                     # Build all examples"
	@echo "  make 01_basic_upload     # Build specific example"
	@echo "  make clean               # Clean build artifacts"
