package org.fastdfs.client.exception;

/**
 * Exception thrown when network I/O timeout occurs.
 */
public class NetworkTimeoutException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public NetworkTimeoutException() {
        super("Network timeout");
    }

    public NetworkTimeoutException(String operation) {
        super("Network timeout during " + operation);
    }
}
