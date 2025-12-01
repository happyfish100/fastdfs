package org.fastdfs.client.model;

import java.util.Date;

/**
 * Information about a file stored in FastDFS.
 */
public class FileInfo {
    
    private long fileSize;
    private Date createTime;
    private long crc32;
    private String sourceIpAddr;
    
    public FileInfo() {
    }
    
    public FileInfo(long fileSize, Date createTime, long crc32, String sourceIpAddr) {
        this.fileSize = fileSize;
        this.createTime = createTime;
        this.crc32 = crc32;
        this.sourceIpAddr = sourceIpAddr;
    }
    
    public long getFileSize() {
        return fileSize;
    }
    
    public void setFileSize(long fileSize) {
        this.fileSize = fileSize;
    }
    
    public Date getCreateTime() {
        return createTime;
    }
    
    public void setCreateTime(Date createTime) {
        this.createTime = createTime;
    }
    
    public long getCrc32() {
        return crc32;
    }
    
    public void setCrc32(long crc32) {
        this.crc32 = crc32;
    }
    
    public String getSourceIpAddr() {
        return sourceIpAddr;
    }
    
    public void setSourceIpAddr(String sourceIpAddr) {
        this.sourceIpAddr = sourceIpAddr;
    }
    
    @Override
    public String toString() {
        return "FileInfo{" +
                "fileSize=" + fileSize +
                ", createTime=" + createTime +
                ", crc32=" + crc32 +
                ", sourceIpAddr='" + sourceIpAddr + '\'' +
                '}';
    }
}
