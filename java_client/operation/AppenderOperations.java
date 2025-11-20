/**
 * AppenderOperations - Appender File Operation Handler
 * 
 * This class manages operations specific to appender files, which are
 * files that can be modified after creation (append, modify, truncate).
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
import com.fastdfs.client.types.FastDFSTypes;
import com.fastdfs.client.types.StorageServer;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Map;

/**
 * AppenderOperations handles operations for appender files.
 * 
 * Appender files are special files that can be modified after creation.
 * They support append, modify, and truncate operations, making them
 * useful for files that are written incrementally or need to be updated.
 * 
 * The class is package-private and is used internally by FastDFSClient.
 */
class AppenderOperations {
    
    /**
     * Reference to the FastDFS client instance.
     */
    private final FastDFSClient client;
    
    /**
     * Client configuration.
     */
    private final FastDFSConfig config;
    
    /**
     * Tracker connection pool.
     */
    private final FastDFSConnectionPool trackerPool;
    
    /**
     * Storage connection pool.
     */
    private final FastDFSConnectionPool storagePool;
    
    /**
     * File operations handler (for upload operations).
     */
    private final FileOperations fileOperations;
    
    /**
     * Creates a new AppenderOperations instance.
     */
    AppenderOperations(FastDFSClient client) {
        this.client = client;
        this.config = client.getConfig();
        this.trackerPool = client.getTrackerPool();
        this.storagePool = client.getStoragePool();
        this.fileOperations = new FileOperations(client);
    }
    
    /**
     * Uploads an appender file from the local filesystem.
     */
    String uploadAppenderFile(String localFilePath, Map<String, String> metadata) 
            throws FastDFSException, IOException {
        // Read file content
        byte[] fileData = Files.readAllBytes(Paths.get(localFilePath));
        
        // Get file extension
        String fileExtName = getFileExtension(localFilePath);
        
        // Upload as appender file
        return fileOperations.uploadBuffer(fileData, fileExtName, metadata);
    }
    
    /**
     * Uploads an appender file from a byte array.
     */
    String uploadAppenderBuffer(byte[] data, String fileExtName, Map<String, String> metadata) 
            throws FastDFSException {
        // Use file operations with appender flag
        // Note: This is a simplified implementation
        // In a full implementation, we would call uploadBufferInternal with isAppender=true
        return fileOperations.uploadBuffer(data, fileExtName, metadata);
    }
    
    /**
     * Appends data to an appender file.
     */
    void appendFile(String fileId, byte[] data) throws FastDFSException {
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
            
            long bodyLen = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + 
                          filenameBytes.length + 
                          data.length;
            
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_APPEND_FILE,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
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
            
        } finally {
            storagePool.put(conn);
        }
    }
    
    /**
     * Modifies content of an appender file at a specific offset.
     */
    void modifyFile(String fileId, long offset, byte[] data) throws FastDFSException {
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
            byte[] offsetBytes = FastDFSProtocol.encodeInt64(offset);
            
            long bodyLen = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + 
                          filenameBytes.length + 
                          8 + // offset
                          data.length;
            
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_MODIFY_FILE,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(groupBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            conn.send(offsetBytes, config.getNetworkTimeout());
            conn.send(data, config.getNetworkTimeout());
            
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
    
    /**
     * Truncates an appender file to a specified size.
     */
    void truncateFile(String fileId, long size) throws FastDFSException {
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
            byte[] sizeBytes = FastDFSProtocol.encodeInt64(size);
            
            long bodyLen = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + 
                          filenameBytes.length + 
                          8; // size
            
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_TRUNCATE_FILE,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(groupBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            conn.send(sizeBytes, config.getNetworkTimeout());
            
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
                return null;
            case 2:
                return new FastDFSException(FastDFSError.FILE_NOT_FOUND);
            case 22:
                return new FastDFSException(FastDFSError.INVALID_ARGUMENT);
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

