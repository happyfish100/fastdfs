package org.fastdfs.client;

import org.fastdfs.client.config.ClientConfig;
import org.fastdfs.client.connection.Connection;
import org.fastdfs.client.connection.ConnectionPool;
import org.fastdfs.client.exception.*;
import org.fastdfs.client.model.FileInfo;
import org.fastdfs.client.model.MetadataFlag;
import org.fastdfs.client.model.StorageServer;
import org.fastdfs.client.model.UploadResponse;
import org.fastdfs.client.protocol.ProtocolConstants;
import org.fastdfs.client.protocol.ProtocolHeader;
import org.fastdfs.client.protocol.ProtocolUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Date;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * FastDFS client for file operations.
 * 
 * This client provides a high-level Java API for interacting with FastDFS servers.
 * It handles connection pooling, automatic retries, and error handling.
 * 
 * Example usage:
 * <pre>
 * ClientConfig config = new ClientConfig("192.168.1.100:22122");
 * config.setMaxConns(100);
 * 
 * FastDFSClient client = new FastDFSClient(config);
 * try {
 *     String fileId = client.uploadFile("test.jpg", null);
 *     byte[] data = client.downloadFile(fileId);
 *     client.deleteFile(fileId);
 * } finally {
 *     client.close();
 * }
 * </pre>
 */
public class FastDFSClient implements AutoCloseable {
    
    private static final Logger logger = LoggerFactory.getLogger(FastDFSClient.class);
    
    private final ClientConfig config;
    private final ConnectionPool trackerPool;
    private final ConnectionPool storagePool;
    private final AtomicBoolean closed;
    
    /**
     * Creates a new FastDFS client with the given configuration.
     * 
     * @param config Client configuration
     * @throws InvalidArgumentException if configuration is invalid
     */
    public FastDFSClient(ClientConfig config) throws InvalidArgumentException {
        validateConfig(config);
        
        this.config = config;
        this.closed = new AtomicBoolean(false);
        
        // Initialize tracker connection pool
        this.trackerPool = new ConnectionPool(
                config.getTrackerAddrs(),
                config.getMaxConns(),
                config.getConnectTimeout(),
                config.getIdleTimeout()
        );
        
        // Initialize storage connection pool (servers discovered dynamically)
        this.storagePool = new ConnectionPool(
                java.util.Collections.emptyList(),
                config.getMaxConns(),
                config.getConnectTimeout(),
                config.getIdleTimeout()
        );
        
        logger.info("FastDFS client initialized with config: {}", config);
    }
    
    /**
     * Validates the client configuration.
     */
    private void validateConfig(ClientConfig config) throws InvalidArgumentException {
        if (config == null) {
            throw new InvalidArgumentException("Config is required");
        }
        
        if (config.getTrackerAddrs() == null || config.getTrackerAddrs().isEmpty()) {
            throw new InvalidArgumentException("Tracker addresses are required");
        }
        
        for (String addr : config.getTrackerAddrs()) {
            if (addr == null || addr.isEmpty() || !addr.contains(":")) {
                throw new InvalidArgumentException("Invalid tracker address: " + addr);
            }
        }
    }
    
    /**
     * Checks if the client is closed and throws an exception if so.
     */
    private void checkClosed() throws ClientClosedException {
        if (closed.get()) {
            throw new ClientClosedException();
        }
    }
    
    /**
     * Uploads a file from the local filesystem to FastDFS.
     * 
     * @param localFilename Path to the local file
     * @param metadata Optional metadata key-value pairs
     * @return The file ID in format "group/remote_filename"
     * @throws FastDFSException if upload fails
     */
    public String uploadFile(String localFilename, Map<String, String> metadata) throws FastDFSException {
        checkClosed();
        
        try {
            byte[] fileData = Files.readAllBytes(Paths.get(localFilename));
            String fileExtName = ProtocolUtil.getFileExtension(localFilename);
            return uploadBuffer(fileData, fileExtName, metadata);
        } catch (IOException e) {
            throw new FastDFSException("Failed to read file: " + localFilename, e);
        }
    }
    
    /**
     * Uploads data from a byte array to FastDFS.
     * 
     * @param data File content as byte array
     * @param fileExtName File extension without dot (e.g., "jpg", "txt")
     * @param metadata Optional metadata key-value pairs
     * @return The file ID
     * @throws FastDFSException if upload fails
     */
    public String uploadBuffer(byte[] data, String fileExtName, Map<String, String> metadata) 
            throws FastDFSException {
        checkClosed();
        return uploadBufferWithRetry(data, fileExtName, metadata, false);
    }
    
    /**
     * Uploads an appender file that can be modified later.
     * 
     * @param localFilename Path to the local file
     * @param metadata Optional metadata
     * @return The file ID
     * @throws FastDFSException if upload fails
     */
    public String uploadAppenderFile(String localFilename, Map<String, String> metadata) 
            throws FastDFSException {
        checkClosed();
        
        try {
            byte[] fileData = Files.readAllBytes(Paths.get(localFilename));
            String fileExtName = ProtocolUtil.getFileExtension(localFilename);
            return uploadAppenderBuffer(fileData, fileExtName, metadata);
        } catch (IOException e) {
            throw new FastDFSException("Failed to read file: " + localFilename, e);
        }
    }
    
    /**
     * Uploads an appender file from buffer.
     * 
     * @param data File content
     * @param fileExtName File extension
     * @param metadata Optional metadata
     * @return The file ID
     * @throws FastDFSException if upload fails
     */
    public String uploadAppenderBuffer(byte[] data, String fileExtName, Map<String, String> metadata) 
            throws FastDFSException {
        checkClosed();
        return uploadBufferWithRetry(data, fileExtName, metadata, true);
    }
    
    /**
     * Downloads a file from FastDFS and returns its content.
     * 
     * @param fileId The file ID to download
     * @return File content as byte array
     * @throws FastDFSException if download fails
     */
    public byte[] downloadFile(String fileId) throws FastDFSException {
        checkClosed();
        return downloadFileRange(fileId, 0, 0);
    }
    
    /**
     * Downloads a specific range of bytes from a file.
     * 
     * @param fileId The file ID
     * @param offset Starting byte offset
     * @param length Number of bytes to download (0 means to end of file)
     * @return File content
     * @throws FastDFSException if download fails
     */
    public byte[] downloadFileRange(String fileId, long offset, long length) throws FastDFSException {
        checkClosed();
        return downloadFileWithRetry(fileId, offset, length);
    }
    
    /**
     * Downloads a file and saves it to the local filesystem.
     * 
     * @param fileId The file ID
     * @param localFilename Path where to save the file
     * @throws FastDFSException if download fails
     */
    public void downloadToFile(String fileId, String localFilename) throws FastDFSException {
        checkClosed();
        
        byte[] data = downloadFile(fileId);
        try {
            Files.write(Paths.get(localFilename), data);
        } catch (IOException e) {
            throw new FastDFSException("Failed to write file: " + localFilename, e);
        }
    }
    
    /**
     * Deletes a file from FastDFS.
     * 
     * @param fileId The file ID to delete
     * @throws FastDFSException if deletion fails
     */
    public void deleteFile(String fileId) throws FastDFSException {
        checkClosed();
        deleteFileWithRetry(fileId);
    }
    
    /**
     * Sets metadata for a file.
     * 
     * @param fileId The file ID
     * @param metadata Metadata key-value pairs
     * @param flag Metadata operation flag (OVERWRITE or MERGE)
     * @throws FastDFSException if operation fails
     */
    public void setMetadata(String fileId, Map<String, String> metadata, MetadataFlag flag) 
            throws FastDFSException {
        checkClosed();
        setMetadataWithRetry(fileId, metadata, flag);
    }
    
    /**
     * Retrieves metadata for a file.
     * 
     * @param fileId The file ID
     * @return Metadata key-value pairs
     * @throws FastDFSException if operation fails
     */
    public Map<String, String> getMetadata(String fileId) throws FastDFSException {
        checkClosed();
        return getMetadataWithRetry(fileId);
    }
    
    /**
     * Retrieves file information including size, create time, and CRC32.
     * 
     * @param fileId The file ID
     * @return File information
     * @throws FastDFSException if operation fails
     */
    public FileInfo getFileInfo(String fileId) throws FastDFSException {
        checkClosed();
        return getFileInfoWithRetry(fileId);
    }
    
    /**
     * Checks if a file exists on the storage server.
     * 
     * @param fileId The file ID
     * @return true if file exists, false otherwise
     */
    public boolean fileExists(String fileId) {
        try {
            checkClosed();
            getFileInfo(fileId);
            return true;
        } catch (FileNotFoundException e) {
            return false;
        } catch (FastDFSException e) {
            return false;
        }
    }
    
    /**
     * Closes the client and releases all resources.
     * 
     * After calling close, all operations will throw ClientClosedException.
     * It's safe to call close multiple times.
     */
    @Override
    public void close() {
        if (closed.compareAndSet(false, true)) {
            logger.info("Closing FastDFS client");
            
            if (trackerPool != null) {
                trackerPool.close();
            }
            
            if (storagePool != null) {
                storagePool.close();
            }
            
            logger.info("FastDFS client closed");
        }
    }
    
    // Private helper methods with retry logic
    
    private String uploadBufferWithRetry(byte[] data, String fileExtName, Map<String, String> metadata, 
                                         boolean isAppender) throws FastDFSException {
        int retryCount = config.getRetryCount();
        FastDFSException lastException = null;
        
        for (int attempt = 0; attempt < retryCount; attempt++) {
            try {
                return uploadBufferInternal(data, fileExtName, metadata, isAppender);
            } catch (FastDFSException e) {
                lastException = e;
                if (attempt < retryCount - 1) {
                    logger.warn("Upload attempt {} failed, retrying...", attempt + 1, e);
                    try {
                        Thread.sleep((attempt + 1) * 1000);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        throw new FastDFSException("Upload interrupted", ie);
                    }
                }
            }
        }
        
        throw lastException;
    }
    
    private String uploadBufferInternal(byte[] data, String fileExtName, Map<String, String> metadata, 
                                        boolean isAppender) throws FastDFSException {
        // Query tracker for storage server
        StorageServer storageServer = queryStorageServerForUpload();
        
        // Prepare upload command
        byte cmd = isAppender ? ProtocolConstants.STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE 
                              : ProtocolConstants.STORAGE_PROTO_CMD_UPLOAD_FILE;
        
        // Build request body
        byte[] extNameBytes = ProtocolUtil.encodeFixedString(fileExtName, 
                ProtocolConstants.FDFS_FILE_EXT_NAME_MAX_LEN);
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(1 + 1 + extNameBytes.length + data.length);
        bodyBuffer.put((byte) storageServer.getStorePathIndex());
        bodyBuffer.put((byte) data.length);
        bodyBuffer.put(extNameBytes);
        bodyBuffer.put(data);
        
        byte[] body = bodyBuffer.array();
        
        // Send request to storage server
        Connection conn = null;
        try {
            conn = storagePool.getConnection(storageServer.getAddress());
            
            // Send header
            ProtocolHeader header = new ProtocolHeader(body.length, cmd, (byte) 0);
            conn.send(header.encode());
            
            // Send body
            conn.send(body);
            
            // Receive response
            ProtocolHeader respHeader = conn.receiveHeader();
            
            // Check for errors
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            // Parse response
            byte[] respBody = conn.receive((int) respHeader.getLength());
            String groupName = ProtocolUtil.decodeFixedString(respBody, 0, 
                    ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
            String remoteFilename = ProtocolUtil.decodeFixedString(respBody, 
                    ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN, 
                    respBody.length - ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
            
            String fileId = groupName + "/" + remoteFilename;
            
            // Set metadata if provided
            if (metadata != null && !metadata.isEmpty()) {
                setMetadata(fileId, metadata, MetadataFlag.OVERWRITE);
            }
            
            logger.info("File uploaded successfully: {}", fileId);
            return fileId;
            
        } finally {
            if (conn != null) {
                storagePool.returnConnection(conn);
            }
        }
    }
    
    private byte[] downloadFileWithRetry(String fileId, long offset, long length) throws FastDFSException {
        int retryCount = config.getRetryCount();
        FastDFSException lastException = null;
        
        for (int attempt = 0; attempt < retryCount; attempt++) {
            try {
                return downloadFileInternal(fileId, offset, length);
            } catch (FastDFSException e) {
                lastException = e;
                if (attempt < retryCount - 1) {
                    logger.warn("Download attempt {} failed, retrying...", attempt + 1, e);
                    try {
                        Thread.sleep((attempt + 1) * 1000);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        throw new FastDFSException("Download interrupted", ie);
                    }
                }
            }
        }
        
        throw lastException;
    }
    
    private byte[] downloadFileInternal(String fileId, long offset, long length) throws FastDFSException {
        String[] parts = ProtocolUtil.parseFileId(fileId);
        String groupName = parts[0];
        String remoteFilename = parts[1];
        
        // Query tracker for storage server
        StorageServer storageServer = queryStorageServerForDownload(groupName, remoteFilename);
        
        // Build request body
        byte[] groupBytes = ProtocolUtil.encodeFixedString(groupName, 
                ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
        byte[] filenameBytes = remoteFilename.getBytes();
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(16 + groupBytes.length + filenameBytes.length);
        bodyBuffer.putLong(offset);
        bodyBuffer.putLong(length);
        bodyBuffer.put(groupBytes);
        bodyBuffer.put(filenameBytes);
        
        byte[] body = bodyBuffer.array();
        
        // Send request
        Connection conn = null;
        try {
            conn = storagePool.getConnection(storageServer.getAddress());
            
            ProtocolHeader header = new ProtocolHeader(body.length, 
                    ProtocolConstants.STORAGE_PROTO_CMD_DOWNLOAD_FILE, (byte) 0);
            conn.send(header.encode());
            conn.send(body);
            
            // Receive response
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            byte[] data = conn.receive((int) respHeader.getLength());
            logger.info("File downloaded successfully: {} ({} bytes)", fileId, data.length);
            return data;
            
        } finally {
            if (conn != null) {
                storagePool.returnConnection(conn);
            }
        }
    }
    
    private void deleteFileWithRetry(String fileId) throws FastDFSException {
        int retryCount = config.getRetryCount();
        FastDFSException lastException = null;
        
        for (int attempt = 0; attempt < retryCount; attempt++) {
            try {
                deleteFileInternal(fileId);
                return;
            } catch (FastDFSException e) {
                lastException = e;
                if (attempt < retryCount - 1) {
                    logger.warn("Delete attempt {} failed, retrying...", attempt + 1, e);
                    try {
                        Thread.sleep((attempt + 1) * 1000);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        throw new FastDFSException("Delete interrupted", ie);
                    }
                }
            }
        }
        
        throw lastException;
    }
    
    private void deleteFileInternal(String fileId) throws FastDFSException {
        String[] parts = ProtocolUtil.parseFileId(fileId);
        String groupName = parts[0];
        String remoteFilename = parts[1];
        
        StorageServer storageServer = queryStorageServerForUpdate(groupName, remoteFilename);
        
        byte[] groupBytes = ProtocolUtil.encodeFixedString(groupName, 
                ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
        byte[] filenameBytes = remoteFilename.getBytes();
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(groupBytes.length + filenameBytes.length);
        bodyBuffer.put(groupBytes);
        bodyBuffer.put(filenameBytes);
        
        byte[] body = bodyBuffer.array();
        
        Connection conn = null;
        try {
            conn = storagePool.getConnection(storageServer.getAddress());
            
            ProtocolHeader header = new ProtocolHeader(body.length, 
                    ProtocolConstants.STORAGE_PROTO_CMD_DELETE_FILE, (byte) 0);
            conn.send(header.encode());
            conn.send(body);
            
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            logger.info("File deleted successfully: {}", fileId);
            
        } finally {
            if (conn != null) {
                storagePool.returnConnection(conn);
            }
        }
    }
    
    private void setMetadataWithRetry(String fileId, Map<String, String> metadata, MetadataFlag flag) 
            throws FastDFSException {
        int retryCount = config.getRetryCount();
        FastDFSException lastException = null;
        
        for (int attempt = 0; attempt < retryCount; attempt++) {
            try {
                setMetadataInternal(fileId, metadata, flag);
                return;
            } catch (FastDFSException e) {
                lastException = e;
                if (attempt < retryCount - 1) {
                    logger.warn("Set metadata attempt {} failed, retrying...", attempt + 1, e);
                    try {
                        Thread.sleep((attempt + 1) * 1000);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        throw new FastDFSException("Set metadata interrupted", ie);
                    }
                }
            }
        }
        
        throw lastException;
    }
    
    private void setMetadataInternal(String fileId, Map<String, String> metadata, MetadataFlag flag) 
            throws FastDFSException {
        String[] parts = ProtocolUtil.parseFileId(fileId);
        String groupName = parts[0];
        String remoteFilename = parts[1];
        
        StorageServer storageServer = queryStorageServerForUpdate(groupName, remoteFilename);
        
        byte[] metadataBytes = ProtocolUtil.encodeMetadata(metadata);
        byte[] groupBytes = ProtocolUtil.encodeFixedString(groupName, 
                ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
        byte[] filenameBytes = remoteFilename.getBytes();
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(16 + 1 + groupBytes.length + filenameBytes.length + metadataBytes.length);
        bodyBuffer.putLong(filenameBytes.length);
        bodyBuffer.putLong(metadataBytes.length);
        bodyBuffer.put(flag.getValue());
        bodyBuffer.put(groupBytes);
        bodyBuffer.put(filenameBytes);
        bodyBuffer.put(metadataBytes);
        
        byte[] body = bodyBuffer.array();
        
        Connection conn = null;
        try {
            conn = storagePool.getConnection(storageServer.getAddress());
            
            ProtocolHeader header = new ProtocolHeader(body.length, 
                    ProtocolConstants.STORAGE_PROTO_CMD_SET_METADATA, (byte) 0);
            conn.send(header.encode());
            conn.send(body);
            
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            logger.debug("Metadata set successfully for: {}", fileId);
            
        } finally {
            if (conn != null) {
                storagePool.returnConnection(conn);
            }
        }
    }
    
    private Map<String, String> getMetadataWithRetry(String fileId) throws FastDFSException {
        int retryCount = config.getRetryCount();
        FastDFSException lastException = null;
        
        for (int attempt = 0; attempt < retryCount; attempt++) {
            try {
                return getMetadataInternal(fileId);
            } catch (FastDFSException e) {
                lastException = e;
                if (attempt < retryCount - 1) {
                    logger.warn("Get metadata attempt {} failed, retrying...", attempt + 1, e);
                    try {
                        Thread.sleep((attempt + 1) * 1000);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        throw new FastDFSException("Get metadata interrupted", ie);
                    }
                }
            }
        }
        
        throw lastException;
    }
    
    private Map<String, String> getMetadataInternal(String fileId) throws FastDFSException {
        String[] parts = ProtocolUtil.parseFileId(fileId);
        String groupName = parts[0];
        String remoteFilename = parts[1];
        
        StorageServer storageServer = queryStorageServerForDownload(groupName, remoteFilename);
        
        byte[] groupBytes = ProtocolUtil.encodeFixedString(groupName, 
                ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
        byte[] filenameBytes = remoteFilename.getBytes();
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(groupBytes.length + filenameBytes.length);
        bodyBuffer.put(groupBytes);
        bodyBuffer.put(filenameBytes);
        
        byte[] body = bodyBuffer.array();
        
        Connection conn = null;
        try {
            conn = storagePool.getConnection(storageServer.getAddress());
            
            ProtocolHeader header = new ProtocolHeader(body.length, 
                    ProtocolConstants.STORAGE_PROTO_CMD_GET_METADATA, (byte) 0);
            conn.send(header.encode());
            conn.send(body);
            
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            byte[] respBody = conn.receive((int) respHeader.getLength());
            return ProtocolUtil.decodeMetadata(respBody);
            
        } finally {
            if (conn != null) {
                storagePool.returnConnection(conn);
            }
        }
    }
    
    private FileInfo getFileInfoWithRetry(String fileId) throws FastDFSException {
        int retryCount = config.getRetryCount();
        FastDFSException lastException = null;
        
        for (int attempt = 0; attempt < retryCount; attempt++) {
            try {
                return getFileInfoInternal(fileId);
            } catch (FastDFSException e) {
                lastException = e;
                if (attempt < retryCount - 1) {
                    logger.warn("Get file info attempt {} failed, retrying...", attempt + 1, e);
                    try {
                        Thread.sleep((attempt + 1) * 1000);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        throw new FastDFSException("Get file info interrupted", ie);
                    }
                }
            }
        }
        
        throw lastException;
    }
    
    private FileInfo getFileInfoInternal(String fileId) throws FastDFSException {
        String[] parts = ProtocolUtil.parseFileId(fileId);
        String groupName = parts[0];
        String remoteFilename = parts[1];
        
        StorageServer storageServer = queryStorageServerForDownload(groupName, remoteFilename);
        
        byte[] groupBytes = ProtocolUtil.encodeFixedString(groupName, 
                ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
        byte[] filenameBytes = remoteFilename.getBytes();
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(groupBytes.length + filenameBytes.length);
        bodyBuffer.put(groupBytes);
        bodyBuffer.put(filenameBytes);
        
        byte[] body = bodyBuffer.array();
        
        Connection conn = null;
        try {
            conn = storagePool.getConnection(storageServer.getAddress());
            
            ProtocolHeader header = new ProtocolHeader(body.length, 
                    ProtocolConstants.STORAGE_PROTO_CMD_QUERY_FILE_INFO, (byte) 0);
            conn.send(header.encode());
            conn.send(body);
            
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            byte[] respBody = conn.receive((int) respHeader.getLength());
            
            // Parse file info: fileSize(8) + createTime(8) + crc32(8) + ipAddr(16)
            ByteBuffer buffer = ByteBuffer.wrap(respBody);
            long fileSize = buffer.getLong();
            long createTimestamp = buffer.getLong();
            long crc32 = buffer.getLong();
            
            byte[] ipBytes = new byte[ProtocolConstants.IP_ADDRESS_SIZE];
            buffer.get(ipBytes);
            String ipAddr = ProtocolUtil.decodeFixedString(ipBytes);
            
            FileInfo fileInfo = new FileInfo();
            fileInfo.setFileSize(fileSize);
            fileInfo.setCreateTime(new Date(createTimestamp * 1000));
            fileInfo.setCrc32(crc32);
            fileInfo.setSourceIpAddr(ipAddr);
            
            return fileInfo;
            
        } finally {
            if (conn != null) {
                storagePool.returnConnection(conn);
            }
        }
    }
    
    // Tracker query methods
    
    private StorageServer queryStorageServerForUpload() throws FastDFSException {
        Connection conn = null;
        try {
            String trackerAddr = config.getTrackerAddrs().get(0);
            conn = trackerPool.getConnection(trackerAddr);
            
            ProtocolHeader header = new ProtocolHeader(0, 
                    ProtocolConstants.TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE, (byte) 0);
            conn.send(header.encode());
            
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            byte[] respBody = conn.receive((int) respHeader.getLength());
            return parseStorageServer(respBody);
            
        } finally {
            if (conn != null) {
                trackerPool.returnConnection(conn);
            }
        }
    }
    
    private StorageServer queryStorageServerForDownload(String groupName, String remoteFilename) 
            throws FastDFSException {
        byte[] groupBytes = ProtocolUtil.encodeFixedString(groupName, 
                ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
        byte[] filenameBytes = remoteFilename.getBytes();
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(groupBytes.length + filenameBytes.length);
        bodyBuffer.put(groupBytes);
        bodyBuffer.put(filenameBytes);
        
        byte[] body = bodyBuffer.array();
        
        Connection conn = null;
        try {
            String trackerAddr = config.getTrackerAddrs().get(0);
            conn = trackerPool.getConnection(trackerAddr);
            
            ProtocolHeader header = new ProtocolHeader(body.length, 
                    ProtocolConstants.TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE, (byte) 0);
            conn.send(header.encode());
            conn.send(body);
            
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            byte[] respBody = conn.receive((int) respHeader.getLength());
            return parseStorageServer(respBody);
            
        } finally {
            if (conn != null) {
                trackerPool.returnConnection(conn);
            }
        }
    }
    
    private StorageServer queryStorageServerForUpdate(String groupName, String remoteFilename) 
            throws FastDFSException {
        byte[] groupBytes = ProtocolUtil.encodeFixedString(groupName, 
                ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN);
        byte[] filenameBytes = remoteFilename.getBytes();
        
        ByteBuffer bodyBuffer = ByteBuffer.allocate(groupBytes.length + filenameBytes.length);
        bodyBuffer.put(groupBytes);
        bodyBuffer.put(filenameBytes);
        
        byte[] body = bodyBuffer.array();
        
        Connection conn = null;
        try {
            String trackerAddr = config.getTrackerAddrs().get(0);
            conn = trackerPool.getConnection(trackerAddr);
            
            ProtocolHeader header = new ProtocolHeader(body.length, 
                    ProtocolConstants.TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE, (byte) 0);
            conn.send(header.encode());
            conn.send(body);
            
            ProtocolHeader respHeader = conn.receiveHeader();
            
            FastDFSException error = ProtocolUtil.mapStatusToException(respHeader.getStatus());
            if (error != null) {
                throw error;
            }
            
            byte[] respBody = conn.receive((int) respHeader.getLength());
            return parseStorageServer(respBody);
            
        } finally {
            if (conn != null) {
                trackerPool.returnConnection(conn);
            }
        }
    }
    
    private StorageServer parseStorageServer(byte[] data) {
        // Response format: groupName(16) + ipAddr(16) + port(8) + storePathIndex(1)
        ByteBuffer buffer = ByteBuffer.wrap(data);
        
        byte[] groupBytes = new byte[ProtocolConstants.FDFS_GROUP_NAME_MAX_LEN];
        buffer.get(groupBytes);
        
        byte[] ipBytes = new byte[ProtocolConstants.IP_ADDRESS_SIZE];
        buffer.get(ipBytes);
        String ipAddr = ProtocolUtil.decodeFixedString(ipBytes);
        
        long port = buffer.getLong();
        byte storePathIndex = buffer.get();
        
        StorageServer server = new StorageServer();
        server.setIpAddr(ipAddr);
        server.setPort((int) port);
        server.setStorePathIndex(storePathIndex);
        
        return server;
    }
}
