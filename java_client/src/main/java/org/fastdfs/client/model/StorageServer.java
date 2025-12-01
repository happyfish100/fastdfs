package org.fastdfs.client.model;

/**
 * Represents a storage server in the FastDFS cluster.
 */
public class StorageServer {
    
    private String ipAddr;
    private int port;
    private int storePathIndex;
    
    public StorageServer() {
    }
    
    public StorageServer(String ipAddr, int port, int storePathIndex) {
        this.ipAddr = ipAddr;
        this.port = port;
        this.storePathIndex = storePathIndex;
    }
    
    public String getIpAddr() {
        return ipAddr;
    }
    
    public void setIpAddr(String ipAddr) {
        this.ipAddr = ipAddr;
    }
    
    public int getPort() {
        return port;
    }
    
    public void setPort(int port) {
        this.port = port;
    }
    
    public int getStorePathIndex() {
        return storePathIndex;
    }
    
    public void setStorePathIndex(int storePathIndex) {
        this.storePathIndex = storePathIndex;
    }
    
    public String getAddress() {
        return ipAddr + ":" + port;
    }
    
    @Override
    public String toString() {
        return "StorageServer{" +
                "ipAddr='" + ipAddr + '\'' +
                ", port=" + port +
                ", storePathIndex=" + storePathIndex +
                '}';
    }
}
