package org.fastdfs.client.exception;

/**
 * Exception thrown when connection timeout occurs.
 */
public class ConnectionTimeoutException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public ConnectionTimeoutException() {
        super("Connection timeout");
    }

    public ConnectionTimeoutException(String addr) {
        super("Connection timeout to " + addr);
    }
}
