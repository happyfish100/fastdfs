/**
 * FastDFS Java Client - Main Client Class
 * 
 * This class provides the primary interface for interacting with FastDFS servers.
 * It manages connections to tracker and storage servers, handles file operations,
 * and provides thread-safe access to the FastDFS distributed file system.
 * 
 * Copyright (C) 2025 FastDFS Java Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * 
 * @author FastDFS Java Client Contributors
 * @version 1.0.0
 * @since 1.0.0
 */
package com.fastdfs.client;

import com.fastdfs.client.config.FastDFSConfig;
import com.fastdfs.client.connection.FastDFSConnection;
import com.fastdfs.client.connection.FastDFSConnectionPool;
import com.fastdfs.client.exception.FastDFSException;
import com.fastdfs.client.exception.FastDFSError;
import com.fastdfs.client.operation.FileOperations;
import com.fastdfs.client.operation.MetadataOperations;
import com.fastdfs.client.operation.AppenderOperations;
import com.fastdfs.client.types.FileInfo;
import com.fastdfs.client.types.StorageServer;
import com.fastdfs.client.types.MetadataFlag;

import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/**
 * FastDFSClient represents a client instance for interacting with FastDFS servers.
 * 
 * This class is the main entry point for all FastDFS operations. It manages
 * connection pools for both tracker and storage servers, provides thread-safe
 * operations, and handles automatic failover between servers.
 * 
 * The client should be created once and reused throughout the application lifecycle.
 * It is thread-safe and can be used concurrently by multiple threads.
 * 
 * Example usage:
 * <pre>
 * FastDFSConfig config = new FastDFSConfig.Builder()
 *     .addTrackerServer("192.168.1.100", 22122)
 *     .build();
 * 
 * FastDFSClient client = new FastDFSClient(config);
 * try {
 *     String fileId = client.uploadFile("test.jpg", null);
 *     // ... use the client
 * } finally {
 *     client.close();
 * }
 * </pre>
 */
public class FastDFSClient implements AutoCloseable {
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * Client configuration object containing all settings for tracker servers,
     * timeouts, connection pool sizes, and retry counts.
     * 
     * This configuration is immutable once the client is created and cannot
     * be changed during the client's lifetime.
     */
    private final FastDFSConfig config;
    
    /**
     * Connection pool for tracker servers.
     * 
     * This pool manages connections to all configured tracker servers and
     * provides automatic load balancing and failover capabilities.
     * Connections are reused to minimize overhead and improve performance.
     */
    private final FastDFSConnectionPool trackerPool;
    
    /**
     * Connection pool for storage servers.
     * 
     * This pool dynamically manages connections to storage servers discovered
     * at runtime. Storage servers are added to the pool as they are discovered
     * through tracker queries.
     */
    private final FastDFSConnectionPool storagePool;
    
    /**
     * Read-write lock for thread-safe access to the client state.
     * 
     * Read locks are used for normal operations, while write locks are used
     * only when closing the client. This allows concurrent read operations
     * while ensuring exclusive access during shutdown.
     */
    private final ReadWriteLock stateLock = new ReentrantReadWriteLock();
    
    /**
     * Flag indicating whether the client has been closed.
     * 
     * Once closed, all operations will fail with FastDFSError.CLIENT_CLOSED.
     * This flag is protected by the stateLock.
     */
    private volatile boolean closed = false;
    
    /**
     * File operations handler.
     * 
     * This object encapsulates all file-related operations including upload,
     * download, delete, and file information queries.
     */
    private final FileOperations fileOperations;
    
    /**
     * Metadata operations handler.
     * 
     * This object handles all metadata-related operations including setting,
     * getting, and merging file metadata.
     */
    private final MetadataOperations metadataOperations;
    
    /**
     * Appender file operations handler.
     * 
     * This object manages operations specific to appender files, which are
     * files that can be modified after creation (append, modify, truncate).
     */
    private final AppenderOperations appenderOperations;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new FastDFS client with the specified configuration.
     * 
     * The client will initialize connection pools for tracker and storage
     * servers based on the provided configuration. The pools start empty
     * and connections are created on-demand when operations are performed.
     * 
     * @param config the client configuration (must not be null)
     * @throws FastDFSException if the configuration is invalid or initialization fails
     * @throws IllegalArgumentException if config is null
     */
    public FastDFSClient(FastDFSConfig config) throws FastDFSException {
        // Validate configuration
        if (config == null) {
            throw new IllegalArgumentException("Configuration cannot be null");
        }
        
        // Validate tracker servers are provided
        if (config.getTrackerServers().isEmpty()) {
            throw new FastDFSException(
                FastDFSError.INVALID_CONFIG,
                "At least one tracker server must be configured"
            );
        }
        
        // Store configuration
        this.config = config;
        
        // Initialize connection pools
        try {
            // Create tracker connection pool
            // The pool will manage connections to all configured tracker servers
            this.trackerPool = new FastDFSConnectionPool(
                config.getTrackerServers(),
                config.getMaxConnectionsPerServer(),
                config.getConnectTimeout(),
                config.getIdleTimeout()
            );
            
            // Create storage connection pool
            // This pool starts empty and will be populated dynamically
            // as storage servers are discovered through tracker queries
            this.storagePool = new FastDFSConnectionPool(
                config.getMaxConnectionsPerServer(),
                config.getConnectTimeout(),
                config.getIdleTimeout()
            );
            
        } catch (Exception e) {
            // If initialization fails, ensure we clean up any partially created pools
            if (trackerPool != null) {
                try {
                    trackerPool.close();
                } catch (Exception ignored) {
                    // Ignore cleanup errors
                }
            }
            if (storagePool != null) {
                try {
                    storagePool.close();
                } catch (Exception ignored) {
                    // Ignore cleanup errors
                }
            }
            throw new FastDFSException(
                FastDFSError.INITIALIZATION_FAILED,
                "Failed to initialize FastDFS client: " + e.getMessage(),
                e
            );
        }
        
        // Initialize operation handlers
        // These handlers encapsulate the logic for different types of operations
        this.fileOperations = new FileOperations(this);
        this.metadataOperations = new MetadataOperations(this);
        this.appenderOperations = new AppenderOperations(this);
    }
    
    // ============================================================================
    // Public API - File Upload Operations
    // ============================================================================
    
    /**
     * Uploads a file from the local filesystem to FastDFS.
     * 
     * This method reads the file from disk, uploads it to a storage server
     * selected by the tracker, and returns the file ID that can be used to
     * download or delete the file later.
     * 
     * The file extension is automatically extracted from the filename and
     * used to determine the file type. The file will be stored in a storage
     * group selected by the tracker based on load balancing and availability.
     * 
     * @param localFilePath the path to the local file to upload (must not be null)
     * @param metadata optional metadata key-value pairs (can be null or empty)
     * @return the file ID in format "groupName/remoteFilename"
     * @throws FastDFSException if the upload fails
     * @throws IllegalArgumentException if localFilePath is null or empty
     * @throws IOException if the file cannot be read
     */
    public String uploadFile(String localFilePath, Map<String, String> metadata) 
            throws FastDFSException, IOException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (localFilePath == null || localFilePath.trim().isEmpty()) {
            throw new IllegalArgumentException("Local file path cannot be null or empty");
        }
        
        // Delegate to file operations handler
        // The handler will perform retry logic and error handling
        return fileOperations.uploadFile(localFilePath, metadata);
    }
    
    /**
     * Uploads file data from a byte array to FastDFS.
     * 
     * This method uploads the provided byte array directly to FastDFS without
     * reading from disk. This is useful when you already have the file data
     * in memory, such as when processing uploaded files in a web application.
     * 
     * The file extension must be provided explicitly since there is no filename
     * to extract it from. The extension should not include the leading dot
     * (e.g., "jpg" not ".jpg").
     * 
     * @param data the file content as a byte array (must not be null)
     * @param fileExtName the file extension without dot (e.g., "jpg", "txt")
     * @param metadata optional metadata key-value pairs (can be null or empty)
     * @return the file ID in format "groupName/remoteFilename"
     * @throws FastDFSException if the upload fails
     * @throws IllegalArgumentException if data is null or fileExtName is invalid
     */
    public String uploadBuffer(byte[] data, String fileExtName, Map<String, String> metadata) 
            throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        if (fileExtName == null || fileExtName.trim().isEmpty()) {
            throw new IllegalArgumentException("File extension cannot be null or empty");
        }
        
        // Delegate to file operations handler
        return fileOperations.uploadBuffer(data, fileExtName, metadata);
    }
    
    /**
     * Uploads an appender file from the local filesystem.
     * 
     * Appender files are special files that can be modified after creation.
     * They support append, modify, and truncate operations. This is useful
     * for files that are written incrementally or need to be updated.
     * 
     * @param localFilePath the path to the local file to upload
     * @param metadata optional metadata key-value pairs
     * @return the file ID of the uploaded appender file
     * @throws FastDFSException if the upload fails
     * @throws IOException if the file cannot be read
     */
    public String uploadAppenderFile(String localFilePath, Map<String, String> metadata) 
            throws FastDFSException, IOException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (localFilePath == null || localFilePath.trim().isEmpty()) {
            throw new IllegalArgumentException("Local file path cannot be null or empty");
        }
        
        // Delegate to appender operations handler
        return appenderOperations.uploadAppenderFile(localFilePath, metadata);
    }
    
    /**
     * Uploads an appender file from a byte array.
     * 
     * @param data the file content as a byte array
     * @param fileExtName the file extension without dot
     * @param metadata optional metadata key-value pairs
     * @return the file ID of the uploaded appender file
     * @throws FastDFSException if the upload fails
     */
    public String uploadAppenderBuffer(byte[] data, String fileExtName, Map<String, String> metadata) 
            throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        if (fileExtName == null || fileExtName.trim().isEmpty()) {
            throw new IllegalArgumentException("File extension cannot be null or empty");
        }
        
        // Delegate to appender operations handler
        return appenderOperations.uploadAppenderBuffer(data, fileExtName, metadata);
    }
    
    /**
     * Uploads a slave file associated with a master file.
     * 
     * Slave files are typically used for thumbnails, previews, or other
     * derived versions of a master file. They are stored in the same
     * storage group as the master file and share the same base path.
     * 
     * @param masterFileId the file ID of the master file
     * @param prefixName the prefix for the slave file (e.g., "thumb", "small")
     * @param fileExtName the file extension without dot
     * @param data the file content as a byte array
     * @param metadata optional metadata key-value pairs
     * @return the file ID of the uploaded slave file
     * @throws FastDFSException if the upload fails
     */
    public String uploadSlaveFile(String masterFileId, String prefixName, 
                                   String fileExtName, byte[] data, 
                                   Map<String, String> metadata) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (masterFileId == null || masterFileId.trim().isEmpty()) {
            throw new IllegalArgumentException("Master file ID cannot be null or empty");
        }
        if (prefixName == null || prefixName.trim().isEmpty()) {
            throw new IllegalArgumentException("Prefix name cannot be null or empty");
        }
        if (fileExtName == null || fileExtName.trim().isEmpty()) {
            throw new IllegalArgumentException("File extension cannot be null or empty");
        }
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        
        // Delegate to file operations handler
        return fileOperations.uploadSlaveFile(masterFileId, prefixName, fileExtName, data, metadata);
    }
    
    // ============================================================================
    // Public API - File Download Operations
    // ============================================================================
    
    /**
     * Downloads a file from FastDFS and returns its content as a byte array.
     * 
     * This method downloads the entire file into memory. For large files,
     * consider using downloadToFile or downloadFileRange to avoid memory issues.
     * 
     * @param fileId the file ID to download (format: "groupName/remoteFilename")
     * @return the file content as a byte array
     * @throws FastDFSException if the download fails or file is not found
     */
    public byte[] downloadFile(String fileId) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        
        // Delegate to file operations handler
        return fileOperations.downloadFile(fileId);
    }
    
    /**
     * Downloads a specific range of bytes from a file.
     * 
     * This method is useful for streaming large files or implementing
     * HTTP range requests. The range is specified by offset and length.
     * 
     * @param fileId the file ID to download
     * @param offset the starting byte offset (0-based)
     * @param length the number of bytes to download (0 means to end of file)
     * @return the file content as a byte array
     * @throws FastDFSException if the download fails
     */
    public byte[] downloadFileRange(String fileId, long offset, long length) 
            throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        if (offset < 0) {
            throw new IllegalArgumentException("Offset cannot be negative");
        }
        if (length < 0) {
            throw new IllegalArgumentException("Length cannot be negative");
        }
        
        // Delegate to file operations handler
        return fileOperations.downloadFileRange(fileId, offset, length);
    }
    
    /**
     * Downloads a file from FastDFS and saves it to the local filesystem.
     * 
     * This method is more memory-efficient than downloadFile for large files
     * as it streams the data directly to disk without loading it all into memory.
     * 
     * @param fileId the file ID to download
     * @param localFilePath the path where the file should be saved
     * @throws FastDFSException if the download fails
     * @throws IOException if the file cannot be written
     */
    public void downloadToFile(String fileId, String localFilePath) 
            throws FastDFSException, IOException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        if (localFilePath == null || localFilePath.trim().isEmpty()) {
            throw new IllegalArgumentException("Local file path cannot be null or empty");
        }
        
        // Delegate to file operations handler
        fileOperations.downloadToFile(fileId, localFilePath);
    }
    
    // ============================================================================
    // Public API - File Deletion Operations
    // ============================================================================
    
    /**
     * Deletes a file from FastDFS.
     * 
     * This operation is permanent and cannot be undone. The file will be
     * removed from the storage server immediately.
     * 
     * @param fileId the file ID to delete
     * @throws FastDFSException if the deletion fails or file is not found
     */
    public void deleteFile(String fileId) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        
        // Delegate to file operations handler
        fileOperations.deleteFile(fileId);
    }
    
    // ============================================================================
    // Public API - Metadata Operations
    // ============================================================================
    
    /**
     * Sets metadata for a file.
     * 
     * Metadata is stored as key-value pairs associated with the file.
     * The flag parameter determines whether to overwrite existing metadata
     * or merge with it.
     * 
     * @param fileId the file ID
     * @param metadata the metadata key-value pairs
     * @param flag OVERWRITE to replace all metadata, MERGE to merge with existing
     * @throws FastDFSException if the operation fails
     */
    public void setMetadata(String fileId, Map<String, String> metadata, MetadataFlag flag) 
            throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        if (metadata == null || metadata.isEmpty()) {
            throw new IllegalArgumentException("Metadata cannot be null or empty");
        }
        if (flag == null) {
            throw new IllegalArgumentException("Metadata flag cannot be null");
        }
        
        // Delegate to metadata operations handler
        metadataOperations.setMetadata(fileId, metadata, flag);
    }
    
    /**
     * Retrieves metadata for a file.
     * 
     * @param fileId the file ID
     * @return a map containing the metadata key-value pairs (empty if no metadata)
     * @throws FastDFSException if the operation fails
     */
    public Map<String, String> getMetadata(String fileId) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        
        // Delegate to metadata operations handler
        return metadataOperations.getMetadata(fileId);
    }
    
    // ============================================================================
    // Public API - File Information Operations
    // ============================================================================
    
    /**
     * Retrieves detailed information about a file.
     * 
     * The information includes file size, creation time, CRC32 checksum,
     * and source storage server information.
     * 
     * @param fileId the file ID
     * @return FileInfo object containing file details
     * @throws FastDFSException if the operation fails or file is not found
     */
    public FileInfo getFileInfo(String fileId) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        
        // Delegate to file operations handler
        return fileOperations.getFileInfo(fileId);
    }
    
    /**
     * Checks if a file exists on the storage server.
     * 
     * This method queries the storage server for file information and returns
     * true if the file exists, false otherwise.
     * 
     * @param fileId the file ID to check
     * @return true if the file exists, false otherwise
     * @throws FastDFSException if the query fails (other than file not found)
     */
    public boolean fileExists(String fileId) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        
        // Try to get file info
        // If file not found exception is thrown, return false
        try {
            getFileInfo(fileId);
            return true;
        } catch (FastDFSException e) {
            if (e.getError() == FastDFSError.FILE_NOT_FOUND) {
                return false;
            }
            // Re-throw other exceptions
            throw e;
        }
    }
    
    // ============================================================================
    // Public API - Appender File Operations
    // ============================================================================
    
    /**
     * Appends data to an appender file.
     * 
     * The data will be appended to the end of the file. The file must have
     * been created as an appender file using uploadAppenderFile or uploadAppenderBuffer.
     * 
     * @param fileId the appender file ID
     * @param data the data to append
     * @throws FastDFSException if the operation fails
     */
    public void appendFile(String fileId, byte[] data) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        
        // Delegate to appender operations handler
        appenderOperations.appendFile(fileId, data);
    }
    
    /**
     * Modifies content of an appender file at a specific offset.
     * 
     * This operation overwrites existing data at the specified offset.
     * The file must be an appender file.
     * 
     * @param fileId the appender file ID
     * @param offset the byte offset where modification should start
     * @param data the new data to write
     * @throws FastDFSException if the operation fails
     */
    public void modifyFile(String fileId, long offset, byte[] data) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        if (offset < 0) {
            throw new IllegalArgumentException("Offset cannot be negative");
        }
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        
        // Delegate to appender operations handler
        appenderOperations.modifyFile(fileId, offset, data);
    }
    
    /**
     * Truncates an appender file to a specified size.
     * 
     * If the new size is larger than the current file size, the file will
     * be extended with zero bytes. If smaller, the file will be truncated.
     * 
     * @param fileId the appender file ID
     * @param size the new file size in bytes
     * @throws FastDFSException if the operation fails
     */
    public void truncateFile(String fileId, long size) throws FastDFSException {
        // Check if client is closed
        checkNotClosed();
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        if (size < 0) {
            throw new IllegalArgumentException("Size cannot be negative");
        }
        
        // Delegate to appender operations handler
        appenderOperations.truncateFile(fileId, size);
    }
    
    // ============================================================================
    // Public API - Lifecycle Management
    // ============================================================================
    
    /**
     * Closes the client and releases all resources.
     * 
     * This method closes all connection pools and releases network resources.
     * After closing, all operations will fail with FastDFSError.CLIENT_CLOSED.
     * 
     * It is safe to call this method multiple times. Subsequent calls will
     * have no effect.
     * 
     * This method implements AutoCloseable, so it can be used with try-with-resources:
     * <pre>
     * try (FastDFSClient client = new FastDFSClient(config)) {
     *     // use client
     * }
     * </pre>
     * 
     * @throws FastDFSException if closing connection pools fails
     */
    @Override
    public void close() throws FastDFSException {
        // Acquire write lock to ensure exclusive access during shutdown
        stateLock.writeLock().lock();
        try {
            // Check if already closed
            if (closed) {
                return;
            }
            
            // Mark as closed
            closed = true;
            
            // Close connection pools
            // We collect any errors but continue closing all pools
            Exception trackerError = null;
            Exception storageError = null;
            
            try {
                if (trackerPool != null) {
                    trackerPool.close();
                }
            } catch (Exception e) {
                trackerError = e;
            }
            
            try {
                if (storagePool != null) {
                    storagePool.close();
                }
            } catch (Exception e) {
                storageError = e;
            }
            
            // If any errors occurred, throw an exception
            if (trackerError != null || storageError != null) {
                StringBuilder message = new StringBuilder("Error closing client: ");
                if (trackerError != null) {
                    message.append("tracker pool: ").append(trackerError.getMessage());
                }
                if (storageError != null) {
                    if (trackerError != null) {
                        message.append(", ");
                    }
                    message.append("storage pool: ").append(storageError.getMessage());
                }
                throw new FastDFSException(
                    FastDFSError.CLOSE_FAILED,
                    message.toString(),
                    trackerError != null ? trackerError : storageError
                );
            }
            
        } finally {
            // Always release the lock
            stateLock.writeLock().unlock();
        }
    }
    
    // ============================================================================
    // Package-Private Methods (for internal use by operation handlers)
    // ============================================================================
    
    /**
     * Gets the client configuration.
     * 
     * This method is package-private and is used by operation handlers
     * to access configuration settings.
     * 
     * @return the client configuration
     */
    FastDFSConfig getConfig() {
        return config;
    }
    
    /**
     * Gets the tracker connection pool.
     * 
     * This method is package-private and is used by operation handlers
     * to obtain connections to tracker servers.
     * 
     * @return the tracker connection pool
     */
    FastDFSConnectionPool getTrackerPool() {
        return trackerPool;
    }
    
    /**
     * Gets the storage connection pool.
     * 
     * This method is package-private and is used by operation handlers
     * to obtain connections to storage servers.
     * 
     * @return the storage connection pool
     */
    FastDFSConnectionPool getStoragePool() {
        return storagePool;
    }
    
    // ============================================================================
    // Private Helper Methods
    // ============================================================================
    
    /**
     * Checks if the client is closed and throws an exception if it is.
     * 
     * This method is called at the beginning of all public operations
     * to ensure the client is still valid.
     * 
     * @throws FastDFSException with FastDFSError.CLIENT_CLOSED if the client is closed
     */
    private void checkNotClosed() throws FastDFSException {
        // Acquire read lock to check the closed flag
        stateLock.readLock().lock();
        try {
            if (closed) {
                throw new FastDFSException(
                    FastDFSError.CLIENT_CLOSED,
                    "Client has been closed"
                );
            }
        } finally {
            // Always release the lock
            stateLock.readLock().unlock();
        }
    }
}

