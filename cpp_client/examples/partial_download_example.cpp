/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Partial Download Example
 *
 * This example demonstrates partial file download capabilities with the FastDFS client.
 * It covers downloading specific byte ranges, resuming interrupted downloads,
 * extracting portions of files, and memory-efficient download patterns.
 *
 * Key Topics Covered:
 * - Download specific byte ranges from files
 * - Efficient handling of large files by downloading only needed portions
 * - Resumable download patterns
 * - Streaming media and large file processing
 * - Bandwidth optimization
 * - Parallel chunk downloads
 *
 * Run this example with:
 *   ./partial_download_example <tracker_address>
 *   Example: ./partial_download_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <iomanip>
#include <chrono>

// Helper function to verify downloaded data matches expected pattern
bool verify_data(const std::vector<uint8_t>& data, int64_t expected_offset) {
    if (data.empty()) return false;
    
    for (size_t i = 0; i < data.size(); ++i) {
        uint8_t expected = static_cast<uint8_t>((expected_offset + i) % 256);
        if (data[i] != expected) {
            return false;
        }
    }
    return true;
}

// Helper function to format data preview
std::string format_data_preview(const std::vector<uint8_t>& data, size_t max_bytes = 10) {
    if (data.empty()) return "empty";
    
    std::ostringstream oss;
    size_t preview_size = std::min(data.size(), max_bytes);
    for (size_t i = 0; i < preview_size; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2) 
            << static_cast<int>(data[i]);
        if (i < preview_size - 1) oss << " ";
    }
    if (data.size() > preview_size) oss << "...";
    return oss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Partial Download Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // STEP 1: Configure and Create Client
        // ====================================================================
        std::cout << "1. Configuring FastDFS Client..." << std::endl;
        fastdfs::ClientConfig config;
        config.tracker_addrs = {argv[1]};
        config.max_conns = 10;
        config.connect_timeout = std::chrono::milliseconds(5000);
        config.network_timeout = std::chrono::milliseconds(30000);

        fastdfs::Client client(config);
        std::cout << "   ✓ Client initialized successfully" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // STEP 2: Prepare Test File
        // ====================================================================
        std::cout << "2. Preparing test file for partial download examples..." << std::endl;
        
        // Create test data with sequential bytes (makes verification easy)
        const int64_t file_size = 10000; // 10KB test file
        std::vector<uint8_t> test_data(file_size);
        for (int64_t i = 0; i < file_size; ++i) {
            test_data[i] = static_cast<uint8_t>(i % 256);
        }

        std::string file_id = client.upload_buffer(test_data, "bin", nullptr);
        std::cout << "   ✓ Test file uploaded: " << file_id << std::endl;
        std::cout << "   File size: " << file_size << " bytes" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Download Specific Byte Ranges
        // ====================================================================
        std::cout << "3. Download Specific Byte Ranges" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to download specific byte ranges from files." << std::endl;
        std::cout << "   Useful for streaming media, large file processing, and bandwidth optimization." << std::endl;
        std::cout << std::endl;

        // Range 1: Download from the beginning (file header)
        std::cout << "   Range 1: First 100 bytes (header/metadata)" << std::endl;
        std::cout << "   → Offset: 0, Length: 100" << std::endl;
        auto start1 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> range1 = client.download_file_range(file_id, 0, 100);
        auto end1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
        
        std::cout << "   ✓ Downloaded " << range1.size() << " bytes in " 
                  << duration1.count() << " ms" << std::endl;
        std::cout << "   → Data preview: " << format_data_preview(range1) << std::endl;
        std::cout << "   → Verified: " << (verify_data(range1, 0) ? "✓" : "✗") << std::endl;
        std::cout << std::endl;

        // Range 2: Download from the middle
        std::cout << "   Range 2: Middle section (bytes 4000-4100)" << std::endl;
        std::cout << "   → Offset: 4000, Length: 100" << std::endl;
        auto start2 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> range2 = client.download_file_range(file_id, 4000, 100);
        auto end2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
        
        std::cout << "   ✓ Downloaded " << range2.size() << " bytes in " 
                  << duration2.count() << " ms" << std::endl;
        std::cout << "   → Data preview: " << format_data_preview(range2) << std::endl;
        std::cout << "   → Verified: " << (verify_data(range2, 4000) ? "✓" : "✗") << std::endl;
        std::cout << std::endl;

        // Range 3: Download from the end (file trailer)
        std::cout << "   Range 3: Last 100 bytes (trailer/recent data)" << std::endl;
        std::cout << "   → Offset: 9900, Length: 100" << std::endl;
        auto start3 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> range3 = client.download_file_range(file_id, 9900, 100);
        auto end3 = std::chrono::high_resolution_clock::now();
        auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(end3 - start3);
        
        std::cout << "   ✓ Downloaded " << range3.size() << " bytes in " 
                  << duration3.count() << " ms" << std::endl;
        std::cout << "   → Data preview: " << format_data_preview(range3) << std::endl;
        std::cout << "   → Verified: " << (verify_data(range3, 9900) ? "✓" : "✗") << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Download to End of File
        // ====================================================================
        std::cout << "4. Download from Offset to End of File" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   When length is 0, downloads from offset to end of file." << std::endl;
        std::cout << std::endl;

        std::cout << "   Downloading from byte 5000 to end of file..." << std::endl;
        std::cout << "   → Offset: 5000, Length: 0 (to end)" << std::endl;
        auto start4 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> range4 = client.download_file_range(file_id, 5000, 0);
        auto end4 = std::chrono::high_resolution_clock::now();
        auto duration4 = std::chrono::duration_cast<std::chrono::milliseconds>(end4 - start4);
        
        std::cout << "   ✓ Downloaded " << range4.size() << " bytes in " 
                  << duration4.count() << " ms" << std::endl;
        std::cout << "   → Expected size: " << (file_size - 5000) << " bytes" << std::endl;
        std::cout << "   → Verified: " << (verify_data(range4, 5000) ? "✓" : "✗") << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Resumable Download Pattern
        // ====================================================================
        std::cout << "5. Resumable Download Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates how to resume an interrupted download." << std::endl;
        std::cout << "   Includes examples for resumable downloads." << std::endl;
        std::cout << std::endl;

        // Simulate partial download
        int64_t downloaded_bytes = 3000;
        std::cout << "   Simulating interrupted download..." << std::endl;
        std::cout << "   → Already downloaded: " << downloaded_bytes << " bytes" << std::endl;
        std::cout << "   → Resuming from offset: " << downloaded_bytes << std::endl;
        
        auto start5 = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> remaining = client.download_file_range(file_id, downloaded_bytes, 0);
        auto end5 = std::chrono::high_resolution_clock::now();
        auto duration5 = std::chrono::duration_cast<std::chrono::milliseconds>(end5 - start5);
        
        std::cout << "   ✓ Downloaded remaining " << remaining.size() << " bytes in " 
                  << duration5.count() << " ms" << std::endl;
        std::cout << "   → Total file size: " << (downloaded_bytes + remaining.size()) << " bytes" << std::endl;
        std::cout << "   → Verified: " << (verify_data(remaining, downloaded_bytes) ? "✓" : "✗") << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Chunked Download Pattern
        // ====================================================================
        std::cout << "6. Chunked Download Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Downloading a large file in smaller chunks for memory efficiency." << std::endl;
        std::cout << "   Demonstrates efficient handling of large files." << std::endl;
        std::cout << std::endl;

        const int64_t chunk_size = 1000;
        int64_t total_chunks = (file_size + chunk_size - 1) / chunk_size;
        std::cout << "   Downloading file in chunks of " << chunk_size << " bytes" << std::endl;
        std::cout << "   → Total chunks: " << total_chunks << std::endl;
        std::cout << std::endl;

        auto chunk_start = std::chrono::high_resolution_clock::now();
        std::vector<std::vector<uint8_t>> chunks;
        bool all_chunks_valid = true;

        for (int64_t i = 0; i < total_chunks; ++i) {
            int64_t offset = i * chunk_size;
            int64_t length = std::min(chunk_size, file_size - offset);
            
            std::vector<uint8_t> chunk = client.download_file_range(file_id, offset, length);
            chunks.push_back(chunk);
            
            if (!verify_data(chunk, offset)) {
                all_chunks_valid = false;
            }
            
            if ((i + 1) % 3 == 0 || i == total_chunks - 1) {
                std::cout << "   → Downloaded chunk " << (i + 1) << "/" << total_chunks 
                          << " (" << chunk.size() << " bytes)" << std::endl;
            }
        }

        auto chunk_end = std::chrono::high_resolution_clock::now();
        auto chunk_duration = std::chrono::duration_cast<std::chrono::milliseconds>(chunk_end - chunk_start);
        
        int64_t total_downloaded = 0;
        for (const auto& chunk : chunks) {
            total_downloaded += chunk.size();
        }

        std::cout << std::endl;
        std::cout << "   ✓ Downloaded " << total_chunks << " chunks (" 
                  << total_downloaded << " bytes) in " << chunk_duration.count() << " ms" << std::endl;
        std::cout << "   → All chunks verified: " << (all_chunks_valid ? "✓" : "✗") << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Parallel Chunk Downloads
        // ====================================================================
        std::cout << "7. Parallel Chunk Downloads" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to download file chunks in parallel." << std::endl;
        std::cout << "   Downloading multiple chunks in parallel for better performance." << std::endl;
        std::cout << std::endl;

        const int64_t parallel_chunk_size = 2000;
        const int num_parallel_chunks = 4;
        std::cout << "   Downloading " << num_parallel_chunks << " chunks in parallel" << std::endl;
        std::cout << "   → Chunk size: " << parallel_chunk_size << " bytes" << std::endl;
        std::cout << std::endl;

        auto parallel_start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<std::vector<uint8_t>>> futures;

        // Launch parallel downloads
        for (int i = 0; i < num_parallel_chunks; ++i) {
            int64_t offset = i * parallel_chunk_size;
            int64_t length = std::min(parallel_chunk_size, file_size - offset);
            
            futures.push_back(std::async(std::launch::async, [&client, file_id, offset, length]() {
                return client.download_file_range(file_id, offset, length);
            }));
        }

        // Collect results
        std::vector<std::vector<uint8_t>> parallel_chunks;
        for (size_t i = 0; i < futures.size(); ++i) {
            std::vector<uint8_t> chunk = futures[i].get();
            parallel_chunks.push_back(chunk);
            
            int64_t expected_offset = i * parallel_chunk_size;
            bool valid = verify_data(chunk, expected_offset);
            std::cout << "   → Chunk " << (i + 1) << ": " << chunk.size() 
                      << " bytes, Verified: " << (valid ? "✓" : "✗") << std::endl;
        }

        auto parallel_end = std::chrono::high_resolution_clock::now();
        auto parallel_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parallel_end - parallel_start);
        
        int64_t parallel_total = 0;
        for (const auto& chunk : parallel_chunks) {
            parallel_total += chunk.size();
        }

        std::cout << std::endl;
        std::cout << "   ✓ Downloaded " << parallel_total << " bytes in " 
                  << parallel_duration.count() << " ms (parallel)" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: Extract File Portions
        // ====================================================================
        std::cout << "8. Extract File Portions" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Extracting specific portions of a file (e.g., headers, sections)." << std::endl;
        std::cout << std::endl;

        // Extract header (first 256 bytes)
        std::cout << "   Extracting file header (first 256 bytes)..." << std::endl;
        std::vector<uint8_t> header = client.download_file_range(file_id, 0, 256);
        std::cout << "   ✓ Extracted " << header.size() << " bytes" << std::endl;
        std::cout << std::endl;

        // Extract middle section
        std::cout << "   Extracting middle section (bytes 3000-3500)..." << std::endl;
        std::vector<uint8_t> middle = client.download_file_range(file_id, 3000, 500);
        std::cout << "   ✓ Extracted " << middle.size() << " bytes" << std::endl;
        std::cout << std::endl;

        // Extract trailer (last 256 bytes)
        std::cout << "   Extracting file trailer (last 256 bytes)..." << std::endl;
        std::vector<uint8_t> trailer = client.download_file_range(file_id, file_size - 256, 256);
        std::cout << "   ✓ Extracted " << trailer.size() << " bytes" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // CLEANUP
        // ====================================================================
        std::cout << "9. Cleaning up test file..." << std::endl;
        client.delete_file(file_id);
        std::cout << "   ✓ Test file deleted successfully" << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::endl << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Download specific byte ranges from files" << std::endl;
        std::cout << "  ✓ Efficient handling of large files by downloading only needed portions" << std::endl;
        std::cout << "  ✓ Resumable download patterns" << std::endl;
        std::cout << "  ✓ Chunked downloads for memory efficiency" << std::endl;
        std::cout << "  ✓ Parallel chunk downloads for performance" << std::endl;
        std::cout << "  ✓ Extract file portions (header, sections, trailer)" << std::endl;
        std::cout << "  ✓ Useful for streaming media, large file processing, and bandwidth optimization" << std::endl;

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

