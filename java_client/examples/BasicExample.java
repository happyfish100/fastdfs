/**
 * BasicExample - Basic FastDFS Client Usage Example
 * 
 * This example demonstrates basic usage of the FastDFS Java client,
 * including file upload, download, and deletion operations.
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

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * BasicExample demonstrates basic FastDFS operations.
 * 
 * This example shows how to:
 * - Create a FastDFS client with configuration
 * - Upload a file from the local filesystem
 * - Upload file data from a byte array
 * - Download a file
 * - Delete a file
 * - Handle errors
 * 
 * To run this example:
 * 1. Make sure FastDFS tracker and storage servers are running
 * 2. Update the tracker server addresses in the configuration
 * 3. Compile and run this class
 */
public class BasicExample {
    
    /**
     * Main method - Entry point for the example.
     * 
     * @param args command line arguments (not used)
     */
    public static void main(String[] args) {
        // Create client configuration
        // Replace with your actual tracker server addresses
        FastDFSConfig config = new FastDFSConfig.Builder()
            .addTrackerServer("192.168.1.100", 22122)
            .addTrackerServer("192.168.1.101", 22122)  // Optional: add more for high availability
            .maxConnectionsPerServer(100)                // Maximum connections per server
            .connectTimeout(5000)                        // Connection timeout: 5 seconds
            .networkTimeout(30000)                       // Network I/O timeout: 30 seconds
            .idleTimeout(60000)                          // Idle connection timeout: 60 seconds
            .retryCount(3)                               // Retry failed operations up to 3 times
            .build();
        
        // Initialize client
        // The client should be created once and reused throughout the application
        FastDFSClient client = null;
        try {
            client = new FastDFSClient(config);
            
            // Example 1: Upload a file from the local filesystem
            System.out.println("Example 1: Upload file from filesystem");
            String fileId1 = uploadFileExample(client);
            System.out.println("File uploaded with ID: " + fileId1);
            
            // Example 2: Upload file data from a byte array
            System.out.println("\nExample 2: Upload file from byte array");
            String fileId2 = uploadBufferExample(client);
            System.out.println("File uploaded with ID: " + fileId2);
            
            // Example 3: Download a file
            System.out.println("\nExample 3: Download file");
            downloadFileExample(client, fileId2);
            
            // Example 4: Delete a file
            System.out.println("\nExample 4: Delete file");
            deleteFileExample(client, fileId1);
            deleteFileExample(client, fileId2);
            
            System.out.println("\nAll examples completed successfully!");
            
        } catch (FastDFSException e) {
            // Handle FastDFS-specific errors
            System.err.println("FastDFS error: " + e.getError());
            System.err.println("Message: " + e.getMessage());
            if (e.getCause() != null) {
                System.err.println("Cause: " + e.getCause().getMessage());
            }
            e.printStackTrace();
        } catch (IOException e) {
            // Handle I/O errors
            System.err.println("I/O error: " + e.getMessage());
            e.printStackTrace();
        } catch (Exception e) {
            // Handle other errors
            System.err.println("Unexpected error: " + e.getMessage());
            e.printStackTrace();
        } finally {
            // Always close the client to release resources
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
     * Example: Upload a file from the local filesystem.
     * 
     * @param client the FastDFS client
     * @return the file ID of the uploaded file
     * @throws FastDFSException if the upload fails
     * @throws IOException if the file cannot be read
     */
    private static String uploadFileExample(FastDFSClient client) 
            throws FastDFSException, IOException {
        // Upload a file from the local filesystem
        // The file extension is automatically extracted from the filename
        String localFilePath = "test.jpg";  // Replace with your file path
        
        // Optional: Add metadata to the file
        Map<String, String> metadata = new HashMap<>();
        metadata.put("author", "John Doe");
        metadata.put("description", "Test image file");
        metadata.put("uploaded-by", "BasicExample");
        
        // Upload the file
        String fileId = client.uploadFile(localFilePath, metadata);
        
        return fileId;
    }
    
    /**
     * Example: Upload file data from a byte array.
     * 
     * @param client the FastDFS client
     * @return the file ID of the uploaded file
     * @throws FastDFSException if the upload fails
     */
    private static String uploadBufferExample(FastDFSClient client) 
            throws FastDFSException {
        // Create some test data
        String content = "Hello, FastDFS! This is a test file.";
        byte[] data = content.getBytes();
        
        // Upload the data
        // The file extension must be provided explicitly
        String fileId = client.uploadBuffer(data, "txt", null);
        
        return fileId;
    }
    
    /**
     * Example: Download a file.
     * 
     * @param client the FastDFS client
     * @param fileId the file ID to download
     * @throws FastDFSException if the download fails
     */
    private static void downloadFileExample(FastDFSClient client, String fileId) 
            throws FastDFSException {
        // Download the entire file
        byte[] data = client.downloadFile(fileId);
        
        System.out.println("Downloaded " + data.length + " bytes");
        System.out.println("Content: " + new String(data));
        
        // Alternatively, download to a local file
        // client.downloadToFile(fileId, "downloaded_file.txt");
    }
    
    /**
     * Example: Delete a file.
     * 
     * @param client the FastDFS client
     * @param fileId the file ID to delete
     * @throws FastDFSException if the deletion fails
     */
    private static void deleteFileExample(FastDFSClient client, String fileId) 
            throws FastDFSException {
        // Delete the file
        client.deleteFile(fileId);
        System.out.println("File deleted: " + fileId);
    }
}

