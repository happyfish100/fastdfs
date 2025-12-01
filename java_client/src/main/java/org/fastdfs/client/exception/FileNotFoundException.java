package org.fastdfs.client.exception;

/**
 * Exception thrown when a requested file does not exist.
 */
public class FileNotFoundException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public FileNotFoundException() {
        super("File not found");
    }

    public FileNotFoundException(String fileId) {
        super("File not found: " + fileId);
    }
}
