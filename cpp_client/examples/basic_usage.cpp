/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * Basic usage example for FastDFS C++ client
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        // Create client configuration
        fastdfs::ClientConfig config;
        config.tracker_addrs = {argv[1]};
        config.max_conns = 10;
        config.connect_timeout = std::chrono::milliseconds(5000);
        config.network_timeout = std::chrono::milliseconds(30000);

        // Initialize client
        fastdfs::Client client(config);

        // Example 1: Upload a file
        std::cout << "Example 1: Upload a file" << std::endl;
        std::string test_file = "test.txt";
        
        // Create a test file
        {
            std::ofstream file(test_file);
            file << "Hello, FastDFS! This is a test file." << std::endl;
        }

        std::string file_id = client.upload_file(test_file, nullptr);
        std::cout << "File uploaded successfully. File ID: " << file_id << std::endl;

        // Example 2: Upload from buffer
        std::cout << "\nExample 2: Upload from buffer" << std::endl;
        std::vector<uint8_t> buffer = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'F', 'a', 's', 't', 'D', 'F', 'S', '!'};
        std::string buffer_file_id = client.upload_buffer(buffer, "txt", nullptr);
        std::cout << "Buffer uploaded successfully. File ID: " << buffer_file_id << std::endl;

        // Example 3: Download a file
        std::cout << "\nExample 3: Download a file" << std::endl;
        std::vector<uint8_t> downloaded_data = client.download_file(file_id);
        std::cout << "Downloaded " << downloaded_data.size() << " bytes" << std::endl;
        std::cout << "Content: ";
        for (uint8_t byte : downloaded_data) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << std::endl;

        // Example 4: Download to file
        std::cout << "\nExample 4: Download to file" << std::endl;
        std::string downloaded_file = "downloaded.txt";
        client.download_to_file(file_id, downloaded_file);
        std::cout << "File downloaded to: " << downloaded_file << std::endl;

        // Example 5: Get file info
        std::cout << "\nExample 5: Get file info" << std::endl;
        fastdfs::FileInfo info = client.get_file_info(file_id);
        std::cout << "File size: " << info.file_size << " bytes" << std::endl;
        std::cout << "Group name: " << info.group_name << std::endl;
        std::cout << "Remote filename: " << info.remote_filename << std::endl;

        // Example 6: Check if file exists
        std::cout << "\nExample 6: Check if file exists" << std::endl;
        bool exists = client.file_exists(file_id);
        std::cout << "File exists: " << (exists ? "Yes" : "No") << std::endl;

        // Example 7: Delete file
        std::cout << "\nExample 7: Delete file" << std::endl;
        client.delete_file(file_id);
        std::cout << "File deleted successfully" << std::endl;

        // Cleanup
        client.close();
        std::remove(test_file.c_str());
        std::remove(downloaded_file.c_str());

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const fastdfs::FastDFSException& e) {
        std::cerr << "FastDFS error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

