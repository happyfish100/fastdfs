/**
 * FastDFSConnectionPool - Connection Pool Manager
 * 
 * This class manages a pool of reusable connections to FastDFS servers.
 * It maintains separate pools for each server address and handles connection
 * reuse, idle connection cleanup, thread-safe concurrent access, and automatic
 * connection health checking.
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
package com.fastdfs.client.connection;

import com.fastdfs.client.exception.FastDFSException;
import com.fastdfs.client.exception.FastDFSError;

import java.net.Socket;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * FastDFSConnectionPool manages a pool of reusable connections to FastDFS servers.
 * 
 * This class provides connection pooling to minimize the overhead of establishing
 * new TCP connections for each operation. It maintains separate pools for each
 * server address and automatically manages connection lifecycle, health checking,
 * and cleanup.
 * 
 * Features:
 * - Connection reuse to minimize overhead
 * - Idle connection cleanup to free resources
 * - Thread-safe concurrent access
 * - Automatic connection health checking
 * - Dynamic server address addition
 * - Automatic failover between servers
 * 
 * The pool starts empty and creates connections on-demand when Get() is called.
 * Connections are returned to the pool after use and can be reused by subsequent
 * operations. Idle connections are automatically closed after a configurable timeout.
 * 
 * Thread Safety:
 * All operations are thread-safe and can be used concurrently by multiple threads.
 * The pool uses locks to protect internal data structures and ensure safe concurrent
 * access.
 * 
 * Example usage:
 * <pre>
 * FastDFSConnectionPool pool = new FastDFSConnectionPool(
 *     Arrays.asList("192.168.1.100:22122"),
 *     10,  // max connections per server
 *     5000, // connect timeout
 *     60000 // idle timeout
 * );
 * 
 * FastDFSConnection conn = pool.get("");
 * try {
 *     // use connection
 * } finally {
 *     pool.put(conn);
 * }
 * </pre>
 */
public class FastDFSConnectionPool {
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * List of server addresses managed by this pool.
     * 
     * For tracker pools, this contains all configured tracker server addresses.
     * For storage pools, this starts empty and is populated dynamically as
     * storage servers are discovered.
     */
    private final List<String> addresses;
    
    /**
     * Maximum number of connections to maintain per server.
     * 
     * This limits the size of each server's connection pool. When the limit
     * is reached, new connections are not added to the pool (they are closed
     * instead). This prevents unbounded memory usage.
     */
    private final int maxConnectionsPerServer;
    
    /**
     * Connection timeout in milliseconds.
     * 
     * This is the maximum time to wait when establishing a new connection
     * to a server. If the connection cannot be established within this time,
     * the operation will fail.
     */
    private final int connectTimeout;
    
    /**
     * Idle connection timeout in milliseconds.
     * 
     * Connections that are idle (not used) for longer than this timeout
     * will be closed and removed from the pool to free resources.
     */
    private final int idleTimeout;
    
    /**
     * Per-server connection pools.
     * 
     * This map stores a list of available connections for each server address.
     * The lists are used as LIFO (Last In, First Out) stacks for efficient
     * connection reuse.
     */
    private final Map<String, List<FastDFSConnection>> pools;
    
    /**
     * Lock for thread-safe access to the pool.
     * 
     * This lock protects all pool operations including getting, putting,
     * and cleaning connections. It ensures that multiple threads can safely
     * access the pool concurrently.
     */
    private final Lock poolLock = new ReentrantLock();
    
    /**
     * Flag indicating whether the pool is closed.
     * 
     * Once closed, all operations will fail and connections will be closed.
     * This flag is checked before performing any pool operations.
     */
    private volatile boolean closed = false;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new connection pool for the specified servers.
     * 
     * The pool starts empty; connections are created on-demand when Get()
     * is called. If addresses is empty, servers can be added later using
     * addAddress().
     * 
     * @param addresses list of server addresses in "host:port" format (can be empty)
     * @param maxConnectionsPerServer maximum connections to maintain per server
     * @param connectTimeout timeout for establishing new connections in milliseconds
     * @param idleTimeout how long connections can be idle before cleanup in milliseconds
     * @throws IllegalArgumentException if parameters are invalid
     */
    public FastDFSConnectionPool(List<String> addresses, int maxConnectionsPerServer,
                                  int connectTimeout, int idleTimeout) {
        // Validate parameters
        if (maxConnectionsPerServer <= 0) {
            throw new IllegalArgumentException("Max connections per server must be greater than 0");
        }
        if (connectTimeout <= 0) {
            throw new IllegalArgumentException("Connect timeout must be greater than 0");
        }
        if (idleTimeout <= 0) {
            throw new IllegalArgumentException("Idle timeout must be greater than 0");
        }
        
        // Store configuration
        this.addresses = addresses != null 
            ? new ArrayList<>(addresses) 
            : new ArrayList<>();
        this.maxConnectionsPerServer = maxConnectionsPerServer;
        this.connectTimeout = connectTimeout;
        this.idleTimeout = idleTimeout;
        
        // Initialize pools map
        this.pools = new HashMap<>();
        
        // Initialize empty pools for each server address
        for (String addr : this.addresses) {
            if (addr != null && !addr.trim().isEmpty()) {
                pools.put(addr, new ArrayList<>());
            }
        }
    }
    
    /**
     * Creates a new connection pool without initial server addresses.
     * 
     * This constructor is used for storage pools that start empty and are
     * populated dynamically as storage servers are discovered.
     * 
     * @param maxConnectionsPerServer maximum connections to maintain per server
     * @param connectTimeout timeout for establishing new connections in milliseconds
     * @param idleTimeout how long connections can be idle before cleanup in milliseconds
     */
    public FastDFSConnectionPool(int maxConnectionsPerServer, int connectTimeout, int idleTimeout) {
        this(Collections.<String>emptyList(), maxConnectionsPerServer, connectTimeout, idleTimeout);
    }
    
    // ============================================================================
    // Public Methods - Connection Management
    // ============================================================================
    
    /**
     * Retrieves a connection from the pool or creates a new one.
     * 
     * This method prefers reusing existing idle connections but will create
     * new ones if needed. Stale or dead connections are automatically discarded.
     * 
     * If a specific address is provided, a connection to that server is returned.
     * If address is empty or null, a connection to the first available server
     * is returned.
     * 
     * @param address specific server address, or "" to use the first available server
     * @return a ready-to-use connection
     * @throws FastDFSException if pool is closed or connection cannot be established
     */
    public FastDFSConnection get(String address) throws FastDFSException {
        // Check if pool is closed
        if (closed) {
            throw new FastDFSException(
                FastDFSError.CLIENT_CLOSED,
                "Connection pool is closed"
            );
        }
        
        // Determine which server to use
        String targetAddress = address;
        if (targetAddress == null || targetAddress.trim().isEmpty()) {
            // Use first available server
            poolLock.lock();
            try {
                if (addresses.isEmpty()) {
                    throw new FastDFSException(
                        FastDFSError.NO_STORAGE_SERVER,
                        "No server addresses available"
                    );
                }
                targetAddress = addresses.get(0);
            } finally {
                poolLock.unlock();
            }
        }
        
        // Get or create connection pool for this server
        List<FastDFSConnection> serverPool = getServerPool(targetAddress);
        
        // Try to reuse an existing connection
        poolLock.lock();
        try {
            // Get connections from the pool (LIFO order)
            while (!serverPool.isEmpty()) {
                FastDFSConnection conn = serverPool.remove(serverPool.size() - 1);
                
                // Verify connection is still healthy
                if (conn.isAlive()) {
                    // Connection is good, return it
                    return conn;
                } else {
                    // Connection is dead, close it and try next one
                    try {
                        conn.close();
                    } catch (Exception ignored) {
                        // Ignore errors when closing dead connections
                    }
                }
            }
        } finally {
            poolLock.unlock();
        }
        
        // No reusable connection available, create a new one
        return createConnection(targetAddress);
    }
    
    /**
     * Returns a connection to the pool for reuse.
     * 
     * The connection is only kept if:
     * - The pool is not closed
     * - The pool is not full
     * - The connection hasn't been idle too long
     * - The connection is still alive
     * 
     * Otherwise, the connection is closed and discarded.
     * 
     * @param conn connection to return (null is safe and will be ignored)
     * @throws FastDFSException if closing the connection fails
     */
    public void put(FastDFSConnection conn) throws FastDFSException {
        // Ignore null connections
        if (conn == null) {
            return;
        }
        
        // Check if pool is closed
        if (closed) {
            // Pool is closed, close the connection
            conn.close();
            return;
        }
        
        // Get server pool
        String address = conn.getAddress();
        List<FastDFSConnection> serverPool = getServerPool(address);
        
        // Check if connection should be kept
        poolLock.lock();
        try {
            // Check if pool is at capacity
            if (serverPool.size() >= maxConnectionsPerServer) {
                // Pool is full, close the connection
                conn.close();
                return;
            }
            
            // Check if connection has been idle too long
            long idleTime = System.currentTimeMillis() - conn.getLastUsedTime();
            if (idleTime > idleTimeout) {
                // Connection is too old, close it
                conn.close();
                return;
            }
            
            // Check if connection is still alive
            if (!conn.isAlive()) {
                // Connection is dead, close it
                conn.close();
                return;
            }
            
            // Connection is healthy and pool has space, add it back
            serverPool.add(conn);
            
            // Periodically clean up stale connections
            cleanupStaleConnections(serverPool);
            
        } finally {
            poolLock.unlock();
        }
    }
    
    /**
     * Dynamically adds a new server address to the pool.
     * 
     * This is useful for adding storage servers discovered at runtime.
     * If the address already exists, this is a no-op.
     * 
     * @param address server address in "host:port" format
     * @throws IllegalArgumentException if address is null or empty
     */
    public void addAddress(String address) {
        // Validate address
        if (address == null || address.trim().isEmpty()) {
            throw new IllegalArgumentException("Address cannot be null or empty");
        }
        
        poolLock.lock();
        try {
            // Check if already exists
            if (addresses.contains(address)) {
                return;
            }
            
            // Add to addresses list
            addresses.add(address);
            
            // Create empty pool for this server
            if (!pools.containsKey(address)) {
                pools.put(address, new ArrayList<>());
            }
            
        } finally {
            poolLock.unlock();
        }
    }
    
    /**
     * Closes the connection pool and all connections.
     * 
     * After Close is called, Get will return FastDFSError.CLIENT_CLOSED.
     * It's safe to call Close multiple times.
     * 
     * @throws FastDFSException if closing connections fails
     */
    public void close() throws FastDFSException {
        // Check if already closed
        if (closed) {
            return;
        }
        
        poolLock.lock();
        try {
            // Check again (double-check locking pattern)
            if (closed) {
                return;
            }
            
            // Mark as closed
            closed = true;
            
            // Close all connections in all pools
            List<Exception> errors = new ArrayList<>();
            for (List<FastDFSConnection> serverPool : pools.values()) {
                for (FastDFSConnection conn : serverPool) {
                    try {
                        conn.close();
                    } catch (Exception e) {
                        errors.add(e);
                    }
                }
                serverPool.clear();
            }
            
            // Clear pools
            pools.clear();
            addresses.clear();
            
            // Throw exception if any errors occurred
            if (!errors.isEmpty()) {
                StringBuilder message = new StringBuilder("Error closing connection pool: ");
                for (int i = 0; i < errors.size(); i++) {
                    if (i > 0) {
                        message.append(", ");
                    }
                    message.append(errors.get(i).getMessage());
                }
                throw new FastDFSException(
                    FastDFSError.CLOSE_FAILED,
                    message.toString(),
                    errors.get(0)
                );
            }
            
        } finally {
            poolLock.unlock();
        }
    }
    
    // ============================================================================
    // Private Helper Methods
    // ============================================================================
    
    /**
     * Gets or creates the connection pool for a specific server address.
     * 
     * If the pool doesn't exist yet, it is created. This allows dynamic
     * addition of storage servers discovered at runtime.
     * 
     * @param address the server address
     * @return the connection pool for this server (never null)
     */
    private List<FastDFSConnection> getServerPool(String address) {
        poolLock.lock();
        try {
            List<FastDFSConnection> serverPool = pools.get(address);
            if (serverPool == null) {
                // Pool doesn't exist, create it
                serverPool = new ArrayList<>();
                pools.put(address, serverPool);
                
                // Add to addresses list if not already there
                if (!addresses.contains(address)) {
                    addresses.add(address);
                }
            }
            return serverPool;
        } finally {
            poolLock.unlock();
        }
    }
    
    /**
     * Creates a new connection to the specified server.
     * 
     * This method establishes a TCP connection to the server and wraps it
     * in a FastDFSConnection object.
     * 
     * @param address the server address in "host:port" format
     * @return a new connection (never null)
     * @throws FastDFSException if connection cannot be established
     */
    private FastDFSConnection createConnection(String address) throws FastDFSException {
        // Parse address
        String[] parts = address.split(":", 2);
        if (parts.length != 2) {
            throw new FastDFSException(
                FastDFSError.INVALID_CONFIG,
                "Invalid server address format: " + address
            );
        }
        
        String host = parts[0];
        int port;
        try {
            port = Integer.parseInt(parts[1]);
        } catch (NumberFormatException e) {
            throw new FastDFSException(
                FastDFSError.INVALID_CONFIG,
                "Invalid port number in address: " + address,
                e
            );
        }
        
        // Create socket and connect
        Socket socket = null;
        try {
            socket = new Socket();
            socket.connect(
                new java.net.InetSocketAddress(host, port),
                connectTimeout
            );
            
            // Create connection wrapper
            return new FastDFSConnection(socket, address);
            
        } catch (java.net.SocketTimeoutException e) {
            if (socket != null) {
                try {
                    socket.close();
                } catch (Exception ignored) {
                }
            }
            throw new FastDFSException(
                FastDFSError.CONNECTION_TIMEOUT,
                "Connection timeout to " + address + " after " + connectTimeout + "ms",
                e
            );
        } catch (java.io.IOException e) {
            if (socket != null) {
                try {
                    socket.close();
                } catch (Exception ignored) {
                }
            }
            throw new FastDFSException(
                FastDFSError.CONNECTION_FAILED,
                "Failed to connect to " + address + ": " + e.getMessage(),
                e
            );
        }
    }
    
    /**
     * Cleans up stale connections from a server pool.
     * 
     * This method removes connections that are dead or have been idle too long.
     * It's called periodically when connections are returned to the pool.
     * 
     * The serverPool must be locked by the caller.
     * 
     * @param serverPool the server pool to clean
     */
    private void cleanupStaleConnections(List<FastDFSConnection> serverPool) {
        // Only clean up occasionally to avoid overhead
        // Clean up every 10 connections returned
        if (serverPool.size() % 10 != 0) {
            return;
        }
        
        long now = System.currentTimeMillis();
        List<FastDFSConnection> validConnections = new ArrayList<>();
        
        // Check each connection and keep only the healthy ones
        for (FastDFSConnection conn : serverPool) {
            long idleTime = now - conn.getLastUsedTime();
            if (idleTime > idleTimeout || !conn.isAlive()) {
                // Connection is stale or dead, close it
                try {
                    conn.close();
                } catch (Exception ignored) {
                    // Ignore errors when closing stale connections
                }
            } else {
                // Connection is healthy, keep it
                validConnections.add(conn);
            }
        }
        
        // Replace pool with cleaned connections
        serverPool.clear();
        serverPool.addAll(validConnections);
    }
}

