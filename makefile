# Mocha Makefile
# Root makefile to build both the compiler and standard library

# Installation prefix
PREFIX ?= /usr/local

# Default target builds everything
all: compiler

# Build just the compiler
compiler:
	@echo "Building compiler..."
	@$(MAKE) -C mx

# Install both compiler and standard library
install: install-compiler install-stdlib

# Install just the compiler
install-compiler:
	@echo "Installing compiler..."
	@$(MAKE) -C mx install PREFIX=$(PREFIX)

# Install just the standard library
install-stdlib:
	@echo "Installing standard library..."
	@$(MAKE) -C lib install PREFIX=$(PREFIX)/lib

# Clean build artifacts
clean:
	@echo "Cleaning compiler..."
	@$(MAKE) -C mx clean

# Uninstall everything
uninstall:
	@echo "Uninstalling compiler..."
	@rm -f $(PREFIX)/bin/mx
	@echo "Uninstalling standard library..."
	@$(MAKE) -C lib uninstall PREFIX=$(PREFIX)/lib

# Testing
test:
	@echo "Running tests..."
	@$(MAKE) -C test

# Only build stdlib (placeholder, may need adjustment)
stdlib:
	@echo "Building standard library..."
	@$(MAKE) -C lib

# Help target to display available make targets
help:
	@echo "Mocha Build System"
	@echo "=================="
	@echo "Available targets:"
	@echo "  all            - Build the compiler (default target)"
	@echo "  compiler       - Build just the compiler"
	@echo "  stdlib         - Build just the standard library"
	@echo "  install        - Install compiler and standard library"
	@echo "  install-compiler - Install just the compiler"
	@echo "  install-stdlib - Install just the standard library"
	@echo "  uninstall      - Uninstall compiler and standard library"
	@echo "  clean          - Clean build artifacts"
	@echo "  test           - Run tests"
	@echo "  help           - Display this help message"
	@echo ""
	@echo "Configuration options:"
	@echo "  PREFIX         - Installation prefix (default: /usr/local)"

.PHONY: all compiler stdlib install install-compiler install-stdlib clean uninstall test help