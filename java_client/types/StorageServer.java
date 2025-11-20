/**
 * StorageServer - Storage Server Information
 * 
 * This class represents information about a storage server in the FastDFS cluster.
 * The information is returned by the tracker when querying for upload or download
 * operations, and includes the server's IP address, port, and storage path index.
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
package com.fastdfs.client.types;

/**
 * StorageServer represents a storage server in the FastDFS cluster.
 * 
 * This information is returned by the tracker server when querying for
 * upload or download operations. It contains the network address of the
 * storage server and the storage path index to use for file operations.
 * 
 * Storage servers can have multiple storage paths configured, and the
 * tracker specifies which path index should be used for each operation.
 * This allows for load balancing across multiple storage paths on the
 * same server.
 * 
 * Example usage:
 * <pre>
 * StorageServer server = tracker.queryStorageServer();
 * String address = server.getIpAddr() + ":" + server.getPort();
 * byte pathIndex = server.getStorePathIndex();
 * </pre>
 */
public class StorageServer {
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * IP address of the storage server.
     * 
     * This is the IPv4 address of the storage server in dotted decimal
     * notation (e.g., "192.168.1.100"). IPv6 addresses are not commonly
     * used in FastDFS deployments.
     */
    private final String ipAddr;
    
    /**
     * Port number of the storage server.
     * 
     * This is the TCP port number where the storage server listens for
     * client connections. The default port is 23000, but it can be
     * configured differently.
     */
    private final int port;
    
    /**
     * Storage path index.
     * 
     * Storage servers can have multiple storage paths configured for
     * storing files. This index (0-based) specifies which path should
     * be used for the current operation.
     * 
     * The tracker selects the path index based on load balancing and
     * available space on each path.
     */
    private final byte storePathIndex;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new StorageServer object with the specified values.
     * 
     * @param ipAddr the IP address of the storage server (must not be null)
     * @param port the port number (must be between 1 and 65535)
     * @param storePathIndex the storage path index (0-based)
     * @throws IllegalArgumentException if ipAddr is null or port is invalid
     */
    public StorageServer(String ipAddr, int port, byte storePathIndex) {
        // Validate IP address
        if (ipAddr == null || ipAddr.trim().isEmpty()) {
            throw new IllegalArgumentException("IP address cannot be null or empty");
        }
        
        // Validate port number
        if (port < 1 || port > 65535) {
            throw new IllegalArgumentException("Port must be between 1 and 65535");
        }
        
        // Store values
        this.ipAddr = ipAddr;
        this.port = port;
        this.storePathIndex = storePathIndex;
    }
    
    /**
     * Creates a new StorageServer object without a storage path index.
     * 
     * This constructor is used when the storage path index is not available
     * or not needed (e.g., for download operations where the path is
     * determined by the file location).
     * 
     * @param ipAddr the IP address of the storage server
     * @param port the port number
     */
    public StorageServer(String ipAddr, int port) {
        this(ipAddr, port, (byte) 0);
    }
    
    // ============================================================================
    // Getters
    // ============================================================================
    
    /**
     * Gets the IP address of the storage server.
     * 
     * @return the IP address (never null)
     */
    public String getIpAddr() {
        return ipAddr;
    }
    
    /**
     * Gets the port number of the storage server.
     * 
     * @return the port number (between 1 and 65535)
     */
    public int getPort() {
        return port;
    }
    
    /**
     * Gets the storage path index.
     * 
     * @return the storage path index (0-based)
     */
    public byte getStorePathIndex() {
        return storePathIndex;
    }
    
    /**
     * Gets the server address in "host:port" format.
     * 
     * This is a convenience method that combines the IP address and port
     * into a single string, which is useful for connection operations.
     * 
     * @return the server address in "host:port" format
     */
    public String getAddress() {
        return ipAddr + ":" + port;
    }
    
    // ============================================================================
    // Object Methods
    // ============================================================================
    
    /**
     * Returns a string representation of this StorageServer object.
     * 
     * @return a string representation
     */
    @Override
    public String toString() {
        return "StorageServer{" +
                "ipAddr='" + ipAddr + '\'' +
                ", port=" + port +
                ", storePathIndex=" + storePathIndex +
                '}';
    }
    
    /**
     * Compares this StorageServer with another object for equality.
     * 
     * Two StorageServer objects are considered equal if their IP address,
     * port, and storage path index are all equal.
     * 
     * @param obj the object to compare with
     * @return true if the objects are equal, false otherwise
     */
    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        
        StorageServer that = (StorageServer) obj;
        
        if (port != that.port) {
            return false;
        }
        if (storePathIndex != that.storePathIndex) {
            return false;
        }
        return ipAddr.equals(that.ipAddr);
    }
    
    /**
     * Returns a hash code for this StorageServer object.
     * 
     * @return a hash code
     */
    @Override
    public int hashCode() {
        int result = ipAddr.hashCode();
        result = 31 * result + port;
        result = 31 * result + (int) storePathIndex;
        return result;
    }
}

