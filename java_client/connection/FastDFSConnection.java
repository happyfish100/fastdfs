/**
 * FastDFSConnection - TCP Connection Wrapper
 * 
 * This class represents a TCP connection to a FastDFS server (tracker or storage).
 * It wraps a Socket with additional metadata, thread-safe operations, and timeout
 * management for network I/O operations.
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

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * FastDFSConnection represents a TCP connection to a FastDFS server.
 * 
 * This class wraps a Java Socket and provides thread-safe methods for
 * sending and receiving data with timeout support. It tracks the last
 * usage time for connection pool management and provides health checking.
 * 
 * Connections are created by the connection pool and should not be
 * instantiated directly. They are returned to the pool after use.
 * 
 * Thread Safety:
 * All operations are thread-safe and protected by locks to ensure
 * that multiple threads can safely use the connection pool, though
 * each connection should only be used by one thread at a time.
 * 
 * Example usage (typically done by connection pool):
 * <pre>
 * FastDFSConnection conn = pool.get();
 * try {
 *     conn.send(data, timeout);
 *     byte[] response = conn.receiveFull(size, timeout);
 * } finally {
 *     pool.put(conn);
 * }
 * </pre>
 */
public class FastDFSConnection {
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * Underlying TCP socket.
     * 
     * This is the actual network socket used for communication with the server.
     * It is created when the connection is established and closed when the
     * connection is no longer needed.
     */
    private final Socket socket;
    
    /**
     * Server address in "host:port" format.
     * 
     * This is stored for logging and error reporting purposes. It identifies
     * which server this connection is connected to.
     */
    private final String address;
    
    /**
     * Input stream for reading data from the server.
     * 
     * This is obtained from the socket and cached for performance. It is
     * used by all receive operations.
     */
    private final InputStream inputStream;
    
    /**
     * Output stream for writing data to the server.
     * 
     * This is obtained from the socket and cached for performance. It is
     * used by all send operations.
     */
    private final OutputStream outputStream;
    
    /**
     * Lock for thread-safe access to the connection.
     * 
     * This lock protects all I/O operations to ensure that only one thread
     * can use the connection at a time. While the connection pool ensures
     * connections are not shared, this provides additional safety.
     */
    private final Lock lock = new ReentrantLock();
    
    /**
     * Timestamp of the last I/O operation.
     * 
     * This is updated whenever data is sent or received. It is used by the
     * connection pool to determine if a connection is idle and should be
     * closed to free resources.
     */
    private volatile long lastUsedTime;
    
    /**
     * Flag indicating whether the connection is closed.
     * 
     * Once closed, all operations will fail. This flag is checked before
     * performing any I/O operations.
     */
    private volatile boolean closed = false;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new FastDFSConnection from an existing socket.
     * 
     * This constructor is used by the connection pool when establishing
     * new connections. The socket must already be connected and ready
     * for I/O operations.
     * 
     * @param socket the connected socket (must not be null and must be connected)
     * @param address the server address in "host:port" format (for logging)
     * @throws IOException if input/output streams cannot be obtained
     * @throws IllegalArgumentException if socket is null or not connected
     */
    public FastDFSConnection(Socket socket, String address) throws IOException {
        // Validate socket
        if (socket == null) {
            throw new IllegalArgumentException("Socket cannot be null");
        }
        if (!socket.isConnected()) {
            throw new IllegalArgumentException("Socket must be connected");
        }
        
        // Validate address
        if (address == null || address.trim().isEmpty()) {
            throw new IllegalArgumentException("Address cannot be null or empty");
        }
        
        // Store socket and address
        this.socket = socket;
        this.address = address;
        
        // Get input/output streams
        // These are cached for performance to avoid repeated calls to getInputStream/getOutputStream
        this.inputStream = socket.getInputStream();
        this.outputStream = socket.getOutputStream();
        
        // Initialize last used time
        this.lastUsedTime = System.currentTimeMillis();
    }
    
    // ============================================================================
    // Public Methods - I/O Operations
    // ============================================================================
    
    /**
     * Sends data to the server.
     * 
     * This method writes the entire byte array to the server. If the write
     * cannot complete within the specified timeout, a FastDFSException is thrown.
     * 
     * The method is thread-safe and updates the last used time after a
     * successful write.
     * 
     * @param data the data to send (must not be null)
     * @param timeout the write timeout in milliseconds (0 means no timeout)
     * @throws FastDFSException if the write fails or times out
     * @throws IllegalArgumentException if data is null
     */
    public void send(byte[] data, int timeout) throws FastDFSException {
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        
        // Check if closed
        if (closed) {
            throw new FastDFSException(
                FastDFSError.CONNECTION_FAILED,
                "Connection is closed"
            );
        }
        
        // Acquire lock for thread-safe access
        lock.lock();
        try {
            // Set write timeout if specified
            if (timeout > 0) {
                socket.setSoTimeout(timeout);
            }
            
            // Write all data
            outputStream.write(data);
            outputStream.flush();
            
            // Update last used time
            lastUsedTime = System.currentTimeMillis();
            
        } catch (SocketTimeoutException e) {
            throw new FastDFSException(
                FastDFSError.NETWORK_TIMEOUT,
                "Write timeout after " + timeout + "ms to " + address,
                e
            );
        } catch (IOException e) {
            closed = true;
            throw new FastDFSException(
                FastDFSError.NETWORK_ERROR,
                "Write failed to " + address + ": " + e.getMessage(),
                e
            );
        } finally {
            // Always release the lock
            lock.unlock();
        }
    }
    
    /**
     * Receives up to 'size' bytes from the server.
     * 
     * This method may return fewer bytes than requested if the server sends
     * less data. For operations that require exactly 'size' bytes, use
     * receiveFull() instead.
     * 
     * The method is thread-safe and updates the last used time after a
     * successful read.
     * 
     * @param size the maximum number of bytes to read
     * @param timeout the read timeout in milliseconds (0 means no timeout)
     * @return the received data (may be less than 'size' bytes)
     * @throws FastDFSException if the read fails or times out
     */
    public byte[] receive(int size, int timeout) throws FastDFSException {
        // Validate input
        if (size <= 0) {
            throw new IllegalArgumentException("Size must be greater than 0");
        }
        
        // Check if closed
        if (closed) {
            throw new FastDFSException(
                FastDFSError.CONNECTION_FAILED,
                "Connection is closed"
            );
        }
        
        // Acquire lock for thread-safe access
        lock.lock();
        try {
            // Set read timeout if specified
            if (timeout > 0) {
                socket.setSoTimeout(timeout);
            }
            
            // Read data
            byte[] buffer = new byte[size];
            int bytesRead = inputStream.read(buffer);
            
            // Handle end of stream
            if (bytesRead == -1) {
                closed = true;
                throw new FastDFSException(
                    FastDFSError.NETWORK_ERROR,
                    "Connection closed by server: " + address
                );
            }
            
            // Update last used time
            lastUsedTime = System.currentTimeMillis();
            
            // Return only the bytes that were actually read
            if (bytesRead < size) {
                byte[] result = new byte[bytesRead];
                System.arraycopy(buffer, 0, result, 0, bytesRead);
                return result;
            }
            
            return buffer;
            
        } catch (SocketTimeoutException e) {
            throw new FastDFSException(
                FastDFSError.NETWORK_TIMEOUT,
                "Read timeout after " + timeout + "ms from " + address,
                e
            );
        } catch (IOException e) {
            closed = true;
            throw new FastDFSException(
                FastDFSError.NETWORK_ERROR,
                "Read failed from " + address + ": " + e.getMessage(),
                e
            );
        } finally {
            // Always release the lock
            lock.unlock();
        }
    }
    
    /**
     * Receives exactly 'size' bytes from the server.
     * 
     * This method blocks until exactly 'size' bytes have been received or
     * an error occurs. The timeout applies to the entire operation, not
     * individual read calls.
     * 
     * This is the preferred method for reading protocol headers and fixed-size
     * response fields where the exact number of bytes is known.
     * 
     * @param size the exact number of bytes to read (must be > 0)
     * @param timeout the total timeout for the operation in milliseconds (0 means no timeout)
     * @return exactly 'size' bytes
     * @throws FastDFSException if the read fails, times out, or fewer bytes are received
     */
    public byte[] receiveFull(int size, int timeout) throws FastDFSException {
        // Validate input
        if (size <= 0) {
            throw new IllegalArgumentException("Size must be greater than 0");
        }
        
        // Check if closed
        if (closed) {
            throw new FastDFSException(
                FastDFSError.CONNECTION_FAILED,
                "Connection is closed"
            );
        }
        
        // Acquire lock for thread-safe access
        lock.lock();
        try {
            // Set read timeout if specified
            if (timeout > 0) {
                socket.setSoTimeout(timeout);
            }
            
            // Read all bytes in a loop
            byte[] buffer = new byte[size];
            int totalRead = 0;
            long startTime = System.currentTimeMillis();
            
            while (totalRead < size) {
                // Check timeout if specified
                if (timeout > 0) {
                    long elapsed = System.currentTimeMillis() - startTime;
                    if (elapsed >= timeout) {
                        throw new FastDFSException(
                            FastDFSError.NETWORK_TIMEOUT,
                            "Read timeout after " + timeout + "ms from " + address
                        );
                    }
                }
                
                // Read data
                int bytesRead = inputStream.read(buffer, totalRead, size - totalRead);
                
                // Handle end of stream
                if (bytesRead == -1) {
                    closed = true;
                    throw new FastDFSException(
                        FastDFSError.NETWORK_ERROR,
                        "Connection closed by server: " + address + " (received " + totalRead + "/" + size + " bytes)"
                    );
                }
                
                totalRead += bytesRead;
            }
            
            // Update last used time
            lastUsedTime = System.currentTimeMillis();
            
            return buffer;
            
        } catch (SocketTimeoutException e) {
            throw new FastDFSException(
                FastDFSError.NETWORK_TIMEOUT,
                "Read timeout after " + timeout + "ms from " + address,
                e
            );
        } catch (IOException e) {
            closed = true;
            throw new FastDFSException(
                FastDFSError.NETWORK_ERROR,
                "Read failed from " + address + ": " + e.getMessage(),
                e
            );
        } finally {
            // Always release the lock
            lock.unlock();
        }
    }
    
    // ============================================================================
    // Public Methods - Connection Management
    // ============================================================================
    
    /**
     * Closes the connection and releases resources.
     * 
     * This method closes the underlying socket and marks the connection as closed.
     * After closing, all I/O operations will fail.
     * 
     * It is safe to call this method multiple times. Subsequent calls will
     * have no effect.
     * 
     * @throws FastDFSException if closing the socket fails
     */
    public void close() throws FastDFSException {
        // Check if already closed
        if (closed) {
            return;
        }
        
        // Acquire lock
        lock.lock();
        try {
            // Check again (double-check locking pattern)
            if (closed) {
                return;
            }
            
            // Mark as closed
            closed = true;
            
            // Close socket
            try {
                socket.close();
            } catch (IOException e) {
                throw new FastDFSException(
                    FastDFSError.CONNECTION_FAILED,
                    "Failed to close connection to " + address + ": " + e.getMessage(),
                    e
                );
            }
            
        } finally {
            // Always release the lock
            lock.unlock();
        }
    }
    
    /**
     * Checks if the connection is alive and healthy.
     * 
     * This method performs a non-blocking check to determine if the connection
     * is still valid. It attempts a very short read with timeout; if it times
     * out, the connection is considered alive (since the socket is still
     * responsive). If it gets an error, the connection is considered dead.
     * 
     * This is a heuristic check and may not detect all failure modes, but it's
     * sufficient for connection pool health checking.
     * 
     * @return true if the connection appears to be alive, false otherwise
     */
    public boolean isAlive() {
        // Check if already marked as closed
        if (closed) {
            return false;
        }
        
        // Acquire lock
        lock.lock();
        try {
            // Check if socket is closed
            if (socket.isClosed() || !socket.isConnected()) {
                closed = true;
                return false;
            }
            
            // Try a very short read with timeout to check if connection is alive
            // If it times out, the connection is alive (socket is responsive)
            // If it gets an error, the connection is dead
            int oldTimeout = socket.getSoTimeout();
            try {
                socket.setSoTimeout(1); // 1ms timeout
                int available = inputStream.available();
                // If available() returns without exception, connection is alive
                socket.setSoTimeout(oldTimeout); // Restore original timeout
                return true;
            } catch (IOException e) {
                // Connection is dead
                closed = true;
                socket.setSoTimeout(oldTimeout); // Restore original timeout
                return false;
            }
            
        } catch (Exception e) {
            // Any exception means connection is dead
            closed = true;
            return false;
        } finally {
            // Always release the lock
            lock.unlock();
        }
    }
    
    /**
     * Gets the timestamp of the last I/O operation.
     * 
     * This is used by the connection pool to determine if a connection
     * is idle and should be closed to free resources.
     * 
     * @return the last used timestamp in milliseconds since epoch
     */
    public long getLastUsedTime() {
        return lastUsedTime;
    }
    
    /**
     * Gets the server address this connection is connected to.
     * 
     * @return the server address in "host:port" format
     */
    public String getAddress() {
        return address;
    }
}

