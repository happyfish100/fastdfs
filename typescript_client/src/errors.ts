/**
 * FastDFS Error Definitions
 * 
 * This module defines all error types and error handling utilities for the FastDFS client.
 * Errors are categorized into common errors, protocol errors, network errors, and server errors.
 */

/**
 * Base exception for all FastDFS errors
 */
export class FastDFSError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'FastDFSError';
    Object.setPrototypeOf(this, FastDFSError.prototype);
  }
}

/**
 * Client has been closed
 */
export class ClientClosedError extends FastDFSError {
  constructor() {
    super('Client is closed');
    this.name = 'ClientClosedError';
    Object.setPrototypeOf(this, ClientClosedError.prototype);
  }
}

/**
 * Requested file does not exist
 */
export class FileNotFoundError extends FastDFSError {
  constructor(fileId?: string) {
    super(fileId ? `File not found: ${fileId}` : 'File not found');
    this.name = 'FileNotFoundError';
    Object.setPrototypeOf(this, FileNotFoundError.prototype);
  }
}

/**
 * No storage server is available
 */
export class NoStorageServerError extends FastDFSError {
  constructor() {
    super('No storage server available');
    this.name = 'NoStorageServerError';
    Object.setPrototypeOf(this, NoStorageServerError.prototype);
  }
}

/**
 * Connection timeout
 */
export class ConnectionTimeoutError extends FastDFSError {
  constructor(addr?: string) {
    super(addr ? `Connection timeout to ${addr}` : 'Connection timeout');
    this.name = 'ConnectionTimeoutError';
    Object.setPrototypeOf(this, ConnectionTimeoutError.prototype);
  }
}

/**
 * Network I/O timeout
 */
export class NetworkTimeoutError extends FastDFSError {
  constructor(operation?: string) {
    super(operation ? `Network timeout during ${operation}` : 'Network timeout');
    this.name = 'NetworkTimeoutError';
    Object.setPrototypeOf(this, NetworkTimeoutError.prototype);
  }
}

/**
 * File ID format is invalid
 */
export class InvalidFileIDError extends FastDFSError {
  constructor(fileId?: string) {
    super(fileId ? `Invalid file ID: ${fileId}` : 'Invalid file ID');
    this.name = 'InvalidFileIDError';
    Object.setPrototypeOf(this, InvalidFileIDError.prototype);
  }
}

/**
 * Server response is invalid
 */
export class InvalidResponseError extends FastDFSError {
  constructor(details?: string) {
    super(details ? `Invalid response from server: ${details}` : 'Invalid response from server');
    this.name = 'InvalidResponseError';
    Object.setPrototypeOf(this, InvalidResponseError.prototype);
  }
}

/**
 * Storage server is offline
 */
export class StorageServerOfflineError extends FastDFSError {
  constructor(addr?: string) {
    super(addr ? `Storage server is offline: ${addr}` : 'Storage server is offline');
    this.name = 'StorageServerOfflineError';
    Object.setPrototypeOf(this, StorageServerOfflineError.prototype);
  }
}

/**
 * Tracker server is offline
 */
export class TrackerServerOfflineError extends FastDFSError {
  constructor(addr?: string) {
    super(addr ? `Tracker server is offline: ${addr}` : 'Tracker server is offline');
    this.name = 'TrackerServerOfflineError';
    Object.setPrototypeOf(this, TrackerServerOfflineError.prototype);
  }
}

/**
 * Insufficient storage space
 */
export class InsufficientSpaceError extends FastDFSError {
  constructor() {
    super('Insufficient storage space');
    this.name = 'InsufficientSpaceError';
    Object.setPrototypeOf(this, InsufficientSpaceError.prototype);
  }
}

/**
 * File already exists
 */
export class FileAlreadyExistsError extends FastDFSError {
  constructor(fileId?: string) {
    super(fileId ? `File already exists: ${fileId}` : 'File already exists');
    this.name = 'FileAlreadyExistsError';
    Object.setPrototypeOf(this, FileAlreadyExistsError.prototype);
  }
}

/**
 * Invalid metadata format
 */
export class InvalidMetadataError extends FastDFSError {
  constructor(details?: string) {
    super(details ? `Invalid metadata: ${details}` : 'Invalid metadata');
    this.name = 'InvalidMetadataError';
    Object.setPrototypeOf(this, InvalidMetadataError.prototype);
  }
}

/**
 * Operation is not supported
 */
export class OperationNotSupportedError extends FastDFSError {
  constructor(operation?: string) {
    super(operation ? `Operation not supported: ${operation}` : 'Operation not supported');
    this.name = 'OperationNotSupportedError';
    Object.setPrototypeOf(this, OperationNotSupportedError.prototype);
  }
}

/**
 * Invalid argument was provided
 */
export class InvalidArgumentError extends FastDFSError {
  constructor(details?: string) {
    super(details ? `Invalid argument: ${details}` : 'Invalid argument');
    this.name = 'InvalidArgumentError';
    Object.setPrototypeOf(this, InvalidArgumentError.prototype);
  }
}

/**
 * Protocol-level error returned by the FastDFS server
 */
export class ProtocolError extends FastDFSError {
  public readonly code: number;

  constructor(code: number, message?: string) {
    super(message || `Unknown error code: ${code}`);
    this.name = 'ProtocolError';
    this.code = code;
    Object.setPrototypeOf(this, ProtocolError.prototype);
  }
}

/**
 * Network-related error during communication
 */
export class NetworkError extends FastDFSError {
  public readonly operation: string;
  public readonly addr: string;
  public readonly originalError: Error;

  constructor(operation: string, addr: string, originalError: Error) {
    super(`Network error during ${operation} to ${addr}: ${originalError.message}`);
    this.name = 'NetworkError';
    this.operation = operation;
    this.addr = addr;
    this.originalError = originalError;
    Object.setPrototypeOf(this, NetworkError.prototype);
  }
}

/**
 * Maps FastDFS protocol status codes to TypeScript errors
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
 */
export function mapStatusToError(status: number): Error | null {
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