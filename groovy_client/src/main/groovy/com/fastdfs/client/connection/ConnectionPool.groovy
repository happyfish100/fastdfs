/**
 * FastDFS Connection Pool
 * 
 * This class manages a pool of connections to FastDFS servers (tracker or storage).
 * It provides connection reuse, automatic cleanup of idle connections, and
 * thread-safe operations.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.connection

import com.fastdfs.client.errors.*
import java.util.concurrent.*
import java.util.concurrent.locks.*
import java.util.concurrent.atomic.*

/**
 * Connection pool for FastDFS servers.
 * 
 * This pool manages connections to FastDFS servers, providing:
 * - Connection reuse for better performance
 * - Automatic cleanup of idle connections
 * - Thread-safe operations
 * - Connection health checking
 * - Automatic reconnection on failure
 * 
 * Example usage:
 * <pre>
 * def pool = new ConnectionPool(
 *     ['192.168.1.100:22122'],
 *     10,      // max connections
 *     5000,    // connect timeout
 *     60000    // idle timeout
 * )
 * 
 * try {
 *     def conn = pool.get()
 *     try {
 *         // Use connection
 *     } finally {
 *         pool.put(conn)
 *     }
 * } finally {
 *     pool.close()
 * }
 * </pre>
 */
class ConnectionPool {
    
    // ============================================================================
    // Configuration
    // ============================================================================
    
    /**
     * List of server addresses.
     * 
     * Format: "host:port" (e.g., "192.168.1.100:22122")
     */
    private final List<String> addresses
    
    /**
     * Maximum number of connections per server.
     */
    private final int maxConns
    
    /**
     * Connection timeout in milliseconds.
     */
    private final long connectTimeout
    
    /**
     * Idle timeout in milliseconds.
     * 
     * Connections idle for longer than this will be closed.
     */
    private final long idleTimeout
    
    // ============================================================================
    // Internal State
    // ============================================================================
    
    /**
     * Map of server address to connection queue.
     * 
     * Each server has its own queue of available connections.
     */
    private final Map<String, BlockingQueue<Connection>> connectionQueues
    
    /**
     * Map of server address to active connection count.
     * 
     * Tracks how many connections are currently in use per server.
     */
    private final Map<String, AtomicInteger> activeCounts
    
    /**
     * Map of server address to total connection count.
     * 
     * Tracks total connections (idle + active) per server.
     */
    private final Map<String, AtomicInteger> totalCounts
    
    /**
     * Lock for thread-safe operations.
     */
    private final ReadWriteLock lock
    
    /**
     * Read lock for concurrent reads.
     */
    private final Lock readLock
    
    /**
     * Write lock for exclusive writes.
     */
    private final Lock writeLock
    
    /**
     * Flag indicating if the pool is closed.
     */
    private volatile boolean closed
    
    /**
     * Scheduled executor for idle connection cleanup.
     */
    private ScheduledExecutorService cleanupExecutor
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new connection pool.
     * 
     * @param addresses list of server addresses (required)
     * @param maxConns maximum connections per server (must be > 0)
     * @param connectTimeout connection timeout in milliseconds (must be > 0)
     * @param idleTimeout idle timeout in milliseconds (must be > 0)
     */
    ConnectionPool(List<String> addresses, int maxConns, long connectTimeout, long idleTimeout) {
        // Validate parameters
        if (addresses == null || addresses.isEmpty()) {
            throw new IllegalArgumentException("Addresses cannot be null or empty")
        }
        
        if (maxConns < 1) {
            throw new IllegalArgumentException("Max connections must be at least 1")
        }
        
        if (connectTimeout < 1) {
            throw new IllegalArgumentException("Connect timeout must be at least 1ms")
        }
        
        if (idleTimeout < 1) {
            throw new IllegalArgumentException("Idle timeout must be at least 1ms")
        }
        
        // Store configuration
        this.addresses = new ArrayList<>(addresses)
        this.maxConns = maxConns
        this.connectTimeout = connectTimeout
        this.idleTimeout = idleTimeout
        
        // Initialize data structures
        this.connectionQueues = new ConcurrentHashMap<>()
        this.activeCounts = new ConcurrentHashMap<>()
        this.totalCounts = new ConcurrentHashMap<>()
        
        // Initialize locks
        this.lock = new ReentrantReadWriteLock()
        this.readLock = lock.readLock()
        this.writeLock = lock.writeLock()
        
        // Initialize closed flag
        this.closed = false
        
        // Initialize connection queues for each address
        for (String address : addresses) {
            connectionQueues.put(address, new LinkedBlockingQueue<>())
            activeCounts.put(address, new AtomicInteger(0))
            totalCounts.put(address, new AtomicInteger(0))
        }
        
        // Start cleanup executor
        this.cleanupExecutor = Executors.newSingleThreadScheduledExecutor({ r ->
            Thread t = new Thread(r, "ConnectionPool-Cleanup")
            t.daemon = true
            return t
        })
        
        // Schedule periodic cleanup
        cleanupExecutor.scheduleWithFixedDelay(
            { cleanupIdleConnections() },
            idleTimeout / 2,
            idleTimeout / 2,
            TimeUnit.MILLISECONDS
        )
    }
    
    // ============================================================================
    // Public API
    // ============================================================================
    
    /**
     * Gets a connection from the pool.
     * 
     * If an idle connection is available, it is returned immediately.
     * Otherwise, a new connection is created (if under max limit).
     * If max connections reached, waits for an available connection.
     * 
     * @return a connection (never null)
     * @throws FastDFSException if the pool is closed or connection fails
     */
    Connection get() {
        return get(null)
    }
    
    /**
     * Gets a connection from the pool for a specific address.
     * 
     * @param address the server address (null for any address)
     * @return a connection (never null)
     * @throws FastDFSException if the pool is closed or connection fails
     */
    Connection get(String address) {
        // Check if pool is closed
        if (closed) {
            throw new ClientClosedException("Connection pool is closed")
        }
        
        // Select address
        String targetAddress = address ?: selectAddress()
        
        // Try to get connection from queue
        Connection conn = connectionQueues.get(targetAddress).poll()
        
        if (conn != null) {
            // Check if connection is still valid
            if (conn.isValid()) {
                activeCounts.get(targetAddress).incrementAndGet()
                return conn
            } else {
                // Connection is invalid, decrement total count
                totalCounts.get(targetAddress).decrementAndGet()
            }
        }
        
        // Need to create new connection
        // Check if we're under the limit
        AtomicInteger total = totalCounts.get(targetAddress)
        AtomicInteger active = activeCounts.get(targetAddress)
        
        if (total.get() < maxConns) {
            // Create new connection
            try {
                conn = new Connection(targetAddress, connectTimeout)
                total.incrementAndGet()
                active.incrementAndGet()
                return conn
            } catch (Exception e) {
                throw new FastDFSException("Failed to create connection to ${targetAddress}: ${e.message}", e)
            }
        }
        
        // At max connections, wait for one to become available
        try {
            conn = connectionQueues.get(targetAddress).take()
            if (conn.isValid()) {
                active.incrementAndGet()
                return conn
            } else {
                // Connection is invalid, try again
                total.decrementAndGet()
                return get(targetAddress)
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt()
            throw new FastDFSException("Interrupted while waiting for connection", e)
        }
    }
    
    /**
     * Returns a connection to the pool.
     * 
     * The connection is made available for reuse. If the connection
     * is invalid or the pool is closed, the connection is closed.
     * 
     * @param conn the connection to return (can be null)
     */
    void put(Connection conn) {
        if (conn == null) {
            return
        }
        
        // Check if pool is closed
        if (closed) {
            conn.close()
            return
        }
        
        // Check if connection is valid
        if (!conn.isValid()) {
            // Connection is invalid, close it and decrement counts
            conn.close()
            String address = conn.getAddress()
            if (address != null) {
                activeCounts.get(address)?.decrementAndGet()
                totalCounts.get(address)?.decrementAndGet()
            }
            return
        }
        
        // Return connection to queue
        String address = conn.getAddress()
        activeCounts.get(address)?.decrementAndGet()
        conn.updateLastUsedTime()
        connectionQueues.get(address)?.offer(conn)
    }
    
    /**
     * Closes the pool and all connections.
     * 
     * After closing, the pool cannot be used for further operations.
     * This method is idempotent.
     */
    void close() {
        writeLock.lock()
        try {
            if (closed) {
                return
            }
            
            closed = true
            
            // Shutdown cleanup executor
            if (cleanupExecutor != null) {
                cleanupExecutor.shutdown()
                try {
                    if (!cleanupExecutor.awaitTermination(5, TimeUnit.SECONDS)) {
                        cleanupExecutor.shutdownNow()
                    }
                } catch (InterruptedException e) {
                    cleanupExecutor.shutdownNow()
                    Thread.currentThread().interrupt()
                }
            }
            
            // Close all connections
            for (BlockingQueue<Connection> queue : connectionQueues.values()) {
                Connection conn
                while ((conn = queue.poll()) != null) {
                    try {
                        conn.close()
                    } catch (Exception e) {
                        // Ignore errors during cleanup
                    }
                }
            }
            
            // Clear data structures
            connectionQueues.clear()
            activeCounts.clear()
            totalCounts.clear()
            
        } finally {
            writeLock.unlock()
        }
    }
    
    // ============================================================================
    // Private Helper Methods
    // ============================================================================
    
    /**
     * Selects an address from the available addresses.
     * 
     * Uses round-robin selection for load balancing.
     * 
     * @return the selected address
     */
    private String selectAddress() {
        // Simple round-robin selection
        // In a real implementation, this could use more sophisticated
        // load balancing algorithms
        int index = (int) (System.currentTimeMillis() % addresses.size())
        return addresses.get(index)
    }
    
    /**
     * Cleans up idle connections.
     * 
     * Removes connections that have been idle for longer than idleTimeout.
     */
    private void cleanupIdleConnections() {
        if (closed) {
            return
        }
        
        long now = System.currentTimeMillis()
        
        for (Map.Entry<String, BlockingQueue<Connection>> entry : connectionQueues.entrySet()) {
            String address = entry.key
            BlockingQueue<Connection> queue = entry.value
            
            List<Connection> toRemove = []
            
            // Check all connections in queue
            for (Connection conn : queue) {
                if (now - conn.getLastUsedTime() > idleTimeout) {
                    toRemove.add(conn)
                }
            }
            
            // Remove idle connections
            for (Connection conn : toRemove) {
                if (queue.remove(conn)) {
                    try {
                        conn.close()
                    } catch (Exception e) {
                        // Ignore errors during cleanup
                    }
                    totalCounts.get(address)?.decrementAndGet()
                }
            }
        }
    }
    
    /**
     * Gets statistics about the connection pool.
     * 
     * @return a map with pool statistics
     */
    Map<String, Object> getStatistics() {
        Map<String, Object> stats = [:]
        
        for (String address : addresses) {
            Map<String, Object> serverStats = [:]
            serverStats['total'] = totalCounts.get(address)?.get() ?: 0
            serverStats['active'] = activeCounts.get(address)?.get() ?: 0
            serverStats['idle'] = connectionQueues.get(address)?.size() ?: 0
            stats[address] = serverStats
        }
        
        return stats
    }
}

