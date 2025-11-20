# FastDFS Connection Pool Management
#
# This module handles TCP connections to FastDFS servers with connection pooling,
# automatic reconnection, health checking, and idle timeout management.
#
# Connection pools manage a set of reusable TCP connections to FastDFS servers,
# reducing the overhead of establishing new connections for each operation.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

require 'socket'
require 'timeout'
require 'thread'
require_relative 'errors'

module FastDFS
  # Connection class representing a TCP connection to a FastDFS server.
  #
  # This class wraps a socket with additional metadata and thread-safe
  # operations. Each connection tracks its last usage time for idle
  # timeout management.
  #
  # Connections are used internally by the ConnectionPool and should not
  # be created directly by client code.
  class Connection
    # Initializes a new Connection with an established socket.
    #
    # This constructor creates a Connection object from an already-established
    # TCP socket. The connection is ready for use immediately.
    #
    # @param sock [TCPSocket] Connected TCP socket.
    # @param addr [String] Server address in "host:port" format.
    def initialize(sock, addr)
      # Store the socket
      # This is the underlying TCP connection
      @sock = sock
      
      # Store the server address
      # This is used for error messages and logging
      @addr = addr
      
      # Track last usage time
      # Used by connection pool for idle timeout management
      @last_used = Time.now
      
      # Mutex for thread-safe operations
      # Ensures that concurrent operations don't interfere
      @mutex = Mutex.new
      
      # Track if connection is closed
      # Prevents operations on closed connections
      @closed = false
    end
    
    # Transmits data to the server with optional timeout.
    #
    # This method sends all the data to the server, blocking until all
    # bytes are sent or an error occurs. It is thread-safe and updates
    # the last_used timestamp.
    #
    # @param data [String] Bytes to send (must be complete message).
    # @param timeout [Float] Write timeout in seconds (0 means no timeout).
    #
    # @raise [NetworkError] If write fails or incomplete.
    #
    # @return [void]
    def send(data, timeout = 30.0)
      # Acquire mutex for thread safety
      # Only one thread can write at a time
      @mutex.synchronize do
        # Check if connection is closed
        # Cannot send data on closed connections
        raise NetworkError.new("write", @addr, StandardError.new("connection is closed")) if @closed
        
        # Set socket timeout if specified
        # This prevents indefinite blocking
        if timeout > 0
          @sock.write_timeout = timeout
        end
        
        # Send all data
        # We need to ensure all bytes are sent
        total_sent = 0
        while total_sent < data.bytesize
          # Send remaining bytes
          # Socket write may not send everything at once
          sent = @sock.write(data[total_sent..-1])
          
          # Check if connection was closed
          # Zero bytes sent usually means connection is broken
          if sent == 0
            raise NetworkError.new("write", @addr, StandardError.new("socket connection broken"))
          end
          
          # Update total sent
          # Continue until all bytes are sent
          total_sent += sent
        end
        
        # Update last used time
        # This is used by connection pool for idle timeout
        @last_used = Time.now
        
        # Send operation complete
        # All data has been successfully transmitted
      rescue => e
        # Wrap error in NetworkError
        # Provides context about operation and address
        if e.is_a?(NetworkError)
          raise e
        else
          raise NetworkError.new("write", @addr, e)
        end
      end
    end
    
    # Reads up to 'size' bytes from the server.
    #
    # This method may return fewer bytes than requested if the connection
    # is closed or an error occurs. Use receive_full if you need exactly
    # 'size' bytes.
    #
    # @param size [Integer] Maximum number of bytes to read.
    # @param timeout [Float] Read timeout in seconds (0 means no timeout).
    #
    # @return [String] Received data (may be less than 'size').
    #
    # @raise [NetworkError] If read fails.
    def receive(size, timeout = 30.0)
      # Acquire mutex for thread safety
      # Only one thread can read at a time
      @mutex.synchronize do
        # Check if connection is closed
        # Cannot read from closed connections
        raise NetworkError.new("read", @addr, StandardError.new("connection is closed")) if @closed
        
        # Set socket timeout if specified
        # This prevents indefinite blocking
        if timeout > 0
          @sock.read_timeout = timeout
        end
        
        # Read data from socket
        # May return fewer bytes than requested
        data = @sock.read(size)
        
        # Check if connection was closed
        # Nil data means connection was closed by peer
        if data.nil?
          raise NetworkError.new("read", @addr, StandardError.new("connection closed by peer"))
        end
        
        # Update last used time
        # This is used by connection pool for idle timeout
        @last_used = Time.now
        
        # Return received data
        # May be less than requested size
        data
      rescue => e
        # Wrap error in NetworkError
        # Provides context about operation and address
        if e.is_a?(NetworkError)
          raise e
        else
          raise NetworkError.new("read", @addr, e)
        end
      end
    end
    
    # Reads exactly 'size' bytes from the server.
    #
    # This method blocks until all bytes are received or an error occurs.
    # The timeout applies to the entire operation, not individual reads.
    #
    # @param size [Integer] Exact number of bytes to read.
    # @param timeout [Float] Total timeout for the operation (0 means no timeout).
    #
    # @return [String] Exactly 'size' bytes.
    #
    # @raise [NetworkError] If read fails before receiving all bytes.
    def receive_full(size, timeout = 30.0)
      # Acquire mutex for thread safety
      # Only one thread can read at a time
      @mutex.synchronize do
        # Check if connection is closed
        # Cannot read from closed connections
        raise NetworkError.new("read", @addr, StandardError.new("connection is closed")) if @closed
        
        # Set socket timeout if specified
        # This prevents indefinite blocking
        if timeout > 0
          @sock.read_timeout = timeout
        end
        
        # Read all data
        # We need to ensure we get exactly 'size' bytes
        data = ''
        remaining = size
        
        # Loop until we have all bytes
        # Socket read may not return everything at once
        while remaining > 0
          # Read remaining bytes
          # Try to read exactly what we need
          chunk = @sock.read(remaining)
          
          # Check if connection was closed
          # Nil chunk means connection was closed by peer
          if chunk.nil?
            raise NetworkError.new("read", @addr, StandardError.new("connection closed by peer"))
          end
          
          # Append chunk to data
          # Keep accumulating until we have all bytes
          data += chunk
          
          # Update remaining bytes
          # Continue until remaining is zero
          remaining -= chunk.bytesize
        end
        
        # Update last used time
        # This is used by connection pool for idle timeout
        @last_used = Time.now
        
        # Return complete data
        # Should be exactly 'size' bytes
        data
      rescue => e
        # Wrap error in NetworkError
        # Provides context about operation and address
        if e.is_a?(NetworkError)
          raise e
        else
          raise NetworkError.new("read", @addr, e)
        end
      end
    end
    
    # Terminates the connection and releases resources.
    #
    # It's safe to call close multiple times. After closing, all
    # operations on the connection will raise an error.
    #
    # @return [void]
    def close
      # Acquire mutex for thread safety
      # Only one thread can close at a time
      @mutex.synchronize do
        # Check if already closed
        # Multiple calls to close should be safe
        return if @closed
        
        # Mark as closed
        # This prevents further operations
        @closed = true
        
        # Close the socket
        # This releases the TCP connection
        begin
          @sock.close if @sock
        rescue => e
          # Ignore errors during close
          # Connection is already marked as closed
          # In production, you might want to log this
        end
        
        # Clear socket reference
        # Help garbage collector
        @sock = nil
      end
    end
    
    # Performs a non-blocking check to determine if the connection is still valid.
    #
    # This is a heuristic check that may not detect all failure modes.
    # It checks if the socket is still open and not closed.
    #
    # @return [Boolean] True if connection appears to be alive, false otherwise.
    def alive?
      # Acquire mutex for thread safety
      # Check connection state safely
      @mutex.synchronize do
        # Check if already closed
        # Closed connections are not alive
        return false if @closed
        
        # Check if socket exists
        # No socket means connection is not alive
        return false unless @sock
        
        # Check if socket is closed
        # Closed sockets are not alive
        @sock.closed? == false
      end
    end
    
    # Returns the server address for this connection.
    #
    # @return [String] Server address in "host:port" format.
    attr_reader :addr
    
    # Returns the last usage time of this connection.
    #
    # @return [Time] Timestamp of last usage.
    attr_reader :last_used
  end
  
  # Connection pool for managing reusable TCP connections.
  #
  # This class manages a pool of connections to FastDFS servers, allowing
  # connections to be reused across multiple operations. It handles connection
  # creation, health checking, idle timeout, and automatic reconnection.
  #
  # The pool is thread-safe and can be used concurrently from multiple threads.
  #
  # @example Create and use a connection pool
  #   pool = ConnectionPool.new(
  #     addrs: ['192.168.1.100:22122'],
  #     max_conns: 10,
  #     connect_timeout: 5.0,
  #     idle_timeout: 60.0
  #   )
  #
  #   conn = pool.get
  #   begin
  #     conn.send(data)
  #     response = conn.receive_full(1024)
  #   ensure
  #     pool.put(conn)
  #   end
  #
  #   pool.close
  class ConnectionPool
    # Initializes a new ConnectionPool with the given parameters.
    #
    # This constructor creates a connection pool that will manage connections
    # to the specified server addresses. Connections are created lazily as
    # needed and are reused across operations.
    #
    # @param addrs [Array<String>] List of server addresses in "host:port" format.
    # @param max_conns [Integer] Maximum number of connections per server (default: 10).
    # @param connect_timeout [Float] Connection timeout in seconds (default: 5.0).
    # @param idle_timeout [Float] Idle connection timeout in seconds (default: 60.0).
    def initialize(addrs:, max_conns: 10, connect_timeout: 5.0, idle_timeout: 60.0)
      # Store server addresses
      # These are the servers we can connect to
      @addrs = addrs.dup
      
      # Store maximum connections per server
      # This limits the pool size
      @max_conns = max_conns
      
      # Store connection timeout
      # Used when establishing new connections
      @connect_timeout = connect_timeout
      
      # Store idle timeout
      # Connections idle longer than this will be closed
      @idle_timeout = idle_timeout
      
      # Create connection pools for each address
      # Each address has its own pool
      @pools = {}
      @addrs.each do |addr|
        @pools[addr] = []
      end
      
      # Mutex for thread safety
      # Protects access to the pools
      @mutex = Mutex.new
      
      # Track if pool is closed
      # Prevents operations on closed pools
      @closed = false
      
      # Current connection count per address
      # Used to track pool size
      @counts = {}
      @addrs.each do |addr|
        @counts[addr] = 0
      end
    end
    
    # Gets a connection from the pool.
    #
    # This method returns an available connection from the pool, creating
    # a new one if necessary. The connection should be returned to the pool
    # using put when done.
    #
    # If no server address is specified, a random server is chosen. If a
    # server address is specified, a connection to that server is returned.
    #
    # @param addr [String, nil] Optional server address. If nil, a random server is chosen.
    #
    # @return [Connection] A connection from the pool.
    #
    # @raise [NoStorageServerError] If no server addresses are available.
    # @raise [ConnectionTimeoutError] If connection cannot be established.
    # @raise [ClientClosedError] If the pool has been closed.
    def get(addr = nil)
      # Check if pool is closed
      # Cannot get connections from closed pools
      raise ClientClosedError.new("connection pool is closed") if @closed
      
      # Acquire mutex for thread safety
      # Only one thread can access pools at a time
      @mutex.synchronize do
        # Choose server address
        # Use specified address or pick a random one
        target_addr = addr || @addrs.sample
        
        # Validate address
        # Must be one of our configured addresses
        unless @addrs.include?(target_addr)
          raise NoStorageServerError.new("server address not found: #{target_addr}")
        end
        
        # Check if we have available connections
        # Reuse existing connections if possible
        if @pools[target_addr].any?
          # Return an available connection
          # Remove from pool and return
          conn = @pools[target_addr].pop
          
          # Check if connection is still alive
          # Dead connections should be discarded
          if conn.alive?
            # Connection is good, return it
            # Update last used time
            return conn
          else
            # Connection is dead, discard it
            # Decrement count and create new one
            @counts[target_addr] -= 1
            
            # Close dead connection
            # Clean up resources
            begin
              conn.close
            rescue => e
              # Ignore errors during close
              # Connection is already dead
            end
          end
        end
        
        # Check if we can create a new connection
        # Must not exceed max_conns limit
        if @counts[target_addr] >= @max_conns
          # Pool is full, wait a bit and try again
          # This is a simple approach; in production you might want to
          # implement proper queueing or blocking
          sleep(0.1)
          
          # Try again with exponential backoff
          # This helps handle temporary connection limits
          retries = 3
          retries.times do |i|
            sleep(0.1 * (2 ** i))
            
            # Check again if connection is available
            if @pools[target_addr].any?
              conn = @pools[target_addr].pop
              return conn if conn.alive?
            end
            
            # Check if count decreased
            # Another thread might have returned a connection
            if @counts[target_addr] < @max_conns
              break
            end
          end
          
          # Still full, raise error
          # Cannot create new connection
          raise ConnectionTimeoutError.new("connection pool is full for #{target_addr}")
        end
        
        # Create new connection
        # This establishes a TCP connection to the server
        begin
          # Parse address
          # Extract host and port
          host, port = target_addr.split(':', 2)
          port = port.to_i
          
          # Create socket with timeout
          # This prevents indefinite blocking
          sock = Timeout.timeout(@connect_timeout) do
            TCPSocket.new(host, port)
          end
          
          # Create connection object
          # Wrap socket in Connection class
          conn = Connection.new(sock, target_addr)
          
          # Increment connection count
          # Track pool size
          @counts[target_addr] += 1
          
          # Return new connection
          # Ready for use
          conn
        rescue Timeout::Error => e
          # Connection timeout
          # Could not establish connection in time
          raise ConnectionTimeoutError.new("connection timeout to #{target_addr}: #{e.message}")
        rescue => e
          # Other connection errors
          # Wrap in NetworkError for consistency
          raise NetworkError.new("dial", target_addr, e)
        end
      end
    end
    
    # Returns a connection to the pool.
    #
    # This method returns a connection to the pool for reuse. The connection
    # is checked for health and may be closed if it's dead or idle too long.
    #
    # @param conn [Connection] The connection to return.
    #
    # @return [void]
    def put(conn)
      # Validate connection
      # Must be a Connection object
      return unless conn.is_a?(Connection)
      
      # Acquire mutex for thread safety
      # Only one thread can access pools at a time
      @mutex.synchronize do
        # Get connection address
        # Must match one of our configured addresses
        addr = conn.addr
        
        # Check if address is valid
        # Ignore connections to unknown addresses
        return unless @addrs.include?(addr)
        
        # Check if pool is closed
        # Close connection instead of returning to pool
        if @closed
          begin
            conn.close
          rescue => e
            # Ignore errors during close
          end
          return
        end
        
        # Check if connection is alive
        # Dead connections should not be returned to pool
        unless conn.alive?
          # Connection is dead, close it
          # Decrement count and discard
          @counts[addr] -= 1 if @counts[addr] > 0
          begin
            conn.close
          rescue => e
            # Ignore errors during close
          end
          return
        end
        
        # Check idle timeout
        # Connections idle too long should be closed
        if (Time.now - conn.last_used) > @idle_timeout
          # Connection is idle too long, close it
          # Decrement count and discard
          @counts[addr] -= 1 if @counts[addr] > 0
          begin
            conn.close
          rescue => e
            # Ignore errors during close
          end
          return
        end
        
        # Check if pool is full
        # Don't add connections if pool is at capacity
        if @pools[addr].size >= @max_conns
          # Pool is full, close connection
          # Don't keep more connections than max_conns
          @counts[addr] -= 1 if @counts[addr] > 0
          begin
            conn.close
          rescue => e
            # Ignore errors during close
          end
          return
        end
        
        # Return connection to pool
        # It can be reused by other operations
        @pools[addr] << conn
      end
    end
    
    # Closes all connections in the pool.
    #
    # This method closes all connections and releases resources. After
    # calling close, the pool cannot be used anymore. It's safe to call
    # close multiple times.
    #
    # @return [void]
    def close
      # Acquire mutex for thread safety
      # Only one thread can close at a time
      @mutex.synchronize do
        # Check if already closed
        # Multiple calls to close should be safe
        return if @closed
        
        # Mark as closed
        # This prevents further operations
        @closed = true
        
        # Close all connections
        # Iterate through all pools and close connections
        @pools.each do |addr, pool|
          # Close all connections for this address
          # Remove them from pool and close
          while pool.any?
            conn = pool.pop
            begin
              conn.close
            rescue => e
              # Ignore errors during close
              # We want to close all connections even if one fails
            end
          end
          
          # Reset count
          # All connections are closed
          @counts[addr] = 0
        end
        
        # Clear pools
        # Help garbage collector
        @pools.clear
      end
    end
    
    # Checks if the pool is closed.
    #
    # @return [Boolean] True if closed, false otherwise.
    def closed?
      # Thread-safe check
      # Use mutex to ensure consistency
      @mutex.synchronize { @closed }
    end
  end
end

