/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * Metadata operations example for FastDFS C++ client
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

        // Upload a file with metadata
        std::cout << "Uploading file with metadata..." << std::endl;
        std::vector<uint8_t> data = {'T', 'e', 's', 't', ' ', 'd', 'a', 't', 'a'};
        
        fastdfs::Metadata metadata;
        metadata["author"] = "John Doe";
        metadata["date"] = "2025-01-01";
        metadata["description"] = "Test file with metadata";
        
        std::string file_id = client.upload_buffer(data, "txt", &metadata);
        std::cout << "File uploaded. File ID: " << file_id << std::endl;

        // Get metadata
        std::cout << "\nRetrieving metadata..." << std::endl;
        fastdfs::Metadata retrieved_metadata = client.get_metadata(file_id);
        
        std::cout << "Metadata:" << std::endl;
        for (const auto& pair : retrieved_metadata) {
            std::cout << "  " << pair.first << " = " << pair.second << std::endl;
        }

        // Update metadata (merge)
        std::cout << "\nUpdating metadata (merge)..." << std::endl;
        fastdfs::Metadata new_metadata;
        new_metadata["version"] = "1.0";
        new_metadata["author"] = "Jane Smith"; // This will update existing key
        
        client.set_metadata(file_id, new_metadata, fastdfs::MetadataFlag::MERGE);
        
        retrieved_metadata = client.get_metadata(file_id);
        std::cout << "Updated metadata:" << std::endl;
        for (const auto& pair : retrieved_metadata) {
            std::cout << "  " << pair.first << " = " << pair.second << std::endl;
        }

        // Overwrite metadata
        std::cout << "\nOverwriting metadata..." << std::endl;
        fastdfs::Metadata overwrite_metadata;
        overwrite_metadata["new_key"] = "new_value";
        
        client.set_metadata(file_id, overwrite_metadata, fastdfs::MetadataFlag::OVERWRITE);
        
        retrieved_metadata = client.get_metadata(file_id);
        std::cout << "Overwritten metadata:" << std::endl;
        for (const auto& pair : retrieved_metadata) {
            std::cout << "  " << pair.first << " = " << pair.second << std::endl;
        }

        // Cleanup
        client.delete_file(file_id);
        client.close();

        std::cout << "\nMetadata example completed successfully!" << std::endl;
    } catch (const fastdfs::FastDFSException& e) {
        std::cerr << "FastDFS error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

