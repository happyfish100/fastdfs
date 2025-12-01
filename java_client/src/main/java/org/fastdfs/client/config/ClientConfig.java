package org.fastdfs.client.config;

import java.util.Arrays;
import java.util.List;

/**
 * Client configuration options.
 */
public class ClientConfig {
    
    private List<String> trackerAddrs;
    private int maxConns = 10;
    private int connectTimeout = 5000;
    private int networkTimeout = 30000;
    private int idleTimeout = 60000;
    private int retryCount = 3;
    
    public ClientConfig() {
    }
    
    public ClientConfig(List<String> trackerAddrs) {
        this.trackerAddrs = trackerAddrs;
    }
    
    public ClientConfig(String... trackerAddrs) {
        this.trackerAddrs = Arrays.asList(trackerAddrs);
    }
    
    public List<String> getTrackerAddrs() {
        return trackerAddrs;
    }
    
    public void setTrackerAddrs(List<String> trackerAddrs) {
        this.trackerAddrs = trackerAddrs;
    }
    
    public int getMaxConns() {
        return maxConns;
    }
    
    /**
     * Sets the maximum number of connections per tracker server.
     * Default: 10
     */
    public void setMaxConns(int maxConns) {
        this.maxConns = maxConns;
    }
    
    public int getConnectTimeout() {
        return connectTimeout;
    }
    
    /**
     * Sets the timeout for establishing connections in milliseconds.
     * Default: 5000ms
     */
    public void setConnectTimeout(int connectTimeout) {
        this.connectTimeout = connectTimeout;
    }
    
    public int getNetworkTimeout() {
        return networkTimeout;
    }
    
    /**
     * Sets the timeout for network I/O operations in milliseconds.
     * Default: 30000ms
     */
    public void setNetworkTimeout(int networkTimeout) {
        this.networkTimeout = networkTimeout;
    }
    
    public int getIdleTimeout() {
        return idleTimeout;
    }
    
    /**
     * Sets the timeout for idle connections in the pool in milliseconds.
     * Default: 60000ms
     */
    public void setIdleTimeout(int idleTimeout) {
        this.idleTimeout = idleTimeout;
    }
    
    public int getRetryCount() {
        return retryCount;
    }
    
    /**
     * Sets the number of retries for failed operations.
     * Default: 3
     */
    public void setRetryCount(int retryCount) {
        this.retryCount = retryCount;
    }
    
    @Override
    public String toString() {
        return "ClientConfig{" +
                "trackerAddrs=" + trackerAddrs +
                ", maxConns=" + maxConns +
                ", connectTimeout=" + connectTimeout +
                ", networkTimeout=" + networkTimeout +
                ", idleTimeout=" + idleTimeout +
                ", retryCount=" + retryCount +
                '}';
    }
}
