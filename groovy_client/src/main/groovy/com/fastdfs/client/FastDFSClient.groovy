/**
 * FastDFS Groovy Client - Main Client Implementation
 * 
 * This is the primary client class for interacting with FastDFS distributed file system.
 * It provides a high-level, idiomatic Groovy API for file operations including upload,
 * download, deletion, and metadata management.
 * 
 * The client handles connection pooling, automatic retries, error handling, and
 * failover between tracker and storage servers automatically.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 * @since 2025
 * 
 * Copyright (C) 2025 FastDFS Groovy Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */
package com.fastdfs.client

import com.fastdfs.client.config.ClientConfig
import com.fastdfs.client.connection.ConnectionPool
import com.fastdfs.client.connection.Connection
import com.fastdfs.client.errors.*
import com.fastdfs.client.operations.*
import com.fastdfs.client.protocol.*
import com.fastdfs.client.types.*
import com.fastdfs.client.util.*
import groovy.transform.Synchronized
import java.util.concurrent.locks.ReentrantReadWriteLock
import java.util.concurrent.locks.Lock
import java.util.concurrent.locks.ReadLock
import java.util.concurrent.locks.WriteLock

/**
 * FastDFS client for distributed file operations.
 * 
 * This class provides a comprehensive interface for interacting with FastDFS servers.
 * It manages connections to both tracker and storage servers, handles retries,
 * implements connection pooling for performance, and provides thread-safe operations.
 * 
 * Example usage:
 * <pre>
 * def config = new ClientConfig(
 *     trackerAddrs: ['192.168.1.100:22122', '192.168.1.101:22122'],
 *     maxConns: 100,
 *     connectTimeout: 5000,
 *     networkTimeout: 30000
 * )
 * 
 * def client = new FastDFSClient(config)
 * 
 * try {
 *     // Upload a file
 *     def fileId = client.uploadFile('test.jpg', [:])
 *     
 *     // Download the file
 *     def data = client.downloadFile(fileId)
 *     
 *     // Delete the file
 *     client.deleteFile(fileId)
 * } finally {
 *     client.close()
 * }
 * </pre>
 */
class FastDFSClient {
    
    // ============================================================================
    // Private Fields - Internal State Management
    // ============================================================================
    
    /**
     * Client configuration object.
     * Contains all settings for tracker addresses, timeouts, connection limits, etc.
     * This is set during construction and should not be modified after initialization.
     */
    private final ClientConfig config
    
    /**
     * Connection pool for tracker servers.
     * Manages connections to tracker servers for querying storage server information.
     * Connections are pooled and reused for better performance.
     */
    private final ConnectionPool trackerPool
    
    /**
     * Connection pool for storage servers.
     * Manages connections to storage servers for file operations.
     * Connections are dynamically created based on tracker responses.
     */
    private final ConnectionPool storagePool
    
    /**
     * Read-write lock for thread-safe operations.
     * Used to synchronize access to the client state, particularly for checking
     * if the client is closed and for closing operations.
     */
    private final ReentrantReadWriteLock stateLock
    
    /**
     * Read lock for checking client state.
     * Multiple threads can acquire this lock simultaneously for read operations.
     */
    private final ReadLock readLock
    
    /**
     * Write lock for modifying client state.
     * Only one thread can acquire this lock at a time, ensuring exclusive access.
     */
    private final WriteLock writeLock
    
    /**
     * Flag indicating whether the client has been closed.
     * Once closed, the client cannot be used for further operations.
     * This is checked before every operation to prevent use-after-close errors.
     */
    private volatile boolean closed
    
    /**
     * Operations helper object.
     * Encapsulates the actual implementation of file operations like upload, download, etc.
     * This separation allows for better code organization and testing.
     */
    private final Operations operations
    
    /**
     * Protocol handler for encoding and decoding FastDFS protocol messages.
     * Handles the low-level protocol details like header encoding, metadata encoding, etc.
     */
    private final ProtocolHandler protocolHandler
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new FastDFS client with the specified configuration.
     * 
     * This constructor validates the configuration, initializes connection pools,
     * and prepares the client for use. If initialization fails, an exception is thrown.
     * 
     * @param config the client configuration (required, must not be null)
     * @throws IllegalArgumentException if the configuration is invalid
     * @throws FastDFSException if initialization fails (e.g., cannot connect to trackers)
     */
    FastDFSClient(ClientConfig config) {
        // Validate configuration first
        // This ensures we fail fast if the configuration is invalid
        if (config == null) {
            throw new IllegalArgumentException("Client configuration cannot be null")
        }
        
        // Validate tracker addresses
        // At least one tracker address must be provided
        if (config.trackerAddrs == null || config.trackerAddrs.isEmpty()) {
            throw new IllegalArgumentException("At least one tracker address is required")
        }
        
        // Validate each tracker address
        // Empty addresses are not allowed
        for (String addr : config.trackerAddrs) {
            if (addr == null || addr.trim().isEmpty()) {
                throw new IllegalArgumentException("Tracker address cannot be null or empty")
            }
        }
        
        // Store configuration
        // Make a defensive copy to prevent external modification
        this.config = new ClientConfig(config)
        
        // Initialize locks for thread safety
        // Read-write locks allow multiple concurrent reads but exclusive writes
        this.stateLock = new ReentrantReadWriteLock()
        this.readLock = stateLock.readLock()
        this.writeLock = stateLock.writeLock()
        
        // Initialize closed flag
        // Client starts in open state
        this.closed = false
        
        // Initialize protocol handler
        // This handles all protocol encoding/decoding
        this.protocolHandler = new ProtocolHandler()
        
        try {
            // Initialize tracker connection pool
            // This pool manages connections to tracker servers
            this.trackerPool = new ConnectionPool(
                config.trackerAddrs,
                config.maxConns ?: 10,
                config.connectTimeout ?: 5000,
                config.idleTimeout ?: 60000
            )
            
            // Initialize storage connection pool
            // This pool will be populated dynamically as storage servers are discovered
            this.storagePool = new ConnectionPool(
                [],
                config.maxConns ?: 10,
                config.connectTimeout ?: 5000,
                config.idleTimeout ?: 60000
            )
            
            // Initialize operations helper
            // This encapsulates the actual operation implementations
            this.operations = new Operations(
                this,
                trackerPool,
                storagePool,
                protocolHandler,
                config
            )
            
        } catch (Exception e) {
            // Clean up on initialization failure
            // Close any pools that were successfully created
            if (trackerPool != null) {
                try {
                    trackerPool.close()
                } catch (Exception ignored) {
                    // Ignore cleanup errors
                }
            }
            
            if (storagePool != null) {
                try {
                    storagePool.close()
                } catch (Exception ignored) {
                    // Ignore cleanup errors
                }
            }
            
            // Wrap and rethrow the exception
            throw new FastDFSException("Failed to initialize FastDFS client: ${e.message}", e)
        }
    }
    
    // ============================================================================
    // Public API - File Upload Operations
    // ============================================================================
    
    /**
     * Uploads a file from the local filesystem to FastDFS.
     * 
     * This method reads the file from the specified path, uploads it to FastDFS,
     * and returns the file ID that can be used to download or delete the file later.
     * 
     * The file ID format is: group_name/remote_filename
     * 
     * @param localFilename the path to the local file to upload (required)
     * @param metadata optional metadata key-value pairs to associate with the file (can be null or empty)
     * @return the file ID (group_name/remote_filename) of the uploaded file
     * @throws FastDFSException if the upload fails
     * @throws IllegalArgumentException if localFilename is null or empty
     * @throws IllegalStateException if the client is closed
     */
    String uploadFile(String localFilename, Map<String, String> metadata = null) {
        // Check if client is closed
        // This prevents operations on closed clients
        checkClosed()
        
        // Validate input
        // Ensure we have a valid filename
        if (localFilename == null || localFilename.trim().isEmpty()) {
            throw new IllegalArgumentException("Local filename cannot be null or empty")
        }
        
        // Normalize metadata
        // Convert null to empty map for easier handling
        Map<String, String> meta = metadata ?: [:]
        
        // Delegate to operations helper
        // This keeps the main client class clean and focused
        return operations.uploadFile(localFilename, meta, false)
    }
    
    /**
     * Uploads data from a byte array to FastDFS.
     * 
     * This method uploads the provided byte array as a file to FastDFS.
     * The file extension is used to determine the storage path and file type.
     * 
     * @param data the file content as a byte array (required, must not be null)
     * @param fileExtName the file extension without dot (e.g., "jpg", "txt", "pdf") (required)
     * @param metadata optional metadata key-value pairs (can be null or empty)
     * @return the file ID of the uploaded file
     * @throws FastDFSException if the upload fails
     * @throws IllegalArgumentException if data is null or fileExtName is invalid
     * @throws IllegalStateException if the client is closed
     */
    String uploadBuffer(byte[] data, String fileExtName, Map<String, String> metadata = null) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null")
        }
        
        if (fileExtName == null || fileExtName.trim().isEmpty()) {
            throw new IllegalArgumentException("File extension cannot be null or empty")
        }
        
        // Validate file extension length
        // FastDFS protocol limits extension to 6 characters
        if (fileExtName.length() > 6) {
            throw new IllegalArgumentException("File extension cannot exceed 6 characters")
        }
        
        // Normalize metadata
        Map<String, String> meta = metadata ?: [:]
        
        // Delegate to operations helper
        return operations.uploadBuffer(data, fileExtName, meta, false)
    }
    
    /**
     * Uploads an appender file from the local filesystem.
     * 
     * Appender files can be modified after upload using appendFile, modifyFile,
     * and truncateFile operations. This is useful for log files or files that
     * need to be updated incrementally.
     * 
     * @param localFilename the path to the local file to upload (required)
     * @param metadata optional metadata (can be null or empty)
     * @return the file ID of the uploaded appender file
     * @throws FastDFSException if the upload fails
     * @throws IllegalArgumentException if localFilename is invalid
     * @throws IllegalStateException if the client is closed
     */
    String uploadAppenderFile(String localFilename, Map<String, String> metadata = null) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (localFilename == null || localFilename.trim().isEmpty()) {
            throw new IllegalArgumentException("Local filename cannot be null or empty")
        }
        
        // Normalize metadata
        Map<String, String> meta = metadata ?: [:]
        
        // Delegate to operations helper with appender flag
        return operations.uploadFile(localFilename, meta, true)
    }
    
    /**
     * Uploads an appender file from a byte array.
     * 
     * @param data the file content (required)
     * @param fileExtName the file extension (required)
     * @param metadata optional metadata (can be null or empty)
     * @return the file ID of the uploaded appender file
     * @throws FastDFSException if the upload fails
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    String uploadAppenderBuffer(byte[] data, String fileExtName, Map<String, String> metadata = null) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null")
        }
        
        if (fileExtName == null || fileExtName.trim().isEmpty()) {
            throw new IllegalArgumentException("File extension cannot be null or empty")
        }
        
        if (fileExtName.length() > 6) {
            throw new IllegalArgumentException("File extension cannot exceed 6 characters")
        }
        
        // Normalize metadata
        Map<String, String> meta = metadata ?: [:]
        
        // Delegate to operations helper with appender flag
        return operations.uploadBuffer(data, fileExtName, meta, true)
    }
    
    /**
     * Uploads a slave file associated with a master file.
     * 
     * Slave files are typically thumbnails, previews, or other derived files
     * associated with a master file. They share the same group as the master
     * and use a prefix to distinguish them.
     * 
     * @param masterFileId the file ID of the master file (required)
     * @param prefixName the prefix for the slave file (e.g., "thumb", "small", "large") (required, max 16 chars)
     * @param fileExtName the file extension (required)
     * @param data the file content (required)
     * @param metadata optional metadata (can be null or empty)
     * @return the file ID of the uploaded slave file
     * @throws FastDFSException if the upload fails
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    String uploadSlaveFile(String masterFileId, String prefixName, String fileExtName,
                          byte[] data, Map<String, String> metadata = null) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (masterFileId == null || masterFileId.trim().isEmpty()) {
            throw new IllegalArgumentException("Master file ID cannot be null or empty")
        }
        
        if (prefixName == null || prefixName.trim().isEmpty()) {
            throw new IllegalArgumentException("Prefix name cannot be null or empty")
        }
        
        // Validate prefix length
        // FastDFS protocol limits prefix to 16 characters
        if (prefixName.length() > 16) {
            throw new IllegalArgumentException("Prefix name cannot exceed 16 characters")
        }
        
        if (fileExtName == null || fileExtName.trim().isEmpty()) {
            throw new IllegalArgumentException("File extension cannot be null or empty")
        }
        
        if (fileExtName.length() > 6) {
            throw new IllegalArgumentException("File extension cannot exceed 6 characters")
        }
        
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null")
        }
        
        // Normalize metadata
        Map<String, String> meta = metadata ?: [:]
        
        // Delegate to operations helper
        return operations.uploadSlaveFile(masterFileId, prefixName, fileExtName, data, meta)
    }
    
    // ============================================================================
    // Public API - File Download Operations
    // ============================================================================
    
    /**
     * Downloads a file from FastDFS and returns its content as a byte array.
     * 
     * This method downloads the entire file into memory. For large files,
     * consider using downloadFileRange or downloadToFile to avoid memory issues.
     * 
     * @param fileId the file ID (group_name/remote_filename) (required)
     * @return the file content as a byte array
     * @throws FastDFSException if the download fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if fileId is invalid
     * @throws IllegalStateException if the client is closed
     */
    byte[] downloadFile(String fileId) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        // Delegate to operations helper
        // Download entire file (offset=0, length=0 means full file)
        return operations.downloadFile(fileId, 0, 0)
    }
    
    /**
     * Downloads a specific range of bytes from a file.
     * 
     * This is useful for streaming large files or downloading only a portion
     * of a file. The range is specified by offset and length.
     * 
     * @param fileId the file ID (required)
     * @param offset the starting byte offset (0-based, must be >= 0)
     * @param length the number of bytes to download (0 means to end of file, must be >= 0)
     * @return the file content as a byte array
     * @throws FastDFSException if the download fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    byte[] downloadFileRange(String fileId, long offset, long length) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        if (offset < 0) {
            throw new IllegalArgumentException("Offset cannot be negative")
        }
        
        if (length < 0) {
            throw new IllegalArgumentException("Length cannot be negative")
        }
        
        // Delegate to operations helper
        return operations.downloadFile(fileId, offset, length)
    }
    
    /**
     * Downloads a file from FastDFS and saves it to the local filesystem.
     * 
     * This method is more memory-efficient than downloadFile for large files
     * as it streams the data directly to disk without loading it all into memory.
     * 
     * @param fileId the file ID (required)
     * @param localFilename the path where the file should be saved (required)
     * @throws FastDFSException if the download fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    void downloadToFile(String fileId, String localFilename) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        if (localFilename == null || localFilename.trim().isEmpty()) {
            throw new IllegalArgumentException("Local filename cannot be null or empty")
        }
        
        // Delegate to operations helper
        operations.downloadToFile(fileId, localFilename)
    }
    
    // ============================================================================
    // Public API - File Deletion Operations
    // ============================================================================
    
    /**
     * Deletes a file from FastDFS.
     * 
     * Once deleted, the file cannot be recovered. Use with caution.
     * 
     * @param fileId the file ID to delete (required)
     * @throws FastDFSException if the deletion fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if fileId is invalid
     * @throws IllegalStateException if the client is closed
     */
    void deleteFile(String fileId) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        // Delegate to operations helper
        operations.deleteFile(fileId)
    }
    
    // ============================================================================
    // Public API - Appender File Operations
    // ============================================================================
    
    /**
     * Appends data to an appender file.
     * 
     * The data is appended to the end of the file. The file must have been
     * uploaded as an appender file (using uploadAppenderFile or uploadAppenderBuffer).
     * 
     * @param fileId the file ID of the appender file (required)
     * @param data the data to append (required)
     * @throws FastDFSException if the append fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    void appendFile(String fileId, byte[] data) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null")
        }
        
        // Delegate to operations helper
        operations.appendFile(fileId, data)
    }
    
    /**
     * Modifies content of an appender file at a specific offset.
     * 
     * This overwrites existing content starting at the specified offset.
     * The file must be an appender file.
     * 
     * @param fileId the file ID of the appender file (required)
     * @param offset the byte offset where modification should start (must be >= 0)
     * @param data the new data to write (required)
     * @throws FastDFSException if the modification fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    void modifyFile(String fileId, long offset, byte[] data) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        if (offset < 0) {
            throw new IllegalArgumentException("Offset cannot be negative")
        }
        
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null")
        }
        
        // Delegate to operations helper
        operations.modifyFile(fileId, offset, data)
    }
    
    /**
     * Truncates an appender file to the specified size.
     * 
     * If the file is larger than the specified size, it is truncated.
     * If the file is smaller, it is extended with zeros.
     * 
     * @param fileId the file ID of the appender file (required)
     * @param size the new size of the file in bytes (must be >= 0)
     * @throws FastDFSException if the truncation fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    void truncateFile(String fileId, long size) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        if (size < 0) {
            throw new IllegalArgumentException("Size cannot be negative")
        }
        
        // Delegate to operations helper
        operations.truncateFile(fileId, size)
    }
    
    // ============================================================================
    // Public API - Metadata Operations
    // ============================================================================
    
    /**
     * Sets metadata for a file.
     * 
     * Metadata is stored as key-value pairs. The flag parameter determines
     * whether to overwrite existing metadata or merge with it.
     * 
     * @param fileId the file ID (required)
     * @param metadata the metadata key-value pairs (required, must not be null)
     * @param flag the metadata operation flag (OVERWRITE or MERGE) (required)
     * @throws FastDFSException if setting metadata fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if parameters are invalid
     * @throws IllegalStateException if the client is closed
     */
    void setMetadata(String fileId, Map<String, String> metadata, MetadataFlag flag) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        if (metadata == null) {
            throw new IllegalArgumentException("Metadata cannot be null")
        }
        
        if (flag == null) {
            throw new IllegalArgumentException("Metadata flag cannot be null")
        }
        
        // Delegate to operations helper
        operations.setMetadata(fileId, metadata, flag)
    }
    
    /**
     * Retrieves metadata for a file.
     * 
     * @param fileId the file ID (required)
     * @return a map of metadata key-value pairs (empty map if no metadata exists)
     * @throws FastDFSException if retrieving metadata fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if fileId is invalid
     * @throws IllegalStateException if the client is closed
     */
    Map<String, String> getMetadata(String fileId) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        // Delegate to operations helper
        return operations.getMetadata(fileId)
    }
    
    /**
     * Retrieves file information including size, creation time, and CRC32 checksum.
     * 
     * @param fileId the file ID (required)
     * @return a FileInfo object containing file details
     * @throws FastDFSException if retrieving file info fails
     * @throws FileNotFoundException if the file does not exist
     * @throws IllegalArgumentException if fileId is invalid
     * @throws IllegalStateException if the client is closed
     */
    FileInfo getFileInfo(String fileId) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        // Delegate to operations helper
        return operations.getFileInfo(fileId)
    }
    
    /**
     * Checks if a file exists on the storage server.
     * 
     * @param fileId the file ID to check (required)
     * @return true if the file exists, false otherwise
     * @throws FastDFSException if the check fails (other than file not found)
     * @throws IllegalArgumentException if fileId is invalid
     * @throws IllegalStateException if the client is closed
     */
    boolean fileExists(String fileId) {
        // Check if client is closed
        checkClosed()
        
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        // Try to get file info
        // If file exists, this will succeed
        // If file doesn't exist, this will throw FileNotFoundException
        try {
            getFileInfo(fileId)
            return true
        } catch (FileNotFoundException e) {
            return false
        } catch (FastDFSException e) {
            // Re-throw other exceptions
            throw e
        }
    }
    
    // ============================================================================
    // Public API - Lifecycle Management
    // ============================================================================
    
    /**
     * Closes the client and releases all resources.
     * 
     * After closing, the client cannot be used for further operations.
     * All connection pools are closed and connections are released.
     * 
     * This method is idempotent - calling it multiple times is safe.
     * 
     * @throws FastDFSException if closing fails (should be rare)
     */
    void close() {
        // Acquire write lock
        // Only one thread can close the client at a time
        writeLock.lock()
        
        try {
            // Check if already closed
            // This makes the method idempotent
            if (closed) {
                return
            }
            
            // Mark as closed
            // This prevents further operations
            closed = true
            
            // Collect any errors during cleanup
            List<Exception> errors = []
            
            // Close tracker pool
            if (trackerPool != null) {
                try {
                    trackerPool.close()
                } catch (Exception e) {
                    errors.add(e)
                }
            }
            
            // Close storage pool
            if (storagePool != null) {
                try {
                    storagePool.close()
                } catch (Exception e) {
                    errors.add(e)
                }
            }
            
            // If there were errors, throw an exception
            if (!errors.isEmpty()) {
                String message = "Errors occurred while closing client: " + errors.collect { it.message }.join(", ")
                throw new FastDFSException(message, errors[0])
            }
            
        } finally {
            // Always release the lock
            writeLock.unlock()
        }
    }
    
    // ============================================================================
    // Private Helper Methods
    // ============================================================================
    
    /**
     * Checks if the client is closed and throws an exception if it is.
     * 
     * This method is called at the beginning of every public operation
     * to ensure the client is still usable.
     * 
     * @throws IllegalStateException if the client is closed
     */
    private void checkClosed() {
        // Acquire read lock
        // Multiple threads can check simultaneously
        readLock.lock()
        
        try {
            // Check closed flag
            if (closed) {
                throw new IllegalStateException("Client has been closed")
            }
        } finally {
            // Always release the lock
            readLock.unlock()
        }
    }
    
    /**
     * Gets the client configuration.
     * 
     * This is used internally by operations and other components.
     * 
     * @return the client configuration (defensive copy)
     */
    ClientConfig getConfig() {
        return new ClientConfig(config)
    }
    
    /**
     * Gets the tracker connection pool.
     * 
     * This is used internally by operations.
     * 
     * @return the tracker connection pool
     */
    ConnectionPool getTrackerPool() {
        return trackerPool
    }
    
    /**
     * Gets the storage connection pool.
     * 
     * This is used internally by operations.
     * 
     * @return the storage connection pool
     */
    ConnectionPool getStoragePool() {
        return storagePool
    }
    
    /**
     * Gets the protocol handler.
     * 
     * This is used internally by operations.
     * 
     * @return the protocol handler
     */
    ProtocolHandler getProtocolHandler() {
        return protocolHandler
    }
}

