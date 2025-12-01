import org.fastdfs.client.FastDFSClient;
import org.fastdfs.client.config.ClientConfig;
import org.fastdfs.client.exception.FastDFSException;

/**
 * Basic usage example for FastDFS Java client.
 */
public class BasicUsage {
    
    public static void main(String[] args) {
        // Configure the client
        ClientConfig config = new ClientConfig("192.168.1.100:22122");
        config.setMaxConns(50);
        config.setConnectTimeout(5000);
        config.setNetworkTimeout(30000);
        
        // Create client instance
        try (FastDFSClient client = new FastDFSClient(config)) {
            
            // Upload a file
            System.out.println("Uploading file...");
            String fileId = client.uploadFile("test.jpg", null);
            System.out.println("File uploaded successfully: " + fileId);
            
            // Check if file exists
            boolean exists = client.fileExists(fileId);
            System.out.println("File exists: " + exists);
            
            // Download the file
            System.out.println("Downloading file...");
            byte[] data = client.downloadFile(fileId);
            System.out.println("Downloaded " + data.length + " bytes");
            
            // Download to local file
            System.out.println("Downloading to local file...");
            client.downloadToFile(fileId, "downloaded_test.jpg");
            System.out.println("File saved to downloaded_test.jpg");
            
            // Upload from buffer
            System.out.println("Uploading from buffer...");
            byte[] content = "Hello, FastDFS!".getBytes();
            String textFileId = client.uploadBuffer(content, "txt", null);
            System.out.println("Text file uploaded: " + textFileId);
            
            // Download and print content
            byte[] textData = client.downloadFile(textFileId);
            System.out.println("Text content: " + new String(textData));
            
            // Delete files
            System.out.println("Deleting files...");
            client.deleteFile(fileId);
            client.deleteFile(textFileId);
            System.out.println("Files deleted successfully");
            
            // Verify deletion
            exists = client.fileExists(fileId);
            System.out.println("File exists after deletion: " + exists);
            
        } catch (FastDFSException e) {
            System.err.println("FastDFS error: " + e.getMessage());
            e.printStackTrace();
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
