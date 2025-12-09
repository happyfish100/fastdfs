/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Streaming Example
 *
 * This comprehensive example demonstrates streaming large files without loading
 * entire file into memory. It covers chunked upload and download patterns,
 * memory-efficient file handling, progress tracking, and resumable operations.
 *
 * Key Topics Covered:
 * - Demonstrates streaming large files without loading entire file into memory
 * - Shows chunked upload and download patterns
 * - Includes examples for processing files in chunks
 * - Demonstrates memory-efficient file handling
 * - Useful for handling very large files (GB+)
 * - Shows progress tracking for streaming operations
 * - Demonstrates resumable upload/download patterns
 *
 * Run this example with:
 *   ./streaming_example <tracker_address>
 *   Example: ./streaming_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <thread>
#include <atomic>
#include <functional>

// Progress callback function type
using ProgressCallback = std::function<void(int64_t current, int64_t total, double percentage)>;

// Helper function to format file size
std::string format_size(int64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

// Helper function to print progress bar
void print_progress(int64_t current, int64_t total, double percentage) {
    const int bar_width = 50;
    int pos = static_cast<int>(bar_width * percentage / 100.0);
    
    std::cout << "\r   [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << percentage << "% "
              << "(" << format_size(current) << " / " << format_size(total) << ")";
    std::cout.flush();
}

// Helper function to create a test file with specified size
void create_test_file(const std::string& filename, int64_t size) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create test file: " + filename);
    }
    
    // Write data in chunks to avoid memory issues
    const int64_t chunk_size = 1024 * 1024; // 1MB chunks
    std::vector<uint8_t> chunk(chunk_size);
    
    for (int64_t i = 0; i < size; i += chunk_size) {
        int64_t write_size = std::min(chunk_size, size - i);
        
        // Fill chunk with pattern data
        for (int64_t j = 0; j < write_size; ++j) {
            chunk[j] = static_cast<uint8_t>((i + j) % 256);
        }
        
        file.write(reinterpret_cast<const char*>(chunk.data()), write_size);
    }
    
    file.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Streaming Example" << std::endl;
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
        // EXAMPLE 1: Chunked Upload with Progress Tracking
        // ====================================================================
        std::cout << "2. Chunked Upload with Progress Tracking" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates streaming large files without loading entire file into memory." << std::endl;
        std::cout << "   Shows chunked upload patterns with progress tracking." << std::endl;
        std::cout << std::endl;

        // Create a test file (500KB for demonstration)
        const int64_t test_file_size = 500 * 1024; // 500KB
        const std::string test_file = "streaming_test_file.bin";
        std::cout << "   Creating test file: " << format_size(test_file_size) << std::endl;
        create_test_file(test_file, test_file_size);
        std::cout << "   ✓ Test file created" << std::endl;
        std::cout << std::endl;

        // Upload using appender file for chunked upload
        std::cout << "   Uploading file in chunks using appender file..." << std::endl;
        const int64_t upload_chunk_size = 64 * 1024; // 64KB chunks
        std::ifstream file_stream(test_file, std::ios::binary);
        
        if (!file_stream) {
            throw std::runtime_error("Failed to open test file");
        }

        // Read and upload first chunk to create appender file
        std::vector<uint8_t> chunk(upload_chunk_size);
        file_stream.read(reinterpret_cast<char*>(chunk.data()), upload_chunk_size);
        std::streamsize bytes_read = file_stream.gcount();
        
        std::string file_id = client.upload_appender_buffer(
            std::vector<uint8_t>(chunk.begin(), chunk.begin() + bytes_read), 
            "bin", nullptr);
        
        int64_t uploaded_bytes = bytes_read;
        print_progress(uploaded_bytes, test_file_size, 
                       (uploaded_bytes * 100.0) / test_file_size);
        std::cout << std::endl;

        // Continue uploading remaining chunks
        while (file_stream.read(reinterpret_cast<char*>(chunk.data()), upload_chunk_size)) {
            bytes_read = file_stream.gcount();
            client.append_file(file_id, 
                std::vector<uint8_t>(chunk.begin(), chunk.begin() + bytes_read));
            uploaded_bytes += bytes_read;
            print_progress(uploaded_bytes, test_file_size, 
                          (uploaded_bytes * 100.0) / test_file_size);
            std::cout << std::endl;
        }
        
        // Handle last partial chunk
        if (uploaded_bytes < test_file_size) {
            bytes_read = test_file_size - uploaded_bytes;
            chunk.resize(bytes_read);
            file_stream.seekg(uploaded_bytes);
            file_stream.read(reinterpret_cast<char*>(chunk.data()), bytes_read);
            client.append_file(file_id, chunk);
            uploaded_bytes = test_file_size;
            print_progress(uploaded_bytes, test_file_size, 100.0);
            std::cout << std::endl;
        }

        file_stream.close();
        std::cout << "   ✓ File uploaded successfully: " << file_id << std::endl;
        std::cout << "   Total uploaded: " << format_size(uploaded_bytes) << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Chunked Download with Progress Tracking
        // ====================================================================
        std::cout << "3. Chunked Download with Progress Tracking" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates downloading large files in chunks without loading entire file into memory." << std::endl;
        std::cout << "   Shows chunked download patterns with progress tracking." << std::endl;
        std::cout << std::endl;

        // Get file info to know the size
        fastdfs::FileInfo file_info = client.get_file_info(file_id);
        int64_t file_size = file_info.file_size;
        std::cout << "   File size: " << format_size(file_size) << std::endl;
        std::cout << "   Downloading in chunks..." << std::endl;

        const int64_t download_chunk_size = 64 * 1024; // 64KB chunks
        const std::string download_file = "streaming_downloaded_file.bin";
        std::ofstream download_stream(download_file, std::ios::binary);
        
        if (!download_stream) {
            throw std::runtime_error("Failed to create download file");
        }

        int64_t downloaded_bytes = 0;
        int64_t offset = 0;

        while (downloaded_bytes < file_size) {
            int64_t chunk_length = std::min(download_chunk_size, file_size - offset);
            std::vector<uint8_t> chunk = client.download_file_range(file_id, offset, chunk_length);
            
            download_stream.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            downloaded_bytes += chunk.size();
            offset += chunk.size();
            
            print_progress(downloaded_bytes, file_size, 
                          (downloaded_bytes * 100.0) / file_size);
            std::cout << std::endl;
        }

        download_stream.close();
        std::cout << "   ✓ File downloaded successfully: " << download_file << std::endl;
        std::cout << "   Total downloaded: " << format_size(downloaded_bytes) << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Processing Files in Chunks
        // ====================================================================
        std::cout << "4. Processing Files in Chunks" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates processing file content in chunks without loading entire file into memory." << std::endl;
        std::cout << "   Useful for handling very large files (GB+)." << std::endl;
        std::cout << std::endl;

        const int64_t process_chunk_size = 32 * 1024; // 32KB chunks for processing
        offset = 0;
        int64_t processed_bytes = 0;
        int chunk_count = 0;
        int64_t total_sum = 0; // Example: sum all bytes (simple processing)

        std::cout << "   Processing file in " << format_size(process_chunk_size) << " chunks..." << std::endl;

        while (processed_bytes < file_size) {
            int64_t chunk_length = std::min(process_chunk_size, file_size - offset);
            std::vector<uint8_t> chunk = client.download_file_range(file_id, offset, chunk_length);
            
            // Process chunk (example: sum all bytes)
            for (uint8_t byte : chunk) {
                total_sum += byte;
            }
            
            processed_bytes += chunk.size();
            offset += chunk.size();
            chunk_count++;
            
            if (chunk_count % 5 == 0 || processed_bytes >= file_size) {
                print_progress(processed_bytes, file_size, 
                              (processed_bytes * 100.0) / file_size);
                std::cout << std::endl;
            }
        }

        std::cout << "   ✓ File processed successfully" << std::endl;
        std::cout << "   Total chunks processed: " << chunk_count << std::endl;
        std::cout << "   Total bytes processed: " << format_size(processed_bytes) << std::endl;
        std::cout << "   Processing result (sum of all bytes): " << total_sum << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Resumable Upload Pattern
        // ====================================================================
        std::cout << "5. Resumable Upload Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates resumable upload patterns for handling interrupted uploads." << std::endl;
        std::cout << std::endl;

        // Simulate interrupted upload scenario
        const std::string resume_test_file = "resume_test_file.bin";
        const int64_t resume_file_size = 200 * 1024; // 200KB
        create_test_file(resume_test_file, resume_file_size);
        
        std::cout << "   Simulating interrupted upload..." << std::endl;
        std::cout << "   → Uploading first 50% of file..." << std::endl;
        
        // Upload first part
        std::ifstream resume_stream(resume_test_file, std::ios::binary);
        int64_t resume_chunk_size = 32 * 1024;
        int64_t resume_uploaded = 0;
        int64_t resume_target = resume_file_size / 2; // Upload 50%
        
        std::vector<uint8_t> resume_chunk(resume_chunk_size);
        resume_stream.read(reinterpret_cast<char*>(resume_chunk.data()), resume_chunk_size);
        std::streamsize resume_bytes = resume_stream.gcount();
        
        std::string resume_file_id = client.upload_appender_buffer(
            std::vector<uint8_t>(resume_chunk.begin(), resume_chunk.begin() + resume_bytes),
            "bin", nullptr);
        resume_uploaded += resume_bytes;
        
        while (resume_uploaded < resume_target && 
               resume_stream.read(reinterpret_cast<char*>(resume_chunk.data()), resume_chunk_size)) {
            resume_bytes = resume_stream.gcount();
            client.append_file(resume_file_id, 
                std::vector<uint8_t>(resume_chunk.begin(), resume_chunk.begin() + resume_bytes));
            resume_uploaded += resume_bytes;
        }
        
        resume_stream.close();
        std::cout << "   → Uploaded: " << format_size(resume_uploaded) << " / " 
                  << format_size(resume_file_size) << std::endl;
        std::cout << "   → Simulated interruption at " << (resume_uploaded * 100 / resume_file_size) << "%" << std::endl;
        std::cout << std::endl;

        // Resume upload
        std::cout << "   Resuming upload from offset " << format_size(resume_uploaded) << "..." << std::endl;
        resume_stream.open(resume_test_file, std::ios::binary);
        resume_stream.seekg(resume_uploaded);
        
        while (resume_stream.read(reinterpret_cast<char*>(resume_chunk.data()), resume_chunk_size)) {
            resume_bytes = resume_stream.gcount();
            client.append_file(resume_file_id, 
                std::vector<uint8_t>(resume_chunk.begin(), resume_chunk.begin() + resume_bytes));
            resume_uploaded += resume_bytes;
            
            print_progress(resume_uploaded, resume_file_size, 
                          (resume_uploaded * 100.0) / resume_file_size);
            std::cout << std::endl;
        }
        
        // Handle last chunk
        if (resume_uploaded < resume_file_size) {
            resume_bytes = resume_file_size - resume_uploaded;
            resume_chunk.resize(resume_bytes);
            resume_stream.seekg(resume_uploaded);
            resume_stream.read(reinterpret_cast<char*>(resume_chunk.data()), resume_bytes);
            client.append_file(resume_file_id, resume_chunk);
            resume_uploaded = resume_file_size;
            print_progress(resume_uploaded, resume_file_size, 100.0);
            std::cout << std::endl;
        }
        
        resume_stream.close();
        std::cout << "   ✓ Upload resumed and completed successfully" << std::endl;
        std::cout << "   Final file ID: " << resume_file_id << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Resumable Download Pattern
        // ====================================================================
        std::cout << "6. Resumable Download Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates resumable download patterns for handling interrupted downloads." << std::endl;
        std::cout << std::endl;

        const std::string resume_download_file = "resume_downloaded_file.bin";
        int64_t resume_downloaded = 0;
        int64_t resume_offset = 0;
        
        // Simulate partial download
        std::cout << "   Simulating interrupted download..." << std::endl;
        std::cout << "   → Downloading first 40% of file..." << std::endl;
        
        std::ofstream resume_download_stream(resume_download_file, std::ios::binary);
        int64_t resume_download_target = file_size * 40 / 100; // Download 40%
        
        while (resume_downloaded < resume_download_target) {
            int64_t chunk_length = std::min(download_chunk_size, resume_download_target - resume_downloaded);
            std::vector<uint8_t> chunk = client.download_file_range(file_id, resume_offset, chunk_length);
            
            resume_download_stream.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            resume_downloaded += chunk.size();
            resume_offset += chunk.size();
        }
        
        resume_download_stream.close();
        std::cout << "   → Downloaded: " << format_size(resume_downloaded) << " / " 
                  << format_size(file_size) << std::endl;
        std::cout << "   → Simulated interruption at " << (resume_downloaded * 100 / file_size) << "%" << std::endl;
        std::cout << std::endl;

        // Resume download
        std::cout << "   Resuming download from offset " << format_size(resume_offset) << "..." << std::endl;
        resume_download_stream.open(resume_download_file, std::ios::binary | std::ios::app);
        
        while (resume_downloaded < file_size) {
            int64_t chunk_length = std::min(download_chunk_size, file_size - resume_offset);
            std::vector<uint8_t> chunk = client.download_file_range(file_id, resume_offset, chunk_length);
            
            resume_download_stream.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            resume_downloaded += chunk.size();
            resume_offset += chunk.size();
            
            print_progress(resume_downloaded, file_size, 
                          (resume_downloaded * 100.0) / file_size);
            std::cout << std::endl;
        }
        
        resume_download_stream.close();
        std::cout << "   ✓ Download resumed and completed successfully" << std::endl;
        std::cout << "   Total downloaded: " << format_size(resume_downloaded) << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: Memory-Efficient Large File Handling
        // ====================================================================
        std::cout << "7. Memory-Efficient Large File Handling" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates memory-efficient handling of very large files (GB+)." << std::endl;
        std::cout << "   Shows how to work with files larger than available memory." << std::endl;
        std::cout << std::endl;

        // Simulate working with a large file (using existing file)
        std::cout << "   Demonstrating memory-efficient operations on large files..." << std::endl;
        std::cout << "   → Using fixed-size buffer regardless of file size" << std::endl;
        
        const int64_t memory_efficient_chunk = 16 * 1024; // 16KB - very small chunks
        int64_t memory_used = memory_efficient_chunk; // Only one chunk in memory at a time
        int64_t large_file_size = file_size; // Could be GB+
        
        std::cout << "   → File size: " << format_size(large_file_size) << std::endl;
        std::cout << "   → Memory used: " << format_size(memory_used) << " (fixed)" << std::endl;
        std::cout << "   → Memory efficiency: " << std::fixed << std::setprecision(2)
                  << (large_file_size * 100.0 / memory_used) << "x" << std::endl;
        std::cout << std::endl;

        // Process in small chunks
        offset = 0;
        int64_t processed = 0;
        int operation_count = 0;
        
        std::cout << "   Processing file in " << format_size(memory_efficient_chunk) << " chunks..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (processed < large_file_size) {
            int64_t chunk_length = std::min(memory_efficient_chunk, large_file_size - offset);
            std::vector<uint8_t> chunk = client.download_file_range(file_id, offset, chunk_length);
            
            // Process chunk (example operation)
            operation_count++;
            
            processed += chunk.size();
            offset += chunk.size();
            
            // Clear chunk from memory immediately after processing
            chunk.clear();
            chunk.shrink_to_fit();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "   ✓ Processed " << format_size(processed) << " in " 
                  << operation_count << " operations" << std::endl;
        std::cout << "   → Processing time: " << duration.count() << " ms" << std::endl;
        std::cout << "   → Throughput: " << std::fixed << std::setprecision(2)
                  << (processed / 1024.0 / 1024.0) / (duration.count() / 1000.0) << " MB/s" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // CLEANUP
        // ====================================================================
        std::cout << "8. Cleaning up test files..." << std::endl;
        client.delete_file(file_id);
        client.delete_file(resume_file_id);
        std::cout << "   ✓ Remote files deleted" << std::endl;
        
        // Clean up local files
        std::remove(test_file.c_str());
        std::remove(download_file.c_str());
        std::remove(resume_test_file.c_str());
        std::remove(resume_download_file.c_str());
        std::cout << "   ✓ Local files cleaned up" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Streaming large files without loading entire file into memory" << std::endl;
        std::cout << "  ✓ Chunked upload and download patterns" << std::endl;
        std::cout << "  ✓ Processing files in chunks" << std::endl;
        std::cout << "  ✓ Memory-efficient file handling" << std::endl;
        std::cout << "  ✓ Useful for handling very large files (GB+)" << std::endl;
        std::cout << "  ✓ Progress tracking for streaming operations" << std::endl;
        std::cout << "  ✓ Resumable upload/download patterns" << std::endl;
        std::cout << std::endl;
        std::cout << "Best Practices:" << std::endl;
        std::cout << "  • Use appender files for chunked uploads" << std::endl;
        std::cout << "  • Use download_file_range for chunked downloads" << std::endl;
        std::cout << "  • Process files in fixed-size chunks to limit memory usage" << std::endl;
        std::cout << "  • Implement progress tracking for user feedback" << std::endl;
        std::cout << "  • Support resumable operations for reliability" << std::endl;
        std::cout << "  • Clear chunks from memory immediately after processing" << std::endl;

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

