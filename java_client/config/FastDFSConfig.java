/**
 * FastDFSConfig - Client Configuration
 * 
 * This class holds all configuration settings for the FastDFS client, including
 * tracker server addresses, timeouts, connection pool sizes, and retry counts.
 * Configuration objects are immutable once created using the Builder pattern.
 * 
 * Copyright (C) 2025 FastDFS Java Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * 
 * @author FastDFS Java Client Contributors
 * @version 1.0.0
 * @since 1.0.0
 */
package com.fastdfs.client.config;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * FastDFSConfig holds the configuration for a FastDFS client instance.
 * 
 * This class is immutable and must be created using the Builder pattern.
 * All configuration values have sensible defaults, but tracker servers
 * must be explicitly configured.
 * 
 * The configuration includes:
 * - Tracker server addresses (required)
 * - Connection pool settings
 * - Timeout values
 * - Retry configuration
 * 
 * Example usage:
 * <pre>
 * FastDFSConfig config = new FastDFSConfig.Builder()
 *     .addTrackerServer("192.168.1.100", 22122)
 *     .addTrackerServer("192.168.1.101", 22122)
 *     .maxConnectionsPerServer(100)
 *     .connectTimeout(5000)
 *     .networkTimeout(30000)
 *     .idleTimeout(60000)
 *     .retryCount(3)
 *     .build();
 * </pre>
 */
public class FastDFSConfig {
    
    // ============================================================================
    // Default Values
    // ============================================================================
    
    /**
     * Default maximum number of connections per server.
     * 
     * This is the default value used when maxConnectionsPerServer is not
     * explicitly set in the configuration.
     */
    private static final int DEFAULT_MAX_CONNECTIONS_PER_SERVER = 10;
    
    /**
     * Default connection timeout in milliseconds.
     * 
     * This is the default value used when connectTimeout is not explicitly
     * set in the configuration.
     */
    private static final int DEFAULT_CONNECT_TIMEOUT = 5000; // 5 seconds
    
    /**
     * Default network I/O timeout in milliseconds.
     * 
     * This is the default value used when networkTimeout is not explicitly
     * set in the configuration.
     */
    private static final int DEFAULT_NETWORK_TIMEOUT = 30000; // 30 seconds
    
    /**
     * Default idle connection timeout in milliseconds.
     * 
     * This is the default value used when idleTimeout is not explicitly
     * set in the configuration.
     */
    private static final int DEFAULT_IDLE_TIMEOUT = 60000; // 60 seconds
    
    /**
     * Default retry count for failed operations.
     * 
     * This is the default value used when retryCount is not explicitly
     * set in the configuration.
     */
    private static final int DEFAULT_RETRY_COUNT = 3;
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * List of tracker server addresses.
     * 
     * Each address is in the format "host:port". The client will try to
     * connect to these servers in order, with automatic failover if a server
     * is unavailable.
     */
    private final List<String> trackerServers;
    
    /**
     * Maximum number of connections to maintain per server.
     * 
     * This controls the size of the connection pool for each server.
     * Larger values allow more concurrent operations but consume more
     * network resources.
     */
    private final int maxConnectionsPerServer;
    
    /**
     * Connection timeout in milliseconds.
     * 
     * This is the maximum time to wait when establishing a new connection
     * to a server. If the connection cannot be established within this
     * time, the operation will fail.
     */
    private final int connectTimeout;
    
    /**
     * Network I/O timeout in milliseconds.
     * 
     * This is the maximum time to wait for network read or write operations
     * to complete. If an operation exceeds this timeout, it will fail.
     */
    private final int networkTimeout;
    
    /**
     * Idle connection timeout in milliseconds.
     * 
     * Connections that are idle (not used) for longer than this timeout
     * will be closed and removed from the connection pool to free resources.
     */
    private final int idleTimeout;
    
    /**
     * Number of retries for failed operations.
     * 
     * When an operation fails due to a transient error (such as network
     * timeout), it will be retried up to this many times before giving up.
     */
    private final int retryCount;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Private constructor - use Builder to create instances.
     * 
     * @param builder the builder containing configuration values
     */
    private FastDFSConfig(Builder builder) {
        // Validate tracker servers
        if (builder.trackerServers == null || builder.trackerServers.isEmpty()) {
            throw new IllegalArgumentException("At least one tracker server must be configured");
        }
        
        // Store values with defensive copies where needed
        this.trackerServers = Collections.unmodifiableList(
            new ArrayList<>(builder.trackerServers)
        );
        this.maxConnectionsPerServer = builder.maxConnectionsPerServer > 0 
            ? builder.maxConnectionsPerServer 
            : DEFAULT_MAX_CONNECTIONS_PER_SERVER;
        this.connectTimeout = builder.connectTimeout > 0 
            ? builder.connectTimeout 
            : DEFAULT_CONNECT_TIMEOUT;
        this.networkTimeout = builder.networkTimeout > 0 
            ? builder.networkTimeout 
            : DEFAULT_NETWORK_TIMEOUT;
        this.idleTimeout = builder.idleTimeout > 0 
            ? builder.idleTimeout 
            : DEFAULT_IDLE_TIMEOUT;
        this.retryCount = builder.retryCount >= 0 
            ? builder.retryCount 
            : DEFAULT_RETRY_COUNT;
    }
    
    // ============================================================================
    // Getters
    // ============================================================================
    
    /**
     * Gets the list of tracker server addresses.
     * 
     * The returned list is unmodifiable and contains addresses in "host:port" format.
     * 
     * @return an unmodifiable list of tracker server addresses (never null)
     */
    public List<String> getTrackerServers() {
        return trackerServers;
    }
    
    /**
     * Gets the maximum number of connections per server.
     * 
     * @return the maximum connections per server (always > 0)
     */
    public int getMaxConnectionsPerServer() {
        return maxConnectionsPerServer;
    }
    
    /**
     * Gets the connection timeout in milliseconds.
     * 
     * @return the connection timeout in milliseconds (always > 0)
     */
    public int getConnectTimeout() {
        return connectTimeout;
    }
    
    /**
     * Gets the network I/O timeout in milliseconds.
     * 
     * @return the network timeout in milliseconds (always > 0)
     */
    public int getNetworkTimeout() {
        return networkTimeout;
    }
    
    /**
     * Gets the idle connection timeout in milliseconds.
     * 
     * @return the idle timeout in milliseconds (always > 0)
     */
    public int getIdleTimeout() {
        return idleTimeout;
    }
    
    /**
     * Gets the retry count for failed operations.
     * 
     * @return the retry count (always >= 0)
     */
    public int getRetryCount() {
        return retryCount;
    }
    
    // ============================================================================
    // Builder Class
    // ============================================================================
    
    /**
     * Builder for creating FastDFSConfig instances.
     * 
     * This builder provides a fluent API for constructing configuration objects.
     * All settings have sensible defaults except tracker servers, which must
     * be explicitly configured.
     * 
     * Example usage:
     * <pre>
     * FastDFSConfig config = new FastDFSConfig.Builder()
     *     .addTrackerServer("192.168.1.100", 22122)
     *     .maxConnectionsPerServer(100)
     *     .build();
     * </pre>
     */
    public static class Builder {
        
        /**
         * List of tracker server addresses being built.
         */
        private List<String> trackerServers = new ArrayList<>();
        
        /**
         * Maximum connections per server (0 means use default).
         */
        private int maxConnectionsPerServer = 0;
        
        /**
         * Connection timeout in milliseconds (0 means use default).
         */
        private int connectTimeout = 0;
        
        /**
         * Network I/O timeout in milliseconds (0 means use default).
         */
        private int networkTimeout = 0;
        
        /**
         * Idle connection timeout in milliseconds (0 means use default).
         */
        private int idleTimeout = 0;
        
        /**
         * Retry count for failed operations (-1 means use default).
         */
        private int retryCount = -1;
        
        /**
         * Adds a tracker server to the configuration.
         * 
         * Multiple tracker servers can be added for high availability.
         * The client will try to connect to them in order, with automatic
         * failover if a server is unavailable.
         * 
         * @param host the tracker server hostname or IP address (must not be null)
         * @param port the tracker server port (must be between 1 and 65535)
         * @return this builder for method chaining
         * @throws IllegalArgumentException if host is null or port is invalid
         */
        public Builder addTrackerServer(String host, int port) {
            if (host == null || host.trim().isEmpty()) {
                throw new IllegalArgumentException("Host cannot be null or empty");
            }
            if (port < 1 || port > 65535) {
                throw new IllegalArgumentException("Port must be between 1 and 65535");
            }
            
            String address = host.trim() + ":" + port;
            if (!trackerServers.contains(address)) {
                trackerServers.add(address);
            }
            return this;
        }
        
        /**
         * Sets the maximum number of connections per server.
         * 
         * This controls the size of the connection pool for each server.
         * Larger values allow more concurrent operations but consume more
         * network resources.
         * 
         * @param maxConnections the maximum connections per server (must be > 0)
         * @return this builder for method chaining
         * @throws IllegalArgumentException if maxConnections is <= 0
         */
        public Builder maxConnectionsPerServer(int maxConnections) {
            if (maxConnections <= 0) {
                throw new IllegalArgumentException("Max connections must be greater than 0");
            }
            this.maxConnectionsPerServer = maxConnections;
            return this;
        }
        
        /**
         * Sets the connection timeout in milliseconds.
         * 
         * This is the maximum time to wait when establishing a new connection
         * to a server.
         * 
         * @param timeout the connection timeout in milliseconds (must be > 0)
         * @return this builder for method chaining
         * @throws IllegalArgumentException if timeout is <= 0
         */
        public Builder connectTimeout(int timeout) {
            if (timeout <= 0) {
                throw new IllegalArgumentException("Connect timeout must be greater than 0");
            }
            this.connectTimeout = timeout;
            return this;
        }
        
        /**
         * Sets the network I/O timeout in milliseconds.
         * 
         * This is the maximum time to wait for network read or write operations
         * to complete.
         * 
         * @param timeout the network timeout in milliseconds (must be > 0)
         * @return this builder for method chaining
         * @throws IllegalArgumentException if timeout is <= 0
         */
        public Builder networkTimeout(int timeout) {
            if (timeout <= 0) {
                throw new IllegalArgumentException("Network timeout must be greater than 0");
            }
            this.networkTimeout = timeout;
            return this;
        }
        
        /**
         * Sets the idle connection timeout in milliseconds.
         * 
         * Connections that are idle for longer than this timeout will be
         * closed and removed from the connection pool.
         * 
         * @param timeout the idle timeout in milliseconds (must be > 0)
         * @return this builder for method chaining
         * @throws IllegalArgumentException if timeout is <= 0
         */
        public Builder idleTimeout(int timeout) {
            if (timeout <= 0) {
                throw new IllegalArgumentException("Idle timeout must be greater than 0");
            }
            this.idleTimeout = timeout;
            return this;
        }
        
        /**
         * Sets the retry count for failed operations.
         * 
         * When an operation fails due to a transient error, it will be
         * retried up to this many times before giving up.
         * 
         * @param count the retry count (must be >= 0)
         * @return this builder for method chaining
         * @throws IllegalArgumentException if count is < 0
         */
        public Builder retryCount(int count) {
            if (count < 0) {
                throw new IllegalArgumentException("Retry count cannot be negative");
            }
            this.retryCount = count;
            return this;
        }
        
        /**
         * Builds a FastDFSConfig instance from this builder.
         * 
         * At least one tracker server must have been added before calling
         * this method, otherwise an IllegalArgumentException will be thrown.
         * 
         * @return a new FastDFSConfig instance
         * @throws IllegalArgumentException if no tracker servers were configured
         */
        public FastDFSConfig build() {
            return new FastDFSConfig(this);
        }
    }
}

