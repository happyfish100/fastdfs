package main

import (
	"context"
	"fmt"
	"log"
	"time"

	fdfs "github.com/happyfish100/fastdfs/go_client"
)

func main() {
	// Create client configuration
	config := &fdfs.ClientConfig{
		TrackerAddrs: []string{
			"192.168.1.100:22122",
			"192.168.1.101:22122",
		},
		MaxConns:       100,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
		IdleTimeout:    60 * time.Second,
		RetryCount:     3,
	}

	// Initialize client
	client, err := fdfs.NewClient(config)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	ctx := context.Background()

	// Example 1: Upload a file
	fmt.Println("=== Upload File ===")
	fileID, err := client.UploadFile(ctx, "test.txt", nil)
	if err != nil {
		log.Fatalf("Failed to upload file: %v", err)
	}
	fmt.Printf("File uploaded successfully: %s\n", fileID)

	// Example 2: Upload from buffer
	fmt.Println("\n=== Upload Buffer ===")
	data := []byte("Hello, FastDFS from Go!")
	bufferFileID, err := client.UploadBuffer(ctx, data, "txt", nil)
	if err != nil {
		log.Fatalf("Failed to upload buffer: %v", err)
	}
	fmt.Printf("Buffer uploaded successfully: %s\n", bufferFileID)

	// Example 3: Download file
	fmt.Println("\n=== Download File ===")
	downloadedData, err := client.DownloadFile(ctx, bufferFileID)
	if err != nil {
		log.Fatalf("Failed to download file: %v", err)
	}
	fmt.Printf("Downloaded %d bytes: %s\n", len(downloadedData), string(downloadedData))

	// Example 4: Get file info
	fmt.Println("\n=== Get File Info ===")
	info, err := client.GetFileInfo(ctx, bufferFileID)
	if err != nil {
		log.Fatalf("Failed to get file info: %v", err)
	}
	fmt.Printf("File Size: %d bytes\n", info.FileSize)
	fmt.Printf("Create Time: %v\n", info.CreateTime)
	fmt.Printf("CRC32: %d\n", info.CRC32)
	fmt.Printf("Source IP: %s\n", info.SourceIPAddr)

	// Example 5: Check if file exists
	fmt.Println("\n=== Check File Exists ===")
	exists, err := client.FileExists(ctx, bufferFileID)
	if err != nil {
		log.Fatalf("Failed to check file existence: %v", err)
	}
	fmt.Printf("File exists: %v\n", exists)

	// Example 6: Download to file
	fmt.Println("\n=== Download to File ===")
	err = client.DownloadToFile(ctx, bufferFileID, "downloaded_test.txt")
	if err != nil {
		log.Fatalf("Failed to download to file: %v", err)
	}
	fmt.Println("File downloaded to: downloaded_test.txt")

	// Example 7: Delete file
	fmt.Println("\n=== Delete File ===")
	err = client.DeleteFile(ctx, bufferFileID)
	if err != nil {
		log.Fatalf("Failed to delete file: %v", err)
	}
	fmt.Println("File deleted successfully")

	// Verify deletion
	exists, err = client.FileExists(ctx, bufferFileID)
	if err != nil {
		log.Fatalf("Failed to check file existence: %v", err)
	}
	fmt.Printf("File exists after deletion: %v\n", exists)

	fmt.Println("\n=== All operations completed successfully! ===")
}
