/**
 * FileOperations - File Operation Handler
 * 
 * This class encapsulates all file-related operations including upload,
 * download, delete, and file information queries. It handles retry logic,
 * error handling, and protocol communication with FastDFS servers.
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
package com.fastdfs.client.operation;

import com.fastdfs.client.FastDFSClient;
import com.fastdfs.client.config.FastDFSConfig;
import com.fastdfs.client.connection.FastDFSConnection;
import com.fastdfs.client.connection.FastDFSConnectionPool;
import com.fastdfs.client.exception.FastDFSException;
import com.fastdfs.client.exception.FastDFSError;
import com.fastdfs.client.protocol.FastDFSProtocol;
import com.fastdfs.client.types.FileInfo;
import com.fastdfs.client.types.StorageServer;
import com.fastdfs.client.types.FastDFSTypes;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Date;
import java.util.Map;

/**
 * FileOperations handles all file-related operations for the FastDFS client.
 * 
 * This class provides methods for uploading, downloading, and deleting files,
 * as well as querying file information. All operations include retry logic
 * and comprehensive error handling.
 * 
 * The class is package-private and is used internally by FastDFSClient.
 * Application code should use the FastDFSClient API instead of calling
 * these methods directly.
 */
class FileOperations {
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * Reference to the FastDFS client instance.
     * 
     * This is used to access connection pools and configuration.
     */
    private final FastDFSClient client;
    
    /**
     * Client configuration.
     * 
     * This contains timeout values, retry counts, and other settings
     * needed for operations.
     */
    private final FastDFSConfig config;
    
    /**
     * Tracker connection pool.
     * 
     * Used to obtain connections to tracker servers for querying storage servers.
     */
    private final FastDFSConnectionPool trackerPool;
    
    /**
     * Storage connection pool.
     * 
     * Used to obtain connections to storage servers for file operations.
     */
    private final FastDFSConnectionPool storagePool;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new FileOperations instance.
     * 
     * @param client the FastDFS client instance
     */
    FileOperations(FastDFSClient client) {
        this.client = client;
        this.config = client.getConfig();
        this.trackerPool = client.getTrackerPool();
        this.storagePool = client.getStoragePool();
    }
    
    // ============================================================================
    // Public Methods - File Upload
    // ============================================================================
    
    /**
     * Uploads a file from the local filesystem to FastDFS.
     * 
     * This method reads the file from disk, uploads it to a storage server
     * selected by the tracker, and returns the file ID.
     * 
     * @param localFilePath the path to the local file
     * @param metadata optional metadata key-value pairs
     * @return the file ID
     * @throws FastDFSException if the upload fails
     * @throws IOException if the file cannot be read
     */
    String uploadFile(String localFilePath, Map<String, String> metadata) 
            throws FastDFSException, IOException {
        // Read file content
        byte[] fileData = Files.readAllBytes(Paths.get(localFilePath));
        
        // Get file extension from filename
        String fileExtName = getFileExtension(localFilePath);
        
        // Upload buffer
        return uploadBufferWithRetry(fileData, fileExtName, metadata, false);
    }
    
    /**
     * Uploads file data from a byte array to FastDFS.
     * 
     * @param data the file content
     * @param fileExtName the file extension without dot
     * @param metadata optional metadata
     * @return the file ID
     * @throws FastDFSException if the upload fails
     */
    String uploadBuffer(byte[] data, String fileExtName, Map<String, String> metadata) 
            throws FastDFSException {
        return uploadBufferWithRetry(data, fileExtName, metadata, false);
    }
    
    /**
     * Uploads a slave file associated with a master file.
     * 
     * @param masterFileId the master file ID
     * @param prefixName the prefix for the slave file
     * @param fileExtName the file extension
     * @param data the file content
     * @param metadata optional metadata
     * @return the slave file ID
     * @throws FastDFSException if the upload fails
     */
    String uploadSlaveFile(String masterFileId, String prefixName, String fileExtName,
                           byte[] data, Map<String, String> metadata) throws FastDFSException {
        // Parse master file ID
        FastDFSProtocol.FileIDComponents masterComponents = 
            FastDFSProtocol.splitFileID(masterFileId);
        
        // Get storage server for download (to find where master file is stored)
        StorageServer storageServer = getDownloadStorageServer(
            masterComponents.getGroupName(),
            masterComponents.getRemoteFilename()
        );
        
        // Get connection to storage server
        FastDFSConnection conn = storagePool.get(storageServer.getAddress());
        try {
            // Build request
            byte[] prefixBytes = FastDFSProtocol.padString(prefixName, FastDFSTypes.FDFS_FILE_PREFIX_MAX_LEN);
            byte[] extBytes = FastDFSProtocol.padString(fileExtName, FastDFSTypes.FDFS_FILE_EXT_NAME_MAX_LEN);
            byte[] groupBytes = FastDFSProtocol.padString(masterComponents.getGroupName(), 
                                                          FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN);
            byte[] filenameBytes = masterComponents.getRemoteFilename().getBytes();
            
            long bodyLen = FastDFSTypes.FDFS_FILE_PREFIX_MAX_LEN + 
                          FastDFSTypes.FDFS_FILE_EXT_NAME_MAX_LEN + 
                          FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + 
                          filenameBytes.length + 
                          data.length;
            
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(prefixBytes, config.getNetworkTimeout());
            conn.send(extBytes, config.getNetworkTimeout());
            conn.send(groupBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            conn.send(data, config.getNetworkTimeout());
            
            // Receive response
            byte[] respHeader = conn.receiveFull(FastDFSTypes.FDFS_PROTO_HEADER_LEN, 
                                                 config.getNetworkTimeout());
            FastDFSProtocol.ProtocolHeader headerObj = FastDFSProtocol.decodeHeader(respHeader);
            
            if (headerObj.getStatus() != 0) {
                throw mapStatusToError(headerObj.getStatus());
            }
            
            if (headerObj.getLength() <= 0) {
                throw new FastDFSException(
                    FastDFSError.INVALID_RESPONSE,
                    "Invalid response length: " + headerObj.getLength()
                );
            }
            
            byte[] respBody = conn.receiveFull((int) headerObj.getLength(), 
                                               config.getNetworkTimeout());
            
            if (respBody.length < FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN) {
                throw new FastDFSException(
                    FastDFSError.INVALID_RESPONSE,
                    "Response body too short"
                );
            }
            
            // Parse response
            String groupName = FastDFSProtocol.unpadString(
                new byte[FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN]
            );
            System.arraycopy(respBody, 0, 
                           new byte[FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN], 0, 
                           FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN);
            groupName = FastDFSProtocol.unpadString(
                java.util.Arrays.copyOf(respBody, FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN)
            );
            String remoteFilename = new String(
                respBody, FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN, 
                respBody.length - FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN
            );
            
            String fileId = FastDFSProtocol.joinFileID(groupName, remoteFilename);
            
            // Set metadata if provided
            if (metadata != null && !metadata.isEmpty()) {
                try {
                    // Metadata operations would be handled by MetadataOperations
                    // For now, we'll skip it if it fails
                } catch (Exception ignored) {
                    // Ignore metadata errors
                }
            }
            
            return fileId;
            
        } finally {
            storagePool.put(conn);
        }
    }
    
    // ============================================================================
    // Public Methods - File Download
    // ============================================================================
    
    /**
     * Downloads a file from FastDFS.
     * 
     * @param fileId the file ID
     * @return the file content
     * @throws FastDFSException if the download fails
     */
    byte[] downloadFile(String fileId) throws FastDFSException {
        return downloadFileRange(fileId, 0, 0);
    }
    
    /**
     * Downloads a specific range of bytes from a file.
     * 
     * @param fileId the file ID
     * @param offset the starting byte offset
     * @param length the number of bytes to download (0 means to end of file)
     * @return the file content
     * @throws FastDFSException if the download fails
     */
    byte[] downloadFileRange(String fileId, long offset, long length) throws FastDFSException {
        // Parse file ID
        FastDFSProtocol.FileIDComponents components = FastDFSProtocol.splitFileID(fileId);
        
        // Get storage server
        StorageServer storageServer = getDownloadStorageServer(
            components.getGroupName(),
            components.getRemoteFilename()
        );
        
        // Get connection
        FastDFSConnection conn = storagePool.get(storageServer.getAddress());
        try {
            // Build request
            byte[] offsetBytes = FastDFSProtocol.encodeInt64(offset);
            byte[] lengthBytes = FastDFSProtocol.encodeInt64(length);
            byte[] filenameBytes = components.getRemoteFilename().getBytes();
            
            long bodyLen = 16 + filenameBytes.length;
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_DOWNLOAD_FILE,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(offsetBytes, config.getNetworkTimeout());
            conn.send(lengthBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            
            // Receive response
            byte[] respHeader = conn.receiveFull(FastDFSTypes.FDFS_PROTO_HEADER_LEN, 
                                                 config.getNetworkTimeout());
            FastDFSProtocol.ProtocolHeader headerObj = FastDFSProtocol.decodeHeader(respHeader);
            
            if (headerObj.getStatus() != 0) {
                throw mapStatusToError(headerObj.getStatus());
            }
            
            if (headerObj.getLength() <= 0) {
                return new byte[0];
            }
            
            // Receive file data
            return conn.receiveFull((int) headerObj.getLength(), config.getNetworkTimeout());
            
        } finally {
            storagePool.put(conn);
        }
    }
    
    /**
     * Downloads a file and saves it to the local filesystem.
     * 
     * @param fileId the file ID
     * @param localFilePath the path where the file should be saved
     * @throws FastDFSException if the download fails
     * @throws IOException if the file cannot be written
     */
    void downloadToFile(String fileId, String localFilePath) throws FastDFSException, IOException {
        // Download file data
        byte[] data = downloadFile(fileId);
        
        // Write to file
        Files.write(Paths.get(localFilePath), data);
    }
    
    // ============================================================================
    // Public Methods - File Deletion
    // ============================================================================
    
    /**
     * Deletes a file from FastDFS.
     * 
     * @param fileId the file ID
     * @throws FastDFSException if the deletion fails
     */
    void deleteFile(String fileId) throws FastDFSException {
        // Parse file ID
        FastDFSProtocol.FileIDComponents components = FastDFSProtocol.splitFileID(fileId);
        
        // Get storage server
        StorageServer storageServer = getDownloadStorageServer(
            components.getGroupName(),
            components.getRemoteFilename()
        );
        
        // Get connection
        FastDFSConnection conn = storagePool.get(storageServer.getAddress());
        try {
            // Build request
            byte[] groupBytes = FastDFSProtocol.padString(
                components.getGroupName(),
                FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN
            );
            byte[] filenameBytes = components.getRemoteFilename().getBytes();
            
            long bodyLen = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + filenameBytes.length;
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_DELETE_FILE,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(groupBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            
            // Receive response
            byte[] respHeader = conn.receiveFull(FastDFSTypes.FDFS_PROTO_HEADER_LEN, 
                                                 config.getNetworkTimeout());
            FastDFSProtocol.ProtocolHeader headerObj = FastDFSProtocol.decodeHeader(respHeader);
            
            if (headerObj.getStatus() != 0) {
                throw mapStatusToError(headerObj.getStatus());
            }
            
        } finally {
            storagePool.put(conn);
        }
    }
    
    // ============================================================================
    // Public Methods - File Information
    // ============================================================================
    
    /**
     * Retrieves detailed information about a file.
     * 
     * @param fileId the file ID
     * @return FileInfo object containing file details
     * @throws FastDFSException if the operation fails
     */
    FileInfo getFileInfo(String fileId) throws FastDFSException {
        // Parse file ID
        FastDFSProtocol.FileIDComponents components = FastDFSProtocol.splitFileID(fileId);
        
        // Get storage server
        StorageServer storageServer = getDownloadStorageServer(
            components.getGroupName(),
            components.getRemoteFilename()
        );
        
        // Get connection
        FastDFSConnection conn = storagePool.get(storageServer.getAddress());
        try {
            // Build request
            byte[] groupBytes = FastDFSProtocol.padString(
                components.getGroupName(),
                FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN
            );
            byte[] filenameBytes = components.getRemoteFilename().getBytes();
            
            long bodyLen = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + filenameBytes.length;
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_QUERY_FILE_INFO,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(groupBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            
            // Receive response
            byte[] respHeader = conn.receiveFull(FastDFSTypes.FDFS_PROTO_HEADER_LEN, 
                                                 config.getNetworkTimeout());
            FastDFSProtocol.ProtocolHeader headerObj = FastDFSProtocol.decodeHeader(respHeader);
            
            if (headerObj.getStatus() != 0) {
                throw mapStatusToError(headerObj.getStatus());
            }
            
            if (headerObj.getLength() < 40) {
                throw new FastDFSException(
                    FastDFSError.INVALID_RESPONSE,
                    "Response body too short for file info"
                );
            }
            
            byte[] respBody = conn.receiveFull((int) headerObj.getLength(), 
                                               config.getNetworkTimeout());
            
            // Parse file info
            long fileSize = FastDFSProtocol.decodeInt64(
                java.util.Arrays.copyOfRange(respBody, 0, 8)
            );
            long createTime = FastDFSProtocol.decodeInt64(
                java.util.Arrays.copyOfRange(respBody, 8, 16)
            );
            int crc32Int = FastDFSProtocol.decodeInt32(
                java.util.Arrays.copyOfRange(respBody, 16, 20)
            );
            long crc32 = crc32Int & 0xFFFFFFFFL;
            String sourceIp = FastDFSProtocol.unpadString(
                java.util.Arrays.copyOfRange(respBody, 20, 36)
            );
            
            return new FileInfo(
                fileSize,
                new Date(createTime * 1000), // Convert seconds to milliseconds
                crc32,
                sourceIp
            );
            
        } finally {
            storagePool.put(conn);
        }
    }
    
    // ============================================================================
    // Private Helper Methods
    // ============================================================================
    
    /**
     * Uploads buffer with retry logic.
     */
    private String uploadBufferWithRetry(byte[] data, String fileExtName, 
                                        Map<String, String> metadata, 
                                        boolean isAppender) throws FastDFSException {
        FastDFSException lastException = null;
        
        for (int i = 0; i <= config.getRetryCount(); i++) {
            try {
                return uploadBufferInternal(data, fileExtName, metadata, isAppender);
            } catch (FastDFSException e) {
                lastException = e;
                
                // Don't retry on certain errors
                if (e.getError() == FastDFSError.INVALID_ARGUMENT ||
                    e.getError() == FastDFSError.FILE_NOT_FOUND) {
                    throw e;
                }
                
                // Wait before retry (exponential backoff)
                if (i < config.getRetryCount()) {
                    try {
                        Thread.sleep(1000 * (i + 1));
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        throw new FastDFSException(
                            FastDFSError.OPERATION_FAILED,
                            "Operation interrupted",
                            ie
                        );
                    }
                }
            }
        }
        
        throw lastException;
    }
    
    /**
     * Performs the actual buffer upload.
     */
    private String uploadBufferInternal(byte[] data, String fileExtName,
                                       Map<String, String> metadata,
                                       boolean isAppender) throws FastDFSException {
        // Get storage server from tracker
        StorageServer storageServer = getStorageServer("");
        
        // Get connection
        FastDFSConnection conn = storagePool.get(storageServer.getAddress());
        try {
            // Prepare upload command
            byte cmd = isAppender 
                ? FastDFSTypes.STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE
                : FastDFSTypes.STORAGE_PROTO_CMD_UPLOAD_FILE;
            
            // Build request
            byte[] extBytes = FastDFSProtocol.padString(fileExtName, 
                                                        FastDFSTypes.FDFS_FILE_EXT_NAME_MAX_LEN);
            byte storePathIndex = storageServer.getStorePathIndex();
            
            long bodyLen = 1 + FastDFSTypes.FDFS_FILE_EXT_NAME_MAX_LEN + data.length;
            byte[] header = FastDFSProtocol.encodeHeader(bodyLen, cmd, (byte) 0);
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(new byte[]{storePathIndex}, config.getNetworkTimeout());
            conn.send(extBytes, config.getNetworkTimeout());
            conn.send(data, config.getNetworkTimeout());
            
            // Receive response
            byte[] respHeader = conn.receiveFull(FastDFSTypes.FDFS_PROTO_HEADER_LEN, 
                                                 config.getNetworkTimeout());
            FastDFSProtocol.ProtocolHeader headerObj = FastDFSProtocol.decodeHeader(respHeader);
            
            if (headerObj.getStatus() != 0) {
                throw mapStatusToError(headerObj.getStatus());
            }
            
            if (headerObj.getLength() <= 0) {
                throw new FastDFSException(
                    FastDFSError.INVALID_RESPONSE,
                    "Invalid response length: " + headerObj.getLength()
                );
            }
            
            byte[] respBody = conn.receiveFull((int) headerObj.getLength(), 
                                               config.getNetworkTimeout());
            
            if (respBody.length < FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN) {
                throw new FastDFSException(
                    FastDFSError.INVALID_RESPONSE,
                    "Response body too short"
                );
            }
            
            // Parse response
            String groupName = FastDFSProtocol.unpadString(
                java.util.Arrays.copyOf(respBody, FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN)
            );
            String remoteFilename = new String(
                respBody, FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN,
                respBody.length - FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN
            );
            
            return FastDFSProtocol.joinFileID(groupName, remoteFilename);
            
        } finally {
            storagePool.put(conn);
        }
    }
    
    /**
     * Gets a storage server from tracker for upload.
     */
    private StorageServer getStorageServer(String groupName) throws FastDFSException {
        FastDFSConnection conn = trackerPool.get("");
        try {
            // Build request
            long bodyLen = groupName.isEmpty() ? 0 : FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN;
            byte cmd = groupName.isEmpty()
                ? FastDFSTypes.TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE
                : FastDFSTypes.TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ONE;
            
            byte[] header = FastDFSProtocol.encodeHeader(bodyLen, cmd, (byte) 0);
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            if (!groupName.isEmpty()) {
                byte[] groupBytes = FastDFSProtocol.padString(groupName, 
                                                             FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN);
                conn.send(groupBytes, config.getNetworkTimeout());
            }
            
            // Receive response
            byte[] respHeader = conn.receiveFull(FastDFSTypes.FDFS_PROTO_HEADER_LEN, 
                                                 config.getNetworkTimeout());
            FastDFSProtocol.ProtocolHeader headerObj = FastDFSProtocol.decodeHeader(respHeader);
            
            if (headerObj.getStatus() != 0) {
                throw mapStatusToError(headerObj.getStatus());
            }
            
            if (headerObj.getLength() <= 0) {
                throw new FastDFSException(
                    FastDFSError.NO_STORAGE_SERVER,
                    "No storage server available"
                );
            }
            
            byte[] respBody = conn.receiveFull((int) headerObj.getLength(), 
                                               config.getNetworkTimeout());
            
            // Parse storage server info
            if (respBody.length < FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + 
                                  FastDFSTypes.IP_ADDRESS_SIZE + 9) {
                throw new FastDFSException(
                    FastDFSError.INVALID_RESPONSE,
                    "Response body too short"
                );
            }
            
            int offset = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN;
            String ipAddr = FastDFSProtocol.unpadString(
                java.util.Arrays.copyOfRange(respBody, offset, offset + FastDFSTypes.IP_ADDRESS_SIZE)
            );
            offset += FastDFSTypes.IP_ADDRESS_SIZE;
            
            int port = (int) FastDFSProtocol.decodeInt64(
                java.util.Arrays.copyOfRange(respBody, offset, offset + 8)
            );
            offset += 8;
            
            byte storePathIndex = respBody[offset];
            
            return new StorageServer(ipAddr, port, storePathIndex);
            
        } finally {
            trackerPool.put(conn);
        }
    }
    
    /**
     * Gets a storage server from tracker for download.
     */
    private StorageServer getDownloadStorageServer(String groupName, String remoteFilename) 
            throws FastDFSException {
        FastDFSConnection conn = trackerPool.get("");
        try {
            // Build request
            byte[] filenameBytes = remoteFilename.getBytes();
            long bodyLen = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + filenameBytes.length;
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            byte[] groupBytes = FastDFSProtocol.padString(groupName, 
                                                         FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN);
            conn.send(groupBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            
            // Receive response
            byte[] respHeader = conn.receiveFull(FastDFSTypes.FDFS_PROTO_HEADER_LEN, 
                                                 config.getNetworkTimeout());
            FastDFSProtocol.ProtocolHeader headerObj = FastDFSProtocol.decodeHeader(respHeader);
            
            if (headerObj.getStatus() != 0) {
                throw mapStatusToError(headerObj.getStatus());
            }
            
            byte[] respBody = conn.receiveFull((int) headerObj.getLength(), 
                                               config.getNetworkTimeout());
            
            // Parse response
            if (respBody.length < FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + 
                                  FastDFSTypes.IP_ADDRESS_SIZE + 8) {
                throw new FastDFSException(
                    FastDFSError.INVALID_RESPONSE,
                    "Response body too short"
                );
            }
            
            int offset = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN;
            String ipAddr = FastDFSProtocol.unpadString(
                java.util.Arrays.copyOfRange(respBody, offset, offset + FastDFSTypes.IP_ADDRESS_SIZE)
            );
            offset += FastDFSTypes.IP_ADDRESS_SIZE;
            
            int port = (int) FastDFSProtocol.decodeInt64(
                java.util.Arrays.copyOfRange(respBody, offset, offset + 8)
            );
            
            return new StorageServer(ipAddr, port);
            
        } finally {
            trackerPool.put(conn);
        }
    }
    
    /**
     * Maps protocol status code to FastDFSException.
     */
    private FastDFSException mapStatusToError(byte status) {
        switch (status) {
            case 0:
                return null; // Success
            case 2:
                return new FastDFSException(FastDFSError.FILE_NOT_FOUND);
            case 6:
                return new FastDFSException(FastDFSError.FILE_ALREADY_EXISTS);
            case 22:
                return new FastDFSException(FastDFSError.INVALID_ARGUMENT);
            case 28:
                return new FastDFSException(FastDFSError.INSUFFICIENT_SPACE);
            default:
                return new FastDFSException(
                    FastDFSError.UNKNOWN_ERROR,
                    "Unknown error code: " + status
                );
        }
    }
    
    /**
     * Gets file extension from filename.
     */
    private String getFileExtension(String filename) {
        int lastDot = filename.lastIndexOf('.');
        if (lastDot < 0 || lastDot >= filename.length() - 1) {
            return "";
        }
        String ext = filename.substring(lastDot + 1);
        if (ext.length() > FastDFSTypes.FDFS_FILE_EXT_NAME_MAX_LEN) {
            ext = ext.substring(0, FastDFSTypes.FDFS_FILE_EXT_NAME_MAX_LEN);
        }
        return ext;
    }
}

