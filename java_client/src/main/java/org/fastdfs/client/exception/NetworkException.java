package org.fastdfs.client.exception;

/**
 * Exception thrown for network-related errors during communication.
 */
public class NetworkException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;
    
    private final String operation;
    private final String addr;

    public NetworkException(String operation, String addr, Throwable cause) {
        super("Network error during " + operation + " to " + addr + ": " + cause.getMessage(), cause);
        this.operation = operation;
        this.addr = addr;
    }

    public String getOperation() {
        return operation;
    }

    public String getAddr() {
        return addr;
    }
}
