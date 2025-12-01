import org.fastdfs.client.FastDFSClient;
import org.fastdfs.client.config.ClientConfig;
import org.fastdfs.client.exception.FastDFSException;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Example demonstrating concurrent file uploads.
 */
public class ConcurrentUpload {
    
    public static void main(String[] args) throws Exception {
        ClientConfig config = new ClientConfig("192.168.1.100:22122");
        config.setMaxConns(100); // Increase connection pool size
        
        try (FastDFSClient client = new FastDFSClient(config)) {
            
            int threadCount = 10;
            int filesPerThread = 10;
            int totalFiles = threadCount * filesPerThread;
            
            System.out.println("Starting concurrent upload test...");
            System.out.println("Threads: " + threadCount);
            System.out.println("Files per thread: " + filesPerThread);
            System.out.println("Total files: " + totalFiles);
            
            ExecutorService executor = Executors.newFixedThreadPool(threadCount);
            CountDownLatch latch = new CountDownLatch(totalFiles);
            
            AtomicInteger successCount = new AtomicInteger(0);
            AtomicInteger failureCount = new AtomicInteger(0);
            List<String> uploadedFileIds = new CopyOnWriteArrayList<>();
            
            long startTime = System.currentTimeMillis();
            
            // Submit upload tasks
            for (int i = 0; i < threadCount; i++) {
                final int threadId = i;
                
                for (int j = 0; j < filesPerThread; j++) {
                    final int fileId = j;
                    
                    executor.submit(() -> {
                        try {
                            // Create sample data
                            String content = String.format(
                                "Thread %d - File %d - %s",
                                threadId, fileId, System.currentTimeMillis()
                            );
                            byte[] data = content.getBytes();
                            
                            // Upload file
                            String uploadedFileId = client.uploadBuffer(data, "txt", null);
                            uploadedFileIds.add(uploadedFileId);
                            successCount.incrementAndGet();
                            
                            System.out.println(String.format(
                                "[Thread %d] Uploaded file %d: %s",
                                threadId, fileId, uploadedFileId
                            ));
                            
                        } catch (FastDFSException e) {
                            failureCount.incrementAndGet();
                            System.err.println(String.format(
                                "[Thread %d] Failed to upload file %d: %s",
                                threadId, fileId, e.getMessage()
                            ));
                        } finally {
                            latch.countDown();
                        }
                    });
                }
            }
            
            // Wait for all uploads to complete
            latch.await();
            executor.shutdown();
            executor.awaitTermination(1, TimeUnit.MINUTES);
            
            long endTime = System.currentTimeMillis();
            long duration = endTime - startTime;
            
            // Print statistics
            System.out.println("\n=== Upload Statistics ===");
            System.out.println("Total files: " + totalFiles);
            System.out.println("Successful: " + successCount.get());
            System.out.println("Failed: " + failureCount.get());
            System.out.println("Duration: " + duration + " ms");
            System.out.println("Average: " + (duration / totalFiles) + " ms per file");
            System.out.println("Throughput: " + (totalFiles * 1000.0 / duration) + " files/sec");
            
            // Verify and clean up
            if (successCount.get() > 0) {
                System.out.println("\n=== Verification ===");
                System.out.println("Verifying first uploaded file...");
                String firstFileId = uploadedFileIds.get(0);
                byte[] data = client.downloadFile(firstFileId);
                System.out.println("Downloaded content: " + new String(data));
                
                System.out.println("\n=== Cleanup ===");
                System.out.println("Deleting uploaded files...");
                int deletedCount = 0;
                for (String fileId : uploadedFileIds) {
                    try {
                        client.deleteFile(fileId);
                        deletedCount++;
                    } catch (FastDFSException e) {
                        System.err.println("Failed to delete " + fileId + ": " + e.getMessage());
                    }
                }
                System.out.println("Deleted " + deletedCount + " files");
            }
            
            System.out.println("\nTest completed successfully!");
            
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
