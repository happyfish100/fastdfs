package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"runtime"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	fdfs "github.com/happyfish100/fastdfs/go_client"
)

// PerformanceMetrics tracks performance statistics
type PerformanceMetrics struct {
	mu                    sync.Mutex
	OperationsCount       int64
	SuccessfulOperations  int64
	FailedOperations      int64
	TotalTime             time.Duration
	MinTime               time.Duration
	MaxTime               time.Duration
	OperationTimes        []time.Duration
	BytesTransferred      int64
}

// RecordOperation records a single operation's metrics
func (pm *PerformanceMetrics) RecordOperation(success bool, duration time.Duration, bytes int64) {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	atomic.AddInt64(&pm.OperationsCount, 1)
	if success {
		atomic.AddInt64(&pm.SuccessfulOperations, 1)
		pm.TotalTime += duration
		pm.OperationTimes = append(pm.OperationTimes, duration)
		if duration < pm.MinTime || pm.MinTime == 0 {
			pm.MinTime = duration
		}
		if duration > pm.MaxTime {
			pm.MaxTime = duration
		}
		atomic.AddInt64(&pm.BytesTransferred, bytes)
	} else {
		atomic.AddInt64(&pm.FailedOperations, 1)
	}
}

// Print prints formatted performance metrics
func (pm *PerformanceMetrics) Print(title string) {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	fmt.Printf("   %s:\n", title)
	fmt.Printf("     Operations: %d (Success: %d, Failed: %d)\n",
		pm.OperationsCount, pm.SuccessfulOperations, pm.FailedOperations)

	if pm.SuccessfulOperations > 0 {
		avgTime := pm.TotalTime / time.Duration(pm.SuccessfulOperations)
		fmt.Printf("     Total Time: %v\n", pm.TotalTime)
		fmt.Printf("     Average Time: %v\n", avgTime)
		fmt.Printf("     Min Time: %v\n", pm.MinTime)
		fmt.Printf("     Max Time: %v\n", pm.MaxTime)

		if len(pm.OperationTimes) > 0 {
			sorted := make([]time.Duration, len(pm.OperationTimes))
			copy(sorted, pm.OperationTimes)
			sort.Slice(sorted, func(i, j int) bool {
				return sorted[i] < sorted[j]
			})

			p50Idx := len(sorted) * 50 / 100
			p95Idx := len(sorted) * 95 / 100
			p99Idx := len(sorted) * 99 / 100

			if p50Idx < len(sorted) {
				fmt.Printf("     P50 (Median): %v\n", sorted[p50Idx])
			}
			if p95Idx < len(sorted) {
				fmt.Printf("     P95: %v\n", sorted[p95Idx])
			}
			if p99Idx < len(sorted) {
				fmt.Printf("     P99: %v\n", sorted[p99Idx])
			}
		}

		if pm.TotalTime > 0 {
			opsPerSec := float64(pm.SuccessfulOperations) / pm.TotalTime.Seconds()
			fmt.Printf("     Throughput: %.2f ops/sec\n", opsPerSec)
		}

		if pm.BytesTransferred > 0 && pm.TotalTime > 0 {
			mbps := float64(pm.BytesTransferred) / 1024.0 / 1024.0 / pm.TotalTime.Seconds()
			fmt.Printf("     Data Rate: %.2f MB/s\n", mbps)
		}
	}
}

// Reset resets all metrics
func (pm *PerformanceMetrics) Reset() {
	pm.mu.Lock()
	defer pm.mu.Unlock()

	pm.OperationsCount = 0
	pm.SuccessfulOperations = 0
	pm.FailedOperations = 0
	pm.TotalTime = 0
	pm.MinTime = 0
	pm.MaxTime = 0
	pm.OperationTimes = pm.OperationTimes[:0]
	pm.BytesTransferred = 0
}

// MemoryStats tracks memory usage
type MemoryStats struct {
	InitialMem uint64
	PeakMem    uint64
}

// Start initializes memory tracking
func (ms *MemoryStats) Start() {
	var m runtime.MemStats
	runtime.ReadMemStats(&m)
	ms.InitialMem = m.Alloc
	ms.PeakMem = m.Alloc
}

// Update updates peak memory
func (ms *MemoryStats) Update() {
	var m runtime.MemStats
	runtime.ReadMemStats(&m)
	if m.Alloc > ms.PeakMem {
		ms.PeakMem = m.Alloc
	}
}

// GetPeakDelta returns peak memory delta in bytes
func (ms *MemoryStats) GetPeakDelta() uint64 {
	if ms.PeakMem > ms.InitialMem {
		return ms.PeakMem - ms.InitialMem
	}
	return 0
}

// FormatMemory formats bytes to human-readable string
func FormatMemory(bytes uint64) string {
	const unit = 1024
	if bytes < unit {
		return fmt.Sprintf("%d B", bytes)
	}
	div, exp := int64(unit), 0
	for n := bytes / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}
	return fmt.Sprintf("%.2f %cB", float64(bytes)/float64(div), "KMG"[exp])
}

// CreateTestData creates test data of specified size
func CreateTestData(size int) []byte {
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

	fmt.Println("FastDFS Go Client - Performance Example")
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println()

	ctx := context.Background()

	// ====================================================================
	// EXAMPLE 1: Connection Pool Tuning
	// ====================================================================
	fmt.Println("1. Connection Pool Tuning")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Shows connection pool tuning techniques.")
	fmt.Println()

	const numOperations = 50
	const dataSize = 10 * 1024 // 10KB per operation

	poolSizes := []int{1, 5, 10, 20, 50}
	var poolMetrics []*PerformanceMetrics

	for _, poolSize := range poolSizes {
		fmt.Printf("   Testing with max_conns = %d...\n", poolSize)

		config := &fdfs.ClientConfig{
			TrackerAddrs:  []string{trackerAddr},
			MaxConns:      poolSize,
			ConnectTimeout: 5 * time.Second,
			NetworkTimeout: 30 * time.Second,
			IdleTimeout:   60 * time.Second,
			EnablePool:    true,
		}

		client, err := fdfs.NewClient(config)
		if err != nil {
			log.Printf("Failed to create client: %v", err)
			continue
		}

		metrics := &PerformanceMetrics{}
		var uploadedFiles []string
		var filesMutex sync.Mutex

		start := time.Now()

		var wg sync.WaitGroup
		for i := 0; i < numOperations; i++ {
			wg.Add(1)
			go func() {
				defer wg.Done()
				opStart := time.Now()
				data := CreateTestData(dataSize)
				fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
				opDuration := time.Since(opStart)

				if err == nil {
					metrics.RecordOperation(true, opDuration, int64(dataSize))
					filesMutex.Lock()
					uploadedFiles = append(uploadedFiles, fileID)
					filesMutex.Unlock()
				} else {
					metrics.RecordOperation(false, opDuration, 0)
				}
			}()
		}

		wg.Wait()
		totalDuration := time.Since(start)

		// Cleanup
		for _, fileID := range uploadedFiles {
			client.DeleteFile(ctx, fileID)
		}

		client.Close()
		poolMetrics = append(poolMetrics, metrics)
		fmt.Printf("     → Completed in %v\n", totalDuration)
	}

	fmt.Println()
	fmt.Println("   Connection Pool Performance Comparison:")
	for i, poolSize := range poolSizes {
		if i < len(poolMetrics) && poolMetrics[i].SuccessfulOperations > 0 {
			opsPerSec := float64(poolMetrics[i].SuccessfulOperations) / poolMetrics[i].TotalTime.Seconds()
			fmt.Printf("     max_conns=%d: %.2f ops/sec\n", poolSize, opsPerSec)
		}
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 2: Batch Operation Performance
	// ====================================================================
	fmt.Println("2. Batch Operation Performance Patterns")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Includes batch operation performance patterns.")
	fmt.Println()

	config := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      20,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
	}

	client, err := fdfs.NewClient(config)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	const batchSize = 100
	const batchDataSize = 5 * 1024 // 5KB per file

	// Sequential batch
	fmt.Printf("   Sequential batch upload (%d files)...\n", batchSize)
	seqMetrics := &PerformanceMetrics{}
	var seqFiles []string

	seqStart := time.Now()
	for i := 0; i < batchSize; i++ {
		opStart := time.Now()
		data := CreateTestData(batchDataSize)
		fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
		opDuration := time.Since(opStart)

		if err == nil {
			seqMetrics.RecordOperation(true, opDuration, int64(batchDataSize))
			seqFiles = append(seqFiles, fileID)
		} else {
			seqMetrics.RecordOperation(false, opDuration, 0)
		}
	}
	seqTotal := time.Since(seqStart)

	// Cleanup sequential
	for _, fileID := range seqFiles {
		client.DeleteFile(ctx, fileID)
	}

	seqMetrics.Print("Sequential Batch")
	fmt.Printf("     Total Wall Time: %v\n", seqTotal)
	fmt.Println()

	// Parallel batch
	fmt.Printf("   Parallel batch upload (%d files)...\n", batchSize)
	parMetrics := &PerformanceMetrics{}
	var parFiles []string
	var parFilesMutex sync.Mutex

	parStart := time.Now()
	var parWg sync.WaitGroup
	for i := 0; i < batchSize; i++ {
		parWg.Add(1)
		go func() {
			defer parWg.Done()
			opStart := time.Now()
			data := CreateTestData(batchDataSize)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			if err == nil {
				parMetrics.RecordOperation(true, opDuration, int64(batchDataSize))
				parFilesMutex.Lock()
				parFiles = append(parFiles, fileID)
				parFilesMutex.Unlock()
			} else {
				parMetrics.RecordOperation(false, opDuration, 0)
			}
		}()
	}
	parWg.Wait()
	parTotal := time.Since(parStart)

	// Cleanup parallel
	for _, fileID := range parFiles {
		client.DeleteFile(ctx, fileID)
	}

	parMetrics.Print("Parallel Batch")
	fmt.Printf("     Total Wall Time: %v\n", parTotal)
	fmt.Println()

	improvement := ((float64(seqTotal) / float64(parTotal)) - 1.0) * 100.0
	fmt.Printf("   Performance Improvement: %.1f%% faster (parallel)\n", improvement)
	fmt.Println()

	// ====================================================================
	// EXAMPLE 3: Memory Usage Optimization
	// ====================================================================
	fmt.Println("3. Memory Usage Optimization")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates memory usage optimization.")
	fmt.Println()

	memTracker := &MemoryStats{}
	memTracker.Start()

	// Test 1: Memory-efficient chunked processing
	fmt.Println("   Test 1: Memory-efficient chunked processing...")
	const largeFileSize = 100 * 1024 // 100KB
	const chunkSize = 10 * 1024      // 10KB chunks

	chunk := make([]byte, chunkSize)
	var chunkedFileID string

	// Upload in chunks using appender
	for offset := 0; offset < largeFileSize; offset += chunkSize {
		currentChunk := chunkSize
		if offset+chunkSize > largeFileSize {
			currentChunk = largeFileSize - offset
		}

		for i := 0; i < currentChunk; i++ {
			chunk[i] = byte((offset + i) % 256)
		}

		if offset == 0 {
			fileID, err := client.UploadAppenderBuffer(ctx, chunk[:currentChunk], "bin", nil)
			if err != nil {
				log.Printf("Failed to upload appender: %v", err)
				break
			}
			chunkedFileID = fileID
		} else {
			err := client.AppendFile(ctx, chunkedFileID, chunk[:currentChunk])
			if err != nil {
				log.Printf("Failed to append: %v", err)
				break
			}
		}

		memTracker.Update()
	}

	if chunkedFileID != "" {
		client.DeleteFile(ctx, chunkedFileID)
	}

	fmt.Printf("     → Peak memory delta: %s\n", FormatMemory(memTracker.GetPeakDelta()))
	fmt.Println()

	// Test 2: Reusing buffers
	fmt.Println("   Test 2: Buffer reuse pattern...")
	memTracker2 := &MemoryStats{}
	memTracker2.Start()

	reusableBuffer := make([]byte, 20*1024) // Reusable 20KB buffer
	var reusedFiles []string

	for i := 0; i < 10; i++ {
		// Fill buffer with different content
		for j := range reusableBuffer {
			reusableBuffer[j] = byte((i*len(reusableBuffer) + j) % 256)
		}

		fileID, err := client.UploadBuffer(ctx, reusableBuffer, "bin", nil)
		if err == nil {
			reusedFiles = append(reusedFiles, fileID)
		}
		memTracker2.Update()
	}

	for _, fileID := range reusedFiles {
		client.DeleteFile(ctx, fileID)
	}

	fmt.Printf("     → Peak memory delta: %s\n", FormatMemory(memTracker2.GetPeakDelta()))
	fmt.Printf("     → Buffer reused %d times\n", len(reusedFiles))
	fmt.Println()

	// ====================================================================
	// EXAMPLE 4: Performance Metrics Collection
	// ====================================================================
	fmt.Println("4. Performance Metrics Collection")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Shows performance metrics collection.")
	fmt.Println()

	metricsConfig := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      15,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
	}

	metricsClient, err := fdfs.NewClient(metricsConfig)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer metricsClient.Close()

	const metricsOps = 30
	detailedMetrics := &PerformanceMetrics{}
	var metricsFiles []string

	fmt.Printf("   Collecting detailed metrics for %d operations...\n", metricsOps)

	for i := 0; i < metricsOps; i++ {
		opStart := time.Now()
		data := CreateTestData(8 * 1024)
		fileID, err := metricsClient.UploadBuffer(ctx, data, "bin", nil)
		opDuration := time.Since(opStart)

		if err == nil {
			detailedMetrics.RecordOperation(true, opDuration, 8*1024)
			metricsFiles = append(metricsFiles, fileID)
		} else {
			detailedMetrics.RecordOperation(false, opDuration, 0)
		}
	}

	// Cleanup
	for _, fileID := range metricsFiles {
		metricsClient.DeleteFile(ctx, fileID)
	}

	detailedMetrics.Print("Detailed Performance Metrics")
	fmt.Println()

	// ====================================================================
	// EXAMPLE 5: Different File Size Performance
	// ====================================================================
	fmt.Println("5. Performance by File Size")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Benchmarking patterns and performance analysis.")
	fmt.Println()

	sizeConfig := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      10,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
	}

	sizeClient, err := fdfs.NewClient(sizeConfig)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer sizeClient.Close()

	testSizes := []int{1 * 1024, 10 * 1024, 100 * 1024, 500 * 1024} // 1KB, 10KB, 100KB, 500KB
	const opsPerSize = 5

	for _, testSize := range testSizes {
		fmt.Printf("   Testing with file size: %s\n", FormatMemory(uint64(testSize)))
		sizeMetrics := &PerformanceMetrics{}
		var sizeFiles []string

		for i := 0; i < opsPerSize; i++ {
			opStart := time.Now()
			data := CreateTestData(testSize)
			fileID, err := sizeClient.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			if err == nil {
				sizeMetrics.RecordOperation(true, opDuration, int64(testSize))
				sizeFiles = append(sizeFiles, fileID)
			} else {
				sizeMetrics.RecordOperation(false, opDuration, 0)
			}
		}

		// Cleanup
		for _, fileID := range sizeFiles {
			sizeClient.DeleteFile(ctx, fileID)
		}

		if sizeMetrics.SuccessfulOperations > 0 {
			avgTime := sizeMetrics.TotalTime / time.Duration(sizeMetrics.SuccessfulOperations)
			mbps := float64(testSize) / 1024.0 / 1024.0 / avgTime.Seconds()
			fmt.Printf("     → Average: %v, Throughput: %.2f MB/s\n", avgTime, mbps)
		}
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 6: Retry Policy Performance Impact
	// ====================================================================
	fmt.Println("6. Retry Policy Performance Impact")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Performance testing and optimization.")
	fmt.Println()

	retryCounts := []int{0, 1, 3, 5}
	const retryTestOps = 20

	for _, retryCount := range retryCounts {
		fmt.Printf("   Testing with retry_count = %d...\n", retryCount)

		retryConfig := &fdfs.ClientConfig{
			TrackerAddrs:  []string{trackerAddr},
			MaxConns:      10,
			ConnectTimeout: 5 * time.Second,
			NetworkTimeout: 30 * time.Second,
			RetryCount:    retryCount,
		}

		retryClient, err := fdfs.NewClient(retryConfig)
		if err != nil {
			log.Printf("Failed to create client: %v", err)
			continue
		}

		retryMetrics := &PerformanceMetrics{}
		var retryFiles []string

		retryStart := time.Now()
		for i := 0; i < retryTestOps; i++ {
			opStart := time.Now()
			data := CreateTestData(5 * 1024)
			fileID, err := retryClient.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			if err == nil {
				retryMetrics.RecordOperation(true, opDuration, 5*1024)
				retryFiles = append(retryFiles, fileID)
			} else {
				retryMetrics.RecordOperation(false, opDuration, 0)
			}
		}
		retryTotal := time.Since(retryStart)

		// Cleanup
		for _, fileID := range retryFiles {
			retryClient.DeleteFile(ctx, fileID)
		}

		retryClient.Close()

		successRate := float64(retryMetrics.SuccessfulOperations) / float64(retryTestOps) * 100.0
		fmt.Printf("     → Total time: %v, Success rate: %.1f%%\n", retryTotal, successRate)
	}
	fmt.Println()

	// ====================================================================
	// SUMMARY
	// ====================================================================
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println("Performance Example completed successfully!")
	fmt.Println()
	fmt.Println("Summary of demonstrated features:")
	fmt.Println("  ✓ Performance benchmarking and optimization")
	fmt.Println("  ✓ Connection pool tuning techniques")
	fmt.Println("  ✓ Batch operation performance patterns")
	fmt.Println("  ✓ Memory usage optimization")
	fmt.Println("  ✓ Performance metrics collection")
	fmt.Println("  ✓ Performance testing and optimization")
	fmt.Println("  ✓ Benchmarking patterns and performance analysis")
	fmt.Println()
	fmt.Println("Best Practices:")
	fmt.Println("  • Tune connection pool size based on concurrent load")
	fmt.Println("  • Use parallel operations for batch processing")
	fmt.Println("  • Process large files in chunks to limit memory usage")
	fmt.Println("  • Reuse buffers when processing multiple files")
	fmt.Println("  • Collect detailed metrics (P50, P95, P99) for analysis")
	fmt.Println("  • Monitor memory usage during operations")
	fmt.Println("  • Test different configurations to find optimal settings")
	fmt.Println("  • Balance retry count with performance requirements")
}

