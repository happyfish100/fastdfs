/**
 * MetadataFlag - Metadata Operation Flag
 * 
 * This enum defines the flags that control how metadata is updated when
 * setting metadata for a file. The flag determines whether new metadata
 * replaces or merges with existing metadata.
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

/**
 * MetadataFlag specifies how metadata should be updated.
 * 
 * This enum controls the behavior of the setMetadata operation, determining
 * whether new metadata completely replaces existing metadata or merges with it.
 * 
 * The FastDFS protocol uses single-byte flags ('O' for overwrite, 'M' for merge)
 * to specify the operation mode. This enum provides a type-safe way to specify
 * these flags in Java code.
 * 
 * Example usage:
 * <pre>
 * // Replace all existing metadata
 * client.setMetadata(fileId, newMetadata, MetadataFlag.OVERWRITE);
 * 
 * // Merge with existing metadata
 * client.setMetadata(fileId, newMetadata, MetadataFlag.MERGE);
 * </pre>
 */
public enum MetadataFlag {
    
    /**
     * Overwrite mode - completely replaces all existing metadata.
     * 
     * When using OVERWRITE, all existing metadata keys are removed and
     * replaced with the new metadata. Any keys that existed before but
     * are not in the new metadata will be deleted.
     * 
     * This is the default mode for most use cases where you want to
     * completely replace the metadata with a new set of values.
     * 
     * Protocol value: 'O' (0x4F)
     */
    OVERWRITE('O'),
    
    /**
     * Merge mode - merges new metadata with existing metadata.
     * 
     * When using MERGE, new metadata keys are added and existing keys
     * are updated with new values. Keys that are not in the new metadata
     * are preserved with their existing values.
     * 
     * This mode is useful when you want to update only specific metadata
     * fields without affecting other existing metadata.
     * 
     * Protocol value: 'M' (0x4D)
     */
    MERGE('M');
    
    // ============================================================================
    // Private Fields
    // ============================================================================
    
    /**
     * Protocol byte value for this flag.
     * 
     * This is the actual byte value that is sent in the FastDFS protocol
     * message to specify the metadata operation mode.
     */
    private final byte protocolValue;
    
    // ============================================================================
    // Constructors
    // ============================================================================
    
    /**
     * Creates a new MetadataFlag enum value with the specified protocol byte.
     * 
     * @param protocolValue the protocol byte value ('O' or 'M')
     */
    MetadataFlag(char protocolValue) {
        this.protocolValue = (byte) protocolValue;
    }
    
    // ============================================================================
    // Public Methods
    // ============================================================================
    
    /**
     * Gets the protocol byte value for this flag.
     * 
     * This value is used when encoding the metadata update request in the
     * FastDFS protocol format.
     * 
     * @return the protocol byte value
     */
    public byte getProtocolValue() {
        return protocolValue;
    }
    
    /**
     * Converts a protocol byte value to a MetadataFlag enum value.
     * 
     * This method is used when decoding FastDFS protocol responses or
     * when converting from protocol-level values to Java enum values.
     * 
     * @param protocolValue the protocol byte value
     * @return the corresponding MetadataFlag, or null if the value is invalid
     */
    public static MetadataFlag fromProtocolValue(byte protocolValue) {
        for (MetadataFlag flag : values()) {
            if (flag.protocolValue == protocolValue) {
                return flag;
            }
        }
        return null;
    }
    
    /**
     * Converts a character to a MetadataFlag enum value.
     * 
     * This is a convenience method that accepts a char and converts it
     * to the corresponding MetadataFlag.
     * 
     * @param c the character ('O' or 'M')
     * @return the corresponding MetadataFlag, or null if the character is invalid
     */
    public static MetadataFlag fromChar(char c) {
        return fromProtocolValue((byte) c);
    }
}

