/**
 * FastDFS Groovy Client - Basic Example
 * 
 * This example demonstrates basic file operations:
 * - Upload a file from the filesystem
 * - Upload a file from a byte array
 * - Download a file to memory
 * - Download a file to the filesystem
 * - Delete a file
 * 
 * @author FastDFS Groovy Client Contributors
 * @version 1.0.0
 */
package com.fastdfs.client.examples

import com.fastdfs.client.FastDFSClient
import com.fastdfs.client.config.ClientConfig
import com.fastdfs.client.errors.*

/**
 * Basic example demonstrating file upload, download, and deletion.
 */
class BasicExample {
    
    /**
     * Main method.
     * 
     * @param args command line arguments
     */
    static void main(String[] args) {
        // Create client configuration
        // Replace with your FastDFS tracker addresses
        def config = new ClientConfig(
            trackerAddrs: ['192.168.1.100:22122', '192.168.1.101:22122'],
            maxConns: 100,
            connectTimeout: 5000,
            networkTimeout: 30000,
            retryCount: 3
        )
        
        // Initialize client
        def client = new FastDFSClient(config)
        
        try {
            println "=== FastDFS Groovy Client - Basic Example ==="
            println ""
            
            // Example 1: Upload a file from filesystem
            println "Example 1: Upload file from filesystem"
            try {
                def localFile = 'test.txt'
                
                // Create a test file if it doesn't exist
                def testFile = new File(localFile)
                if (!testFile.exists()) {
                    testFile.write("Hello, FastDFS! This is a test file.\n")
                    println "Created test file: ${localFile}"
                }
                
                // Upload the file
                def fileId = client.uploadFile(localFile, [:])
                println "File uploaded successfully!"
                println "File ID: ${fileId}"
                println ""
                
                // Example 2: Download the file to memory
                println "Example 2: Download file to memory"
                def data = client.downloadFile(fileId)
                println "Downloaded ${data.length} bytes"
                println "Content: ${new String(data)}"
                println ""
                
                // Example 3: Download the file to filesystem
                println "Example 3: Download file to filesystem"
                def downloadedFile = 'downloaded_test.txt'
                client.downloadToFile(fileId, downloadedFile)
                println "File downloaded to: ${downloadedFile}"
                println ""
                
                // Example 4: Upload from byte array
                println "Example 4: Upload from byte array"
                def byteData = "This is uploaded from a byte array!".bytes
                def byteFileId = client.uploadBuffer(byteData, 'txt', [:])
                println "Byte array uploaded successfully!"
                println "File ID: ${byteFileId}"
                println ""
                
                // Example 5: Download partial file (range)
                println "Example 5: Download partial file (range)"
                def partialData = client.downloadFileRange(byteFileId, 0, 10)
                println "Downloaded first 10 bytes: ${new String(partialData)}"
                println ""
                
                // Example 6: Check if file exists
                println "Example 6: Check if file exists"
                def exists = client.fileExists(fileId)
                println "File exists: ${exists}"
                println ""
                
                // Example 7: Get file information
                println "Example 7: Get file information"
                def fileInfo = client.getFileInfo(fileId)
                println "File size: ${fileInfo.fileSize} bytes"
                println "Create time: ${fileInfo.createTime}"
                println "CRC32: ${fileInfo.crc32}"
                println "Source IP: ${fileInfo.sourceIPAddr}"
                println ""
                
                // Example 8: Delete files
                println "Example 8: Delete files"
                client.deleteFile(fileId)
                println "First file deleted"
                
                client.deleteFile(byteFileId)
                println "Second file deleted"
                println ""
                
                println "=== All examples completed successfully! ==="
                
            } catch (FileNotFoundException e) {
                println "Error: File not found - ${e.message}"
            } catch (NoStorageServerException e) {
                println "Error: No storage server available - ${e.message}"
            } catch (ConnectionTimeoutException e) {
                println "Error: Connection timeout - ${e.message}"
            } catch (NetworkTimeoutException e) {
                println "Error: Network timeout - ${e.message}"
            } catch (FastDFSException e) {
                println "Error: FastDFS error - ${e.message}"
                e.printStackTrace()
            } catch (Exception e) {
                println "Error: Unexpected error - ${e.message}"
                e.printStackTrace()
            }
            
        } finally {
            // Always close the client
            client.close()
            println ""
            println "Client closed"
        }
    }
}

