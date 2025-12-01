package org.fastdfs.client.exception;

/**
 * Exception thrown when attempting to use a closed client.
 */
public class ClientClosedException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public ClientClosedException() {
        super("Client is closed");
    }
}
