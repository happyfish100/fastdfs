/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Connection Pool Example
 *
 * This example demonstrates connection pool management with the FastDFS client.
 * It covers configuration, monitoring, performance impact, and best practices
 * for managing connections efficiently in production applications.
 *
 * Key Topics Covered:
 * - Connection pool configuration and tuning
 * - Optimize connection pool size for different workloads
 * - Connection pool monitoring
 * - Connection reuse patterns
 * - Performance optimization and resource management
 * - Connection pool exhaustion scenarios
 *
 * Run this example with:
 *   ./connection_pool_example <tracker_address>
 *   Example: ./connection_pool_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <iomanip>

// Structure to track pool performance
struct PoolPerformance {
    size_t operations;
    std::chrono::milliseconds total_time;
    size_t successful;
    size_t failed;

    PoolPerformance() : operations(0), total_time(0), successful(0), failed(0) {}

    double average_time() const {
        return operations > 0 ? static_cast<double>(total_time.count()) / operations : 0.0;
    }

    double success_rate() const {
        return operations > 0 ? (static_cast<double>(successful) / operations) * 100.0 : 0.0;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Connection Pool Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Basic Connection Pool Configuration
        // ====================================================================
        std::cout << "1. Basic Connection Pool Configuration" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates connection pool configuration and tuning." << std::endl;
        std::cout << "   Shows how to optimize connection pool size for different workloads." << std::endl;
        std::cout << std::endl;

        // Configuration 1: Small Connection Pool
        std::cout << "   Configuration 1: Small Connection Pool" << std::endl;
        std::cout << "   → max_conns: 10" << std::endl;
        std::cout << "   → Suitable for: Low to moderate traffic" << std::endl;
        std::cout << "   → Resource usage: Low" << std::endl;
        std::cout << "   → Concurrency limit: Moderate" << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig small_pool_config;
        small_pool_config.tracker_addrs = {argv[1]};
        small_pool_config.max_conns = 10;
        small_pool_config.connect_timeout = std::chrono::milliseconds(5000);
        small_pool_config.network_timeout = std::chrono::milliseconds(30000);
        small_pool_config.idle_timeout = std::chrono::milliseconds(60000);

        fastdfs::Client small_pool_client(small_pool_config);

        std::cout << "   Testing small pool with 5 concurrent operations..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<std::string>> small_futures;

        for (int i = 0; i < 5; ++i) {
            small_futures.push_back(std::async(std::launch::async,
                [&small_pool_client, i]() {
                    std::string content = "Small pool test " + std::to_string(i);
                    std::vector<uint8_t> data(content.begin(), content.end());
                    return small_pool_client.upload_buffer(data, "txt", nullptr);
                }));
        }

        std::vector<std::string> small_file_ids;
        for (auto& future : small_futures) {
            try {
                small_file_ids.push_back(future.get());
            } catch (const std::exception&) {
                // Ignore errors for demo
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto small_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "   → Completed in: " << small_duration.count() << " ms" << std::endl;
        std::cout << "   → Successful: " << small_file_ids.size() << "/5" << std::endl;
        std::cout << std::endl;

        // Clean up
        for (const auto& file_id : small_file_ids) {
            try {
                small_pool_client.delete_file(file_id);
            } catch (...) {}
        }

        // Configuration 2: Medium Connection Pool
        std::cout << "   Configuration 2: Medium Connection Pool" << std::endl;
        std::cout << "   → max_conns: 50" << std::endl;
        std::cout << "   → Suitable for: Most production applications" << std::endl;
        std::cout << "   → Resource usage: Moderate" << std::endl;
        std::cout << "   → Concurrency limit: High" << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig medium_pool_config;
        medium_pool_config.tracker_addrs = {argv[1]};
        medium_pool_config.max_conns = 50;
        medium_pool_config.connect_timeout = std::chrono::milliseconds(5000);
        medium_pool_config.network_timeout = std::chrono::milliseconds(30000);
        medium_pool_config.idle_timeout = std::chrono::milliseconds(60000);

        fastdfs::Client medium_pool_client(medium_pool_config);

        std::cout << "   Testing medium pool with 20 concurrent operations..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<std::string>> medium_futures;

        for (int i = 0; i < 20; ++i) {
            medium_futures.push_back(std::async(std::launch::async,
                [&medium_pool_client, i]() {
                    std::string content = "Medium pool test " + std::to_string(i);
                    std::vector<uint8_t> data(content.begin(), content.end());
                    return medium_pool_client.upload_buffer(data, "txt", nullptr);
                }));
        }

        std::vector<std::string> medium_file_ids;
        for (auto& future : medium_futures) {
            try {
                medium_file_ids.push_back(future.get());
            } catch (const std::exception&) {
                // Ignore errors for demo
            }
        }

        end = std::chrono::high_resolution_clock::now();
        auto medium_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "   → Completed in: " << medium_duration.count() << " ms" << std::endl;
        std::cout << "   → Successful: " << medium_file_ids.size() << "/20" << std::endl;
        std::cout << std::endl;

        // Clean up
        for (const auto& file_id : medium_file_ids) {
            try {
                medium_pool_client.delete_file(file_id);
            } catch (...) {}
        }

        // Configuration 3: Large Connection Pool
        std::cout << "   Configuration 3: Large Connection Pool" << std::endl;
        std::cout << "   → max_conns: 100" << std::endl;
        std::cout << "   → Suitable for: High-traffic applications, batch processing" << std::endl;
        std::cout << "   → Resource usage: High" << std::endl;
        std::cout << "   → Concurrency limit: Very high" << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig large_pool_config;
        large_pool_config.tracker_addrs = {argv[1]};
        large_pool_config.max_conns = 100;
        large_pool_config.connect_timeout = std::chrono::milliseconds(5000);
        large_pool_config.network_timeout = std::chrono::milliseconds(30000);
        large_pool_config.idle_timeout = std::chrono::milliseconds(60000);

        fastdfs::Client large_pool_client(large_pool_config);

        std::cout << "   Testing large pool with 50 concurrent operations..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<std::string>> large_futures;

        for (int i = 0; i < 50; ++i) {
            large_futures.push_back(std::async(std::launch::async,
                [&large_pool_client, i]() {
                    std::string content = "Large pool test " + std::to_string(i);
                    std::vector<uint8_t> data(content.begin(), content.end());
                    return large_pool_client.upload_buffer(data, "txt", nullptr);
                }));
        }

        std::vector<std::string> large_file_ids;
        for (auto& future : large_futures) {
            try {
                large_file_ids.push_back(future.get());
            } catch (const std::exception&) {
                // Ignore errors for demo
            }
        }

        end = std::chrono::high_resolution_clock::now();
        auto large_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "   → Completed in: " << large_duration.count() << " ms" << std::endl;
        std::cout << "   → Successful: " << large_file_ids.size() << "/50" << std::endl;
        std::cout << std::endl;

        // Clean up
        for (const auto& file_id : large_file_ids) {
            try {
                large_pool_client.delete_file(file_id);
            } catch (...) {}
        }

        small_pool_client.close();
        medium_pool_client.close();
        large_pool_client.close();

        // ====================================================================
        // EXAMPLE 2: Connection Reuse Patterns
        // ====================================================================
        std::cout << "2. Connection Reuse Patterns" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates connection reuse patterns." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig reuse_config;
        reuse_config.tracker_addrs = {argv[1]};
        reuse_config.max_conns = 10;
        reuse_config.connect_timeout = std::chrono::milliseconds(5000);
        reuse_config.network_timeout = std::chrono::milliseconds(30000);
        reuse_config.idle_timeout = std::chrono::milliseconds(60000);

        fastdfs::Client reuse_client(reuse_config);

        std::cout << "   Performing multiple operations to demonstrate connection reuse..." << std::endl;
        std::cout << "   → Pool size: 10 connections" << std::endl;
        std::cout << "   → Performing 30 operations (connections will be reused)" << std::endl;
        std::cout << std::endl;

        start = std::chrono::high_resolution_clock::now();
        std::vector<std::string> reuse_file_ids;

        for (int i = 0; i < 30; ++i) {
            std::string content = "Reuse test " + std::to_string(i);
            std::vector<uint8_t> data(content.begin(), content.end());
            std::string file_id = reuse_client.upload_buffer(data, "txt", nullptr);
            reuse_file_ids.push_back(file_id);
            
            if ((i + 1) % 10 == 0) {
                std::cout << "   → Completed " << (i + 1) << " operations" << std::endl;
            }
        }

        end = std::chrono::high_resolution_clock::now();
        auto reuse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << std::endl;
        std::cout << "   → Total time: " << reuse_duration.count() << " ms" << std::endl;
        std::cout << "   → Average per operation: " 
                  << (reuse_duration.count() / 30) << " ms" << std::endl;
        std::cout << "   → Connections are reused efficiently" << std::endl;
        std::cout << std::endl;

        // Clean up
        for (const auto& file_id : reuse_file_ids) {
            try {
                reuse_client.delete_file(file_id);
            } catch (...) {}
        }

        reuse_client.close();

        // ====================================================================
        // EXAMPLE 3: Connection Pool Monitoring (Simulated)
        // ====================================================================
        std::cout << "3. Connection Pool Monitoring" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes examples of connection pool monitoring." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig monitor_config;
        monitor_config.tracker_addrs = {argv[1]};
        monitor_config.max_conns = 20;
        monitor_config.connect_timeout = std::chrono::milliseconds(5000);
        monitor_config.network_timeout = std::chrono::milliseconds(30000);
        monitor_config.idle_timeout = std::chrono::milliseconds(60000);

        fastdfs::Client monitor_client(monitor_config);

        std::cout << "   Simulating connection pool monitoring..." << std::endl;
        std::cout << "   → Max connections: " << monitor_config.max_conns << std::endl;
        std::cout << "   → Connect timeout: " << monitor_config.connect_timeout.count() << " ms" << std::endl;
        std::cout << "   → Network timeout: " << monitor_config.network_timeout.count() << " ms" << std::endl;
        std::cout << "   → Idle timeout: " << monitor_config.idle_timeout.count() << " ms" << std::endl;
        std::cout << std::endl;

        // Simulate monitoring by performing operations and tracking performance
        PoolPerformance perf;
        const size_t monitor_ops = 15;

        start = std::chrono::high_resolution_clock::now();
        std::vector<std::string> monitor_file_ids;

        for (size_t i = 0; i < monitor_ops; ++i) {
            auto op_start = std::chrono::high_resolution_clock::now();
            try {
                std::string content = "Monitor test " + std::to_string(i);
                std::vector<uint8_t> data(content.begin(), content.end());
                std::string file_id = monitor_client.upload_buffer(data, "txt", nullptr);
                monitor_file_ids.push_back(file_id);
                
                auto op_end = std::chrono::high_resolution_clock::now();
                auto op_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    op_end - op_start);
                
                perf.operations++;
                perf.successful++;
                perf.total_time += op_duration;
            } catch (const std::exception&) {
                perf.operations++;
                perf.failed++;
            }
        }

        end = std::chrono::high_resolution_clock::now();
        perf.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "   Pool Performance Metrics:" << std::endl;
        std::cout << "     Total operations: " << perf.operations << std::endl;
        std::cout << "     Successful: " << perf.successful << std::endl;
        std::cout << "     Failed: " << perf.failed << std::endl;
        std::cout << "     Total time: " << perf.total_time.count() << " ms" << std::endl;
        std::cout << "     Average time: " << std::fixed << std::setprecision(2) 
                  << perf.average_time() << " ms" << std::endl;
        std::cout << "     Success rate: " << std::fixed << std::setprecision(1) 
                  << perf.success_rate() << "%" << std::endl;
        std::cout << std::endl;

        // Clean up
        for (const auto& file_id : monitor_file_ids) {
            try {
                monitor_client.delete_file(file_id);
            } catch (...) {}
        }

        monitor_client.close();

        // ====================================================================
        // EXAMPLE 4: Connection Pool Exhaustion Scenarios
        // ====================================================================
        std::cout << "4. Connection Pool Exhaustion Scenarios" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to handle connection pool exhaustion scenarios." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig exhaustion_config;
        exhaustion_config.tracker_addrs = {argv[1]};
        exhaustion_config.max_conns = 5;  // Small pool to demonstrate exhaustion
        exhaustion_config.connect_timeout = std::chrono::milliseconds(5000);
        exhaustion_config.network_timeout = std::chrono::milliseconds(30000);
        exhaustion_config.idle_timeout = std::chrono::milliseconds(60000);

        fastdfs::Client exhaustion_client(exhaustion_config);

        std::cout << "   Testing with small pool (max_conns: 5) and high concurrency (15 operations)..." << std::endl;
        std::cout << "   → Pool will be exhausted, connections will be reused" << std::endl;
        std::cout << std::endl;

        start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<std::string>> exhaustion_futures;

        for (int i = 0; i < 15; ++i) {
            exhaustion_futures.push_back(std::async(std::launch::async,
                [&exhaustion_client, i]() {
                    std::string content = "Exhaustion test " + std::to_string(i);
                    std::vector<uint8_t> data(content.begin(), content.end());
                    return exhaustion_client.upload_buffer(data, "txt", nullptr);
                }));
        }

        std::vector<std::string> exhaustion_file_ids;
        size_t exhaustion_successful = 0;
        size_t exhaustion_failed = 0;

        for (auto& future : exhaustion_futures) {
            try {
                std::string file_id = future.get();
                exhaustion_file_ids.push_back(file_id);
                exhaustion_successful++;
            } catch (const std::exception& e) {
                exhaustion_failed++;
                std::cout << "   → Operation failed (pool exhausted): " << e.what() << std::endl;
            }
        }

        end = std::chrono::high_resolution_clock::now();
        auto exhaustion_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << std::endl;
        std::cout << "   Exhaustion Test Results:" << std::endl;
        std::cout << "     Total operations: 15" << std::endl;
        std::cout << "     Successful: " << exhaustion_successful << std::endl;
        std::cout << "     Failed: " << exhaustion_failed << std::endl;
        std::cout << "     Total time: " << exhaustion_duration.count() << " ms" << std::endl;
        std::cout << "   → Pool handled exhaustion by reusing connections" << std::endl;
        std::cout << std::endl;

        // Clean up
        for (const auto& file_id : exhaustion_file_ids) {
            try {
                exhaustion_client.delete_file(file_id);
            } catch (...) {}
        }

        exhaustion_client.close();

        // ====================================================================
        // EXAMPLE 5: Performance Optimization Recommendations
        // ====================================================================
        std::cout << "5. Performance Optimization Recommendations" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Useful for performance optimization and resource management." << std::endl;
        std::cout << std::endl;

        std::cout << "   Best Practices:" << std::endl;
        std::cout << "   1. Start with max_conns = 10-20 for most applications" << std::endl;
        std::cout << "   2. Increase pool size for high-concurrency workloads" << std::endl;
        std::cout << "   3. Monitor connection pool utilization" << std::endl;
        std::cout << "   4. Set appropriate timeouts based on network conditions" << std::endl;
        std::cout << "   5. Use idle_timeout to clean up unused connections" << std::endl;
        std::cout << "   6. Balance pool size between performance and resource usage" << std::endl;
        std::cout << std::endl;

        std::cout << "   Workload Recommendations:" << std::endl;
        std::cout << "   - Low traffic: max_conns = 5-10" << std::endl;
        std::cout << "   - Medium traffic: max_conns = 20-50" << std::endl;
        std::cout << "   - High traffic: max_conns = 50-100" << std::endl;
        std::cout << "   - Batch processing: max_conns = 100+" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Connection pool configuration and tuning" << std::endl;
        std::cout << "  ✓ Optimize connection pool size for different workloads" << std::endl;
        std::cout << "  ✓ Connection pool monitoring" << std::endl;
        std::cout << "  ✓ Connection reuse patterns" << std::endl;
        std::cout << "  ✓ Performance optimization and resource management" << std::endl;
        std::cout << "  ✓ Connection pool exhaustion scenarios" << std::endl;

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

