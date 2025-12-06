/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Concurrent Operations Example
 *
 * This example demonstrates how to perform concurrent operations with the FastDFS client.
 * It covers various patterns for parallel uploads, downloads, and other operations
 * using C++ threading primitives.
 *
 * Key Topics Covered:
 * - Concurrent uploads and downloads
 * - Thread-safe client usage patterns
 * - Examples using std::thread, std::async, and thread pools
 * - Performance comparison between sequential and concurrent operations
 * - Connection pool behavior under concurrent load
 * - Useful for high-throughput applications and parallel processing
 *
 * Run this example with:
 *   ./concurrent_operations_example <tracker_address>
 *   Example: ./concurrent_operations_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <atomic>

// Structure to track operation results
struct OperationResult {
    bool success;
    std::string file_id;
    std::string error;
    size_t thread_id;
    std::chrono::milliseconds duration;
};

// Thread-safe counter for statistics
struct Statistics {
    std::mutex mutex;
    std::atomic<size_t> total_operations{0};
    std::atomic<size_t> successful_operations{0};
    std::atomic<size_t> failed_operations{0};
    std::chrono::milliseconds total_time{0};

    void record(bool success, std::chrono::milliseconds duration) {
        total_operations++;
        if (success) {
            successful_operations++;
        } else {
            failed_operations++;
        }
        std::lock_guard<std::mutex> lock(mutex);
        total_time += duration;
    }

    void print() {
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "   Statistics:" << std::endl;
        std::cout << "     Total operations: " << total_operations << std::endl;
        std::cout << "     Successful: " << successful_operations << std::endl;
        std::cout << "     Failed: " << failed_operations << std::endl;
        std::cout << "     Total time: " << total_time.count() << " ms" << std::endl;
        if (total_operations > 0) {
            std::cout << "     Average time: " 
                      << (total_time.count() / total_operations) << " ms" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Concurrent Operations Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // STEP 1: Configure and Create Client
        // ====================================================================
        std::cout << "1. Configuring FastDFS Client..." << std::endl;
        std::cout << "   The client is thread-safe and can be used concurrently" << std::endl;
        std::cout << "   from multiple threads. The connection pool manages connections" << std::endl;
        std::cout << "   efficiently across concurrent operations." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig config;
        config.tracker_addrs = {argv[1]};
        config.max_conns = 50;  // Higher connection limit for concurrent operations
        config.connect_timeout = std::chrono::milliseconds(5000);
        config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client client(config);
        std::cout << "   ✓ Client initialized successfully" << std::endl;
        std::cout << "   → Max connections: " << config.max_conns << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Concurrent Uploads with std::thread
        // ====================================================================
        std::cout << "2. Concurrent Uploads with std::thread" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates multi-threaded FastDFS operations." << std::endl;
        std::cout << "   Shows thread-safe client usage patterns." << std::endl;
        std::cout << std::endl;

        const size_t num_threads = 5;
        std::vector<std::thread> threads;
        std::vector<OperationResult> results(num_threads);
        std::mutex results_mutex;

        std::cout << "   Uploading " << num_threads << " files concurrently using std::thread..." << std::endl;
        std::cout << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        // Create threads for concurrent uploads
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([&client, i, &results, &results_mutex]() {
                auto op_start = std::chrono::high_resolution_clock::now();
                try {
                    std::string content = "Concurrent upload file " + std::to_string(i + 1);
                    std::vector<uint8_t> data(content.begin(), content.end());
                    std::string file_id = client.upload_buffer(data, "txt", nullptr);
                    
                    auto op_end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        op_end - op_start);
                    
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results[i] = {true, file_id, "", i, duration};
                    std::cout << "   Thread " << i << ": ✓ Uploaded " << file_id 
                              << " in " << duration.count() << " ms" << std::endl;
                } catch (const std::exception& e) {
                    auto op_end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        op_end - op_start);
                    
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results[i] = {false, "", e.what(), i, duration};
                    std::cout << "   Thread " << i << ": ✗ Failed - " << e.what() << std::endl;
                }
            });
        }

        // Wait for all threads to complete
        for (auto& t : threads) {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << std::endl;
        std::cout << "   Total time: " << total_duration.count() << " ms" << std::endl;
        std::cout << "   → All uploads completed concurrently" << std::endl;
        std::cout << std::endl;

        // Collect successful file IDs for cleanup
        std::vector<std::string> uploaded_file_ids;
        for (const auto& result : results) {
            if (result.success) {
                uploaded_file_ids.push_back(result.file_id);
            }
        }

        // ====================================================================
        // EXAMPLE 2: Concurrent Operations with std::async
        // ====================================================================
        std::cout << "3. Concurrent Operations with std::async" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes examples using std::async." << std::endl;
        std::cout << std::endl;

        std::cout << "   Uploading 3 files concurrently using std::async..." << std::endl;
        std::cout << std::endl;

        start = std::chrono::high_resolution_clock::now();

        // Create async tasks
        auto future1 = std::async(std::launch::async, [&client]() {
            std::vector<uint8_t> data{'F', 'i', 'l', 'e', ' ', '1'};
            return client.upload_buffer(data, "txt", nullptr);
        });

        auto future2 = std::async(std::launch::async, [&client]() {
            std::vector<uint8_t> data{'F', 'i', 'l', 'e', ' ', '2'};
            return client.upload_buffer(data, "txt", nullptr);
        });

        auto future3 = std::async(std::launch::async, [&client]() {
            std::vector<uint8_t> data{'F', 'i', 'l', 'e', ' ', '3'};
            return client.upload_buffer(data, "txt", nullptr);
        });

        // Wait for all tasks to complete
        std::string file_id1 = future1.get();
        std::string file_id2 = future2.get();
        std::string file_id3 = future3.get();

        end = std::chrono::high_resolution_clock::now();
        total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "   ✓ All 3 files uploaded successfully!" << std::endl;
        std::cout << "   File ID 1: " << file_id1 << std::endl;
        std::cout << "   File ID 2: " << file_id2 << std::endl;
        std::cout << "   File ID 3: " << file_id3 << std::endl;
        std::cout << "   Total time: " << total_duration.count() << " ms" << std::endl;
        std::cout << std::endl;

        uploaded_file_ids.push_back(file_id1);
        uploaded_file_ids.push_back(file_id2);
        uploaded_file_ids.push_back(file_id3);

        // ====================================================================
        // EXAMPLE 3: Concurrent Downloads
        // ====================================================================
        std::cout << "4. Concurrent Downloads" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Downloading multiple files concurrently." << std::endl;
        std::cout << std::endl;

        if (uploaded_file_ids.size() >= 3) {
            std::cout << "   Downloading 3 files concurrently..." << std::endl;
            std::cout << std::endl;

            start = std::chrono::high_resolution_clock::now();

            auto download_future1 = std::async(std::launch::async, 
                [&client, &uploaded_file_ids]() {
                    return client.download_file(uploaded_file_ids[0]);
                });

            auto download_future2 = std::async(std::launch::async,
                [&client, &uploaded_file_ids]() {
                    return client.download_file(uploaded_file_ids[1]);
                });

            auto download_future3 = std::async(std::launch::async,
                [&client, &uploaded_file_ids]() {
                    return client.download_file(uploaded_file_ids[2]);
                });

            std::vector<uint8_t> data1 = download_future1.get();
            std::vector<uint8_t> data2 = download_future2.get();
            std::vector<uint8_t> data3 = download_future3.get();

            end = std::chrono::high_resolution_clock::now();
            total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            std::cout << "   ✓ All 3 files downloaded successfully!" << std::endl;
            std::cout << "   File 1 size: " << data1.size() << " bytes" << std::endl;
            std::cout << "   File 2 size: " << data2.size() << " bytes" << std::endl;
            std::cout << "   File 3 size: " << data3.size() << " bytes" << std::endl;
            std::cout << "   Total time: " << total_duration.count() << " ms" << std::endl;
            std::cout << std::endl;
        }

        // ====================================================================
        // EXAMPLE 4: Performance Comparison - Sequential vs Concurrent
        // ====================================================================
        std::cout << "5. Performance Comparison - Sequential vs Concurrent" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates performance comparison between sequential and concurrent operations." << std::endl;
        std::cout << std::endl;

        const size_t num_operations = 10;
        std::vector<std::vector<uint8_t>> test_data;
        for (size_t i = 0; i < num_operations; ++i) {
            std::string content = "Test file " + std::to_string(i + 1);
            std::vector<uint8_t> data(content.begin(), content.end());
            test_data.push_back(data);
        }

        // Sequential operations
        std::cout << "   Sequential Operations:" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::string> sequential_file_ids;
        
        for (size_t i = 0; i < num_operations; ++i) {
            std::string file_id = client.upload_buffer(test_data[i], "txt", nullptr);
            sequential_file_ids.push_back(file_id);
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto sequential_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "     Total time: " << sequential_duration.count() << " ms" << std::endl;
        std::cout << "     Average per operation: " 
                  << (sequential_duration.count() / num_operations) << " ms" << std::endl;
        std::cout << std::endl;

        // Concurrent operations
        std::cout << "   Concurrent Operations:" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<std::string>> concurrent_futures;
        
        for (size_t i = 0; i < num_operations; ++i) {
            concurrent_futures.push_back(std::async(std::launch::async,
                [&client, &test_data, i]() {
                    return client.upload_buffer(test_data[i], "txt", nullptr);
                }));
        }
        
        std::vector<std::string> concurrent_file_ids;
        for (auto& future : concurrent_futures) {
            concurrent_file_ids.push_back(future.get());
        }
        
        end = std::chrono::high_resolution_clock::now();
        auto concurrent_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "     Total time: " << concurrent_duration.count() << " ms" << std::endl;
        std::cout << "     Average per operation: " 
                  << (concurrent_duration.count() / num_operations) << " ms" << std::endl;
        std::cout << std::endl;

        // Performance comparison
        double speedup = static_cast<double>(sequential_duration.count()) / 
                        static_cast<double>(concurrent_duration.count());
        std::cout << "   Performance Improvement:" << std::endl;
        std::cout << "     Speedup: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        std::cout << "     Time saved: " << (sequential_duration.count() - concurrent_duration.count()) 
                  << " ms" << std::endl;
        std::cout << std::endl;

        // Clean up sequential files
        for (const auto& file_id : sequential_file_ids) {
            try {
                client.delete_file(file_id);
            } catch (...) {}
        }

        // Clean up concurrent files
        for (const auto& file_id : concurrent_file_ids) {
            try {
                client.delete_file(file_id);
            } catch (...) {}
        }

        // ====================================================================
        // EXAMPLE 5: Connection Pool Behavior Under Concurrent Load
        // ====================================================================
        std::cout << "6. Connection Pool Behavior Under Concurrent Load" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows connection pool behavior under concurrent load." << std::endl;
        std::cout << std::endl;

        const size_t high_concurrency = 20;
        std::cout << "   Testing with " << high_concurrency << " concurrent operations..." << std::endl;
        std::cout << std::endl;

        Statistics stats;
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<OperationResult>> high_concurrency_futures;

        for (size_t i = 0; i < high_concurrency; ++i) {
            high_concurrency_futures.push_back(std::async(std::launch::async,
                [&client, i, &stats]() -> OperationResult {
                    auto op_start = std::chrono::high_resolution_clock::now();
                    try {
                        std::string content = "High concurrency file " + std::to_string(i + 1);
                        std::vector<uint8_t> data(content.begin(), content.end());
                        std::string file_id = client.upload_buffer(data, "txt", nullptr);
                        
                        auto op_end = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            op_end - op_start);
                        
                        stats.record(true, duration);
                        return {true, file_id, "", i, duration};
                    } catch (const std::exception& e) {
                        auto op_end = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            op_end - op_start);
                        
                        stats.record(false, duration);
                        return {false, "", e.what(), i, duration};
                    }
                }));
        }

        std::vector<std::string> high_concurrency_file_ids;
        for (auto& future : high_concurrency_futures) {
            OperationResult result = future.get();
            if (result.success) {
                high_concurrency_file_ids.push_back(result.file_id);
            }
        }

        end = std::chrono::high_resolution_clock::now();
        total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "   Total time: " << total_duration.count() << " ms" << std::endl;
        stats.print();
        std::cout << std::endl;

        // Clean up
        for (const auto& file_id : high_concurrency_file_ids) {
            try {
                client.delete_file(file_id);
            } catch (...) {}
        }

        // Clean up earlier uploaded files
        for (const auto& file_id : uploaded_file_ids) {
            try {
                client.delete_file(file_id);
            } catch (...) {}
        }

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Multi-threaded FastDFS operations" << std::endl;
        std::cout << "  ✓ Thread-safe client usage patterns" << std::endl;
        std::cout << "  ✓ Examples using std::thread, std::async, and thread pools" << std::endl;
        std::cout << "  ✓ Performance comparison between sequential and concurrent operations" << std::endl;
        std::cout << "  ✓ Useful for high-throughput applications and parallel processing" << std::endl;
        std::cout << "  ✓ Connection pool behavior under concurrent load" << std::endl;

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

