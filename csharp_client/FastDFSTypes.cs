// ============================================================================
// FastDFS Type Definitions
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This file defines all data types used by the FastDFS client, including
// file information structures, storage server information, metadata flags,
// and other protocol-level types.
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
    /// Enumeration of metadata operation flags.
    /// 
    /// Metadata operations can either overwrite all existing metadata or
    /// merge with existing metadata. This flag controls the behavior when
    /// setting metadata for a file.
    /// </summary>
    public enum MetadataFlag : byte
    {
        /// <summary>
        /// Overwrite flag: replaces all existing metadata with new values.
        /// Any existing metadata keys not in the new set will be removed.
        /// This is the default behavior for metadata operations.
        /// </summary>
        Overwrite = (byte)'O',

        /// <summary>
        /// Merge flag: combines new metadata with existing metadata.
        /// Existing keys are updated with new values, new keys are added,
        /// and unspecified keys are kept unchanged.
        /// </summary>
        Merge = (byte)'M'
    }

    /// <summary>
    /// Represents detailed information about a file stored in FastDFS.
    /// 
    /// This structure contains all available information about a file,
    /// including size, creation timestamp, checksum, and source server
    /// information. It is returned by GetFileInfoAsync operations.
    /// </summary>
    public class FileInfo
    {
        /// <summary>
        /// Gets or sets the size of the file in bytes.
        /// This value represents the total number of bytes stored in the file.
        /// </summary>
        public long FileSize { get; set; }

        /// <summary>
        /// Gets or sets the timestamp when the file was created.
        /// This is a Unix timestamp (seconds since January 1, 1970 UTC).
        /// </summary>
        public DateTime CreateTime { get; set; }

        /// <summary>
        /// Gets or sets the CRC32 checksum of the file.
        /// This value can be used to verify file integrity and detect
        /// corruption or transmission errors.
        /// </summary>
        public uint CRC32 { get; set; }

        /// <summary>
        /// Gets or sets the IP address of the source storage server.
        /// This is the storage server where the file was originally uploaded.
        /// </summary>
        public string SourceIPAddr { get; set; }

        /// <summary>
        /// Returns a string representation of the file information.
        /// </summary>
        /// <returns>
        /// A string containing file size, creation time, CRC32, and source IP.
        /// </returns>
        public override string ToString()
        {
            return $"FileSize: {FileSize}, CreateTime: {CreateTime}, CRC32: {CRC32:X8}, SourceIP: {SourceIPAddr}";
        }
    }

    /// <summary>
    /// Represents information about a storage server in the FastDFS cluster.
    /// 
    /// This structure contains the network address and storage path information
    /// for a storage server. It is returned by tracker queries when requesting
    /// storage servers for upload, download, or update operations.
    /// </summary>
    public class StorageServer
    {
        /// <summary>
        /// Gets or sets the IP address of the storage server.
        /// This is the network address where the storage server can be reached.
        /// Can be either IPv4 or IPv6 format.
        /// </summary>
        public string IPAddr { get; set; }

        /// <summary>
        /// Gets or sets the port number of the storage server.
        /// The default storage server port is 23000, but custom ports
        /// can be configured.
        /// </summary>
        public int Port { get; set; }

        /// <summary>
        /// Gets or sets the store path index on the storage server.
        /// Storage servers can have multiple storage paths, and this index
        /// indicates which path should be used for file operations.
        /// </summary>
        public byte StorePathIndex { get; set; }

        /// <summary>
        /// Gets or sets the group name that this storage server belongs to.
        /// Storage servers are organized into groups for replication and
        /// load balancing purposes.
        /// </summary>
        public string GroupName { get; set; }

        /// <summary>
        /// Returns a string representation of the storage server information.
        /// </summary>
        /// <returns>
        /// A string containing IP address, port, and store path index.
        /// </returns>
        public override string ToString()
        {
            return $"{IPAddr}:{Port} (Group: {GroupName}, PathIndex: {StorePathIndex})";
        }
    }

    /// <summary>
    /// Represents information about a storage group in the FastDFS cluster.
    /// 
    /// A storage group is a collection of storage servers that replicate
    /// files among themselves. This structure contains aggregate information
    /// about the group, including total capacity, free space, and server counts.
    /// </summary>
    public class GroupInfo
    {
        /// <summary>
        /// Gets or sets the name of the storage group.
        /// Group names are unique identifiers for storage groups in the cluster.
        /// </summary>
        public string GroupName { get; set; }

        /// <summary>
        /// Gets or sets the total storage capacity of the group in megabytes.
        /// This represents the sum of all storage capacity across all servers
        /// in the group.
        /// </summary>
        public long TotalMB { get; set; }

        /// <summary>
        /// Gets or sets the free storage space in the group in megabytes.
        /// This represents the sum of all free space across all servers
        /// in the group.
        /// </summary>
        public long FreeMB { get; set; }

        /// <summary>
        /// Gets or sets the free trunk space in the group in megabytes.
        /// Trunk space is used for large file storage in trunk mode.
        /// </summary>
        public long TrunkFreeMB { get; set; }

        /// <summary>
        /// Gets or sets the total number of storage servers in the group.
        /// This includes all servers regardless of their current status.
        /// </summary>
        public int StorageCount { get; set; }

        /// <summary>
        /// Gets or sets the port number used by storage servers in this group.
        /// All storage servers in a group typically use the same port number.
        /// </summary>
        public int StoragePort { get; set; }

        /// <summary>
        /// Gets or sets the HTTP port number used by storage servers in this group.
        /// This port is used for HTTP-based file access if HTTP support is enabled.
        /// </summary>
        public int StorageHTTPPort { get; set; }

        /// <summary>
        /// Gets or sets the number of active storage servers in the group.
        /// Active servers are online and ready to handle requests.
        /// </summary>
        public int ActiveCount { get; set; }

        /// <summary>
        /// Gets or sets the index of the current write server in the group.
        /// The write server is the server that handles new file uploads.
        /// </summary>
        public int CurrentWriteServer { get; set; }

        /// <summary>
        /// Gets or sets the number of storage paths per server in the group.
        /// Each storage server can have multiple storage paths for organizing files.
        /// </summary>
        public int StorePathCount { get; set; }

        /// <summary>
        /// Gets or sets the number of subdirectories per storage path.
        /// Files are organized into subdirectories to improve filesystem performance.
        /// </summary>
        public int SubdirCountPerPath { get; set; }

        /// <summary>
        /// Gets or sets the current trunk file ID.
        /// Trunk files are used for storing large files in trunk mode.
        /// </summary>
        public int CurrentTrunkFileID { get; set; }

        /// <summary>
        /// Returns a string representation of the group information.
        /// </summary>
        /// <returns>
        /// A string containing group name, capacity, and server counts.
        /// </returns>
        public override string ToString()
        {
            return $"Group: {GroupName}, Total: {TotalMB}MB, Free: {FreeMB}MB, " +
                   $"Servers: {StorageCount} (Active: {ActiveCount})";
        }
    }

    /// <summary>
    /// Represents detailed information about a storage server.
    /// 
    /// This structure contains comprehensive information about a storage server,
    /// including status, capacity, version, and configuration details. It is
    /// returned by tracker queries when requesting detailed server information.
    /// </summary>
    public class StorageInfo
    {
        /// <summary>
        /// Gets or sets the current status of the storage server.
        /// Status values are defined in FastDFSConstants (e.g., StorageStatusActive,
        /// StorageStatusOffline, etc.).
        /// </summary>
        public byte Status { get; set; }

        /// <summary>
        /// Gets or sets the unique identifier of the storage server.
        /// Server IDs are assigned during server initialization and remain
        /// constant throughout the server's lifetime.
        /// </summary>
        public string ID { get; set; }

        /// <summary>
        /// Gets or sets the IP address of the storage server.
        /// </summary>
        public string IPAddr { get; set; }

        /// <summary>
        /// Gets or sets the source IP address of the storage server.
        /// This may differ from IPAddr in certain network configurations.
        /// </summary>
        public string SrcIPAddr { get; set; }

        /// <summary>
        /// Gets or sets the domain name of the storage server, if configured.
        /// Domain names provide an alternative way to access storage servers.
        /// </summary>
        public string DomainName { get; set; }

        /// <summary>
        /// Gets or sets the version string of the storage server.
        /// Version strings identify the FastDFS server version for compatibility
        /// checking and protocol negotiation.
        /// </summary>
        public string Version { get; set; }

        /// <summary>
        /// Gets or sets the timestamp when the storage server joined the cluster.
        /// This is a Unix timestamp indicating when the server was first registered
        /// with the tracker.
        /// </summary>
        public DateTime JoinTime { get; set; }

        /// <summary>
        /// Gets or sets the timestamp when the storage server last came online.
        /// This is a Unix timestamp indicating the most recent time the server
        /// became available for operations.
        /// </summary>
        public DateTime UpTime { get; set; }

        /// <summary>
        /// Gets or sets the total storage capacity of the server in megabytes.
        /// </summary>
        public long TotalMB { get; set; }

        /// <summary>
        /// Gets or sets the free storage space on the server in megabytes.
        /// </summary>
        public long FreeMB { get; set; }

        /// <summary>
        /// Gets or sets the upload priority of the storage server.
        /// Higher priority servers are preferred for file uploads when multiple
        /// servers are available.
        /// </summary>
        public int UploadPriority { get; set; }

        /// <summary>
        /// Gets or sets the number of storage paths on the server.
        /// </summary>
        public int StorePathCount { get; set; }

        /// <summary>
        /// Gets or sets the number of subdirectories per storage path.
        /// </summary>
        public int SubdirCountPerPath { get; set; }

        /// <summary>
        /// Gets or sets the port number used by the storage server.
        /// </summary>
        public int StoragePort { get; set; }

        /// <summary>
        /// Gets or sets the HTTP port number used by the storage server.
        /// </summary>
        public int StorageHTTPPort { get; set; }

        /// <summary>
        /// Gets or sets the index of the current write path on the server.
        /// </summary>
        public int CurrentWritePath { get; set; }

        /// <summary>
        /// Gets or sets the source storage server ID, if this server is a replica.
        /// </summary>
        public string SourceStorageID { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether this server is a trunk server.
        /// Trunk servers are used for storing large files in trunk mode.
        /// </summary>
        public bool IfTrunkServer { get; set; }

        /// <summary>
        /// Returns a string representation of the storage server information.
        /// </summary>
        /// <returns>
        /// A string containing server ID, IP address, status, and capacity.
        /// </returns>
        public override string ToString()
        {
            return $"Server: {ID} ({IPAddr}:{StoragePort}), Status: {Status}, " +
                   $"Total: {TotalMB}MB, Free: {FreeMB}MB";
        }
    }

    /// <summary>
    /// Represents the FastDFS protocol header.
    /// 
    /// Every message between client and server starts with this header.
    /// It contains the message length, command code, and status code.
    /// </summary>
    public class ProtocolHeader
    {
        /// <summary>
        /// Gets or sets the length of the message body in bytes.
        /// This value does not include the header itself (10 bytes).
        /// </summary>
        public long Length { get; set; }

        /// <summary>
        /// Gets or sets the command code for the message.
        /// Command codes are defined in FastDFSConstants and indicate the
        /// type of operation being performed (e.g., upload, download, delete).
        /// </summary>
        public byte Cmd { get; set; }

        /// <summary>
        /// Gets or sets the status code for the message.
        /// Status code 0 indicates success, non-zero values indicate errors.
        /// </summary>
        public byte Status { get; set; }

        /// <summary>
        /// Returns a string representation of the protocol header.
        /// </summary>
        /// <returns>
        /// A string containing length, command code, and status code.
        /// </returns>
        public override string ToString()
        {
            return $"Length: {Length}, Cmd: {Cmd}, Status: {Status}";
        }
    }

    /// <summary>
    /// Represents the response from an upload operation.
    /// 
    /// When a file is successfully uploaded, the server returns the group
    /// name and remote filename, which together form the file ID that can
    /// be used for subsequent operations.
    /// </summary>
    public class UploadResponse
    {
        /// <summary>
        /// Gets or sets the storage group name where the file was stored.
        /// Group names are used to organize storage servers and identify
        /// where files are located in the cluster.
        /// </summary>
        public string GroupName { get; set; }

        /// <summary>
        /// Gets or sets the remote filename on the storage server.
        /// Remote filenames are paths used to store files on storage servers.
        /// They typically follow patterns like "M00/00/00/xxx".
        /// </summary>
        public string RemoteFilename { get; set; }

        /// <summary>
        /// Gets the complete file ID by combining group name and remote filename.
        /// File IDs have the format "group/remote_filename" and uniquely
        /// identify files in the FastDFS cluster.
        /// </summary>
        public string FileId => $"{GroupName}/{RemoteFilename}";

        /// <summary>
        /// Returns a string representation of the upload response.
        /// </summary>
        /// <returns>
        /// The file ID (group/remote_filename format).
        /// </returns>
        public override string ToString()
        {
            return FileId;
        }
    }
}

