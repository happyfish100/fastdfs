/**
 * FastDFS Protocol Types and Constants
 * 
 * This file defines all protocol-level constants, command codes, and data structures
 * used in communication with FastDFS tracker and storage servers.
 * 
 * These constants must match the values defined in the FastDFS C implementation
 * to ensure protocol compatibility.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.types

import java.time.LocalDateTime

/**
 * Protocol constants for FastDFS communication.
 * 
 * These constants define the protocol structure, command codes, field sizes,
 * and other protocol-level details.
 */
class ProtocolConstants {
    
    // ============================================================================
    // Default Network Ports
    // ============================================================================
    
    /**
     * Default port for tracker servers.
     * 
     * This is the standard port used by FastDFS tracker servers.
     * It can be overridden in the tracker configuration.
     */
    static final int TRACKER_DEFAULT_PORT = 22122
    
    /**
     * Default port for storage servers.
     * 
     * This is the standard port used by FastDFS storage servers.
     * It can be overridden in the storage configuration.
     */
    static final int STORAGE_DEFAULT_PORT = 23000
    
    // ============================================================================
    // Tracker Protocol Commands
    // ============================================================================
    
    /**
     * Query storage server for upload without group.
     * 
     * Command code: 101
     * Used to get a storage server for uploading files when no specific
     * group is specified. The tracker will select an appropriate group.
     */
    static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE = 101
    
    /**
     * Query storage server for download.
     * 
     * Command code: 102
     * Used to get a storage server that has the specified file for downloading.
     */
    static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE = 102
    
    /**
     * Query storage server for update.
     * 
     * Command code: 103
     * Used to get a storage server for updating file metadata or content.
     */
    static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE = 103
    
    /**
     * Query storage server for upload with group.
     * 
     * Command code: 104
     * Used to get a storage server for uploading files to a specific group.
     */
    static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ONE = 104
    
    /**
     * Query all storage servers for download.
     * 
     * Command code: 105
     * Used to get all storage servers that have the specified file.
     */
    static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ALL = 105
    
    /**
     * Query all storage servers for upload without group.
     * 
     * Command code: 106
     * Used to get all available storage servers for uploading.
     */
    static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL = 106
    
    /**
     * Query all storage servers for upload with group.
     * 
     * Command code: 107
     * Used to get all storage servers in a specific group for uploading.
     */
    static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ALL = 107
    
    /**
     * List servers in one group.
     * 
     * Command code: 90
     * Used to get information about all servers in a specific group.
     */
    static final byte TRACKER_PROTO_CMD_SERVER_LIST_ONE_GROUP = 90
    
    /**
     * List servers in all groups.
     * 
     * Command code: 91
     * Used to get information about all servers in all groups.
     */
    static final byte TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS = 91
    
    /**
     * List storage servers.
     * 
     * Command code: 92
     * Used to get a list of all storage servers.
     */
    static final byte TRACKER_PROTO_CMD_SERVER_LIST_STORAGE = 92
    
    /**
     * Delete storage server.
     * 
     * Command code: 93
     * Used to remove a storage server from the cluster.
     */
    static final byte TRACKER_PROTO_CMD_SERVER_DELETE_STORAGE = 93
    
    /**
     * Storage report IP changed.
     * 
     * Command code: 94
     * Used by storage servers to report IP address changes.
     */
    static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_IP_CHANGED = 94
    
    /**
     * Storage report status.
     * 
     * Command code: 95
     * Used by storage servers to report their current status.
     */
    static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_STATUS = 95
    
    /**
     * Storage report disk usage.
     * 
     * Command code: 96
     * Used by storage servers to report disk usage statistics.
     */
    static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_DISK_USAGE = 96
    
    /**
     * Storage sync timestamp.
     * 
     * Command code: 97
     * Used for synchronization timestamp management.
     */
    static final byte TRACKER_PROTO_CMD_STORAGE_SYNC_TIMESTAMP = 97
    
    /**
     * Storage sync report.
     * 
     * Command code: 98
     * Used by storage servers to report synchronization status.
     */
    static final byte TRACKER_PROTO_CMD_STORAGE_SYNC_REPORT = 98
    
    // ============================================================================
    // Storage Protocol Commands
    // ============================================================================
    
    /**
     * Upload a regular file.
     * 
     * Command code: 11
     * Used to upload a normal file that cannot be modified after upload.
     */
    static final byte STORAGE_PROTO_CMD_UPLOAD_FILE = 11
    
    /**
     * Delete a file.
     * 
     * Command code: 12
     * Used to delete a file from the storage server.
     */
    static final byte STORAGE_PROTO_CMD_DELETE_FILE = 12
    
    /**
     * Set file metadata.
     * 
     * Command code: 13
     * Used to set or update metadata associated with a file.
     */
    static final byte STORAGE_PROTO_CMD_SET_METADATA = 13
    
    /**
     * Download a file.
     * 
     * Command code: 14
     * Used to download a file from the storage server.
     */
    static final byte STORAGE_PROTO_CMD_DOWNLOAD_FILE = 14
    
    /**
     * Get file metadata.
     * 
     * Command code: 15
     * Used to retrieve metadata associated with a file.
     */
    static final byte STORAGE_PROTO_CMD_GET_METADATA = 15
    
    /**
     * Upload a slave file.
     * 
     * Command code: 21
     * Used to upload a slave file (e.g., thumbnail) associated with a master file.
     */
    static final byte STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE = 21
    
    /**
     * Query file information.
     * 
     * Command code: 22
     * Used to get detailed information about a file (size, timestamps, CRC32, etc.).
     */
    static final byte STORAGE_PROTO_CMD_QUERY_FILE_INFO = 22
    
    /**
     * Upload an appender file.
     * 
     * Command code: 23
     * Used to upload a file that can be modified after upload (append, modify, truncate).
     */
    static final byte STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE = 23
    
    /**
     * Append data to an appender file.
     * 
     * Command code: 24
     * Used to append data to the end of an appender file.
     */
    static final byte STORAGE_PROTO_CMD_APPEND_FILE = 24
    
    /**
     * Modify content of an appender file.
     * 
     * Command code: 34
     * Used to overwrite content at a specific offset in an appender file.
     */
    static final byte STORAGE_PROTO_CMD_MODIFY_FILE = 34
    
    /**
     * Truncate an appender file.
     * 
     * Command code: 36
     * Used to truncate an appender file to a specific size.
     */
    static final byte STORAGE_PROTO_CMD_TRUNCATE_FILE = 36
    
    // ============================================================================
    // Protocol Response Codes
    // ============================================================================
    
    /**
     * Standard protocol response code.
     * 
     * This is the standard response code used in protocol headers.
     * Status byte 0 indicates success, non-zero indicates an error.
     */
    static final byte FDFS_PROTO_RESP = 100
    
    /**
     * Tracker protocol response code.
     * 
     * Same as FDFS_PROTO_RESP, used for tracker responses.
     */
    static final byte TRACKER_PROTO_RESP = FDFS_PROTO_RESP
    
    /**
     * Storage protocol response code.
     * 
     * Same as FDFS_PROTO_RESP, used for storage responses.
     */
    static final byte FDFS_STORAGE_PROTO_RESP = FDFS_PROTO_RESP
    
    // ============================================================================
    // Protocol Field Size Limits
    // ============================================================================
    
    /**
     * Maximum length of a storage group name.
     * 
     * Group names are limited to 16 characters in the FastDFS protocol.
     */
    static final int FDFS_GROUP_NAME_MAX_LEN = 16
    
    /**
     * Maximum length of file extension (without dot).
     * 
     * File extensions are limited to 6 characters in the FastDFS protocol.
     */
    static final int FDFS_FILE_EXT_NAME_MAX_LEN = 6
    
    /**
     * Maximum length of metadata key name.
     * 
     * Metadata keys are limited to 64 characters.
     */
    static final int FDFS_MAX_META_NAME_LEN = 64
    
    /**
     * Maximum length of metadata value.
     * 
     * Metadata values are limited to 256 characters.
     */
    static final int FDFS_MAX_META_VALUE_LEN = 256
    
    /**
     * Maximum length of slave file prefix.
     * 
     * Slave file prefixes are limited to 16 characters.
     */
    static final int FDFS_FILE_PREFIX_MAX_LEN = 16
    
    /**
     * Maximum size of storage server ID.
     * 
     * Storage server IDs are limited to 16 bytes.
     */
    static final int FDFS_STORAGE_ID_MAX_SIZE = 16
    
    /**
     * Size of version string field.
     * 
     * Version strings in protocol messages are 8 bytes.
     */
    static final int FDFS_VERSION_SIZE = 8
    
    /**
     * Size of IP address field.
     * 
     * IP addresses in protocol messages are 16 bytes (supports IPv4 and IPv6).
     */
    static final int IP_ADDRESS_SIZE = 16
    
    // ============================================================================
    // Protocol Separators
    // ============================================================================
    
    /**
     * Record separator for metadata encoding.
     * 
     * Used to separate different key-value pairs in metadata.
     * Value: 0x01
     */
    static final byte FDFS_RECORD_SEPARATOR = 0x01
    
    /**
     * Field separator for metadata encoding.
     * 
     * Used to separate key from value in metadata key-value pairs.
     * Value: 0x02
     */
    static final byte FDFS_FIELD_SEPARATOR = 0x02
    
    // ============================================================================
    // Protocol Header
    // ============================================================================
    
    /**
     * Size of protocol header in bytes.
     * 
     * Protocol header structure:
     * - 8 bytes: body length (big-endian long)
     * - 1 byte: command code
     * - 1 byte: status code
     * Total: 10 bytes
     */
    static final int FDFS_PROTO_HEADER_LEN = 10
    
    // ============================================================================
    // Storage Server Status Codes
    // ============================================================================
    
    /**
     * Storage server is initializing.
     * 
     * Status code: 0
     * The server is starting up and not yet ready.
     */
    static final byte FDFS_STORAGE_STATUS_INIT = 0
    
    /**
     * Storage server is waiting for synchronization.
     * 
     * Status code: 1
     * The server is waiting for file synchronization to complete.
     */
    static final byte FDFS_STORAGE_STATUS_WAIT_SYNC = 1
    
    /**
     * Storage server is synchronizing files.
     * 
     * Status code: 2
     * The server is actively synchronizing files with other servers.
     */
    static final byte FDFS_STORAGE_STATUS_SYNCING = 2
    
    /**
     * Storage server IP address has changed.
     * 
     * Status code: 3
     * The server's IP address has changed and needs to be updated.
     */
    static final byte FDFS_STORAGE_STATUS_IP_CHANGED = 3
    
    /**
     * Storage server has been deleted.
     * 
     * Status code: 4
     * The server has been removed from the cluster.
     */
    static final byte FDFS_STORAGE_STATUS_DELETED = 4
    
    /**
     * Storage server is offline.
     * 
     * Status code: 5
     * The server is not available.
     */
    static final byte FDFS_STORAGE_STATUS_OFFLINE = 5
    
    /**
     * Storage server is online.
     * 
     * Status code: 6
     * The server is available and operational.
     */
    static final byte FDFS_STORAGE_STATUS_ONLINE = 6
    
    /**
     * Storage server is active.
     * 
     * Status code: 7
     * The server is active and ready to handle requests.
     */
    static final byte FDFS_STORAGE_STATUS_ACTIVE = 7
    
    /**
     * Storage server is in recovery mode.
     * 
     * Status code: 9
     * The server is recovering from a failure.
     */
    static final byte FDFS_STORAGE_STATUS_RECOVERY = 9
    
    /**
     * No status information available.
     * 
     * Status code: 99
     * Status information is not available or unknown.
     */
    static final byte FDFS_STORAGE_STATUS_NONE = 99
}

/**
 * Metadata operation flag.
 * 
 * Controls how metadata is updated when setting metadata for a file.
 */
enum MetadataFlag {
    /**
     * Overwrite mode.
     * 
     * Completely replaces all existing metadata with new values.
     * Any existing metadata keys not in the new set will be removed.
     * 
     * Protocol value: 'O' (0x4F)
     */
    OVERWRITE('O' as byte),
    
    /**
     * Merge mode.
     * 
     * Merges new metadata with existing metadata.
     * Existing keys are updated, new keys are added, and unspecified keys are kept.
     * 
     * Protocol value: 'M' (0x4D)
     */
    MERGE('M' as byte)
    
    /**
     * The protocol byte value for this flag.
     */
    final byte value
    
    /**
     * Constructor.
     * 
     * @param value the protocol byte value
     */
    MetadataFlag(byte value) {
        this.value = value
    }
    
    /**
     * Gets the protocol byte value.
     * 
     * @return the byte value
     */
    byte getValue() {
        return value
    }
}

/**
 * File information structure.
 * 
 * Contains detailed information about a file stored in FastDFS.
 * This information is returned by getFileInfo operations.
 */
class FileInfo {
    /**
     * File size in bytes.
     * 
     * The total size of the file as stored on the storage server.
     */
    long fileSize
    
    /**
     * File creation time.
     * 
     * The timestamp when the file was created/uploaded.
     */
    LocalDateTime createTime
    
    /**
     * CRC32 checksum of the file.
     * 
     * Used for integrity verification.
     */
    long crc32
    
    /**
     * Source IP address.
     * 
     * The IP address of the storage server where the file is stored.
     */
    String sourceIPAddr
    
    /**
     * Default constructor.
     */
    FileInfo() {
    }
    
    /**
     * Constructor with all fields.
     * 
     * @param fileSize the file size
     * @param createTime the creation time
     * @param crc32 the CRC32 checksum
     * @param sourceIPAddr the source IP address
     */
    FileInfo(long fileSize, LocalDateTime createTime, long crc32, String sourceIPAddr) {
        this.fileSize = fileSize
        this.createTime = createTime
        this.crc32 = crc32
        this.sourceIPAddr = sourceIPAddr
    }
    
    /**
     * Returns a string representation.
     * 
     * @return string representation
     */
    @Override
    String toString() {
        return "FileInfo{" +
            "fileSize=" + fileSize +
            ", createTime=" + createTime +
            ", crc32=" + crc32 +
            ", sourceIPAddr='" + sourceIPAddr + '\'' +
            '}'
    }
}

/**
 * Storage server information.
 * 
 * Represents a storage server in the FastDFS cluster.
 * This information is returned by the tracker when querying for upload or download.
 */
class StorageServer {
    /**
     * IP address of the storage server.
     */
    String ipAddr
    
    /**
     * Port number of the storage server.
     */
    int port
    
    /**
     * Store path index.
     * 
     * Index of the storage path to use (0-based).
     * Storage servers can have multiple storage paths.
     */
    byte storePathIndex
    
    /**
     * Default constructor.
     */
    StorageServer() {
    }
    
    /**
     * Constructor with all fields.
     * 
     * @param ipAddr the IP address
     * @param port the port number
     * @param storePathIndex the store path index
     */
    StorageServer(String ipAddr, int port, byte storePathIndex) {
        this.ipAddr = ipAddr
        this.port = port
        this.storePathIndex = storePathIndex
    }
    
    /**
     * Returns the server address as "host:port".
     * 
     * @return the server address
     */
    String getAddress() {
        return "${ipAddr}:${port}"
    }
    
    /**
     * Returns a string representation.
     * 
     * @return string representation
     */
    @Override
    String toString() {
        return "StorageServer{" +
            "ipAddr='" + ipAddr + '\'' +
            ", port=" + port +
            ", storePathIndex=" + storePathIndex +
            '}'
    }
}

/**
 * Protocol header structure.
 * 
 * Every message between client and server starts with this 10-byte header.
 */
class ProtocolHeader {
    /**
     * Length of the message body (not including header).
     * 
     * This is a 64-bit big-endian integer (8 bytes).
     */
    long length
    
    /**
     * Command code (request type or response type).
     * 
     * This is a single byte indicating the operation.
     */
    byte cmd
    
    /**
     * Status code.
     * 
     * 0 for success, error code otherwise.
     * This is a single byte.
     */
    byte status
    
    /**
     * Default constructor.
     */
    ProtocolHeader() {
    }
    
    /**
     * Constructor with all fields.
     * 
     * @param length the body length
     * @param cmd the command code
     * @param status the status code
     */
    ProtocolHeader(long length, byte cmd, byte status) {
        this.length = length
        this.cmd = cmd
        this.status = status
    }
    
    /**
     * Returns a string representation.
     * 
     * @return string representation
     */
    @Override
    String toString() {
        return "ProtocolHeader{" +
            "length=" + length +
            ", cmd=" + cmd +
            ", status=" + status +
            '}'
    }
}

/**
 * Upload response structure.
 * 
 * Represents the response from an upload operation.
 * The server returns the group name and remote filename which together form the file ID.
 */
class UploadResponse {
    /**
     * Storage group where the file was stored.
     * 
     * This is part of the file ID.
     */
    String groupName
    
    /**
     * Path and filename on the storage server.
     * 
     * This is part of the file ID.
     * The full file ID is: groupName + "/" + remoteFilename
     */
    String remoteFilename
    
    /**
     * Default constructor.
     */
    UploadResponse() {
    }
    
    /**
     * Constructor with all fields.
     * 
     * @param groupName the group name
     * @param remoteFilename the remote filename
     */
    UploadResponse(String groupName, String remoteFilename) {
        this.groupName = groupName
        this.remoteFilename = remoteFilename
    }
    
    /**
     * Returns the full file ID.
     * 
     * @return the file ID (groupName/remoteFilename)
     */
    String getFileId() {
        return "${groupName}/${remoteFilename}"
    }
    
    /**
     * Returns a string representation.
     * 
     * @return string representation
     */
    @Override
    String toString() {
        return "UploadResponse{" +
            "groupName='" + groupName + '\'' +
            ", remoteFilename='" + remoteFilename + '\'' +
            ", fileId='" + getFileId() + '\'' +
            '}'
    }
}

