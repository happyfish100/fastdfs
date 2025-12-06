/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Upload from Memory Buffer Example
 *
 * This comprehensive example demonstrates uploading data from memory buffers
 * to FastDFS. It covers various data types, use cases, and patterns for
 * uploading in-memory data efficiently.
 *
 * Key Topics Covered:
 * - Demonstrates uploading files from memory buffers
 * - Shows how to upload data from std::vector<uint8_t>, arrays, and string buffers
 * - Includes examples for different data sources (network streams, generated data)
 * - Demonstrates memory-efficient upload patterns
 * - Useful for in-memory file processing and API integrations
 * - Shows how to handle large buffers efficiently
 *
 * Run this example with:
 *   ./upload_buffer_example <tracker_address>
 *   Example: ./upload_buffer_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>

// Helper function to generate JSON content
std::string generate_json_content() {
    std::ostringstream json;
    json << "{\n"
         << "  \"id\": 12345,\n"
         << "  \"name\": \"Example Document\",\n"
         << "  \"type\": \"json\",\n"
         << "  \"timestamp\": \"2025-01-15T10:30:00Z\",\n"
         << "  \"data\": {\n"
         << "    \"field1\": \"value1\",\n"
         << "    \"field2\": 42,\n"
         << "    \"field3\": true\n"
         << "  },\n"
         << "  \"tags\": [\"example\", \"json\", \"test\"]\n"
         << "}";
    return json.str();
}

// Helper function to generate XML content
std::string generate_xml_content() {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<document>\n"
        << "  <id>12345</id>\n"
        << "  <name>Example Document</name>\n"
        << "  <type>xml</type>\n"
        << "  <timestamp>2025-01-15T10:30:00Z</timestamp>\n"
        << "  <data>\n"
        << "    <field1>value1</field1>\n"
        << "    <field2>42</field2>\n"
        << "    <field3>true</field3>\n"
        << "  </data>\n"
        << "  <tags>\n"
        << "    <tag>example</tag>\n"
        << "    <tag>xml</tag>\n"
        << "    <tag>test</tag>\n"
        << "  </tags>\n"
        << "</document>";
    return xml.str();
}

// Helper function to generate CSV content
std::string generate_csv_content() {
    std::ostringstream csv;
    csv << "id,name,type,value,active\n";
    for (int i = 1; i <= 10; ++i) {
        csv << i << ",Item" << i << ",type" << (i % 3) 
            << "," << (i * 10) << "," << (i % 2 == 0 ? "true" : "false") << "\n";
    }
    return csv.str();
}

// Helper function to generate binary content
std::vector<uint8_t> generate_binary_content(size_t size) {
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
        std::cout << "FastDFS C++ Client - Upload from Memory Buffer Example" << std::endl;
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
        // EXAMPLE 1: Basic Buffer Upload
        // ====================================================================
        std::cout << "2. Basic Buffer Upload" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates uploading files from memory buffers." << std::endl;
        std::cout << std::endl;

        // Example 1.1: Upload from byte array (C-style)
        std::cout << "   Example 1.1: Upload from C-style array" << std::endl;
        uint8_t array_data[] = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'F', 'a', 's', 't', 'D', 'F', 'S', '!'};
        std::vector<uint8_t> array_vec(array_data, array_data + sizeof(array_data));
        std::string file_id1 = client.upload_buffer(array_vec, "txt", nullptr);
        std::cout << "     ✓ Uploaded " << sizeof(array_data) << " bytes from array" << std::endl;
        std::cout << "     File ID: " << file_id1 << std::endl;
        std::cout << std::endl;

        // Example 1.2: Upload from std::vector<uint8_t>
        std::cout << "   Example 1.2: Upload from std::vector<uint8_t>" << std::endl;
        std::vector<uint8_t> vec_data(1000);
        for (size_t i = 0; i < vec_data.size(); ++i) {
            vec_data[i] = static_cast<uint8_t>(i % 256);
        }
        std::string file_id2 = client.upload_buffer(vec_data, "bin", nullptr);
        std::cout << "     ✓ Uploaded " << vec_data.size() << " bytes from std::vector<uint8_t>" << std::endl;
        std::cout << "     File ID: " << file_id2 << std::endl;
        std::cout << std::endl;

        // Example 1.3: Upload from string buffer
        std::cout << "   Example 1.3: Upload from string buffer" << std::endl;
        std::string text_content = "This is text content uploaded from a string buffer.";
        std::vector<uint8_t> string_data(text_content.begin(), text_content.end());
        std::string file_id3 = client.upload_buffer(string_data, "txt", nullptr);
        std::cout << "     ✓ Uploaded " << string_data.size() << " bytes from string buffer" << std::endl;
        std::cout << "     File ID: " << file_id3 << std::endl;
        std::cout << std::endl;

        // Clean up
        client.delete_file(file_id1);
        client.delete_file(file_id2);
        client.delete_file(file_id3);

        // ====================================================================
        // EXAMPLE 2: Upload Generated Content
        // ====================================================================
        std::cout << "3. Upload Generated Content" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes examples for different data sources (network streams, generated data)." << std::endl;
        std::cout << std::endl;

        // Example 2.1: Upload JSON content
        std::cout << "   Example 2.1: Upload JSON content" << std::endl;
        std::string json_content = generate_json_content();
        std::vector<uint8_t> json_data(json_content.begin(), json_content.end());
        std::string json_file_id = client.upload_buffer(json_data, "json", nullptr);
        std::cout << "     ✓ Uploaded " << json_data.size() << " bytes of JSON" << std::endl;
        std::cout << "     File ID: " << json_file_id << std::endl;
        std::cout << std::endl;

        // Example 2.2: Upload XML content
        std::cout << "   Example 2.2: Upload XML content" << std::endl;
        std::string xml_content = generate_xml_content();
        std::vector<uint8_t> xml_data(xml_content.begin(), xml_content.end());
        std::string xml_file_id = client.upload_buffer(xml_data, "xml", nullptr);
        std::cout << "     ✓ Uploaded " << xml_data.size() << " bytes of XML" << std::endl;
        std::cout << "     File ID: " << xml_file_id << std::endl;
        std::cout << std::endl;

        // Example 2.3: Upload CSV content
        std::cout << "   Example 2.3: Upload CSV content" << std::endl;
        std::string csv_content = generate_csv_content();
        std::vector<uint8_t> csv_data(csv_content.begin(), csv_content.end());
        std::string csv_file_id = client.upload_buffer(csv_data, "csv", nullptr);
        std::cout << "     ✓ Uploaded " << csv_data.size() << " bytes of CSV" << std::endl;
        std::cout << "     File ID: " << csv_file_id << std::endl;
        std::cout << std::endl;

        // Clean up
        client.delete_file(json_file_id);
        client.delete_file(xml_file_id);
        client.delete_file(csv_file_id);

        // ====================================================================
        // EXAMPLE 3: Upload Binary Data
        // ====================================================================
        std::cout << "4. Upload Binary Data" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates uploading binary data from memory." << std::endl;
        std::cout << std::endl;

        // Example 3.1: Small binary data
        std::cout << "   Example 3.1: Small binary data (1KB)" << std::endl;
        std::vector<uint8_t> small_binary = generate_binary_content(1024);
        std::string small_binary_id = client.upload_buffer(small_binary, "bin", nullptr);
        std::cout << "     ✓ Uploaded " << small_binary.size() << " bytes" << std::endl;
        std::cout << "     File ID: " << small_binary_id << std::endl;
        std::cout << std::endl;

        // Example 3.2: Medium binary data
        std::cout << "   Example 3.2: Medium binary data (10KB)" << std::endl;
        std::vector<uint8_t> medium_binary = generate_binary_content(10 * 1024);
        std::string medium_binary_id = client.upload_buffer(medium_binary, "bin", nullptr);
        std::cout << "     ✓ Uploaded " << medium_binary.size() << " bytes" << std::endl;
        std::cout << "     File ID: " << medium_binary_id << std::endl;
        std::cout << std::endl;

        // Clean up
        client.delete_file(small_binary_id);
        client.delete_file(medium_binary_id);

        // ====================================================================
        // EXAMPLE 4: Memory-Efficient Upload Patterns
        // ====================================================================
        std::cout << "5. Memory-Efficient Upload Patterns" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates memory-efficient upload patterns." << std::endl;
        std::cout << std::endl;

        // Example 4.1: Reuse buffer
        std::cout << "   Example 4.1: Reusing buffer for multiple uploads" << std::endl;
        std::vector<uint8_t> reusable_buffer(512);
        std::vector<std::string> uploaded_ids;

        for (int i = 0; i < 5; ++i) {
            // Fill buffer with different content
            std::string content = "Reusable buffer upload " + std::to_string(i);
            std::copy(content.begin(), content.end(), reusable_buffer.begin());
            reusable_buffer[content.size()] = '\0';

            std::string id = client.upload_buffer(
                std::vector<uint8_t>(reusable_buffer.begin(), reusable_buffer.begin() + content.size()),
                "txt", nullptr);
            uploaded_ids.push_back(id);
        }

        std::cout << "     ✓ Uploaded " << uploaded_ids.size() << " files using reusable buffer" << std::endl;
        std::cout << std::endl;

        // Clean up
        for (const auto& id : uploaded_ids) {
            client.delete_file(id);
        }

        // ====================================================================
        // EXAMPLE 5: Handling Large Buffers Efficiently
        // ====================================================================
        std::cout << "6. Handling Large Buffers Efficiently" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to handle large buffers efficiently." << std::endl;
        std::cout << std::endl;

        // Example 5.1: Large buffer upload
        std::cout << "   Example 5.1: Large buffer upload (100KB)" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<uint8_t> large_buffer = generate_binary_content(100 * 1024);
        std::string large_file_id = client.upload_buffer(large_buffer, "bin", nullptr);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "     ✓ Uploaded " << large_buffer.size() << " bytes in " 
                  << duration.count() << " ms" << std::endl;
        std::cout << "     File ID: " << large_file_id << std::endl;
        std::cout << "     Throughput: " << std::fixed << std::setprecision(2)
                  << (large_buffer.size() / 1024.0 / 1024.0) / (duration.count() / 1000.0)
                  << " MB/s" << std::endl;
        std::cout << std::endl;

        // Clean up
        client.delete_file(large_file_id);

        // ====================================================================
        // EXAMPLE 6: Upload with Metadata
        // ====================================================================
        std::cout << "7. Upload Buffer with Metadata" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Uploading buffers with metadata for better organization." << std::endl;
        std::cout << std::endl;

        std::string metadata_content = "Content with metadata";
        std::vector<uint8_t> metadata_data(metadata_content.begin(), metadata_content.end());

        fastdfs::Metadata metadata;
        metadata["source"] = "buffer_upload";
        metadata["type"] = "text";
        metadata["generated_at"] = "2025-01-15";
        metadata["size"] = std::to_string(metadata_data.size());

        std::string metadata_file_id = client.upload_buffer(metadata_data, "txt", &metadata);
        std::cout << "   ✓ Uploaded buffer with metadata" << std::endl;
        std::cout << "   File ID: " << metadata_file_id << std::endl;

        // Retrieve and display metadata
        fastdfs::Metadata retrieved = client.get_metadata(metadata_file_id);
        std::cout << "   Retrieved metadata:" << std::endl;
        for (const auto& pair : retrieved) {
            std::cout << "     " << pair.first << " = " << pair.second << std::endl;
        }
        std::cout << std::endl;

        // Clean up
        client.delete_file(metadata_file_id);

        // ====================================================================
        // EXAMPLE 7: Simulated Network Stream Upload
        // ====================================================================
        std::cout << "8. Simulated Network Stream Upload" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Simulating upload from network stream data." << std::endl;
        std::cout << std::endl;

        // Simulate receiving data in chunks (as from network)
        std::cout << "   Simulating receiving data in chunks..." << std::endl;
        std::vector<uint8_t> stream_data;
        
        // Simulate 5 chunks of data
        for (int i = 0; i < 5; ++i) {
            std::string chunk = "Chunk " + std::to_string(i + 1) + " of network stream data\n";
            std::vector<uint8_t> chunk_data(chunk.begin(), chunk.end());
            stream_data.insert(stream_data.end(), chunk_data.begin(), chunk_data.end());
            std::cout << "     → Received chunk " << (i + 1) << " (" << chunk_data.size() << " bytes)" << std::endl;
        }

        std::string stream_file_id = client.upload_buffer(stream_data, "txt", nullptr);
        std::cout << "   ✓ Uploaded " << stream_data.size() << " bytes from simulated network stream" << std::endl;
        std::cout << "   File ID: " << stream_file_id << std::endl;
        std::cout << std::endl;

        // Clean up
        client.delete_file(stream_file_id);

        // ====================================================================
        // EXAMPLE 8: API Integration Pattern
        // ====================================================================
        std::cout << "9. API Integration Pattern" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Useful for in-memory file processing and API integrations." << std::endl;
        std::cout << std::endl;

        // Simulate API response data
        std::cout << "   Simulating API response upload..." << std::endl;
        std::ostringstream api_response;
        api_response << "HTTP/1.1 200 OK\n"
                     << "Content-Type: application/json\n"
                     << "Content-Length: 150\n\n"
                     << generate_json_content();

        std::string api_data_str = api_response.str();
        std::vector<uint8_t> api_data(api_data_str.begin(), api_data_str.end());

        fastdfs::Metadata api_metadata;
        api_metadata["source"] = "api_response";
        api_metadata["content_type"] = "application/json";
        api_metadata["status"] = "200";

        std::string api_file_id = client.upload_buffer(api_data, "txt", &api_metadata);
        std::cout << "   ✓ Uploaded API response data (" << api_data.size() << " bytes)" << std::endl;
        std::cout << "   File ID: " << api_file_id << std::endl;
        std::cout << std::endl;

        // Clean up
        client.delete_file(api_file_id);

        // ====================================================================
        // EXAMPLE 9: Comparison: Buffer vs File Upload
        // ====================================================================
        std::cout << "10. Comparison: Buffer vs File Upload" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates when to use buffer upload vs file upload." << std::endl;
        std::cout << std::endl;

        std::string comparison_content = "Comparison test content";
        std::vector<uint8_t> comparison_data(comparison_content.begin(), comparison_content.end());

        std::cout << "   Buffer Upload Advantages:" << std::endl;
        std::cout << "   - No temporary files needed" << std::endl;
        std::cout << "   - Direct upload from memory" << std::endl;
        std::cout << "   - Efficient for generated content" << std::endl;
        std::cout << "   - Supports all data types" << std::endl;
        std::cout << "   - Useful for API integrations" << std::endl;
        std::cout << std::endl;

        std::cout << "   Use Buffer Upload When:" << std::endl;
        std::cout << "   - Data is generated in memory" << std::endl;
        std::cout << "   - Data comes from network streams" << std::endl;
        std::cout << "   - You want to avoid temporary files" << std::endl;
        std::cout << "   - Working with API responses" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Uploading files from memory buffers" << std::endl;
        std::cout << "  ✓ Upload data from std::vector<uint8_t>, arrays, and string buffers" << std::endl;
        std::cout << "  ✓ Examples for different data sources (network streams, generated data)" << std::endl;
        std::cout << "  ✓ Memory-efficient upload patterns" << std::endl;
        std::cout << "  ✓ Useful for in-memory file processing and API integrations" << std::endl;
        std::cout << "  ✓ Handling large buffers efficiently" << std::endl;

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

