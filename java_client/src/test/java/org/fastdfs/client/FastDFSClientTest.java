package org.fastdfs.client;

import org.fastdfs.client.config.ClientConfig;
import org.fastdfs.client.exception.InvalidArgumentException;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for FastDFSClient.
 */
class FastDFSClientTest {
    
    @Test
    void testConfigValidation_NullConfig() {
        assertThrows(InvalidArgumentException.class, () -> {
            new FastDFSClient(null);
        });
    }
    
    @Test
    void testConfigValidation_EmptyTrackerAddrs() {
        ClientConfig config = new ClientConfig();
        assertThrows(InvalidArgumentException.class, () -> {
            new FastDFSClient(config);
        });
    }
    
    @Test
    void testConfigValidation_InvalidTrackerAddr() {
        ClientConfig config = new ClientConfig("invalid_address");
        assertThrows(InvalidArgumentException.class, () -> {
            new FastDFSClient(config);
        });
    }
    
    @Test
    void testConfigValidation_ValidConfig() {
        ClientConfig config = new ClientConfig("192.168.1.100:22122");
        assertDoesNotThrow(() -> {
            FastDFSClient client = new FastDFSClient(config);
            client.close();
        });
    }
    
    @Test
    void testClientClose_MultipleCloses() {
        ClientConfig config = new ClientConfig("192.168.1.100:22122");
        assertDoesNotThrow(() -> {
            FastDFSClient client = new FastDFSClient(config);
            client.close();
            client.close(); // Should not throw
        });
    }
}
