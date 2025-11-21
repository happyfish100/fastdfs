/**
 * Client Configuration Unit Tests
 * 
 * This file contains unit tests for the ClientConfig class.
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.config

import spock.lang.*

/**
 * Unit tests for ClientConfig.
 */
class ClientConfigTest extends Specification {
    
    /**
     * Test: Default configuration.
     */
    def "test default configuration"() {
        when:
        def config = new ClientConfig()
        
        then:
        config.maxConns == 10
        config.connectTimeout == 5000L
        config.networkTimeout == 30000L
        config.idleTimeout == 60000L
        config.retryCount == 3
        config.enablePool == true
    }
    
    /**
     * Test: Configuration with custom values.
     */
    def "test configuration with custom values"() {
        when:
        def config = new ClientConfig(
            trackerAddrs: ['192.168.1.100:22122'],
            maxConns: 100,
            connectTimeout: 10000L,
            networkTimeout: 60000L
        )
        
        then:
        config.trackerAddrs == ['192.168.1.100:22122']
        config.maxConns == 100
        config.connectTimeout == 10000L
        config.networkTimeout == 60000L
    }
    
    /**
     * Test: Copy constructor.
     */
    def "test copy constructor"() {
        given:
        def original = new ClientConfig(
            trackerAddrs: ['192.168.1.100:22122'],
            maxConns: 50
        )
        
        when:
        def copy = new ClientConfig(original)
        
        then:
        copy.trackerAddrs == original.trackerAddrs
        copy.maxConns == original.maxConns
        copy != original // Different objects
    }
    
    /**
     * Test: Copy constructor with null.
     */
    def "test copy constructor with null"() {
        when:
        new ClientConfig(null)
        
        then:
        thrown(IllegalArgumentException)
    }
    
    /**
     * Test: Fluent API.
     */
    def "test fluent API"() {
        when:
        def config = new ClientConfig()
            .trackerAddrs('192.168.1.100:22122', '192.168.1.101:22122')
            .maxConns(100)
            .connectTimeout(5000L)
            .networkTimeout(30000L)
        
        then:
        config.trackerAddrs.size() == 2
        config.maxConns == 100
        config.connectTimeout == 5000L
        config.networkTimeout == 30000L
    }
    
    /**
     * Test: Validation with valid configuration.
     */
    def "test validation with valid configuration"() {
        given:
        def config = new ClientConfig(
            trackerAddrs: ['192.168.1.100:22122']
        )
        
        when:
        config.validate()
        
        then:
        noExceptionThrown()
    }
    
    /**
     * Test: Validation with null tracker addresses.
     */
    def "test validation with null tracker addresses"() {
        given:
        def config = new ClientConfig(trackerAddrs: null)
        
        when:
        config.validate()
        
        then:
        thrown(IllegalArgumentException)
    }
    
    /**
     * Test: Validation with empty tracker addresses.
     */
    def "test validation with empty tracker addresses"() {
        given:
        def config = new ClientConfig(trackerAddrs: [])
        
        when:
        config.validate()
        
        then:
        thrown(IllegalArgumentException)
    }
    
    /**
     * Test: Validation with invalid max connections.
     */
    def "test validation with invalid max connections"() {
        given:
        def config = new ClientConfig(
            trackerAddrs: ['192.168.1.100:22122'],
            maxConns: 0
        )
        
        when:
        config.validate()
        
        then:
        thrown(IllegalArgumentException)
    }
    
    /**
     * Test: Validation with invalid timeout.
     */
    def "test validation with invalid timeout"() {
        given:
        def config = new ClientConfig(
            trackerAddrs: ['192.168.1.100:22122'],
            connectTimeout: 500L // Too small
        )
        
        when:
        config.validate()
        
        then:
        thrown(IllegalArgumentException)
    }
}

