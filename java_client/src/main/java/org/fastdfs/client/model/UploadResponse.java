package org.fastdfs.client.model;

/**
 * Response from an upload operation.
 */
public class UploadResponse {
    
    private String groupName;
    private String remoteFilename;
    
    public UploadResponse() {
    }
    
    public UploadResponse(String groupName, String remoteFilename) {
        this.groupName = groupName;
        this.remoteFilename = remoteFilename;
    }
    
    public String getGroupName() {
        return groupName;
    }
    
    public void setGroupName(String groupName) {
        this.groupName = groupName;
    }
    
    public String getRemoteFilename() {
        return remoteFilename;
    }
    
    public void setRemoteFilename(String remoteFilename) {
        this.remoteFilename = remoteFilename;
    }
    
    /**
     * Returns the full file ID in format "group/remote_filename".
     */
    public String getFileId() {
        return groupName + "/" + remoteFilename;
    }
    
    @Override
    public String toString() {
        return "UploadResponse{" +
                "groupName='" + groupName + '\'' +
                ", remoteFilename='" + remoteFilename + '\'' +
                '}';
    }
}
