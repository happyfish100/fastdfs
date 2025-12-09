/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Performance Example
 *
 * This comprehensive example demonstrates performance benchmarking and optimization,
 * connection pool tuning, batch operation patterns, memory usage optimization,
 * performance metrics collection, and benchmarking patterns.
 *
 * Key Topics Covered:
 * - Demonstrates performance benchmarking and optimization
 * - Shows connection pool tuning techniques
 * - Includes batch operation performance patterns
 * - Demonstrates memory usage optimization
 * - Shows performance metrics collection
 * - Useful for performance testing and optimization
 * - Demonstrates benchmarking patterns and performance analysis
 *
 * Run this example with:
 *   ./performance_example <tracker_address>
 *   Example: ./performance_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

// Performance metrics structure
struct PerformanceMetrics {
    size_t operations_count = 0;
    size_t successful_operations = 0;
    size_t failed_operations = 0;
    std::chrono::milliseconds total_time{0};
    std::chrono::milliseconds min_time{std::chrono::milliseconds::max()};
    std::chrono::milliseconds max_time{0};
    std::vector<std::chrono::milliseconds> operation_times;
    int64_t bytes_transferred = 0;
    
    void record_operation(bool success, std::chrono::milliseconds duration, int64_t bytes = 0) {
        operations_count++;
        if (success) {
            successful_operations++;
            total_time += duration;
            operation_times.push_back(duration);
            if (duration < min_time) min_time = duration;
            if (duration > max_time) max_time = duration;
            bytes_transferred += bytes;
        } else {
            failed_operations++;
        }
    }
    
    void print(const std::string& title) {
        std::cout << "   " << title << ":" << std::endl;
        std::cout << "     Operations: " << operations_count 
                  << " (Success: " << successful_operations 
                  << ", Failed: " << failed_operations << ")" << std::endl;
        
        if (successful_operations > 0) {
            std::cout << "     Total Time: " << total_time.count() << " ms" << std::endl;
            std::cout << "     Average Time: " << (total_time.count() / successful_operations) << " ms" << std::endl;
            std::cout << "     Min Time: " << min_time.count() << " ms" << std::endl;
            std::cout << "     Max Time: " << max_time.count() << " ms" << std::endl;
            
            if (operation_times.size() > 0) {
                std::sort(operation_times.begin(), operation_times.end());
                size_t p50_idx = operation_times.size() * 0.5;
                size_t p95_idx = operation_times.size() * 0.95;
                size_t p99_idx = operation_times.size() * 0.99;
                
                std::cout << "     P50 (Median): " << operation_times[p50_idx].count() << " ms" << std::endl;
                std::cout << "     P95: " << operation_times[p95_idx].count() << " ms" << std::endl;
                std::cout << "     P99: " << operation_times[p99_idx].count() << " ms" << std::endl;
            }
            
            if (total_time.count() > 0) {
                double ops_per_sec = (successful_operations * 1000.0) / total_time.count();
                std::cout << "     Throughput: " << std::fixed << std::setprecision(2) 
                         << ops_per_sec << " ops/sec" << std::endl;
            }
            
            if (bytes_transferred > 0 && total_time.count() > 0) {
                double mbps = (bytes_transferred / 1024.0 / 1024.0) / (total_time.count() / 1000.0);
                std::cout << "     Data Rate: " << std::fixed << std::setprecision(2) 
                         << mbps << " MB/s" << std::endl;
            }
        }
    }
};

// Memory usage tracking
struct MemoryUsage {
    size_t initial_memory = 0;
    size_t peak_memory = 0;
    
    void start() {
        initial_memory = get_current_memory();
    }
    
    void update() {
        size_t current = get_current_memory();
        if (current > peak_memory) {
            peak_memory = current;
        }
    }
    
    size_t get_peak_delta() {
        return peak_memory > initial_memory ? peak_memory - initial_memory : 0;
    }
    
private:
    size_t get_current_memory() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
        return 0;
#else
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            return usage.ru_maxrss * 1024; // ru_maxrss is in KB
        }
        return 0;
#endif
    }
};

// Helper function to format memory size
std::string format_memory(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

// Helper function to create test data
std::vector<uint8_t> create_test_data(size_t size) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    return data;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Performance Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Connection Pool Tuning
        // ====================================================================
        std::cout << "1. Connection Pool Tuning" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows connection pool tuning techniques." << std::endl;
        std::cout << std::endl;

        const int num_operations = 50;
        const size_t data_size = 10 * 1024; // 10KB per operation

        // Test with different connection pool sizes
        std::vector<int> pool_sizes = {1, 5, 10, 20, 50};
        std::vector<PerformanceMetrics> pool_metrics;

        for (int pool_size : pool_sizes) {
            std::cout << "   Testing with max_conns = " << pool_size << "..." << std::endl;
            
            fastdfs::ClientConfig config;
            config.tracker_addrs = {argv[1]};
            config.max_conns = pool_size;
            config.connect_timeout = std::chrono::milliseconds(5000);
            config.network_timeout = std::chrono::milliseconds(30000);
            config.enable_pool = true;

            fastdfs::Client client(config);
            PerformanceMetrics metrics;
            std::vector<std::string> uploaded_files;

            auto start = std::chrono::high_resolution_clock::now();
            
            // Perform concurrent uploads
            std::vector<std::future<void>> futures;
            std::mutex files_mutex;
            for (int i = 0; i < num_operations; ++i) {
                futures.push_back(std::async(std::launch::async, [&client, &metrics, &uploaded_files, &files_mutex, data_size, i]() {
                    try {
                        auto op_start = std::chrono::high_resolution_clock::now();
                        std::vector<uint8_t> data = create_test_data(data_size);
                        std::string file_id = client.upload_buffer(data, "bin", nullptr);
                        auto op_end = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(op_end - op_start);
                        
                        metrics.record_operation(true, duration, data_size);
                        
                        std::lock_guard<std::mutex> lock(files_mutex);
                        uploaded_files.push_back(file_id);
                    } catch (...) {
                        metrics.record_operation(false, std::chrono::milliseconds(0));
                    }
                }));
            }
            
            for (auto& future : futures) {
                future.wait();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            // Cleanup
            for (const auto& file_id : uploaded_files) {
                try {
                    client.delete_file(file_id);
                } catch (...) {}
            }
            
            pool_metrics.push_back(metrics);
            std::cout << "     → Completed in " << total_duration.count() << " ms" << std::endl;
        }

        std::cout << std::endl;
        std::cout << "   Connection Pool Performance Comparison:" << std::endl;
        for (size_t i = 0; i < pool_sizes.size(); ++i) {
            std::cout << "     max_conns=" << pool_sizes[i] << ": ";
            if (pool_metrics[i].successful_operations > 0) {
                double ops_per_sec = (pool_metrics[i].successful_operations * 1000.0) / 
                                   pool_metrics[i].total_time.count();
                std::cout << std::fixed << std::setprecision(2) << ops_per_sec << " ops/sec" << std::endl;
            } else {
                std::cout << "N/A" << std::endl;
            }
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Batch Operation Performance
        // ====================================================================
        std::cout << "2. Batch Operation Performance Patterns" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes batch operation performance patterns." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig batch_config;
        batch_config.tracker_addrs = {argv[1]};
        batch_config.max_conns = 20;
        batch_config.connect_timeout = std::chrono::milliseconds(5000);
        batch_config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client batch_client(batch_config);
        
        const int batch_size = 100;
        const size_t batch_data_size = 5 * 1024; // 5KB per file

        // Sequential batch
        std::cout << "   Sequential batch upload (" << batch_size << " files)..." << std::endl;
        PerformanceMetrics seq_metrics;
        std::vector<std::string> seq_files;
        
        auto seq_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < batch_size; ++i) {
            try {
                auto op_start = std::chrono::high_resolution_clock::now();
                std::vector<uint8_t> data = create_test_data(batch_data_size);
                std::string file_id = batch_client.upload_buffer(data, "bin", nullptr);
                auto op_end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(op_end - op_start);
                
                seq_metrics.record_operation(true, duration, batch_data_size);
                seq_files.push_back(file_id);
            } catch (...) {
                seq_metrics.record_operation(false, std::chrono::milliseconds(0));
            }
        }
        auto seq_end = std::chrono::high_resolution_clock::now();
        auto seq_total = std::chrono::duration_cast<std::chrono::milliseconds>(seq_end - seq_start);
        
        // Cleanup sequential
        for (const auto& file_id : seq_files) {
            try {
                batch_client.delete_file(file_id);
            } catch (...) {}
        }
        
        seq_metrics.print("Sequential Batch");
        std::cout << "     Total Wall Time: " << seq_total.count() << " ms" << std::endl;
        std::cout << std::endl;

        // Parallel batch
        std::cout << "   Parallel batch upload (" << batch_size << " files)..." << std::endl;
        PerformanceMetrics par_metrics;
        std::vector<std::string> par_files;
        std::mutex files_mutex;
        
        auto par_start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<void>> par_futures;
        for (int i = 0; i < batch_size; ++i) {
            par_futures.push_back(std::async(std::launch::async, [&batch_client, &par_metrics, &par_files, &files_mutex, batch_data_size]() {
                try {
                    auto op_start = std::chrono::high_resolution_clock::now();
                    std::vector<uint8_t> data = create_test_data(batch_data_size);
                    std::string file_id = batch_client.upload_buffer(data, "bin", nullptr);
                    auto op_end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(op_end - op_start);
                    
                    par_metrics.record_operation(true, duration, batch_data_size);
                    
                    std::lock_guard<std::mutex> lock(files_mutex);
                    par_files.push_back(file_id);
                } catch (...) {
                    par_metrics.record_operation(false, std::chrono::milliseconds(0));
                }
            }));
        }
        
        for (auto& future : par_futures) {
            future.wait();
        }
        auto par_end = std::chrono::high_resolution_clock::now();
        auto par_total = std::chrono::duration_cast<std::chrono::milliseconds>(par_end - par_start);
        
        // Cleanup parallel
        for (const auto& file_id : par_files) {
            try {
                batch_client.delete_file(file_id);
            } catch (...) {}
        }
        
        par_metrics.print("Parallel Batch");
        std::cout << "     Total Wall Time: " << par_total.count() << " ms" << std::endl;
        std::cout << std::endl;
        
        std::cout << "   Performance Improvement: " << std::fixed << std::setprecision(1)
                  << ((seq_total.count() * 100.0 / par_total.count()) - 100.0) << "% faster (parallel)" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Memory Usage Optimization
        // ====================================================================
        std::cout << "3. Memory Usage Optimization" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates memory usage optimization." << std::endl;
        std::cout << std::endl;

        MemoryUsage mem_tracker;
        mem_tracker.start();

        fastdfs::ClientConfig mem_config;
        mem_config.tracker_addrs = {argv[1]};
        mem_config.max_conns = 10;
        mem_config.connect_timeout = std::chrono::milliseconds(5000);
        mem_config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client mem_client(mem_config);

        // Test 1: Memory-efficient chunked processing
        std::cout << "   Test 1: Memory-efficient chunked processing..." << std::endl;
        const int64_t large_file_size = 100 * 1024; // 100KB
        const int64_t chunk_size = 10 * 1024; // 10KB chunks
        
        std::vector<uint8_t> chunk(chunk_size);
        std::string chunked_file_id;
        
        // Upload in chunks using appender
        for (int64_t offset = 0; offset < large_file_size; offset += chunk_size) {
            int64_t current_chunk = std::min(chunk_size, large_file_size - offset);
            for (int64_t i = 0; i < current_chunk; ++i) {
                chunk[i] = static_cast<uint8_t>((offset + i) % 256);
            }
            
            if (offset == 0) {
                chunked_file_id = mem_client.upload_appender_buffer(
                    std::vector<uint8_t>(chunk.begin(), chunk.begin() + current_chunk),
                    "bin", nullptr);
            } else {
                mem_client.append_file(chunked_file_id,
                    std::vector<uint8_t>(chunk.begin(), chunk.begin() + current_chunk));
            }
            
            mem_tracker.update();
        }
        
        mem_client.delete_file(chunked_file_id);
        std::cout << "     → Peak memory delta: " << format_memory(mem_tracker.get_peak_delta()) << std::endl;
        std::cout << std::endl;

        // Test 2: Reusing buffers
        std::cout << "   Test 2: Buffer reuse pattern..." << std::endl;
        MemoryUsage mem_tracker2;
        mem_tracker2.start();
        
        std::vector<uint8_t> reusable_buffer(20 * 1024); // Reusable 20KB buffer
        std::vector<std::string> reused_files;
        
        for (int i = 0; i < 10; ++i) {
            // Fill buffer with different content
            for (size_t j = 0; j < reusable_buffer.size(); ++j) {
                reusable_buffer[j] = static_cast<uint8_t>((i * reusable_buffer.size() + j) % 256);
            }
            
            std::string file_id = mem_client.upload_buffer(reusable_buffer, "bin", nullptr);
            reused_files.push_back(file_id);
            mem_tracker2.update();
        }
        
        for (const auto& file_id : reused_files) {
            mem_client.delete_file(file_id);
        }
        
        std::cout << "     → Peak memory delta: " << format_memory(mem_tracker2.get_peak_delta()) << std::endl;
        std::cout << "     → Buffer reused " << reused_files.size() << " times" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Performance Metrics Collection
        // ====================================================================
        std::cout << "4. Performance Metrics Collection" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows performance metrics collection." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig metrics_config;
        metrics_config.tracker_addrs = {argv[1]};
        metrics_config.max_conns = 15;
        metrics_config.connect_timeout = std::chrono::milliseconds(5000);
        metrics_config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client metrics_client(metrics_config);
        
        const int metrics_ops = 30;
        PerformanceMetrics detailed_metrics;
        std::vector<std::string> metrics_files;

        std::cout << "   Collecting detailed metrics for " << metrics_ops << " operations..." << std::endl;
        
        for (int i = 0; i < metrics_ops; ++i) {
            try {
                auto op_start = std::chrono::high_resolution_clock::now();
                std::vector<uint8_t> data = create_test_data(8 * 1024);
                std::string file_id = metrics_client.upload_buffer(data, "bin", nullptr);
                auto op_end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(op_end - op_start);
                
                detailed_metrics.record_operation(true, duration, 8 * 1024);
                metrics_files.push_back(file_id);
            } catch (...) {
                detailed_metrics.record_operation(false, std::chrono::milliseconds(0));
            }
        }
        
        // Cleanup
        for (const auto& file_id : metrics_files) {
            try {
                metrics_client.delete_file(file_id);
            } catch (...) {}
        }
        
        detailed_metrics.print("Detailed Performance Metrics");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Different File Size Performance
        // ====================================================================
        std::cout << "5. Performance by File Size" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Benchmarking patterns and performance analysis." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig size_config;
        size_config.tracker_addrs = {argv[1]};
        size_config.max_conns = 10;
        size_config.connect_timeout = std::chrono::milliseconds(5000);
        size_config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client size_client(size_config);
        
        std::vector<size_t> test_sizes = {1 * 1024, 10 * 1024, 100 * 1024, 500 * 1024}; // 1KB, 10KB, 100KB, 500KB
        const int ops_per_size = 5;

        for (size_t test_size : test_sizes) {
            std::cout << "   Testing with file size: " << format_memory(test_size) << std::endl;
            PerformanceMetrics size_metrics;
            std::vector<std::string> size_files;

            for (int i = 0; i < ops_per_size; ++i) {
                try {
                    auto op_start = std::chrono::high_resolution_clock::now();
                    std::vector<uint8_t> data = create_test_data(test_size);
                    std::string file_id = size_client.upload_buffer(data, "bin", nullptr);
                    auto op_end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(op_end - op_start);
                    
                    size_metrics.record_operation(true, duration, test_size);
                    size_files.push_back(file_id);
                } catch (...) {
                    size_metrics.record_operation(false, std::chrono::milliseconds(0));
                }
            }
            
            // Cleanup
            for (const auto& file_id : size_files) {
                try {
                    size_client.delete_file(file_id);
                } catch (...) {}
            }
            
            if (size_metrics.successful_operations > 0) {
                double avg_time = size_metrics.total_time.count() / size_metrics.successful_operations;
                double mbps = (test_size / 1024.0 / 1024.0) / (avg_time / 1000.0);
                std::cout << "     → Average: " << avg_time << " ms, Throughput: " 
                         << std::fixed << std::setprecision(2) << mbps << " MB/s" << std::endl;
            }
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: Retry Policy Performance Impact
        // ====================================================================
        std::cout << "6. Retry Policy Performance Impact" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Performance testing and optimization." << std::endl;
        std::cout << std::endl;

        std::vector<int> retry_counts = {0, 1, 3, 5};
        const int retry_test_ops = 20;

        for (int retry_count : retry_counts) {
            std::cout << "   Testing with retry_count = " << retry_count << "..." << std::endl;
            
            fastdfs::ClientConfig retry_config;
            retry_config.tracker_addrs = {argv[1]};
            retry_config.max_conns = 10;
            retry_config.connect_timeout = std::chrono::milliseconds(5000);
            retry_config.network_timeout = std::chrono::milliseconds(30000);
            retry_config.retry_count = retry_count;

            fastdfs::Client retry_client(retry_config);
            PerformanceMetrics retry_metrics;
            std::vector<std::string> retry_files;

            auto retry_start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < retry_test_ops; ++i) {
                try {
                    auto op_start = std::chrono::high_resolution_clock::now();
                    std::vector<uint8_t> data = create_test_data(5 * 1024);
                    std::string file_id = retry_client.upload_buffer(data, "bin", nullptr);
                    auto op_end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(op_end - op_start);
                    
                    retry_metrics.record_operation(true, duration, 5 * 1024);
                    retry_files.push_back(file_id);
                } catch (...) {
                    retry_metrics.record_operation(false, std::chrono::milliseconds(0));
                }
            }
            auto retry_end = std::chrono::high_resolution_clock::now();
            auto retry_total = std::chrono::duration_cast<std::chrono::milliseconds>(retry_end - retry_start);
            
            // Cleanup
            for (const auto& file_id : retry_files) {
                try {
                    retry_client.delete_file(file_id);
                } catch (...) {}
            }
            
            std::cout << "     → Total time: " << retry_total.count() << " ms, "
                     << "Success rate: " << std::fixed << std::setprecision(1)
                     << (retry_metrics.successful_operations * 100.0 / retry_test_ops) << "%" << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Performance Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Performance benchmarking and optimization" << std::endl;
        std::cout << "  ✓ Connection pool tuning techniques" << std::endl;
        std::cout << "  ✓ Batch operation performance patterns" << std::endl;
        std::cout << "  ✓ Memory usage optimization" << std::endl;
        std::cout << "  ✓ Performance metrics collection" << std::endl;
        std::cout << "  ✓ Performance testing and optimization" << std::endl;
        std::cout << "  ✓ Benchmarking patterns and performance analysis" << std::endl;
        std::cout << std::endl;
        std::cout << "Best Practices:" << std::endl;
        std::cout << "  • Tune connection pool size based on concurrent load" << std::endl;
        std::cout << "  • Use parallel operations for batch processing" << std::endl;
        std::cout << "  • Process large files in chunks to limit memory usage" << std::endl;
        std::cout << "  • Reuse buffers when processing multiple files" << std::endl;
        std::cout << "  • Collect detailed metrics (P50, P95, P99) for analysis" << std::endl;
        std::cout << "  • Monitor memory usage during operations" << std::endl;
        std::cout << "  • Test different configurations to find optimal settings" << std::endl;
        std::cout << "  • Balance retry count with performance requirements" << std::endl;

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

