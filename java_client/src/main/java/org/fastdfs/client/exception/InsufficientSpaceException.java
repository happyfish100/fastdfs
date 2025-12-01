package org.fastdfs.client.exception;

/**
 * Exception thrown when insufficient storage space is available.
 */
public class InsufficientSpaceException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public InsufficientSpaceException() {
        super("Insufficient storage space");
    }
}
