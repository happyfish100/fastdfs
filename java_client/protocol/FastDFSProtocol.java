/**
 * FastDFSProtocol - Protocol Encoding and Decoding
 * 
 * This class provides static utility methods for encoding and decoding FastDFS
 * protocol messages. It handles header encoding/decoding, metadata encoding/decoding,
 * integer encoding/decoding, and string padding/unpadding according to the FastDFS
 * protocol specification.
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
package com.fastdfs.client.protocol;

import com.fastdfs.client.exception.FastDFSException;
import com.fastdfs.client.exception.FastDFSError;
import com.fastdfs.client.types.FastDFSTypes;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;

/**
 * FastDFSProtocol provides protocol encoding and decoding utilities.
 * 
 * This class contains static methods for encoding and decoding FastDFS protocol
 * messages according to the FastDFS protocol specification. All methods are
 * stateless and thread-safe.
 * 
 * The FastDFS protocol uses:
 * - Big-endian byte order for all numeric values
 * - Fixed-width fields padded with null bytes
 * - Special separators for metadata encoding
 * - 10-byte headers for all messages
 * 
 * Example usage:
 * <pre>
 * // Encode a protocol header
 * byte[] header = FastDFSProtocol.encodeHeader(1024, (byte)11, (byte)0);
 * 
 * // Decode a protocol header
 * ProtocolHeader decoded = FastDFSProtocol.decodeHeader(header);
 * 
 * // Encode metadata
 * Map&lt;String, String&gt; metadata = new HashMap&lt;&gt;();
 * metadata.put("author", "John Doe");
 * byte[] encoded = FastDFSProtocol.encodeMetadata(metadata);
 * </pre>
 */
public final class FastDFSProtocol {
    
    // ============================================================================
    // Private Constructor - Prevent Instantiation
    // ============================================================================
    
    /**
     * Private constructor to prevent instantiation.
     * 
     * This class contains only static utility methods and should not be instantiated.
     */
    private FastDFSProtocol() {
        throw new UnsupportedOperationException("This class cannot be instantiated");
    }
    
    // ============================================================================
    // Protocol Header Encoding/Decoding
    // ============================================================================
    
    /**
     * Encodes a FastDFS protocol header into a 10-byte array.
     * 
     * The header format is:
     * - Bytes 0-7: Body length (8 bytes, big-endian uint64)
     * - Byte 8: Command code
     * - Byte 9: Status code (0 for request, error code for response)
     * 
     * @param length the length of the message body in bytes (must be >= 0)
     * @param cmd the protocol command code
     * @param status the status code (typically 0 for requests)
     * @return a 10-byte header ready to be sent to the server
     */
    public static byte[] encodeHeader(long length, byte cmd, byte status) {
        // Validate length
        if (length < 0) {
            throw new IllegalArgumentException("Length cannot be negative");
        }
        
        // Create header buffer
        ByteBuffer buffer = ByteBuffer.allocate(FastDFSTypes.FDFS_PROTO_HEADER_LEN);
        buffer.order(ByteOrder.BIG_ENDIAN);
        
        // Write body length (8 bytes, big-endian)
        buffer.putLong(length);
        
        // Write command code (1 byte)
        buffer.put(cmd);
        
        // Write status code (1 byte)
        buffer.put(status);
        
        // Return header bytes
        return buffer.array();
    }
    
    /**
     * Decodes a FastDFS protocol header from a byte array.
     * 
     * The header must be exactly 10 bytes long. This method parses the
     * header and extracts the body length, command code, and status code.
     * 
     * @param data the raw header bytes (must be at least 10 bytes)
     * @return a ProtocolHeader object containing the parsed values
     * @throws FastDFSException if data is too short or invalid
     * @throws IllegalArgumentException if data is null
     */
    public static ProtocolHeader decodeHeader(byte[] data) throws FastDFSException {
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        if (data.length < FastDFSTypes.FDFS_PROTO_HEADER_LEN) {
            throw new FastDFSException(
                FastDFSError.PROTOCOL_DECODING_ERROR,
                "Header data too short: expected " + FastDFSTypes.FDFS_PROTO_HEADER_LEN + 
                " bytes, got " + data.length
            );
        }
        
        // Parse header
        ByteBuffer buffer = ByteBuffer.wrap(data);
        buffer.order(ByteOrder.BIG_ENDIAN);
        
        // Read body length (8 bytes, big-endian)
        long length = buffer.getLong();
        
        // Read command code (1 byte)
        byte cmd = buffer.get();
        
        // Read status code (1 byte)
        byte status = buffer.get();
        
        // Create and return header object
        return new ProtocolHeader(length, cmd, status);
    }
    
    // ============================================================================
    // Integer Encoding/Decoding
    // ============================================================================
    
    /**
     * Encodes a 64-bit integer to an 8-byte big-endian representation.
     * 
     * FastDFS protocol uses big-endian byte order for all numeric fields.
     * This method converts a Java long to the 8-byte big-endian format used
     * in FastDFS protocol messages.
     * 
     * @param value the integer to encode
     * @return an 8-byte array in big-endian format
     */
    public static byte[] encodeInt64(long value) {
        ByteBuffer buffer = ByteBuffer.allocate(8);
        buffer.order(ByteOrder.BIG_ENDIAN);
        buffer.putLong(value);
        return buffer.array();
    }
    
    /**
     * Decodes an 8-byte big-endian representation to a 64-bit integer.
     * 
     * This is the inverse operation of encodeInt64(). It reads 8 bytes from
     * the provided array and interprets them as a big-endian 64-bit integer.
     * 
     * @param data byte array (must be at least 8 bytes)
     * @return the decoded integer
     * @throws FastDFSException if data is too short
     * @throws IllegalArgumentException if data is null
     */
    public static long decodeInt64(byte[] data) throws FastDFSException {
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        if (data.length < 8) {
            throw new FastDFSException(
                FastDFSError.PROTOCOL_DECODING_ERROR,
                "Data too short for int64: expected 8 bytes, got " + data.length
            );
        }
        
        // Decode integer
        ByteBuffer buffer = ByteBuffer.wrap(data, 0, 8);
        buffer.order(ByteOrder.BIG_ENDIAN);
        return buffer.getLong();
    }
    
    /**
     * Encodes a 32-bit integer to a 4-byte big-endian representation.
     * 
     * @param value the integer to encode
     * @return a 4-byte array in big-endian format
     */
    public static byte[] encodeInt32(int value) {
        ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.order(ByteOrder.BIG_ENDIAN);
        buffer.putInt(value);
        return buffer.array();
    }
    
    /**
     * Decodes a 4-byte big-endian representation to a 32-bit integer.
     * 
     * @param data byte array (must be at least 4 bytes)
     * @return the decoded integer
     * @throws FastDFSException if data is too short
     * @throws IllegalArgumentException if data is null
     */
    public static int decodeInt32(byte[] data) throws FastDFSException {
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        if (data.length < 4) {
            throw new FastDFSException(
                FastDFSError.PROTOCOL_DECODING_ERROR,
                "Data too short for int32: expected 4 bytes, got " + data.length
            );
        }
        
        // Decode integer
        ByteBuffer buffer = ByteBuffer.wrap(data, 0, 4);
        buffer.order(ByteOrder.BIG_ENDIAN);
        return buffer.getInt();
    }
    
    // ============================================================================
    // String Padding/Unpadding
    // ============================================================================
    
    /**
     * Pads a string to a fixed length with null bytes (0x00).
     * 
     * This is used to create fixed-width fields in the FastDFS protocol.
     * If the string is longer than the specified length, it will be truncated.
     * 
     * @param str the string to pad
     * @param length the desired length in bytes
     * @return a byte array of exactly 'length' bytes
     * @throws IllegalArgumentException if length is negative
     */
    public static byte[] padString(String str, int length) {
        // Validate input
        if (length < 0) {
            throw new IllegalArgumentException("Length cannot be negative");
        }
        
        // Create buffer
        byte[] buffer = new byte[length];
        
        // Copy string bytes (truncate if necessary)
        if (str != null) {
            byte[] strBytes = str.getBytes();
            int copyLength = Math.min(strBytes.length, length);
            System.arraycopy(strBytes, 0, buffer, 0, copyLength);
        }
        
        // Remaining bytes are already 0 (null bytes)
        return buffer;
    }
    
    /**
     * Removes trailing null bytes from a byte slice.
     * 
     * This is the inverse of padString(), used to extract strings from
     * fixed-width protocol fields. It converts the byte array to a string
     * and removes any trailing null characters.
     * 
     * @param data byte array with potential trailing nulls
     * @return the string with trailing null bytes removed
     * @throws IllegalArgumentException if data is null
     */
    public static String unpadString(byte[] data) {
        // Validate input
        if (data == null) {
            throw new IllegalArgumentException("Data cannot be null");
        }
        
        // Find the last non-null byte
        int length = data.length;
        while (length > 0 && data[length - 1] == 0) {
            length--;
        }
        
        // Convert to string (only up to the last non-null byte)
        if (length == 0) {
            return "";
        }
        return new String(data, 0, length);
    }
    
    // ============================================================================
    // Metadata Encoding/Decoding
    // ============================================================================
    
    /**
     * Encodes metadata key-value pairs into FastDFS wire format.
     * 
     * The format uses special separators:
     * - Field separator (0x02) between key and value
     * - Record separator (0x01) between different key-value pairs
     * 
     * Format: key1&lt;0x02&gt;value1&lt;0x01&gt;key2&lt;0x02&gt;value2&lt;0x01&gt;
     * 
     * Keys are truncated to 64 bytes and values to 256 bytes if they exceed limits.
     * 
     * @param metadata map of key-value pairs to encode
     * @return the encoded byte array, or null if metadata is null or empty
     */
    public static byte[] encodeMetadata(Map<String, String> metadata) {
        // Check if metadata is null or empty
        if (metadata == null || metadata.isEmpty()) {
            return null;
        }
        
        // Build encoded data
        java.io.ByteArrayOutputStream buffer = new java.io.ByteArrayOutputStream();
        
        for (Map.Entry<String, String> entry : metadata.entrySet()) {
            // Get key and value
            String key = entry.getKey();
            String value = entry.getValue();
            
            // Truncate if necessary
            if (key != null && key.length() > FastDFSTypes.FDFS_MAX_META_NAME_LEN) {
                key = key.substring(0, FastDFSTypes.FDFS_MAX_META_NAME_LEN);
            }
            if (value != null && value.length() > FastDFSTypes.FDFS_MAX_META_VALUE_LEN) {
                value = value.substring(0, FastDFSTypes.FDFS_MAX_META_VALUE_LEN);
            }
            
            // Skip null keys or values
            if (key == null || value == null) {
                continue;
            }
            
            // Write key
            buffer.write(key.getBytes(), 0, key.getBytes().length);
            
            // Write field separator
            buffer.write(FastDFSTypes.FDFS_FIELD_SEPARATOR);
            
            // Write value
            buffer.write(value.getBytes(), 0, value.getBytes().length);
            
            // Write record separator
            buffer.write(FastDFSTypes.FDFS_RECORD_SEPARATOR);
        }
        
        // Return encoded bytes
        return buffer.toByteArray();
    }
    
    /**
     * Decodes FastDFS wire format metadata into a map.
     * 
     * This is the inverse operation of encodeMetadata(). It parses records
     * separated by 0x01 and fields separated by 0x02. Invalid records
     * (not exactly 2 fields) are silently skipped.
     * 
     * @param data the raw metadata bytes from the server (can be null or empty)
     * @return a map containing the decoded key-value pairs (never null, may be empty)
     */
    public static Map<String, String> decodeMetadata(byte[] data) {
        // Create result map
        Map<String, String> metadata = new HashMap<>();
        
        // Check if data is null or empty
        if (data == null || data.length == 0) {
            return metadata;
        }
        
        // Split by record separator
        byte[][] records = splitBytes(data, FastDFSTypes.FDFS_RECORD_SEPARATOR);
        
        // Parse each record
        for (byte[] record : records) {
            // Skip empty records
            if (record.length == 0) {
                continue;
            }
            
            // Split by field separator
            byte[][] fields = splitBytes(record, FastDFSTypes.FDFS_FIELD_SEPARATOR);
            
            // Must have exactly 2 fields (key and value)
            if (fields.length != 2) {
                continue;
            }
            
            // Extract key and value
            String key = new String(fields[0]);
            String value = new String(fields[1]);
            
            // Add to metadata map
            metadata.put(key, value);
        }
        
        return metadata;
    }
    
    /**
     * Splits a byte array by a delimiter byte.
     * 
     * This is a helper method for parsing metadata and other delimited data.
     * 
     * @param data the byte array to split
     * @param delimiter the delimiter byte
     * @return an array of byte arrays, one for each segment
     */
    private static byte[][] splitBytes(byte[] data, byte delimiter) {
        // Count delimiters
        int count = 0;
        for (byte b : data) {
            if (b == delimiter) {
                count++;
            }
        }
        
        // Create result array
        byte[][] result = new byte[count + 1][];
        
        // Split data
        int start = 0;
        int index = 0;
        for (int i = 0; i < data.length; i++) {
            if (data[i] == delimiter) {
                // Extract segment
                int length = i - start;
                result[index] = new byte[length];
                System.arraycopy(data, start, result[index], 0, length);
                index++;
                start = i + 1;
            }
        }
        
        // Add last segment
        int length = data.length - start;
        result[index] = new byte[length];
        System.arraycopy(data, start, result[index], 0, length);
        
        return result;
    }
    
    // ============================================================================
    // File ID Parsing
    // ============================================================================
    
    /**
     * Splits a FastDFS file ID into its components.
     * 
     * A file ID has the format: "groupName/path/to/file"
     * For example: "group1/M00/00/00/wKgBcFxyz.jpg"
     * 
     * @param fileId the complete file ID string
     * @return a FileIDComponents object containing group name and remote filename
     * @throws FastDFSException if the file ID format is invalid
     * @throws IllegalArgumentException if fileId is null or empty
     */
    public static FileIDComponents splitFileID(String fileId) throws FastDFSException {
        // Validate input
        if (fileId == null || fileId.trim().isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty");
        }
        
        // Find first slash
        int slashIndex = fileId.indexOf('/');
        if (slashIndex < 0) {
            throw new FastDFSException(
                FastDFSError.INVALID_FILE_ID,
                "File ID must contain a slash: " + fileId
            );
        }
        
        // Extract group name and remote filename
        String groupName = fileId.substring(0, slashIndex);
        String remoteFilename = fileId.substring(slashIndex + 1);
        
        // Validate group name
        if (groupName.isEmpty() || groupName.length() > FastDFSTypes.FDFS_GROUP_NAME_MAX_LEN) {
            throw new FastDFSException(
                FastDFSError.INVALID_FILE_ID,
                "Invalid group name in file ID: " + fileId
            );
        }
        
        // Validate remote filename
        if (remoteFilename.isEmpty()) {
            throw new FastDFSException(
                FastDFSError.INVALID_FILE_ID,
                "Remote filename cannot be empty in file ID: " + fileId
            );
        }
        
        // Return components
        return new FileIDComponents(groupName, remoteFilename);
    }
    
    /**
     * Constructs a complete file ID from its components.
     * 
     * This is the inverse operation of splitFileID(). It combines the group
     * name and remote filename into a complete file ID string.
     * 
     * @param groupName the storage group name
     * @param remoteFilename the path and filename on the storage server
     * @return a complete file ID in the format "groupName/remoteFilename"
     */
    public static String joinFileID(String groupName, String remoteFilename) {
        return groupName + "/" + remoteFilename;
    }
    
    // ============================================================================
    // Inner Classes
    // ============================================================================
    
    /**
     * ProtocolHeader represents a parsed FastDFS protocol header.
     * 
     * This class holds the three components of a protocol header: body length,
     * command code, and status code.
     */
    public static class ProtocolHeader {
        private final long length;
        private final byte cmd;
        private final byte status;
        
        public ProtocolHeader(long length, byte cmd, byte status) {
            this.length = length;
            this.cmd = cmd;
            this.status = status;
        }
        
        public long getLength() {
            return length;
        }
        
        public byte getCmd() {
            return cmd;
        }
        
        public byte getStatus() {
            return status;
        }
    }
    
    /**
     * FileIDComponents represents the components of a FastDFS file ID.
     * 
     * This class holds the group name and remote filename that together
     * form a complete file ID.
     */
    public static class FileIDComponents {
        private final String groupName;
        private final String remoteFilename;
        
        public FileIDComponents(String groupName, String remoteFilename) {
            this.groupName = groupName;
            this.remoteFilename = remoteFilename;
        }
        
        public String getGroupName() {
            return groupName;
        }
        
        public String getRemoteFilename() {
            return remoteFilename;
        }
    }
}

