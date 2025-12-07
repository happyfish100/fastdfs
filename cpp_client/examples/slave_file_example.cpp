/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Slave File Example
 *
 * This example demonstrates slave file operations with the FastDFS client.
 * Slave files are associated with master files and are commonly used for
 * thumbnails, previews, transcoded versions, and other derived content.
 *
 * Key Topics Covered:
 * - Upload master files
 * - Upload slave files (thumbnails, previews, variants)
 * - Linking slave files to master files
 * - Metadata management for slave files
 * - Use cases: image processing, video transcoding, file transformation workflows
 *
 * Run this example with:
 *   ./slave_file_example <tracker_address>
 *   Example: ./slave_file_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <map>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Slave File Example" << std::endl;
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
        // EXAMPLE 1: Upload Master File
        // ====================================================================
        std::cout << "2. Upload Master File" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Master files are the original files that slave files reference." << std::endl;
        std::cout << "   They serve as the source for generating thumbnails, previews, etc." << std::endl;
        std::cout << std::endl;

        // Simulate master file data (in real scenario, this would be actual image/video)
        std::string master_content = "This is a master file - original image content. "
                                   "In a real application, this would be binary image data "
                                   "from a JPEG, PNG, or other image format.";
        std::vector<uint8_t> master_data(master_content.begin(), master_content.end());

        std::cout << "   Uploading master file (simulated image)..." << std::endl;
        std::cout << "   → Master file represents the original content" << std::endl;
        std::cout << "   → This could be an image, video, or document" << std::endl;
        std::cout << std::endl;

        fastdfs::Metadata master_metadata;
        master_metadata["type"] = "master";
        master_metadata["original"] = "true";
        master_metadata["width"] = "1920";
        master_metadata["height"] = "1080";
        master_metadata["format"] = "jpg";

        std::string master_file_id = client.upload_buffer(master_data, "jpg", &master_metadata);
        std::cout << "   ✓ Master file uploaded successfully" << std::endl;
        std::cout << "   File ID: " << master_file_id << std::endl;
        std::cout << "   → This file ID will be used to associate slave files" << std::endl;
        std::cout << std::endl;

        // Get master file information
        fastdfs::FileInfo master_info = client.get_file_info(master_file_id);
        std::cout << "   Master File Information:" << std::endl;
        std::cout << "   → Size: " << master_info.file_size << " bytes" << std::endl;
        std::cout << "   → Group: " << master_info.group_name << std::endl;
        std::cout << "   → Source IP: " << master_info.source_ip_addr << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Upload Slave File - Thumbnail
        // ====================================================================
        std::cout << "3. Upload Slave File - Thumbnail" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates creating slave files (variants of master files)." << std::endl;
        std::cout << "   Shows how to generate thumbnails, resized images, or other derived files." << std::endl;
        std::cout << std::endl;

        // Simulate thumbnail data (much smaller than master)
        std::string thumbnail_content = "Thumbnail version - small preview";
        std::vector<uint8_t> thumbnail_data(thumbnail_content.begin(), thumbnail_content.end());

        std::cout << "   Uploading thumbnail slave file..." << std::endl;
        std::cout << "   → Prefix: 'thumb' (identifies this as a thumbnail)" << std::endl;
        std::cout << "   → Master file ID: " << master_file_id << std::endl;
        std::cout << "   → Thumbnail size: " << thumbnail_data.size() 
                  << " bytes (smaller than master)" << std::endl;
        std::cout << std::endl;

        fastdfs::Metadata thumb_metadata;
        thumb_metadata["type"] = "thumbnail";
        thumb_metadata["master_file_id"] = master_file_id;
        thumb_metadata["width"] = "150";
        thumb_metadata["height"] = "150";
        thumb_metadata["format"] = "jpg";

        std::string thumb_file_id = client.upload_slave_file(
            master_file_id, "thumb", "jpg", thumbnail_data, &thumb_metadata);
        
        std::cout << "   ✓ Thumbnail slave file uploaded successfully" << std::endl;
        std::cout << "   Slave File ID: " << thumb_file_id << std::endl;
        std::cout << "   → Slave files are stored on the same storage server as master" << std::endl;
        std::cout << "   → They share the same group but have different filenames" << std::endl;
        std::cout << "   → Includes examples of linking slave files to master files" << std::endl;
        std::cout << std::endl;

        // Get thumbnail file information
        fastdfs::FileInfo thumb_info = client.get_file_info(thumb_file_id);
        std::cout << "   Thumbnail File Information:" << std::endl;
        std::cout << "   → Size: " << thumb_info.file_size << " bytes" << std::endl;
        std::cout << "   → Group: " << thumb_info.group_name << std::endl;
        std::cout << "   → Source IP: " << thumb_info.source_ip_addr << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Upload Slave File - Preview
        // ====================================================================
        std::cout << "4. Upload Slave File - Preview" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Previews are medium-sized versions of master files." << std::endl;
        std::cout << "   Larger than thumbnails but smaller than full masters." << std::endl;
        std::cout << std::endl;

        // Simulate preview data
        std::string preview_content = "Preview version - medium size for detailed view";
        std::vector<uint8_t> preview_data(preview_content.begin(), preview_content.end());

        std::cout << "   Uploading preview slave file..." << std::endl;
        std::cout << "   → Prefix: 'preview' (identifies this as a preview)" << std::endl;
        std::cout << "   → Master file ID: " << master_file_id << std::endl;
        std::cout << std::endl;

        fastdfs::Metadata preview_metadata;
        preview_metadata["type"] = "preview";
        preview_metadata["master_file_id"] = master_file_id;
        preview_metadata["width"] = "800";
        preview_metadata["height"] = "600";
        preview_metadata["format"] = "jpg";

        std::string preview_file_id = client.upload_slave_file(
            master_file_id, "preview", "jpg", preview_data, &preview_metadata);
        
        std::cout << "   ✓ Preview slave file uploaded successfully" << std::endl;
        std::cout << "   Slave File ID: " << preview_file_id << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Upload Slave File - Small Variant
        // ====================================================================
        std::cout << "5. Upload Slave File - Small Variant" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Small variants are optimized for mobile or low-bandwidth scenarios." << std::endl;
        std::cout << std::endl;

        std::string small_content = "Small variant - optimized for mobile";
        std::vector<uint8_t> small_data(small_content.begin(), small_content.end());

        fastdfs::Metadata small_metadata;
        small_metadata["type"] = "small";
        small_metadata["master_file_id"] = master_file_id;
        small_metadata["width"] = "640";
        small_metadata["height"] = "480";
        small_metadata["format"] = "jpg";
        small_metadata["optimized_for"] = "mobile";

        std::string small_file_id = client.upload_slave_file(
            master_file_id, "small", "jpg", small_data, &small_metadata);
        
        std::cout << "   ✓ Small variant slave file uploaded successfully" << std::endl;
        std::cout << "   Slave File ID: " << small_file_id << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Metadata Management for Slave Files
        // ====================================================================
        std::cout << "6. Metadata Management for Slave Files" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows metadata management for slave files." << std::endl;
        std::cout << std::endl;

        // Retrieve and display metadata for thumbnail
        std::cout << "   Retrieving metadata for thumbnail slave file..." << std::endl;
        fastdfs::Metadata retrieved_thumb_meta = client.get_metadata(thumb_file_id);
        std::cout << "   Thumbnail Metadata:" << std::endl;
        for (const auto& pair : retrieved_thumb_meta) {
            std::cout << "     " << pair.first << " = " << pair.second << std::endl;
        }
        std::cout << std::endl;

        // Update metadata for thumbnail
        std::cout << "   Updating thumbnail metadata..." << std::endl;
        fastdfs::Metadata updated_thumb_meta;
        updated_thumb_meta["quality"] = "high";
        updated_thumb_meta["generated_at"] = "2025-01-15";
        client.set_metadata(thumb_file_id, updated_thumb_meta, fastdfs::MetadataFlag::MERGE);
        
        fastdfs::Metadata final_thumb_meta = client.get_metadata(thumb_file_id);
        std::cout << "   Updated Thumbnail Metadata:" << std::endl;
        for (const auto& pair : final_thumb_meta) {
            std::cout << "     " << pair.first << " = " << pair.second << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: Download Slave Files
        // ====================================================================
        std::cout << "7. Download Slave Files" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Downloading slave files to verify they work correctly." << std::endl;
        std::cout << std::endl;

        std::cout << "   Downloading thumbnail..." << std::endl;
        std::vector<uint8_t> downloaded_thumb = client.download_file(thumb_file_id);
        std::cout << "   ✓ Downloaded " << downloaded_thumb.size() << " bytes" << std::endl;
        std::cout << std::endl;

        std::cout << "   Downloading preview..." << std::endl;
        std::vector<uint8_t> downloaded_preview = client.download_file(preview_file_id);
        std::cout << "   ✓ Downloaded " << downloaded_preview.size() << " bytes" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 7: Use Cases - Image Processing Workflow
        // ====================================================================
        std::cout << "8. Use Cases - Image Processing Workflow" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Useful for image processing, video transcoding, and file transformation workflows." << std::endl;
        std::cout << std::endl;

        std::cout << "   Image Processing Workflow:" << std::endl;
        std::cout << "   1. Upload original image as master file" << std::endl;
        std::cout << "   2. Generate and upload thumbnail (150x150)" << std::endl;
        std::cout << "   3. Generate and upload preview (800x600)" << std::endl;
        std::cout << "   4. Generate and upload small variant (640x480)" << std::endl;
        std::cout << "   5. All variants linked to master via metadata" << std::endl;
        std::cout << "   6. Serve appropriate variant based on client needs" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 8: Multiple Slave Files for One Master
        // ====================================================================
        std::cout << "9. Multiple Slave Files for One Master" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   A single master file can have multiple slave files with different prefixes." << std::endl;
        std::cout << std::endl;

        std::map<std::string, std::string> slave_files;
        slave_files["thumb"] = thumb_file_id;
        slave_files["preview"] = preview_file_id;
        slave_files["small"] = small_file_id;

        std::cout << "   Master File: " << master_file_id << std::endl;
        std::cout << "   Associated Slave Files:" << std::endl;
        for (const auto& pair : slave_files) {
            std::cout << "     - " << pair.first << ": " << pair.second << std::endl;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 9: Video Transcoding Use Case
        // ====================================================================
        std::cout << "10. Video Transcoding Use Case" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates how slave files can be used for video transcoding." << std::endl;
        std::cout << std::endl;

        std::cout << "   Video Transcoding Workflow:" << std::endl;
        std::cout << "   1. Upload original video as master file" << std::endl;
        std::cout << "   2. Transcode to different formats/resolutions:" << std::endl;
        std::cout << "      - 'mp4_720p' - MP4 format, 720p resolution" << std::endl;
        std::cout << "      - 'mp4_480p' - MP4 format, 480p resolution" << std::endl;
        std::cout << "      - 'webm' - WebM format for web playback" << std::endl;
        std::cout << "   3. Each transcoded version is a slave file" << std::endl;
        std::cout << "   4. Serve appropriate format based on client capabilities" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // CLEANUP
        // ====================================================================
        std::cout << "11. Cleaning up test files..." << std::endl;
        client.delete_file(thumb_file_id);
        std::cout << "   ✓ Thumbnail deleted" << std::endl;
        client.delete_file(preview_file_id);
        std::cout << "   ✓ Preview deleted" << std::endl;
        client.delete_file(small_file_id);
        std::cout << "   ✓ Small variant deleted" << std::endl;
        client.delete_file(master_file_id);
        std::cout << "   ✓ Master file deleted" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Creating slave files (variants of master files)" << std::endl;
        std::cout << "  ✓ Generating thumbnails, resized images, or other derived files" << std::endl;
        std::cout << "  ✓ Linking slave files to master files" << std::endl;
        std::cout << "  ✓ Metadata management for slave files" << std::endl;
        std::cout << "  ✓ Use cases for image processing workflows" << std::endl;
        std::cout << "  ✓ Use cases for video transcoding workflows" << std::endl;
        std::cout << "  ✓ File transformation workflows" << std::endl;

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

