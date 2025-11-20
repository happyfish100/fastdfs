# FastDFS Ruby Client
#
# Main client class for interacting with FastDFS distributed file system.
#
# This module provides a Ruby client library for FastDFS, enabling Ruby applications
# to upload, download, delete, and manage files stored in a FastDFS cluster.
#
# The client handles connection pooling, automatic retries, error handling, and
# provides a simple Ruby-like API for interacting with FastDFS servers.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.
#
# @example Basic usage
#   require 'fastdfs'
#
#   # Create client configuration
#   config = FastDFS::ClientConfig.new(
#     tracker_addrs: ['192.168.1.100:22122', '192.168.1.101:22122'],
#     max_conns: 100,
#     connect_timeout: 5.0,
#     network_timeout: 30.0
#   )
#
#   # Initialize client
#   client = FastDFS::Client.new(config)
#
#   # Upload a file
#   file_id = client.upload_file('test.jpg')
#
#   # Download the file
#   data = client.download_file(file_id)
#
#   # Delete the file
#   client.delete_file(file_id)
#
#   # Close the client
#   client.close

require 'socket'
require 'timeout'
require 'thread'
require 'uri'

# Require all dependent modules
require_relative 'client_config'
require_relative 'connection_pool'
require_relative 'operations'
require_relative 'types'
require_relative 'errors'

module FastDFS
  # Client class for interacting with FastDFS distributed file system.
  #
  # This class provides a high-level Ruby API for FastDFS operations including
  # file upload, download, deletion, metadata management, and appender file operations.
  #
  # The client is thread-safe and can be used concurrently from multiple threads.
  # It manages connection pooling internally and handles automatic retries for
  # transient failures.
  #
  # @example Create and use a client
  #   config = FastDFS::ClientConfig.new(tracker_addrs: ['127.0.0.1:22122'])
  #   client = FastDFS::Client.new(config)
  #   begin
  #     file_id = client.upload_file('document.pdf')
  #     data = client.download_file(file_id)
  #     client.delete_file(file_id)
  #   ensure
  #     client.close
  #   end
  #
  # @see ClientConfig
  # @see Operations
  # @see ConnectionPool
  class Client
    # Initializes a new FastDFS client with the given configuration.
    #
    # This constructor creates connection pools for tracker and storage servers,
    # validates the configuration, and prepares the client for use.
    #
    # @param config [ClientConfig] The client configuration containing tracker
    #   addresses, timeouts, and other settings.
    #
    # @raise [InvalidArgumentError] If the configuration is invalid or missing
    #   required parameters such as tracker addresses.
    #
    # @raise [ConnectionError] If unable to establish initial connections to
    #   tracker servers (non-blocking, may succeed later).
    #
    # @example Basic initialization
    #   config = FastDFS::ClientConfig.new(
    #     tracker_addrs: ['192.168.1.100:22122']
    #   )
    #   client = FastDFS::Client.new(config)
    #
    # @example Full configuration
    #   config = FastDFS::ClientConfig.new(
    #     tracker_addrs: ['192.168.1.100:22122', '192.168.1.101:22122'],
    #     max_conns: 100,
    #     connect_timeout: 5.0,
    #     network_timeout: 30.0,
    #     idle_timeout: 60.0,
    #     retry_count: 3
    #   )
    #   client = FastDFS::Client.new(config)
    def initialize(config)
      # Validate configuration before proceeding
      # This ensures all required parameters are present and valid
      _validate_config(config)
      
      # Store the configuration for later use
      # We'll need these settings for various operations
      @config = config
      
      # Track whether the client has been closed
      # Once closed, no further operations are allowed
      @closed = false
      
      # Mutex for thread-safe operations
      # This ensures that concurrent operations don't interfere with each other
      @mutex = Mutex.new
      
      # Initialize connection pool for tracker servers
      # Tracker servers are used to locate storage servers for operations
      # The pool manages multiple connections for load balancing and failover
      @tracker_pool = ConnectionPool.new(
        addrs: config.tracker_addrs,
        max_conns: config.max_conns,
        connect_timeout: config.connect_timeout,
        idle_timeout: config.idle_timeout
      )
      
      # Initialize connection pool for storage servers
      # Storage servers are discovered dynamically through tracker queries
      # We start with an empty list and add servers as they are discovered
      @storage_pool = ConnectionPool.new(
        addrs: [],  # Storage servers are discovered dynamically
        max_conns: config.max_conns,
        connect_timeout: config.connect_timeout,
        idle_timeout: config.idle_timeout
      )
      
      # Initialize operations handler
      # This object handles all file operations such as upload, download, delete
      # It uses the connection pools and implements retry logic
      @operations = Operations.new(
        tracker_pool: @tracker_pool,
        storage_pool: @storage_pool,
        network_timeout: config.network_timeout,
        retry_count: config.retry_count
      )
      
      # Client initialization complete
      # All resources have been allocated and the client is ready for use
    end
    
    # Uploads a file from the local filesystem to FastDFS.
    #
    # This method reads the file from the local filesystem, uploads it to a
    # storage server, and returns a file ID that can be used to reference the
    # file in subsequent operations.
    #
    # @param local_filename [String] Path to the local file to upload.
    #
    # @param metadata [Hash<String, String>] Optional metadata key-value pairs
    #   to associate with the file. Keys and values are limited to 64 and 256
    #   characters respectively.
    #
    # @return [String] The file ID in the format "group/remote_filename".
    #   This ID can be used to download or delete the file.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the local file does not exist or cannot
    #   be read.
    #
    # @raise [NetworkError] If network communication fails after retries.
    #
    # @raise [StorageError] If the storage server reports an error.
    #
    # @example Upload a simple file
    #   file_id = client.upload_file('test.jpg')
    #
    # @example Upload with metadata
    #   metadata = { 'author' => 'John Doe', 'date' => '2025-01-01' }
    #   file_id = client.upload_file('document.pdf', metadata)
    def upload_file(local_filename, metadata = nil)
      # Check if client is closed before proceeding
      # Closed clients cannot perform operations
      _check_closed
      
      # Delegate to operations handler
      # This handles retry logic, error handling, and protocol communication
      @operations.upload_file(local_filename, metadata, is_appender: false)
    end
    
    # Uploads data from a byte buffer to FastDFS.
    #
    # This method uploads raw binary data directly to FastDFS without requiring
    # a file on the local filesystem. This is useful for in-memory data such as
    # generated content or data received from network requests.
    #
    # @param data [String] The file content as binary data (bytes).
    #
    # @param file_ext_name [String] File extension without dot (e.g., "jpg", "txt").
    #   Maximum length is 6 characters.
    #
    # @param metadata [Hash<String, String>] Optional metadata key-value pairs.
    #
    # @return [String] The file ID for the uploaded file.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [InvalidArgumentError] If the data is nil or file extension is invalid.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Upload from memory
    #   data = "Hello, FastDFS!"
    #   file_id = client.upload_buffer(data, 'txt')
    #
    # @example Upload image data
    #   image_data = File.read('image.jpg', mode: 'rb')
    #   file_id = client.upload_buffer(image_data, 'jpg')
    def upload_buffer(data, file_ext_name, metadata = nil)
      # Ensure client is still open
      # We cannot perform operations on closed clients
      _check_closed
      
      # Validate input parameters
      # Data must not be nil and file extension must be valid
      raise InvalidArgumentError, "data cannot be nil" if data.nil?
      raise InvalidArgumentError, "file_ext_name cannot be nil" if file_ext_name.nil?
      
      # Delegate to operations handler
      # This will handle all the protocol communication
      @operations.upload_buffer(data, file_ext_name, metadata, is_appender: false)
    end
    
    # Uploads an appender file from the local filesystem.
    #
    # Appender files can be modified after upload using append, modify, and
    # truncate operations. They are useful for log files or files that need
    # to grow over time.
    #
    # @param local_filename [String] Path to the local file to upload.
    #
    # @param metadata [Hash<String, String>] Optional metadata key-value pairs.
    #
    # @return [String] The file ID for the appender file.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the local file does not exist.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Upload an appender file
    #   file_id = client.upload_appender_file('log.txt')
    #   client.append_file(file_id, "New log entry\n")
    def upload_appender_file(local_filename, metadata = nil)
      # Check client state
      # Operations cannot be performed on closed clients
      _check_closed
      
      # Delegate to operations handler with appender flag
      # This tells the protocol to use appender file upload command
      @operations.upload_file(local_filename, metadata, is_appender: true)
    end
    
    # Uploads an appender file from a byte buffer.
    #
    # @param data [String] The file content as binary data.
    #
    # @param file_ext_name [String] File extension without dot.
    #
    # @param metadata [Hash<String, String>] Optional metadata.
    #
    # @return [String] The file ID for the appender file.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [InvalidArgumentError] If parameters are invalid.
    #
    # @raise [NetworkError] If network communication fails.
    def upload_appender_buffer(data, file_ext_name, metadata = nil)
      # Ensure client is open
      # Closed clients cannot perform operations
      _check_closed
      
      # Validate inputs
      # Both data and file extension must be provided
      raise InvalidArgumentError, "data cannot be nil" if data.nil?
      raise InvalidArgumentError, "file_ext_name cannot be nil" if file_ext_name.nil?
      
      # Delegate to operations handler
      # This will use the appender file upload protocol
      @operations.upload_buffer(data, file_ext_name, metadata, is_appender: true)
    end
    
    # Uploads a slave file associated with a master file.
    #
    # Slave files are typically thumbnails, previews, or other variants of a
    # master file. They are stored on the same storage server as the master
    # file and share the same group.
    #
    # @param master_file_id [String] The file ID of the master file.
    #
    # @param prefix_name [String] Prefix for the slave file (e.g., "thumb", "small").
    #   Maximum length is 16 characters.
    #
    # @param file_ext_name [String] File extension without dot.
    #
    # @param data [String] The slave file content as binary data.
    #
    # @param metadata [Hash<String, String>] Optional metadata.
    #
    # @return [String] The file ID for the slave file.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [InvalidArgumentError] If parameters are invalid.
    #
    # @raise [FileNotFoundError] If the master file does not exist.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Upload a thumbnail
    #   master_id = client.upload_file('photo.jpg')
    #   thumbnail_data = generate_thumbnail(photo_data)
    #   thumb_id = client.upload_slave_file(master_id, 'thumb', 'jpg', thumbnail_data)
    def upload_slave_file(master_file_id, prefix_name, file_ext_name, data, metadata = nil)
      # Check client state
      # Operations require an open client
      _check_closed
      
      # Validate all required parameters
      # Master file ID, prefix, extension, and data are all required
      raise InvalidArgumentError, "master_file_id cannot be nil" if master_file_id.nil?
      raise InvalidArgumentError, "prefix_name cannot be nil" if prefix_name.nil?
      raise InvalidArgumentError, "file_ext_name cannot be nil" if file_ext_name.nil?
      raise InvalidArgumentError, "data cannot be nil" if data.nil?
      
      # Delegate to operations handler
      # This will upload the slave file to the same storage as the master
      @operations.upload_slave_file(master_file_id, prefix_name, file_ext_name, data, metadata)
    end
    
    # Downloads a file from FastDFS and returns its content.
    #
    # @param file_id [String] The file ID to download.
    #
    # @return [String] The file content as binary data.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Download a file
    #   data = client.download_file(file_id)
    #   File.write('downloaded.jpg', data, mode: 'wb')
    def download_file(file_id)
      # Ensure client is open
      # Closed clients cannot download files
      _check_closed
      
      # Delegate to operations handler
      # This will download the entire file
      @operations.download_file(file_id, offset: 0, length: 0)
    end
    
    # Downloads a specific range of bytes from a file.
    #
    # This method allows partial file downloads, which is useful for large files
    # or when implementing resumable downloads.
    #
    # @param file_id [String] The file ID to download.
    #
    # @param offset [Integer] Starting byte offset (0-based).
    #
    # @param length [Integer] Number of bytes to download. If 0, downloads
    #   from offset to end of file.
    #
    # @return [String] The requested file content as binary data.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Download a range
    #   # Download first 1024 bytes
    #   header = client.download_file_range(file_id, 0, 1024)
    #
    # @example Download from offset to end
    #   # Download everything from byte 1000 onwards
    #   tail = client.download_file_range(file_id, 1000, 0)
    def download_file_range(file_id, offset, length)
      # Check client state
      # Operations require an open client
      _check_closed
      
      # Validate offset
      # Offset must be non-negative
      raise InvalidArgumentError, "offset must be >= 0" if offset < 0
      raise InvalidArgumentError, "length must be >= 0" if length < 0
      
      # Delegate to operations handler
      # This will request the specified byte range from the server
      @operations.download_file(file_id, offset: offset, length: length)
    end
    
    # Downloads a file and saves it to the local filesystem.
    #
    # @param file_id [String] The file ID to download.
    #
    # @param local_filename [String] Path where to save the downloaded file.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [IOError] If unable to write to the local file.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Download to file
    #   client.download_to_file(file_id, '/path/to/save/image.jpg')
    def download_to_file(file_id, local_filename)
      # Ensure client is open
      # Closed clients cannot perform downloads
      _check_closed
      
      # Validate local filename
      # Must provide a valid path to save the file
      raise InvalidArgumentError, "local_filename cannot be nil" if local_filename.nil?
      
      # Delegate to operations handler
      # This will download and save the file in one operation
      @operations.download_to_file(file_id, local_filename)
    end
    
    # Deletes a file from FastDFS.
    #
    # @param file_id [String] The file ID to delete.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Delete a file
    #   client.delete_file(file_id)
    def delete_file(file_id)
      # Check client state
      # Operations require an open client
      _check_closed
      
      # Validate file ID
      # Must provide a valid file ID
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      
      # Delegate to operations handler
      # This will send the delete command to the storage server
      @operations.delete_file(file_id)
    end
    
    # Appends data to an appender file.
    #
    # This method adds data to the end of an appender file. The file must have
    # been uploaded as an appender file using upload_appender_file or
    # upload_appender_buffer.
    #
    # @param file_id [String] The file ID of the appender file.
    #
    # @param data [String] The data to append as binary data.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [OperationNotSupportedError] If the file is not an appender file.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Append to a log file
    #   file_id = client.upload_appender_file('log.txt')
    #   client.append_file(file_id, "Entry 1\n")
    #   client.append_file(file_id, "Entry 2\n")
    def append_file(file_id, data)
      # Ensure client is open
      # Closed clients cannot perform append operations
      _check_closed
      
      # Validate parameters
      # Both file ID and data must be provided
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      raise InvalidArgumentError, "data cannot be nil" if data.nil?
      
      # Delegate to operations handler
      # This will append the data to the end of the file
      @operations.append_file(file_id, data)
    end
    
    # Modifies content of an appender file at specified offset.
    #
    # This method overwrites data in an appender file starting at the given
    # offset. The file must be an appender file.
    #
    # @param file_id [String] The file ID of the appender file.
    #
    # @param offset [Integer] Byte offset where to start modifying (0-based).
    #
    # @param data [String] The new data as binary data.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [OperationNotSupportedError] If the file is not an appender file.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Modify file content
    #   client.modify_file(file_id, 0, "New header\n")
    def modify_file(file_id, offset, data)
      # Check client state
      # Operations require an open client
      _check_closed
      
      # Validate all parameters
      # File ID, offset, and data are all required
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      raise InvalidArgumentError, "offset must be >= 0" if offset < 0
      raise InvalidArgumentError, "data cannot be nil" if data.nil?
      
      # Delegate to operations handler
      # This will overwrite the file content at the specified offset
      @operations.modify_file(file_id, offset, data)
    end
    
    # Truncates an appender file to specified size.
    #
    # This method reduces the size of an appender file to the given length.
    # Data beyond the new size is permanently lost.
    #
    # @param file_id [String] The file ID of the appender file.
    #
    # @param size [Integer] The new size in bytes.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [OperationNotSupportedError] If the file is not an appender file.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Truncate a file
    #   client.truncate_file(file_id, 1024)  # Truncate to 1KB
    def truncate_file(file_id, size)
      # Ensure client is open
      # Closed clients cannot perform truncate operations
      _check_closed
      
      # Validate parameters
      # File ID and size must be provided and valid
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      raise InvalidArgumentError, "size must be >= 0" if size < 0
      
      # Delegate to operations handler
      # This will truncate the file to the specified size
      @operations.truncate_file(file_id, size)
    end
    
    # Sets metadata for a file.
    #
    # Metadata can be used to store custom key-value pairs associated with a
    # file. Keys are limited to 64 characters and values to 256 characters.
    #
    # @param file_id [String] The file ID.
    #
    # @param metadata [Hash<String, String>] Metadata key-value pairs.
    #
    # @param flag [Symbol] Metadata operation flag: :overwrite (replace all
    #   existing metadata) or :merge (merge with existing metadata).
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [InvalidMetadataError] If metadata format is invalid.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Set metadata with overwrite
    #   metadata = { 'author' => 'John Doe', 'date' => '2025-01-01' }
    #   client.set_metadata(file_id, metadata, :overwrite)
    #
    # @example Merge metadata
    #   new_metadata = { 'version' => '2.0' }
    #   client.set_metadata(file_id, new_metadata, :merge)
    def set_metadata(file_id, metadata, flag = :overwrite)
      # Check client state
      # Operations require an open client
      _check_closed
      
      # Validate parameters
      # File ID and metadata are required
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      raise InvalidArgumentError, "metadata cannot be nil" if metadata.nil?
      
      # Validate flag
      # Must be either :overwrite or :merge
      unless [:overwrite, :merge].include?(flag)
        raise InvalidArgumentError, "flag must be :overwrite or :merge"
      end
      
      # Delegate to operations handler
      # This will set or merge the metadata as specified
      @operations.set_metadata(file_id, metadata, flag)
    end
    
    # Retrieves metadata for a file.
    #
    # @param file_id [String] The file ID.
    #
    # @return [Hash<String, String>] Dictionary of metadata key-value pairs.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Get metadata
    #   metadata = client.get_metadata(file_id)
    #   puts "Author: #{metadata['author']}"
    def get_metadata(file_id)
      # Ensure client is open
      # Closed clients cannot retrieve metadata
      _check_closed
      
      # Validate file ID
      # Must provide a valid file ID
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      
      # Delegate to operations handler
      # This will retrieve the metadata from the storage server
      @operations.get_metadata(file_id)
    end
    
    # Retrieves file information including size, create time, and CRC32.
    #
    # @param file_id [String] The file ID.
    #
    # @return [FileInfo] Object containing file information.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [FileNotFoundError] If the file does not exist.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [NetworkError] If network communication fails.
    #
    # @example Get file info
    #   info = client.get_file_info(file_id)
    #   puts "Size: #{info.file_size} bytes"
    #   puts "Created: #{info.create_time}"
    #   puts "CRC32: #{info.crc32}"
    def get_file_info(file_id)
      # Check client state
      # Operations require an open client
      _check_closed
      
      # Validate file ID
      # Must provide a valid file ID
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      
      # Delegate to operations handler
      # This will query the file information from the storage server
      @operations.get_file_info(file_id)
    end
    
    # Checks if a file exists on the storage server.
    #
    # This method attempts to retrieve file information. If successful, the
    # file exists. If FileNotFoundError is raised, the file does not exist.
    #
    # @param file_id [String] The file ID to check.
    #
    # @return [Boolean] True if file exists, false otherwise.
    #
    # @raise [ClientClosedError] If the client has been closed.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    #
    # @raise [NetworkError] If network communication fails (not file not found).
    #
    # @example Check if file exists
    #   if client.file_exists?(file_id)
    #     puts "File exists"
    #   else
    #     puts "File does not exist"
    #   end
    def file_exists?(file_id)
      # Ensure client is open
      # Closed clients cannot check file existence
      _check_closed
      
      # Validate file ID
      # Must provide a valid file ID
      raise InvalidArgumentError, "file_id cannot be nil" if file_id.nil?
      
      # Try to get file info
      # If successful, file exists
      begin
        get_file_info(file_id)
        true
      rescue FileNotFoundError
        # File not found means it doesn't exist
        # Return false in this case
        false
      end
    end
    
    # Closes the client and releases all resources.
    #
    # After calling close, all operations will raise ClientClosedError.
    # It is safe to call close multiple times.
    #
    # @return [void]
    #
    # @example Close the client
    #   client.close
    #
    # @example Use with ensure block
    #   client = FastDFS::Client.new(config)
    #   begin
    #     # Use client...
    #   ensure
    #     client.close
    #   end
    def close
      # Acquire mutex for thread-safe closing
      # Only one thread can close the client at a time
      @mutex.synchronize do
        # Check if already closed
        # Multiple calls to close should be safe
        return if @closed
        
        # Mark as closed
        # This prevents further operations
        @closed = true
        
        # Close tracker connection pool
        # Release all connections to tracker servers
        if @tracker_pool
          begin
            @tracker_pool.close
          rescue => e
            # Log error but don't fail
            # We want to close all resources even if one fails
            # In production, you might want to log this error
          end
        end
        
        # Close storage connection pool
        # Release all connections to storage servers
        if @storage_pool
          begin
            @storage_pool.close
          rescue => e
            # Log error but don't fail
            # Similar to tracker pool, we continue closing
            # In production, you might want to log this error
          end
        end
        
        # Clear references
        # Help garbage collector by clearing references
        @tracker_pool = nil
        @storage_pool = nil
        @operations = nil
        
        # Client is now fully closed
        # All resources have been released
      end
    end
    
    # Checks if the client is closed.
    #
    # @return [Boolean] True if closed, false otherwise.
    #
    # @example Check if closed
    #   if client.closed?
    #     puts "Client is closed"
    #   end
    def closed?
      # Thread-safe check
      # Use mutex to ensure consistency
      @mutex.synchronize { @closed }
    end
    
    private
    
    # Validates the client configuration.
    #
    # This method checks that all required parameters are present and valid.
    # It raises InvalidArgumentError if the configuration is invalid.
    #
    # @param config [ClientConfig] The configuration to validate.
    #
    # @raise [InvalidArgumentError] If the configuration is invalid.
    #
    # @return [void]
    def _validate_config(config)
      # Check if config is nil
      # Configuration is required for client initialization
      if config.nil?
        raise InvalidArgumentError, "config cannot be nil"
      end
      
      # Check if config is a ClientConfig instance
      # Type checking ensures we have the right kind of object
      unless config.is_a?(ClientConfig)
        raise InvalidArgumentError, "config must be a ClientConfig instance"
      end
      
      # Check tracker addresses
      # At least one tracker address is required
      if config.tracker_addrs.nil? || config.tracker_addrs.empty?
        raise InvalidArgumentError, "tracker_addrs cannot be nil or empty"
      end
      
      # Validate each tracker address
      # Addresses must be in format "host:port"
      config.tracker_addrs.each do |addr|
        # Check for nil or empty
        if addr.nil? || addr.empty?
          raise InvalidArgumentError, "tracker address cannot be nil or empty"
        end
        
        # Check format (should contain colon)
        unless addr.include?(':')
          raise InvalidArgumentError, "tracker address must be in format 'host:port': #{addr}"
        end
        
        # Try to parse address
        # This validates that it's a valid address format
        begin
          host, port_str = addr.split(':', 2)
          port = port_str.to_i
          
          # Validate host is not empty
          if host.nil? || host.empty?
            raise InvalidArgumentError, "tracker address host cannot be empty: #{addr}"
          end
          
          # Validate port is valid
          if port <= 0 || port > 65535
            raise InvalidArgumentError, "tracker address port must be 1-65535: #{addr}"
          end
        rescue => e
          raise InvalidArgumentError, "invalid tracker address format: #{addr} - #{e.message}"
        end
      end
      
      # Validate max_conns if provided
      # Must be positive if specified
      if !config.max_conns.nil? && config.max_conns <= 0
        raise InvalidArgumentError, "max_conns must be positive"
      end
      
      # Validate timeouts if provided
      # Must be positive if specified
      if !config.connect_timeout.nil? && config.connect_timeout <= 0
        raise InvalidArgumentError, "connect_timeout must be positive"
      end
      
      if !config.network_timeout.nil? && config.network_timeout <= 0
        raise InvalidArgumentError, "network_timeout must be positive"
      end
      
      if !config.idle_timeout.nil? && config.idle_timeout <= 0
        raise InvalidArgumentError, "idle_timeout must be positive"
      end
      
      # Validate retry_count if provided
      # Must be non-negative if specified
      if !config.retry_count.nil? && config.retry_count < 0
        raise InvalidArgumentError, "retry_count must be non-negative"
      end
      
      # Configuration validation complete
      # All checks have passed
    end
    
    # Checks if the client is closed and raises an error if so.
    #
    # This is called at the beginning of every public operation method to
    # ensure the client is still open.
    #
    # @raise [ClientClosedError] If the client is closed.
    #
    # @return [void]
    def _check_closed
      # Thread-safe check
      # Use mutex to ensure consistency
      @mutex.synchronize do
        # Raise error if closed
        # Closed clients cannot perform operations
        if @closed
          raise ClientClosedError, "client is closed"
        end
      end
    end
  end
end

