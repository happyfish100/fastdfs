package org.fastdfs.client.protocol;

import org.fastdfs.client.exception.*;

import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Protocol utility methods for encoding and decoding FastDFS protocol messages.
 */
public final class ProtocolUtil {
    
    private static final String CHARSET = "UTF-8";
    
    private ProtocolUtil() {
        // Prevent instantiation
    }
    
    /**
     * Maps FastDFS protocol status codes to exceptions.
     */
    public static FastDFSException mapStatusToException(byte status) {
        switch (status) {
            case ProtocolConstants.STATUS_SUCCESS:
                return null;
            case ProtocolConstants.STATUS_FILE_NOT_FOUND:
                return new FileNotFoundException();
            case ProtocolConstants.STATUS_FILE_ALREADY_EXISTS:
                return new FileAlreadyExistsException();
            case ProtocolConstants.STATUS_INVALID_ARGUMENT:
                return new InvalidArgumentException();
            case ProtocolConstants.STATUS_INSUFFICIENT_SPACE:
                return new InsufficientSpaceException();
            default:
                return new ProtocolException(status, "Unknown error code: " + status);
        }
    }
    
    /**
     * Parses a file ID into group name and remote filename.
     * 
     * @param fileId File ID in format "group/remote_filename"
     * @return Array with [groupName, remoteFilename]
     */
    public static String[] parseFileId(String fileId) throws InvalidFileIdException {
        if (fileId == null || fileId.isEmpty()) {
            throw new InvalidFileIdException("File ID is empty");
        }
        
        int slashIndex = fileId.indexOf('/');
        if (slashIndex <= 0 || slashIndex >= fileId.length() - 1) {
            throw new InvalidFileIdException(fileId);
        }
        
        String groupName = fileId.substring(0, slashIndex);
        String remoteFilename = fileId.substring(slashIndex + 1);
        
        return new String[]{groupName, remoteFilename};
    }
    
    /**
     * Encodes a string to a fixed-length byte array.
     * If the string is shorter, it's padded with zeros.
     * If longer, it's truncated.
     */
    public static byte[] encodeFixedString(String str, int length) {
        byte[] result = new byte[length];
        if (str != null && !str.isEmpty()) {
            try {
                byte[] bytes = str.getBytes(CHARSET);
                int copyLen = Math.min(bytes.length, length);
                System.arraycopy(bytes, 0, result, 0, copyLen);
            } catch (UnsupportedEncodingException e) {
                throw new RuntimeException("UTF-8 encoding not supported", e);
            }
        }
        return result;
    }
    
    /**
     * Decodes a fixed-length byte array to a string, trimming null bytes.
     */
    public static String decodeFixedString(byte[] data) {
        return decodeFixedString(data, 0, data.length);
    }
    
    /**
     * Decodes a fixed-length byte array to a string, trimming null bytes.
     */
    public static String decodeFixedString(byte[] data, int offset, int length) {
        // Find the actual length (excluding trailing zeros)
        int actualLength = length;
        for (int i = offset + length - 1; i >= offset; i--) {
            if (data[i] != 0) {
                break;
            }
            actualLength--;
        }
        
        if (actualLength == 0) {
            return "";
        }
        
        try {
            return new String(data, offset, actualLength, CHARSET);
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException("UTF-8 encoding not supported", e);
        }
    }
    
    /**
     * Encodes metadata to FastDFS protocol format.
     * Format: key1\x02value1\x01key2\x02value2\x01...
     */
    public static byte[] encodeMetadata(Map<String, String> metadata) {
        if (metadata == null || metadata.isEmpty()) {
            return new byte[0];
        }
        
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> entry : metadata.entrySet()) {
            if (sb.length() > 0) {
                sb.append((char) ProtocolConstants.FDFS_RECORD_SEPARATOR);
            }
            sb.append(entry.getKey());
            sb.append((char) ProtocolConstants.FDFS_FIELD_SEPARATOR);
            sb.append(entry.getValue());
        }
        
        try {
            return sb.toString().getBytes(CHARSET);
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException("UTF-8 encoding not supported", e);
        }
    }
    
    /**
     * Decodes metadata from FastDFS protocol format.
     */
    public static Map<String, String> decodeMetadata(byte[] data) {
        Map<String, String> metadata = new HashMap<>();
        
        if (data == null || data.length == 0) {
            return metadata;
        }
        
        try {
            String metaStr = new String(data, CHARSET);
            String[] records = metaStr.split(String.valueOf((char) ProtocolConstants.FDFS_RECORD_SEPARATOR));
            
            for (String record : records) {
                if (record.isEmpty()) {
                    continue;
                }
                
                String[] fields = record.split(String.valueOf((char) ProtocolConstants.FDFS_FIELD_SEPARATOR), 2);
                if (fields.length == 2) {
                    metadata.put(fields[0], fields[1]);
                }
            }
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException("UTF-8 encoding not supported", e);
        }
        
        return metadata;
    }
    
    /**
     * Extracts file extension from filename.
     */
    public static String getFileExtension(String filename) {
        if (filename == null || filename.isEmpty()) {
            return "";
        }
        
        int dotIndex = filename.lastIndexOf('.');
        if (dotIndex > 0 && dotIndex < filename.length() - 1) {
            return filename.substring(dotIndex + 1);
        }
        
        return "";
    }
    
    /**
     * Encodes a long value to 8 bytes (big-endian).
     */
    public static byte[] encodeLong(long value) {
        ByteBuffer buffer = ByteBuffer.allocate(8);
        buffer.putLong(value);
        return buffer.array();
    }
    
    /**
     * Decodes 8 bytes to a long value (big-endian).
     */
    public static long decodeLong(byte[] data, int offset) {
        ByteBuffer buffer = ByteBuffer.wrap(data, offset, 8);
        return buffer.getLong();
    }
}
