// ============================================================================
// FastDFS Protocol Constants
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This file defines all protocol-level constants used in communication with
// FastDFS tracker and storage servers. These constants must match the values
// defined in the FastDFS C implementation to ensure protocol compatibility.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FastDFS.Client
{
    /// <summary>
    /// Constants for FastDFS protocol communication.
    /// 
    /// This class contains all protocol-level constants including command codes,
    /// response codes, field size limits, and other protocol-specific values.
    /// These constants are used throughout the client implementation to build
    /// and parse protocol messages.
    /// 
    /// All values in this class are based on the FastDFS protocol specification
    /// and must match the C implementation exactly to ensure compatibility.
    /// </summary>
    public static class FastDFSConstants
    {
        // ====================================================================
        // Network Port Constants
        // ====================================================================

        /// <summary>
        /// Default port number for FastDFS tracker servers.
        /// Tracker servers coordinate file storage and retrieval operations
        /// by maintaining information about storage servers in the cluster.
        /// </summary>
        public const int TrackerDefaultPort = 22122;

        /// <summary>
        /// Default port number for FastDFS storage servers.
        /// Storage servers are responsible for actually storing and serving
        /// file data in the FastDFS cluster.
        /// </summary>
        public const int StorageDefaultPort = 23000;

        // ====================================================================
        // Tracker Protocol Command Codes
        // ====================================================================

        /// <summary>
        /// Command code for querying a storage server for file upload
        /// without specifying a group name. The tracker will select an
        /// appropriate storage server based on load balancing.
        /// </summary>
        public const byte TrackerProtoCmdServiceQueryStoreWithoutGroupOne = 101;

        /// <summary>
        /// Command code for querying a storage server for file download
        /// or metadata retrieval. Returns one storage server that can
        /// provide the requested file.
        /// </summary>
        public const byte TrackerProtoCmdServiceQueryFetchOne = 102;

        /// <summary>
        /// Command code for querying a storage server for file update
        /// operations (delete, set metadata, etc.). Returns the storage
        /// server that owns the file.
        /// </summary>
        public const byte TrackerProtoCmdServiceQueryUpdate = 103;

        /// <summary>
        /// Command code for querying a storage server for file upload
        /// with a specific group name. The tracker will select a storage
        /// server from the specified group.
        /// </summary>
        public const byte TrackerProtoCmdServiceQueryStoreWithGroupOne = 104;

        /// <summary>
        /// Command code for querying all storage servers that can provide
        /// a file for download. Returns a list of all storage servers
        /// containing the file (including replicas).
        /// </summary>
        public const byte TrackerProtoCmdServiceQueryFetchAll = 105;

        /// <summary>
        /// Command code for querying all storage servers available for
        /// file upload without specifying a group name. Returns a list
        /// of all available storage servers.
        /// </summary>
        public const byte TrackerProtoCmdServiceQueryStoreWithoutGroupAll = 106;

        /// <summary>
        /// Command code for querying all storage servers available for
        /// file upload with a specific group name. Returns a list of
        /// all storage servers in the specified group.
        /// </summary>
        public const byte TrackerProtoCmdServiceQueryStoreWithGroupAll = 107;

        /// <summary>
        /// Command code for listing storage servers in a specific group.
        /// Returns detailed information about all storage servers in
        /// the specified group.
        /// </summary>
        public const byte TrackerProtoCmdServerListOneGroup = 90;

        /// <summary>
        /// Command code for listing all storage groups and their servers.
        /// Returns information about all groups in the FastDFS cluster.
        /// </summary>
        public const byte TrackerProtoCmdServerListAllGroups = 91;

        /// <summary>
        /// Command code for listing storage servers for a specific file.
        /// Returns all storage servers that contain the specified file.
        /// </summary>
        public const byte TrackerProtoCmdServerListStorage = 92;

        /// <summary>
        /// Command code for deleting a storage server from the cluster.
        /// This is an administrative operation that removes a storage
        /// server from the tracker's registry.
        /// </summary>
        public const byte TrackerProtoCmdServerDeleteStorage = 93;

        /// <summary>
        /// Command code for reporting IP address changes from storage servers.
        /// Storage servers use this command to notify trackers when their
        /// IP address changes.
        /// </summary>
        public const byte TrackerProtoCmdStorageReportIPChanged = 94;

        /// <summary>
        /// Command code for reporting storage server status to trackers.
        /// Storage servers periodically send status updates to trackers
        /// to indicate their current operational state.
        /// </summary>
        public const byte TrackerProtoCmdStorageReportStatus = 95;

        /// <summary>
        /// Command code for reporting disk usage from storage servers.
        /// Storage servers send disk usage information to trackers to
        /// help with load balancing and capacity planning.
        /// </summary>
        public const byte TrackerProtoCmdStorageReportDiskUsage = 96;

        /// <summary>
        /// Command code for synchronizing timestamps between storage servers.
        /// Used during file synchronization to ensure consistent timestamps
        /// across replicas.
        /// </summary>
        public const byte TrackerProtoCmdStorageSyncTimestamp = 97;

        /// <summary>
        /// Command code for reporting synchronization status from storage servers.
        /// Storage servers use this command to report synchronization progress
        /// and status to trackers.
        /// </summary>
        public const byte TrackerProtoCmdStorageSyncReport = 98;

        // ====================================================================
        // Storage Protocol Command Codes
        // ====================================================================

        /// <summary>
        /// Command code for uploading a regular file to a storage server.
        /// Regular files are immutable once uploaded and cannot be modified.
        /// </summary>
        public const byte StorageProtoCmdUploadFile = 11;

        /// <summary>
        /// Command code for deleting a file from a storage server.
        /// This operation permanently removes the file from storage.
        /// </summary>
        public const byte StorageProtoCmdDeleteFile = 12;

        /// <summary>
        /// Command code for setting metadata for a file on a storage server.
        /// Metadata consists of key-value pairs that provide additional
        /// information about files.
        /// </summary>
        public const byte StorageProtoCmdSetMetadata = 13;

        /// <summary>
        /// Command code for downloading a file from a storage server.
        /// Supports downloading the entire file or a specific byte range.
        /// </summary>
        public const byte StorageProtoCmdDownloadFile = 14;

        /// <summary>
        /// Command code for retrieving metadata for a file from a storage server.
        /// Returns all metadata key-value pairs associated with the file.
        /// </summary>
        public const byte StorageProtoCmdGetMetadata = 15;

        /// <summary>
        /// Command code for uploading a slave file to a storage server.
        /// Slave files are associated with master files and are typically
        /// used for thumbnails, previews, or other derived versions.
        /// </summary>
        public const byte StorageProtoCmdUploadSlaveFile = 21;

        /// <summary>
        /// Command code for querying file information from a storage server.
        /// Returns file size, creation timestamp, CRC32 checksum, and
        /// source server information.
        /// </summary>
        public const byte StorageProtoCmdQueryFileInfo = 22;

        /// <summary>
        /// Command code for uploading an appender file to a storage server.
        /// Appender files can be modified after upload using append, modify,
        /// and truncate operations.
        /// </summary>
        public const byte StorageProtoCmdUploadAppenderFile = 23;

        /// <summary>
        /// Command code for appending data to an appender file on a storage server.
        /// Appends new data to the end of an existing appender file.
        /// </summary>
        public const byte StorageProtoCmdAppendFile = 24;

        /// <summary>
        /// Command code for modifying content of an appender file on a storage server.
        /// Allows overwriting data at a specific offset in an appender file.
        /// </summary>
        public const byte StorageProtoCmdModifyFile = 34;

        /// <summary>
        /// Command code for truncating an appender file on a storage server.
        /// Reduces the file size to a specified length, discarding data
        /// beyond the truncation point.
        /// </summary>
        public const byte StorageProtoCmdTruncateFile = 36;

        // ====================================================================
        // Protocol Response Codes
        // ====================================================================

        /// <summary>
        /// Standard response code for successful protocol operations.
        /// All successful responses from FastDFS servers use this code.
        /// </summary>
        public const byte TrackerProtoResp = 100;

        /// <summary>
        /// Alias for TrackerProtoResp. Used for consistency with C implementation.
        /// </summary>
        public const byte FdfsProtoResp = TrackerProtoResp;

        /// <summary>
        /// Alias for TrackerProtoResp. Used specifically for storage server responses.
        /// </summary>
        public const byte FdfsStorageProtoResp = TrackerProtoResp;

        // ====================================================================
        // Protocol Field Size Limits
        // ====================================================================

        /// <summary>
        /// Maximum length of a storage group name in bytes.
        /// Group names are used to organize storage servers into logical
        /// groups for replication and load balancing.
        /// </summary>
        public const int GroupNameMaxLength = 16;

        /// <summary>
        /// Maximum length of a file extension name in bytes (without the dot).
        /// File extensions are used to categorize files and determine storage
        /// paths. Common examples: "jpg", "txt", "pdf", "mp4".
        /// </summary>
        public const int FileExtNameMaxLength = 6;

        /// <summary>
        /// Maximum length of a metadata key name in bytes.
        /// Metadata keys are used to identify metadata values associated
        /// with files. Examples: "width", "height", "author", "date".
        /// </summary>
        public const int MaxMetaNameLength = 64;

        /// <summary>
        /// Maximum length of a metadata value in bytes.
        /// Metadata values store the actual data associated with metadata keys.
        /// Values can be strings, numbers, or other text-based data.
        /// </summary>
        public const int MaxMetaValueLength = 256;

        /// <summary>
        /// Maximum length of a slave file prefix name in bytes.
        /// Prefix names are used to generate slave file IDs from master
        /// file IDs. Examples: "thumb", "small", "preview", "icon".
        /// </summary>
        public const int FilePrefixMaxLength = 16;

        /// <summary>
        /// Maximum size of a storage server ID in bytes.
        /// Storage server IDs are unique identifiers assigned to each
        /// storage server in the cluster.
        /// </summary>
        public const int StorageIDMaxSize = 16;

        /// <summary>
        /// Size of version string field in bytes.
        /// Version strings are used to identify FastDFS server versions
        /// for compatibility checking and protocol negotiation.
        /// </summary>
        public const int VersionSize = 8;

        /// <summary>
        /// Size of IP address field in bytes.
        /// Supports both IPv4 (4 bytes) and IPv6 (16 bytes) addresses.
        /// The field is sized to accommodate IPv6 addresses.
        /// </summary>
        public const int IPAddressSize = 16;

        /// <summary>
        /// Maximum length of a remote filename on storage server in bytes.
        /// Remote filenames are the paths used to store files on storage
        /// servers. They typically follow patterns like "M00/00/00/xxx".
        /// </summary>
        public const int RemoteFilenameMaxLength = 256;

        // ====================================================================
        // Protocol Separators
        // ====================================================================

        /// <summary>
        /// Record separator character used in metadata encoding.
        /// This character separates different key-value pairs in the metadata
        /// encoding format. Value: 0x01.
        /// </summary>
        public const byte RecordSeparator = 0x01;

        /// <summary>
        /// Field separator character used in metadata encoding.
        /// This character separates keys from values in metadata key-value
        /// pairs. Value: 0x02.
        /// </summary>
        public const byte FieldSeparator = 0x02;

        // ====================================================================
        // Protocol Header Constants
        // ====================================================================

        /// <summary>
        /// Size of the FastDFS protocol header in bytes.
        /// Every protocol message starts with this header, which contains:
        /// - 8 bytes: message body length (big-endian int64)
        /// - 1 byte: command code
        /// - 1 byte: status code
        /// </summary>
        public const int ProtocolHeaderLength = 10;

        // ====================================================================
        // Storage Server Status Codes
        // ====================================================================

        /// <summary>
        /// Status code indicating that a storage server is initializing.
        /// The server is starting up and not yet ready to handle requests.
        /// </summary>
        public const byte StorageStatusInit = 0;

        /// <summary>
        /// Status code indicating that a storage server is waiting for
        /// file synchronization. The server is ready but may not have
        /// all files synchronized with other servers in the group.
        /// </summary>
        public const byte StorageStatusWaitSync = 1;

        /// <summary>
        /// Status code indicating that a storage server is currently
        /// synchronizing files with other servers in the group.
        /// </summary>
        public const byte StorageStatusSyncing = 2;

        /// <summary>
        /// Status code indicating that a storage server's IP address has changed.
        /// The server has notified the tracker of the IP address change.
        /// </summary>
        public const byte StorageStatusIPChanged = 3;

        /// <summary>
        /// Status code indicating that a storage server has been deleted
        /// from the cluster. The server is no longer part of the cluster.
        /// </summary>
        public const byte StorageStatusDeleted = 4;

        /// <summary>
        /// Status code indicating that a storage server is offline.
        /// The server is not responding to requests and may be unavailable.
        /// </summary>
        public const byte StorageStatusOffline = 5;

        /// <summary>
        /// Status code indicating that a storage server is online.
        /// The server is connected to the tracker and available for operations.
        /// </summary>
        public const byte StorageStatusOnline = 6;

        /// <summary>
        /// Status code indicating that a storage server is active and ready.
        /// The server is fully operational and ready to handle all requests.
        /// </summary>
        public const byte StorageStatusActive = 7;

        /// <summary>
        /// Status code indicating that a storage server is in recovery mode.
        /// The server is recovering from a failure and may have limited
        /// functionality until recovery is complete.
        /// </summary>
        public const byte StorageStatusRecovery = 9;

        /// <summary>
        /// Status code indicating that no status information is available
        /// for a storage server. This is typically used when status has
        /// not been reported or is unknown.
        /// </summary>
        public const byte StorageStatusNone = 99;

        // ====================================================================
        // Helper Methods
        // ====================================================================

        /// <summary>
        /// Gets a human-readable description of a storage server status code.
        /// </summary>
        /// <param name="status">
        /// The status code to describe.
        /// </param>
        /// <returns>
        /// A string describing the status code.
        /// </returns>
        public static string GetStorageStatusDescription(byte status)
        {
            switch (status)
            {
                case StorageStatusInit:
                    return "Initializing";
                case StorageStatusWaitSync:
                    return "Waiting for sync";
                case StorageStatusSyncing:
                    return "Synchronizing";
                case StorageStatusIPChanged:
                    return "IP address changed";
                case StorageStatusDeleted:
                    return "Deleted";
                case StorageStatusOffline:
                    return "Offline";
                case StorageStatusOnline:
                    return "Online";
                case StorageStatusActive:
                    return "Active";
                case StorageStatusRecovery:
                    return "Recovery";
                case StorageStatusNone:
                    return "Unknown";
                default:
                    return $"Unknown status code: {status}";
            }
        }
    }
}

