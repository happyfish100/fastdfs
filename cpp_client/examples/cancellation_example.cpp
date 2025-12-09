/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Cancellation Example
 *
 * This comprehensive example demonstrates how to cancel long-running operations,
 * handle cancellation tokens, implement timeout-based cancellation, and perform
 * graceful shutdown with proper resource cleanup.
 *
 * Key Topics Covered:
 * - Demonstrates how to cancel long-running operations
 * - Shows cancellation token patterns and interrupt handling
 * - Includes examples for timeout-based cancellation
 * - Demonstrates graceful shutdown of operations
 * - Useful for user-initiated cancellations and timeout handling
 * - Shows how to clean up resources after cancellation
 *
 * Run this example with:
 *   ./cancellation_example <tracker_address>
 *   Example: ./cancellation_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <future>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <mutex>

// Simple cancellation token class
class CancellationToken {
public:
    CancellationToken() : cancelled_(false) {}
    
    void cancel() {
        cancelled_.store(true);
    }
    
    bool is_cancelled() const {
        return cancelled_.load();
    }
    
    void reset() {
        cancelled_.store(false);
    }
    
private:
    std::atomic<bool> cancelled_;
};

// Helper function to create a test file
void create_test_file(const std::string& filename, int64_t size) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create test file: " + filename);
    }
    
    const int64_t chunk_size = 1024 * 1024; // 1MB chunks
    std::vector<uint8_t> chunk(chunk_size);
    
    for (int64_t i = 0; i < size; i += chunk_size) {
        int64_t write_size = std::min(chunk_size, size - i);
        for (int64_t j = 0; j < write_size; ++j) {
            chunk[j] = static_cast<uint8_t>((i + j) % 256);
        }
        file.write(reinterpret_cast<const char*>(chunk.data()), write_size);
    }
    
    file.close();
}

// Helper function to format duration
std::string format_duration(std::chrono::milliseconds ms) {
    if (ms.count() < 1000) {
        return std::to_string(ms.count()) + " ms";
    } else {
        return std::to_string(ms.count() / 1000.0) + " s";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Cancellation Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // STEP 1: Initialize Client
        // ====================================================================
        std::cout << "1. Initializing FastDFS Client..." << std::endl;
        fastdfs::ClientConfig config;
        config.tracker_addrs = {argv[1]};
        config.max_conns = 10;
        config.connect_timeout = std::chrono::milliseconds(5000);
        config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client client(config);
        std::cout << "   ✓ Client initialized successfully" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Cancellation Token Pattern
        // ====================================================================
        std::cout << "2. Cancellation Token Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates cancellation token patterns and interrupt handling." << std::endl;
        std::cout << std::endl;

        CancellationToken cancel_token;
        
        // Create a test file for upload
        const std::string test_file = "cancellation_test.bin";
        const int64_t file_size = 100 * 1024; // 100KB
        create_test_file(test_file, file_size);
        std::cout << "   Created test file: " << test_file << " (" << file_size << " bytes)" << std::endl;
        std::cout << std::endl;

        // Start upload in a separate thread
        std::cout << "   Starting upload operation..." << std::endl;
        std::atomic<bool> upload_completed(false);
        std::string uploaded_file_id;
        std::exception_ptr upload_exception = nullptr;

        auto upload_future = std::async(std::launch::async, [&]() {
            try {
                // Check cancellation before starting
                if (cancel_token.is_cancelled()) {
                    throw std::runtime_error("Operation cancelled before start");
                }
                
                // Perform upload
                std::string file_id = client.upload_file(test_file, nullptr);
                uploaded_file_id = file_id;
                upload_completed.store(true);
                return file_id;
            } catch (...) {
                upload_exception = std::current_exception();
                upload_completed.store(true);
                throw;
            }
        });

        // Simulate cancellation after a short delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "   → Cancelling operation..." << std::endl;
        cancel_token.cancel();

        // Wait for upload to complete (or timeout)
        auto status = upload_future.wait_for(std::chrono::seconds(5));
        
        if (status == std::future_status::ready) {
            try {
                std::string file_id = upload_future.get();
                std::cout << "   ⚠ Upload completed before cancellation: " << file_id << std::endl;
                std::cout << "   → Note: FastDFS operations are synchronous and cannot be" << std::endl;
                std::cout << "     cancelled mid-operation. Cancellation should be checked" << std::endl;
                std::cout << "     between operations or using timeout mechanisms." << std::endl;
                
                // Clean up
                client.delete_file(file_id);
            } catch (...) {
                if (upload_exception) {
                    try {
                        std::rethrow_exception(upload_exception);
                    } catch (const std::exception& e) {
                        std::cout << "   → Upload failed: " << e.what() << std::endl;
                    }
                }
            }
        } else {
            std::cout << "   → Upload still in progress (would need timeout mechanism)" << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Timeout-Based Cancellation
        // ====================================================================
        std::cout << "3. Timeout-Based Cancellation" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes examples for timeout-based cancellation." << std::endl;
        std::cout << std::endl;

        // Create client with short timeout for demonstration
        std::cout << "   Creating client with short timeout (5 seconds)..." << std::endl;
        fastdfs::ClientConfig timeout_config;
        timeout_config.tracker_addrs = {argv[1]};
        timeout_config.max_conns = 10;
        timeout_config.connect_timeout = std::chrono::milliseconds(5000);
        timeout_config.network_timeout = std::chrono::milliseconds(5000); // 5 second timeout

        fastdfs::Client timeout_client(timeout_config);
        std::cout << "   ✓ Client with timeout configured" << std::endl;
        std::cout << std::endl;

        // Demonstrate timeout handling
        std::cout << "   Attempting operation with timeout protection..." << std::endl;
        auto timeout_start = std::chrono::high_resolution_clock::now();
        
        try {
            // This will use the configured network_timeout
            std::string content = "Timeout test";
            std::vector<uint8_t> data(content.begin(), content.end());
            std::string file_id = timeout_client.upload_buffer(data, "txt", nullptr);
            
            auto timeout_end = std::chrono::high_resolution_clock::now();
            auto timeout_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                timeout_end - timeout_start);
            
            std::cout << "   ✓ Operation completed in " << format_duration(timeout_duration) << std::endl;
            std::cout << "   File ID: " << file_id << std::endl;
            
            // Clean up
            timeout_client.delete_file(file_id);
        } catch (const fastdfs::TimeoutException& e) {
            auto timeout_end = std::chrono::high_resolution_clock::now();
            auto timeout_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                timeout_end - timeout_start);
            
            std::cout << "   ✓ Timeout occurred after " << format_duration(timeout_duration) << std::endl;
            std::cout << "   Error: " << e.what() << std::endl;
            std::cout << "   → Operation was automatically cancelled due to timeout" << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: User-Initiated Cancellation
        // ====================================================================
        std::cout << "4. User-Initiated Cancellation" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Useful for user-initiated cancellations and timeout handling." << std::endl;
        std::cout << std::endl;

        CancellationToken user_cancel_token;
        std::atomic<bool> operation_running(false);
        std::atomic<bool> operation_cancelled(false);

        // Simulate a long-running chunked operation
        std::cout << "   Simulating long-running chunked upload operation..." << std::endl;
        std::cout << "   (In real scenario, user could press Ctrl+C or click Cancel)" << std::endl;
        std::cout << std::endl;

        const std::string chunked_file = "chunked_upload_test.bin";
        const int64_t chunked_size = 200 * 1024; // 200KB
        create_test_file(chunked_file, chunked_size);

        auto chunked_upload_future = std::async(std::launch::async, [&]() {
            operation_running.store(true);
            try {
                // Simulate chunked upload with cancellation checks
                const int64_t chunk_size = 32 * 1024; // 32KB chunks
                std::ifstream file_stream(chunked_file, std::ios::binary);
                
                if (!file_stream) {
                    throw std::runtime_error("Failed to open file");
                }

                // Upload first chunk to create appender file
                std::vector<uint8_t> chunk(chunk_size);
                file_stream.read(reinterpret_cast<char*>(chunk.data()), chunk_size);
                std::streamsize bytes_read = file_stream.gcount();
                
                if (user_cancel_token.is_cancelled()) {
                    throw std::runtime_error("Operation cancelled by user");
                }
                
                std::string file_id = client.upload_appender_buffer(
                    std::vector<uint8_t>(chunk.begin(), chunk.begin() + bytes_read),
                    "bin", nullptr);
                
                int64_t uploaded = bytes_read;
                
                // Continue uploading chunks with cancellation checks
                while (file_stream.read(reinterpret_cast<char*>(chunk.data()), chunk_size)) {
                    if (user_cancel_token.is_cancelled()) {
                        std::cout << "   → Cancellation detected during chunk upload" << std::endl;
                        operation_cancelled.store(true);
                        // Clean up partial upload
                        client.delete_file(file_id);
                        throw std::runtime_error("Operation cancelled by user");
                    }
                    
                    bytes_read = file_stream.gcount();
                    client.append_file(file_id, 
                        std::vector<uint8_t>(chunk.begin(), chunk.begin() + bytes_read));
                    uploaded += bytes_read;
                    
                    // Simulate processing time
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                
                file_stream.close();
                operation_running.store(false);
                return file_id;
            } catch (...) {
                operation_running.store(false);
                throw;
            }
        });

        // Simulate user cancellation after 300ms
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::cout << "   → User initiated cancellation..." << std::endl;
        user_cancel_token.cancel();

        // Wait for operation
        auto chunked_status = chunked_upload_future.wait_for(std::chrono::seconds(10));
        
        if (chunked_status == std::future_status::ready) {
            try {
                std::string file_id = chunked_upload_future.get();
                if (!operation_cancelled.load()) {
                    std::cout << "   ⚠ Operation completed before cancellation" << std::endl;
                    std::cout << "   File ID: " << file_id << std::endl;
                    client.delete_file(file_id);
                }
            } catch (const std::exception& e) {
                std::cout << "   ✓ Operation cancelled: " << e.what() << std::endl;
                std::cout << "   → Resources cleaned up properly" << std::endl;
            }
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Graceful Shutdown Pattern
        // ====================================================================
        std::cout << "5. Graceful Shutdown Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates graceful shutdown of operations." << std::endl;
        std::cout << std::endl;

        std::atomic<bool> shutdown_requested(false);
        std::vector<std::thread> worker_threads;
        std::mutex output_mutex;

        // Simulate multiple worker threads
        std::cout << "   Starting 3 worker threads..." << std::endl;
        for (int i = 0; i < 3; ++i) {
            worker_threads.emplace_back([&, i]() {
                int operation_count = 0;
                while (!shutdown_requested.load()) {
                    try {
                        // Simulate work
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        
                        if (shutdown_requested.load()) {
                            break;
                        }
                        
                        // Perform operation
                        std::string content = "Worker " + std::to_string(i) + " operation " + 
                                            std::to_string(operation_count);
                        std::vector<uint8_t> data(content.begin(), content.end());
                        std::string file_id = client.upload_buffer(data, "txt", nullptr);
                        
                        {
                            std::lock_guard<std::mutex> lock(output_mutex);
                            std::cout << "   → Worker " << i << " completed operation " 
                                     << operation_count << std::endl;
                        }
                        
                        // Clean up immediately
                        client.delete_file(file_id);
                        operation_count++;
                        
                        // Limit operations for demo
                        if (operation_count >= 5) {
                            break;
                        }
                    } catch (const std::exception& e) {
                        if (!shutdown_requested.load()) {
                            std::lock_guard<std::mutex> lock(output_mutex);
                            std::cout << "   → Worker " << i << " error: " << e.what() << std::endl;
                        }
                        break;
                    }
                }
                
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "   → Worker " << i << " shutting down gracefully" << std::endl;
            });
        }

        // Let workers run for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Request graceful shutdown
        std::cout << "   → Requesting graceful shutdown..." << std::endl;
        shutdown_requested.store(true);

        // Wait for all workers to finish
        for (auto& thread : worker_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        std::cout << "   ✓ All workers shut down gracefully" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Resource Cleanup After Cancellation
        // ====================================================================
        std::cout << "6. Resource Cleanup After Cancellation" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to clean up resources after cancellation." << std::endl;
        std::cout << std::endl;

        std::vector<std::string> uploaded_files;
        CancellationToken cleanup_cancel_token;

        std::cout << "   Starting batch upload operation..." << std::endl;
        
        auto batch_upload_future = std::async(std::launch::async, [&]() {
            try {
                for (int i = 0; i < 10; ++i) {
                    if (cleanup_cancel_token.is_cancelled()) {
                        std::cout << "   → Cancellation detected, cleaning up..." << std::endl;
                        break;
                    }
                    
                    std::string content = "Batch file " + std::to_string(i);
                    std::vector<uint8_t> data(content.begin(), content.end());
                    std::string file_id = client.upload_buffer(data, "txt", nullptr);
                    uploaded_files.push_back(file_id);
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (const std::exception& e) {
                std::cout << "   → Error during batch upload: " << e.what() << std::endl;
            }
        });

        // Cancel after some operations
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::cout << "   → Cancelling batch operation..." << std::endl;
        cleanup_cancel_token.cancel();

        // Wait for operation
        batch_upload_future.wait();

        // Clean up uploaded files
        std::cout << "   Cleaning up " << uploaded_files.size() << " uploaded files..." << std::endl;
        for (const auto& file_id : uploaded_files) {
            try {
                client.delete_file(file_id);
            } catch (const std::exception& e) {
                std::cout << "   → Warning: Failed to delete " << file_id << ": " << e.what() << std::endl;
            }
        }
        std::cout << "   ✓ Resources cleaned up successfully" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: Cancellation with Future and Promise
        // ====================================================================
        std::cout << "7. Cancellation with Future and Promise" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Advanced pattern using std::promise for cancellation." << std::endl;
        std::cout << std::endl;

        std::promise<void> cancellation_promise;
        std::future<void> cancellation_future = cancellation_promise.get_future();

        std::cout << "   Starting cancellable operation..." << std::endl;
        
        auto advanced_future = std::async(std::launch::async, [&]() {
            try {
                // Check cancellation before starting
                if (cancellation_future.wait_for(std::chrono::milliseconds(0)) == 
                    std::future_status::ready) {
                    throw std::runtime_error("Operation cancelled");
                }

                // Perform work with periodic cancellation checks
                for (int i = 0; i < 10; ++i) {
                    // Check for cancellation
                    if (cancellation_future.wait_for(std::chrono::milliseconds(0)) == 
                        std::future_status::ready) {
                        throw std::runtime_error("Operation cancelled");
                    }
                    
                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                std::string content = "Advanced cancellation test";
                std::vector<uint8_t> data(content.begin(), content.end());
                return client.upload_buffer(data, "txt", nullptr);
            } catch (const std::exception&) {
                throw;
            }
        });

        // Cancel after delay
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::cout << "   → Sending cancellation signal..." << std::endl;
        cancellation_promise.set_value();

        // Wait for operation
        try {
            auto status = advanced_future.wait_for(std::chrono::seconds(2));
            if (status == std::future_status::ready) {
                try {
                    std::string file_id = advanced_future.get();
                    std::cout << "   ⚠ Operation completed: " << file_id << std::endl;
                    client.delete_file(file_id);
                } catch (const std::exception& e) {
                    std::cout << "   ✓ Operation cancelled: " << e.what() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "   → Error: " << e.what() << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // CLEANUP
        // ====================================================================
        std::cout << "8. Cleaning up test files..." << std::endl;
        std::remove(test_file.c_str());
        std::remove(chunked_file.c_str());
        std::cout << "   ✓ Local test files cleaned up" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ How to cancel long-running operations" << std::endl;
        std::cout << "  ✓ Cancellation token patterns and interrupt handling" << std::endl;
        std::cout << "  ✓ Timeout-based cancellation" << std::endl;
        std::cout << "  ✓ Graceful shutdown of operations" << std::endl;
        std::cout << "  ✓ User-initiated cancellations and timeout handling" << std::endl;
        std::cout << "  ✓ How to clean up resources after cancellation" << std::endl;
        std::cout << std::endl;
        std::cout << "Best Practices:" << std::endl;
        std::cout << "  • Use std::atomic<bool> for cancellation tokens" << std::endl;
        std::cout << "  • Check cancellation status between operations" << std::endl;
        std::cout << "  • Configure appropriate timeouts in ClientConfig" << std::endl;
        std::cout << "  • Always clean up resources in exception handlers" << std::endl;
        std::cout << "  • Use RAII patterns for automatic cleanup" << std::endl;
        std::cout << "  • Implement graceful shutdown for long-running processes" << std::endl;
        std::cout << "  • Use std::future and std::async for cancellable operations" << std::endl;

        client.close();
        timeout_client.close();
        std::cout << std::endl << "✓ Clients closed. All resources released." << std::endl;

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

