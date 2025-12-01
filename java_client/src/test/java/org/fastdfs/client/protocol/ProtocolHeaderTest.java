package org.fastdfs.client.protocol;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for ProtocolHeader.
 */
class ProtocolHeaderTest {
    
    @Test
    void testEncodeDecodeHeader() {
        ProtocolHeader header = new ProtocolHeader(1024, (byte) 11, (byte) 0);
        
        byte[] encoded = header.encode();
        assertEquals(ProtocolConstants.FDFS_PROTO_HEADER_LEN, encoded.length);
        
        ProtocolHeader decoded = ProtocolHeader.decode(encoded);
        
        assertEquals(header.getLength(), decoded.getLength());
        assertEquals(header.getCmd(), decoded.getCmd());
        assertEquals(header.getStatus(), decoded.getStatus());
    }
    
    @Test
    void testDecodeHeader_InvalidData() {
        assertThrows(IllegalArgumentException.class, () -> {
            ProtocolHeader.decode(new byte[5]); // Too short
        });
        
        assertThrows(IllegalArgumentException.class, () -> {
            ProtocolHeader.decode(null);
        });
    }
    
    @Test
    void testHeaderToString() {
        ProtocolHeader header = new ProtocolHeader(1024, (byte) 11, (byte) 0);
        String str = header.toString();
        
        assertNotNull(str);
        assertTrue(str.contains("1024"));
        assertTrue(str.contains("11"));
        assertTrue(str.contains("0"));
    }
}
