/**
 * FastDFS Connection
 * 
 * This class represents a TCP connection to a FastDFS server (tracker or storage).
 * It handles socket management, I/O operations, and connection state.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.connection

import com.fastdfs.client.errors.*
import java.net.*
import java.nio.*
import java.nio.channels.*
import java.util.concurrent.atomic.*

/**
 * Connection to a FastDFS server.
 * 
 * This class manages a TCP socket connection to a FastDFS server.
 * It provides methods for sending and receiving data according to
 * the FastDFS protocol.
 * 
 * Thread-safety: This class is not thread-safe. Each connection
 * should be used by only one thread at a time.
 */
class Connection {
    
    // ============================================================================
    // Configuration
    // ============================================================================
    
    /**
     * Server address (host:port).
     */
    private final String address
    
    /**
     * Host name or IP address.
     */
    private final String host
    
    /**
     * Port number.
     */
    private final int port
    
    /**
     * Connection timeout in milliseconds.
     */
    private final long connectTimeout
    
    // ============================================================================
    // Connection State
    // ============================================================================
    
    /**
     * Socket channel for the connection.
     */
    private SocketChannel socketChannel
    
    /**
     * Socket for the connection.
     */
    private Socket socket
    
    /**
     * Flag indicating if the connection is closed.
     */
    private volatile boolean closed
    
    /**
     * Timestamp when the connection was last used.
     * 
     * Used for idle connection cleanup.
     */
    private final AtomicLong lastUsedTime
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new connection to the specified address.
     * 
     * @param address server address in format "host:port" (required)
     * @param connectTimeout connection timeout in milliseconds (must be > 0)
     * @throws FastDFSException if connection fails
     */
    Connection(String address, long connectTimeout) {
        // Validate parameters
        if (address == null || address.trim().isEmpty()) {
            throw new IllegalArgumentException("Address cannot be null or empty")
        }
        
        if (connectTimeout < 1) {
            throw new IllegalArgumentException("Connect timeout must be at least 1ms")
        }
        
        // Parse address
        String[] parts = address.split(':')
        if (parts.length != 2) {
            throw new IllegalArgumentException("Invalid address format: ${address}. Expected 'host:port'")
        }
        
        this.address = address
        this.host = parts[0]
        this.port = Integer.parseInt(parts[1])
        this.connectTimeout = connectTimeout
        
        // Initialize state
        this.closed = false
        this.lastUsedTime = new AtomicLong(System.currentTimeMillis())
        
        // Connect
        connect()
    }
    
    // ============================================================================
    // Connection Management
    // ============================================================================
    
    /**
     * Establishes the connection to the server.
     * 
     * @throws FastDFSException if connection fails
     */
    private void connect() {
        try {
            // Create socket channel
            socketChannel = SocketChannel.open()
            
            // Configure socket channel
            socketChannel.configureBlocking(true)
            
            // Get socket
            socket = socketChannel.socket()
            
            // Configure socket options
            socket.setTcpNoDelay(true)
            socket.setKeepAlive(true)
            socket.setReuseAddress(true)
            socket.setSoTimeout((int) connectTimeout)
            socket.setReceiveBufferSize(65536)
            socket.setSendBufferSize(65536)
            
            // Connect with timeout
            InetSocketAddress socketAddress = new InetSocketAddress(host, port)
            socket.connect(socketAddress, (int) connectTimeout)
            
        } catch (SocketTimeoutException e) {
            close()
            throw new ConnectionTimeoutException(address, e)
        } catch (ConnectException e) {
            close()
            throw new FastDFSException("Failed to connect to ${address}: ${e.message}", e)
        } catch (IOException e) {
            close()
            throw new NetworkError("connect", address, e)
        }
    }
    
    /**
     * Closes the connection.
     * 
     * This method is idempotent - calling it multiple times is safe.
     */
    void close() {
        if (closed) {
            return
        }
        
        closed = true
        
        try {
            if (socketChannel != null && socketChannel.isOpen()) {
                socketChannel.close()
            }
        } catch (IOException e) {
            // Ignore errors during close
        }
        
        try {
            if (socket != null && !socket.isClosed()) {
                socket.close()
            }
        } catch (IOException e) {
            // Ignore errors during close
        }
    }
    
    /**
     * Checks if the connection is valid (open and connected).
     * 
     * @return true if the connection is valid, false otherwise
     */
    boolean isValid() {
        if (closed) {
            return false
        }
        
        if (socket == null || socket.isClosed()) {
            return false
        }
        
        if (!socket.isConnected()) {
            return false
        }
        
        // Check if socket is still connected by trying to read
        // (without actually reading data)
        try {
            return socket.getChannel().isOpen()
        } catch (Exception e) {
            return false
        }
    }
    
    // ============================================================================
    // I/O Operations
    // ============================================================================
    
    /**
     * Sends data to the server.
     * 
     * @param data the data to send (required)
     * @param timeout timeout in milliseconds (must be > 0)
     * @throws FastDFSException if send fails
     */
    void send(byte[] data, long timeout) {
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null")
        }
        
        if (timeout < 1) {
            throw new IllegalArgumentException("Timeout must be at least 1ms")
        }
        
        checkValid()
        updateLastUsedTime()
        
        try {
            // Set socket timeout
            socket.setSoTimeout((int) timeout)
            
            // Write data
            ByteBuffer buffer = ByteBuffer.wrap(data)
            int totalWritten = 0
            
            while (buffer.hasRemaining()) {
                int written = socketChannel.write(buffer)
                if (written < 0) {
                    throw new IOException("Connection closed by server")
                }
                totalWritten += written
            }
            
            if (totalWritten != data.length) {
                throw new IOException("Incomplete write: ${totalWritten} of ${data.length} bytes")
            }
            
        } catch (SocketTimeoutException e) {
            throw new NetworkTimeoutException("write", address, e)
        } catch (IOException e) {
            close()
            throw new NetworkError("write", address, e)
        }
    }
    
    /**
     * Receives exactly the specified number of bytes from the server.
     * 
     * @param length number of bytes to receive (must be > 0)
     * @param timeout timeout in milliseconds (must be > 0)
     * @return the received data (never null, length equals requested length)
     * @throws FastDFSException if receive fails
     */
    byte[] receiveFull(int length, long timeout) {
        if (length < 1) {
            throw new IllegalArgumentException("Length must be at least 1")
        }
        
        if (timeout < 1) {
            throw new IllegalArgumentException("Timeout must be at least 1ms")
        }
        
        checkValid()
        updateLastUsedTime()
        
        try {
            // Set socket timeout
            socket.setSoTimeout((int) timeout)
            
            // Read data
            byte[] data = new byte[length]
            ByteBuffer buffer = ByteBuffer.wrap(data)
            int totalRead = 0
            
            while (buffer.hasRemaining()) {
                int read = socketChannel.read(buffer)
                if (read < 0) {
                    throw new IOException("Connection closed by server")
                }
                totalRead += read
            }
            
            if (totalRead != length) {
                throw new IOException("Incomplete read: ${totalRead} of ${length} bytes")
            }
            
            return data
            
        } catch (SocketTimeoutException e) {
            throw new NetworkTimeoutException("read", address, e)
        } catch (IOException e) {
            close()
            throw new NetworkError("read", address, e)
        }
    }
    
    // ============================================================================
    // Helper Methods
    // ============================================================================
    
    /**
     * Checks if the connection is valid and throws an exception if not.
     * 
     * @throws FastDFSException if the connection is invalid
     */
    private void checkValid() {
        if (!isValid()) {
            throw new FastDFSException("Connection is closed or invalid: ${address}")
        }
    }
    
    /**
     * Updates the last used time to the current time.
     */
    void updateLastUsedTime() {
        lastUsedTime.set(System.currentTimeMillis())
    }
    
    /**
     * Gets the last used time.
     * 
     * @return the timestamp when the connection was last used
     */
    long getLastUsedTime() {
        return lastUsedTime.get()
    }
    
    /**
     * Gets the server address.
     * 
     * @return the address (host:port)
     */
    String getAddress() {
        return address
    }
    
    /**
     * Gets the host name.
     * 
     * @return the host name or IP address
     */
    String getHost() {
        return host
    }
    
    /**
     * Gets the port number.
     * 
     * @return the port number
     */
    int getPort() {
        return port
    }
    
    /**
     * Returns a string representation.
     * 
     * @return string representation
     */
    @Override
    String toString() {
        return "Connection{" +
            "address='" + address + '\'' +
            ", closed=" + closed +
            ", valid=" + isValid() +
            '}'
    }
}

