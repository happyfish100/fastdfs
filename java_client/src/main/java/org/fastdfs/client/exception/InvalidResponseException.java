package org.fastdfs.client.exception;

/**
 * Exception thrown when server response is invalid.
 */
public class InvalidResponseException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public InvalidResponseException() {
        super("Invalid response from server");
    }

    public InvalidResponseException(String details) {
        super("Invalid response from server: " + details);
    }
}
