package org.fastdfs.client.exception;

/**
 * Base exception for all FastDFS errors.
 * 
 * This is the root exception class for all FastDFS-related errors.
 * All specific FastDFS exceptions extend from this class.
 */
public class FastDFSException extends Exception {
    
    private static final long serialVersionUID = 1L;

    public FastDFSException(String message) {
        super(message);
    }

    public FastDFSException(String message, Throwable cause) {
        super(message, cause);
    }

    public FastDFSException(Throwable cause) {
        super(cause);
    }
}
