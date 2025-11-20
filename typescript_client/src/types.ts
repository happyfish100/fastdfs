/**
 * FastDFS Protocol Types and Constants
 * 
 * This module defines all protocol-level constants, command codes, and data structures
 * used in communication with FastDFS tracker and storage servers.
 */

// Protocol Constants
export const TRACKER_DEFAULT_PORT = 22122;
export const STORAGE_DEFAULT_PORT = 23000;

// Protocol Header Size
export const FDFS_PROTO_HEADER_LEN = 10; // 8 bytes length + 1 byte cmd + 1 byte status

// Field Size Limits
export const FDFS_GROUP_NAME_MAX_LEN = 16;
export const FDFS_FILE_EXT_NAME_MAX_LEN = 6;
export const FDFS_MAX_META_NAME_LEN = 64;
export const FDFS_MAX_META_VALUE_LEN = 256;
export const FDFS_FILE_PREFIX_MAX_LEN = 16;
export const FDFS_STORAGE_ID_MAX_SIZE = 16;
export const FDFS_VERSION_SIZE = 8;
export const IP_ADDRESS_SIZE = 16;

// Protocol Separators
export const FDFS_RECORD_SEPARATOR = 0x01;
export const FDFS_FIELD_SEPARATOR = 0x02;

/**
 * Tracker protocol commands
 */
export enum TrackerCommand {
  SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE = 101,
  SERVICE_QUERY_FETCH_ONE = 102,
  SERVICE_QUERY_UPDATE = 103,
  SERVICE_QUERY_STORE_WITH_GROUP_ONE = 104,
  SERVICE_QUERY_FETCH_ALL = 105,
  SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL = 106,
  SERVICE_QUERY_STORE_WITH_GROUP_ALL = 107,
  SERVER_LIST_ONE_GROUP = 90,
  SERVER_LIST_ALL_GROUPS = 91,
  SERVER_LIST_STORAGE = 92,
  SERVER_DELETE_STORAGE = 93,
  STORAGE_REPORT_IP_CHANGED = 94,
  STORAGE_REPORT_STATUS = 95,
  STORAGE_REPORT_DISK_USAGE = 96,
  STORAGE_SYNC_TIMESTAMP = 97,
  STORAGE_SYNC_REPORT = 98,
}

/**
 * Storage protocol commands
 */
export enum StorageCommand {
  UPLOAD_FILE = 11,
  DELETE_FILE = 12,
  SET_METADATA = 13,
  DOWNLOAD_FILE = 14,
  GET_METADATA = 15,
  UPLOAD_SLAVE_FILE = 21,
  QUERY_FILE_INFO = 22,
  UPLOAD_APPENDER_FILE = 23,
  APPEND_FILE = 24,
  MODIFY_FILE = 34,
  TRUNCATE_FILE = 36,
}

/**
 * Storage server status codes
 */
export enum StorageStatus {
  INIT = 0,
  WAIT_SYNC = 1,
  SYNCING = 2,
  IP_CHANGED = 3,
  DELETED = 4,
  OFFLINE = 5,
  ONLINE = 6,
  ACTIVE = 7,
  RECOVERY = 9,
  NONE = 99,
}

/**
 * Metadata operation flags
 */
export enum MetadataFlag {
  OVERWRITE = 0x4f, // 'O'
  MERGE = 0x4d,     // 'M'
}

/**
 * Information about a file stored in FastDFS
 */
export interface FileInfo {
  /** Size of the file in bytes */
  fileSize: number;
  /** Timestamp when the file was created */
  createTime: Date;
  /** CRC32 checksum of the file */
  crc32: number;
  /** IP address of the source storage server */
  sourceIpAddr: string;
}

/**
 * Represents a storage server in the FastDFS cluster
 */
export interface StorageServer {
  /** IP address of the storage server */
  ipAddr: string;
  /** Port number of the storage server */
  port: number;
  /** Index of the storage path to use (0-based) */
  storePathIndex: number;
}

/**
 * FastDFS protocol header (10 bytes)
 */
export interface TrackerHeader {
  /** Length of the message body (not including header) */
  length: number;
  /** Command code (request type or response type) */
  cmd: number;
  /** Status code (0 for success, error code otherwise) */
  status: number;
}

/**
 * Response from an upload operation
 */
export interface UploadResponse {
  /** Storage group where the file was stored */
  groupName: string;
  /** Path and filename on the storage server */
  remoteFilename: string;
}

/**
 * Client configuration options
 */
export interface ClientConfig {
  /** List of tracker server addresses in format "host:port" */
  trackerAddrs: string[];
  /** Maximum number of connections per tracker server (default: 10) */
  maxConns?: number;
  /** Timeout for establishing connections in milliseconds (default: 5000) */
  connectTimeout?: number;
  /** Timeout for network I/O operations in milliseconds (default: 30000) */
  networkTimeout?: number;
  /** Timeout for idle connections in the pool in milliseconds (default: 60000) */
  idleTimeout?: number;
  /** Number of retries for failed operations (default: 3) */
  retryCount?: number;
}

/**
 * Metadata dictionary type
 */
export type Metadata = Record<string, string>;