// Package fdfs error definitions.
// This file defines all error types and error handling utilities for the FastDFS client.
// Errors are categorized into common errors, protocol errors, network errors, and server errors.
package fdfs

import (
	"errors"
	"fmt"
)

// Common errors returned by the FastDFS client.
// These are sentinel errors that can be checked using errors.Is().
var (
	// ErrClientClosed indicates the client has been closed
	ErrClientClosed = errors.New("client is closed")

	// ErrFileNotFound indicates the requested file does not exist
	ErrFileNotFound = errors.New("file not found")

	// ErrNoStorageServer indicates no storage server is available
	ErrNoStorageServer = errors.New("no storage server available")

	// ErrConnectionTimeout indicates connection timeout
	ErrConnectionTimeout = errors.New("connection timeout")

	// ErrNetworkTimeout indicates network I/O timeout
	ErrNetworkTimeout = errors.New("network timeout")

	// ErrInvalidFileID indicates the file ID format is invalid
	ErrInvalidFileID = errors.New("invalid file ID")

	// ErrInvalidResponse indicates the server response is invalid
	ErrInvalidResponse = errors.New("invalid response from server")

	// ErrStorageServerOffline indicates the storage server is offline
	ErrStorageServerOffline = errors.New("storage server is offline")

	// ErrTrackerServerOffline indicates the tracker server is offline
	ErrTrackerServerOffline = errors.New("tracker server is offline")

	// ErrInsufficientSpace indicates insufficient storage space
	ErrInsufficientSpace = errors.New("insufficient storage space")

	// ErrFileAlreadyExists indicates the file already exists
	ErrFileAlreadyExists = errors.New("file already exists")

	// ErrInvalidMetadata indicates invalid metadata format
	ErrInvalidMetadata = errors.New("invalid metadata")

	// ErrOperationNotSupported indicates the operation is not supported
	ErrOperationNotSupported = errors.New("operation not supported")

	// ErrInvalidArgument indicates an invalid argument was provided
	ErrInvalidArgument = errors.New("invalid argument")
)

// ProtocolError represents a protocol-level error returned by the FastDFS server.
// It includes the error code from the protocol header and a descriptive message.
// Protocol errors indicate issues with the request format or server-side problems.
type ProtocolError struct {
	Code    byte   // Error code from the protocol status field
	Message string // Human-readable error description
}

// Error implements the error interface for ProtocolError.
func (e *ProtocolError) Error() string {
	return fmt.Sprintf("protocol error (code %d): %s", e.Code, e.Message)
}

// NetworkError represents a network-related error during communication.
// It wraps the underlying network error with context about the operation and server.
// Network errors typically indicate connectivity issues or timeouts.
type NetworkError struct {
	Op   string // Operation being performed ("dial", "read", "write")
	Addr string // Server address where the error occurred
	Err  error  // Underlying network error
}

// Error implements the error interface for NetworkError.
func (e *NetworkError) Error() string {
	return fmt.Sprintf("network error during %s to %s: %v", e.Op, e.Addr, e.Err)
}

// Unwrap returns the underlying error for error chain unwrapping.
func (e *NetworkError) Unwrap() error {
	return e.Err
}

// StorageError represents an error from a storage server.
// It wraps the underlying error with the storage server address for context.
type StorageError struct {
	Server string
	Err    error
}

// Error implements the error interface for StorageError.
func (e *StorageError) Error() string {
	return fmt.Sprintf("storage error from %s: %v", e.Server, e.Err)
}

// Unwrap returns the underlying error for error chain unwrapping.
func (e *StorageError) Unwrap() error {
	return e.Err
}

// TrackerError represents an error from a tracker server.
// It wraps the underlying error with the tracker server address for context.
type TrackerError struct {
	Server string // Tracker server address
	Err    error  // Underlying error
}

// Error implements the error interface for TrackerError.
func (e *TrackerError) Error() string {
	return fmt.Sprintf("tracker error from %s: %v", e.Server, e.Err)
}

// Unwrap returns the underlying error for error chain unwrapping.
func (e *TrackerError) Unwrap() error {
	return e.Err
}

// mapStatusToError maps FastDFS protocol status codes to Go errors.
// Status code 0 indicates success (no error).
// Other status codes are mapped to predefined errors or a ProtocolError.
//
// Common status codes:
//   - 0: Success
//   - 2: File not found (ENOENT)
//   - 6: File already exists (EEXIST)
//   - 22: Invalid argument (EINVAL)
//   - 28: Insufficient space (ENOSPC)
//
// Parameters:
//   - status: the status byte from the protocol header
//
// Returns the corresponding error, or nil for success.
func mapStatusToError(status byte) error {
	switch status {
	case 0:
		return nil
	case 2:
		return ErrFileNotFound
	case 6:
		return ErrFileAlreadyExists
	case 22:
		return ErrInvalidArgument
	case 28:
		return ErrInsufficientSpace
	default:
		return &ProtocolError{
			Code:    status,
			Message: fmt.Sprintf("unknown error code: %d", status),
		}
	}
}
