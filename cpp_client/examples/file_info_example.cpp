/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS File Information Retrieval Example
 *
 * This comprehensive example demonstrates how to retrieve and work with
 * detailed file information from FastDFS storage servers. File information
 * is essential for validation, monitoring, auditing, and understanding
 * the state of files in your distributed storage system.
 *
 * The FileInfo struct provides critical metadata about files including:
 * - File size in bytes (useful for capacity planning and validation)
 * - Creation timestamp (for auditing and lifecycle management)
 * - CRC32 checksum (for data integrity verification)
 * - Source server IP address (for tracking and troubleshooting)
 *
 * Use cases for file information retrieval:
 * - Validation: Verify file size matches expected values
 * - Monitoring: Track file creation times and storage usage
 * - Auditing: Maintain records of when files were created and where
 * - Integrity checking: Use CRC32 to verify file hasn't been corrupted
 * - Troubleshooting: Identify which storage server holds a file
 *
 * Run this example with:
 *   ./file_info_example <tracker_address>
 *   Example: ./file_info_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <chrono>

// Helper function to format timestamp
std::string format_timestamp(int64_t timestamp) {
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::gmtime(&time);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S UTC");
    return oss.str();
}

// Helper function to calculate file age
std::string calculate_file_age(int64_t create_time) {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    int64_t age_seconds = now_time - create_time;
    
    if (age_seconds < 60) {
        return std::to_string(age_seconds) + " seconds";
    } else if (age_seconds < 3600) {
        return std::to_string(age_seconds / 60) + " minutes";
    } else if (age_seconds < 86400) {
        return std::to_string(age_seconds / 3600) + " hours";
    } else {
        return std::to_string(age_seconds / 86400) + " days";
    }
}

// Helper function to format file size
std::string format_file_size(int64_t size) {
    if (size < 1024) {
        return std::to_string(size) + " bytes";
    } else if (size < 1024 * 1024) {
        double kb = static_cast<double>(size) / 1024.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << kb << " KB";
        return oss.str();
    } else {
        double mb = static_cast<double>(size) / (1024.0 * 1024.0);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << mb << " MB";
        return oss.str();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - File Information Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // STEP 1: Configure the FastDFS Client
        // ====================================================================
        // Before we can retrieve file information, we need to set up a client
        // connection to the FastDFS tracker server. The tracker server acts
        // as a coordinator that knows where files are stored in the cluster.

        std::cout << "1. Configuring FastDFS Client..." << std::endl;
        fastdfs::ClientConfig config;
        config.tracker_addrs = {argv[1]};
        config.max_conns = 10;
        config.connect_timeout = std::chrono::milliseconds(5000);
        config.network_timeout = std::chrono::milliseconds(30000);

        // ====================================================================
        // STEP 2: Create the Client Instance
        // ====================================================================
        // The client manages connection pools and handles automatic retries.
        // It's thread-safe and can be used from multiple threads.

        fastdfs::Client client(config);
        std::cout << "   ✓ Client initialized successfully" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Upload a File and Get Its Information
        // ====================================================================
        // First, we'll upload a test file so we have something to inspect.
        // Then we'll retrieve detailed information about that file.

        std::cout << "2. Uploading a test file..." << std::endl;
        std::string test_data = "This is a test file for demonstrating file information retrieval. "
                               "It contains sample content that we can use to verify the file info "
                               "operations work correctly.";
        
        std::vector<uint8_t> data(test_data.begin(), test_data.end());
        std::string file_id = client.upload_buffer(data, "txt", nullptr);
        std::cout << "   ✓ File uploaded successfully!" << std::endl;
        std::cout << "   File ID: " << file_id << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Retrieve Basic File Information
        // ====================================================================
        // The get_file_info method retrieves comprehensive information about
        // a file without downloading the actual file content. This is efficient
        // for validation and monitoring purposes.

        std::cout << "3. Retrieving file information..." << std::endl;
        fastdfs::FileInfo file_info = client.get_file_info(file_id);
        
        std::cout << "   File Information Details:" << std::endl;
        std::cout << "   " << std::string(50, '-') << std::endl;

        // ====================================================================
        // EXAMPLE 3: Display File Size Information
        // ====================================================================
        // File size is crucial for:
        // - Validating uploads completed successfully
        // - Capacity planning and quota management
        // - Detecting truncated or corrupted uploads

        std::cout << std::endl << "   File Size Information:" << std::endl;
        std::cout << "     File Size: " << file_info.file_size << " bytes" << std::endl;
        std::cout << "     File Size: " << format_file_size(file_info.file_size) << std::endl;
        
        // Validate that the file size matches our uploaded data
        int64_t expected_size = static_cast<int64_t>(data.size());
        if (file_info.file_size == expected_size) {
            std::cout << "     ✓ File size validation passed (matches uploaded data)" << std::endl;
        } else {
            std::cout << "     ⚠ Warning: File size mismatch!" << std::endl;
            std::cout << "       Expected: " << expected_size << " bytes" << std::endl;
            std::cout << "       Actual: " << file_info.file_size << " bytes" << std::endl;
        }

        // ====================================================================
        // EXAMPLE 4: Display Creation Time Information
        // ====================================================================
        // Creation time is important for:
        // - Auditing: Knowing when files were created
        // - Lifecycle management: Identifying old files for archival
        // - Debugging: Understanding the timeline of file operations

        std::cout << std::endl << "   Creation Time Information:" << std::endl;
        std::cout << "     Create Time (timestamp): " << file_info.create_time << std::endl;
        std::cout << "     Create Time (formatted): " << format_timestamp(file_info.create_time) << std::endl;
        std::cout << "     File Age: " << calculate_file_age(file_info.create_time) << std::endl;

        // ====================================================================
        // EXAMPLE 5: Display CRC32 Checksum Information
        // ====================================================================
        // CRC32 is a checksum used for:
        // - Data integrity verification
        // - Detecting corruption or transmission errors
        // - Validating that files haven't been modified

        std::cout << std::endl << "   CRC32 Checksum Information:" << std::endl;
        std::cout << "     CRC32: 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(8) << file_info.crc32 << std::dec << std::endl;
        std::cout << "     CRC32: " << file_info.crc32 << " (decimal)" << std::endl;
        std::cout << "     Note: CRC32 can be used to verify file integrity" << std::endl;
        std::cout << "           Compare this value before and after operations" << std::endl;

        // ====================================================================
        // EXAMPLE 6: Display Source Server Information
        // ====================================================================
        // Source server information is valuable for:
        // - Troubleshooting: Knowing which server stores the file
        // - Load balancing: Understanding file distribution
        // - Monitoring: Tracking server-specific issues

        std::cout << std::endl << "   Source Server Information:" << std::endl;
        std::cout << "     Group Name: " << file_info.group_name << std::endl;
        std::cout << "     Remote Filename: " << file_info.remote_filename << std::endl;
        std::cout << "     Source IP Address: " << file_info.source_ip_addr << std::endl;
        if (!file_info.storage_id.empty()) {
            std::cout << "     Storage ID: " << file_info.storage_id << std::endl;
        }
        std::cout << "     Note: This is the storage server that holds the file" << std::endl;
        std::cout << "           Useful for troubleshooting and monitoring" << std::endl;

        // ====================================================================
        // EXAMPLE 7: Complete FileInfo Struct Display
        // ====================================================================
        // Display the entire FileInfo struct for comprehensive inspection.

        std::cout << std::endl << "4. Complete FileInfo struct:" << std::endl;
        std::cout << "   Group Name:        " << file_info.group_name << std::endl;
        std::cout << "   Remote Filename:    " << file_info.remote_filename << std::endl;
        std::cout << "   File Size:          " << file_info.file_size << " bytes" << std::endl;
        std::cout << "   Create Time:        " << file_info.create_time << " (" 
                  << format_timestamp(file_info.create_time) << ")" << std::endl;
        std::cout << "   CRC32:              0x" << std::hex << std::uppercase 
                  << std::setfill('0') << std::setw(8) << file_info.crc32 << std::dec << std::endl;
        std::cout << "   Source IP Address:  " << file_info.source_ip_addr << std::endl;
        if (!file_info.storage_id.empty()) {
            std::cout << "   Storage ID:         " << file_info.storage_id << std::endl;
        }

        // ====================================================================
        // EXAMPLE 8: File Information for Validation Use Case
        // ====================================================================
        // Demonstrate how file information can be used for validation.
        // This is a common pattern in production applications.

        std::cout << std::endl << "5. Validation Use Case:" << std::endl;
        
        // Check 1: Verify file size is within acceptable range
        int64_t min_size = 1;
        int64_t max_size = 100 * 1024 * 1024; // 100 MB
        if (file_info.file_size >= min_size && file_info.file_size <= max_size) {
            std::cout << "   ✓ File size validation: PASSED (within acceptable range)" << std::endl;
        } else {
            std::cout << "   ✗ File size validation: FAILED" << std::endl;
            std::cout << "     Size: " << file_info.file_size 
                      << " bytes (acceptable range: " << min_size << " - " << max_size << " bytes)" << std::endl;
        }
        
        // Check 2: Verify file was created recently (for new uploads)
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        int64_t age_seconds = now_time - file_info.create_time;
        int64_t max_age_seconds = 3600; // 1 hour
        
        if (age_seconds < max_age_seconds) {
            std::cout << "   ✓ File age validation: PASSED (file is recent)" << std::endl;
        } else {
            std::cout << "   ⚠ File age validation: WARNING (file is older than 1 hour)" << std::endl;
        }
        
        // Check 3: Verify source server is accessible
        if (!file_info.source_ip_addr.empty()) {
            std::cout << "   ✓ Source server validation: PASSED (server IP available)" << std::endl;
        } else {
            std::cout << "   ✗ Source server validation: FAILED (no server IP)" << std::endl;
        }

        // ====================================================================
        // EXAMPLE 9: File Information for Monitoring Use Case
        // ====================================================================
        // Demonstrate how file information can be used for monitoring.
        // This helps track storage usage and file distribution.

        std::cout << std::endl << "6. Monitoring Use Case:" << std::endl;
        std::cout << "   Storage Metrics:" << std::endl;
        std::cout << "     - File size: " << file_info.file_size << " bytes" << std::endl;
        double efficiency = (static_cast<double>(file_info.file_size) / 1024.0) * 100.0;
        std::cout << "     - Storage efficiency: " << std::fixed << std::setprecision(2) 
                  << efficiency << "% of 1KB block" << std::endl;
        std::cout << "   Creation Pattern:" << std::endl;
        std::cout << "     - File created at: " << format_timestamp(file_info.create_time) << std::endl;
        std::cout << "     - Source server: " << file_info.source_ip_addr << std::endl;

        // ====================================================================
        // EXAMPLE 10: File Information for Auditing Use Case
        // ====================================================================
        // Demonstrate how file information supports auditing requirements.
        // Auditing is important for compliance and security.

        std::cout << std::endl << "7. Auditing Use Case:" << std::endl;
        std::cout << "   Audit Log Entry:" << std::endl;
        auto audit_time = std::chrono::system_clock::now();
        auto audit_time_t = std::chrono::system_clock::to_time_t(audit_time);
        std::cout << "     Timestamp: " << format_timestamp(audit_time_t) << std::endl;
        std::cout << "     Operation: File Information Retrieval" << std::endl;
        std::cout << "     File ID: " << file_id << std::endl;
        std::cout << "     File Size: " << file_info.file_size << " bytes" << std::endl;
        std::cout << "     Created: " << format_timestamp(file_info.create_time) << std::endl;
        std::cout << "     CRC32: 0x" << std::hex << std::uppercase << std::setfill('0') 
                  << std::setw(8) << file_info.crc32 << std::dec << std::endl;
        std::cout << "     Source Server: " << file_info.source_ip_addr << std::endl;
        std::cout << "     Status: Retrieved successfully" << std::endl;

        // ====================================================================
        // EXAMPLE 11: Working with Multiple Files
        // ====================================================================
        // Demonstrate retrieving information for multiple files.
        // This is common in batch processing scenarios.

        std::cout << std::endl << "8. Batch File Information Retrieval:" << std::endl;
        std::vector<std::string> file_ids;
        
        // Upload a few more files
        for (int i = 0; i < 3; ++i) {
            std::string batch_data = "Batch file " + std::to_string(i + 1);
            std::vector<uint8_t> batch_bytes(batch_data.begin(), batch_data.end());
            std::string batch_file_id = client.upload_buffer(batch_bytes, "txt", nullptr);
            file_ids.push_back(batch_file_id);
        }
        
        std::cout << "   Retrieved information for " << file_ids.size() << " files:" << std::endl;
        for (size_t i = 0; i < file_ids.size(); ++i) {
            try {
                fastdfs::FileInfo info = client.get_file_info(file_ids[i]);
                std::cout << "   File " << (i + 1) << ": " << info.file_size 
                          << " bytes, CRC32: 0x" << std::hex << std::uppercase 
                          << std::setfill('0') << std::setw(8) << info.crc32 << std::dec << std::endl;
            } catch (const fastdfs::FastDFSException& e) {
                std::cout << "   File " << (i + 1) << ": Error retrieving info - " << e.what() << std::endl;
            }
        }
        
        // Clean up batch files
        for (const auto& id : file_ids) {
            try {
                client.delete_file(id);
            } catch (...) {
                // Ignore cleanup errors
            }
        }
        std::cout << "   ✓ Batch files cleaned up" << std::endl;

        // ====================================================================
        // EXAMPLE 12: Error Handling for File Information
        // ====================================================================
        // Demonstrate proper error handling when retrieving file information.
        // This is important for robust applications.

        std::cout << std::endl << "9. Error Handling Example:" << std::endl;
        std::string non_existent_file = "group1/nonexistent_file.txt";
        try {
            fastdfs::FileInfo info = client.get_file_info(non_existent_file);
            std::cout << "   ⚠ Unexpected: Retrieved info for non-existent file" << std::endl;
        } catch (const fastdfs::FileNotFoundException& e) {
            std::cout << "   ✓ Correctly handled error for non-existent file" << std::endl;
            std::cout << "     Error: " << e.what() << std::endl;
        } catch (const fastdfs::FastDFSException& e) {
            std::cout << "   ✓ Handled FastDFS error: " << e.what() << std::endl;
        }

        // ====================================================================
        // CLEANUP: Delete Test File
        // ====================================================================
        // Always clean up test files to avoid cluttering the storage system.

        std::cout << std::endl << "10. Cleaning up test file..." << std::endl;
        client.delete_file(file_id);
        std::cout << "   ✓ Test file deleted successfully" << std::endl;
        
        // Verify the file is gone
        try {
            client.get_file_info(file_id);
            std::cout << "   ⚠ Warning: File still exists after deletion" << std::endl;
        } catch (const fastdfs::FileNotFoundException&) {
            std::cout << "   ✓ Confirmed: File no longer exists" << std::endl;
        }

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::endl << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ File information retrieval" << std::endl;
        std::cout << "  ✓ File size inspection and validation" << std::endl;
        std::cout << "  ✓ Creation time analysis" << std::endl;
        std::cout << "  ✓ CRC32 checksum usage" << std::endl;
        std::cout << "  ✓ Source server information" << std::endl;
        std::cout << "  ✓ Validation use cases" << std::endl;
        std::cout << "  ✓ Monitoring use cases" << std::endl;
        std::cout << "  ✓ Auditing use cases" << std::endl;
        std::cout << "  ✓ Batch file processing" << std::endl;
        std::cout << "  ✓ Error handling" << std::endl;

        // ====================================================================
        // CLOSE CLIENT
        // ====================================================================
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

