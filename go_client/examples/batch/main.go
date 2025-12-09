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

// BatchResult represents the result of a batch operation
type BatchResult struct {
	Success   bool
	FileID    string
	Error     error
	Index     int
	Duration  time.Duration
}

// BatchStats tracks batch operation statistics
type BatchStats struct {
	Total       int
	Successful  int
	Failed      int
	TotalTime   time.Duration
	AvgTime     time.Duration
	Throughput  float64
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

	fmt.Println("FastDFS Go Client - Batch Operations Example")
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println()

	ctx := context.Background()

	config := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      50, // Higher for batch operations
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
	}

	client, err := fdfs.NewClient(config)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	// ====================================================================
	// EXAMPLE 1: Simple Batch Upload
	// ====================================================================
	fmt.Println("1. Simple Batch Upload")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Batch upload/download operations.")
	fmt.Println()

	const batchSize = 20
	const dataSize = 4 * 1024

	fmt.Printf("   Uploading %d files in batch...\n", batchSize)

	var batchFiles []string
	var batchMutex sync.Mutex
	var successCount int64

	start := time.Now()
	for i := 0; i < batchSize; i++ {
		data := createTestData(dataSize)
		fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
		if err == nil {
			atomic.AddInt64(&successCount, 1)
			batchMutex.Lock()
			batchFiles = append(batchFiles, fileID)
			batchMutex.Unlock()
		} else {
			fmt.Printf("     → Upload %d failed: %v\n", i+1, err)
		}
	}
	duration := time.Since(start)

	fmt.Printf("   ✓ Completed in %v\n", duration)
	fmt.Printf("   → Successful: %d/%d\n", successCount, batchSize)
	if successCount > 0 {
		throughput := float64(successCount) / duration.Seconds()
		fmt.Printf("   → Throughput: %.2f ops/sec\n", throughput)
	}

	// Cleanup
	for _, fileID := range batchFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 2: Parallel Batch Operations
	// ====================================================================
	fmt.Println("2. Parallel Batch Operations")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Batch processing, parallel batch operations.")
	fmt.Println()

	const parallelBatchSize = 30

	fmt.Printf("   Uploading %d files in parallel...\n", parallelBatchSize)

	var parallelWg sync.WaitGroup
	parallelResults := make(chan BatchResult, parallelBatchSize)
	var parallelFiles []string
	var parallelMutex sync.Mutex
	var parallelSuccess int64

	parallelStart := time.Now()
	for i := 0; i < parallelBatchSize; i++ {
		parallelWg.Add(1)
		go func(index int) {
			defer parallelWg.Done()

			opStart := time.Now()
			data := createTestData(dataSize)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			result := BatchResult{
				Success:  err == nil,
				FileID:   fileID,
				Error:    err,
				Index:    index,
				Duration: opDuration,
			}

			if err == nil {
				atomic.AddInt64(&parallelSuccess, 1)
				parallelMutex.Lock()
				parallelFiles = append(parallelFiles, fileID)
				parallelMutex.Unlock()
			}

			parallelResults <- result
		}(i)
	}

	parallelWg.Wait()
	close(parallelResults)
	parallelDuration := time.Since(parallelStart)

	fmt.Printf("   ✓ Completed in %v\n", parallelDuration)
	fmt.Printf("   → Successful: %d/%d\n", parallelSuccess, parallelBatchSize)
	if parallelSuccess > 0 {
		throughput := float64(parallelSuccess) / parallelDuration.Seconds()
		fmt.Printf("   → Throughput: %.2f ops/sec\n", throughput)
	}

	// Cleanup
	for _, fileID := range parallelFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 3: Batch Download
	// ====================================================================
	fmt.Println("3. Batch Download")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Batch download operations.")
	fmt.Println()

	// First upload files to download
	const downloadBatchSize = 15
	fmt.Printf("   Preparing %d files for batch download...\n", downloadBatchSize)

	var downloadFileIDs []string
	for i := 0; i < downloadBatchSize; i++ {
		data := createTestData(3 * 1024)
		fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
		if err == nil {
			downloadFileIDs = append(downloadFileIDs, fileID)
		}
	}

	fmt.Printf("   → Uploaded %d files\n", len(downloadFileIDs))
	fmt.Println("   Downloading files in batch...")

	var downloadWg sync.WaitGroup
	downloadResults := make(chan BatchResult, len(downloadFileIDs))
	var downloadSuccess int64

	downloadStart := time.Now()
	for i, fileID := range downloadFileIDs {
		downloadWg.Add(1)
		go func(index int, fid string) {
			defer downloadWg.Done()

			opStart := time.Now()
			_, err := client.DownloadFile(ctx, fid)
			opDuration := time.Since(opStart)

			result := BatchResult{
				Success:  err == nil,
				FileID:   fid,
				Error:    err,
				Index:    index,
				Duration: opDuration,
			}

			if err == nil {
				atomic.AddInt64(&downloadSuccess, 1)
			}

			downloadResults <- result
		}(i, fileID)
	}

	downloadWg.Wait()
	close(downloadResults)
	downloadDuration := time.Since(downloadStart)

	fmt.Printf("   ✓ Completed in %v\n", downloadDuration)
	fmt.Printf("   → Successful: %d/%d\n", downloadSuccess, len(downloadFileIDs))
	if downloadSuccess > 0 {
		throughput := float64(downloadSuccess) / downloadDuration.Seconds()
		fmt.Printf("   → Throughput: %.2f ops/sec\n", throughput)
	}

	// Cleanup
	for _, fileID := range downloadFileIDs {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 4: Batch Error Handling
	// ====================================================================
	fmt.Println("4. Batch Error Handling")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Batch error handling.")
	fmt.Println()

	const errorBatchSize = 25

	fmt.Printf("   Processing batch of %d operations with error handling...\n", errorBatchSize)

	var errorWg sync.WaitGroup
	errorResults := make(chan BatchResult, errorBatchSize)
	errorChan := make(chan error, errorBatchSize)
	var errorFiles []string
	var errorMutex sync.Mutex
	var errorSuccess int64
	var errorFailed int64

	errorStart := time.Now()
	for i := 0; i < errorBatchSize; i++ {
		errorWg.Add(1)
		go func(index int) {
			defer errorWg.Done()

			opStart := time.Now()
			data := createTestData(2 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			result := BatchResult{
				Success:  err == nil,
				FileID:   fileID,
				Error:    err,
				Index:    index,
				Duration: opDuration,
			}

			if err == nil {
				atomic.AddInt64(&errorSuccess, 1)
				errorMutex.Lock()
				errorFiles = append(errorFiles, fileID)
				errorMutex.Unlock()
			} else {
				atomic.AddInt64(&errorFailed, 1)
				errorChan <- err
			}

			errorResults <- result
		}(i)
	}

	go func() {
		errorWg.Wait()
		close(errorResults)
		close(errorChan)
	}()

	// Collect errors
	var errors []error
	for err := range errorChan {
		errors = append(errors, err)
	}

	errorDuration := time.Since(errorStart)

	fmt.Printf("   ✓ Completed in %v\n", errorDuration)
	fmt.Printf("   → Successful: %d, Failed: %d\n", errorSuccess, errorFailed)
	if len(errors) > 0 {
		fmt.Printf("   → Errors encountered: %d\n", len(errors))
		for i, err := range errors {
			if i < 3 { // Show first 3 errors
				fmt.Printf("     → Error %d: %v\n", i+1, err)
			}
		}
		if len(errors) > 3 {
			fmt.Printf("     → ... and %d more errors\n", len(errors)-3)
		}
	}

	// Cleanup
	for _, fileID := range errorFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 5: Batch with Progress Tracking
	// ====================================================================
	fmt.Println("5. Batch with Progress Tracking")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Batch processing with progress tracking.")
	fmt.Println()

	const progressBatchSize = 40

	fmt.Printf("   Processing batch of %d operations with progress tracking...\n", progressBatchSize)

	var progressWg sync.WaitGroup
	progressChan := make(chan int, progressBatchSize)
	var progressFiles []string
	var progressMutex sync.Mutex
	var progressCompleted int64

	progressStart := time.Now()
	for i := 0; i < progressBatchSize; i++ {
		progressWg.Add(1)
		go func(index int) {
			defer progressWg.Done()

			data := createTestData(2 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				progressMutex.Lock()
				progressFiles = append(progressFiles, fileID)
				progressMutex.Unlock()
			}

			atomic.AddInt64(&progressCompleted, 1)
			progressChan <- int(atomic.LoadInt64(&progressCompleted))
		}(i)
	}

	// Monitor progress
	go func() {
		progressWg.Wait()
		close(progressChan)
	}()

	// Display progress
	for completed := range progressChan {
		progress := float64(completed) / float64(progressBatchSize) * 100.0
		fmt.Printf("\r   Progress: %d/%d (%.1f%%)", completed, progressBatchSize, progress)
	}
	fmt.Println()

	progressDuration := time.Since(progressStart)
	fmt.Printf("   ✓ Completed in %v\n", progressDuration)
	fmt.Printf("   → Successful: %d/%d\n", len(progressFiles), progressBatchSize)

	// Cleanup
	for _, fileID := range progressFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 6: Batch Statistics
	// ====================================================================
	fmt.Println("6. Batch Statistics")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Collecting detailed batch operation statistics.")
	fmt.Println()

	const statsBatchSize = 30

	fmt.Printf("   Processing batch of %d operations for statistics...\n", statsBatchSize)

	var statsWg sync.WaitGroup
	statsResults := make(chan BatchResult, statsBatchSize)
	var statsFiles []string
	var statsMutex sync.Mutex

	statsStart := time.Now()
	for i := 0; i < statsBatchSize; i++ {
		statsWg.Add(1)
		go func(index int) {
			defer statsWg.Done()

			opStart := time.Now()
			data := createTestData(3 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			result := BatchResult{
				Success:  err == nil,
				FileID:   fileID,
				Error:    err,
				Index:    index,
				Duration: opDuration,
			}

			if err == nil {
				statsMutex.Lock()
				statsFiles = append(statsFiles, fileID)
				statsMutex.Unlock()
			}

			statsResults <- result
		}(i)
	}

	statsWg.Wait()
	close(statsResults)
	statsDuration := time.Since(statsStart)

	// Calculate statistics
	var batchStats BatchStats
	var operationTimes []time.Duration

	for result := range statsResults {
		batchStats.Total++
		if result.Success {
			batchStats.Successful++
			batchStats.TotalTime += result.Duration
			operationTimes = append(operationTimes, result.Duration)
		} else {
			batchStats.Failed++
		}
	}

	if batchStats.Successful > 0 {
		batchStats.AvgTime = batchStats.TotalTime / time.Duration(batchStats.Successful)
		batchStats.Throughput = float64(batchStats.Successful) / statsDuration.Seconds()
	}

	fmt.Printf("   Batch Statistics:\n")
	fmt.Printf("     Total operations: %d\n", batchStats.Total)
	fmt.Printf("     Successful: %d\n", batchStats.Successful)
	fmt.Printf("     Failed: %d\n", batchStats.Failed)
	fmt.Printf("     Total time: %v\n", statsDuration)
	fmt.Printf("     Average operation time: %v\n", batchStats.AvgTime)
	fmt.Printf("     Throughput: %.2f ops/sec\n", batchStats.Throughput)

	if len(operationTimes) > 0 {
		var minTime, maxTime time.Duration = operationTimes[0], operationTimes[0]
		for _, t := range operationTimes {
			if t < minTime {
				minTime = t
			}
			if t > maxTime {
				maxTime = t
			}
		}
		fmt.Printf("     Min operation time: %v\n", minTime)
		fmt.Printf("     Max operation time: %v\n", maxTime)
	}

	// Cleanup
	for _, fileID := range statsFiles {
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// SUMMARY
	// ====================================================================
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println("Batch Operations Example completed successfully!")
	fmt.Println()
	fmt.Println("Summary of demonstrated features:")
	fmt.Println("  ✓ Batch upload/download operations")
	fmt.Println("  ✓ Batch processing patterns")
	fmt.Println("  ✓ Parallel batch operations")
	fmt.Println("  ✓ Batch error handling")
	fmt.Println("  ✓ Progress tracking for batches")
	fmt.Println("  ✓ Batch statistics collection")
	fmt.Println()
	fmt.Println("Best Practices:")
	fmt.Println("  • Use goroutines for parallel batch operations")
	fmt.Println("  • Use channels for result collection and coordination")
	fmt.Println("  • Implement proper error handling for batch operations")
	fmt.Println("  • Track progress for long-running batches")
	fmt.Println("  • Collect statistics for batch performance analysis")
	fmt.Println("  • Clean up resources after batch operations")
	fmt.Println("  • Configure appropriate MaxConns for batch workloads")
}

