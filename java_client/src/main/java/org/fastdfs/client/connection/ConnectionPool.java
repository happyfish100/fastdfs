package org.fastdfs.client.connection;

import org.fastdfs.client.exception.ConnectionTimeoutException;
import org.fastdfs.client.exception.NetworkException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Connection pool for managing connections to FastDFS servers.
 * 
 * This pool maintains connections to multiple servers and provides
 * connection pooling with automatic cleanup of idle connections.
 */
public class ConnectionPool {
    
    private static final Logger logger = LoggerFactory.getLogger(ConnectionPool.class);
    
    private final List<String> serverAddrs;
    private final int maxConns;
    private final int connectTimeout;
    private final int idleTimeout;
    private final Map<String, LinkedBlockingQueue<Connection>> pools;
    private final AtomicBoolean closed;
    
    /**
     * Creates a new connection pool.
     * 
     * @param serverAddrs List of server addresses in format "host:port"
     * @param maxConns Maximum number of connections per server
     * @param connectTimeout Connection timeout in milliseconds
     * @param idleTimeout Idle timeout for connections in milliseconds
     */
    public ConnectionPool(List<String> serverAddrs, int maxConns, int connectTimeout, int idleTimeout) {
        this.serverAddrs = serverAddrs;
        this.maxConns = maxConns;
        this.connectTimeout = connectTimeout;
        this.idleTimeout = idleTimeout;
        this.pools = new ConcurrentHashMap<>();
        this.closed = new AtomicBoolean(false);
        
        // Initialize pools for each server
        for (String addr : serverAddrs) {
            pools.put(addr, new LinkedBlockingQueue<>(maxConns));
        }
        
        logger.info("Connection pool initialized with {} servers, max {} connections per server",
                serverAddrs.size(), maxConns);
    }
    
    /**
     * Adds a server address to the pool dynamically.
     */
    public void addServer(String addr) {
        if (!pools.containsKey(addr)) {
            pools.put(addr, new LinkedBlockingQueue<>(maxConns));
            logger.info("Added server to pool: {}", addr);
        }
    }
    
    /**
     * Gets a connection from the pool or creates a new one.
     * 
     * @param addr Server address
     * @return A connection to the server
     */
    public Connection getConnection(String addr) throws ConnectionTimeoutException, NetworkException {
        if (closed.get()) {
            throw new IllegalStateException("Connection pool is closed");
        }
        
        LinkedBlockingQueue<Connection> pool = pools.get(addr);
        if (pool == null) {
            // Dynamically add the server if not in the pool
            addServer(addr);
            pool = pools.get(addr);
        }
        
        // Try to get an existing connection from the pool
        Connection conn = pool.poll();
        
        // Check if connection is still valid
        if (conn != null) {
            if (conn.isAlive() && !conn.isIdle(idleTimeout)) {
                logger.debug("Reusing connection to {}", addr);
                return conn;
            } else {
                // Connection is stale, close it
                logger.debug("Closing stale connection to {}", addr);
                conn.close();
            }
        }
        
        // Create a new connection
        logger.debug("Creating new connection to {}", addr);
        return new Connection(addr, connectTimeout);
    }
    
    /**
     * Returns a connection to the pool.
     * 
     * @param conn Connection to return
     */
    public void returnConnection(Connection conn) {
        if (conn == null || !conn.isAlive()) {
            return;
        }
        
        if (closed.get()) {
            conn.close();
            return;
        }
        
        LinkedBlockingQueue<Connection> pool = pools.get(conn.getAddr());
        if (pool != null) {
            boolean offered = pool.offer(conn);
            if (!offered) {
                // Pool is full, close the connection
                logger.debug("Pool full, closing connection to {}", conn.getAddr());
                conn.close();
            } else {
                logger.debug("Returned connection to pool: {}", conn.getAddr());
            }
        } else {
            conn.close();
        }
    }
    
    /**
     * Closes the connection pool and all connections.
     */
    public void close() {
        if (closed.compareAndSet(false, true)) {
            logger.info("Closing connection pool");
            
            for (Map.Entry<String, LinkedBlockingQueue<Connection>> entry : pools.entrySet()) {
                LinkedBlockingQueue<Connection> pool = entry.getValue();
                Connection conn;
                while ((conn = pool.poll()) != null) {
                    conn.close();
                }
            }
            
            pools.clear();
            logger.info("Connection pool closed");
        }
    }
    
    /**
     * Cleans up idle connections in the pool.
     */
    public void cleanupIdleConnections() {
        if (closed.get()) {
            return;
        }
        
        for (Map.Entry<String, LinkedBlockingQueue<Connection>> entry : pools.entrySet()) {
            LinkedBlockingQueue<Connection> pool = entry.getValue();
            LinkedBlockingQueue<Connection> tempQueue = new LinkedBlockingQueue<>();
            
            Connection conn;
            while ((conn = pool.poll()) != null) {
                if (conn.isAlive() && !conn.isIdle(idleTimeout)) {
                    tempQueue.offer(conn);
                } else {
                    logger.debug("Closing idle connection to {}", conn.getAddr());
                    conn.close();
                }
            }
            
            // Put back valid connections
            Connection tempConn;
            while ((tempConn = tempQueue.poll()) != null) {
                pool.offer(tempConn);
            }
        }
    }
    
    public boolean isClosed() {
        return closed.get();
    }
    
    public List<String> getServerAddrs() {
        return serverAddrs;
    }
}
