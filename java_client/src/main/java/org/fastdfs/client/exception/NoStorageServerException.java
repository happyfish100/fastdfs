package org.fastdfs.client.exception;

/**
 * Exception thrown when no storage server is available.
 */
public class NoStorageServerException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public NoStorageServerException() {
        super("No storage server available");
    }
}
