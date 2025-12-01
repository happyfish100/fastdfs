package org.fastdfs.client.protocol;

/**
 * FastDFS Protocol Constants.
 * 
 * This class defines all protocol-level constants used in communication
 * with FastDFS tracker and storage servers.
 */
public final class ProtocolConstants {
    
    // Protocol Ports
    public static final int TRACKER_DEFAULT_PORT = 22122;
    public static final int STORAGE_DEFAULT_PORT = 23000;
    
    // Protocol Header Size
    public static final int FDFS_PROTO_HEADER_LEN = 10; // 8 bytes length + 1 byte cmd + 1 byte status
    
    // Field Size Limits
    public static final int FDFS_GROUP_NAME_MAX_LEN = 16;
    public static final int FDFS_FILE_EXT_NAME_MAX_LEN = 6;
    public static final int FDFS_MAX_META_NAME_LEN = 64;
    public static final int FDFS_MAX_META_VALUE_LEN = 256;
    public static final int FDFS_FILE_PREFIX_MAX_LEN = 16;
    public static final int FDFS_STORAGE_ID_MAX_SIZE = 16;
    public static final int FDFS_VERSION_SIZE = 8;
    public static final int IP_ADDRESS_SIZE = 16;
    
    // Protocol Separators
    public static final byte FDFS_RECORD_SEPARATOR = 0x01;
    public static final byte FDFS_FIELD_SEPARATOR = 0x02;
    
    // Tracker Protocol Commands
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE = 101;
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE = 102;
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE = 103;
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ONE = 104;
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ALL = 105;
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL = 106;
    public static final byte TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ALL = 107;
    public static final byte TRACKER_PROTO_CMD_SERVER_LIST_ONE_GROUP = 90;
    public static final byte TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS = 91;
    public static final byte TRACKER_PROTO_CMD_SERVER_LIST_STORAGE = 92;
    public static final byte TRACKER_PROTO_CMD_SERVER_DELETE_STORAGE = 93;
    public static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_IP_CHANGED = 94;
    public static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_STATUS = 95;
    public static final byte TRACKER_PROTO_CMD_STORAGE_REPORT_DISK_USAGE = 96;
    public static final byte TRACKER_PROTO_CMD_STORAGE_SYNC_TIMESTAMP = 97;
    public static final byte TRACKER_PROTO_CMD_STORAGE_SYNC_REPORT = 98;
    
    // Storage Protocol Commands
    public static final byte STORAGE_PROTO_CMD_UPLOAD_FILE = 11;
    public static final byte STORAGE_PROTO_CMD_DELETE_FILE = 12;
    public static final byte STORAGE_PROTO_CMD_SET_METADATA = 13;
    public static final byte STORAGE_PROTO_CMD_DOWNLOAD_FILE = 14;
    public static final byte STORAGE_PROTO_CMD_GET_METADATA = 15;
    public static final byte STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE = 21;
    public static final byte STORAGE_PROTO_CMD_QUERY_FILE_INFO = 22;
    public static final byte STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE = 23;
    public static final byte STORAGE_PROTO_CMD_APPEND_FILE = 24;
    public static final byte STORAGE_PROTO_CMD_MODIFY_FILE = 34;
    public static final byte STORAGE_PROTO_CMD_TRUNCATE_FILE = 36;
    
    // Storage Server Status
    public static final byte STORAGE_STATUS_INIT = 0;
    public static final byte STORAGE_STATUS_WAIT_SYNC = 1;
    public static final byte STORAGE_STATUS_SYNCING = 2;
    public static final byte STORAGE_STATUS_IP_CHANGED = 3;
    public static final byte STORAGE_STATUS_DELETED = 4;
    public static final byte STORAGE_STATUS_OFFLINE = 5;
    public static final byte STORAGE_STATUS_ONLINE = 6;
    public static final byte STORAGE_STATUS_ACTIVE = 7;
    public static final byte STORAGE_STATUS_RECOVERY = 9;
    public static final byte STORAGE_STATUS_NONE = 99;
    
    // Metadata Operation Flags
    public static final byte METADATA_FLAG_OVERWRITE = 0x4F; // 'O'
    public static final byte METADATA_FLAG_MERGE = 0x4D;     // 'M'
    
    // Error Status Codes
    public static final byte STATUS_SUCCESS = 0;
    public static final byte STATUS_FILE_NOT_FOUND = 2;
    public static final byte STATUS_FILE_ALREADY_EXISTS = 6;
    public static final byte STATUS_INVALID_ARGUMENT = 22;
    public static final byte STATUS_INSUFFICIENT_SPACE = 28;
    
    private ProtocolConstants() {
        // Prevent instantiation
    }
}
