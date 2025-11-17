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

	// Upload file with metadata
	fmt.Println("=== Upload File with Metadata ===")
	metadata := map[string]string{
		"author":      "John Doe",
		"title":       "Sample Document",
		"date":        "2025-01-15",
		"version":     "1.0",
		"description": "This is a test file with metadata",
	}

	data := []byte("File content with metadata")
	fileID, err := client.UploadBuffer(ctx, data, "txt", metadata)
	if err != nil {
		log.Fatalf("Failed to upload file: %v", err)
	}
	fmt.Printf("File uploaded: %s\n", fileID)

	// Get metadata
	fmt.Println("\n=== Get Metadata ===")
	retrievedMeta, err := client.GetMetadata(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to get metadata: %v", err)
	}

	fmt.Println("Retrieved metadata:")
	for key, value := range retrievedMeta {
		fmt.Printf("  %s: %s\n", key, value)
	}

	// Update metadata (merge)
	fmt.Println("\n=== Update Metadata (Merge) ===")
	newMeta := map[string]string{
		"version":    "1.1",
		"updated_by": "Jane Smith",
		"updated_at": time.Now().Format(time.RFC3339),
	}

	err = client.SetMetadata(ctx, fileID, newMeta, fdfs.MetadataMerge)
	if err != nil {
		log.Fatalf("Failed to update metadata: %v", err)
	}
	fmt.Println("Metadata updated (merged)")

	// Get updated metadata
	updatedMeta, err := client.GetMetadata(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to get updated metadata: %v", err)
	}

	fmt.Println("Updated metadata:")
	for key, value := range updatedMeta {
		fmt.Printf("  %s: %s\n", key, value)
	}

	// Overwrite metadata
	fmt.Println("\n=== Overwrite Metadata ===")
	overwriteMeta := map[string]string{
		"status": "archived",
		"year":   "2025",
	}

	err = client.SetMetadata(ctx, fileID, overwriteMeta, fdfs.MetadataOverwrite)
	if err != nil {
		log.Fatalf("Failed to overwrite metadata: %v", err)
	}
	fmt.Println("Metadata overwritten")

	// Get final metadata
	finalMeta, err := client.GetMetadata(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to get final metadata: %v", err)
	}

	fmt.Println("Final metadata:")
	for key, value := range finalMeta {
		fmt.Printf("  %s: %s\n", key, value)
	}

	// Clean up
	fmt.Println("\n=== Cleanup ===")
	err = client.DeleteFile(ctx, fileID)
	if err != nil {
		log.Fatalf("Failed to delete file: %v", err)
	}
	fmt.Println("File deleted successfully")
}
