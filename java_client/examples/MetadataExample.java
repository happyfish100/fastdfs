/**
 * MetadataExample - Metadata Operations Example
 * 
 * This example demonstrates how to work with file metadata in FastDFS,
 * including setting, getting, and merging metadata.
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
package com.fastdfs.client.examples;

import com.fastdfs.client.FastDFSClient;
import com.fastdfs.client.config.FastDFSConfig;
import com.fastdfs.client.exception.FastDFSException;
import com.fastdfs.client.types.MetadataFlag;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * MetadataExample demonstrates metadata operations in FastDFS.
 * 
 * This example shows how to:
 * - Upload a file with metadata
 * - Set metadata on an existing file (overwrite mode)
 * - Set metadata on an existing file (merge mode)
 * - Get metadata from a file
 * - Work with metadata key-value pairs
 * 
 * Metadata in FastDFS is stored as key-value pairs associated with files.
 * Keys are limited to 64 characters and values to 256 characters.
 * 
 * To run this example:
 * 1. Make sure FastDFS tracker and storage servers are running
 * 2. Update the tracker server addresses in the configuration
 * 3. Compile and run this class
 */
public class MetadataExample {
    
    /**
     * Main method - Entry point for the example.
     * 
     * @param args command line arguments (not used)
     */
    public static void main(String[] args) {
        // Create client configuration
        FastDFSConfig config = new FastDFSConfig.Builder()
            .addTrackerServer("192.168.1.100", 22122)
            .maxConnectionsPerServer(100)
            .connectTimeout(5000)
            .networkTimeout(30000)
            .idleTimeout(60000)
            .retryCount(3)
            .build();
        
        // Initialize client
        FastDFSClient client = null;
        try {
            client = new FastDFSClient(config);
            
            // Example 1: Upload a file with initial metadata
            System.out.println("Example 1: Upload file with metadata");
            String fileId = uploadWithMetadataExample(client);
            System.out.println("File uploaded with ID: " + fileId);
            
            // Example 2: Get metadata from a file
            System.out.println("\nExample 2: Get metadata from file");
            getMetadataExample(client, fileId);
            
            // Example 3: Set metadata (overwrite mode)
            System.out.println("\nExample 3: Set metadata (overwrite mode)");
            setMetadataOverwriteExample(client, fileId);
            
            // Example 4: Set metadata (merge mode)
            System.out.println("\nExample 4: Set metadata (merge mode)");
            setMetadataMergeExample(client, fileId);
            
            // Example 5: Get updated metadata
            System.out.println("\nExample 5: Get updated metadata");
            getMetadataExample(client, fileId);
            
            // Clean up: Delete the file
            System.out.println("\nCleaning up: Delete file");
            client.deleteFile(fileId);
            System.out.println("File deleted: " + fileId);
            
            System.out.println("\nAll examples completed successfully!");
            
        } catch (FastDFSException e) {
            System.err.println("FastDFS error: " + e.getError());
            System.err.println("Message: " + e.getMessage());
            if (e.getCause() != null) {
                System.err.println("Cause: " + e.getCause().getMessage());
            }
            e.printStackTrace();
        } catch (IOException e) {
            System.err.println("I/O error: " + e.getMessage());
            e.printStackTrace();
        } catch (Exception e) {
            System.err.println("Unexpected error: " + e.getMessage());
            e.printStackTrace();
        } finally {
            if (client != null) {
                try {
                    client.close();
                    System.out.println("\nClient closed successfully");
                } catch (FastDFSException e) {
                    System.err.println("Error closing client: " + e.getMessage());
                }
            }
        }
    }
    
    /**
     * Example: Upload a file with initial metadata.
     * 
     * @param client the FastDFS client
     * @return the file ID of the uploaded file
     * @throws FastDFSException if the upload fails
     * @throws IOException if the file cannot be read
     */
    private static String uploadWithMetadataExample(FastDFSClient client) 
            throws FastDFSException, IOException {
        // Create metadata map
        Map<String, String> metadata = new HashMap<>();
        metadata.put("author", "John Doe");
        metadata.put("title", "Example Document");
        metadata.put("description", "This is an example file uploaded with metadata");
        metadata.put("category", "examples");
        metadata.put("version", "1.0");
        metadata.put("uploaded-by", "MetadataExample");
        metadata.put("upload-date", "2025-01-01");
        
        // Create test data
        String content = "This is a test file with metadata.";
        byte[] data = content.getBytes();
        
        // Upload file with metadata
        String fileId = client.uploadBuffer(data, "txt", metadata);
        
        return fileId;
    }
    
    /**
     * Example: Get metadata from a file.
     * 
     * @param client the FastDFS client
     * @param fileId the file ID
     * @throws FastDFSException if the operation fails
     */
    private static void getMetadataExample(FastDFSClient client, String fileId) 
            throws FastDFSException {
        // Get metadata
        Map<String, String> metadata = client.getMetadata(fileId);
        
        // Display metadata
        System.out.println("Metadata for file " + fileId + ":");
        if (metadata.isEmpty()) {
            System.out.println("  (no metadata)");
        } else {
            for (Map.Entry<String, String> entry : metadata.entrySet()) {
                System.out.println("  " + entry.getKey() + " = " + entry.getValue());
            }
        }
    }
    
    /**
     * Example: Set metadata in overwrite mode.
     * 
     * Overwrite mode replaces all existing metadata with the new metadata.
     * Any keys that existed before but are not in the new metadata will be deleted.
     * 
     * @param client the FastDFS client
     * @param fileId the file ID
     * @throws FastDFSException if the operation fails
     */
    private static void setMetadataOverwriteExample(FastDFSClient client, String fileId) 
            throws FastDFSException {
        // Create new metadata (this will replace all existing metadata)
        Map<String, String> newMetadata = new HashMap<>();
        newMetadata.put("author", "Jane Smith");
        newMetadata.put("title", "Updated Document");
        newMetadata.put("updated", "true");
        newMetadata.put("update-date", "2025-01-02");
        
        // Set metadata in overwrite mode
        client.setMetadata(fileId, newMetadata, MetadataFlag.OVERWRITE);
        
        System.out.println("Metadata overwritten for file: " + fileId);
    }
    
    /**
     * Example: Set metadata in merge mode.
     * 
     * Merge mode adds new metadata keys and updates existing keys, but keeps
     * keys that are not in the new metadata. This is useful for updating
     * specific fields without affecting others.
     * 
     * @param client the FastDFS client
     * @param fileId the file ID
     * @throws FastDFSException if the operation fails
     */
    private static void setMetadataMergeExample(FastDFSClient client, String fileId) 
            throws FastDFSException {
        // Create metadata to merge (only some fields)
        Map<String, String> mergeMetadata = new HashMap<>();
        mergeMetadata.put("author", "Bob Johnson");  // Update existing key
        mergeMetadata.put("tags", "example,test,demo");  // Add new key
        mergeMetadata.put("updated", "false");  // Update existing key
        
        // Set metadata in merge mode
        client.setMetadata(fileId, mergeMetadata, MetadataFlag.MERGE);
        
        System.out.println("Metadata merged for file: " + fileId);
    }
}

