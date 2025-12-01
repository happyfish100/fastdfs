package org.fastdfs.client.connection;

import org.fastdfs.client.exception.ConnectionTimeoutException;
import org.fastdfs.client.exception.NetworkException;
import org.fastdfs.client.protocol.ProtocolConstants;
import org.fastdfs.client.protocol.ProtocolHeader;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketTimeoutException;

/**
 * Represents a connection to a FastDFS server (tracker or storage).
 */
public class Connection implements AutoCloseable {
    
    private static final Logger logger = LoggerFactory.getLogger(Connection.class);
    
    private final String addr;
    private final Socket socket;
    private final InputStream inputStream;
    private final OutputStream outputStream;
    private long lastActiveTime;
    
    /**
     * Creates a new connection to the specified address.
     * 
     * @param addr Server address in format "host:port"
     * @param connectTimeout Connection timeout in milliseconds
     */
    public Connection(String addr, int connectTimeout) throws ConnectionTimeoutException, NetworkException {
        this.addr = addr;
        this.lastActiveTime = System.currentTimeMillis();
        
        String[] parts = addr.split(":");
        if (parts.length != 2) {
            throw new IllegalArgumentException("Invalid address format: " + addr);
        }
        
        String host = parts[0];
        int port;
        try {
            port = Integer.parseInt(parts[1]);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("Invalid port in address: " + addr);
        }
        
        try {
            socket = new Socket();
            socket.connect(new InetSocketAddress(host, port), connectTimeout);
            socket.setSoTimeout(connectTimeout);
            socket.setTcpNoDelay(true);
            socket.setKeepAlive(true);
            
            inputStream = socket.getInputStream();
            outputStream = socket.getOutputStream();
            
            logger.debug("Connected to {}", addr);
        } catch (SocketTimeoutException e) {
            throw new ConnectionTimeoutException(addr);
        } catch (IOException e) {
            throw new NetworkException("connect", addr, e);
        }
    }
    
    /**
     * Sends data to the server.
     */
    public void send(byte[] data) throws NetworkException {
        try {
            outputStream.write(data);
            outputStream.flush();
            lastActiveTime = System.currentTimeMillis();
        } catch (IOException e) {
            throw new NetworkException("send", addr, e);
        }
    }
    
    /**
     * Receives the protocol header from the server.
     */
    public ProtocolHeader receiveHeader() throws NetworkException {
        byte[] headerData = receive(ProtocolConstants.FDFS_PROTO_HEADER_LEN);
        return ProtocolHeader.decode(headerData);
    }
    
    /**
     * Receives exactly the specified number of bytes from the server.
     */
    public byte[] receive(int length) throws NetworkException {
        if (length <= 0) {
            return new byte[0];
        }
        
        byte[] data = new byte[length];
        int totalRead = 0;
        
        try {
            while (totalRead < length) {
                int read = inputStream.read(data, totalRead, length - totalRead);
                if (read < 0) {
                    throw new IOException("Connection closed by server");
                }
                totalRead += read;
            }
            
            lastActiveTime = System.currentTimeMillis();
            return data;
        } catch (IOException e) {
            throw new NetworkException("receive", addr, e);
        }
    }
    
    /**
     * Checks if the connection is idle for longer than the specified timeout.
     */
    public boolean isIdle(long idleTimeout) {
        return System.currentTimeMillis() - lastActiveTime > idleTimeout;
    }
    
    /**
     * Checks if the connection is still alive.
     */
    public boolean isAlive() {
        return socket != null && socket.isConnected() && !socket.isClosed();
    }
    
    public String getAddr() {
        return addr;
    }
    
    @Override
    public void close() {
        try {
            if (inputStream != null) {
                inputStream.close();
            }
        } catch (IOException e) {
            logger.warn("Error closing input stream", e);
        }
        
        try {
            if (outputStream != null) {
                outputStream.close();
            }
        } catch (IOException e) {
            logger.warn("Error closing output stream", e);
        }
        
        try {
            if (socket != null) {
                socket.close();
            }
        } catch (IOException e) {
            logger.warn("Error closing socket", e);
        }
        
        logger.debug("Closed connection to {}", addr);
    }
}
