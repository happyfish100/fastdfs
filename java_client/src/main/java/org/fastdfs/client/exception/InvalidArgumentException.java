package org.fastdfs.client.exception;

/**
 * Exception thrown when invalid argument is provided.
 */
public class InvalidArgumentException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public InvalidArgumentException() {
        super("Invalid argument");
    }

    public InvalidArgumentException(String details) {
        super("Invalid argument: " + details);
    }
}
