/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Advanced Metadata Example
 *
 * This comprehensive example demonstrates advanced metadata operations including
 * merging, overwriting, conditional updates, versioning patterns, and using metadata
 * for file organization and search.
 *
 * Key Topics Covered:
 * - Demonstrates advanced metadata operations
 * - Shows metadata merging, overwriting, and conditional updates
 * - Includes examples of metadata queries and filtering
 * - Demonstrates metadata versioning patterns
 * - Useful for complex metadata management scenarios
 * - Shows how to use metadata for file organization and search
 *
 * Run this example with:
 *   ./advanced_metadata_example <tracker_address>
 *   Example: ./advanced_metadata_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <chrono>

// Helper function to print metadata
void print_metadata(const fastdfs::Metadata& metadata, const std::string& title = "Metadata") {
    std::cout << "   " << title << ":" << std::endl;
    if (metadata.empty()) {
        std::cout << "     (empty)" << std::endl;
    } else {
        for (const auto& pair : metadata) {
            std::cout << "     " << std::setw(20) << std::left << pair.first 
                     << " = " << pair.second << std::endl;
        }
    }
}

// Helper function to merge two metadata maps (client-side)
fastdfs::Metadata merge_metadata(const fastdfs::Metadata& existing, 
                                 const fastdfs::Metadata& updates) {
    fastdfs::Metadata result = existing;
    for (const auto& pair : updates) {
        result[pair.first] = pair.second;
    }
    return result;
}

// Helper function to filter metadata by key prefix
fastdfs::Metadata filter_metadata_by_prefix(const fastdfs::Metadata& metadata, 
                                            const std::string& prefix) {
    fastdfs::Metadata filtered;
    for (const auto& pair : metadata) {
        if (pair.first.substr(0, prefix.length()) == prefix) {
            filtered[pair.first] = pair.second;
        }
    }
    return filtered;
}

// Helper function to check if metadata matches criteria
bool metadata_matches(const fastdfs::Metadata& metadata, 
                     const std::map<std::string, std::string>& criteria) {
    for (const auto& criterion : criteria) {
        auto it = metadata.find(criterion.first);
        if (it == metadata.end() || it->second != criterion.second) {
            return false;
        }
    }
    return true;
}

// Helper function to get current timestamp as string
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Helper function to increment version
std::string increment_version(const std::string& version) {
    // Simple version increment (e.g., "1.0" -> "1.1", "2.3" -> "2.4")
    size_t dot_pos = version.find_last_of('.');
    if (dot_pos != std::string::npos) {
        int major = std::stoi(version.substr(0, dot_pos));
        int minor = std::stoi(version.substr(dot_pos + 1));
        minor++;
        return std::to_string(major) + "." + std::to_string(minor);
    }
    return version + ".1";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Advanced Metadata Example" << std::endl;
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
        // EXAMPLE 1: Advanced Metadata Merging
        // ====================================================================
        std::cout << "2. Advanced Metadata Merging" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates advanced metadata operations with merging." << std::endl;
        std::cout << std::endl;

        // Upload file with initial metadata
        std::cout << "   Uploading file with initial metadata..." << std::endl;
        std::vector<uint8_t> data = {'A', 'd', 'v', 'a', 'n', 'c', 'e', 'd', ' ', 'M', 'e', 't', 'a', 'd', 'a', 't', 'a'};
        
        fastdfs::Metadata initial_metadata;
        initial_metadata["type"] = "document";
        initial_metadata["category"] = "technical";
        initial_metadata["author"] = "John Doe";
        initial_metadata["created_at"] = get_timestamp();
        initial_metadata["status"] = "draft";
        
        std::string file_id = client.upload_buffer(data, "txt", &initial_metadata);
        std::cout << "   ✓ File uploaded: " << file_id << std::endl;
        print_metadata(initial_metadata, "Initial Metadata");
        std::cout << std::endl;

        // Merge with new metadata (preserves existing, updates/adds new)
        std::cout << "   Merging with new metadata..." << std::endl;
        fastdfs::Metadata merge_updates;
        merge_updates["status"] = "published";  // Update existing
        merge_updates["published_at"] = get_timestamp();  // Add new
        merge_updates["editor"] = "Jane Smith";  // Add new
        
        client.set_metadata(file_id, merge_updates, fastdfs::MetadataFlag::MERGE);
        
        fastdfs::Metadata merged_metadata = client.get_metadata(file_id);
        print_metadata(merged_metadata, "Merged Metadata");
        std::cout << "   → Note: 'status' was updated, new fields were added" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Conditional Metadata Updates
        // ====================================================================
        std::cout << "3. Conditional Metadata Updates" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows metadata merging, overwriting, and conditional updates." << std::endl;
        std::cout << std::endl;

        // Conditional update: only update if certain conditions are met
        std::cout << "   Implementing conditional update..." << std::endl;
        fastdfs::Metadata current_metadata = client.get_metadata(file_id);
        
        // Only update if status is "published"
        if (current_metadata.find("status") != current_metadata.end() && 
            current_metadata["status"] == "published") {
            fastdfs::Metadata conditional_update;
            conditional_update["last_modified"] = get_timestamp();
            conditional_update["modified_by"] = "System";
            
            client.set_metadata(file_id, conditional_update, fastdfs::MetadataFlag::MERGE);
            std::cout << "   ✓ Conditional update applied (status was 'published')" << std::endl;
        } else {
            std::cout << "   → Conditional update skipped (status not 'published')" << std::endl;
        }
        
        fastdfs::Metadata updated_metadata = client.get_metadata(file_id);
        print_metadata(updated_metadata, "After Conditional Update");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Metadata Overwriting Strategies
        // ====================================================================
        std::cout << "4. Metadata Overwriting Strategies" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates different overwriting strategies." << std::endl;
        std::cout << std::endl;

        // Strategy 1: Complete overwrite
        std::cout << "   Strategy 1: Complete overwrite" << std::endl;
        fastdfs::Metadata complete_overwrite;
        complete_overwrite["type"] = "archive";
        complete_overwrite["archived_at"] = get_timestamp();
        
        client.set_metadata(file_id, complete_overwrite, fastdfs::MetadataFlag::OVERWRITE);
        fastdfs::Metadata overwritten = client.get_metadata(file_id);
        print_metadata(overwritten, "After Complete Overwrite");
        std::cout << "   → All previous metadata was replaced" << std::endl;
        std::cout << std::endl;

        // Strategy 2: Selective overwrite (client-side merge with selective replacement)
        std::cout << "   Strategy 2: Selective overwrite (preserve some, replace others)" << std::endl;
        fastdfs::Metadata current = client.get_metadata(file_id);
        
        // Preserve 'type', replace everything else
        fastdfs::Metadata selective;
        selective["type"] = current["type"];  // Preserve
        selective["category"] = "archived";
        selective["archived_by"] = "Admin";
        selective["archived_at"] = get_timestamp();
        
        client.set_metadata(file_id, selective, fastdfs::MetadataFlag::OVERWRITE);
        fastdfs::Metadata selective_result = client.get_metadata(file_id);
        print_metadata(selective_result, "After Selective Overwrite");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Metadata Versioning Patterns
        // ====================================================================
        std::cout << "5. Metadata Versioning Patterns" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates metadata versioning patterns." << std::endl;
        std::cout << std::endl;

        // Create a new file for versioning example
        std::cout << "   Creating file with versioned metadata..." << std::endl;
        std::vector<uint8_t> versioned_data = {'V', 'e', 'r', 's', 'i', 'o', 'n', 'e', 'd', ' ', 'F', 'i', 'l', 'e'};
        
        fastdfs::Metadata versioned_metadata;
        versioned_metadata["version"] = "1.0";
        versioned_metadata["version_history"] = "1.0:initial";
        versioned_metadata["created_at"] = get_timestamp();
        versioned_metadata["type"] = "document";
        
        std::string versioned_file_id = client.upload_buffer(versioned_data, "txt", &versioned_metadata);
        std::cout << "   ✓ File uploaded: " << versioned_file_id << std::endl;
        print_metadata(versioned_metadata, "Version 1.0 Metadata");
        std::cout << std::endl;

        // Update version
        std::cout << "   Updating to version 1.1..." << std::endl;
        fastdfs::Metadata current_versioned = client.get_metadata(versioned_file_id);
        std::string current_version = current_versioned["version"];
        std::string new_version = increment_version(current_version);
        
        fastdfs::Metadata version_update;
        version_update["version"] = new_version;
        version_update["version_history"] = current_versioned["version_history"] + ";" + 
                                           new_version + ":minor_update";
        version_update["updated_at"] = get_timestamp();
        version_update["changelog"] = "Minor bug fixes";
        
        client.set_metadata(versioned_file_id, version_update, fastdfs::MetadataFlag::MERGE);
        fastdfs::Metadata updated_versioned = client.get_metadata(versioned_file_id);
        print_metadata(updated_versioned, "Version 1.1 Metadata");
        std::cout << std::endl;

        // Major version update
        std::cout << "   Updating to version 2.0 (major update)..." << std::endl;
        fastdfs::Metadata major_update;
        major_update["version"] = "2.0";
        major_update["version_history"] = updated_versioned["version_history"] + ";2.0:major_update";
        major_update["updated_at"] = get_timestamp();
        major_update["changelog"] = "Major feature additions";
        major_update["breaking_changes"] = "true";
        
        client.set_metadata(versioned_file_id, major_update, fastdfs::MetadataFlag::MERGE);
        fastdfs::Metadata major_versioned = client.get_metadata(versioned_file_id);
        print_metadata(major_versioned, "Version 2.0 Metadata");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Metadata Queries and Filtering
        // ====================================================================
        std::cout << "6. Metadata Queries and Filtering" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes examples of metadata queries and filtering." << std::endl;
        std::cout << std::endl;

        // Create multiple files with different metadata for querying
        std::cout << "   Creating multiple files with different metadata..." << std::endl;
        std::vector<std::string> file_ids;
        std::vector<fastdfs::Metadata> all_metadata;
        
        // File 1: Technical document
        std::vector<uint8_t> file1_data = {'T', 'e', 'c', 'h', 'n', 'i', 'c', 'a', 'l'};
        fastdfs::Metadata file1_meta;
        file1_meta["type"] = "document";
        file1_meta["category"] = "technical";
        file1_meta["department"] = "engineering";
        file1_meta["priority"] = "high";
        std::string file1_id = client.upload_buffer(file1_data, "txt", &file1_meta);
        file_ids.push_back(file1_id);
        all_metadata.push_back(file1_meta);
        std::cout << "   → File 1: " << file1_id << " (technical, high priority)" << std::endl;
        
        // File 2: Marketing document
        std::vector<uint8_t> file2_data = {'M', 'a', 'r', 'k', 'e', 't', 'i', 'n', 'g'};
        fastdfs::Metadata file2_meta;
        file2_meta["type"] = "document";
        file2_meta["category"] = "marketing";
        file2_meta["department"] = "sales";
        file2_meta["priority"] = "medium";
        std::string file2_id = client.upload_buffer(file2_data, "txt", &file2_meta);
        file_ids.push_back(file2_id);
        all_metadata.push_back(file2_meta);
        std::cout << "   → File 2: " << file2_id << " (marketing, medium priority)" << std::endl;
        
        // File 3: Another technical document
        std::vector<uint8_t> file3_data = {'T', 'e', 'c', 'h', '2'};
        fastdfs::Metadata file3_meta;
        file3_meta["type"] = "document";
        file3_meta["category"] = "technical";
        file3_meta["department"] = "engineering";
        file3_meta["priority"] = "low";
        std::string file3_id = client.upload_buffer(file3_data, "txt", &file3_meta);
        file_ids.push_back(file3_id);
        all_metadata.push_back(file3_meta);
        std::cout << "   → File 3: " << file3_id << " (technical, low priority)" << std::endl;
        
        std::cout << std::endl;

        // Query 1: Find files by category
        std::cout << "   Query 1: Find files with category='technical'" << std::endl;
        std::map<std::string, std::string> query1 = {{"category", "technical"}};
        std::vector<std::string> matching_files;
        
        for (size_t i = 0; i < file_ids.size(); ++i) {
            fastdfs::Metadata file_meta = client.get_metadata(file_ids[i]);
            if (metadata_matches(file_meta, query1)) {
                matching_files.push_back(file_ids[i]);
                std::cout << "     ✓ Match: " << file_ids[i] << std::endl;
            }
        }
        std::cout << "   → Found " << matching_files.size() << " matching file(s)" << std::endl;
        std::cout << std::endl;

        // Query 2: Find files by multiple criteria
        std::cout << "   Query 2: Find files with category='technical' AND priority='high'" << std::endl;
        std::map<std::string, std::string> query2 = {{"category", "technical"}, {"priority", "high"}};
        matching_files.clear();
        
        for (size_t i = 0; i < file_ids.size(); ++i) {
            fastdfs::Metadata file_meta = client.get_metadata(file_ids[i]);
            if (metadata_matches(file_meta, query2)) {
                matching_files.push_back(file_ids[i]);
                std::cout << "     ✓ Match: " << file_ids[i] << std::endl;
            }
        }
        std::cout << "   → Found " << matching_files.size() << " matching file(s)" << std::endl;
        std::cout << std::endl;

        // Filter by prefix
        std::cout << "   Filter 1: Get all metadata keys with prefix 'dep'" << std::endl;
        fastdfs::Metadata file1_full = client.get_metadata(file1_id);
        fastdfs::Metadata filtered = filter_metadata_by_prefix(file1_full, "dep");
        print_metadata(filtered, "Filtered Metadata (prefix 'dep')");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: File Organization with Metadata
        // ====================================================================
        std::cout << "7. File Organization with Metadata" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to use metadata for file organization and search." << std::endl;
        std::cout << std::endl;

        // Organize files by tags
        std::cout << "   Organizing files with tags..." << std::endl;
        
        // Add tags to existing files
        fastdfs::Metadata tags1;
        tags1["tags"] = "api,documentation,backend";
        tags1["project"] = "api-server";
        client.set_metadata(file1_id, tags1, fastdfs::MetadataFlag::MERGE);
        
        fastdfs::Metadata tags2;
        tags2["tags"] = "marketing,public,frontend";
        tags2["project"] = "website";
        client.set_metadata(file2_id, tags2, fastdfs::MetadataFlag::MERGE);
        
        fastdfs::Metadata tags3;
        tags3["tags"] = "api,internal,backend";
        tags3["project"] = "api-server";
        client.set_metadata(file3_id, tags3, fastdfs::MetadataFlag::MERGE);
        
        std::cout << "   ✓ Tags added to all files" << std::endl;
        std::cout << std::endl;

        // Search by project
        std::cout << "   Search: Find all files in project 'api-server'" << std::endl;
        std::map<std::string, std::string> project_query = {{"project", "api-server"}};
        std::vector<std::string> project_files;
        
        for (const auto& fid : file_ids) {
            fastdfs::Metadata file_meta = client.get_metadata(fid);
            if (metadata_matches(file_meta, project_query)) {
                project_files.push_back(fid);
                std::cout << "     ✓ " << fid << std::endl;
                print_metadata(file_meta, "  Metadata");
            }
        }
        std::cout << "   → Found " << project_files.size() << " file(s) in project 'api-server'" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 7: Complex Metadata Management
        // ====================================================================
        std::cout << "8. Complex Metadata Management Scenarios" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Useful for complex metadata management scenarios." << std::endl;
        std::cout << std::endl;

        // Scenario: Workflow state management
        std::cout << "   Scenario: Workflow state management" << std::endl;
        std::vector<uint8_t> workflow_data = {'W', 'o', 'r', 'k', 'f', 'l', 'o', 'w'};
        
        fastdfs::Metadata workflow_meta;
        workflow_meta["workflow_state"] = "pending";
        workflow_meta["workflow_steps"] = "upload,review,approve,publish";
        workflow_meta["current_step"] = "upload";
        workflow_meta["assigned_to"] = "user1";
        workflow_meta["created_at"] = get_timestamp();
        
        std::string workflow_file_id = client.upload_buffer(workflow_data, "txt", &workflow_meta);
        std::cout << "   ✓ Workflow file created: " << workflow_file_id << std::endl;
        print_metadata(workflow_meta, "Initial Workflow Metadata");
        std::cout << std::endl;

        // Transition: pending -> in_review
        std::cout << "   Transition: pending -> in_review" << std::endl;
        fastdfs::Metadata transition1;
        transition1["workflow_state"] = "in_review";
        transition1["current_step"] = "review";
        transition1["reviewed_at"] = get_timestamp();
        transition1["reviewed_by"] = "user2";
        
        client.set_metadata(workflow_file_id, transition1, fastdfs::MetadataFlag::MERGE);
        fastdfs::Metadata after_review = client.get_metadata(workflow_file_id);
        print_metadata(after_review, "After Review");
        std::cout << std::endl;

        // Transition: in_review -> approved
        std::cout << "   Transition: in_review -> approved" << std::endl;
        fastdfs::Metadata transition2;
        transition2["workflow_state"] = "approved";
        transition2["current_step"] = "approve";
        transition2["approved_at"] = get_timestamp();
        transition2["approved_by"] = "user3";
        
        client.set_metadata(workflow_file_id, transition2, fastdfs::MetadataFlag::MERGE);
        fastdfs::Metadata after_approval = client.get_metadata(workflow_file_id);
        print_metadata(after_approval, "After Approval");
        std::cout << std::endl;

        // Scenario: Audit trail
        std::cout << "   Scenario: Audit trail with metadata" << std::endl;
        fastdfs::Metadata audit_meta;
        audit_meta["audit_trail"] = "created:user1:" + get_timestamp();
        audit_meta["last_modified_by"] = "user1";
        audit_meta["modification_count"] = "1";
        
        std::string audit_file_id = client.upload_buffer(workflow_data, "txt", &audit_meta);
        
        // Add to audit trail
        fastdfs::Metadata audit_update;
        fastdfs::Metadata current_audit = client.get_metadata(audit_file_id);
        std::string new_audit_entry = "modified:user2:" + get_timestamp();
        audit_update["audit_trail"] = current_audit["audit_trail"] + ";" + new_audit_entry;
        audit_update["last_modified_by"] = "user2";
        audit_update["modification_count"] = std::to_string(std::stoi(current_audit["modification_count"]) + 1);
        
        client.set_metadata(audit_file_id, audit_update, fastdfs::MetadataFlag::MERGE);
        fastdfs::Metadata final_audit = client.get_metadata(audit_file_id);
        print_metadata(final_audit, "Audit Trail Metadata");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 8: Metadata for Search and Discovery
        // ====================================================================
        std::cout << "9. Metadata for Search and Discovery" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Advanced patterns for using metadata in search scenarios." << std::endl;
        std::cout << std::endl;

        // Create files with rich metadata for search
        std::cout << "   Creating files with rich searchable metadata..." << std::endl;
        
        std::vector<uint8_t> search_data1 = {'S', 'e', 'a', 'r', 'c', 'h', '1'};
        fastdfs::Metadata search_meta1;
        search_meta1["title"] = "API Documentation";
        search_meta1["description"] = "Complete API reference guide";
        search_meta1["keywords"] = "api,rest,documentation,reference";
        search_meta1["content_type"] = "text/markdown";
        search_meta1["language"] = "en";
        search_meta1["author"] = "Tech Writer";
        std::string search_file1 = client.upload_buffer(search_data1, "txt", &search_meta1);
        std::cout << "   → File 1: " << search_file1 << std::endl;
        
        std::vector<uint8_t> search_data2 = {'S', 'e', 'a', 'r', 'c', 'h', '2'};
        fastdfs::Metadata search_meta2;
        search_meta2["title"] = "User Guide";
        search_meta2["description"] = "User manual for the application";
        search_meta2["keywords"] = "guide,user,manual,tutorial";
        search_meta2["content_type"] = "text/html";
        search_meta2["language"] = "en";
        search_meta2["author"] = "Tech Writer";
        std::string search_file2 = client.upload_buffer(search_data2, "txt", &search_meta2);
        std::cout << "   → File 2: " << search_file2 << std::endl;
        
        std::cout << std::endl;

        // Search by author
        std::cout << "   Search: Find all files by 'Tech Writer'" << std::endl;
        std::map<std::string, std::string> author_query = {{"author", "Tech Writer"}};
        std::vector<std::string> author_files = {search_file1, search_file2};
        
        for (const auto& fid : author_files) {
            fastdfs::Metadata file_meta = client.get_metadata(fid);
            if (metadata_matches(file_meta, author_query)) {
                std::cout << "     ✓ " << fid << " - " << file_meta["title"] << std::endl;
            }
        }
        std::cout << std::endl;

        // Search by content type
        std::cout << "   Search: Find all files with content_type='text/markdown'" << std::endl;
        std::map<std::string, std::string> type_query = {{"content_type", "text/markdown"}};
        
        for (const auto& fid : author_files) {
            fastdfs::Metadata file_meta = client.get_metadata(fid);
            if (metadata_matches(file_meta, type_query)) {
                std::cout << "     ✓ " << fid << " - " << file_meta["title"] << std::endl;
            }
        }
        std::cout << std::endl;

        // ====================================================================
        // CLEANUP
        // ====================================================================
        std::cout << "10. Cleaning up test files..." << std::endl;
        client.delete_file(file_id);
        client.delete_file(versioned_file_id);
        client.delete_file(file1_id);
        client.delete_file(file2_id);
        client.delete_file(file3_id);
        client.delete_file(workflow_file_id);
        client.delete_file(audit_file_id);
        client.delete_file(search_file1);
        client.delete_file(search_file2);
        std::cout << "   ✓ All test files deleted" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Advanced metadata operations" << std::endl;
        std::cout << "  ✓ Metadata merging, overwriting, and conditional updates" << std::endl;
        std::cout << "  ✓ Metadata queries and filtering" << std::endl;
        std::cout << "  ✓ Metadata versioning patterns" << std::endl;
        std::cout << "  ✓ Complex metadata management scenarios" << std::endl;
        std::cout << "  ✓ Using metadata for file organization and search" << std::endl;
        std::cout << std::endl;
        std::cout << "Best Practices:" << std::endl;
        std::cout << "  • Use MERGE flag to preserve existing metadata when updating" << std::endl;
        std::cout << "  • Use OVERWRITE flag to replace all metadata" << std::endl;
        std::cout << "  • Implement conditional updates based on current metadata state" << std::endl;
        std::cout << "  • Use versioning patterns for tracking changes" << std::endl;
        std::cout << "  • Organize files using consistent metadata schemas" << std::endl;
        std::cout << "  • Use metadata for search and discovery (client-side filtering)" << std::endl;
        std::cout << "  • Maintain audit trails in metadata for compliance" << std::endl;
        std::cout << "  • Use prefixes for metadata namespaces (e.g., 'workflow_', 'audit_')" << std::endl;

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

