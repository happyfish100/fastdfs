package org.fastdfs.client.protocol;

import org.fastdfs.client.exception.InvalidFileIdException;
import org.junit.jupiter.api.Test;

import java.util.HashMap;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for ProtocolUtil.
 */
class ProtocolUtilTest {
    
    @Test
    void testParseFileId_Valid() throws InvalidFileIdException {
        String fileId = "group1/M00/00/00/test.jpg";
        String[] parts = ProtocolUtil.parseFileId(fileId);
        
        assertEquals(2, parts.length);
        assertEquals("group1", parts[0]);
        assertEquals("M00/00/00/test.jpg", parts[1]);
    }
    
    @Test
    void testParseFileId_Invalid_NoSlash() {
        assertThrows(InvalidFileIdException.class, () -> {
            ProtocolUtil.parseFileId("invalid_file_id");
        });
    }
    
    @Test
    void testParseFileId_Invalid_Empty() {
        assertThrows(InvalidFileIdException.class, () -> {
            ProtocolUtil.parseFileId("");
        });
    }
    
    @Test
    void testParseFileId_Invalid_Null() {
        assertThrows(InvalidFileIdException.class, () -> {
            ProtocolUtil.parseFileId(null);
        });
    }
    
    @Test
    void testGetFileExtension() {
        assertEquals("jpg", ProtocolUtil.getFileExtension("test.jpg"));
        assertEquals("txt", ProtocolUtil.getFileExtension("document.txt"));
        assertEquals("", ProtocolUtil.getFileExtension("noextension"));
        assertEquals("", ProtocolUtil.getFileExtension(""));
        assertEquals("", ProtocolUtil.getFileExtension(null));
    }
    
    @Test
    void testEncodeDecodeFixedString() {
        String original = "test";
        byte[] encoded = ProtocolUtil.encodeFixedString(original, 16);
        
        assertEquals(16, encoded.length);
        
        String decoded = ProtocolUtil.decodeFixedString(encoded);
        assertEquals(original, decoded);
    }
    
    @Test
    void testEncodeDecodeMetadata() {
        Map<String, String> metadata = new HashMap<>();
        metadata.put("author", "John Doe");
        metadata.put("date", "2025-01-01");
        metadata.put("version", "1.0");
        
        byte[] encoded = ProtocolUtil.encodeMetadata(metadata);
        Map<String, String> decoded = ProtocolUtil.decodeMetadata(encoded);
        
        assertEquals(metadata.size(), decoded.size());
        assertEquals(metadata.get("author"), decoded.get("author"));
        assertEquals(metadata.get("date"), decoded.get("date"));
        assertEquals(metadata.get("version"), decoded.get("version"));
    }
    
    @Test
    void testEncodeDecodeMetadata_Empty() {
        Map<String, String> metadata = new HashMap<>();
        byte[] encoded = ProtocolUtil.encodeMetadata(metadata);
        
        assertEquals(0, encoded.length);
        
        Map<String, String> decoded = ProtocolUtil.decodeMetadata(encoded);
        assertTrue(decoded.isEmpty());
    }
    
    @Test
    void testEncodeLong() {
        long value = 123456789L;
        byte[] encoded = ProtocolUtil.encodeLong(value);
        
        assertEquals(8, encoded.length);
        
        long decoded = ProtocolUtil.decodeLong(encoded, 0);
        assertEquals(value, decoded);
    }
}
