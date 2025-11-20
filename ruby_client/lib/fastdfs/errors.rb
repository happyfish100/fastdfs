# FastDFS Error Definitions
#
# This module defines all error types and error handling utilities for the
# FastDFS Ruby client. Errors are categorized into common errors, protocol
# errors, network errors, and server errors.
#
# All errors inherit from StandardError and can be rescued and handled
# appropriately by client code.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

module FastDFS
  # Base error class for all FastDFS client errors.
  #
  # All other FastDFS errors inherit from this class, making it easy to
  # catch all FastDFS-related errors with a single rescue clause.
  #
  # @example Catch all FastDFS errors
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue FastDFS::Error => e
  #     puts "FastDFS error: #{e.message}"
  #   end
  class Error < StandardError
  end
  
  # Client has been closed error.
  #
  # This error is raised when attempting to perform an operation on a
  # client that has been closed. Once a client is closed, no further
  # operations are allowed.
  #
  # @example Handle closed client
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue ClientClosedError => e
  #     puts "Client is closed: #{e.message}"
  #   end
  class ClientClosedError < Error
    # Creates a new ClientClosedError.
    #
    # @param message [String] Error message (default: "client is closed").
    def initialize(message = "client is closed")
      super(message)
    end
  end
  
  # File not found error.
  #
  # This error is raised when attempting to access a file that does not
  # exist on the storage server. It can occur during download, delete,
  # metadata operations, or file info queries.
  #
  # @example Handle file not found
  #   begin
  #     client.download_file(file_id)
  #   rescue FileNotFoundError => e
  #     puts "File not found: #{e.message}"
  #   end
  class FileNotFoundError < Error
    # Creates a new FileNotFoundError.
    #
    # @param message [String] Error message (default: "file not found").
    def initialize(message = "file not found")
      super(message)
    end
  end
  
  # No storage server available error.
  #
  # This error is raised when no storage server is available to handle
  # a request. This can happen if all storage servers are offline or
  # if there are no storage servers configured in the cluster.
  #
  # @example Handle no storage server
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue NoStorageServerError => e
  #     puts "No storage server: #{e.message}"
  #   end
  class NoStorageServerError < Error
    # Creates a new NoStorageServerError.
    #
    # @param message [String] Error message (default: "no storage server available").
    def initialize(message = "no storage server available")
      super(message)
    end
  end
  
  # Connection timeout error.
  #
  # This error is raised when a connection to a tracker or storage server
  # cannot be established within the configured connection timeout period.
  #
  # @example Handle connection timeout
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue ConnectionTimeoutError => e
  #     puts "Connection timeout: #{e.message}"
  #   end
  class ConnectionTimeoutError < Error
    # Creates a new ConnectionTimeoutError.
    #
    # @param message [String] Error message (default: "connection timeout").
    def initialize(message = "connection timeout")
      super(message)
    end
  end
  
  # Network timeout error.
  #
  # This error is raised when a network I/O operation (read or write) does
  # not complete within the configured network timeout period.
  #
  # @example Handle network timeout
  #   begin
  #     client.download_file(file_id)
  #   rescue NetworkTimeoutError => e
  #     puts "Network timeout: #{e.message}"
  #   end
  class NetworkTimeoutError < Error
    # Creates a new NetworkTimeoutError.
    #
    # @param message [String] Error message (default: "network timeout").
    def initialize(message = "network timeout")
      super(message)
    end
  end
  
  # Invalid file ID error.
  #
  # This error is raised when a file ID format is invalid. File IDs must
  # be in the format "group/remote_filename" where group is the storage
  # group name and remote_filename is the path on the storage server.
  #
  # @example Handle invalid file ID
  #   begin
  #     client.download_file("invalid-file-id")
  #   rescue InvalidFileIDError => e
  #     puts "Invalid file ID: #{e.message}"
  #   end
  class InvalidFileIDError < Error
    # Creates a new InvalidFileIDError.
    #
    # @param message [String] Error message (default: "invalid file ID").
    def initialize(message = "invalid file ID")
      super(message)
    end
  end
  
  # Invalid response error.
  #
  # This error is raised when the server response is invalid or malformed.
  # This can happen if the protocol is corrupted, the server version is
  # incompatible, or there is a communication error.
  #
  # @example Handle invalid response
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue InvalidResponseError => e
    #     puts "Invalid response: #{e.message}"
    #   end
  class InvalidResponseError < Error
    # Creates a new InvalidResponseError.
    #
    # @param message [String] Error message (default: "invalid response from server").
    def initialize(message = "invalid response from server")
      super(message)
    end
  end
  
  # Storage server offline error.
  #
  # This error is raised when attempting to communicate with a storage
  # server that is offline or unavailable.
  #
  # @example Handle storage server offline
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue StorageServerOfflineError => e
  #     puts "Storage server offline: #{e.message}"
  #   end
  class StorageServerOfflineError < Error
    # Creates a new StorageServerOfflineError.
    #
    # @param message [String] Error message (default: "storage server is offline").
    def initialize(message = "storage server is offline")
      super(message)
    end
  end
  
  # Tracker server offline error.
  #
  # This error is raised when attempting to communicate with a tracker
  # server that is offline or unavailable.
  #
  # @example Handle tracker server offline
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue TrackerServerOfflineError => e
  #     puts "Tracker server offline: #{e.message}"
  #   end
  class TrackerServerOfflineError < Error
    # Creates a new TrackerServerOfflineError.
    #
    # @param message [String] Error message (default: "tracker server is offline").
    def initialize(message = "tracker server is offline")
      super(message)
    end
  end
  
  # Insufficient space error.
  #
  # This error is raised when attempting to upload a file but there is
  # insufficient storage space available on the storage server.
  #
  # @example Handle insufficient space
  #   begin
  #     client.upload_file('large-file.bin')
  #   rescue InsufficientSpaceError => e
  #     puts "Insufficient space: #{e.message}"
  #   end
  class InsufficientSpaceError < Error
    # Creates a new InsufficientSpaceError.
    #
    # @param message [String] Error message (default: "insufficient storage space").
    def initialize(message = "insufficient storage space")
      super(message)
    end
  end
  
  # File already exists error.
  #
  # This error is raised when attempting to upload a file but a file with
  # the same ID already exists. This is rare in FastDFS as file IDs are
  # typically unique, but can occur in edge cases.
  #
  # @example Handle file already exists
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue FileAlreadyExistsError => e
  #     puts "File already exists: #{e.message}"
  #   end
  class FileAlreadyExistsError < Error
    # Creates a new FileAlreadyExistsError.
    #
    # @param message [String] Error message (default: "file already exists").
    def initialize(message = "file already exists")
      super(message)
    end
  end
  
  # Invalid metadata error.
  #
  # This error is raised when metadata format is invalid. Metadata keys
  # must be 64 characters or less and values must be 256 characters or less.
  #
  # @example Handle invalid metadata
  #   begin
  #     metadata = { 'key' => 'x' * 300 }  # Value too long
  #     client.set_metadata(file_id, metadata)
  #   rescue InvalidMetadataError => e
  #     puts "Invalid metadata: #{e.message}"
  #   end
  class InvalidMetadataError < Error
    # Creates a new InvalidMetadataError.
    #
    # @param message [String] Error message (default: "invalid metadata").
    def initialize(message = "invalid metadata")
      super(message)
    end
  end
  
  # Operation not supported error.
  #
  # This error is raised when attempting to perform an operation that is
  # not supported for the given file type. For example, appending to a
  # regular file or modifying a non-appender file.
  #
  # @example Handle operation not supported
  #   begin
  #     # Try to append to a regular file
  #     client.append_file(file_id, "data")
  #   rescue OperationNotSupportedError => e
  #     puts "Operation not supported: #{e.message}"
  #   end
  class OperationNotSupportedError < Error
    # Creates a new OperationNotSupportedError.
    #
    # @param message [String] Error message (default: "operation not supported").
    def initialize(message = "operation not supported")
      super(message)
    end
  end
  
  # Invalid argument error.
  #
  # This error is raised when an invalid argument is passed to a method.
  # For example, passing nil where a value is required, or passing a
  # negative value where a positive value is expected.
  #
  # @example Handle invalid argument
  #   begin
  #     client.upload_buffer(nil, 'txt')  # nil data
  #   rescue InvalidArgumentError => e
  #     puts "Invalid argument: #{e.message}"
  #   end
  class InvalidArgumentError < Error
    # Creates a new InvalidArgumentError.
    #
    # @param message [String] Error message (default: "invalid argument").
    def initialize(message = "invalid argument")
      super(message)
    end
  end
  
  # Protocol error.
  #
  # This error represents a protocol-level error returned by the FastDFS
  # server. It includes the error code from the protocol header and a
  # descriptive message.
  #
  # Protocol errors indicate issues with the request format or server-side
  # problems that are communicated through the protocol status field.
  #
  # @example Handle protocol error
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue ProtocolError => e
  #     puts "Protocol error (code #{e.code}): #{e.message}"
  #   end
  class ProtocolError < Error
    # Error code from the protocol status field.
    #
    # @return [Integer] Error code.
    attr_reader :code
    
    # Creates a new ProtocolError.
    #
    # @param code [Integer] Error code from the protocol.
    # @param message [String] Error message.
    def initialize(code, message)
      @code = code
      super("protocol error (code #{code}): #{message}")
    end
  end
  
  # Network error.
  #
  # This error represents a network-related error during communication
  # with a FastDFS server. It wraps the underlying network error with
  # context about the operation and server address.
  #
  # Network errors typically indicate connectivity issues, socket errors,
  # or timeouts during network I/O operations.
  #
  # @example Handle network error
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue NetworkError => e
  #     puts "Network error during #{e.op} to #{e.addr}: #{e.original_error}"
  #   end
  class NetworkError < Error
    # Operation being performed when the error occurred.
    #
    # @return [String] Operation name (e.g., "dial", "read", "write").
    attr_reader :op
    
    # Server address where the error occurred.
    #
    # @return [String] Server address (e.g., "192.168.1.100:22122").
    attr_reader :addr
    
    # Underlying network error.
    #
    # @return [Exception] Original error that caused this network error.
    attr_reader :original_error
    
    # Creates a new NetworkError.
    #
    # @param op [String] Operation being performed.
    # @param addr [String] Server address.
    # @param original_error [Exception] Underlying error.
    def initialize(op, addr, original_error)
      @op = op
      @addr = addr
      @original_error = original_error
      super("network error during #{op} to #{addr}: #{original_error.message}")
    end
  end
  
  # Storage error.
  #
  # This error represents an error from a storage server. It wraps the
  # underlying error with the storage server address for context.
  #
  # @example Handle storage error
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue StorageError => e
  #     puts "Storage error from #{e.server}: #{e.original_error}"
  #   end
  class StorageError < Error
    # Storage server address where the error occurred.
    #
    # @return [String] Storage server address.
    attr_reader :server
    
    # Underlying error.
    #
    # @return [Exception] Original error that caused this storage error.
    attr_reader :original_error
    
    # Creates a new StorageError.
    #
    # @param server [String] Storage server address.
    # @param original_error [Exception] Underlying error.
    def initialize(server, original_error)
      @server = server
      @original_error = original_error
      super("storage error from #{server}: #{original_error.message}")
    end
  end
  
  # Tracker error.
  #
  # This error represents an error from a tracker server. It wraps the
  # underlying error with the tracker server address for context.
  #
  # @example Handle tracker error
  #   begin
  #     client.upload_file('test.jpg')
  #   rescue TrackerError => e
  #     puts "Tracker error from #{e.server}: #{e.original_error}"
  #   end
  class TrackerError < Error
    # Tracker server address where the error occurred.
    #
    # @return [String] Tracker server address.
    attr_reader :server
    
    # Underlying error.
    #
    # @return [Exception] Original error that caused this tracker error.
    attr_reader :original_error
    
    # Creates a new TrackerError.
    #
    # @param server [String] Tracker server address.
    # @param original_error [Exception] Underlying error.
    def initialize(server, original_error)
      @server = server
      @original_error = original_error
      super("tracker error from #{server}: #{original_error.message}")
    end
  end
  
  # Maps FastDFS protocol status codes to Ruby errors.
  #
  # Status code 0 indicates success (no error). Other status codes are
  # mapped to predefined errors or a ProtocolError.
  #
  # Common status codes:
  #   - 0: Success
  #   - 2: File not found (ENOENT)
  #   - 6: File already exists (EEXIST)
  #   - 22: Invalid argument (EINVAL)
  #   - 28: Insufficient space (ENOSPC)
  #
  # @param status [Integer] The status byte from the protocol header.
  #
  # @return [Error, nil] The corresponding error, or nil for success.
  #
  # @example Map status to error
  #   error = FastDFS.map_status_to_error(2)
  #   # => #<FastDFS::FileNotFoundError: file not found>
  def self.map_status_to_error(status)
    # Handle success case
    # Status 0 means no error
    return nil if status == 0
    
    # Map common status codes to specific errors
    # These are the most common error codes from FastDFS
    case status
    when 2
      # File not found
      # The requested file does not exist on the storage server
      FileNotFoundError.new
    when 6
      # File already exists
      # A file with this ID already exists
      FileAlreadyExistsError.new
    when 22
      # Invalid argument
      # The request parameters are invalid
      InvalidArgumentError.new
    when 28
      # Insufficient space
      # There is not enough storage space available
      InsufficientSpaceError.new
    else
      # Unknown error code
      # Create a generic protocol error with the code
      ProtocolError.new(status, "unknown error code: #{status}")
    end
  end
end

