/**
 * FastDFS Client Configuration
 * 
 * This class holds all configuration parameters for the FastDFS client.
 * It provides sensible defaults for all optional parameters and validates
 * required parameters.
 * 
 * Configuration includes:
 * - Tracker server addresses (required)
 * - Connection pool settings
 * - Timeout settings
 * - Retry settings
 * - Other advanced options
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.config

/**
 * Configuration class for FastDFS client.
 * 
 * This class encapsulates all configuration options for the client.
 * It provides builder-style methods and sensible defaults.
 * 
 * Example usage:
 * <pre>
 * def config = new ClientConfig(
 *     trackerAddrs: ['192.168.1.100:22122'],
 *     maxConns: 100,
 *     connectTimeout: 5000,
 *     networkTimeout: 30000
 * )
 * </pre>
 */
class ClientConfig {
    
    // ============================================================================
    // Required Configuration
    // ============================================================================
    
    /**
     * List of tracker server addresses.
     * 
     * Format: "host:port" (e.g., "192.168.1.100:22122")
     * 
     * At least one tracker address is required. Multiple addresses provide
     * redundancy and automatic failover. The client will try each tracker
     * in order until one responds.
     * 
     * This field is required and must not be null or empty.
     */
    List<String> trackerAddrs
    
    // ============================================================================
    // Connection Pool Configuration
    // ============================================================================
    
    /**
     * Maximum number of connections per server.
     * 
     * This limits the number of concurrent connections to each tracker
     * or storage server. Higher values allow more concurrent operations
     * but consume more resources.
     * 
     * Default: 10
     * Minimum: 1
     * Recommended: 10-100 depending on load
     */
    Integer maxConns = 10
    
    /**
     * Enable connection pooling.
     * 
     * When enabled, connections are reused across operations for better
     * performance. When disabled, a new connection is created for each
     * operation (not recommended for production).
     * 
     * Default: true
     */
    Boolean enablePool = true
    
    /**
     * Idle timeout for connections in the pool (milliseconds).
     * 
     * Connections that are idle for longer than this duration will be
     * closed and removed from the pool. This helps free up resources
     * during low activity periods.
     * 
     * Default: 60000 (60 seconds)
     * Minimum: 1000 (1 second)
     */
    Long idleTimeout = 60000L
    
    // ============================================================================
    // Timeout Configuration
    // ============================================================================
    
    /**
     * Connection timeout (milliseconds).
     * 
     * Maximum time to wait when establishing a new connection to a server.
     * If the connection cannot be established within this time, it fails.
     * 
     * Default: 5000 (5 seconds)
     * Minimum: 1000 (1 second)
     * Recommended: 5000-10000
     */
    Long connectTimeout = 5000L
    
    /**
     * Network I/O timeout (milliseconds).
     * 
     * Maximum time to wait for network read/write operations to complete.
     * This applies to all network I/O operations including sending requests
     * and receiving responses.
     * 
     * Default: 30000 (30 seconds)
     * Minimum: 1000 (1 second)
     * Recommended: 30000-60000 for large file operations
     */
    Long networkTimeout = 30000L
    
    // ============================================================================
    // Retry Configuration
    // ============================================================================
    
    /**
     * Number of retries for failed operations.
     * 
     * When an operation fails due to a transient error (network timeout,
     * connection error, etc.), the client will automatically retry up to
     * this many times before giving up.
     * 
     * Default: 3
     * Minimum: 0 (no retries)
     * Recommended: 3-5
     */
    Integer retryCount = 3
    
    /**
     * Retry delay base (milliseconds).
     * 
     * Base delay between retries. The actual delay uses exponential backoff:
     * delay = retryDelayBase * (2 ^ retryAttempt)
     * 
     * Default: 1000 (1 second)
     * Minimum: 100
     */
    Long retryDelayBase = 1000L
    
    /**
     * Maximum retry delay (milliseconds).
     * 
     * Caps the retry delay to prevent excessively long waits.
     * 
     * Default: 10000 (10 seconds)
     * Minimum: retryDelayBase
     */
    Long retryDelayMax = 10000L
    
    // ============================================================================
    // Advanced Configuration
    // ============================================================================
    
    /**
     * Enable automatic failover.
     * 
     * When enabled, the client will automatically try alternative servers
     * if the primary server fails. This provides high availability.
     * 
     * Default: true
     */
    Boolean enableFailover = true
    
    /**
     * Enable connection keep-alive.
     * 
     * When enabled, TCP keep-alive is used to detect dead connections
     * and automatically reconnect.
     * 
     * Default: true
     */
    Boolean enableKeepAlive = true
    
    /**
     * Keep-alive interval (milliseconds).
     * 
     * How often to send keep-alive probes.
     * 
     * Default: 30000 (30 seconds)
     */
    Long keepAliveInterval = 30000L
    
    /**
     * TCP no-delay (Nagle's algorithm).
     * 
     * When enabled, disables Nagle's algorithm for lower latency.
     * May increase network traffic for small packets.
     * 
     * Default: true
     */
    Boolean tcpNoDelay = true
    
    /**
     * Receive buffer size (bytes).
     * 
     * Size of the TCP receive buffer. Larger values may improve
     * performance for large file transfers.
     * 
     * Default: 65536 (64 KB)
     * Minimum: 1024
     */
    Integer receiveBufferSize = 65536
    
    /**
     * Send buffer size (bytes).
     * 
     * Size of the TCP send buffer. Larger values may improve
     * performance for large file transfers.
     * 
     * Default: 65536 (64 KB)
     * Minimum: 1024
     */
    Integer sendBufferSize = 65536
    
    /**
     * Enable logging.
     * 
     * When enabled, the client will log operations and errors.
     * 
     * Default: false
     */
    Boolean enableLogging = false
    
    /**
     * Log level.
     * 
     * Controls the verbosity of logging. Options: DEBUG, INFO, WARN, ERROR
     * 
     * Default: "INFO"
     */
    String logLevel = "INFO"
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Default constructor.
     * 
     * Creates a configuration with default values.
     * Tracker addresses must be set before using the configuration.
     */
    ClientConfig() {
        // Initialize with defaults
        // All fields have default values assigned above
    }
    
    /**
     * Copy constructor.
     * 
     * Creates a deep copy of another configuration object.
     * This is used internally to prevent external modification.
     * 
     * @param other the configuration to copy (must not be null)
     */
    ClientConfig(ClientConfig other) {
        if (other == null) {
            throw new IllegalArgumentException("Configuration to copy cannot be null")
        }
        
        // Copy all fields
        this.trackerAddrs = other.trackerAddrs ? new ArrayList<>(other.trackerAddrs) : null
        this.maxConns = other.maxConns
        this.enablePool = other.enablePool
        this.idleTimeout = other.idleTimeout
        this.connectTimeout = other.connectTimeout
        this.networkTimeout = other.networkTimeout
        this.retryCount = other.retryCount
        this.retryDelayBase = other.retryDelayBase
        this.retryDelayMax = other.retryDelayMax
        this.enableFailover = other.enableFailover
        this.enableKeepAlive = other.enableKeepAlive
        this.keepAliveInterval = other.keepAliveInterval
        this.tcpNoDelay = other.tcpNoDelay
        this.receiveBufferSize = other.receiveBufferSize
        this.sendBufferSize = other.sendBufferSize
        this.enableLogging = other.enableLogging
        this.logLevel = other.logLevel
    }
    
    // ============================================================================
    // Builder Methods (Fluent API)
    // ============================================================================
    
    /**
     * Sets tracker addresses.
     * 
     * @param addresses the tracker addresses (required)
     * @return this configuration for method chaining
     */
    ClientConfig trackerAddrs(List<String> addresses) {
        this.trackerAddrs = addresses
        return this
    }
    
    /**
     * Sets tracker addresses (varargs).
     * 
     * @param addresses the tracker addresses (required)
     * @return this configuration for method chaining
     */
    ClientConfig trackerAddrs(String... addresses) {
        this.trackerAddrs = addresses.toList()
        return this
    }
    
    /**
     * Sets maximum connections.
     * 
     * @param maxConns the maximum connections (must be > 0)
     * @return this configuration for method chaining
     */
    ClientConfig maxConns(Integer maxConns) {
        this.maxConns = maxConns
        return this
    }
    
    /**
     * Sets connection timeout.
     * 
     * @param timeout the timeout in milliseconds (must be > 0)
     * @return this configuration for method chaining
     */
    ClientConfig connectTimeout(Long timeout) {
        this.connectTimeout = timeout
        return this
    }
    
    /**
     * Sets network timeout.
     * 
     * @param timeout the timeout in milliseconds (must be > 0)
     * @return this configuration for method chaining
     */
    ClientConfig networkTimeout(Long timeout) {
        this.networkTimeout = timeout
        return this
    }
    
    /**
     * Sets retry count.
     * 
     * @param count the retry count (must be >= 0)
     * @return this configuration for method chaining
     */
    ClientConfig retryCount(Integer count) {
        this.retryCount = count
        return this
    }
    
    // ============================================================================
    // Validation
    // ============================================================================
    
    /**
     * Validates the configuration.
     * 
     * Checks that all required fields are set and all values are within
     * acceptable ranges.
     * 
     * @throws IllegalArgumentException if the configuration is invalid
     */
    void validate() {
        // Validate tracker addresses
        if (trackerAddrs == null || trackerAddrs.isEmpty()) {
            throw new IllegalArgumentException("At least one tracker address is required")
        }
        
        for (String addr : trackerAddrs) {
            if (addr == null || addr.trim().isEmpty()) {
                throw new IllegalArgumentException("Tracker address cannot be null or empty")
            }
        }
        
        // Validate max connections
        if (maxConns != null && maxConns < 1) {
            throw new IllegalArgumentException("Max connections must be at least 1")
        }
        
        // Validate timeouts
        if (connectTimeout != null && connectTimeout < 1000) {
            throw new IllegalArgumentException("Connect timeout must be at least 1000ms")
        }
        
        if (networkTimeout != null && networkTimeout < 1000) {
            throw new IllegalArgumentException("Network timeout must be at least 1000ms")
        }
        
        if (idleTimeout != null && idleTimeout < 1000) {
            throw new IllegalArgumentException("Idle timeout must be at least 1000ms")
        }
        
        // Validate retry settings
        if (retryCount != null && retryCount < 0) {
            throw new IllegalArgumentException("Retry count cannot be negative")
        }
        
        if (retryDelayBase != null && retryDelayBase < 100) {
            throw new IllegalArgumentException("Retry delay base must be at least 100ms")
        }
        
        if (retryDelayMax != null && retryDelayMax < retryDelayBase) {
            throw new IllegalArgumentException("Retry delay max must be >= retry delay base")
        }
        
        // Validate buffer sizes
        if (receiveBufferSize != null && receiveBufferSize < 1024) {
            throw new IllegalArgumentException("Receive buffer size must be at least 1024 bytes")
        }
        
        if (sendBufferSize != null && sendBufferSize < 1024) {
            throw new IllegalArgumentException("Send buffer size must be at least 1024 bytes")
        }
    }
    
    /**
     * Returns a string representation of the configuration.
     * 
     * Sensitive information (if any) is not included.
     * 
     * @return string representation
     */
    @Override
    String toString() {
        return "ClientConfig{" +
            "trackerAddrs=" + trackerAddrs +
            ", maxConns=" + maxConns +
            ", enablePool=" + enablePool +
            ", idleTimeout=" + idleTimeout +
            ", connectTimeout=" + connectTimeout +
            ", networkTimeout=" + networkTimeout +
            ", retryCount=" + retryCount +
            ", retryDelayBase=" + retryDelayBase +
            ", retryDelayMax=" + retryDelayMax +
            ", enableFailover=" + enableFailover +
            ", enableKeepAlive=" + enableKeepAlive +
            ", keepAliveInterval=" + keepAliveInterval +
            ", tcpNoDelay=" + tcpNoDelay +
            ", receiveBufferSize=" + receiveBufferSize +
            ", sendBufferSize=" + sendBufferSize +
            ", enableLogging=" + enableLogging +
            ", logLevel='" + logLevel + '\'' +
            '}'
    }
}

