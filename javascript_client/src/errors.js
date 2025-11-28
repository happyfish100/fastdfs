/**
 * FastDFS Error Definitions
 * 
 * This module defines all error types and error handling utilities for the FastDFS client.
 * Errors are categorized into common errors, protocol errors, network errors, and server errors.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

'use strict';

/**
 * Base exception for all FastDFS errors
 * 
 * All FastDFS-specific errors extend from this base class,
 * making it easy to catch all FastDFS-related exceptions.
 */
class FastDFSError extends Error {
  constructor(message) {
    super(message);
    this.name = 'FastDFSError';
    Error.captureStackTrace(this, this.constructor);
  }
}

/**
 * Client has been closed
 * 
 * Thrown when attempting to perform operations on a closed client.
 * Once a client is closed, it cannot be reused.
 */
class ClientClosedError extends FastDFSError {
  constructor() {
    super('Client is closed');
    this.name = 'ClientClosedError';
  }
}

/**
 * Requested file does not exist
 * 
 * Thrown when a file operation references a file ID that doesn't exist
 * on the storage server.
 */
class FileNotFoundError extends FastDFSError {
  constructor(fileId) {
    super(fileId ? `File not found: ${fileId}` : 'File not found');
    this.name = 'FileNotFoundError';
    this.fileId = fileId;
  }
}

/**
 * No storage server is available
 * 
 * Thrown when the tracker server cannot provide a storage server
 * for the requested operation.
 */
class NoStorageServerError extends FastDFSError {
  constructor() {
    super('No storage server available');
    this.name = 'NoStorageServerError';
  }
}

/**
 * Connection timeout
 * 
 * Thrown when a connection attempt to a server times out.
 */
class ConnectionTimeoutError extends FastDFSError {
  constructor(addr) {
    super(addr ? `Connection timeout to ${addr}` : 'Connection timeout');
    this.name = 'ConnectionTimeoutError';
    this.addr = addr;
  }
}

/**
 * Network I/O timeout
 * 
 * Thrown when a network read or write operation times out.
 */
class NetworkTimeoutError extends FastDFSError {
  constructor(operation) {
    super(operation ? `Network timeout during ${operation}` : 'Network timeout');
    this.name = 'NetworkTimeoutError';
    this.operation = operation;
  }
}

/**
 * File ID format is invalid
 * 
 * Thrown when a file ID doesn't match the expected format (group/path).
 */
class InvalidFileIDError extends FastDFSError {
  constructor(fileId) {
    super(fileId ? `Invalid file ID: ${fileId}` : 'Invalid file ID');
    this.name = 'InvalidFileIDError';
    this.fileId = fileId;
  }
}

/**
 * Server response is invalid
 * 
 * Thrown when the server returns a response that doesn't match
 * the expected protocol format.
 */
class InvalidResponseError extends FastDFSError {
  constructor(details) {
    super(details ? `Invalid response from server: ${details}` : 'Invalid response from server');
    this.name = 'InvalidResponseError';
    this.details = details;
  }
}

/**
 * Storage server is offline
 * 
 * Thrown when attempting to connect to a storage server that is offline.
 */
class StorageServerOfflineError extends FastDFSError {
  constructor(addr) {
    super(addr ? `Storage server is offline: ${addr}` : 'Storage server is offline');
    this.name = 'StorageServerOfflineError';
    this.addr = addr;
  }
}

/**
 * Tracker server is offline
 * 
 * Thrown when attempting to connect to a tracker server that is offline.
 */
class TrackerServerOfflineError extends FastDFSError {
  constructor(addr) {
    super(addr ? `Tracker server is offline: ${addr}` : 'Tracker server is offline');
    this.name = 'TrackerServerOfflineError';
    this.addr = addr;
  }
}

/**
 * Insufficient storage space
 * 
 * Thrown when the storage server doesn't have enough space for the upload.
 */
class InsufficientSpaceError extends FastDFSError {
  constructor() {
    super('Insufficient storage space');
    this.name = 'InsufficientSpaceError';
  }
}

/**
 * File already exists
 * 
 * Thrown when attempting to create a file that already exists.
 */
class FileAlreadyExistsError extends FastDFSError {
  constructor(fileId) {
    super(fileId ? `File already exists: ${fileId}` : 'File already exists');
    this.name = 'FileAlreadyExistsError';
    this.fileId = fileId;
  }
}

/**
 * Invalid metadata format
 * 
 * Thrown when metadata doesn't meet the required format constraints.
 */
class InvalidMetadataError extends FastDFSError {
  constructor(details) {
    super(details ? `Invalid metadata: ${details}` : 'Invalid metadata');
    this.name = 'InvalidMetadataError';
    this.details = details;
  }
}

/**
 * Operation is not supported
 * 
 * Thrown when attempting an operation that is not supported by the server
 * or the file type (e.g., appending to a non-appender file).
 */
class OperationNotSupportedError extends FastDFSError {
  constructor(operation) {
    super(operation ? `Operation not supported: ${operation}` : 'Operation not supported');
    this.name = 'OperationNotSupportedError';
    this.operation = operation;
  }
}

/**
 * Invalid argument was provided
 * 
 * Thrown when a function is called with invalid arguments.
 */
class InvalidArgumentError extends FastDFSError {
  constructor(details) {
    super(details ? `Invalid argument: ${details}` : 'Invalid argument');
    this.name = 'InvalidArgumentError';
    this.details = details;
  }
}

/**
 * Protocol-level error returned by the FastDFS server
 * 
 * This error wraps status codes returned by the server that don't
 * map to specific error types.
 */
class ProtocolError extends FastDFSError {
  constructor(code, message) {
    super(message || `Unknown error code: ${code}`);
    this.name = 'ProtocolError';
    this.code = code;
  }
}

/**
 * Network-related error during communication
 * 
 * This error wraps low-level network errors that occur during
 * socket operations.
 */
class NetworkError extends FastDFSError {
  constructor(operation, addr, originalError) {
    super(`Network error during ${operation} to ${addr}: ${originalError.message}`);
    this.name = 'NetworkError';
    this.operation = operation;
    this.addr = addr;
    this.originalError = originalError;
  }
}

/**
 * Maps FastDFS protocol status codes to JavaScript errors
 * 
 * Status code 0 indicates success (no error).
 * Other status codes are mapped to predefined errors or a ProtocolError.
 * 
 * Common status codes:
 *   0: Success
 *   2: File not found (ENOENT)
 *   6: File already exists (EEXIST)
 *   22: Invalid argument (EINVAL)
 *   28: Insufficient space (ENOSPC)
 * 
 * @param {number} status - The status code from the server
 * @returns {Error|null} The corresponding error object, or null if status is 0
 */
function mapStatusToError(status) {
  switch (status) {
    case 0:
      return null;
    case 2:
      return new FileNotFoundError();
    case 6:
      return new FileAlreadyExistsError();
    case 22:
      return new InvalidArgumentError();
    case 28:
      return new InsufficientSpaceError();
    default:
      return new ProtocolError(status, `Unknown error code: ${status}`);
  }
}

// Export all error classes and utility functions
module.exports = {
  FastDFSError,
  ClientClosedError,
  FileNotFoundError,
  NoStorageServerError,
  ConnectionTimeoutError,
  NetworkTimeoutError,
  InvalidFileIDError,
  InvalidResponseError,
  StorageServerOfflineError,
  TrackerServerOfflineError,
  InsufficientSpaceError,
  FileAlreadyExistsError,
  InvalidMetadataError,
  OperationNotSupportedError,
  InvalidArgumentError,
  ProtocolError,
  NetworkError,
  mapStatusToError,
};
