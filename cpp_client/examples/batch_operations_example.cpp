/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Batch Operations Example
 *
 * This example demonstrates how to perform batch operations with the FastDFS client.
 * It covers efficient patterns for processing multiple files in batches, including
 * progress tracking, error handling, and performance optimization.
 *
 * Key Topics Covered:
 * - Batch upload multiple files
 * - Batch download multiple files
 * - Progress tracking for batches
 * - Error handling in batches
 * - Performance optimization techniques
 * - Bulk operations patterns
 * - Useful for bulk data migration, backup operations, and ETL processes
 *
 * Run this example with:
 *   ./batch_operations_example <tracker_address>
 *   Example: ./batch_operations_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <map>

// Structure to track batch operation results
struct BatchResult {
    bool success;
    std::string file_id;
    std::string error_message;
    size_t index;
};

// Structure to track progress
struct ProgressTracker {
    std::mutex mutex;
    size_t completed = 0;
    size_t successful = 0;
    size_t failed = 0;
    size_t total = 0;

    void update(bool success) {
        std::lock_guard<std::mutex> lock(mutex);
        completed++;
        if (success) {
            successful++;
        } else {
            failed++;
        }
    }

    void print_progress() {
        std::lock_guard<std::mutex> lock(mutex);
        double progress = total > 0 ? (completed * 100.0 / total) : 0.0;
        std::cout << "   Progress: " << std::fixed << std::setprecision(1) << progress 
                  << "% (" << completed << "/" << total << " completed, "
                  << successful << " successful, " << failed << " failed)" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Batch Operations Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // STEP 1: Configure and Create Client
        // ====================================================================
        std::cout << "1. Configuring FastDFS Client..." << std::endl;
        fastdfs::ClientConfig config;
        config.tracker_addrs = {argv[1]};
        config.max_conns = 50;  // Higher limit for batch operations
        config.connect_timeout = std::chrono::milliseconds(5000);
        config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client client(config);
        std::cout << "   ✓ Client initialized successfully" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Simple Batch Upload
        // ====================================================================
        std::cout << "2. Simple Batch Upload" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates efficient batch processing of multiple files." << std::endl;
        std::cout << "   Shows how to upload/download multiple files in a single operation." << std::endl;
        std::cout << std::endl;

        // Prepare file data for batch upload
        std::vector<std::pair<std::string, std::vector<uint8_t>>> file_data;
        for (int i = 1; i <= 5; ++i) {
            std::string name = "file" + std::to_string(i) + ".txt";
            std::string content = "Content of file " + std::to_string(i);
            std::vector<uint8_t> data(content.begin(), content.end());
            file_data.push_back({name, data});
        }

        std::cout << "   Preparing to upload " << file_data.size() << " files..." << std::endl;
        std::cout << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        // Create upload tasks for all files
        std::vector<std::future<BatchResult>> upload_futures;
        for (size_t i = 0; i < file_data.size(); ++i) {
            std::cout << "   → Queuing upload for: " << file_data[i].first << std::endl;
            
            upload_futures.push_back(std::async(std::launch::async, 
                [&client, i, &file_data]() -> BatchResult {
                    try {
                        std::string file_id = client.upload_buffer(
                            file_data[i].second, "txt", nullptr);
                        return {true, file_id, "", i};
                    } catch (const std::exception& e) {
                        return {false, "", e.what(), i};
                    }
                }));
        }

        // Collect results
        std::vector<BatchResult> results;
        std::vector<std::string> uploaded_file_ids;
        
        for (size_t i = 0; i < upload_futures.size(); ++i) {
            BatchResult result = upload_futures[i].get();
            results.push_back(result);
            
            if (result.success) {
                uploaded_file_ids.push_back(result.file_id);
                std::cout << "   ✓ File " << (i + 1) << " uploaded: " << result.file_id << std::endl;
            } else {
                std::cout << "   ✗ File " << (i + 1) << " failed: " << result.error_message << std::endl;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        size_t successful = uploaded_file_ids.size();
        size_t failed = results.size() - successful;

        std::cout << std::endl;
        std::cout << "   Batch Upload Summary:" << std::endl;
        std::cout << "   - Total files: " << file_data.size() << std::endl;
        std::cout << "   - Successful: " << successful << std::endl;
        std::cout << "   - Failed: " << failed << std::endl;
        std::cout << "   - Total time: " << duration.count() << " ms" << std::endl;
        if (file_data.size() > 0) {
            std::cout << "   - Average time per file: " 
                      << (duration.count() / file_data.size()) << " ms" << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Batch Upload with Progress Tracking
        // ====================================================================
        std::cout << "3. Batch Upload with Progress Tracking" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates progress tracking for batch operations." << std::endl;
        std::cout << std::endl;

        const size_t batch_size = 10;
        std::vector<std::pair<std::string, std::vector<uint8_t>>> progress_files;
        for (size_t i = 1; i <= batch_size; ++i) {
            std::string name = "progress_file_" + std::to_string(i) + ".txt";
            std::string content = "Content of progress file " + std::to_string(i);
            std::vector<uint8_t> data(content.begin(), content.end());
            progress_files.push_back({name, data});
        }

        std::cout << "   Uploading " << batch_size << " files with progress tracking..." << std::endl;
        std::cout << std::endl;

        ProgressTracker progress;
        progress.total = batch_size;

        start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<BatchResult>> progress_futures;
        std::vector<std::string> progress_file_ids;

        // Create upload tasks with progress tracking
        for (size_t i = 0; i < progress_files.size(); ++i) {
            progress_futures.push_back(std::async(std::launch::async,
                [&client, i, &progress_files, &progress]() -> BatchResult {
                    try {
                        std::string file_id = client.upload_buffer(
                            progress_files[i].second, "txt", nullptr);
                        progress.update(true);
                        return {true, file_id, "", i};
                    } catch (const std::exception& e) {
                        progress.update(false);
                        return {false, "", e.what(), i};
                    }
                }));
        }

        // Collect results and show progress
        for (size_t i = 0; i < progress_futures.size(); ++i) {
            BatchResult result = progress_futures[i].get();
            
            if (result.success) {
                progress_file_ids.push_back(result.file_id);
                std::cout << "   [" << (i + 1) << "/" << batch_size << "] ✓ " 
                          << progress_files[i].first << " uploaded: " << result.file_id << std::endl;
            } else {
                std::cout << "   [" << (i + 1) << "/" << batch_size << "] ✗ " 
                          << progress_files[i].first << " failed: " << result.error_message << std::endl;
            }
            
            progress.print_progress();
        }

        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << std::endl;
        std::cout << "   Batch completed in " << duration.count() << " ms" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Batch Download
        // ====================================================================
        std::cout << "4. Batch Download" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Downloading multiple files in batch." << std::endl;
        std::cout << std::endl;

        if (uploaded_file_ids.empty()) {
            std::cout << "   No files to download (previous uploads failed)" << std::endl;
        } else {
            std::cout << "   Downloading " << uploaded_file_ids.size() << " files..." << std::endl;
            std::cout << std::endl;

            start = std::chrono::high_resolution_clock::now();
            std::vector<std::future<std::pair<bool, size_t>>> download_futures;

            for (size_t i = 0; i < uploaded_file_ids.size(); ++i) {
                download_futures.push_back(std::async(std::launch::async,
                    [&client, i, &uploaded_file_ids]() -> std::pair<bool, size_t> {
                        try {
                            std::vector<uint8_t> data = client.download_file(uploaded_file_ids[i]);
                            return {true, data.size()};
                        } catch (const std::exception&) {
                            return {false, 0};
                        }
                    }));
            }

            size_t download_successful = 0;
            size_t download_failed = 0;
            size_t total_bytes = 0;

            for (size_t i = 0; i < download_futures.size(); ++i) {
                auto [success, size] = download_futures[i].get();
                if (success) {
                    download_successful++;
                    total_bytes += size;
                    std::cout << "   ✓ Downloaded file " << (i + 1) 
                              << " (" << size << " bytes)" << std::endl;
                } else {
                    download_failed++;
                    std::cout << "   ✗ Failed to download file " << (i + 1) << std::endl;
                }
            }

            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            std::cout << std::endl;
            std::cout << "   Batch Download Summary:" << std::endl;
            std::cout << "   - Successful: " << download_successful << std::endl;
            std::cout << "   - Failed: " << download_failed << std::endl;
            std::cout << "   - Total bytes: " << total_bytes << std::endl;
            std::cout << "   - Total time: " << duration.count() << " ms" << std::endl;
            std::cout << std::endl;
        }

        // ====================================================================
        // EXAMPLE 4: Error Handling for Partial Batch Failures
        // ====================================================================
        std::cout << "5. Error Handling for Partial Batch Failures" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes error handling for partial batch failures." << std::endl;
        std::cout << std::endl;

        // Create a batch with some files that will succeed and some that might fail
        std::vector<std::pair<std::string, std::vector<uint8_t>>> mixed_batch;
        for (int i = 1; i <= 5; ++i) {
            std::string name = "mixed_file_" + std::to_string(i) + ".txt";
            std::string content = "Content " + std::to_string(i);
            std::vector<uint8_t> data(content.begin(), content.end());
            mixed_batch.push_back({name, data});
        }

        std::cout << "   Uploading batch with error handling..." << std::endl;
        std::cout << std::endl;

        std::vector<BatchResult> mixed_results;
        std::vector<std::future<BatchResult>> mixed_futures;

        for (size_t i = 0; i < mixed_batch.size(); ++i) {
            mixed_futures.push_back(std::async(std::launch::async,
                [&client, i, &mixed_batch]() -> BatchResult {
                    try {
                        std::string file_id = client.upload_buffer(
                            mixed_batch[i].second, "txt", nullptr);
                        return {true, file_id, "", i};
                    } catch (const std::exception& e) {
                        return {false, "", e.what(), i};
                    }
                }));
        }

        std::vector<std::string> successful_file_ids;
        std::vector<std::pair<size_t, std::string>> failed_files;

        for (size_t i = 0; i < mixed_futures.size(); ++i) {
            BatchResult result = mixed_futures[i].get();
            mixed_results.push_back(result);

            if (result.success) {
                successful_file_ids.push_back(result.file_id);
                std::cout << "   ✓ File " << (i + 1) << " succeeded: " << result.file_id << std::endl;
            } else {
                failed_files.push_back({i, result.error_message});
                std::cout << "   ✗ File " << (i + 1) << " failed: " << result.error_message << std::endl;
            }
        }

        std::cout << std::endl;
        std::cout << "   Error Handling Summary:" << std::endl;
        std::cout << "   - Successful: " << successful_file_ids.size() << std::endl;
        std::cout << "   - Failed: " << failed_files.size() << std::endl;
        if (!failed_files.empty()) {
            std::cout << "   - Failed files can be retried or logged for investigation" << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Optimization Techniques for Batch Processing
        // ====================================================================
        std::cout << "6. Optimization Techniques for Batch Processing" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows optimization techniques for batch processing." << std::endl;
        std::cout << "   Useful for bulk data migration, backup operations, and ETL processes." << std::endl;
        std::cout << std::endl;

        std::cout << "   Optimization Strategies:" << std::endl;
        std::cout << "   1. Use higher connection pool size for concurrent operations" << std::endl;
        std::cout << "   2. Process files in parallel using std::async or std::thread" << std::endl;
        std::cout << "   3. Batch similar operations together" << std::endl;
        std::cout << "   4. Implement retry logic for failed operations" << std::endl;
        std::cout << "   5. Use progress tracking for long-running batches" << std::endl;
        std::cout << "   6. Clean up resources after batch completion" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // CLEANUP
        // ====================================================================
        std::cout << "7. Cleaning up test files..." << std::endl;
        
        // Clean up uploaded files
        for (const auto& file_id : uploaded_file_ids) {
            try {
                client.delete_file(file_id);
            } catch (...) {
                // Ignore cleanup errors
            }
        }
        
        for (const auto& file_id : progress_file_ids) {
            try {
                client.delete_file(file_id);
            } catch (...) {
                // Ignore cleanup errors
            }
        }
        
        for (const auto& file_id : successful_file_ids) {
            try {
                client.delete_file(file_id);
            } catch (...) {
                // Ignore cleanup errors
            }
        }
        
        std::cout << "   ✓ Test files cleaned up" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Efficient batch processing of multiple files" << std::endl;
        std::cout << "  ✓ Upload/download multiple files in a single operation" << std::endl;
        std::cout << "  ✓ Error handling for partial batch failures" << std::endl;
        std::cout << "  ✓ Progress tracking for batch operations" << std::endl;
        std::cout << "  ✓ Useful for bulk data migration, backup operations, and ETL processes" << std::endl;
        std::cout << "  ✓ Optimization techniques for batch processing" << std::endl;

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

