package org.fastdfs.client.model;

import org.fastdfs.client.protocol.ProtocolConstants;

/**
 * Metadata operation flags.
 */
public enum MetadataFlag {
    
    /**
     * Overwrite existing metadata.
     */
    OVERWRITE(ProtocolConstants.METADATA_FLAG_OVERWRITE),
    
    /**
     * Merge with existing metadata.
     */
    MERGE(ProtocolConstants.METADATA_FLAG_MERGE);
    
    private final byte value;
    
    MetadataFlag(byte value) {
        this.value = value;
    }
    
    public byte getValue() {
        return value;
    }
}
