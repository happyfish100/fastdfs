package org.fastdfs.client.exception;

/**
 * Exception thrown for protocol-level errors returned by FastDFS server.
 */
public class ProtocolException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;
    
    private final int code;

    public ProtocolException(int code, String message) {
        super(message != null ? message : "Unknown error code: " + code);
        this.code = code;
    }

    public int getCode() {
        return code;
    }
}
