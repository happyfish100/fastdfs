/**
 * FastDFS Protocol Handler
 * 
 * This class handles encoding and decoding of FastDFS protocol messages.
 * It provides methods for building requests and parsing responses according
 * to the FastDFS protocol specification.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.protocol

import com.fastdfs.client.types.*
import java.nio.*
import java.nio.charset.*
import java.util.*

/**
 * Protocol handler for FastDFS protocol encoding and decoding.
 * 
 * This class handles all protocol-level operations including:
 * - Encoding protocol headers
 * - Decoding protocol headers
 * - Encoding metadata
 * - Decoding metadata
 * - Encoding file IDs
 * - Decoding file IDs
 * - Building request messages
 * - Parsing response messages
 */
class ProtocolHandler {
    
    // ============================================================================
    // Constants
    // ============================================================================
    
    /**
     * Character encoding for protocol strings.
     */
    private static final Charset PROTOCOL_CHARSET = Charset.forName('UTF-8')
    
    // ============================================================================
    // Protocol Header Encoding/Decoding
    // ============================================================================
    
    /**
     * Encodes a protocol header.
     * 
     * Protocol header format:
     * - 8 bytes: body length (big-endian long)
     * - 1 byte: command code
     * - 1 byte: status code
     * 
     * @param length the body length
     * @param cmd the command code
     * @param status the status code
     * @return the encoded header (10 bytes)
     */
    byte[] encodeHeader(long length, byte cmd, byte status) {
        ByteBuffer buffer = ByteBuffer.allocate(ProtocolConstants.FDFS_PROTO_HEADER_LEN)
        buffer.order(ByteOrder.BIG_ENDIAN)
        
        // Write body length (8 bytes, big-endian)
        buffer.putLong(length)
        
        // Write command code (1 byte)
        buffer.put(cmd)
        
        // Write status code (1 byte)
        buffer.put(status)
        
        return buffer.array()
    }
    
    /**
     * Decodes a protocol header.
     * 
     * @param data the header data (must be exactly 10 bytes)
     * @return the decoded header
     * @throws IllegalArgumentException if data length is invalid
     */
    ProtocolHeader decodeHeader(byte[] data) {
        if (data == null || data.length != ProtocolConstants.FDFS_PROTO_HEADER_LEN) {
            throw new IllegalArgumentException(
                "Invalid header length: ${data?.length}, expected ${ProtocolConstants.FDFS_PROTO_HEADER_LEN}"
            )
        }
        
        ByteBuffer buffer = ByteBuffer.wrap(data)
        buffer.order(ByteOrder.BIG_ENDIAN)
        
        // Read body length (8 bytes, big-endian)
        long length = buffer.getLong()
        
        // Read command code (1 byte)
        byte cmd = buffer.get()
        
        // Read status code (1 byte)
        byte status = buffer.get()
        
        return new ProtocolHeader(length, cmd, status)
    }
    
    // ============================================================================
    // Metadata Encoding/Decoding
    // ============================================================================
    
    /**
     * Encodes metadata to protocol format.
     * 
     * Metadata format:
     * key1\x02value1\x01key2\x02value2\x01...
     * 
     * Where \x01 is the record separator and \x02 is the field separator.
     * 
     * @param metadata the metadata map
     * @return the encoded metadata
     */
    byte[] encodeMetadata(Map<String, String> metadata) {
        if (metadata == null || metadata.isEmpty()) {
            return new byte[0]
        }
        
        List<Byte> bytes = new ArrayList<>()
        
        boolean first = true
        for (Map.Entry<String, String> entry : metadata.entrySet()) {
            if (!first) {
                bytes.add(ProtocolConstants.FDFS_RECORD_SEPARATOR)
            }
            first = false
            
            String key = entry.key
            String value = entry.value ?: ''
            
            // Validate key length
            if (key.length() > ProtocolConstants.FDFS_MAX_META_NAME_LEN) {
                throw new IllegalArgumentException(
                    "Metadata key too long: ${key.length()} > ${ProtocolConstants.FDFS_MAX_META_NAME_LEN}"
                )
            }
            
            // Validate value length
            if (value.length() > ProtocolConstants.FDFS_MAX_META_VALUE_LEN) {
                throw new IllegalArgumentException(
                    "Metadata value too long: ${value.length()} > ${ProtocolConstants.FDFS_MAX_META_VALUE_LEN}"
                )
            }
            
            // Add key
            byte[] keyBytes = key.getBytes(PROTOCOL_CHARSET)
            for (byte b : keyBytes) {
                bytes.add(b)
            }
            
            // Add field separator
            bytes.add(ProtocolConstants.FDFS_FIELD_SEPARATOR)
            
            // Add value
            byte[] valueBytes = value.getBytes(PROTOCOL_CHARSET)
            for (byte b : valueBytes) {
                bytes.add(b)
            }
        }
        
        // Convert to byte array
        byte[] result = new byte[bytes.size()]
        for (int i = 0; i < bytes.size(); i++) {
            result[i] = bytes.get(i)
        }
        
        return result
    }
    
    /**
     * Decodes metadata from protocol format.
     * 
     * @param data the encoded metadata
     * @return the metadata map
     */
    Map<String, String> decodeMetadata(byte[] data) {
        Map<String, String> metadata = [:]
        
        if (data == null || data.length == 0) {
            return metadata
        }
        
        // Split by record separator
        List<Byte> currentRecord = new ArrayList<>()
        
        for (int i = 0; i < data.length; i++) {
            byte b = data[i]
            
            if (b == ProtocolConstants.FDFS_RECORD_SEPARATOR) {
                // Process current record
                processMetadataRecord(currentRecord, metadata)
                currentRecord.clear()
            } else {
                currentRecord.add(b)
            }
        }
        
        // Process last record
        if (!currentRecord.isEmpty()) {
            processMetadataRecord(currentRecord, metadata)
        }
        
        return metadata
    }
    
    /**
     * Processes a single metadata record.
     * 
     * @param record the record bytes
     * @param metadata the metadata map to populate
     */
    private void processMetadataRecord(List<Byte> record, Map<String, String> metadata) {
        if (record.isEmpty()) {
            return
        }
        
        // Find field separator
        int separatorIndex = -1
        for (int i = 0; i < record.size(); i++) {
            if (record.get(i) == ProtocolConstants.FDFS_FIELD_SEPARATOR) {
                separatorIndex = i
                break
            }
        }
        
        if (separatorIndex < 0) {
            // No separator found, treat entire record as key with empty value
            byte[] keyBytes = new byte[record.size()]
            for (int i = 0; i < record.size(); i++) {
                keyBytes[i] = record.get(i)
            }
            String key = new String(keyBytes, PROTOCOL_CHARSET)
            metadata[key] = ''
            return
        }
        
        // Extract key
        byte[] keyBytes = new byte[separatorIndex]
        for (int i = 0; i < separatorIndex; i++) {
            keyBytes[i] = record.get(i)
        }
        String key = new String(keyBytes, PROTOCOL_CHARSET)
        
        // Extract value
        byte[] valueBytes = new byte[record.size() - separatorIndex - 1]
        for (int i = 0; i < valueBytes.length; i++) {
            valueBytes[i] = record.get(separatorIndex + 1 + i)
        }
        String value = new String(valueBytes, PROTOCOL_CHARSET)
        
        metadata[key] = value
    }
    
    // ============================================================================
    // File ID Parsing
    // ============================================================================
    
    /**
     * Parses a file ID into group name and remote filename.
     * 
     * File ID format: group_name/remote_filename
     * 
     * @param fileId the file ID
     * @return an array with [groupName, remoteFilename]
     * @throws IllegalArgumentException if file ID format is invalid
     */
    String[] parseFileId(String fileId) {
        if (fileId == null || fileId.isEmpty()) {
            throw new IllegalArgumentException("File ID cannot be null or empty")
        }
        
        int slashIndex = fileId.indexOf('/')
        if (slashIndex < 0) {
            throw new IllegalArgumentException("Invalid file ID format: ${fileId}. Expected 'group_name/remote_filename'")
        }
        
        String groupName = fileId.substring(0, slashIndex)
        String remoteFilename = fileId.substring(slashIndex + 1)
        
        if (groupName.isEmpty()) {
            throw new IllegalArgumentException("Group name cannot be empty in file ID: ${fileId}")
        }
        
        if (remoteFilename.isEmpty()) {
            throw new IllegalArgumentException("Remote filename cannot be empty in file ID: ${fileId}")
        }
        
        return [groupName, remoteFilename]
    }
    
    // ============================================================================
    // String Padding
    // ============================================================================
    
    /**
     * Pads a string to the specified length with null bytes.
     * 
     * @param str the string to pad
     * @param length the target length
     * @return the padded string as bytes
     */
    byte[] padString(String str, int length) {
        byte[] result = new byte[length]
        Arrays.fill(result, (byte) 0)
        
        if (str != null && !str.isEmpty()) {
            byte[] strBytes = str.getBytes(PROTOCOL_CHARSET)
            int copyLength = Math.min(strBytes.length, length)
            System.arraycopy(strBytes, 0, result, 0, copyLength)
        }
        
        return result
    }
}

