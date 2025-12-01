package org.fastdfs.client.exception;

/**
 * Exception thrown when file already exists.
 */
public class FileAlreadyExistsException extends FastDFSException {
    
    private static final long serialVersionUID = 1L;

    public FileAlreadyExistsException() {
        super("File already exists");
    }

    public FileAlreadyExistsException(String fileId) {
        super("File already exists: " + fileId);
    }
}
