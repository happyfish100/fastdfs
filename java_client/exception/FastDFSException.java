/**
 * FastDFSException - FastDFS Client Exception
 * 
 * This is the main exception class for all FastDFS client operations.
 * It wraps error codes, messages, and underlying exceptions to provide
 * comprehensive error information for debugging and error handling.
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
 * FastDFSException is the main exception class for FastDFS client operations.
 * 
 * This exception is thrown whenever an operation fails, providing detailed
 * error information including an error code, descriptive message, and optionally
 * an underlying cause exception.
 * 
 * The exception includes:
 * - Error code: A FastDFSError enum value that categorizes the error
 * - Message: A human-readable description of what went wrong
 * - Cause: An optional underlying exception that caused this error
 * 
 * Example usage:
 * <pre>
 * try {
 *     String fileId = client.uploadFile("test.jpg", null);
 * } catch (FastDFSException e) {
 *     FastDFSError error = e.getError();
 *     String message = e.getMessage();
 *     Throwable cause = e.getCause();
 *     
 *     // Handle error based on error code
 *     if (error == FastDFSError.FILE_NOT_FOUND) {
 *         // Handle file not found
 *     } else if (error == FastDFSError.NETWORK_TIMEOUT) {
 *         // Handle timeout - maybe retry
 *     }
 * }
 * </pre>
 */
public class FastDFSException extends Exception {
    
    // ============================================================================
    // Serialization
    // ============================================================================
    
    /**
     * Serial version UID for serialization compatibility.
     * 
     * This value is used by Java's serialization mechanism to ensure that
     * serialized exceptions can be deserialized correctly even if the class
     * structure changes slightly.
     */
    private static final long serialVersionUID = 1L;
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * Error code categorizing this exception.
     * 
     * The error code provides a programmatic way to identify the type of error
     * that occurred, which is useful for error handling logic and logging.
     */
    private final FastDFSError error;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new FastDFSException with the specified error code.
     * 
     * The message is taken from the error code's default message.
     * 
     * @param error the error code (must not be null)
     */
    public FastDFSException(FastDFSError error) {
        super(error != null ? error.getMessage() : "Unknown error");
        if (error == null) {
            throw new IllegalArgumentException("Error code cannot be null");
        }
        this.error = error;
    }
    
    /**
     * Creates a new FastDFSException with the specified error code and message.
     * 
     * @param error the error code (must not be null)
     * @param message the error message (can be null, will use error code message)
     */
    public FastDFSException(FastDFSError error, String message) {
        super(message != null ? message : (error != null ? error.getMessage() : "Unknown error"));
        if (error == null) {
            throw new IllegalArgumentException("Error code cannot be null");
        }
        this.error = error;
    }
    
    /**
     * Creates a new FastDFSException with the specified error code, message, and cause.
     * 
     * This constructor is useful when wrapping another exception that caused
     * the FastDFS error, such as IOException or SocketException.
     * 
     * @param error the error code (must not be null)
     * @param message the error message (can be null)
     * @param cause the underlying exception that caused this error (can be null)
     */
    public FastDFSException(FastDFSError error, String message, Throwable cause) {
        super(message != null ? message : (error != null ? error.getMessage() : "Unknown error"), cause);
        if (error == null) {
            throw new IllegalArgumentException("Error code cannot be null");
        }
        this.error = error;
    }
    
    /**
     * Creates a new FastDFSException with the specified error code and cause.
     * 
     * The message is taken from the error code's default message.
     * 
     * @param error the error code (must not be null)
     * @param cause the underlying exception that caused this error (can be null)
     */
    public FastDFSException(FastDFSError error, Throwable cause) {
        super(error != null ? error.getMessage() : "Unknown error", cause);
        if (error == null) {
            throw new IllegalArgumentException("Error code cannot be null");
        }
        this.error = error;
    }
    
    // ============================================================================
    // Public Methods
    // ============================================================================
    
    /**
     * Gets the error code for this exception.
     * 
     * The error code provides a programmatic way to identify the type of error,
     * which is useful for error handling logic.
     * 
     * @return the error code (never null)
     */
    public FastDFSError getError() {
        return error;
    }
    
    /**
     * Returns a detailed string representation of this exception.
     * 
     * The string includes the error code name, message, and if available,
     * information about the underlying cause.
     * 
     * @return a detailed string representation
     */
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(getClass().getName());
        sb.append(": [");
        sb.append(error.name());
        sb.append("] ");
        sb.append(getMessage());
        
        if (getCause() != null) {
            sb.append(" (caused by: ");
            sb.append(getCause().getClass().getName());
            sb.append(": ");
            sb.append(getCause().getMessage());
            sb.append(")");
        }
        
        return sb.toString();
    }
}

