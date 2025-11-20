//! FastDFS Protocol Types and Constants
//!
//! This module defines all protocol-level constants, command codes, and data structures
//! used in communication with FastDFS tracker and storage servers.

use std::time::SystemTime;

/// Default network ports for FastDFS servers
pub const TRACKER_DEFAULT_PORT: u16 = 22122;
pub const STORAGE_DEFAULT_PORT: u16 = 23000;

/// Protocol header size
pub const FDFS_PROTO_HEADER_LEN: usize = 10;

/// Field size limits
pub const FDFS_GROUP_NAME_MAX_LEN: usize = 16;
pub const FDFS_FILE_EXT_NAME_MAX_LEN: usize = 6;
pub const FDFS_MAX_META_NAME_LEN: usize = 64;
pub const FDFS_MAX_META_VALUE_LEN: usize = 256;
pub const FDFS_FILE_PREFIX_MAX_LEN: usize = 16;
pub const FDFS_STORAGE_ID_MAX_SIZE: usize = 16;
pub const FDFS_VERSION_SIZE: usize = 8;
pub const IP_ADDRESS_SIZE: usize = 16;

/// Protocol separators
pub const FDFS_RECORD_SEPARATOR: u8 = 0x01;
pub const FDFS_FIELD_SEPARATOR: u8 = 0x02;

/// Tracker protocol commands
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum TrackerCommand {
    ServiceQueryStoreWithoutGroupOne = 101,
    ServiceQueryFetchOne = 102,
    ServiceQueryUpdate = 103,
    ServiceQueryStoreWithGroupOne = 104,
    ServiceQueryFetchAll = 105,
    ServiceQueryStoreWithoutGroupAll = 106,
    ServiceQueryStoreWithGroupAll = 107,
    ServerListOneGroup = 90,
    ServerListAllGroups = 91,
    ServerListStorage = 92,
    ServerDeleteStorage = 93,
    StorageReportIpChanged = 94,
    StorageReportStatus = 95,
    StorageReportDiskUsage = 96,
    StorageSyncTimestamp = 97,
    StorageSyncReport = 98,
}

impl From<TrackerCommand> for u8 {
    fn from(cmd: TrackerCommand) -> u8 {
        cmd as u8
    }
}

/// Storage protocol commands
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum StorageCommand {
    UploadFile = 11,
    DeleteFile = 12,
    SetMetadata = 13,
    DownloadFile = 14,
    GetMetadata = 15,
    UploadSlaveFile = 21,
    QueryFileInfo = 22,
    UploadAppenderFile = 23,
    AppendFile = 24,
    ModifyFile = 34,
    TruncateFile = 36,
}

impl From<StorageCommand> for u8 {
    fn from(cmd: StorageCommand) -> u8 {
        cmd as u8
    }
}

/// Storage server status codes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum StorageStatus {
    Init = 0,
    WaitSync = 1,
    Syncing = 2,
    IpChanged = 3,
    Deleted = 4,
    Offline = 5,
    Online = 6,
    Active = 7,
    Recovery = 9,
    None = 99,
}

/// Metadata operation flags
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum MetadataFlag {
    /// Replace all existing metadata with new values
    Overwrite = b'O',
    /// Merge new metadata with existing metadata
    Merge = b'M',
}

impl From<MetadataFlag> for u8 {
    fn from(flag: MetadataFlag) -> u8 {
        flag as u8
    }
}

/// Information about a file stored in FastDFS
#[derive(Debug, Clone)]
pub struct FileInfo {
    /// Size of the file in bytes
    pub file_size: u64,
    /// Timestamp when the file was created
    pub create_time: SystemTime,
    /// CRC32 checksum of the file
    pub crc32: u32,
    /// IP address of the source storage server
    pub source_ip_addr: String,
}

/// Represents a storage server in the FastDFS cluster
#[derive(Debug, Clone)]
pub struct StorageServer {
    /// IP address of the storage server
    pub ip_addr: String,
    /// Port number of the storage server
    pub port: u16,
    /// Index of the storage path to use (0-based)
    pub store_path_index: u8,
}

/// FastDFS protocol header (10 bytes)
#[derive(Debug, Clone)]
pub struct TrackerHeader {
    /// Length of the message body (not including header)
    pub length: u64,
    /// Command code (request type or response type)
    pub cmd: u8,
    /// Status code (0 for success, error code otherwise)
    pub status: u8,
}

/// Response from an upload operation
#[derive(Debug, Clone)]
pub struct UploadResponse {
    /// Storage group where the file was stored
    pub group_name: String,
    /// Path and filename on the storage server
    pub remote_filename: String,
}

/// Client configuration options
#[derive(Debug, Clone)]
pub struct ClientConfig {
    /// List of tracker server addresses in format "host:port"
    pub tracker_addrs: Vec<String>,
    /// Maximum number of connections per tracker server
    pub max_conns: usize,
    /// Timeout for establishing connections in milliseconds
    pub connect_timeout: u64,
    /// Timeout for network I/O operations in milliseconds
    pub network_timeout: u64,
    /// Timeout for idle connections in the pool in milliseconds
    pub idle_timeout: u64,
    /// Number of retries for failed operations
    pub retry_count: usize,
}

impl Default for ClientConfig {
    fn default() -> Self {
        Self {
            tracker_addrs: Vec::new(),
            max_conns: 10,
            connect_timeout: 5000,
            network_timeout: 30000,
            idle_timeout: 60000,
            retry_count: 3,
        }
    }
}

impl ClientConfig {
    /// Creates a new client configuration with tracker addresses
    pub fn new(tracker_addrs: Vec<String>) -> Self {
        Self {
            tracker_addrs,
            ..Default::default()
        }
    }

    /// Sets the maximum number of connections per server
    pub fn with_max_conns(mut self, max_conns: usize) -> Self {
        self.max_conns = max_conns;
        self
    }

    /// Sets the connection timeout in milliseconds
    pub fn with_connect_timeout(mut self, timeout: u64) -> Self {
        self.connect_timeout = timeout;
        self
    }

    /// Sets the network timeout in milliseconds
    pub fn with_network_timeout(mut self, timeout: u64) -> Self {
        self.network_timeout = timeout;
        self
    }

    /// Sets the idle timeout in milliseconds
    pub fn with_idle_timeout(mut self, timeout: u64) -> Self {
        self.idle_timeout = timeout;
        self
    }

    /// Sets the retry count
    pub fn with_retry_count(mut self, count: usize) -> Self {
        self.retry_count = count;
        self
    }
}

/// Metadata dictionary type
pub type Metadata = std::collections::HashMap<String, String>;