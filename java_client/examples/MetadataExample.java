import org.fastdfs.client.FastDFSClient;
import org.fastdfs.client.config.ClientConfig;
import org.fastdfs.client.exception.FastDFSException;
import org.fastdfs.client.model.FileInfo;
import org.fastdfs.client.model.MetadataFlag;

import java.util.HashMap;
import java.util.Map;

/**
 * Example demonstrating metadata operations.
 */
public class MetadataExample {
    
    public static void main(String[] args) {
        ClientConfig config = new ClientConfig("192.168.1.100:22122");
        
        try (FastDFSClient client = new FastDFSClient(config)) {
            
            // Upload file with metadata
            System.out.println("Uploading file with metadata...");
            Map<String, String> metadata = new HashMap<>();
            metadata.put("author", "John Doe");
            metadata.put("title", "Sample Document");
            metadata.put("date", "2025-01-01");
            metadata.put("version", "1.0");
            
            String fileId = client.uploadFile("document.pdf", metadata);
            System.out.println("File uploaded: " + fileId);
            
            // Get metadata
            System.out.println("\nRetrieving metadata...");
            Map<String, String> retrievedMeta = client.getMetadata(fileId);
            System.out.println("Metadata:");
            for (Map.Entry<String, String> entry : retrievedMeta.entrySet()) {
                System.out.println("  " + entry.getKey() + ": " + entry.getValue());
            }
            
            // Update metadata (merge)
            System.out.println("\nMerging additional metadata...");
            Map<String, String> additionalMeta = new HashMap<>();
            additionalMeta.put("category", "Technical");
            additionalMeta.put("tags", "java,fastdfs,example");
            
            client.setMetadata(fileId, additionalMeta, MetadataFlag.MERGE);
            
            // Get updated metadata
            retrievedMeta = client.getMetadata(fileId);
            System.out.println("Updated metadata:");
            for (Map.Entry<String, String> entry : retrievedMeta.entrySet()) {
                System.out.println("  " + entry.getKey() + ": " + entry.getValue());
            }
            
            // Overwrite metadata
            System.out.println("\nOverwriting metadata...");
            Map<String, String> newMeta = new HashMap<>();
            newMeta.put("status", "archived");
            newMeta.put("archived_date", "2025-12-31");
            
            client.setMetadata(fileId, newMeta, MetadataFlag.OVERWRITE);
            
            retrievedMeta = client.getMetadata(fileId);
            System.out.println("Metadata after overwrite:");
            for (Map.Entry<String, String> entry : retrievedMeta.entrySet()) {
                System.out.println("  " + entry.getKey() + ": " + entry.getValue());
            }
            
            // Get file information
            System.out.println("\nRetrieving file information...");
            FileInfo info = client.getFileInfo(fileId);
            System.out.println("File size: " + info.getFileSize() + " bytes");
            System.out.println("Create time: " + info.getCreateTime());
            System.out.println("CRC32: " + info.getCrc32());
            System.out.println("Source IP: " + info.getSourceIpAddr());
            
            // Clean up
            System.out.println("\nDeleting file...");
            client.deleteFile(fileId);
            System.out.println("File deleted successfully");
            
        } catch (FastDFSException e) {
            System.err.println("FastDFS error: " + e.getMessage());
            e.printStackTrace();
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
