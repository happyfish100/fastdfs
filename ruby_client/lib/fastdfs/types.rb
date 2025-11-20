# FastDFS Protocol Types and Constants
#
# This module defines all protocol-level constants, command codes, and
# data structures used in communication with FastDFS tracker and storage servers.
#
# The constants and types defined here match the FastDFS protocol specification
# and must be kept in sync with the C implementation and other client libraries.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

require 'time'

module FastDFS
  # Default network ports for FastDFS servers.
  #
  # These are the standard ports used by FastDFS tracker and storage servers.
  # They can be overridden in server configuration, but these are the defaults.
  TRACKER_DEFAULT_PORT = 22122  # Default port for tracker servers
  STORAGE_DEFAULT_PORT = 23000  # Default port for storage servers
  
  # Protocol header size in bytes.
  #
  # Every message between client and server starts with a 10-byte header
  # containing: 8 bytes for body length, 1 byte for command code, 1 byte for status.
  FDFS_PROTO_HEADER_LEN = 10
  
  # Field size limits for various protocol fields.
  #
  # These constants define the maximum lengths for different fields in the
  # FastDFS protocol. They must match the values defined in the C implementation.
  FDFS_GROUP_NAME_MAX_LEN = 16      # Maximum length of storage group name
  FDFS_FILE_EXT_NAME_MAX_LEN = 6    # Maximum length of file extension (without dot)
  FDFS_MAX_META_NAME_LEN = 64       # Maximum length of metadata key
  FDFS_MAX_META_VALUE_LEN = 256     # Maximum length of metadata value
  FDFS_FILE_PREFIX_MAX_LEN = 16     # Maximum length of slave file prefix
  FDFS_STORAGE_ID_MAX_SIZE = 16     # Maximum size of storage server ID
  FDFS_VERSION_SIZE = 8             # Size of version string field
  IP_ADDRESS_SIZE = 16              # Size of IP address field (supports IPv4 and IPv6)
  
  # Protocol separators for metadata encoding.
  #
  # FastDFS uses special characters to separate different metadata entries
  # and to separate keys from values within entries.
  FDFS_RECORD_SEPARATOR = "\x01"    # Separates different key-value pairs
  FDFS_FIELD_SEPARATOR = "\x02"     # Separates key from value
  
  # Tracker protocol command codes.
  #
  # These commands are used when communicating with tracker servers to
  # query for storage servers, list groups, and perform administrative tasks.
  module TrackerCommand
    # Query for a storage server without specifying a group
    SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE = 101
    
    # Query for a storage server to fetch (download) a file
    SERVICE_QUERY_FETCH_ONE = 102
    
    # Query for a storage server to update (modify) a file
    SERVICE_QUERY_UPDATE = 103
    
    # Query for a storage server in a specific group
    SERVICE_QUERY_STORE_WITH_GROUP_ONE = 104
    
    # Query for all storage servers to fetch a file
    SERVICE_QUERY_FETCH_ALL = 105
    
    # Query for all storage servers without specifying a group
    SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL = 106
    
    # Query for all storage servers in a specific group
    SERVICE_QUERY_STORE_WITH_GROUP_ALL = 107
    
    # List servers in one group
    SERVER_LIST_ONE_GROUP = 90
    
    # List servers in all groups
    SERVER_LIST_ALL_GROUPS = 91
    
    # List storage servers
    SERVER_LIST_STORAGE = 92
    
    # Delete a storage server
    SERVER_DELETE_STORAGE = 93
    
    # Report that storage IP has changed
    STORAGE_REPORT_IP_CHANGED = 94
    
    # Report storage server status
    STORAGE_REPORT_STATUS = 95
    
    # Report storage server disk usage
    STORAGE_REPORT_DISK_USAGE = 96
    
    # Storage sync timestamp
    STORAGE_SYNC_TIMESTAMP = 97
    
    # Storage sync report
    STORAGE_SYNC_REPORT = 98
  end
  
  # Storage protocol command codes.
  #
  # These commands are used when communicating with storage servers to
  # upload, download, delete, and manage files.
  module StorageCommand
    # Upload a regular file
    UPLOAD_FILE = 11
    
    # Delete a file
    DELETE_FILE = 12
    
    # Set file metadata
    SET_METADATA = 13
    
    # Download a file
    DOWNLOAD_FILE = 14
    
    # Get file metadata
    GET_METADATA = 15
    
    # Upload a slave file (thumbnail, etc.)
    UPLOAD_SLAVE_FILE = 21
    
    # Query file information
    QUERY_FILE_INFO = 22
    
    # Upload an appender file (can be modified later)
    UPLOAD_APPENDER_FILE = 23
    
    # Append data to an appender file
    APPEND_FILE = 24
    
    # Modify content of an appender file
    MODIFY_FILE = 34
    
    # Truncate an appender file
    TRUNCATE_FILE = 36
  end
  
  # Storage server status codes.
  #
  # These codes indicate the current state of a storage server in the cluster.
  module StorageStatus
    # Storage server is initializing
    INIT = 0
    
    # Waiting for file synchronization
    WAIT_SYNC = 1
    
    # Currently synchronizing files
    SYNCING = 2
    
    # IP address has changed
    IP_CHANGED = 3
    
    # Storage server has been deleted
    DELETED = 4
    
    # Storage server is offline
    OFFLINE = 5
    
    # Storage server is online
    ONLINE = 6
    
    # Storage server is active and ready
    ACTIVE = 7
    
    # Storage server is in recovery mode
    RECOVERY = 9
    
    # No status information
    NONE = 99
  end
  
  # Metadata operation flags.
  #
  # These flags control how metadata is updated when using set_metadata.
  module MetadataFlag
    # Overwrite all existing metadata with new values
    # Any existing metadata keys not in the new set will be removed
    OVERWRITE = 'O'
    
    # Merge new metadata with existing metadata
    # Existing keys are updated, new keys are added, unspecified keys are kept
    MERGE = 'M'
  end
  
  # File information structure.
  #
  # This class holds detailed information about a file stored in FastDFS,
  # including size, timestamps, checksum, and source server information.
  #
  # @example Get file info
  #   info = client.get_file_info(file_id)
  #   puts "Size: #{info.file_size} bytes"
  #   puts "Created: #{info.create_time}"
  #   puts "CRC32: #{info.crc32}"
  class FileInfo
    # File size in bytes.
    #
    # @return [Integer] File size in bytes.
    attr_accessor :file_size
    
    # Timestamp when the file was created.
    #
    # @return [Time] Creation timestamp.
    attr_accessor :create_time
    
    # CRC32 checksum of the file.
    #
    # This can be used to verify file integrity.
    #
    # @return [Integer] CRC32 checksum value.
    attr_accessor :crc32
    
    # IP address of the source storage server.
    #
    # This is the storage server where the file was originally uploaded.
    #
    # @return [String] IP address of source storage server.
    attr_accessor :source_ip_addr
    
    # Initializes a new FileInfo with the given values.
    #
    # @param file_size [Integer] File size in bytes.
    # @param create_time [Time] Creation timestamp.
    # @param crc32 [Integer] CRC32 checksum.
    # @param source_ip_addr [String] IP address of source storage server.
    def initialize(file_size, create_time, crc32, source_ip_addr)
      # Set file size
      # Must be a non-negative integer
      @file_size = file_size
      
      # Set creation time
      # Must be a Time object
      @create_time = create_time
      
      # Set CRC32 checksum
      # Must be an integer
      @crc32 = crc32
      
      # Set source IP address
      # Must be a string
      @source_ip_addr = source_ip_addr
    end
    
    # Returns a string representation of the file info.
    #
    # @return [String] String representation.
    def to_s
      "FileInfo(file_size=#{@file_size}, create_time=#{@create_time}, crc32=#{@crc32}, source_ip_addr=#{@source_ip_addr})"
    end
    
    # Checks if this FileInfo equals another FileInfo.
    #
    # @param other [FileInfo] The other FileInfo to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def ==(other)
      return false unless other.is_a?(FileInfo)
      
      @file_size == other.file_size &&
        @create_time == other.create_time &&
        @crc32 == other.crc32 &&
        @source_ip_addr == other.source_ip_addr
    end
    
    # Computes hash code for this FileInfo.
    #
    # @return [Integer] Hash code.
    def hash
      [@file_size, @create_time, @crc32, @source_ip_addr].hash
    end
    
    # Checks if this FileInfo equals another (eql? version).
    #
    # @param other [FileInfo] The other FileInfo to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def eql?(other)
      self == other
    end
  end
  
  # Storage server information structure.
  #
  # This class represents a storage server in the FastDFS cluster.
  # It contains the IP address, port, and storage path index for the server.
  #
  # @example Storage server from tracker query
  #   server = client.get_storage_server('group1')
  #   puts "IP: #{server.ip_addr}"
  #   puts "Port: #{server.port}"
  class StorageServer
    # IP address of the storage server.
    #
    # @return [String] IP address.
    attr_accessor :ip_addr
    
    # Port number of the storage server.
    #
    # @return [Integer] Port number.
    attr_accessor :port
    
    # Index of the storage path to use (0-based).
    #
    # Storage servers can have multiple storage paths. This index
    # specifies which path should be used for uploads.
    #
    # @return [Integer] Storage path index.
    attr_accessor :store_path_index
    
    # Initializes a new StorageServer with the given values.
    #
    # @param ip_addr [String] IP address of the storage server.
    # @param port [Integer] Port number.
    # @param store_path_index [Integer] Storage path index (default: 0).
    def initialize(ip_addr, port, store_path_index = 0)
      # Set IP address
      # Must be a non-empty string
      @ip_addr = ip_addr
      
      # Set port number
      # Must be a valid port (1-65535)
      @port = port
      
      # Set storage path index
      # Default to 0 if not provided
      @store_path_index = store_path_index
    end
    
    # Returns the address string in "host:port" format.
    #
    # @return [String] Address string.
    def address
      "#{@ip_addr}:#{@port}"
    end
    
    # Returns a string representation of the storage server.
    #
    # @return [String] String representation.
    def to_s
      "StorageServer(ip_addr=#{@ip_addr}, port=#{@port}, store_path_index=#{@store_path_index})"
    end
    
    # Checks if this StorageServer equals another StorageServer.
    #
    # @param other [StorageServer] The other StorageServer to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def ==(other)
      return false unless other.is_a?(StorageServer)
      
      @ip_addr == other.ip_addr &&
        @port == other.port &&
        @store_path_index == other.store_path_index
    end
    
    # Computes hash code for this StorageServer.
    #
    # @return [Integer] Hash code.
    def hash
      [@ip_addr, @port, @store_path_index].hash
    end
    
    # Checks if this StorageServer equals another (eql? version).
    #
    # @param other [StorageServer] The other StorageServer to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def eql?(other)
      self == other
    end
  end
  
  # Protocol header structure.
  #
  # This class represents the FastDFS protocol header that appears at the
  # beginning of every message between client and server.
  #
  # The header is 10 bytes: 8 bytes for body length, 1 byte for command code, 1 byte for status.
  class TrackerHeader
    # Length of the message body (not including header).
    #
    # @return [Integer] Body length in bytes.
    attr_accessor :length
    
    # Command code (request type or response type).
    #
    # @return [Integer] Command code.
    attr_accessor :cmd
    
    # Status code (0 for success, error code otherwise).
    #
    # @return [Integer] Status code.
    attr_accessor :status
    
    # Initializes a new TrackerHeader with the given values.
    #
    # @param length [Integer] Body length in bytes.
    # @param cmd [Integer] Command code.
    # @param status [Integer] Status code (default: 0).
    def initialize(length, cmd, status = 0)
      # Set body length
      # Must be a non-negative integer
      @length = length
      
      # Set command code
      # Must be a valid command code
      @cmd = cmd
      
      # Set status code
      # 0 means success, non-zero means error
      @status = status
    end
    
    # Returns a string representation of the header.
    #
    # @return [String] String representation.
    def to_s
      "TrackerHeader(length=#{@length}, cmd=#{@cmd}, status=#{@status})"
    end
    
    # Checks if this TrackerHeader equals another TrackerHeader.
    #
    # @param other [TrackerHeader] The other TrackerHeader to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def ==(other)
      return false unless other.is_a?(TrackerHeader)
      
      @length == other.length &&
        @cmd == other.cmd &&
        @status == other.status
    end
    
    # Computes hash code for this TrackerHeader.
    #
    # @return [Integer] Hash code.
    def hash
      [@length, @cmd, @status].hash
    end
    
    # Checks if this TrackerHeader equals another (eql? version).
    #
    # @param other [TrackerHeader] The other TrackerHeader to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def eql?(other)
      self == other
    end
  end
  
  # Upload response structure.
  #
  # This class represents the response from an upload operation.
  # It contains the group name and remote filename which together form the file ID.
  class UploadResponse
    # Storage group where the file was stored.
    #
    # @return [String] Group name.
    attr_accessor :group_name
    
    # Path and filename on the storage server.
    #
    # @return [String] Remote filename.
    attr_accessor :remote_filename
    
    # Initializes a new UploadResponse with the given values.
    #
    # @param group_name [String] Storage group name.
    # @param remote_filename [String] Remote filename.
    def initialize(group_name, remote_filename)
      # Set group name
      # Must be a non-empty string
      @group_name = group_name
      
      # Set remote filename
      # Must be a non-empty string
      @remote_filename = remote_filename
    end
    
    # Returns the file ID in "group/remote_filename" format.
    #
    # @return [String] File ID.
    def file_id
      "#{@group_name}/#{@remote_filename}"
    end
    
    # Returns a string representation of the upload response.
    #
    # @return [String] String representation.
    def to_s
      "UploadResponse(group_name=#{@group_name}, remote_filename=#{@remote_filename})"
    end
    
    # Checks if this UploadResponse equals another UploadResponse.
    #
    # @param other [UploadResponse] The other UploadResponse to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def ==(other)
      return false unless other.is_a?(UploadResponse)
      
      @group_name == other.group_name &&
        @remote_filename == other.remote_filename
    end
    
    # Computes hash code for this UploadResponse.
    #
    # @return [Integer] Hash code.
    def hash
      [@group_name, @remote_filename].hash
    end
    
    # Checks if this UploadResponse equals another (eql? version).
    #
    # @param other [UploadResponse] The other UploadResponse to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def eql?(other)
      self == other
    end
  end
end

