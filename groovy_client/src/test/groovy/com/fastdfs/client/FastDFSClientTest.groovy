/**
 * FastDFS Client Unit Tests
 * 
 * This file contains unit tests for the FastDFSClient class.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client

import com.fastdfs.client.config.ClientConfig
import com.fastdfs.client.errors.*
import spock.lang.*

/**
 * Unit tests for FastDFSClient.
 */
class FastDFSClientTest extends Specification {
    
    /**
     * Test client configuration.
     */
    def config = new ClientConfig(
        trackerAddrs: ['127.0.0.1:22122'],
        maxConns: 10,
        connectTimeout: 5000,
        networkTimeout: 30000
    )
    
    /**
     * Test: Client creation with valid configuration.
     */
    def "test client creation with valid configuration"() {
        when:
        def client = new FastDFSClient(config)
        
        then:
        client != null
        
        cleanup:
        client?.close()
    }
    
    /**
     * Test: Client creation with null configuration.
     */
    def "test client creation with null configuration"() {
        when:
        new FastDFSClient(null)
        
        then:
        thrown(IllegalArgumentException)
    }
    
    /**
     * Test: Client creation with empty tracker addresses.
     */
    def "test client creation with empty tracker addresses"() {
        given:
        def invalidConfig = new ClientConfig(trackerAddrs: [])
        
        when:
        new FastDFSClient(invalidConfig)
        
        then:
        thrown(IllegalArgumentException)
    }
    
    /**
     * Test: Client close.
     */
    def "test client close"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.close()
        
        then:
        noExceptionThrown()
        
        when:
        client.close() // Second close should be idempotent
        
        then:
        noExceptionThrown()
    }
    
    /**
     * Test: Operations on closed client.
     */
    def "test operations on closed client"() {
        given:
        def client = new FastDFSClient(config)
        client.close()
        
        when:
        client.uploadFile('test.txt', [:])
        
        then:
        thrown(IllegalStateException)
    }
    
    /**
     * Test: Upload file with null filename.
     */
    def "test upload file with null filename"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.uploadFile(null, [:])
        
        then:
        thrown(IllegalArgumentException)
        
        cleanup:
        client?.close()
    }
    
    /**
     * Test: Upload buffer with null data.
     */
    def "test upload buffer with null data"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.uploadBuffer(null, 'txt', [:])
        
        then:
        thrown(IllegalArgumentException)
        
        cleanup:
        client?.close()
    }
    
    /**
     * Test: Download file with null file ID.
     */
    def "test download file with null file ID"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.downloadFile(null)
        
        then:
        thrown(IllegalArgumentException)
        
        cleanup:
        client?.close()
    }
    
    /**
     * Test: Delete file with null file ID.
     */
    def "test delete file with null file ID"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.deleteFile(null)
        
        then:
        thrown(IllegalArgumentException)
        
        cleanup:
        client?.close()
    }
    
    /**
     * Test: Set metadata with null file ID.
     */
    def "test set metadata with null file ID"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.setMetadata(null, [:], MetadataFlag.OVERWRITE)
        
        then:
        thrown(IllegalArgumentException)
        
        cleanup:
        client?.close()
    }
    
    /**
     * Test: Get metadata with null file ID.
     */
    def "test get metadata with null file ID"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.getMetadata(null)
        
        then:
        thrown(IllegalArgumentException)
        
        cleanup:
        client?.close()
    }
    
    /**
     * Test: File exists with null file ID.
     */
    def "test file exists with null file ID"() {
        given:
        def client = new FastDFSClient(config)
        
        when:
        client.fileExists(null)
        
        then:
        thrown(IllegalArgumentException)
        
        cleanup:
        client?.close()
    }
}

