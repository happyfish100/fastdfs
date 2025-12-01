package org.fastdfs.client.exception;

/**
 * Exception thrown when file ID format is invalid.
 */
public class InvalidFileIdException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public InvalidFileIdException() {
        super("Invalid file ID");
    }

    public InvalidFileIdException(String fileId) {
        super("Invalid file ID: " + fileId);
    }
}
