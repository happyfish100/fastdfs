/**
 * FastDFS Protocol Types and Constants
 * 
 * This module defines all protocol-level constants, command codes, and data structures
 * used in communication with FastDFS tracker and storage servers.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

'use strict';

// ============================================================================
// Protocol Constants
// ============================================================================

/**
 * Default port for tracker servers
 * @constant {number}
 */
const TRACKER_DEFAULT_PORT = 22122;

/**
 * Default port for storage servers
 * @constant {number}
 */
const STORAGE_DEFAULT_PORT = 23000;

/**
 * Protocol header size in bytes (8 bytes length + 1 byte cmd + 1 byte status)
 * @constant {number}
 */
const FDFS_PROTO_HEADER_LEN = 10;

// ============================================================================
// Field Size Limits
// ============================================================================

/**
 * Maximum length of group name
 * @constant {number}
 */
const FDFS_GROUP_NAME_MAX_LEN = 16;

/**
 * Maximum length of file extension name
 * @constant {number}
 */
const FDFS_FILE_EXT_NAME_MAX_LEN = 6;

/**
 * Maximum length of metadata name (key)
 * @constant {number}
 */
const FDFS_MAX_META_NAME_LEN = 64;

/**
 * Maximum length of metadata value
 * @constant {number}
 */
const FDFS_MAX_META_VALUE_LEN = 256;

/**
 * Maximum length of file prefix (for slave files)
 * @constant {number}
 */
const FDFS_FILE_PREFIX_MAX_LEN = 16;

/**
 * Maximum size of storage ID
 * @constant {number}
 */
const FDFS_STORAGE_ID_MAX_SIZE = 16;

/**
 * Size of version string
 * @constant {number}
 */
const FDFS_VERSION_SIZE = 8;

/**
 * Size of IP address field
 * @constant {number}
 */
const IP_ADDRESS_SIZE = 16;

// ============================================================================
// Protocol Separators
// ============================================================================

/**
 * Record separator character (used between metadata entries)
 * @constant {number}
 */
const FDFS_RECORD_SEPARATOR = 0x01;

/**
 * Field separator character (used between key and value in metadata)
 * @constant {number}
 */
const FDFS_FIELD_SEPARATOR = 0x02;

// ============================================================================
// Tracker Protocol Commands
// ============================================================================

/**
 * Tracker protocol command codes
 * 
 * These commands are sent to tracker servers to query for storage servers
 * or retrieve cluster information.
 * 
 * @enum {number}
 */
const TrackerCommand = {
  /** Query storage server for upload without specifying group */
  SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE: 101,
  
  /** Query storage server for download/fetch */
  SERVICE_QUERY_FETCH_ONE: 102,
  
  /** Query storage server for update operations */
  SERVICE_QUERY_UPDATE: 103,
  
  /** Query storage server for upload with specified group */
  SERVICE_QUERY_STORE_WITH_GROUP_ONE: 104,
  
  /** Query all storage servers for fetch */
  SERVICE_QUERY_FETCH_ALL: 105,
  
  /** Query all storage servers for upload without group */
  SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL: 106,
  
  /** Query all storage servers for upload with group */
  SERVICE_QUERY_STORE_WITH_GROUP_ALL: 107,
  
  /** List servers in one group */
  SERVER_LIST_ONE_GROUP: 90,
  
  /** List all groups */
  SERVER_LIST_ALL_GROUPS: 91,
  
  /** List storage servers */
  SERVER_LIST_STORAGE: 92,
  
  /** Delete storage server */
  SERVER_DELETE_STORAGE: 93,
  
  /** Storage server reports IP change */
  STORAGE_REPORT_IP_CHANGED: 94,
  
  /** Storage server reports status */
  STORAGE_REPORT_STATUS: 95,
  
  /** Storage server reports disk usage */
  STORAGE_REPORT_DISK_USAGE: 96,
  
  /** Storage server sync timestamp */
  STORAGE_SYNC_TIMESTAMP: 97,
  
  /** Storage server sync report */
  STORAGE_SYNC_REPORT: 98,
};

// ============================================================================
// Storage Protocol Commands
// ============================================================================

/**
 * Storage protocol command codes
 * 
 * These commands are sent to storage servers to perform file operations.
 * 
 * @enum {number}
 */
const StorageCommand = {
  /** Upload a regular file */
  UPLOAD_FILE: 11,
  
  /** Delete a file */
  DELETE_FILE: 12,
  
  /** Set file metadata */
  SET_METADATA: 13,
  
  /** Download a file */
  DOWNLOAD_FILE: 14,
  
  /** Get file metadata */
  GET_METADATA: 15,
  
  /** Upload a slave file (thumbnail, etc.) */
  UPLOAD_SLAVE_FILE: 21,
  
  /** Query file information */
  QUERY_FILE_INFO: 22,
  
  /** Upload an appender file */
  UPLOAD_APPENDER_FILE: 23,
  
  /** Append data to an appender file */
  APPEND_FILE: 24,
  
  /** Modify content of an appender file */
  MODIFY_FILE: 34,
  
  /** Truncate an appender file */
  TRUNCATE_FILE: 36,
};

// ============================================================================
// Storage Server Status
// ============================================================================

/**
 * Storage server status codes
 * 
 * These codes indicate the current state of a storage server.
 * 
 * @enum {number}
 */
const StorageStatus = {
  /** Server is initializing */
  INIT: 0,
  
  /** Server is waiting for sync */
  WAIT_SYNC: 1,
  
  /** Server is syncing */
  SYNCING: 2,
  
  /** Server IP has changed */
  IP_CHANGED: 3,
  
  /** Server has been deleted */
  DELETED: 4,
  
  /** Server is offline */
  OFFLINE: 5,
  
  /** Server is online */
  ONLINE: 6,
  
  /** Server is active and ready */
  ACTIVE: 7,
  
  /** Server is in recovery mode */
  RECOVERY: 9,
  
  /** No status / unknown */
  NONE: 99,
};

// ============================================================================
// Metadata Operation Flags
// ============================================================================

/**
 * Metadata operation flags
 * 
 * These flags control how metadata is set on a file.
 * 
 * @enum {number}
 */
const MetadataFlag = {
  /** Overwrite all existing metadata */
  OVERWRITE: 0x4f, // 'O'
  
  /** Merge with existing metadata */
  MERGE: 0x4d,     // 'M'
};

// ============================================================================
// Type Definitions (JSDoc)
// ============================================================================

/**
 * Information about a file stored in FastDFS
 * 
 * @typedef {Object} FileInfo
 * @property {number} fileSize - Size of the file in bytes
 * @property {Date} createTime - Timestamp when the file was created
 * @property {number} crc32 - CRC32 checksum of the file
 * @property {string} sourceIpAddr - IP address of the source storage server
 */

/**
 * Represents a storage server in the FastDFS cluster
 * 
 * @typedef {Object} StorageServer
 * @property {string} ipAddr - IP address of the storage server
 * @property {number} port - Port number of the storage server
 * @property {number} storePathIndex - Index of the storage path to use (0-based)
 */

/**
 * FastDFS protocol header (10 bytes)
 * 
 * @typedef {Object} TrackerHeader
 * @property {number} length - Length of the message body (not including header)
 * @property {number} cmd - Command code (request type or response type)
 * @property {number} status - Status code (0 for success, error code otherwise)
 */

/**
 * Response from an upload operation
 * 
 * @typedef {Object} UploadResponse
 * @property {string} groupName - Storage group where the file was stored
 * @property {string} remoteFilename - Path and filename on the storage server
 */

/**
 * Client configuration options
 * 
 * @typedef {Object} ClientConfig
 * @property {string[]} trackerAddrs - List of tracker server addresses in format "host:port"
 * @property {number} [maxConns=10] - Maximum number of connections per tracker server
 * @property {number} [connectTimeout=5000] - Timeout for establishing connections in milliseconds
 * @property {number} [networkTimeout=30000] - Timeout for network I/O operations in milliseconds
 * @property {number} [idleTimeout=60000] - Timeout for idle connections in the pool in milliseconds
 * @property {number} [retryCount=3] - Number of retries for failed operations
 */

/**
 * Metadata dictionary type
 * 
 * @typedef {Object.<string, string>} Metadata
 */

// ============================================================================
// Exports
// ============================================================================

module.exports = {
  // Protocol Constants
  TRACKER_DEFAULT_PORT,
  STORAGE_DEFAULT_PORT,
  FDFS_PROTO_HEADER_LEN,
  
  // Field Size Limits
  FDFS_GROUP_NAME_MAX_LEN,
  FDFS_FILE_EXT_NAME_MAX_LEN,
  FDFS_MAX_META_NAME_LEN,
  FDFS_MAX_META_VALUE_LEN,
  FDFS_FILE_PREFIX_MAX_LEN,
  FDFS_STORAGE_ID_MAX_SIZE,
  FDFS_VERSION_SIZE,
  IP_ADDRESS_SIZE,
  
  // Protocol Separators
  FDFS_RECORD_SEPARATOR,
  FDFS_FIELD_SEPARATOR,
  
  // Command Enums
  TrackerCommand,
  StorageCommand,
  StorageStatus,
  MetadataFlag,
};
