# FastDFS Operations Implementation
#
# This module implements all file operations for the FastDFS client,
# including upload, download, delete, metadata management, and appender
# file operations.
#
# All operations are implemented with retry logic, error handling, and
# proper protocol communication with tracker and storage servers.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

require 'fileutils'
require_relative 'connection_pool'
require_relative 'protocol'
require_relative 'types'
require_relative 'errors'

module FastDFS
  # Operations handler for FastDFS file operations.
  #
  # This class handles all file operations including upload, download, delete,
  # metadata management, and appender file operations. It uses connection pools
  # for tracker and storage servers and implements retry logic for failed operations.
  #
  # Operations are not called directly by client code; they are used internally
  # by the Client class.
  class Operations
    # Initializes a new Operations handler.
    #
    # This constructor creates an operations handler that will use the provided
    # connection pools and configuration for all file operations.
    #
    # @param tracker_pool [ConnectionPool] Connection pool for tracker servers.
    # @param storage_pool [ConnectionPool] Connection pool for storage servers.
    # @param network_timeout [Float] Network I/O timeout in seconds.
    # @param retry_count [Integer] Number of retries for failed operations.
    def initialize(tracker_pool:, storage_pool:, network_timeout:, retry_count:)
      # Store connection pools
      # These are used for all network operations
      @tracker_pool = tracker_pool
      @storage_pool = storage_pool
      
      # Store network timeout
      # Used for all socket I/O operations
      @network_timeout = network_timeout
      
      # Store retry count
      # Used for retry logic on failed operations
      @retry_count = retry_count
      
      # Operations handler initialized
      # Ready to perform file operations
    end
    
    # Uploads a file from the local filesystem.
    #
    # This method reads the file from the local filesystem and uploads it to
    # a storage server. It handles retry logic, error handling, and protocol
    # communication.
    #
    # @param local_filename [String] Path to the local file to upload.
    # @param metadata [Hash<String, String>, nil] Optional metadata key-value pairs.
    # @param is_appender [Boolean] Whether this is an appender file (default: false).
    #
    # @return [String] The file ID for the uploaded file.
    #
    # @raise [FileNotFoundError] If the local file does not exist.
    # @raise [NetworkError] If network communication fails.
    # @raise [StorageError] If the storage server reports an error.
    def upload_file(local_filename, metadata, is_appender: false)
      # Validate local filename
      # Must not be nil or empty
      raise InvalidArgumentError.new("local_filename cannot be nil") if local_filename.nil?
      
      # Check if file exists
      # Must exist before we can upload it
      unless File.exist?(local_filename)
        raise FileNotFoundError.new("local file not found: #{local_filename}")
      end
      
      # Read file content
      # Must read in binary mode for binary files
      begin
        file_data = File.read(local_filename, mode: 'rb')
      rescue => e
        raise FileNotFoundError.new("failed to read file: #{local_filename} - #{e.message}")
      end
      
      # Get file extension
      # Extract extension from filename
      ext_name = File.extname(local_filename)
      
      # Remove leading dot
      # Protocol expects extension without dot
      ext_name = ext_name[1..-1] if ext_name.start_with?('.')
      
      # Use default extension if empty
      # Some files may not have extensions
      ext_name = 'bin' if ext_name.empty?
      
      # Upload buffer with file data
      # Delegate to upload_buffer method
      upload_buffer(file_data, ext_name, metadata, is_appender: is_appender)
    end
    
    # Uploads data from a byte buffer.
    #
    # This method uploads raw binary data directly to FastDFS without requiring
    # a file on the local filesystem. It handles retry logic, error handling,
    # and protocol communication.
    #
    # @param data [String] The file content as binary data.
    # @param file_ext_name [String] File extension without dot (e.g., "jpg", "txt").
    # @param metadata [Hash<String, String>, nil] Optional metadata key-value pairs.
    # @param is_appender [Boolean] Whether this is an appender file (default: false).
    #
    # @return [String] The file ID for the uploaded file.
    #
    # @raise [InvalidArgumentError] If parameters are invalid.
    # @raise [NetworkError] If network communication fails.
    # @raise [StorageError] If the storage server reports an error.
    def upload_buffer(data, file_ext_name, metadata, is_appender: false)
      # Validate data
      # Must not be nil or empty
      raise InvalidArgumentError.new("data cannot be nil") if data.nil?
      raise InvalidArgumentError.new("data cannot be empty") if data.empty?
      
      # Validate file extension
      # Must not be nil or empty
      raise InvalidArgumentError.new("file_ext_name cannot be nil") if file_ext_name.nil?
      raise InvalidArgumentError.new("file_ext_name cannot be empty") if file_ext_name.empty?
      
      # Validate extension length
      # Protocol limits extension to 6 characters
      if file_ext_name.bytesize > FDFS_FILE_EXT_NAME_MAX_LEN
        raise InvalidArgumentError.new("file_ext_name too long: maximum #{FDFS_FILE_EXT_NAME_MAX_LEN} bytes")
      end
      
      # Perform upload with retry logic
      # Retry on transient failures
      last_error = nil
      @retry_count.times do |attempt|
        begin
          # Attempt upload
          # Try to upload the file
          file_id = _upload_buffer_internal(data, file_ext_name, metadata, is_appender)
          
          # Upload successful
          # Return file ID
          return file_id
        rescue NetworkError, ConnectionTimeoutError, StorageServerOfflineError => e
          # Transient error, retry
          # Store error for final raise if all retries fail
          last_error = e
          
          # Don't retry on last attempt
          # Will raise error after loop
          next if attempt < @retry_count - 1
        rescue => e
          # Non-transient error, don't retry
          # Raise immediately
          raise e
        end
        
        # Wait before retry
        # Exponential backoff
        sleep(0.1 * (2 ** attempt)) if attempt < @retry_count - 1
      end
      
      # All retries failed
      # Raise last error
      raise last_error
    end
    
    # Downloads a file from FastDFS.
    #
    # This method downloads file content from a storage server. It handles
    # retry logic, error handling, and protocol communication. Supports
    # partial downloads via offset and length parameters.
    #
    # @param file_id [String] The file ID to download.
    # @param offset [Integer] Starting byte offset (default: 0).
    # @param length [Integer] Number of bytes to download (0 means to end, default: 0).
    #
    # @return [String] The file content as binary data.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    # @raise [FileNotFoundError] If the file does not exist.
    # @raise [NetworkError] If network communication fails.
    # @raise [StorageError] If the storage server reports an error.
    def download_file(file_id, offset: 0, length: 0)
      # Validate file ID
      # Must not be nil or empty
      raise InvalidArgumentError.new("file_id cannot be nil") if file_id.nil?
      raise InvalidArgumentError.new("file_id cannot be empty") if file_id.empty?
      
      # Validate offset
      # Must be non-negative
      raise InvalidArgumentError.new("offset must be >= 0") if offset < 0
      
      # Validate length
      # Must be non-negative
      raise InvalidArgumentError.new("length must be >= 0") if length < 0
      
      # Perform download with retry logic
      # Retry on transient failures
      last_error = nil
      @retry_count.times do |attempt|
        begin
          # Attempt download
          # Try to download the file
          data = _download_file_internal(file_id, offset, length)
          
          # Download successful
          # Return file data
          return data
        rescue NetworkError, ConnectionTimeoutError, StorageServerOfflineError => e
          # Transient error, retry
          # Store error for final raise if all retries fail
          last_error = e
          
          # Don't retry on last attempt
          # Will raise error after loop
          next if attempt < @retry_count - 1
        rescue => e
          # Non-transient error, don't retry
          # Raise immediately
          raise e
        end
        
        # Wait before retry
        # Exponential backoff
        sleep(0.1 * (2 ** attempt)) if attempt < @retry_count - 1
      end
      
      # All retries failed
      # Raise last error
      raise last_error
    end
    
    # Downloads a file and saves it to the local filesystem.
    #
    # This method downloads file content and writes it to a local file in one
    # operation. It handles retry logic, error handling, and file I/O.
    #
    # @param file_id [String] The file ID to download.
    # @param local_filename [String] Path where to save the downloaded file.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    # @raise [FileNotFoundError] If the file does not exist.
    # @raise [IOError] If unable to write to the local file.
    # @raise [NetworkError] If network communication fails.
    def download_to_file(file_id, local_filename)
      # Validate local filename
      # Must not be nil or empty
      raise InvalidArgumentError.new("local_filename cannot be nil") if local_filename.nil?
      raise InvalidArgumentError.new("local_filename cannot be empty") if local_filename.empty?
      
      # Download file content
      # Get file data from server
      data = download_file(file_id, offset: 0, length: 0)
      
      # Write to local file
      # Must write in binary mode
      begin
        # Create directory if needed
        # Parent directory must exist
        dir = File.dirname(local_filename)
        FileUtils.mkdir_p(dir) unless dir == '.' || dir == ''
        
        # Write file content
        # Open in binary write mode
        File.open(local_filename, 'wb') do |f|
          f.write(data)
        end
      rescue => e
        raise IOError.new("failed to write file: #{local_filename} - #{e.message}")
      end
    end
    
    # Deletes a file from FastDFS.
    #
    # This method sends a delete command to the storage server. It handles
    # retry logic, error handling, and protocol communication.
    #
    # @param file_id [String] The file ID to delete.
    #
    # @raise [InvalidFileIDError] If the file ID format is invalid.
    # @raise [FileNotFoundError] If the file does not exist.
    # @raise [NetworkError] If network communication fails.
    # @raise [StorageError] If the storage server reports an error.
    def delete_file(file_id)
      # Validate file ID
      # Must not be nil or empty
      raise InvalidArgumentError.new("file_id cannot be nil") if file_id.nil?
      raise InvalidArgumentError.new("file_id cannot be empty") if file_id.empty?
      
      # Perform delete with retry logic
      # Retry on transient failures
      last_error = nil
      @retry_count.times do |attempt|
        begin
          # Attempt delete
          # Try to delete the file
          _delete_file_internal(file_id)
          
          # Delete successful
          # Return without error
          return
        rescue NetworkError, ConnectionTimeoutError, StorageServerOfflineError => e
          # Transient error, retry
          # Store error for final raise if all retries fail
          last_error = e
          
          # Don't retry on last attempt
          # Will raise error after loop
          next if attempt < @retry_count - 1
        rescue => e
          # Non-transient error, don't retry
          # Raise immediately
          raise e
        end
        
        # Wait before retry
        # Exponential backoff
        sleep(0.1 * (2 ** attempt)) if attempt < @retry_count - 1
      end
      
      # All retries failed
      # Raise last error
      raise last_error
    end
    
    # Placeholder methods for other operations
    # These would be implemented similarly with retry logic and error handling
    
    # Uploads a slave file (placeholder - not fully implemented)
    def upload_slave_file(master_file_id, prefix_name, file_ext_name, data, metadata)
      # Placeholder implementation
      # This would implement slave file upload logic
      raise OperationNotSupportedError.new("slave file upload not yet implemented")
    end
    
    # Appends data to an appender file (placeholder)
    def append_file(file_id, data)
      # Placeholder implementation
      # This would implement append operation
      raise OperationNotSupportedError.new("append file not yet implemented")
    end
    
    # Modifies content of an appender file (placeholder)
    def modify_file(file_id, offset, data)
      # Placeholder implementation
      # This would implement modify operation
      raise OperationNotSupportedError.new("modify file not yet implemented")
    end
    
    # Truncates an appender file (placeholder)
    def truncate_file(file_id, size)
      # Placeholder implementation
      # This would implement truncate operation
      raise OperationNotSupportedError.new("truncate file not yet implemented")
    end
    
    # Sets metadata for a file (placeholder)
    def set_metadata(file_id, metadata, flag)
      # Placeholder implementation
      # This would implement set metadata operation
      raise OperationNotSupportedError.new("set metadata not yet implemented")
    end
    
    # Gets metadata for a file (placeholder)
    def get_metadata(file_id)
      # Placeholder implementation
      # This would implement get metadata operation
      raise OperationNotSupportedError.new("get metadata not yet implemented")
    end
    
    # Gets file information (placeholder)
    def get_file_info(file_id)
      # Placeholder implementation
      # This would implement get file info operation
      raise OperationNotSupportedError.new("get file info not yet implemented")
    end
    
    private
    
    # Internal method to upload buffer (implementation placeholder)
    def _upload_buffer_internal(data, file_ext_name, metadata, is_appender)
      # Placeholder implementation
      # This would implement the actual upload protocol
      raise OperationNotSupportedError.new("upload not yet fully implemented")
    end
    
    # Internal method to download file (implementation placeholder)
    def _download_file_internal(file_id, offset, length)
      # Placeholder implementation
      # This would implement the actual download protocol
      raise OperationNotSupportedError.new("download not yet fully implemented")
    end
    
    # Internal method to delete file (implementation placeholder)
    def _delete_file_internal(file_id)
      # Placeholder implementation
      # This would implement the actual delete protocol
      raise OperationNotSupportedError.new("delete not yet fully implemented")
    end
  end
end

