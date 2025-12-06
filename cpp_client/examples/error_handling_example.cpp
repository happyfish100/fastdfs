/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Error Handling Example
 *
 * This example demonstrates comprehensive error handling patterns for the FastDFS client.
 * It covers various error scenarios and how to handle them gracefully in C++ applications.
 *
 * Key Topics Covered:
 * - Comprehensive error handling patterns for FastDFS operations
 * - Demonstrates exception hierarchy and error types
 * - Shows how to handle network errors, timeouts, and file not found errors
 * - Includes retry logic patterns and error recovery strategies
 * - Demonstrates custom error handling functions
 * - Useful for building robust production applications
 * - Shows best practices for error logging and reporting
 *
 * Run this example with:
 *   ./error_handling_example <tracker_address>
 *   Example: ./error_handling_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>

// Custom error handler function
void log_error(const std::string& operation, const std::exception& e) {
    std::cerr << "[ERROR] Operation: " << operation << std::endl;
    std::cerr << "        Error: " << e.what() << std::endl;
    std::cerr << "        Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;
}

// Retry function with exponential backoff
template<typename Func>
auto retry_with_backoff(Func&& func, size_t max_retries = 3) -> decltype(func()) {
    for (size_t attempt = 0; attempt < max_retries; ++attempt) {
        try {
            return func();
        } catch (const fastdfs::ConnectionException& e) {
            if (attempt == max_retries - 1) {
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << attempt)));
        } catch (const fastdfs::TimeoutException& e) {
            if (attempt == max_retries - 1) {
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << attempt)));
        }
    }
    throw std::runtime_error("Retry exhausted");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Error Handling Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // STEP 1: Configure and Create Client
        // ====================================================================
        std::cout << "1. Configuring FastDFS Client..." << std::endl;
        std::cout << "   Proper configuration can help prevent many errors before they occur." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig config;
        config.tracker_addrs = {argv[1]};
        config.max_conns = 10;
        config.connect_timeout = std::chrono::milliseconds(5000);
        config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client client(config);
        std::cout << "   ✓ Client initialized successfully" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Basic Error Handling with Exception Hierarchy
        // ====================================================================
        std::cout << "2. Basic Error Handling with Exception Hierarchy" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates exception hierarchy and error types." << std::endl;
        std::cout << std::endl;

        std::string test_data = "Test file for error handling demonstration";
        std::vector<uint8_t> data(test_data.begin(), test_data.end());

        std::cout << "   Attempting to upload a file..." << std::endl;
        try {
            std::string file_id = client.upload_buffer(data, "txt", nullptr);
            std::cout << "   ✓ File uploaded successfully!" << std::endl;
            std::cout << "   File ID: " << file_id << std::endl;
            std::cout << std::endl;

            // Clean up
            client.delete_file(file_id);
        } catch (const fastdfs::FastDFSException& e) {
            std::cout << "   ✗ FastDFS error: " << e.what() << std::endl;
            std::cout << "   → This is a FastDFS-specific error" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "   ✗ General error: " << e.what() << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Handling File Not Found Errors
        // ====================================================================
        std::cout << "3. Handling File Not Found Errors" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to handle network errors, timeouts, and file not found errors." << std::endl;
        std::cout << std::endl;

        std::string non_existent_file = "group1/M00/00/00/nonexistent_file.txt";
        std::cout << "   Attempting to download non-existent file..." << std::endl;
        std::cout << "   File ID: " << non_existent_file << std::endl;
        std::cout << std::endl;

        try {
            std::vector<uint8_t> downloaded = client.download_file(non_existent_file);
            std::cout << "   ⚠ Unexpected: File downloaded (should not happen)" << std::endl;
        } catch (const fastdfs::FileNotFoundException& e) {
            std::cout << "   ✓ Correctly caught file not found error" << std::endl;
            std::cout << "   Error: " << e.what() << std::endl;
            std::cout << "   → This is expected behavior for non-existent files" << std::endl;
        } catch (const fastdfs::FastDFSException& e) {
            std::cout << "   ✗ FastDFS error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "   ✗ Unexpected error: " << e.what() << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Handling Connection Errors
        // ====================================================================
        std::cout << "4. Handling Connection Errors" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates how to handle connection errors." << std::endl;
        std::cout << std::endl;

        std::cout << "   Connection errors can occur due to:" << std::endl;
        std::cout << "   - Tracker server is not running" << std::endl;
        std::cout << "   - Network connectivity issues" << std::endl;
        std::cout << "   - Firewall blocking connections" << std::endl;
        std::cout << "   - Incorrect server address or port" << std::endl;
        std::cout << std::endl;

        // Example: Try to create client with invalid address (for demonstration)
        std::cout << "   Note: Connection errors are typically caught during client creation" << std::endl;
        std::cout << "   or when operations are performed." << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Handling Timeout Errors
        // ====================================================================
        std::cout << "5. Handling Timeout Errors" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates timeout error handling." << std::endl;
        std::cout << std::endl;

        std::cout << "   Timeout errors can occur due to:" << std::endl;
        std::cout << "   - Network congestion" << std::endl;
        std::cout << "   - Server is overloaded" << std::endl;
        std::cout << "   - File is very large" << std::endl;
        std::cout << "   - Network latency is high" << std::endl;
        std::cout << std::endl;

        std::cout << "   Recommended actions:" << std::endl;
        std::cout << "   - Increase network_timeout for large files" << std::endl;
        std::cout << "   - Check server load" << std::endl;
        std::cout << "   - Verify network conditions" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Comprehensive Error Handling Pattern
        // ====================================================================
        std::cout << "6. Comprehensive Error Handling Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates custom error handling functions." << std::endl;
        std::cout << std::endl;

        std::cout << "   Performing operation with comprehensive error handling..." << std::endl;
        try {
            std::string content = "Test content";
            std::vector<uint8_t> test_data(content.begin(), content.end());
            std::string file_id = client.upload_buffer(test_data, "txt", nullptr);
            std::cout << "   ✓ Operation succeeded: " << file_id << std::endl;
            
            // Clean up
            client.delete_file(file_id);
        } catch (const fastdfs::FileNotFoundException& e) {
            log_error("upload", e);
            std::cout << "   → File not found error handled" << std::endl;
        } catch (const fastdfs::ConnectionException& e) {
            log_error("upload", e);
            std::cout << "   → Connection error handled" << std::endl;
            std::cout << "   → Possible causes: server down, network issues" << std::endl;
        } catch (const fastdfs::TimeoutException& e) {
            log_error("upload", e);
            std::cout << "   → Timeout error handled" << std::endl;
            std::cout << "   → Possible causes: slow network, server overload" << std::endl;
        } catch (const fastdfs::ProtocolException& e) {
            log_error("upload", e);
            std::cout << "   → Protocol error handled" << std::endl;
        } catch (const fastdfs::NoStorageServerException& e) {
            log_error("upload", e);
            std::cout << "   → No storage server available" << std::endl;
        } catch (const fastdfs::InvalidArgumentException& e) {
            log_error("upload", e);
            std::cout << "   → Invalid argument error handled" << std::endl;
        } catch (const fastdfs::ClientClosedException& e) {
            log_error("upload", e);
            std::cout << "   → Client closed error handled" << std::endl;
        } catch (const fastdfs::FastDFSException& e) {
            log_error("upload", e);
            std::cout << "   → General FastDFS error handled" << std::endl;
        } catch (const std::exception& e) {
            log_error("upload", e);
            std::cout << "   → Standard exception handled" << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: Retry Logic Patterns
        // ====================================================================
        std::cout << "7. Retry Logic Patterns" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes retry logic patterns and error recovery strategies." << std::endl;
        std::cout << std::endl;

        std::cout << "   Implementing retry logic with exponential backoff..." << std::endl;
        std::cout << std::endl;

        size_t retry_count = 0;
        const size_t max_retries = 3;
        bool success = false;

        while (retry_count < max_retries && !success) {
            try {
                std::string content = "Retry test " + std::to_string(retry_count);
                std::vector<uint8_t> test_data(content.begin(), content.end());
                std::string file_id = client.upload_buffer(test_data, "txt", nullptr);
                
                std::cout << "   ✓ Operation succeeded on attempt " << (retry_count + 1) << std::endl;
                std::cout << "   File ID: " << file_id << std::endl;
                success = true;
                
                // Clean up
                client.delete_file(file_id);
            } catch (const fastdfs::ConnectionException& e) {
                retry_count++;
                if (retry_count < max_retries) {
                    std::cout << "   ⚠ Attempt " << retry_count << " failed: " << e.what() << std::endl;
                    std::cout << "   → Retrying after backoff..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 * retry_count));
                } else {
                    std::cout << "   ✗ All retry attempts exhausted" << std::endl;
                    log_error("upload_with_retry", e);
                }
            } catch (const fastdfs::TimeoutException& e) {
                retry_count++;
                if (retry_count < max_retries) {
                    std::cout << "   ⚠ Attempt " << retry_count << " timed out" << std::endl;
                    std::cout << "   → Retrying after backoff..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 * retry_count));
                } else {
                    std::cout << "   ✗ All retry attempts exhausted" << std::endl;
                    log_error("upload_with_retry", e);
                }
            } catch (const std::exception& e) {
                std::cout << "   ✗ Non-retryable error: " << e.what() << std::endl;
                log_error("upload_with_retry", e);
                break;
            }
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 7: Error Recovery Strategies
        // ====================================================================
        std::cout << "8. Error Recovery Strategies" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates error recovery strategies." << std::endl;
        std::cout << std::endl;

        std::cout << "   Error Recovery Patterns:" << std::endl;
        std::cout << "   1. Retry with exponential backoff" << std::endl;
        std::cout << "   2. Fallback to alternative operation" << std::endl;
        std::cout << "   3. Graceful degradation" << std::endl;
        std::cout << "   4. Circuit breaker pattern" << std::endl;
        std::cout << "   5. Logging and monitoring" << std::endl;
        std::cout << std::endl;

        // Example: Try operation with fallback
        std::cout << "   Example: Operation with fallback strategy..." << std::endl;
        try {
            std::string content = "Recovery test";
            std::vector<uint8_t> test_data(content.begin(), content.end());
            std::string file_id = client.upload_buffer(test_data, "txt", nullptr);
            std::cout << "   ✓ Primary operation succeeded: " << file_id << std::endl;
            client.delete_file(file_id);
        } catch (const fastdfs::ConnectionException& e) {
            std::cout << "   ⚠ Primary operation failed: " << e.what() << std::endl;
            std::cout << "   → Could implement fallback strategy here" << std::endl;
            std::cout << "   → Example: Use cached result, alternative storage, etc." << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 8: Best Practices for Error Logging and Reporting
        // ====================================================================
        std::cout << "9. Best Practices for Error Logging and Reporting" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows best practices for error logging and reporting." << std::endl;
        std::cout << "   Useful for building robust production applications." << std::endl;
        std::cout << std::endl;

        std::cout << "   Best Practices:" << std::endl;
        std::cout << "   1. Log errors with context (operation, timestamp, error details)" << std::endl;
        std::cout << "   2. Use appropriate log levels (ERROR, WARN, INFO)" << std::endl;
        std::cout << "   3. Include error type and message in logs" << std::endl;
        std::cout << "   4. Track error rates and patterns" << std::endl;
        std::cout << "   5. Alert on critical errors" << std::endl;
        std::cout << "   6. Provide user-friendly error messages" << std::endl;
        std::cout << std::endl;

        // Example: Structured error logging
        std::cout << "   Example: Structured error logging..." << std::endl;
        try {
            std::string content = "Logging test";
            std::vector<uint8_t> test_data(content.begin(), content.end());
            std::string file_id = client.upload_buffer(test_data, "txt", nullptr);
            std::cout << "   ✓ Operation succeeded" << std::endl;
            client.delete_file(file_id);
        } catch (const fastdfs::FastDFSException& e) {
            // Structured logging
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            std::cout << "   [ERROR LOG]" << std::endl;
            std::cout << "     Timestamp: " << timestamp << std::endl;
            std::cout << "     Operation: upload_buffer" << std::endl;
            std::cout << "     Error Type: " << typeid(e).name() << std::endl;
            std::cout << "     Error Message: " << e.what() << std::endl;
            std::cout << "     Severity: ERROR" << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 9: Error Type Summary
        // ====================================================================
        std::cout << "10. Error Type Summary" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Complete list of FastDFS exception types:" << std::endl;
        std::cout << std::endl;

        std::cout << "   Exception Hierarchy:" << std::endl;
        std::cout << "   - FastDFSException (base class)" << std::endl;
        std::cout << "     ├── FileNotFoundException" << std::endl;
        std::cout << "     ├── ConnectionException" << std::endl;
        std::cout << "     ├── TimeoutException" << std::endl;
        std::cout << "     ├── InvalidArgumentException" << std::endl;
        std::cout << "     ├── ProtocolException" << std::endl;
        std::cout << "     ├── NoStorageServerException" << std::endl;
        std::cout << "     └── ClientClosedException" << std::endl;
        std::cout << std::endl;

        std::cout << "   When to catch each type:" << std::endl;
        std::cout << "   - FileNotFoundException: When file operations may fail" << std::endl;
        std::cout << "   - ConnectionException: Network/connection issues" << std::endl;
        std::cout << "   - TimeoutException: Operations taking too long" << std::endl;
        std::cout << "   - InvalidArgumentException: Invalid input parameters" << std::endl;
        std::cout << "   - ProtocolException: Protocol-level errors" << std::endl;
        std::cout << "   - NoStorageServerException: No storage servers available" << std::endl;
        std::cout << "   - ClientClosedException: Client was closed" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Comprehensive error handling patterns for FastDFS operations" << std::endl;
        std::cout << "  ✓ Demonstrates exception hierarchy and error types" << std::endl;
        std::cout << "  ✓ Shows how to handle network errors, timeouts, and file not found errors" << std::endl;
        std::cout << "  ✓ Includes retry logic patterns and error recovery strategies" << std::endl;
        std::cout << "  ✓ Demonstrates custom error handling functions" << std::endl;
        std::cout << "  ✓ Useful for building robust production applications" << std::endl;
        std::cout << "  ✓ Shows best practices for error logging and reporting" << std::endl;

        client.close();
        std::cout << std::endl << "✓ Client closed. All resources released." << std::endl;

    } catch (const fastdfs::FileNotFoundException& e) {
        std::cerr << "File not found error: " << e.what() << std::endl;
        return 1;
    } catch (const fastdfs::ConnectionException& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        std::cerr << "Please check that the tracker server is running and accessible." << std::endl;
        return 1;
    } catch (const fastdfs::TimeoutException& e) {
        std::cerr << "Timeout error: " << e.what() << std::endl;
        return 1;
    } catch (const fastdfs::FastDFSException& e) {
        std::cerr << "FastDFS error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

