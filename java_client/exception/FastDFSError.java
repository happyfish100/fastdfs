/**
 * FastDFSError - Error Code Definitions
 * 
 * This enum defines all error codes that can be returned by the FastDFS client.
 * Error codes are categorized into common errors, protocol errors, network errors,
 * and server errors for easier error handling and debugging.
 * 
 * Copyright (C) 2025 FastDFS Java Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * 
 * @author FastDFS Java Client Contributors
 * @version 1.0.0
 * @since 1.0.0
 */
package com.fastdfs.client.exception;

/**
 * FastDFSError defines all error codes for the FastDFS client.
 * 
 * This enum provides a comprehensive set of error codes that cover all
 * possible failure scenarios when interacting with FastDFS servers. Each
 * error code has a descriptive message and can be used to provide
 * meaningful error information to application code.
 * 
 * Error codes are organized into categories:
 * - Client errors: issues with the client itself (closed, invalid config)
 * - File errors: issues with file operations (not found, invalid ID)
 * - Network errors: connectivity and timeout issues
 * - Server errors: issues reported by FastDFS servers
 * - Protocol errors: issues with protocol encoding/decoding
 * 
 * Example usage:
 * <pre>
 * try {
 *     client.uploadFile("test.jpg", null);
 * } catch (FastDFSException e) {
 *     if (e.getError() == FastDFSError.FILE_NOT_FOUND) {
 *         // Handle file not found
 *     } else if (e.getError() == FastDFSError.NETWORK_TIMEOUT) {
 *         // Handle timeout
 *     }
 * }
 * </pre>
 */
public enum FastDFSError {
    
    // ============================================================================
    // Client Errors
    // ============================================================================
    
    /**
     * Client has been closed.
     * 
     * This error is returned when an operation is attempted on a client
     * that has already been closed. Once a client is closed, all operations
     * will fail with this error code.
     */
    CLIENT_CLOSED("Client has been closed"),
    
    /**
     * Invalid client configuration.
     * 
     * This error indicates that the client configuration is invalid, such as
     * missing tracker servers, invalid timeout values, or other configuration
     * problems.
     */
    INVALID_CONFIG("Invalid client configuration"),
    
    /**
     * Client initialization failed.
     * 
     * This error indicates that the client failed to initialize properly,
     * typically due to connection pool creation failures or other setup issues.
     */
    INITIALIZATION_FAILED("Client initialization failed"),
    
    /**
     * Failed to close client.
     * 
     * This error is returned when closing the client fails, typically due
     * to errors closing connection pools.
     */
    CLOSE_FAILED("Failed to close client"),
    
    // ============================================================================
    // File Errors
    // ============================================================================
    
    /**
     * File not found.
     * 
     * This error is returned when attempting to download, delete, or query
     * a file that does not exist on the storage server.
     */
    FILE_NOT_FOUND("File not found"),
    
    /**
     * Invalid file ID format.
     * 
     * This error indicates that the provided file ID does not match the
     * expected format (groupName/remoteFilename).
     */
    INVALID_FILE_ID("Invalid file ID format"),
    
    /**
     * File already exists.
     * 
     * This error is returned when attempting to upload a file that already
     * exists (though this is rare in FastDFS as files are typically assigned
     * unique IDs automatically).
     */
    FILE_ALREADY_EXISTS("File already exists"),
    
    /**
     * Invalid metadata format.
     * 
     * This error indicates that the metadata provided is invalid, such as
     * keys or values that exceed the maximum length limits.
     */
    INVALID_METADATA("Invalid metadata format"),
    
    // ============================================================================
    // Network Errors
    // ============================================================================
    
    /**
     * Connection timeout.
     * 
     * This error is returned when establishing a connection to a server
     * exceeds the configured connection timeout.
     */
    CONNECTION_TIMEOUT("Connection timeout"),
    
    /**
     * Network I/O timeout.
     * 
     * This error is returned when a network read or write operation exceeds
     * the configured network timeout.
     */
    NETWORK_TIMEOUT("Network I/O timeout"),
    
    /**
     * Connection failed.
     * 
     * This error indicates that a connection to a server could not be
     * established, typically due to network issues or server unavailability.
     */
    CONNECTION_FAILED("Connection failed"),
    
    /**
     * Network I/O error.
     * 
     * This error indicates a general network I/O error occurred during
     * communication with a server.
     */
    NETWORK_ERROR("Network I/O error"),
    
    // ============================================================================
    // Server Errors
    // ============================================================================
    
    /**
     * No storage server available.
     * 
     * This error is returned when the tracker cannot provide a storage server
     * for the requested operation, typically because all servers are offline
     * or no servers are configured in the group.
     */
    NO_STORAGE_SERVER("No storage server available"),
    
    /**
     * Storage server offline.
     * 
     * This error indicates that the storage server is offline and cannot
     * accept requests.
     */
    STORAGE_SERVER_OFFLINE("Storage server is offline"),
    
    /**
     * Tracker server offline.
     * 
     * This error indicates that the tracker server is offline and cannot
     * accept requests.
     */
    TRACKER_SERVER_OFFLINE("Tracker server is offline"),
    
    /**
     * Insufficient storage space.
     * 
     * This error is returned when the storage server does not have enough
     * free space to store the file.
     */
    INSUFFICIENT_SPACE("Insufficient storage space"),
    
    /**
     * Invalid server response.
     * 
     * This error indicates that the server returned a response that does
     * not match the expected protocol format or is otherwise invalid.
     */
    INVALID_RESPONSE("Invalid server response"),
    
    // ============================================================================
    // Protocol Errors
    // ============================================================================
    
    /**
     * Protocol encoding error.
     * 
     * This error indicates that encoding a request message failed, typically
     * due to invalid data or encoding logic errors.
     */
    PROTOCOL_ENCODING_ERROR("Protocol encoding error"),
    
    /**
     * Protocol decoding error.
     * 
     * This error indicates that decoding a response message failed, typically
     * due to invalid response format or decoding logic errors.
     */
    PROTOCOL_DECODING_ERROR("Protocol decoding error"),
    
    /**
     * Invalid protocol command.
     * 
     * This error indicates that an invalid protocol command was used or
     * received.
     */
    INVALID_PROTOCOL_COMMAND("Invalid protocol command"),
    
    /**
     * Protocol version mismatch.
     * 
     * This error indicates that the client and server are using incompatible
     * protocol versions.
     */
    PROTOCOL_VERSION_MISMATCH("Protocol version mismatch"),
    
    // ============================================================================
    // Operation Errors
    // ============================================================================
    
    /**
     * Operation not supported.
     * 
     * This error indicates that the requested operation is not supported
     * by the server or client.
     */
    OPERATION_NOT_SUPPORTED("Operation not supported"),
    
    /**
     * Invalid argument.
     * 
     * This error indicates that one or more arguments to an operation are
     * invalid, such as null values, negative offsets, or out-of-range values.
     */
    INVALID_ARGUMENT("Invalid argument"),
    
    /**
     * Operation failed.
     * 
     * This is a generic error for operations that fail for reasons not
     * covered by more specific error codes.
     */
    OPERATION_FAILED("Operation failed"),
    
    /**
     * Retry limit exceeded.
     * 
     * This error is returned when an operation has been retried the maximum
     * number of times without success.
     */
    RETRY_LIMIT_EXCEEDED("Retry limit exceeded"),
    
    /**
     * Unknown error.
     * 
     * This error is used for unexpected or unclassified errors that don't
     * fit into any other category.
     */
    UNKNOWN_ERROR("Unknown error");
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * Human-readable error message.
     * 
     * This message provides a descriptive explanation of the error that
     * can be displayed to users or logged for debugging purposes.
     */
    private final String message;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new FastDFSError enum value with the specified message.
     * 
     * @param message the human-readable error message
     */
    FastDFSError(String message) {
        this.message = message;
    }
    
    // ============================================================================
    // Public Methods
    // ============================================================================
    
    /**
     * Gets the human-readable error message.
     * 
     * @return the error message
     */
    public String getMessage() {
        return message;
    }
    
    /**
     * Returns a string representation of this error code.
     * 
     * The string includes both the error code name and the message,
     * which is useful for logging and debugging.
     * 
     * @return a string representation
     */
    @Override
    public String toString() {
        return name() + ": " + message;
    }
}

