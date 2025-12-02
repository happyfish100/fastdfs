/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * Appender file operations example for FastDFS C++ client
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>

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

        // Initialize client
        fastdfs::Client client(config);

        // Example 1: Upload appender file
        std::cout << "Example 1: Upload appender file" << std::endl;
        std::vector<uint8_t> initial_data = {'I', 'n', 'i', 't', 'i', 'a', 'l', ' '};
        std::string appender_file_id = client.upload_appender_buffer(initial_data, "txt", nullptr);
        std::cout << "Appender file uploaded. File ID: " << appender_file_id << std::endl;

        // Example 2: Append data
        std::cout << "\nExample 2: Append data" << std::endl;
        std::vector<uint8_t> append_data1 = {'d', 'a', 't', 'a', '1', '\n'};
        client.append_file(appender_file_id, append_data1);
        std::cout << "Data appended" << std::endl;

        std::vector<uint8_t> append_data2 = {'d', 'a', 't', 'a', '2', '\n'};
        client.append_file(appender_file_id, append_data2);
        std::cout << "More data appended" << std::endl;

        // Download and show content
        std::vector<uint8_t> content = client.download_file(appender_file_id);
        std::cout << "Current content (" << content.size() << " bytes): ";
        for (uint8_t byte : content) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << std::endl;

        // Example 3: Modify file at offset
        std::cout << "\nExample 3: Modify file at offset" << std::endl;
        std::vector<uint8_t> modify_data = {'M', 'O', 'D', 'I', 'F', 'I', 'E', 'D'};
        client.modify_file(appender_file_id, 0, modify_data);
        std::cout << "File modified at offset 0" << std::endl;

        content = client.download_file(appender_file_id);
        std::cout << "Modified content (" << content.size() << " bytes): ";
        for (uint8_t byte : content) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << std::endl;

        // Example 4: Truncate file
        std::cout << "\nExample 4: Truncate file" << std::endl;
        client.truncate_file(appender_file_id, 10);
        std::cout << "File truncated to 10 bytes" << std::endl;

        content = client.download_file(appender_file_id);
        std::cout << "Truncated content (" << content.size() << " bytes): ";
        for (uint8_t byte : content) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << std::endl;

        // Example 5: Upload slave file
        std::cout << "\nExample 5: Upload slave file" << std::endl;
        std::vector<uint8_t> slave_data = {'S', 'l', 'a', 'v', 'e', ' ', 'f', 'i', 'l', 'e'};
        std::string slave_file_id = client.upload_slave_file(
            appender_file_id, "thumb", "txt", slave_data, nullptr);
        std::cout << "Slave file uploaded. File ID: " << slave_file_id << std::endl;

        // Cleanup
        client.delete_file(slave_file_id);
        client.delete_file(appender_file_id);
        client.close();

        std::cout << "\nAppender example completed successfully!" << std::endl;
    } catch (const fastdfs::FastDFSException& e) {
        std::cerr << "FastDFS error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

