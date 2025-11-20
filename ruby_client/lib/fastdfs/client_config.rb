# FastDFS Client Configuration
#
# This module defines the configuration class for FastDFS clients.
# Configuration includes tracker addresses, connection pool settings,
# timeouts, retry counts, and other client behavior parameters.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

module FastDFS
  # Client configuration class.
  #
  # This class holds all configuration parameters for a FastDFS client.
  # It provides default values for optional parameters and validates
  # required parameters.
  #
  # @example Basic configuration
  #   config = FastDFS::ClientConfig.new(
  #     tracker_addrs: ['192.168.1.100:22122']
  #   )
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
  class ClientConfig
    # List of tracker server addresses.
    #
    # Each address should be in the format "host:port" (e.g., "192.168.1.100:22122").
    # At least one tracker address is required.
    #
    # Multiple tracker addresses can be provided for failover and load balancing.
    # The client will try each tracker in order until one is available.
    #
    # @return [Array<String>] Array of tracker server addresses.
    attr_accessor :tracker_addrs
    
    # Maximum number of connections per server in the connection pool.
    #
    # This limits the number of concurrent connections maintained for each
    # tracker or storage server. Higher values allow more parallelism
    # but consume more resources.
    #
    # Default: 10
    #
    # @return [Integer] Maximum connections per server.
    attr_accessor :max_conns
    
    # Connection timeout in seconds.
    #
    # This is the maximum time to wait when establishing a new connection
    # to a tracker or storage server. If the connection cannot be established
    # within this time, an error is raised.
    #
    # Default: 5.0 seconds
    #
    # @return [Float] Connection timeout in seconds.
    attr_accessor :connect_timeout
    
    # Network I/O timeout in seconds.
    #
    # This is the maximum time to wait for network I/O operations such as
    # reading or writing data. If an operation does not complete within
    # this time, it is considered failed and may be retried.
    #
    # Default: 30.0 seconds
    #
    # @return [Float] Network timeout in seconds.
    attr_accessor :network_timeout
    
    # Idle connection timeout in seconds.
    #
    # Connections that have been idle (unused) for longer than this timeout
    # will be closed and removed from the connection pool. This helps free
    # up resources when connections are not actively used.
    #
    # Default: 60.0 seconds
    #
    # @return [Float] Idle timeout in seconds.
    attr_accessor :idle_timeout
    
    # Number of retries for failed operations.
    #
    # When an operation fails due to a transient error (network timeout,
    # connection error, etc.), it will be retried up to this many times
    # before giving up.
    #
    # Default: 3
    #
    # @return [Integer] Number of retries.
    attr_accessor :retry_count
    
    # Initializes a new ClientConfig with the given parameters.
    #
    # This constructor accepts a hash of configuration parameters and
    # applies default values for optional parameters.
    #
    # @param options [Hash] Configuration parameters.
    # @option options [Array<String>] :tracker_addrs (required) List of tracker addresses.
    # @option options [Integer] :max_conns (10) Maximum connections per server.
    # @option options [Float] :connect_timeout (5.0) Connection timeout in seconds.
    # @option options [Float] :network_timeout (30.0) Network I/O timeout in seconds.
    # @option options [Float] :idle_timeout (60.0) Idle connection timeout in seconds.
    # @option options [Integer] :retry_count (3) Number of retries for failed operations.
    #
    # @raise [ArgumentError] If tracker_addrs is missing or invalid.
    #
    # @example Create configuration
    #   config = FastDFS::ClientConfig.new(
    #     tracker_addrs: ['127.0.0.1:22122'],
    #     max_conns: 50
    #   )
    def initialize(options = {})
      # Validate that tracker_addrs is provided
      # This is a required parameter
      unless options.key?(:tracker_addrs)
        raise ArgumentError, "tracker_addrs is required"
      end
      
      # Set tracker addresses
      # Must be an array of strings
      @tracker_addrs = options[:tracker_addrs]
      
      # Validate tracker addresses
      # Must be an array with at least one address
      unless @tracker_addrs.is_a?(Array) && !@tracker_addrs.empty?
        raise ArgumentError, "tracker_addrs must be a non-empty array"
      end
      
      # Set maximum connections per server
      # Use default if not provided
      @max_conns = options[:max_conns] || 10
      
      # Validate max_conns
      # Must be a positive integer
      unless @max_conns.is_a?(Integer) && @max_conns > 0
        raise ArgumentError, "max_conns must be a positive integer"
      end
      
      # Set connection timeout
      # Use default if not provided
      @connect_timeout = options[:connect_timeout] || 5.0
      
      # Validate connect_timeout
      # Must be a positive number
      unless @connect_timeout.is_a?(Numeric) && @connect_timeout > 0
        raise ArgumentError, "connect_timeout must be a positive number"
      end
      
      # Set network timeout
      # Use default if not provided
      @network_timeout = options[:network_timeout] || 30.0
      
      # Validate network_timeout
      # Must be a positive number
      unless @network_timeout.is_a?(Numeric) && @network_timeout > 0
        raise ArgumentError, "network_timeout must be a positive number"
      end
      
      # Set idle timeout
      # Use default if not provided
      @idle_timeout = options[:idle_timeout] || 60.0
      
      # Validate idle_timeout
      # Must be a positive number
      unless @idle_timeout.is_a?(Numeric) && @idle_timeout > 0
        raise ArgumentError, "idle_timeout must be a positive number"
      end
      
      # Set retry count
      # Use default if not provided
      @retry_count = options[:retry_count] || 3
      
      # Validate retry_count
      # Must be a non-negative integer
      unless @retry_count.is_a?(Integer) && @retry_count >= 0
        raise ArgumentError, "retry_count must be a non-negative integer"
      end
      
      # Configuration initialization complete
      # All parameters have been set and validated
    end
    
    # Returns a string representation of the configuration.
    #
    # This is useful for debugging and logging.
    #
    # @return [String] String representation of the configuration.
    def to_s
      # Build string representation
      # Include all configuration parameters
      "ClientConfig(" \
        "tracker_addrs=#{@tracker_addrs.inspect}, " \
        "max_conns=#{@max_conns}, " \
        "connect_timeout=#{@connect_timeout}, " \
        "network_timeout=#{@network_timeout}, " \
        "idle_timeout=#{@idle_timeout}, " \
        "retry_count=#{@retry_count}" \
      ")"
    end
    
    # Returns a hash representation of the configuration.
    #
    # This can be useful for serialization or inspection.
    #
    # @return [Hash] Hash representation of the configuration.
    def to_h
      # Build hash with all configuration parameters
      {
        tracker_addrs: @tracker_addrs,
        max_conns: @max_conns,
        connect_timeout: @connect_timeout,
        network_timeout: @network_timeout,
        idle_timeout: @idle_timeout,
        retry_count: @retry_count
      }
    end
    
    # Checks if this configuration equals another configuration.
    #
    # Two configurations are equal if all their parameters are equal.
    #
    # @param other [ClientConfig] The other configuration to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def ==(other)
      # Check if other is a ClientConfig
      return false unless other.is_a?(ClientConfig)
      
      # Compare all parameters
      @tracker_addrs == other.tracker_addrs &&
        @max_conns == other.max_conns &&
        @connect_timeout == other.connect_timeout &&
        @network_timeout == other.network_timeout &&
        @idle_timeout == other.idle_timeout &&
        @retry_count == other.retry_count
    end
    
    # Computes hash code for this configuration.
    #
    # This is needed for using ClientConfig as a hash key.
    #
    # @return [Integer] Hash code.
    def hash
      # Combine hash codes of all parameters
      [@tracker_addrs, @max_conns, @connect_timeout, @network_timeout, @idle_timeout, @retry_count].hash
    end
    
    # Checks if this configuration equals another (eql? version).
    #
    # This is needed for hash equality.
    #
    # @param other [ClientConfig] The other configuration to compare.
    #
    # @return [Boolean] True if equal, false otherwise.
    def eql?(other)
      self == other
    end
  end
end

