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

// PoolStats tracks connection pool statistics
type PoolStats struct {
	TotalOperations   int64
	SuccessfulOps     int64
	FailedOps         int64
	TotalDuration     time.Duration
	AvgOperationTime  time.Duration
	Throughput        float64
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

	fmt.Println("FastDFS Go Client - Connection Pool Example")
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println()

	ctx := context.Background()

	// ====================================================================
	// EXAMPLE 1: Pool Sizing
	// ====================================================================
	fmt.Println("1. Pool Sizing")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates connection pool configuration and tuning.")
	fmt.Println()

	const numOps = 30
	const dataSize = 5 * 1024

	poolSizes := []int{1, 5, 10, 20, 50}
	var poolResults []PoolStats

	for _, poolSize := range poolSizes {
		fmt.Printf("   Testing with MaxConns = %d...\n", poolSize)

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

		var stats PoolStats
		var wg sync.WaitGroup
		var filesMutex sync.Mutex
		var uploadedFiles []string

		start := time.Now()

		for i := 0; i < numOps; i++ {
			wg.Add(1)
			go func() {
				defer wg.Done()

				opStart := time.Now()
				data := createTestData(dataSize)
				fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
				opDuration := time.Since(opStart)

				atomic.AddInt64(&stats.TotalOperations, 1)
				if err == nil {
					atomic.AddInt64(&stats.SuccessfulOps, 1)
					stats.TotalDuration += opDuration
					filesMutex.Lock()
					uploadedFiles = append(uploadedFiles, fileID)
					filesMutex.Unlock()
				} else {
					atomic.AddInt64(&stats.FailedOps, 1)
				}
			}()
		}

		wg.Wait()
		totalDuration := time.Since(start)

		// Calculate statistics
		if stats.SuccessfulOps > 0 {
			stats.AvgOperationTime = stats.TotalDuration / time.Duration(stats.SuccessfulOps)
			stats.Throughput = float64(stats.SuccessfulOps) / totalDuration.Seconds()
		}

		// Cleanup
		for _, fileID := range uploadedFiles {
			client.DeleteFile(ctx, fileID)
		}

		client.Close()
		poolResults = append(poolResults, stats)

		fmt.Printf("     → Completed in %v, Throughput: %.2f ops/sec\n",
			totalDuration, stats.Throughput)
	}

	fmt.Println()
	fmt.Println("   Pool Size Performance Comparison:")
	for i, poolSize := range poolSizes {
		if i < len(poolResults) {
			fmt.Printf("     MaxConns=%d: %.2f ops/sec (Avg: %v)\n",
				poolSize, poolResults[i].Throughput, poolResults[i].AvgOperationTime)
		}
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 2: Connection Reuse
	// ====================================================================
	fmt.Println("2. Connection Reuse")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Shows pool sizing, connection reuse, pool monitoring.")
	fmt.Println()

	fmt.Println("   Testing connection reuse with repeated operations...")

	reuseConfig := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      10,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
		IdleTimeout:   60 * time.Second,
		EnablePool:    true,
	}

	reuseClient, err := fdfs.NewClient(reuseConfig)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer reuseClient.Close()

	const reuseOps = 50
	var reuseFiles []string
	var reuseMutex sync.Mutex

	// First batch - connections are created
	fmt.Println("   First batch (connections being created)...")
	firstStart := time.Now()
	var firstWg sync.WaitGroup
	for i := 0; i < reuseOps/2; i++ {
		firstWg.Add(1)
		go func() {
			defer firstWg.Done()
			data := createTestData(3 * 1024)
			fileID, err := reuseClient.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				reuseMutex.Lock()
				reuseFiles = append(reuseFiles, fileID)
				reuseMutex.Unlock()
			}
		}()
	}
	firstWg.Wait()
	firstDuration := time.Since(firstStart)

	// Second batch - connections should be reused
	fmt.Println("   Second batch (connections being reused)...")
	secondStart := time.Now()
	var secondWg sync.WaitGroup
	for i := 0; i < reuseOps/2; i++ {
		secondWg.Add(1)
		go func() {
			defer secondWg.Done()
			data := createTestData(3 * 1024)
			fileID, err := reuseClient.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				reuseMutex.Lock()
				reuseFiles = append(reuseFiles, fileID)
				reuseMutex.Unlock()
			}
		}()
	}
	secondWg.Wait()
	secondDuration := time.Since(secondStart)

	fmt.Printf("     → First batch: %v (connections created)\n", firstDuration)
	fmt.Printf("     → Second batch: %v (connections reused)\n", secondDuration)
	if secondDuration > 0 {
		improvement := ((float64(firstDuration) / float64(secondDuration)) - 1.0) * 100.0
		if improvement > 0 {
			fmt.Printf("     → Improvement: %.1f%% faster with reused connections\n", improvement)
		}
	}

	// Cleanup
	for _, fileID := range reuseFiles {
		reuseClient.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 3: Pool Enabled vs Disabled
	// ====================================================================
	fmt.Println("3. Pool Enabled vs Disabled")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Comparing performance with pool enabled and disabled.")
	fmt.Println()

	const compareOps = 20

	// With pool enabled
	fmt.Println("   Testing with pool enabled...")
	poolEnabledConfig := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      10,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
		EnablePool:    true,
	}

	poolEnabledClient, err := fdfs.NewClient(poolEnabledConfig)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}

	var enabledWg sync.WaitGroup
	var enabledFiles []string
	var enabledMutex sync.Mutex

	enabledStart := time.Now()
	for i := 0; i < compareOps; i++ {
		enabledWg.Add(1)
		go func() {
			defer enabledWg.Done()
			data := createTestData(4 * 1024)
			fileID, err := poolEnabledClient.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				enabledMutex.Lock()
				enabledFiles = append(enabledFiles, fileID)
				enabledMutex.Unlock()
			}
		}()
	}
	enabledWg.Wait()
	enabledDuration := time.Since(enabledStart)

	// Cleanup
	for _, fileID := range enabledFiles {
		poolEnabledClient.DeleteFile(ctx, fileID)
	}
	poolEnabledClient.Close()

	// With pool disabled
	fmt.Println("   Testing with pool disabled...")
	poolDisabledConfig := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      1, // Not used when pool is disabled
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
		EnablePool:    false,
	}

	poolDisabledClient, err := fdfs.NewClient(poolDisabledConfig)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}

	var disabledWg sync.WaitGroup
	var disabledFiles []string
	var disabledMutex sync.Mutex

	disabledStart := time.Now()
	for i := 0; i < compareOps; i++ {
		disabledWg.Add(1)
		go func() {
			defer disabledWg.Done()
			data := createTestData(4 * 1024)
			fileID, err := poolDisabledClient.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				disabledMutex.Lock()
				disabledFiles = append(disabledFiles, fileID)
				disabledMutex.Unlock()
			}
		}()
	}
	disabledWg.Wait()
	disabledDuration := time.Since(disabledStart)

	// Cleanup
	for _, fileID := range disabledFiles {
		poolDisabledClient.DeleteFile(ctx, fileID)
	}
	poolDisabledClient.Close()

	fmt.Printf("     → Pool enabled: %v (%d operations)\n", enabledDuration, len(enabledFiles))
	fmt.Printf("     → Pool disabled: %v (%d operations)\n", disabledDuration, len(disabledFiles))
	if disabledDuration > 0 && enabledDuration > 0 {
		improvement := ((float64(disabledDuration) / float64(enabledDuration)) - 1.0) * 100.0
		if improvement > 0 {
			fmt.Printf("     → Pool enabled is %.1f%% faster\n", improvement)
		}
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 4: Idle Timeout Configuration
	// ====================================================================
	fmt.Println("4. Idle Timeout Configuration")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates idle timeout behavior.")
	fmt.Println()

	idleConfigs := []struct {
		name        string
		idleTimeout time.Duration
	}{
		{"Short idle timeout", 10 * time.Second},
		{"Medium idle timeout", 30 * time.Second},
		{"Long idle timeout", 120 * time.Second},
	}

	for _, cfg := range idleConfigs {
		fmt.Printf("   Testing with %s (%v)...\n", cfg.name, cfg.idleTimeout)

		config := &fdfs.ClientConfig{
			TrackerAddrs:  []string{trackerAddr},
			MaxConns:      5,
			ConnectTimeout: 5 * time.Second,
			NetworkTimeout: 30 * time.Second,
			IdleTimeout:   cfg.idleTimeout,
			EnablePool:    true,
		}

		client, err := fdfs.NewClient(config)
		if err != nil {
			log.Printf("Failed to create client: %v", err)
			continue
		}

		// Perform some operations
		var testFiles []string
		for i := 0; i < 5; i++ {
			data := createTestData(2 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				testFiles = append(testFiles, fileID)
			}
		}

		// Wait longer than idle timeout for short timeout
		if cfg.idleTimeout < 20*time.Second {
			fmt.Printf("     → Waiting %v (longer than idle timeout)...\n", cfg.idleTimeout+5*time.Second)
			time.Sleep(cfg.idleTimeout + 5*time.Second)
		}

		// Perform more operations (connections may need to be recreated)
		start := time.Now()
		for i := 0; i < 5; i++ {
			data := createTestData(2 * 1024)
			fileID, err := client.UploadBuffer(ctx, data, "bin", nil)
			if err == nil {
				testFiles = append(testFiles, fileID)
			}
		}
		duration := time.Since(start)

		// Cleanup
		for _, fileID := range testFiles {
			client.DeleteFile(ctx, fileID)
		}

		client.Close()
		fmt.Printf("     → Second batch completed in %v\n", duration)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 5: Pool Monitoring (Indirect)
	// ====================================================================
	fmt.Println("5. Pool Monitoring (Indirect)")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Monitoring pool behavior through operation metrics.")
	fmt.Println()

	monitorConfig := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      15,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
		IdleTimeout:   60 * time.Second,
		EnablePool:    true,
	}

	monitorClient, err := fdfs.NewClient(monitorConfig)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer monitorClient.Close()

	const monitorOps = 40
	var monitorWg sync.WaitGroup
	operationTimes := make([]time.Duration, 0, monitorOps)
	var timesMutex sync.Mutex
	var monitorFiles []string
	var monitorMutex sync.Mutex

	fmt.Printf("   Performing %d operations to monitor pool behavior...\n", monitorOps)

	monitorStart := time.Now()
	for i := 0; i < monitorOps; i++ {
		monitorWg.Add(1)
		go func(index int) {
			defer monitorWg.Done()

			opStart := time.Now()
			data := createTestData(3 * 1024)
			fileID, err := monitorClient.UploadBuffer(ctx, data, "bin", nil)
			opDuration := time.Since(opStart)

			if err == nil {
				timesMutex.Lock()
				operationTimes = append(operationTimes, opDuration)
				timesMutex.Unlock()

				monitorMutex.Lock()
				monitorFiles = append(monitorFiles, fileID)
				monitorMutex.Unlock()
			}
		}(i)
	}

	monitorWg.Wait()
	monitorDuration := time.Since(monitorStart)

	// Analyze operation times
	if len(operationTimes) > 0 {
		var totalTime time.Duration
		var minTime, maxTime time.Duration = operationTimes[0], operationTimes[0]

		for _, t := range operationTimes {
			totalTime += t
			if t < minTime {
				minTime = t
			}
			if t > maxTime {
				maxTime = t
			}
		}

		avgTime := totalTime / time.Duration(len(operationTimes))
		fmt.Printf("   → Total operations: %d\n", len(operationTimes))
		fmt.Printf("   → Total duration: %v\n", monitorDuration)
		fmt.Printf("   → Average operation time: %v\n", avgTime)
		fmt.Printf("   → Min operation time: %v\n", minTime)
		fmt.Printf("   → Max operation time: %v\n", maxTime)
		fmt.Printf("   → Throughput: %.2f ops/sec\n", float64(len(operationTimes))/monitorDuration.Seconds())
		fmt.Println("   → Note: Consistent operation times indicate effective connection reuse")
	}

	// Cleanup
	for _, fileID := range monitorFiles {
		monitorClient.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// SUMMARY
	// ====================================================================
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println("Connection Pool Example completed successfully!")
	fmt.Println()
	fmt.Println("Summary of demonstrated features:")
	fmt.Println("  ✓ Pool sizing (different MaxConns values)")
	fmt.Println("  ✓ Connection reuse patterns")
	fmt.Println("  ✓ Pool enabled vs disabled comparison")
	fmt.Println("  ✓ Idle timeout configuration")
	fmt.Println("  ✓ Pool monitoring through operation metrics")
	fmt.Println()
	fmt.Println("Best Practices:")
	fmt.Println("  • Set MaxConns based on expected concurrent load")
	fmt.Println("  • Enable connection pooling for better performance")
	fmt.Println("  • Configure appropriate IdleTimeout for your workload")
	fmt.Println("  • Monitor operation times to verify pool effectiveness")
	fmt.Println("  • Higher MaxConns for high-concurrency scenarios")
	fmt.Println("  • Lower MaxConns for resource-constrained environments")
}

