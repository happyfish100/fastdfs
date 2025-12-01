package org.fastdfs.client.protocol;

import java.nio.ByteBuffer;

/**
 * FastDFS protocol header (10 bytes).
 * 
 * The header consists of:
 * - 8 bytes: body length (big-endian long)
 * - 1 byte: command code
 * - 1 byte: status code
 */
public class ProtocolHeader {
    
    private long length;
    private byte cmd;
    private byte status;
    
    public ProtocolHeader() {
    }
    
    public ProtocolHeader(long length, byte cmd, byte status) {
        this.length = length;
        this.cmd = cmd;
        this.status = status;
    }
    
    /**
     * Encodes the header to a byte array.
     */
    public byte[] encode() {
        ByteBuffer buffer = ByteBuffer.allocate(ProtocolConstants.FDFS_PROTO_HEADER_LEN);
        buffer.putLong(length);
        buffer.put(cmd);
        buffer.put(status);
        return buffer.array();
    }
    
    /**
     * Decodes a header from a byte array.
     */
    public static ProtocolHeader decode(byte[] data) {
        if (data == null || data.length < ProtocolConstants.FDFS_PROTO_HEADER_LEN) {
            throw new IllegalArgumentException("Invalid header data");
        }
        
        ByteBuffer buffer = ByteBuffer.wrap(data);
        long length = buffer.getLong();
        byte cmd = buffer.get();
        byte status = buffer.get();
        
        return new ProtocolHeader(length, cmd, status);
    }
    
    public long getLength() {
        return length;
    }
    
    public void setLength(long length) {
        this.length = length;
    }
    
    public byte getCmd() {
        return cmd;
    }
    
    public void setCmd(byte cmd) {
        this.cmd = cmd;
    }
    
    public byte getStatus() {
        return status;
    }
    
    public void setStatus(byte status) {
        this.status = status;
    }
    
    @Override
    public String toString() {
        return "ProtocolHeader{" +
                "length=" + length +
                ", cmd=" + cmd +
                ", status=" + status +
                '}';
    }
}
