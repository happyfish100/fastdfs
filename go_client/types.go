// Package fdfs provides types and constants for the FastDFS protocol.
// This file defines all protocol-level constants, command codes, and data structures
// used in communication with FastDFS tracker and storage servers.
package fdfs

import (
	"time"
)

// Constants for FastDFS protocol.
// These values are defined by the FastDFS protocol specification and must match
// the values used by the C implementation.
const (
	// Default network ports for FastDFS servers
	TrackerDefaultPort = 22122 // Default port for tracker servers
	StorageDefaultPort = 23000 // Default port for storage servers

	// Tracker protocol commands - used when communicating with tracker servers
	TrackerProtoCmdServiceQueryStoreWithoutGroupOne = 101
	TrackerProtoCmdServiceQueryFetchOne             = 102
	TrackerProtoCmdServiceQueryUpdate               = 103
	TrackerProtoCmdServiceQueryStoreWithGroupOne    = 104
	TrackerProtoCmdServiceQueryFetchAll             = 105
	TrackerProtoCmdServiceQueryStoreWithoutGroupAll = 106
	TrackerProtoCmdServiceQueryStoreWithGroupAll    = 107
	TrackerProtoCmdServerListOneGroup               = 90
	TrackerProtoCmdServerListAllGroups              = 91
	TrackerProtoCmdServerListStorage                = 92
	TrackerProtoCmdServerDeleteStorage              = 93
	TrackerProtoCmdStorageReportIPChanged           = 94
	TrackerProtoCmdStorageReportStatus              = 95
	TrackerProtoCmdStorageReportDiskUsage           = 96
	TrackerProtoCmdStorageSyncTimestamp             = 97
	TrackerProtoCmdStorageSyncReport                = 98

	// Storage protocol commands - used when communicating with storage servers
	StorageProtoCmdUploadFile         = 11 // Upload a regular file
	StorageProtoCmdDeleteFile         = 12 // Delete a file
	StorageProtoCmdSetMetadata        = 13 // Set file metadata
	StorageProtoCmdDownloadFile       = 14 // Download a file
	StorageProtoCmdGetMetadata        = 15 // Get file metadata
	StorageProtoCmdUploadSlaveFile    = 21 // Upload a slave file (thumbnail, etc.)
	StorageProtoCmdQueryFileInfo      = 22 // Query file information
	StorageProtoCmdUploadAppenderFile = 23 // Upload an appender file (can be modified)
	StorageProtoCmdAppendFile         = 24 // Append data to an appender file
	StorageProtoCmdModifyFile         = 34 // Modify content of an appender file
	StorageProtoCmdTruncateFile       = 36 // Truncate an appender file

	// Protocol response codes
	TrackerProtoResp     = 100 // Standard response code
	FdfsProtoResp        = TrackerProtoResp
	FdfsStorageProtoResp = TrackerProtoResp

	// Protocol field size limits - these define maximum lengths for various fields
	FdfsGroupNameMaxLen   = 16  // Maximum length of a storage group name
	FdfsFileExtNameMaxLen = 6   // Maximum length of file extension (without dot)
	FdfsMaxMetaNameLen    = 64  // Maximum length of metadata key
	FdfsMaxMetaValueLen   = 256 // Maximum length of metadata value
	FdfsFilePrefixMaxLen  = 16  // Maximum length of slave file prefix
	FdfsStorageIDMaxSize  = 16  // Maximum size of storage server ID
	FdfsVersionSize       = 8   // Size of version string field
	IPAddressSize         = 16  // Size of IP address field (supports IPv4 and IPv6)

	// Protocol separators - special characters used in metadata encoding
	FdfsRecordSeparator = '\x01' // Separates different key-value pairs in metadata
	FdfsFieldSeparator  = '\x02' // Separates key from value in metadata

	// Protocol header size
	FdfsProtoHeaderLen = 10 // Size of protocol header (8 bytes length + 1 byte cmd + 1 byte status)

	// Storage server status codes - indicate the current state of a storage server
	FdfsStorageStatusInit      = 0  // Storage server is initializing
	FdfsStorageStatusWaitSync  = 1  // Waiting for file synchronization
	FdfsStorageStatusSyncing   = 2  // Currently synchronizing files
	FdfsStorageStatusIPChanged = 3  // IP address has changed
	FdfsStorageStatusDeleted   = 4  // Storage server has been deleted
	FdfsStorageStatusOffline   = 5  // Storage server is offline
	FdfsStorageStatusOnline    = 6  // Storage server is online
	FdfsStorageStatusActive    = 7  // Storage server is active and ready
	FdfsStorageStatusRecovery  = 9  // Storage server is in recovery mode
	FdfsStorageStatusNone      = 99 // No status information
)

// MetadataFlag specifies how metadata should be updated.
// This controls whether new metadata replaces or merges with existing metadata.
type MetadataFlag byte

const (
	// MetadataOverwrite completely replaces all existing metadata with new values.
	// Any existing metadata keys not in the new set will be removed.
	MetadataOverwrite MetadataFlag = 'O'
	
	// MetadataMerge merges new metadata with existing metadata.
	// Existing keys are updated, new keys are added, and unspecified keys are kept.
	MetadataMerge MetadataFlag = 'M'
)

// FileInfo contains detailed information about a file stored in FastDFS.
// This information is returned by GetFileInfo and includes size, timestamps,
// checksum, and source server information.
type FileInfo struct {
	// FileSize is the size of the file in bytes
	FileSize int64

	// CreateTime is the timestamp when the file was created
	CreateTime time.Time

	// CRC32 is the CRC32 checksum of the file
	CRC32 uint32

	// SourceIPAddr is the IP address of the source storage server
	SourceIPAddr string
}

// StorageServer represents a storage server in the FastDFS cluster.
// This information is returned by the tracker when querying for upload or download.
type StorageServer struct {
	IPAddr         string // IP address of the storage server
	Port           int    // Port number of the storage server
	StorePathIndex byte   // Index of the storage path to use (0-based)
}

// GroupInfo contains information about a storage group.
// A group is a collection of storage servers that replicate files among themselves.
type GroupInfo struct {
	GroupName      string
	TotalMB        int64
	FreeMB         int64
	TrunkFreeMB    int64
	StorageCount   int
	StoragePort    int
	StorageHTTPPort int
	ActiveCount    int
	CurrentWriteServer int
	StorePathCount int
	SubdirCountPerPath int
	CurrentTrunkFileID int
}

// StorageInfo contains detailed information about a storage server.
// This includes status, capacity, version, and configuration details.
type StorageInfo struct {
	Status             byte
	ID                 string
	IPAddr             string
	SrcIPAddr          string
	DomainName         string
	Version            string
	JoinTime           time.Time
	UpTime             time.Time
	TotalMB            int64
	FreeMB             int64
	UploadPriority     int
	StorePathCount     int
	SubdirCountPerPath int
	StoragePort        int
	StorageHTTPPort    int
	CurrentWritePath   int
	SourceStorageID    string
	IfTrunkServer      bool
}

// TrackerHeader represents the FastDFS protocol header.
// Every message between client and server starts with this 10-byte header.
type TrackerHeader struct {
	Length int64 // Length of the message body (not including header)
	Cmd    byte  // Command code (request type or response type)
	Status byte  // Status code (0 for success, error code otherwise)
}

// UploadResponse represents the response from an upload operation.
// The server returns the group name and remote filename which together form the file ID.
type UploadResponse struct {
	GroupName      string // Storage group where the file was stored
	RemoteFilename string // Path and filename on the storage server
}

// ConnectionInfo represents connection information for a server.
// This is used internally for tracking server endpoints.
type ConnectionInfo struct {
	IPAddr string // IP address of the server
	Port   int    // Port number of the server
	Sock   int    // Socket file descriptor (for internal use)
}
