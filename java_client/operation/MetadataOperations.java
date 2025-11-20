/**
 * MetadataOperations - Metadata Operation Handler
 * 
 * This class handles all metadata-related operations including setting,
 * getting, and merging file metadata. Metadata is stored as key-value
 * pairs associated with files in FastDFS.
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
import com.fastdfs.client.types.MetadataFlag;
import com.fastdfs.client.types.FastDFSTypes;
import com.fastdfs.client.types.StorageServer;

import java.util.Map;

/**
 * MetadataOperations handles all metadata-related operations for the FastDFS client.
 * 
 * This class provides methods for setting and getting file metadata. Metadata
 * can be set in overwrite mode (replacing all existing metadata) or merge mode
 * (merging with existing metadata).
 * 
 * The class is package-private and is used internally by FastDFSClient.
 */
class MetadataOperations {
    
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
     * Creates a new MetadataOperations instance.
     */
    MetadataOperations(FastDFSClient client) {
        this.client = client;
        this.config = client.getConfig();
        this.trackerPool = client.getTrackerPool();
        this.storagePool = client.getStoragePool();
    }
    
    /**
     * Sets metadata for a file.
     */
    void setMetadata(String fileId, Map<String, String> metadata, MetadataFlag flag) 
            throws FastDFSException {
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
            // Encode metadata
            byte[] metadataBytes = FastDFSProtocol.encodeMetadata(metadata);
            if (metadataBytes == null) {
                throw new FastDFSException(
                    FastDFSError.INVALID_METADATA,
                    "Metadata cannot be null or empty"
                );
            }
            
            // Build request
            byte[] groupBytes = FastDFSProtocol.padString(
                components.getGroupName(),
                FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN
            );
            byte[] filenameBytes = components.getRemoteFilename().getBytes();
            
            long bodyLen = FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN + 
                          filenameBytes.length + 
                          1 + // flag byte
                          metadataBytes.length;
            
            byte[] header = FastDFSProtocol.encodeHeader(
                bodyLen,
                FastDFSTypes.STORAGE_PROTO_CMD_SET_METADATA,
                (byte) 0
            );
            
            // Send request
            conn.send(header, config.getNetworkTimeout());
            conn.send(groupBytes, config.getNetworkTimeout());
            conn.send(filenameBytes, config.getNetworkTimeout());
            conn.send(new byte[]{flag.getProtocolValue()}, config.getNetworkTimeout());
            conn.send(metadataBytes, config.getNetworkTimeout());
            
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
     * Retrieves metadata for a file.
     */
    Map<String, String> getMetadata(String fileId) throws FastDFSException {
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
                FastDFSTypes.STORAGE_PROTO_CMD_GET_METADATA,
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
            
            if (headerObj.getLength() <= 0) {
                return new java.util.HashMap<>();
            }
            
            byte[] respBody = conn.receiveFull((int) headerObj.getLength(), 
                                               config.getNetworkTimeout());
            
            // Decode metadata
            return FastDFSProtocol.decodeMetadata(respBody);
            
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
}

