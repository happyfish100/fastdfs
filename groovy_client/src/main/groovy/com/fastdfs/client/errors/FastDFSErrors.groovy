/**
 * FastDFS Error Definitions
 * 
 * This file defines all error types and error handling utilities for the FastDFS client.
 * Errors are categorized into common errors, protocol errors, network errors, and server errors.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.errors

/**
 * Base exception class for all FastDFS-related errors.
 * 
 * All exceptions thrown by the FastDFS client extend this class.
 * This allows for easy exception handling and error categorization.
 */
class FastDFSException extends RuntimeException {
    
    /**
     * Default constructor.
     */
    FastDFSException() {
        super()
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    FastDFSException(String message) {
        super(message)
    }
    
    /**
     * Constructor with message and cause.
     * 
     * @param message the error message
     * @param cause the underlying exception
     */
    FastDFSException(String message, Throwable cause) {
        super(message, cause)
    }
    
    /**
     * Constructor with cause.
     * 
     * @param cause the underlying exception
     */
    FastDFSException(Throwable cause) {
        super(cause)
    }
}

/**
 * Exception thrown when the client has been closed.
 * 
 * This exception is thrown when attempting to use a client that has already
 * been closed. Once closed, a client cannot be used for further operations.
 */
class ClientClosedException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    ClientClosedException() {
        super("Client has been closed")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    ClientClosedException(String message) {
        super(message)
    }
}

/**
 * Exception thrown when a file is not found.
 * 
 * This exception is thrown when attempting to access a file that does not
 * exist on the storage server.
 */
class FileNotFoundException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    FileNotFoundException() {
        super("File not found")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    FileNotFoundException(String message) {
        super(message)
    }
    
    /**
     * Constructor with file ID.
     * 
     * @param fileId the file ID that was not found
     */
    FileNotFoundException(String fileId) {
        super("File not found: ${fileId}")
    }
}

/**
 * Exception thrown when no storage server is available.
 * 
 * This exception is thrown when the tracker cannot provide a storage server
 * for the requested operation. This may happen if all storage servers are
 * offline or if there are no storage servers in the cluster.
 */
class NoStorageServerException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    NoStorageServerException() {
        super("No storage server available")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    NoStorageServerException(String message) {
        super(message)
    }
}

/**
 * Exception thrown when a connection timeout occurs.
 * 
 * This exception is thrown when establishing a connection to a server
 * takes longer than the configured connection timeout.
 */
class ConnectionTimeoutException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    ConnectionTimeoutException() {
        super("Connection timeout")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    ConnectionTimeoutException(String message) {
        super(message)
    }
    
    /**
     * Constructor with server address.
     * 
     * @param address the server address that timed out
     */
    ConnectionTimeoutException(String address) {
        super("Connection timeout to ${address}")
    }
}

/**
 * Exception thrown when a network I/O timeout occurs.
 * 
 * This exception is thrown when a network read or write operation
 * takes longer than the configured network timeout.
 */
class NetworkTimeoutException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    NetworkTimeoutException() {
        super("Network timeout")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    NetworkTimeoutException(String message) {
        super(message)
    }
    
    /**
     * Constructor with operation and address.
     * 
     * @param operation the operation that timed out (e.g., "read", "write")
     * @param address the server address
     */
    NetworkTimeoutException(String operation, String address) {
        super("Network timeout during ${operation} to ${address}")
    }
}

/**
 * Exception thrown when a file ID format is invalid.
 * 
 * This exception is thrown when a file ID does not match the expected
 * format (group_name/remote_filename).
 */
class InvalidFileIdException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    InvalidFileIdException() {
        super("Invalid file ID")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    InvalidFileIdException(String message) {
        super(message)
    }
    
    /**
     * Constructor with file ID.
     * 
     * @param fileId the invalid file ID
     */
    InvalidFileIdException(String fileId) {
        super("Invalid file ID format: ${fileId}")
    }
}

/**
 * Exception thrown when a server response is invalid.
 * 
 * This exception is thrown when the server response does not match
 * the expected protocol format or contains invalid data.
 */
class InvalidResponseException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    InvalidResponseException() {
        super("Invalid response from server")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    InvalidResponseException(String message) {
        super(message)
    }
}

/**
 * Exception thrown when a storage server is offline.
 * 
 * This exception is thrown when attempting to communicate with a storage
 * server that is currently offline or unavailable.
 */
class StorageServerOfflineException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    StorageServerOfflineException() {
        super("Storage server is offline")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    StorageServerOfflineException(String message) {
        super(message)
    }
    
    /**
     * Constructor with server address.
     * 
     * @param address the offline server address
     */
    StorageServerOfflineException(String address) {
        super("Storage server is offline: ${address}")
    }
}

/**
 * Exception thrown when a tracker server is offline.
 * 
 * This exception is thrown when attempting to communicate with a tracker
 * server that is currently offline or unavailable.
 */
class TrackerServerOfflineException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    TrackerServerOfflineException() {
        super("Tracker server is offline")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    TrackerServerOfflineException(String message) {
        super(message)
    }
    
    /**
     * Constructor with server address.
     * 
     * @param address the offline server address
     */
    TrackerServerOfflineException(String address) {
        super("Tracker server is offline: ${address}")
    }
}

/**
 * Exception thrown when there is insufficient storage space.
 * 
 * This exception is thrown when attempting to upload a file but the
 * storage server does not have enough free space.
 */
class InsufficientSpaceException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    InsufficientSpaceException() {
        super("Insufficient storage space")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    InsufficientSpaceException(String message) {
        super(message)
    }
}

/**
 * Exception thrown when a file already exists.
 * 
 * This exception is thrown when attempting to create a file that
 * already exists (in operations that don't allow overwriting).
 */
class FileAlreadyExistsException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    FileAlreadyExistsException() {
        super("File already exists")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    FileAlreadyExistsException(String message) {
        super(message)
    }
    
    /**
     * Constructor with file ID.
     * 
     * @param fileId the file ID that already exists
     */
    FileAlreadyExistsException(String fileId) {
        super("File already exists: ${fileId}")
    }
}

/**
 * Exception thrown when metadata format is invalid.
 * 
 * This exception is thrown when metadata key-value pairs do not
 * conform to the FastDFS protocol requirements (e.g., key or value
 * exceeds maximum length).
 */
class InvalidMetadataException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    InvalidMetadataException() {
        super("Invalid metadata")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    InvalidMetadataException(String message) {
        super(message)
    }
}

/**
 * Exception thrown when an operation is not supported.
 * 
 * This exception is thrown when attempting to perform an operation
 * that is not supported by the FastDFS server or client.
 */
class OperationNotSupportedException extends FastDFSException {
    
    /**
     * Default constructor.
     */
    OperationNotSupportedException() {
        super("Operation not supported")
    }
    
    /**
     * Constructor with message.
     * 
     * @param message the error message
     */
    OperationNotSupportedException(String message) {
        super(message)
    }
    
    /**
     * Constructor with operation name.
     * 
     * @param operation the unsupported operation name
     */
    OperationNotSupportedException(String operation) {
        super("Operation not supported: ${operation}")
    }
}

/**
 * Protocol-level error from FastDFS server.
 * 
 * This exception represents an error returned by the FastDFS server
 * in the protocol response. It includes the error code from the
 * protocol header and a descriptive message.
 */
class ProtocolError extends FastDFSException {
    
    /**
     * Error code from the protocol status field.
     * 
     * Status code 0 indicates success, non-zero indicates an error.
     */
    final byte code
    
    /**
     * Constructor with code and message.
     * 
     * @param code the error code
     * @param message the error message
     */
    ProtocolError(byte code, String message) {
        super("Protocol error (code ${code}): ${message}")
        this.code = code
    }
    
    /**
     * Gets the error code.
     * 
     * @return the error code
     */
    byte getCode() {
        return code
    }
}

/**
 * Network-related error during communication.
 * 
 * This exception wraps underlying network errors with context about
 * the operation and server. Network errors typically indicate
 * connectivity issues or timeouts.
 */
class NetworkError extends FastDFSException {
    
    /**
     * Operation being performed when the error occurred.
     * 
     * Examples: "dial", "read", "write", "connect"
     */
    final String operation
    
    /**
     * Server address where the error occurred.
     */
    final String address
    
    /**
     * Constructor with operation, address, and cause.
     * 
     * @param operation the operation
     * @param address the server address
     * @param cause the underlying network error
     */
    NetworkError(String operation, String address, Throwable cause) {
        super("Network error during ${operation} to ${address}: ${cause.message}", cause)
        this.operation = operation
        this.address = address
    }
    
    /**
     * Gets the operation.
     * 
     * @return the operation
     */
    String getOperation() {
        return operation
    }
    
    /**
     * Gets the server address.
     * 
     * @return the server address
     */
    String getAddress() {
        return address
    }
}

/**
 * Error from a storage server.
 * 
 * This exception wraps errors that occur when communicating with
 * storage servers, providing context about which server failed.
 */
class StorageError extends FastDFSException {
    
    /**
     * Storage server address.
     */
    final String server
    
    /**
     * Constructor with server and cause.
     * 
     * @param server the server address
     * @param cause the underlying error
     */
    StorageError(String server, Throwable cause) {
        super("Storage error from ${server}: ${cause.message}", cause)
        this.server = server
    }
    
    /**
     * Gets the server address.
     * 
     * @return the server address
     */
    String getServer() {
        return server
    }
}

/**
 * Error from a tracker server.
 * 
 * This exception wraps errors that occur when communicating with
 * tracker servers, providing context about which server failed.
 */
class TrackerError extends FastDFSException {
    
    /**
     * Tracker server address.
     */
    final String server
    
    /**
     * Constructor with server and cause.
     * 
     * @param server the server address
     * @param cause the underlying error
     */
    TrackerError(String server, Throwable cause) {
        super("Tracker error from ${server}: ${cause.message}", cause)
        this.server = server
    }
    
    /**
     * Gets the server address.
     * 
     * @return the server address
     */
    String getServer() {
        return server
    }
}

/**
 * Error mapping utility.
 * 
 * Maps FastDFS protocol status codes to appropriate exception types.
 * Status code 0 indicates success (no error).
 * Other status codes are mapped to predefined exceptions or ProtocolError.
 */
class ErrorMapper {
    
    /**
     * Maps a protocol status code to an exception.
     * 
     * Common status codes:
     *   - 0: Success (returns null)
     *   - 2: File not found (ENOENT)
     *   - 6: File already exists (EEXIST)
     *   - 22: Invalid argument (EINVAL)
     *   - 28: Insufficient space (ENOSPC)
     * 
     * @param status the status byte from the protocol header
     * @return the corresponding exception, or null for success
     */
    static FastDFSException mapStatusToError(byte status) {
        switch (status) {
            case 0:
                // Success - no error
                return null
                
            case 2:
                // File not found
                return new FileNotFoundException()
                
            case 6:
                // File already exists
                return new FileAlreadyExistsException()
                
            case 22:
                // Invalid argument
                return new FastDFSException("Invalid argument")
                
            case 28:
                // Insufficient space
                return new InsufficientSpaceException()
                
            default:
                // Unknown error code - return generic protocol error
                return new ProtocolError(status, "Unknown error code: ${status}")
        }
    }
    
    /**
     * Maps a protocol status code to an exception with context.
     * 
     * @param status the status byte
     * @param context additional context information
     * @return the corresponding exception, or null for success
     */
    static FastDFSException mapStatusToError(byte status, String context) {
        FastDFSException error = mapStatusToError(status)
        
        if (error != null && context != null) {
            // Add context to the error message if possible
            return new FastDFSException("${error.message} (${context})", error)
        }
        
        return error
    }
}

