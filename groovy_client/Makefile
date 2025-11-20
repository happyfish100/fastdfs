# FastDFS Groovy Client - Makefile
#
# This Makefile provides convenient commands for building, testing, and
# managing the FastDFS Groovy client project.
#
# Usage:
#   make build          - Build the project
#   make test           - Run unit tests
#   make clean          - Clean build artifacts
#   make install        - Install to local Maven repository
#   make publish        - Publish to repository
#

# Default target
.DEFAULT_GOAL := help

# Gradle wrapper
GRADLE = ./gradlew
GRADLE_FLAGS = --no-daemon

# Help target
.PHONY: help
help:
	@echo "FastDFS Groovy Client - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  make build          - Build the project"
	@echo "  make test           - Run unit tests"
	@echo "  make integration    - Run integration tests"
	@echo "  make clean          - Clean build artifacts"
	@echo "  make install        - Install to local Maven repository"
	@echo "  make publish        - Publish to repository"
	@echo "  make jar            - Build JAR file"
	@echo "  make sources        - Build sources JAR"
	@echo "  make javadoc        - Generate Javadoc"
	@echo "  make check          - Run code quality checks"
	@echo "  make help           - Show this help message"

# Build target
.PHONY: build
build:
	$(GRADLE) $(GRADLE_FLAGS) build

# Test target
.PHONY: test
test:
	$(GRADLE) $(GRADLE_FLAGS) test

# Integration test target
.PHONY: integration
integration:
	$(GRADLE) $(GRADLE_FLAGS) integrationTest

# Clean target
.PHONY: clean
clean:
	$(GRADLE) $(GRADLE_FLAGS) clean

# Install target
.PHONY: install
install:
	$(GRADLE) $(GRADLE_FLAGS) install

# Publish target
.PHONY: publish
publish:
	$(GRADLE) $(GRADLE_FLAGS) publish

# JAR target
.PHONY: jar
jar:
	$(GRADLE) $(GRADLE_FLAGS) jar

# Sources JAR target
.PHONY: sources
sources:
	$(GRADLE) $(GRADLE_FLAGS) sourcesJar

# Javadoc target
.PHONY: javadoc
javadoc:
	$(GRADLE) $(GRADLE_FLAGS) javadoc

# Check target
.PHONY: check
check:
	$(GRADLE) $(GRADLE_FLAGS) check

