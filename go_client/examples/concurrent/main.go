package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	fdfs "github.com/happyfish100/fastdfs/go_client"
)

// OperationResult represents the result of a single operation
type OperationResult struct {
	Success   bool
	FileID    string
	Error     error
	Duration  time.Duration
	Operation string
	Index     int
}

// createTestData creates test data of specified size
func createTestData(size int) []byte {
	data := make([]byte, size)
	for i := range data {
		data[i] = byte(i % 256)
	}
	return data
}

func main() {
	if len(os.Args) < 2 {
		log.Fatal("Usage: go run main.go <tracker_address>")
	}

	trackerAddr := os.Args[1]

	fmt.Println("FastDFS Go Client - Concurrent Operations Example")
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println()

	ctx := context.Background()

	// ====================================================================
	// EXAMPLE 1: Concurrent Uploads with Goroutines
	// ====================================================================
	fmt.Println("1. Concurrent Uploads with Goroutines")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Shows goroutine-based concurrent operations.")
	fmt.Println()

	config := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      50, // Higher connection limit for concurrent operations
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
	}

	client, err := fdfs.NewClient(config)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	const numConcurrentUploads = 20
	const dataSize = 5 * 1024 // 5KB per file

	fmt.Printf("   Uploading %d files concurrently using goroutines...\n", numConcurrentUploads)

	var wg sync.WaitGroup
	results := make(chan OperationResult, numConcurrentUploads)
	var uploadedFiles []string
	var filesMutex sync.Mutex
	var successCount int64
	var failureCount int64

	start := time.Now()

	for i := 0; i < numConcurrentUploads; i++ {
		wg.Add(1)
		go func(index int) {
			defer wg.Done()

			opStart := time.Now()
			data := createTestData(dataSize)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			result := OperationResult{
				Success:   err == nil,
				FileID:    fileID,
				Error:     err,
				Duration:  opDuration,
				Operation: "upload",
				Index:     index,
			}

			if err == nil {
				atomic.AddInt64(&successCount, 1)
				filesMutex.Lock()
				uploadedFiles = append(uploadedFiles, fileID)
				filesMutex.Unlock()
			} else {
				atomic.AddInt64(&failureCount, 1)
			}

			results <- result
		}(i)
	}

	// Wait for all goroutines to complete
	wg.Wait()
	close(results)
	totalDuration := time.Since(start)

	// Collect results
	var allResults []OperationResult
	for result := range results {
		allResults = append(allResults, result)
	}

	fmt.Printf("   ✓ Completed in %v\n", totalDuration)
	fmt.Printf("   → Successful: %d, Failed: %d\n", successCount, failureCount)
	if len(allResults) > 0 {
		var totalTime time.Duration
		for _, r := range allResults {
			if r.Success {
				totalTime += r.Duration
			}
		}
		if successCount > 0 {
			avgTime := totalTime / time.Duration(successCount)
			fmt.Printf("   → Average operation time: %v\n", avgTime)
			opsPerSec := float64(successCount) / totalDuration.Seconds()
			fmt.Printf("   → Throughput: %.2f ops/sec\n", opsPerSec)
		}
	}

	// Cleanup
	for _, fileID := range uploadedFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 2: Concurrent Downloads
	// ====================================================================
	fmt.Println("2. Concurrent Downloads")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates concurrent downloads using goroutines.")
	fmt.Println()

	// First upload some files to download
	const numFilesToDownload = 15
	var fileIDs []string
	fmt.Printf("   Preparing %d files for concurrent download...\n", numFilesToDownload)

	for i := 0; i < numFilesToDownload; i++ {
		data := createTestData(3 * 1024)
		fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
		if err == nil {
			fileIDs = append(fileIDs, fileID)
		}
	}

	fmt.Printf("   → Uploaded %d files\n", len(fileIDs))
	fmt.Println("   Downloading files concurrently...")

	var downloadWg sync.WaitGroup
	downloadResults := make(chan OperationResult, len(fileIDs))
	var downloadSuccessCount int64

	downloadStart := time.Now()

	for i, fileID := range fileIDs {
		downloadWg.Add(1)
		go func(index int, fid string) {
			defer downloadWg.Done()

			opStart := time.Now()
			_, err := client.DownloadFile(ctx, fid)
			opDuration := time.Since(opStart)

			result := OperationResult{
				Success:   err == nil,
				FileID:    fid,
				Error:     err,
				Duration:  opDuration,
				Operation: "download",
				Index:     index,
			}

			if err == nil {
				atomic.AddInt64(&downloadSuccessCount, 1)
			}

			downloadResults <- result
		}(i, fileID)
	}

	downloadWg.Wait()
	close(downloadResults)
	downloadDuration := time.Since(downloadStart)

	fmt.Printf("   ✓ Completed in %v\n", downloadDuration)
	fmt.Printf("   → Successful downloads: %d/%d\n", downloadSuccessCount, len(fileIDs))
	if downloadSuccessCount > 0 {
		opsPerSec := float64(downloadSuccessCount) / downloadDuration.Seconds()
		fmt.Printf("   → Throughput: %.2f ops/sec\n", opsPerSec)
	}

	// Cleanup
	for _, fileID := range fileIDs {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 3: Using Channels for Coordination
	// ====================================================================
	fmt.Println("3. Using Channels for Coordination")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Shows channels, sync.WaitGroup, concurrent uploads/downloads.")
	fmt.Println()

	const numWorkers = 10
	const jobsPerWorker = 5

	// Create a job channel
	jobs := make(chan int, numWorkers*jobsPerWorker)
	resultsChan := make(chan OperationResult, numWorkers*jobsPerWorker)

	// Fill job channel
	for i := 0; i < numWorkers*jobsPerWorker; i++ {
		jobs <- i
	}
	close(jobs)

	// Start worker goroutines
	var workersWg sync.WaitGroup
	for w := 0; w < numWorkers; w++ {
		workersWg.Add(1)
		go func(workerID int) {
			defer workersWg.Done()

			for jobID := range jobs {
				opStart := time.Now()
				data := createTestData(2 * 1024)
				fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
				opDuration := time.Since(opStart)

				result := OperationResult{
					Success:   err == nil,
					FileID:    fileID,
					Error:     err,
					Duration:  opDuration,
					Operation: fmt.Sprintf("worker-%d-job-%d", workerID, jobID),
					Index:     jobID,
				}

				resultsChan <- result
			}
		}(w)
	}

	// Wait for all workers to complete
	go func() {
		workersWg.Wait()
		close(resultsChan)
	}()

	// Collect results
	var workerResults []OperationResult
	var workerFiles []string
	for result := range resultsChan {
		workerResults = append(workerResults, result)
		if result.Success {
			workerFiles = append(workerFiles, result.FileID)
		}
	}

	fmt.Printf("   ✓ Processed %d jobs with %d workers\n", len(workerResults), numWorkers)
	successfulJobs := 0
	for _, r := range workerResults {
		if r.Success {
			successfulJobs++
		}
	}
	fmt.Printf("   → Successful: %d/%d\n", successfulJobs, len(workerResults))

	// Cleanup
	for _, fileID := range workerFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 4: Mixed Concurrent Operations
	// ====================================================================
	fmt.Println("4. Mixed Concurrent Operations")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Concurrent uploads and downloads happening simultaneously.")
	fmt.Println()

	const mixedOps = 10
	var mixedWg sync.WaitGroup
	mixedResults := make(chan OperationResult, mixedOps*2)
	var mixedFiles []string
	var mixedMutex sync.Mutex

	mixedStart := time.Now()

	// Concurrent uploads
	for i := 0; i < mixedOps; i++ {
		mixedWg.Add(1)
		go func(index int) {
			defer mixedWg.Done()

			opStart := time.Now()
			data := createTestData(4 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			result := OperationResult{
				Success:   err == nil,
				FileID:    fileID,
				Error:     err,
				Duration:  opDuration,
				Operation: "upload",
				Index:     index,
			}

			if err == nil {
				mixedMutex.Lock()
				mixedFiles = append(mixedFiles, fileID)
				mixedMutex.Unlock()
			}

			mixedResults <- result
		}(i)
	}

	// Wait a bit for some uploads to complete, then start downloads
	time.Sleep(100 * time.Millisecond)

	// Concurrent downloads (of files that were just uploaded)
	for i := 0; i < mixedOps; i++ {
		mixedWg.Add(1)
		go func(index int) {
			defer mixedWg.Done()

			// Wait for a file to be available
			for {
				mixedMutex.Lock()
				if len(mixedFiles) > index {
					fileID := mixedFiles[index]
					mixedMutex.Unlock()

					opStart := time.Now()
					_, err := client.DownloadFile(ctx, fileID)
					opDuration := time.Since(opStart)

					result := OperationResult{
						Success:   err == nil,
						FileID:    fileID,
						Error:     err,
						Duration:  opDuration,
						Operation: "download",
						Index:     index,
					}

					mixedResults <- result
					return
				}
				mixedMutex.Unlock()
				time.Sleep(50 * time.Millisecond)
			}
		}(i)
	}

	mixedWg.Wait()
	close(mixedResults)
	mixedDuration := time.Since(mixedStart)

	var mixedUploads, mixedDownloads int
	var mixedUploadSuccess, mixedDownloadSuccess int
	for result := range mixedResults {
		if result.Operation == "upload" {
			mixedUploads++
			if result.Success {
				mixedUploadSuccess++
			}
		} else {
			mixedDownloads++
			if result.Success {
				mixedDownloadSuccess++
			}
		}
	}

	fmt.Printf("   ✓ Completed in %v\n", mixedDuration)
	fmt.Printf("   → Uploads: %d successful/%d total\n", mixedUploadSuccess, mixedUploads)
	fmt.Printf("   → Downloads: %d successful/%d total\n", mixedDownloadSuccess, mixedDownloads)

	// Cleanup
	for _, fileID := range mixedFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 5: Error Handling in Concurrent Operations
	// ====================================================================
	fmt.Println("5. Error Handling in Concurrent Operations")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates error handling in concurrent scenarios.")
	fmt.Println()

	const errorTestOps = 15
	var errorWg sync.WaitGroup
	errorResults := make(chan OperationResult, errorTestOps)
	errorChan := make(chan error, errorTestOps)

	errorStart := time.Now()

	for i := 0; i < errorTestOps; i++ {
		errorWg.Add(1)
		go func(index int) {
			defer errorWg.Done()

			opStart := time.Now()
			data := createTestData(3 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			result := OperationResult{
				Success:   err == nil,
				FileID:    fileID,
				Error:     err,
				Duration:  opDuration,
				Operation: "upload",
				Index:     index,
			}

			if err != nil {
				errorChan <- err
			}

			errorResults <- result
		}(i)
	}

	// Monitor errors in a separate goroutine
	go func() {
		errorWg.Wait()
		close(errorResults)
		close(errorChan)
	}()

	var errorFiles []string
	var errorCount int
	for result := range errorResults {
		if result.Success {
			errorFiles = append(errorFiles, result.FileID)
		} else {
			errorCount++
		}
	}

	errorDuration := time.Since(errorStart)

	fmt.Printf("   ✓ Completed in %v\n", errorDuration)
	fmt.Printf("   → Successful: %d, Failed: %d\n", len(errorFiles), errorCount)

	// Print errors
	for err := range errorChan {
		fmt.Printf("   → Error: %v\n", err)
	}

	// Cleanup
	for _, fileID := range errorFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 6: Performance Comparison: Sequential vs Concurrent
	// ====================================================================
	fmt.Println("6. Performance Comparison: Sequential vs Concurrent")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Comparing sequential and concurrent operation performance.")
	fmt.Println()

	const comparisonOps = 20
	const comparisonDataSize = 3 * 1024

	// Sequential operations
	fmt.Println("   Sequential operations...")
	seqStart := time.Now()
	var seqFiles []string
	for i := 0; i < comparisonOps; i++ {
		data := createTestData(comparisonDataSize)
		fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
		if err == nil {
			seqFiles = append(seqFiles, fileID)
		}
	}
	seqDuration := time.Since(seqStart)

	// Cleanup sequential
	for _, fileID := range seqFiles {
		client.DeleteFile(ctx, fileID)
	}

	// Concurrent operations
	fmt.Println("   Concurrent operations...")
	conStart := time.Now()
	var conWg sync.WaitGroup
	var conFiles []string
	var conMutex sync.Mutex

	for i := 0; i < comparisonOps; i++ {
		conWg.Add(1)
		go func() {
			defer conWg.Done()
			data := createTestData(comparisonDataSize)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				conMutex.Lock()
				conFiles = append(conFiles, fileID)
				conMutex.Unlock()
			}
		}()
	}

	conWg.Wait()
	conDuration := time.Since(conStart)

	// Cleanup concurrent
	for _, fileID := range conFiles {
		client.DeleteFile(ctx, fileID)
	}

	fmt.Printf("   Sequential: %v (%d operations)\n", seqDuration, len(seqFiles))
	fmt.Printf("   Concurrent: %v (%d operations)\n", conDuration, len(conFiles))
	if seqDuration > 0 && conDuration > 0 {
		improvement := ((float64(seqDuration) / float64(conDuration)) - 1.0) * 100.0
		fmt.Printf("   → Improvement: %.1f%% faster (concurrent)\n", improvement)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 7: Rate-Limited Concurrent Operations
	// ====================================================================
	fmt.Println("7. Rate-Limited Concurrent Operations")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Using channels to limit concurrent operations.")
	fmt.Println()

	const maxConcurrent = 5
	const totalOps = 15
	semaphore := make(chan struct{}, maxConcurrent)
	var rateWg sync.WaitGroup
	var rateFiles []string
	var rateMutex sync.Mutex

	rateStart := time.Now()

	for i := 0; i < totalOps; i++ {
		rateWg.Add(1)
		go func(index int) {
			defer rateWg.Done()

			// Acquire semaphore
			semaphore <- struct{}{}
			defer func() { <-semaphore }()

			data := createTestData(2 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				rateMutex.Lock()
				rateFiles = append(rateFiles, fileID)
				rateMutex.Unlock()
				fmt.Printf("     → Operation %d completed\n", index+1)
			}
		}(i)
	}

	rateWg.Wait()
	rateDuration := time.Since(rateStart)

	fmt.Printf("   ✓ Completed %d operations with max %d concurrent in %v\n",
		len(rateFiles), maxConcurrent, rateDuration)

	// Cleanup
	for _, fileID := range rateFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// SUMMARY
	// ====================================================================
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println("Concurrent Operations Example completed successfully!")
	fmt.Println()
	fmt.Println("Summary of demonstrated features:")
	fmt.Println("  ✓ Goroutine-based concurrent operations")
	fmt.Println("  ✓ Channels for coordination and result collection")
	fmt.Println("  ✓ sync.WaitGroup for synchronization")
	fmt.Println("  ✓ Concurrent uploads and downloads")
	fmt.Println("  ✓ Error handling in concurrent scenarios")
	fmt.Println("  ✓ Performance comparison (sequential vs concurrent)")
	fmt.Println("  ✓ Rate-limited concurrent operations")
	fmt.Println()
	fmt.Println("Best Practices:")
	fmt.Println("  • Use sync.WaitGroup to wait for goroutines to complete")
	fmt.Println("  • Use channels for coordination and result collection")
	fmt.Println("  • Use mutexes to protect shared data structures")
	fmt.Println("  • Use atomic operations for simple counters")
	fmt.Println("  • Use semaphores (buffered channels) to limit concurrency")
	fmt.Println("  • Handle errors properly in concurrent operations")
	fmt.Println("  • Clean up resources (files) after operations")
	fmt.Println("  • Configure appropriate MaxConns for concurrent workloads")
}

