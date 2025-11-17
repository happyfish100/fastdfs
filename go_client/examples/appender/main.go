package main

import (
	"context"
	"fmt"
	"log"
	"time"

	fdfs "github.com/happyfish100/fastdfs/go_client"
)

func main() {
	// Create client
	config := &fdfs.ClientConfig{
		TrackerAddrs:   []string{"192.168.1.100:22122"},
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
	}

	client, err := fdfs.NewClient(config)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	ctx := context.Background()

	// Upload appender file
	fmt.Println("=== Upload Appender File ===")
	initialData := []byte("Log file started at " + time.Now().Format(time.RFC3339) + "\n")
	fileID, err := client.UploadAppenderBuffer(ctx, initialData, "log", nil)
	if err != nil {
		log.Fatalf("Failed to upload appender file: %v", err)
	}
	fmt.Printf("Appender file created: %s\n", fileID)

	// Get initial file info
	info, err := client.GetFileInfo(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to get file info: %v", err)
	}
	fmt.Printf("Initial file size: %d bytes\n", info.FileSize)

	// Append data multiple times
	fmt.Println("\n=== Append Data ===")
	for i := 1; i <= 5; i++ {
		logEntry := fmt.Sprintf("[%s] Log entry #%d\n", time.Now().Format("15:04:05"), i)
		err = client.AppendFile(ctx, fileID, []byte(logEntry))
		if err != nil {
			log.Fatalf("Failed to append data: %v", err)
		}
		fmt.Printf("Appended: %s", logEntry)
		time.Sleep(100 * time.Millisecond)
	}

	// Get updated file info
	info, err = client.GetFileInfo(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to get file info: %v", err)
	}
	fmt.Printf("\nFile size after appends: %d bytes\n", info.FileSize)

	// Download and display content
	fmt.Println("\n=== Download Content ===")
	content, err := client.DownloadFile(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to download file: %v", err)
	}
	fmt.Println("File content:")
	fmt.Println(string(content))

	// Modify content
	fmt.Println("\n=== Modify Content ===")
	modifyData := []byte("MODIFIED")
	err = client.ModifyFile(ctx, fileID, 0, modifyData)
	if err != nil {
		log.Fatalf("Failed to modify file: %v", err)
	}
	fmt.Println("Modified first 8 bytes")

	// Download modified content
	modifiedContent, err := client.DownloadFile(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to download modified file: %v", err)
	}
	fmt.Println("Modified content:")
	fmt.Println(string(modifiedContent))

	// Truncate file
	fmt.Println("\n=== Truncate File ===")
	err = client.TruncateFile(ctx, fileID, 50)
	if err != nil {
		log.Fatalf("Failed to truncate file: %v", err)
	}
	fmt.Println("File truncated to 50 bytes")

	// Get final file info
	info, err = client.GetFileInfo(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to get file info: %v", err)
	}
	fmt.Printf("Final file size: %d bytes\n", info.FileSize)

	// Download truncated content
	truncatedContent, err := client.DownloadFile(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to download truncated file: %v", err)
	}
	fmt.Println("Truncated content:")
	fmt.Println(string(truncatedContent))

	// Clean up
	fmt.Println("\n=== Cleanup ===")
	err = client.DeleteFile(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to delete file: %v", err)
	}
	fmt.Println("File deleted successfully")
}
