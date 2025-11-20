/**
 * FastDFS Types and Constants
 * 
 * This file defines all protocol-level constants, command codes, and data structures
 * used in communication with FastDFS tracker and storage servers.
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
package com.fastdfs.client.types;

import java.util.Date;

/**
 * FastDFSTypes contains all protocol constants and type definitions.
 * 
 * This class is not meant to be instantiated. All members are static constants
 * and utility methods that define the FastDFS protocol specification.
 * 
 * The constants defined here must match the values used by the C implementation
 * and other FastDFS clients to ensure protocol compatibility.
 */
public final class FastDFSTypes {
    
    // ============================================================================
    // Private Constructor - Prevent Instantiation
    // ============================================================================
    
    /**
     * Private constructor to prevent instantiation.
     * 
     * This class contains only static constants and utility methods.
     */
    private FastDFSTypes() {
        throw new UnsupportedOperationException("This class cannot be instantiated");
    }
    
    // ============================================================================
    // Default Network Ports
    // ============================================================================
    
    /**
     * Default port number for FastDFS tracker servers.
     * 
     * Tracker servers listen on this port by default, though it can be
     * configured differently in the tracker configuration file.
     */
    public static final int TRACKER_DEFAULT_PORT = 22122;
    
    /**
     * Default port number for FastDFS storage servers.
     * 
     * Storage servers listen on this port by default for client connections.
     * HTTP access typically uses a different port (usually 8080 or 8888).
     */
    public static final int STORAGE_DEFAULT_PORT = 23000;
    
    // ============================================================================
    // Tracker Protocol Commands
    // ============================================================================
    
    /**
     * Query storage server for upload without specifying a group.
     * 
     * This command asks the tracker to return a storage server that can
     * be used for uploading a file. The tracker will select a server based
     * on load balancing and availability.
     */
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE = 101;
    
    /**
     * Query storage server for download.
     * 
     * This command asks the tracker to return the storage server where
     * a specific file is stored, given the file's group name and remote filename.
     */
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE = 102;
    
    /**
     * Query storage server for update.
     * 
     * This command is used to find the storage server for updating file
     * metadata or performing other update operations.
     */
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE = 103;
    
    /**
     * Query storage server for upload with a specific group.
     * 
     * This command asks the tracker to return a storage server from a
     * specific group that can be used for uploading a file.
     */
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ONE = 104;
    
    /**
     * Query all storage servers for download.
     * 
     * This command returns all storage servers that have a copy of the file,
     * which is useful for load balancing downloads across multiple servers.
     */
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ALL = 105;
    
    /**
     * Query all storage servers for upload without specifying a group.
     * 
     * This command returns all available storage servers that can be used
     * for uploading files, allowing the client to choose which one to use.
     */
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL = 106;
    
    /**
     * Query all storage servers for upload with a specific group.
     * 
     * This command returns all storage servers in a specific group that
     * can be used for uploading files.
     */
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ALL = 107;
    
    /**
     * List servers in one group.
     * 
     * This command returns information about all storage servers in a
     * specific storage group.
     */
    public static final byte TRACKER_PROTO_CMD_SERVER_LIST_ONE_GROUP = 90;
    
    /**
     * List servers in all groups.
     * 
     * This command returns information about all storage servers in all
     * storage groups known to the tracker.
     */
    public static final byte TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS = 91;
    
    /**
     * List storage servers.
     * 
     * This command returns detailed information about storage servers,
     * including their status, capacity, and configuration.
     */
    public static final byte TRACKER_PROTO_CMD_SERVER_LIST_STORAGE = 92;
    
    /**
     * Delete storage server.
     * 
     * This command is used by administrators to remove a storage server
     * from the tracker's registry.
     */
    public static final byte TRACKER_PROTO_CMD_SERVER_DELETE_STORAGE = 93;
    
    /**
     * Storage server reports IP address change.
     * 
     * This command is sent by storage servers when their IP address changes,
     * allowing the tracker to update its records.
     */
    public static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_IP_CHANGED = 94;
    
    /**
     * Storage server reports status.
     * 
     * This command is sent periodically by storage servers to report their
     * current status to the tracker.
     */
    public static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_STATUS = 95;
    
    /**
     * Storage server reports disk usage.
     * 
     * This command is sent by storage servers to report their current
     * disk usage and available space.
     */
    public static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_DISK_USAGE = 96;
    
    /**
     * Storage server sync timestamp.
     * 
     * This command is used for synchronizing timestamps between storage
     * servers during file replication.
     */
    public static final byte TRACKER_PROTO_CMD_STORAGE_SYNC_TIMESTAMP = 97;
    
    /**
     * Storage server sync report.
     * 
     * This command is sent by storage servers to report synchronization
     * status and progress.
     */
    public static final byte TRACKER_PROTO_CMD_STORAGE_SYNC_REPORT = 98;
    
    // ============================================================================
    // Storage Protocol Commands
    // ============================================================================
    
    /**
     * Upload a regular file.
     * 
     * This command uploads a file to the storage server. Regular files
     * cannot be modified after upload (unlike appender files).
     */
    public static final byte STORAGE_PROTO_CMD_UPLOAD_FILE = 11;
    
    /**
     * Delete a file.
     * 
     * This command deletes a file from the storage server. The deletion
     * is permanent and cannot be undone.
     */
    public static final byte STORAGE_PROTO_CMD_DELETE_FILE = 12;
    
    /**
     * Set file metadata.
     * 
     * This command sets or updates metadata associated with a file.
     * Metadata is stored as key-value pairs.
     */
    public static final byte STORAGE_PROTO_CMD_SET_METADATA = 13;
    
    /**
     * Download a file.
     * 
     * This command downloads a file from the storage server. It supports
     * downloading the entire file or a specific range of bytes.
     */
    public static final byte STORAGE_PROTO_CMD_DOWNLOAD_FILE = 14;
    
    /**
     * Get file metadata.
     * 
     * This command retrieves metadata associated with a file.
     */
    public static final byte STORAGE_PROTO_CMD_GET_METADATA = 15;
    
    /**
     * Upload a slave file.
     * 
     * This command uploads a slave file associated with a master file.
     * Slave files are typically used for thumbnails or other derived versions.
     */
    public static final byte STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE = 21;
    
    /**
     * Query file information.
     * 
     * This command retrieves detailed information about a file, including
     * size, creation time, CRC32 checksum, and source server.
     */
    public static final byte STORAGE_PROTO_CMD_QUERY_FILE_INFO = 22;
    
    /**
     * Upload an appender file.
     * 
     * This command uploads an appender file, which can be modified after
     * creation using append, modify, or truncate operations.
     */
    public static final byte STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE = 23;
    
    /**
     * Append data to an appender file.
     * 
     * This command appends data to the end of an appender file.
     */
    public static final byte STORAGE_PROTO_CMD_APPEND_FILE = 24;
    
    /**
     * Modify content of an appender file.
     * 
     * This command modifies data at a specific offset in an appender file.
     */
    public static final byte STORAGE_PROTO_CMD_MODIFY_FILE = 34;
    
    /**
     * Truncate an appender file.
     * 
     * This command truncates an appender file to a specified size.
     */
    public static final byte STORAGE_PROTO_CMD_TRUNCATE_FILE = 36;
    
    // ============================================================================
    // Protocol Response Codes
    // ============================================================================
    
    /**
     * Standard protocol response code.
     * 
     * This value is used in the protocol header to indicate a standard
     * response message (as opposed to a request).
     */
    public static final byte TRACKER_PROTO_RESP = 100;
    
    /**
     * FastDFS protocol response code (alias for TRACKER_PROTO_RESP).
     */
    public static final byte FDFS_PROTO_RESP = TRACKER_PROTO_RESP;
    
    /**
     * FastDFS storage protocol response code (alias for TRACKER_PROTO_RESP).
     */
    public static final byte FDFS_STORAGE_PROTO_RESP = TRACKER_PROTO_RESP;
    
    // ============================================================================
    // Protocol Field Size Limits
    // ============================================================================
    
    /**
     * Maximum length of a storage group name in bytes.
     * 
     * Group names are limited to 16 characters in the FastDFS protocol.
     */
    public static final int FDFS_GROUP_NAME_MAX_LEN = 16;
    
    /**
     * Maximum length of file extension (without dot) in bytes.
     * 
     * File extensions are limited to 6 characters in the FastDFS protocol.
     */
    public static final int FDFS_FILE_EXT_NAME_MAX_LEN = 6;
    
    /**
     * Maximum length of metadata key name in bytes.
     * 
     * Metadata keys are limited to 64 characters in the FastDFS protocol.
     */
    public static final int FDFS_MAX_META_NAME_LEN = 64;
    
    /**
     * Maximum length of metadata value in bytes.
     * 
     * Metadata values are limited to 256 characters in the FastDFS protocol.
     */
    public static final int FDFS_MAX_META_VALUE_LEN = 256;
    
    /**
     * Maximum length of slave file prefix in bytes.
     * 
     * Slave file prefixes (like "thumb", "small") are limited to 16 characters.
     */
    public static final int FDFS_FILE_PREFIX_MAX_LEN = 16;
    
    /**
     * Maximum size of storage server ID in bytes.
     * 
     * Storage server IDs are limited to 16 characters.
     */
    public static final int FDFS_STORAGE_ID_MAX_SIZE = 16;
    
    /**
     * Size of version string field in bytes.
     * 
     * Version strings in protocol messages are fixed at 8 bytes.
     */
    public static final int FDFS_VERSION_SIZE = 8;
    
    /**
     * Size of IP address field in bytes.
     * 
     * IP address fields in protocol messages are fixed at 16 bytes to
     * support both IPv4 and IPv6 addresses (though IPv6 is not commonly used).
     */
    public static final int IP_ADDRESS_SIZE = 16;
    
    // ============================================================================
    // Protocol Separators
    // ============================================================================
    
    /**
     * Record separator character used in metadata encoding.
     * 
     * This character (0x01) separates different key-value pairs in the
     * metadata wire format.
     */
    public static final byte FDFS_RECORD_SEPARATOR = 0x01;
    
    /**
     * Field separator character used in metadata encoding.
     * 
     * This character (0x02) separates keys from values in the metadata
     * wire format.
     */
    public static final byte FDFS_FIELD_SEPARATOR = 0x02;
    
    // ============================================================================
    // Protocol Header Size
    // ============================================================================
    
    /**
     * Size of protocol header in bytes.
     * 
     * Every FastDFS protocol message starts with a 10-byte header:
     * - 8 bytes: body length (big-endian uint64)
     * - 1 byte: command code
     * - 1 byte: status code
     */
    public static final int FDFS_PROTO_HEADER_LEN = 10;
    
    // ============================================================================
    // Storage Server Status Codes
    // ============================================================================
    
    /**
     * Storage server status: Initializing.
     * 
     * The storage server is in the process of initializing and is not
     * yet ready to accept requests.
     */
    public static final byte FDFS_STORAGE_STATUS_INIT = 0;
    
    /**
     * Storage server status: Waiting for synchronization.
     * 
     * The storage server is waiting for file synchronization to complete
     * before it can accept upload requests.
     */
    public static final byte FDFS_STORAGE_STATUS_WAIT_SYNC = 1;
    
    /**
     * Storage server status: Synchronizing.
     * 
     * The storage server is currently synchronizing files with other
     * servers in the group.
     */
    public static final byte FDFS_STORAGE_STATUS_SYNCING = 2;
    
    /**
     * Storage server status: IP address changed.
     * 
     * The storage server has detected that its IP address has changed
     * and is reporting this to the tracker.
     */
    public static final byte FDFS_STORAGE_STATUS_IP_CHANGED = 3;
    
    /**
     * Storage server status: Deleted.
     * 
     * The storage server has been marked for deletion and will be removed
     * from the cluster.
     */
    public static final byte FDFS_STORAGE_STATUS_DELETED = 4;
    
    /**
     * Storage server status: Offline.
     * 
     * The storage server is offline and not accepting requests.
     */
    public static final byte FDFS_STORAGE_STATUS_OFFLINE = 5;
    
    /**
     * Storage server status: Online.
     * 
     * The storage server is online and ready to accept requests.
     */
    public static final byte FDFS_STORAGE_STATUS_ONLINE = 6;
    
    /**
     * Storage server status: Active.
     * 
     * The storage server is active and fully operational.
     */
    public static final byte FDFS_STORAGE_STATUS_ACTIVE = 7;
    
    /**
     * Storage server status: Recovery.
     * 
     * The storage server is in recovery mode, attempting to restore
     * its state after a failure.
     */
    public static final byte FDFS_STORAGE_STATUS_RECOVERY = 9;
    
    /**
     * Storage server status: None.
     * 
     * No status information is available for the storage server.
     */
    public static final byte FDFS_STORAGE_STATUS_NONE = 99;
    
}

