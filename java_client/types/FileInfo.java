/**
 * FileInfo - File Information Data Structure
 * 
 * This class represents detailed information about a file stored in FastDFS.
 * The information is retrieved from the storage server and includes file size,
 * creation time, CRC32 checksum, and source server information.
 * 
 * Copyright (C) 2025 FastDFS Java Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * 
 * @author FastDFS Java Client Contributors
 * @version 1.0.0
 * @since 1.0.0
 */
package com.fastdfs.client.types;

import java.util.Date;

/**
 * FileInfo contains detailed information about a file stored in FastDFS.
 * 
 * This information is returned by the getFileInfo() method and includes:
 * - File size in bytes
 * - Creation timestamp
 * - CRC32 checksum for integrity verification
 * - Source storage server IP address
 * 
 * All fields are immutable once the object is created. This ensures that
 * file information remains consistent throughout its lifetime.
 * 
 * Example usage:
 * <pre>
 * FileInfo info = client.getFileInfo(fileId);
 * System.out.println("File size: " + info.getFileSize() + " bytes");
 * System.out.println("Created: " + info.getCreateTime());
 * System.out.println("CRC32: " + Long.toHexString(info.getCrc32()));
 * </pre>
 */
public class FileInfo {
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * File size in bytes.
     * 
     * This is the actual size of the file content stored on the storage server.
     * It does not include any overhead from the FastDFS storage format.
     */
    private final long fileSize;
    
    /**
     * Creation timestamp.
     * 
     * This is the time when the file was created on the storage server.
     * The timestamp is in milliseconds since the Unix epoch (January 1, 1970).
     */
    private final Date createTime;
    
    /**
     * CRC32 checksum.
     * 
     * This is a 32-bit cyclic redundancy check value computed over the
     * file content. It can be used to verify file integrity after download.
     * 
     * Note: The CRC32 is stored as an unsigned 32-bit integer, but Java's
     * long type is used to avoid sign issues. The value should be interpreted
     * as an unsigned integer when comparing with other CRC32 values.
     */
    private final long crc32;
    
    /**
     * Source storage server IP address.
     * 
     * This is the IP address of the storage server where the file was
     * originally uploaded. It may differ from the current storage server
     * if the file has been replicated to other servers in the group.
     */
    private final String sourceIpAddr;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new FileInfo object with the specified values.
     * 
     * All parameters are required and cannot be null (except sourceIpAddr
     * which can be null if not available).
     * 
     * @param fileSize the file size in bytes (must be >= 0)
     * @param createTime the creation timestamp (must not be null)
     * @param crc32 the CRC32 checksum (as unsigned 32-bit integer)
     * @param sourceIpAddr the source storage server IP address (can be null)
     * @throws IllegalArgumentException if fileSize is negative or createTime is null
     */
    public FileInfo(long fileSize, Date createTime, long crc32, String sourceIpAddr) {
        // Validate file size
        if (fileSize < 0) {
            throw new IllegalArgumentException("File size cannot be negative");
        }
        
        // Validate creation time
        if (createTime == null) {
            throw new IllegalArgumentException("Create time cannot be null");
        }
        
        // Store values
        this.fileSize = fileSize;
        this.createTime = new Date(createTime.getTime()); // Defensive copy
        this.crc32 = crc32;
        this.sourceIpAddr = sourceIpAddr;
    }
    
    // ============================================================================
    // Getters
    // ============================================================================
    
    /**
     * Gets the file size in bytes.
     * 
     * @return the file size in bytes (always >= 0)
     */
    public long getFileSize() {
        return fileSize;
    }
    
    /**
     * Gets the creation timestamp.
     * 
     * The returned Date object is a defensive copy, so modifications to it
     * will not affect the internal state of this FileInfo object.
     * 
     * @return the creation timestamp (never null)
     */
    public Date getCreateTime() {
        return new Date(createTime.getTime()); // Defensive copy
    }
    
    /**
     * Gets the CRC32 checksum.
     * 
     * The CRC32 is returned as a long to avoid sign issues. It should be
     * interpreted as an unsigned 32-bit integer. To convert to a hex string:
     * <pre>
     * String hex = Long.toHexString(fileInfo.getCrc32());
     * </pre>
     * 
     * @return the CRC32 checksum as an unsigned 32-bit integer
     */
    public long getCrc32() {
        return crc32;
    }
    
    /**
     * Gets the source storage server IP address.
     * 
     * This may be null if the source IP address is not available or
     * was not provided by the storage server.
     * 
     * @return the source storage server IP address, or null if not available
     */
    public String getSourceIpAddr() {
        return sourceIpAddr;
    }
    
    // ============================================================================
    // Object Methods
    // ============================================================================
    
    /**
     * Returns a string representation of this FileInfo object.
     * 
     * The string includes all fields in a human-readable format, which is
     * useful for debugging and logging purposes.
     * 
     * @return a string representation of this FileInfo
     */
    @Override
    public String toString() {
        return "FileInfo{" +
                "fileSize=" + fileSize +
                ", createTime=" + createTime +
                ", crc32=0x" + Long.toHexString(crc32) +
                ", sourceIpAddr='" + sourceIpAddr + '\'' +
                '}';
    }
    
    /**
     * Compares this FileInfo with another object for equality.
     * 
     * Two FileInfo objects are considered equal if all their fields are equal.
     * The comparison includes file size, creation time, CRC32, and source IP address.
     * 
     * @param obj the object to compare with
     * @return true if the objects are equal, false otherwise
     */
    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        
        FileInfo fileInfo = (FileInfo) obj;
        
        if (fileSize != fileInfo.fileSize) {
            return false;
        }
        if (crc32 != fileInfo.crc32) {
            return false;
        }
        if (!createTime.equals(fileInfo.createTime)) {
            return false;
        }
        if (sourceIpAddr != null ? !sourceIpAddr.equals(fileInfo.sourceIpAddr) 
                                 : fileInfo.sourceIpAddr != null) {
            return false;
        }
        
        return true;
    }
    
    /**
     * Returns a hash code for this FileInfo object.
     * 
     * The hash code is computed from all fields, ensuring that equal objects
     * have equal hash codes (as required by the hashCode contract).
     * 
     * @return a hash code for this FileInfo
     */
    @Override
    public int hashCode() {
        int result = (int) (fileSize ^ (fileSize >>> 32));
        result = 31 * result + createTime.hashCode();
        result = 31 * result + (int) (crc32 ^ (crc32 >>> 32));
        result = 31 * result + (sourceIpAddr != null ? sourceIpAddr.hashCode() : 0);
        return result;
    }
}

