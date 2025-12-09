package main

import (
	"context"
	"errors"
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	fdfs "github.com/happyfish100/fastdfs/go_client"
)

// logError logs an error with context
func logError(operation string, err error) {
	log.Printf("[ERROR] Operation: %s", operation)
	log.Printf("        Error: %v", err)
	log.Printf("        Time: %v", time.Now().Format(time.RFC3339))
}

// retryWithBackoff retries an operation with exponential backoff
func retryWithBackoff(ctx context.Context, maxRetries int, operation func() error) error {
	var lastErr error
	for attempt := 0; attempt < maxRetries; attempt++ {
		if err := operation(); err == nil {
			return nil
		} else {
			lastErr = err
			// Don't retry on certain errors
			if errors.Is(err, fdfs.ErrInvalidArgument) || errors.Is(err, fdfs.ErrFileNotFound) {
				return err
			}
			// Wait before retry
			if attempt < maxRetries-1 {
				backoff := time.Duration(1<<uint(attempt)) * time.Second
				select {
				case <-ctx.Done():
					return ctx.Err()
				case <-time.After(backoff):
				}
			}
		}
	}
	return lastErr
}

// retryWithFixedDelay retries an operation with fixed delay
func retryWithFixedDelay(ctx context.Context, maxRetries int, delay time.Duration, operation func() error) error {
	var lastErr error
	for attempt := 0; attempt < maxRetries; attempt++ {
		if err := operation(); err == nil {
			return nil
		} else {
			lastErr = err
			if errors.Is(err, fdfs.ErrInvalidArgument) {
				return err
			}
			if attempt < maxRetries-1 {
				select {
				case <-ctx.Done():
					return ctx.Err()
				case <-time.After(delay):
				}
			}
		}
	}
	return lastErr
}

func main() {
	if len(os.Args) < 2 {
		log.Fatal("Usage: go run main.go <tracker_address>")
	}

	trackerAddr := os.Args[1]

	fmt.Println("FastDFS Go Client - Error Handling Example")
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println()

	ctx := context.Background()

	// ====================================================================
	// EXAMPLE 1: Basic Error Handling
	// ====================================================================
	fmt.Println("1. Basic Error Handling")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates error handling patterns and error types.")
	fmt.Println()

	config := &fdfs.ClientConfig{
		TrackerAddrs:  []string{trackerAddr},
		MaxConns:      10,
		ConnectTimeout: 5 * time.Second,
		NetworkTimeout: 30 * time.Second,
	}

	client, err := fdfs.NewClient(config)
	if err != nil {
		log.Fatalf("Failed to create client: %v", err)
	}
	defer client.Close()

	// Example 1.1: Handle file not found error
	fmt.Println("   Example 1.1: Handling file not found error")
	nonExistentFile := "group1/M00/00/00/nonexistent_file.txt"
	_, err = client.DownloadFile(ctx, nonExistentFile)
	if err != nil {
		if errors.Is(err, fdfs.ErrFileNotFound) {
			fmt.Printf("     ✓ Correctly caught file not found error: %v\n", err)
		} else {
			fmt.Printf("     ✗ Unexpected error type: %v\n", err)
		}
	}
	fmt.Println()

	// Example 1.2: Handle successful operation
	fmt.Println("   Example 1.2: Successful operation")
	data := []byte("Error handling test")
	fileID, err := client.UploadBuffer(ctx, data, "txt", nil)
	if err != nil {
		logError("upload", err)
	} else {
		fmt.Printf("     ✓ File uploaded successfully: %s\n", fileID)
		// Cleanup
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 2: Error Type Checking
	// ====================================================================
	fmt.Println("2. Error Type Checking")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Shows error types, retry strategies, error recovery.")
	fmt.Println()

	// Example 2.1: Check multiple error types
	fmt.Println("   Example 2.1: Comprehensive error type checking")
	testData := []byte("Error type test")
	uploadFileID, err := client.UploadBuffer(ctx, testData, "txt", nil)
	if err != nil {
		switch {
		case errors.Is(err, fdfs.ErrFileNotFound):
			fmt.Println("     → File not found error")
		case errors.Is(err, fdfs.ErrNoStorageServer):
			fmt.Println("     → No storage server available")
		case errors.Is(err, fdfs.ErrConnectionTimeout):
			fmt.Println("     → Connection timeout")
		case errors.Is(err, fdfs.ErrNetworkTimeout):
			fmt.Println("     → Network timeout")
		case errors.Is(err, fdfs.ErrInvalidArgument):
			fmt.Println("     → Invalid argument")
		case errors.Is(err, fdfs.ErrClientClosed):
			fmt.Println("     → Client closed")
		default:
			// Check for wrapped errors
			var netErr *fdfs.NetworkError
			if errors.As(err, &netErr) {
				fmt.Printf("     → Network error: %v\n", netErr)
			}
			var protoErr *fdfs.ProtocolError
			if errors.As(err, &protoErr) {
				fmt.Printf("     → Protocol error (code %d): %v\n", protoErr.Code, protoErr)
			}
			var storageErr *fdfs.StorageError
			if errors.As(err, &storageErr) {
				fmt.Printf("     → Storage error from %s: %v\n", storageErr.Server, storageErr)
			}
			var trackerErr *fdfs.TrackerError
			if errors.As(err, &trackerErr) {
				fmt.Printf("     → Tracker error from %s: %v\n", trackerErr.Server, trackerErr)
			}
		}
	} else {
		fmt.Printf("     ✓ Upload successful: %s\n", uploadFileID)
		// Cleanup
		client.DeleteFile(ctx, uploadFileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 3: Retry Strategies
	// ====================================================================
	fmt.Println("3. Retry Strategies")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates retry logic with different strategies.")
	fmt.Println()

	// Example 3.1: Exponential backoff retry
	fmt.Println("   Example 3.1: Exponential backoff retry")
	retryData := []byte("Retry test with exponential backoff")
	var retryFileID string
	err = retryWithBackoff(ctx, 3, func() error {
		var uploadErr error
		retryFileID, uploadErr = client.UploadBuffer(ctx, retryData, "txt", nil)
		if uploadErr != nil {
			fmt.Printf("     → Attempt failed: %v\n", uploadErr)
		}
		return uploadErr
	})
	if err != nil {
		logError("upload_with_retry", err)
	} else {
		fmt.Printf("     ✓ Upload succeeded after retries: %s\n", retryFileID)
		client.DeleteFile(ctx, retryFileID)
	}
	fmt.Println()

	// Example 3.2: Fixed delay retry
	fmt.Println("   Example 3.2: Fixed delay retry")
	fixedRetryData := []byte("Retry test with fixed delay")
	var fixedRetryFileID string
	err = retryWithFixedDelay(ctx, 3, 1*time.Second, func() error {
		var uploadErr error
		fixedRetryFileID, uploadErr = client.UploadBuffer(ctx, fixedRetryData, "txt", nil)
		if uploadErr != nil {
			fmt.Printf("     → Attempt failed: %v\n", uploadErr)
		}
		return uploadErr
	})
	if err != nil {
		logError("upload_with_fixed_retry", err)
	} else {
		fmt.Printf("     ✓ Upload succeeded: %s\n", fixedRetryFileID)
		client.DeleteFile(ctx, fixedRetryFileID)
	}
	fmt.Println()

	// Example 3.3: Conditional retry (only retry on specific errors)
	fmt.Println("   Example 3.3: Conditional retry (only retry on network errors)")
	conditionalData := []byte("Conditional retry test")
	var conditionalFileID string
	maxAttempts := 3
	for attempt := 0; attempt < maxAttempts; attempt++ {
		var uploadErr error
		conditionalFileID, uploadErr = client.UploadBuffer(ctx, conditionalData, "txt", nil)
		if uploadErr == nil {
			fmt.Printf("     ✓ Upload succeeded on attempt %d\n", attempt+1)
			break
		}

		// Only retry on network/timeout errors
		if errors.Is(uploadErr, fdfs.ErrConnectionTimeout) ||
			errors.Is(uploadErr, fdfs.ErrNetworkTimeout) {
			fmt.Printf("     → Retryable error on attempt %d: %v\n", attempt+1, uploadErr)
			if attempt < maxAttempts-1 {
				time.Sleep(time.Duration(attempt+1) * time.Second)
			}
		} else {
			fmt.Printf("     ✗ Non-retryable error: %v\n", uploadErr)
			break
		}
	}
	if conditionalFileID != "" {
		client.DeleteFile(ctx, conditionalFileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 4: Error Recovery
	// ====================================================================
	fmt.Println("4. Error Recovery")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates error recovery and graceful degradation.")
	fmt.Println()

	// Example 4.1: Fallback strategy
	fmt.Println("   Example 4.1: Fallback strategy")
	primaryData := []byte("Primary upload attempt")
	fileID, err := client.UploadBuffer(ctx, primaryData, "txt", nil)
	if err != nil {
		fmt.Printf("     ⚠ Primary upload failed: %v\n", err)
		fmt.Println("     → Implementing fallback strategy...")
		// Fallback: try with different configuration or alternative method
		// For demonstration, we'll just log the fallback
		fmt.Println("     → Fallback: Could use alternative storage, cached result, etc.")
	} else {
		fmt.Printf("     ✓ Primary upload succeeded: %s\n", fileID)
		client.DeleteFile(ctx, fileID)
	}
	fmt.Println()

	// Example 4.2: Partial recovery
	fmt.Println("   Example 4.2: Partial recovery pattern")
	recoveryData := []byte("Recovery test data")
	recoveryFileID, err := client.UploadBuffer(ctx, recoveryData, "txt", nil)
	if err != nil {
		fmt.Printf("     ⚠ Upload failed: %v\n", err)
		fmt.Println("     → Attempting recovery...")
		// Try alternative approach
		time.Sleep(500 * time.Millisecond)
		recoveryFileID, err = client.UploadBuffer(ctx, recoveryData, "txt", nil)
		if err != nil {
			fmt.Printf("     ✗ Recovery failed: %v\n", err)
		} else {
			fmt.Printf("     ✓ Recovery succeeded: %s\n", recoveryFileID)
			client.DeleteFile(ctx, recoveryFileID)
		}
	} else {
		fmt.Printf("     ✓ Upload succeeded: %s\n", recoveryFileID)
		client.DeleteFile(ctx, recoveryFileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 5: Graceful Degradation
	// ====================================================================
	fmt.Println("5. Graceful Degradation")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Shows graceful degradation patterns.")
	fmt.Println()

	// Example 5.1: Degrade functionality on error
	fmt.Println("   Example 5.1: Degrade functionality on error")
	degradeData := []byte("Graceful degradation test")
	degradeFileID, err := client.UploadBuffer(ctx, degradeData, "txt", nil)
	if err != nil {
		fmt.Printf("     ⚠ Upload failed: %v\n", err)
		fmt.Println("     → Degrading to read-only mode or cached operations")
		fmt.Println("     → Application continues with limited functionality")
	} else {
		fmt.Printf("     ✓ Upload succeeded: %s\n", degradeFileID)
		// Simulate metadata operation failure
		metadata := map[string]string{"key": "value"}
		err = client.SetMetadata(ctx, degradeFileID, metadata, fdfs.MetadataOverwrite)
		if err != nil {
			fmt.Printf("     ⚠ Metadata operation failed: %v\n", err)
			fmt.Println("     → Continuing without metadata (graceful degradation)")
		} else {
			fmt.Println("     ✓ Metadata operation succeeded")
		}
		client.DeleteFile(ctx, degradeFileID)
	}
	fmt.Println()

	// Example 5.2: Circuit breaker pattern (simplified)
	fmt.Println("   Example 5.2: Circuit breaker pattern (simplified)")
	failureCount := 0
	const failureThreshold = 3

	for i := 0; i < 5; i++ {
		testData := []byte(fmt.Sprintf("Circuit breaker test %d", i))
		_, err := client.UploadBuffer(ctx, testData, "txt", nil)
		if err != nil {
			failureCount++
			fmt.Printf("     → Operation %d failed (failures: %d/%d)\n", i+1, failureCount, failureThreshold)
			if failureCount >= failureThreshold {
				fmt.Println("     → Circuit breaker opened: stopping operations")
				break
			}
		} else {
			failureCount = 0 // Reset on success
			fmt.Printf("     ✓ Operation %d succeeded\n", i+1)
		}
		time.Sleep(100 * time.Millisecond)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 6: Context-Based Error Handling
	// ====================================================================
	fmt.Println("6. Context-Based Error Handling")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates error handling with context cancellation.")
	fmt.Println()

	// Example 6.1: Timeout context
	fmt.Println("   Example 6.1: Timeout context")
	timeoutCtx, cancel := context.WithTimeout(ctx, 2*time.Second)
	defer cancel()

	timeoutData := []byte("Timeout test")
	_, err = client.UploadBuffer(timeoutCtx, timeoutData, "txt", nil)
	if err != nil {
		if errors.Is(err, context.DeadlineExceeded) {
			fmt.Printf("     ✓ Correctly caught timeout: %v\n", err)
		} else if errors.Is(err, context.Canceled) {
			fmt.Printf("     → Context canceled: %v\n", err)
		} else {
			fmt.Printf("     → Other error: %v\n", err)
		}
	} else {
		fmt.Println("     ✓ Upload completed within timeout")
	}
	fmt.Println()

	// Example 6.2: Cancellation context
	fmt.Println("   Example 6.2: Cancellation context")
	cancelCtx, cancelFunc := context.WithCancel(ctx)
	
	// Cancel after a short delay
	go func() {
		time.Sleep(500 * time.Millisecond)
		cancelFunc()
	}()

	cancelData := []byte("Cancellation test")
	_, err = client.UploadBuffer(cancelCtx, cancelData, "txt", nil)
	if err != nil {
		if errors.Is(err, context.Canceled) {
			fmt.Printf("     ✓ Correctly caught cancellation: %v\n", err)
		} else {
			fmt.Printf("     → Other error: %v\n", err)
		}
	} else {
		fmt.Println("     ✓ Upload completed before cancellation")
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 7: Error Wrapping and Unwrapping
	// ====================================================================
	fmt.Println("7. Error Wrapping and Unwrapping")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Demonstrates error wrapping and unwrapping patterns.")
	fmt.Println()

	// Example 7.1: Check wrapped errors
	fmt.Println("   Example 7.1: Checking wrapped errors")
	wrapData := []byte("Error wrapping test")
	wrapFileID, err := client.UploadBuffer(ctx, wrapData, "txt", nil)
	if err != nil {
		// Check for NetworkError
		var netErr *fdfs.NetworkError
		if errors.As(err, &netErr) {
			fmt.Printf("     → Unwrapped NetworkError: Op=%s, Addr=%s, Err=%v\n",
				netErr.Op, netErr.Addr, netErr.Err)
		}

		// Check for StorageError
		var storageErr *fdfs.StorageError
		if errors.As(err, &storageErr) {
			fmt.Printf("     → Unwrapped StorageError: Server=%s, Err=%v\n",
				storageErr.Server, storageErr.Err)
		}

		// Check for TrackerError
		var trackerErr *fdfs.TrackerError
		if errors.As(err, &trackerErr) {
			fmt.Printf("     → Unwrapped TrackerError: Server=%s, Err=%v\n",
				trackerErr.Server, trackerErr.Err)
		}

		// Check for ProtocolError
		var protoErr *fdfs.ProtocolError
		if errors.As(err, &protoErr) {
			fmt.Printf("     → Unwrapped ProtocolError: Code=%d, Message=%s\n",
				protoErr.Code, protoErr.Message)
		}
	} else {
		fmt.Printf("     ✓ Upload succeeded: %s\n", wrapFileID)
		client.DeleteFile(ctx, wrapFileID)
	}
	fmt.Println()

	// ====================================================================
	// EXAMPLE 8: Comprehensive Error Handling Pattern
	// ====================================================================
	fmt.Println("8. Comprehensive Error Handling Pattern")
	fmt.Println(strings.Repeat("-", 70))
	fmt.Println("   Complete error handling pattern for production use.")
	fmt.Println()

	comprehensiveData := []byte("Comprehensive error handling test")
	comprehensiveFileID, err := client.UploadBuffer(ctx, comprehensiveData, "txt", nil)
	if err != nil {
		// Comprehensive error handling
		switch {
		case errors.Is(err, fdfs.ErrFileNotFound):
			logError("upload", err)
			fmt.Println("     → Action: Check file path and permissions")
		case errors.Is(err, fdfs.ErrNoStorageServer):
			logError("upload", err)
			fmt.Println("     → Action: Check tracker server status")
		case errors.Is(err, fdfs.ErrConnectionTimeout):
			logError("upload", err)
			fmt.Println("     → Action: Increase ConnectTimeout or check network")
		case errors.Is(err, fdfs.ErrNetworkTimeout):
			logError("upload", err)
			fmt.Println("     → Action: Increase NetworkTimeout or check network speed")
		case errors.Is(err, fdfs.ErrInvalidArgument):
			logError("upload", err)
			fmt.Println("     → Action: Validate input parameters")
		case errors.Is(err, fdfs.ErrClientClosed):
			logError("upload", err)
			fmt.Println("     → Action: Recreate client")
		case errors.Is(err, context.DeadlineExceeded):
			logError("upload", err)
			fmt.Println("     → Action: Increase timeout or optimize operation")
		case errors.Is(err, context.Canceled):
			logError("upload", err)
			fmt.Println("     → Action: Operation was cancelled")
		default:
			// Check for wrapped errors
			var netErr *fdfs.NetworkError
			if errors.As(err, &netErr) {
				logError("upload", err)
				fmt.Printf("     → Action: Network issue during %s to %s\n", netErr.Op, netErr.Addr)
			} else {
				logError("upload", err)
				fmt.Println("     → Action: Unknown error, check logs")
			}
		}
	} else {
		fmt.Printf("     ✓ Upload succeeded: %s\n", comprehensiveFileID)
		client.DeleteFile(ctx, comprehensiveFileID)
	}
	fmt.Println()

	// ====================================================================
	// SUMMARY
	// ====================================================================
	fmt.Println(strings.Repeat("=", 70))
	fmt.Println("Error Handling Example completed successfully!")
	fmt.Println()
	fmt.Println("Summary of demonstrated features:")
	fmt.Println("  ✓ Error types and error checking")
	fmt.Println("  ✓ Retry strategies (exponential backoff, fixed delay, conditional)")
	fmt.Println("  ✓ Error recovery patterns")
	fmt.Println("  ✓ Graceful degradation")
	fmt.Println("  ✓ Context-based error handling")
	fmt.Println("  ✓ Error wrapping and unwrapping")
	fmt.Println("  ✓ Comprehensive error handling patterns")
	fmt.Println()
	fmt.Println("Best Practices:")
	fmt.Println("  • Use errors.Is() for sentinel error checking")
	fmt.Println("  • Use errors.As() for error type checking")
	fmt.Println("  • Implement retry logic for transient errors")
	fmt.Println("  • Use exponential backoff for retries")
	fmt.Println("  • Don't retry on non-retryable errors (InvalidArgument, FileNotFound)")
	fmt.Println("  • Implement graceful degradation for resilience")
	fmt.Println("  • Use context for cancellation and timeouts")
	fmt.Println("  • Log errors with context for debugging")
	fmt.Println("  • Unwrap errors to get underlying error details")
}

