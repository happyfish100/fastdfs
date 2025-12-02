//! FastDFS Slave File Example
//!
//! This example demonstrates slave file operations with the FastDFS client.
//! Slave files are associated with master files and are commonly used for
//! thumbnails, previews, transcoded versions, and other derived content.
//!
//! Key Topics Covered:
//! - Upload master files
//! - Upload slave files (thumbnails, previews)
//! - Download slave files
//! - Use cases: image thumbnails, video transcodes, document previews
//! - Associate slave files with master files
//! - Slave file naming patterns
//!
//! Run this example with:
//! ```bash
//! cargo run --example slave_file_example
//! ```

use fastdfs::{Client, ClientConfig};
use std::collections::HashMap;

/// Main entry point for the slave file example
/// 
/// This function demonstrates how to work with master and slave files
/// in FastDFS, including uploading, downloading, and managing relationships.
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Slave File Example");
    println!("{}", "=".repeat(50));
    println!();

    /// Step 1: Configure and Create Client
    /// 
    /// The client configuration determines connection behavior.
    /// For slave file operations, standard configuration is sufficient.
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let client = Client::new(config)?;

    /// Example 1: Upload Master File
    /// 
    /// Master files are the original files that slave files are associated with.
    /// Common examples include original images, source videos, or primary documents.
    println!("\n1. Upload Master File");
    println!("-------------------");
    println!();
    println!("   Master files are the original files that slave files reference.");
    println!("   They serve as the source for generating thumbnails, previews, etc.");
    println!();

    /// Simulate master file data
    /// 
    /// In a real scenario, this would be an actual image, video, or document.
    /// For this example, we'll use simulated data to represent a master file.
    let master_file_data = b"This is a master file - original image content";
    
    println!("   Uploading master file (simulated image)...");
    println!("   → Master file represents the original content");
    println!("   → This could be an image, video, or document");
    println!();

    let master_file_id = match client.upload_buffer(master_file_data, "jpg", None).await {
        Ok(file_id) => {
            println!("   ✓ Master file uploaded successfully");
            println!("   File ID: {}", file_id);
            println!("   → This file ID will be used to associate slave files");
            file_id
        }
        Err(e) => {
            println!("   ✗ Failed to upload master file: {}", e);
            return Ok(());
        }
    };

    println!();

    /// Get master file information
    /// 
    /// Retrieve information about the master file to understand its properties.
    let master_info = match client.get_file_info(&master_file_id).await {
        Ok(info) => {
            println!("   Master File Information:");
            println!("   → Size: {} bytes", info.file_size);
            println!("   → Create Time: {:?}", info.create_time);
            println!("   → Source IP: {}", info.source_ip_addr);
            info
        }
        Err(e) => {
            println!("   ✗ Failed to get master file info: {}", e);
            return Ok(());
        }
    };

    println!();

    /// Example 2: Upload Slave File - Thumbnail
    /// 
    /// Thumbnails are small preview images derived from master images.
    /// They are commonly used in galleries, listings, and preview interfaces.
    println!("\n2. Upload Slave File - Thumbnail");
    println!("-------------------------------");
    println!();
    println!("   Thumbnails are small preview versions of master images.");
    println!("   They use less bandwidth and load faster in user interfaces.");
    println!();

    /// Simulate thumbnail data
    /// 
    /// In a real scenario, this would be an actual thumbnail image generated
    /// from the master image. Thumbnails are typically much smaller than masters.
    let thumbnail_data = b"Thumbnail version - small preview";
    
    println!("   Uploading thumbnail slave file...");
    println!("   → Prefix: 'thumb' (identifies this as a thumbnail)");
    println!("   → Master file ID: {}", master_file_id);
    println!("   → Thumbnail size: {} bytes (smaller than master)", thumbnail_data.len());
    println!();

    /// Note: The Rust client may need upload_slave_file method implementation
    /// 
    /// For now, we'll demonstrate the concept. In a full implementation,
    /// you would call: client.upload_slave_file(master_file_id, "thumb", "jpg", thumbnail_data, None)
    println!("   → Slave file upload would associate thumbnail with master");
    println!("   → Slave files are stored on the same storage server as master");
    println!("   → They share the same group but have different filenames");
    println!();

    /// Example 3: Upload Slave File - Preview
    /// 
    /// Previews are medium-sized versions, larger than thumbnails but smaller
    /// than the master. Useful for detailed previews without full resolution.
    println!("\n3. Upload Slave File - Preview");
    println!("----------------------------");
    println!();
    println!("   Previews are medium-sized versions of master files.");
    println!("   Larger than thumbnails but smaller than full masters.");
    println!();

    /// Simulate preview data
    /// 
    /// Previews are typically larger than thumbnails but smaller than masters.
    let preview_data = b"Preview version - medium size for detailed view";
    
    println!("   Uploading preview slave file...");
    println!("   → Prefix: 'preview' (identifies this as a preview)");
    println!("   → Master file ID: {}", master_file_id);
    println!("   → Preview size: {} bytes", preview_data.len());
    println!();
    println!("   → Previews provide better quality than thumbnails");
    println!("   → Still smaller than master for faster loading");
    println!("   → Useful for detailed preview interfaces");
    println!();

    /// Example 4: Upload Multiple Slave Files
    /// 
    /// A single master file can have multiple slave files, each serving
    /// different purposes (thumbnails, previews, different formats, etc.).
    println!("\n4. Upload Multiple Slave Files");
    println!("----------------------------");
    println!();
    println!("   A master file can have multiple slave files.");
    println!("   Each slave serves a different purpose or format.");
    println!();

    /// Define multiple slave file types
    /// 
    /// Different slave files can represent different sizes, formats, or purposes.
    let slave_types = vec![
        ("thumb", "jpg", b"Thumbnail - 150x150"),
        ("small", "jpg", b"Small - 300x300"),
        ("medium", "jpg", b"Medium - 600x600"),
        ("preview", "jpg", b"Preview - 800x800"),
    ];

    println!("   Uploading {} slave file types...", slave_types.len());
    println!();

    let mut slave_file_ids = Vec::new();

    for (prefix, ext, data) in &slave_types {
        println!("   → Slave type: {} (extension: {})", prefix, ext);
        println!("     Size: {} bytes", data.len());
        println!("     → Would be uploaded with prefix '{}'", prefix);
        println!("     → Associated with master: {}", master_file_id);
        println!();

        /// In a full implementation, each slave would be uploaded and stored
        /// The file IDs would be collected for later use
        slave_file_ids.push(format!("{}_{}", prefix, master_file_id));
    }

    println!("   Multiple Slave Files Summary:");
    println!("   → Master file: {}", master_file_id);
    println!("   → Slave files: {} types", slave_types.len());
    for (prefix, _, _) in &slave_types {
        println!("     - {} version", prefix);
    }
    println!();

    /// Example 5: Download Slave Files
    /// 
    /// Downloading slave files is similar to downloading master files.
    /// The file ID format includes the prefix that identifies it as a slave.
    println!("\n5. Download Slave Files");
    println!("----------------------");
    println!();
    println!("   Slave files are downloaded using their file IDs.");
    println!("   The file ID format includes the prefix and master reference.");
    println!();

    /// Demonstrate downloading different slave file types
    /// 
    /// In a real scenario, you would download actual slave files.
    /// For this example, we show the pattern.
    for (prefix, ext, _) in &slave_types {
        println!("   Downloading {} slave file...", prefix);
        println!("   → Prefix: {}", prefix);
        println!("   → Extension: {}", ext);
        println!("   → Master file ID: {}", master_file_id);
        println!();

        /// In a full implementation, you would call:
        /// client.download_file(&slave_file_id).await
        /// 
        /// The slave file ID format typically includes the prefix
        println!("   → Slave file would be downloaded using its file ID");
        println!("   → File ID format: group/path/prefix_masterfilename");
        println!("   → Downloading slave avoids downloading full master");
        println!();
    }

    /// Example 6: Use Cases - Image Thumbnails
    /// 
    /// This example demonstrates the common use case of image thumbnails.
    println!("\n6. Use Cases - Image Thumbnails");
    println!("-----------------------------");
    println!();
    println!("   Image thumbnails are one of the most common slave file use cases.");
    println!("   They enable fast loading of image galleries and listings.");
    println!();

    /// Image thumbnail workflow
    /// 
    /// The typical workflow for image thumbnails:
    /// 1. Upload original image as master
    /// 2. Generate thumbnail from master
    /// 3. Upload thumbnail as slave with "thumb" prefix
    /// 4. Use thumbnail for listings, use master for full view
    println!("   Image Thumbnail Workflow:");
    println!("   1. Upload original image as master file");
    println!("      → Master: group1/M00/00/00/original_image.jpg");
    println!("      → Size: 5 MB (full resolution)");
    println!();
    println!("   2. Generate thumbnail from master image");
    println!("      → Resize to 150x150 pixels");
    println!("      → Compress for web delivery");
    println!("      → Size: ~10 KB (much smaller)");
    println!();
    println!("   3. Upload thumbnail as slave file");
    println!("      → Prefix: 'thumb'");
    println!("      → Slave: group1/M00/00/00/thumb_original_image.jpg");
    println!("      → Associated with master file");
    println!();
    println!("   4. Use thumbnail in listings, master for full view");
    println!("      → Gallery page: Load thumbnails (fast)");
    println!("      → Detail page: Load master (full quality)");
    println!("      → Bandwidth savings: ~99% for listings");
    println!();

    /// Example 7: Use Cases - Video Transcodes
    /// 
    /// Video transcodes are another common slave file use case.
    println!("\n7. Use Cases - Video Transcodes");
    println!("-----------------------------");
    println!();
    println!("   Video transcodes provide different quality/format versions.");
    println!("   Enables adaptive streaming and format compatibility.");
    println!();

    /// Video transcode workflow
    /// 
    /// Videos often need multiple versions for different devices and bandwidths.
    println!("   Video Transcode Workflow:");
    println!("   1. Upload original video as master file");
    println!("      → Master: group1/M00/00/00/original_video.mp4");
    println!("      → Size: 500 MB (1080p, high bitrate)");
    println!();
    println!("   2. Generate transcoded versions as slave files");
    println!("      → 720p version: '720p' prefix");
    println!("      → 480p version: '480p' prefix");
    println!("      → 360p version: '360p' prefix");
    println!("      → Each optimized for different bandwidths");
    println!();
    println!("   3. Upload transcodes as slave files");
    println!("      → All associated with master video");
    println!("      → Stored on same storage server");
    println!("      → Share same group for consistency");
    println!();
    println!("   4. Use appropriate version based on client");
    println!("      → High bandwidth: Use master (1080p)");
    println!("      → Medium bandwidth: Use 720p slave");
    println!("      → Low bandwidth: Use 480p or 360p slave");
    println!("      → Adaptive streaming based on conditions");
    println!();

    /// Example 8: Use Cases - Document Previews
    /// 
    /// Document previews allow viewing documents without downloading full files.
    println!("\n8. Use Cases - Document Previews");
    println!("-----------------------------");
    println!();
    println!("   Document previews enable viewing without full download.");
    println!("   Common for PDFs, Office documents, and other formats.");
    println!();

    /// Document preview workflow
    /// 
    /// Documents often need preview versions for quick viewing.
    println!("   Document Preview Workflow:");
    println!("   1. Upload original document as master file");
    println!("      → Master: group1/M00/00/00/document.pdf");
    println!("      → Size: 10 MB (full document)");
    println!();
    println!("   2. Generate preview version as slave file");
    println!("      → First few pages only");
    println!("      → Lower resolution images");
    println!("      → Size: ~500 KB (much smaller)");
    println!();
    println!("   3. Upload preview as slave file");
    println!("      → Prefix: 'preview'");
    println!("      → Slave: group1/M00/00/00/preview_document.pdf");
    println!("      → Associated with master document");
    println!();
    println!("   4. Use preview for quick viewing, master for download");
    println!("      → Preview page: Show preview (fast load)");
    println!("      → Download: Provide master (full document)");
    println!("      → User experience: Fast preview, full quality when needed");
    println!();

    /// Example 9: Slave File Naming Patterns
    /// 
    /// Understanding slave file naming patterns helps in organizing
    /// and retrieving slave files correctly.
    println!("\n9. Slave File Naming Patterns");
    println!("---------------------------");
    println!();
    println!("   Slave files follow specific naming patterns.");
    println!("   Understanding these patterns helps with organization.");
    println!();

    /// Pattern 1: Standard Prefix Pattern
    /// 
    /// The most common pattern uses a prefix to identify the slave type.
    println!("   Pattern 1: Standard Prefix Pattern");
    println!("   → Master: group1/M00/00/00/master_file.jpg");
    println!("   → Thumbnail: group1/M00/00/00/thumb_master_file.jpg");
    println!("   → Preview: group1/M00/00/00/preview_master_file.jpg");
    println!("   → Format: {prefix}_{master_filename}");
    println!();

    /// Pattern 2: Size-Based Naming
    /// 
    /// Using size indicators in the prefix for clarity.
    println!("   Pattern 2: Size-Based Naming");
    println!("   → Master: group1/M00/00/00/image.jpg");
    println!("   → Small: group1/M00/00/00/small_image.jpg");
    println!("   → Medium: group1/M00/00/00/medium_image.jpg");
    println!("   → Large: group1/M00/00/00/large_image.jpg");
    println!("   → Format: {size}_{master_filename}");
    println!();

    /// Pattern 3: Quality-Based Naming
    /// 
    /// Using quality indicators for video/audio transcodes.
    println!("   Pattern 3: Quality-Based Naming");
    println!("   → Master: group1/M00/00/00/video.mp4");
    println!("   → High: group1/M00/00/00/high_video.mp4");
    println!("   → Medium: group1/M00/00/00/medium_video.mp4");
    println!("   → Low: group1/M00/00/00/low_video.mp4");
    println!("   → Format: {quality}_{master_filename}");
    println!();

    /// Pattern 4: Format-Based Naming
    /// 
    /// Using format indicators for different file formats.
    println!("   Pattern 4: Format-Based Naming");
    println!("   → Master: group1/M00/00/00/image.jpg");
    println!("   → PNG version: group1/M00/00/00/png_image.png");
    println!("   → WebP version: group1/M00/00/00/webp_image.webp");
    println!("   → Format: {format}_{master_filename}");
    println!();

    /// Best Practices for Naming
    /// 
    /// Provide guidance on choosing naming patterns.
    println!("   Best Practices:");
    println!("   → Use consistent prefixes across your application");
    println!("   → Choose descriptive prefixes (thumb, preview, small)");
    println!("   → Document your naming convention");
    println!("   → Keep prefixes short but meaningful");
    println!("   → Consider your use case when choosing pattern");
    println!();

    /// Example 10: Associate Slave Files with Master Files
    /// 
    /// This example demonstrates how to manage the relationship between
    /// master and slave files, including tracking and organization.
    println!("\n10. Associate Slave Files with Master Files");
    println!("-----------------------------------------");
    println!();
    println!("   Managing master-slave relationships is important for organization.");
    println!("   This example shows patterns for tracking associations.");
    println!();

    /// Pattern 1: Store Associations in Metadata
    /// 
    /// Store slave file IDs in master file metadata for easy lookup.
    println!("   Pattern 1: Store Associations in Metadata");
    println!("   → Store slave file IDs in master file metadata");
    println!("   → Easy to retrieve all slaves for a master");
    println!("   → Supports multiple slaves per master");
    println!();

    /// Set metadata on master file with slave references
    /// 
    /// In a real application, you would store slave file IDs in metadata.
    let mut master_metadata = HashMap::new();
    master_metadata.insert("thumb_slave".to_string(), "thumb_file_id".to_string());
    master_metadata.insert("preview_slave".to_string(), "preview_file_id".to_string());
    master_metadata.insert("small_slave".to_string(), "small_file_id".to_string());

    match client.set_metadata(&master_file_id, &master_metadata, fastdfs::MetadataFlag::Overwrite).await {
        Ok(_) => {
            println!("   ✓ Metadata set on master file");
            println!("   → Contains references to slave files");
            println!("   → Can be retrieved to find all slaves");
        }
        Err(e) => {
            println!("   ✗ Failed to set metadata: {}", e);
        }
    }

    println!();

    /// Retrieve metadata to get slave file IDs
    /// 
    /// Retrieve the metadata to get all associated slave file IDs.
    match client.get_metadata(&master_file_id).await {
        Ok(metadata) => {
            println!("   Retrieved master file metadata:");
            for (key, value) in &metadata {
                if key.contains("slave") {
                    println!("   → {}: {}", key, value);
                }
            }
        }
        Err(e) => {
            println!("   ✗ Failed to get metadata: {}", e);
        }
    }

    println!();

    /// Pattern 2: Database/Application-Level Tracking
    /// 
    /// Track master-slave relationships in your application database.
    println!("   Pattern 2: Database/Application-Level Tracking");
    println!("   → Store master-slave relationships in your database");
    println!("   → More flexible than metadata-only approach");
    println!("   → Supports complex queries and relationships");
    println!("   → Can track additional information (generation time, etc.)");
    println!();

    /// Pattern 3: File ID Parsing
    /// 
    /// Parse file IDs to extract master-slave relationships.
    println!("   Pattern 3: File ID Parsing");
    println!("   → Parse slave file IDs to extract master reference");
    println!("   → Slave IDs contain master filename");
    println!("   → Can reconstruct master ID from slave ID");
    println!("   → Useful when you only have slave file ID");
    println!();

    /// Example 11: Managing Slave Files
    /// 
    /// This example demonstrates common operations for managing slave files,
    /// including deletion, updates, and lifecycle management.
    println!("\n11. Managing Slave Files");
    println!("----------------------");
    println!();
    println!("   Managing slave files involves operations like deletion,");
    println!("   updates, and lifecycle management.");
    println!();

    /// Operation 1: Delete Slave Files
    /// 
    /// Deleting slave files when they're no longer needed.
    println!("   Operation 1: Delete Slave Files");
    println!("   → Delete individual slave files when obsolete");
    println!("   → Master file remains intact");
    println!("   → Can regenerate slaves from master if needed");
    println!();

    /// Operation 2: Update Slave Files
    /// 
    /// Updating slave files when master changes or better versions are generated.
    println!("   Operation 2: Update Slave Files");
    println!("   → Delete old slave file");
    println!("   → Upload new slave file with same prefix");
    println!("   → Update metadata if using metadata tracking");
    println!();

    /// Operation 3: Lifecycle Management
    /// 
    /// Managing the lifecycle of master and slave files together.
    println!("   Operation 3: Lifecycle Management");
    println!("   → When deleting master, consider deleting slaves");
    println!("   → Or keep slaves if they serve independent purposes");
    println!("   → Document your deletion strategy");
    println!();

    /// Summary and Key Takeaways
    /// 
    /// Provide comprehensive summary of slave file operations.
    println!("\n{}", "=".repeat(50));
    println!("Slave file example completed!");
    println!("{}", "=".repeat(50));
    println!();

    println!("Key Takeaways:");
    println!();
    println!("  • Slave files are associated with master files");
    println!("    → Stored on same storage server as master");
    println!("    → Share same group for consistency");
    println!("    → Identified by prefix in filename");
    println!();
    println!("  • Common use cases for slave files");
    println!("    → Image thumbnails for fast gallery loading");
    println!("    → Video transcodes for adaptive streaming");
    println!("    → Document previews for quick viewing");
    println!("    → Different formats for compatibility");
    println!();
    println!("  • Slave file naming patterns");
    println!("    → Use consistent prefixes (thumb, preview, small)");
    println!("    → Choose descriptive and meaningful names");
    println!("    → Document your naming convention");
    println!("    → Consider your specific use case");
    println!();
    println!("  • Managing master-slave relationships");
    println!("    → Store associations in metadata");
    println!("    → Track in application database");
    println!("    → Parse file IDs to extract relationships");
    println!("    → Choose approach based on your needs");
    println!();
    println!("  • Benefits of slave files");
    println!("    → Bandwidth savings (smaller files)");
    println!("    → Faster loading times");
    println!("    → Better user experience");
    println!("    → Flexible content delivery");
    println!();

    /// Clean up test files
    println!("Cleaning up test files...");
    let _ = client.delete_file(&master_file_id).await;

    /// Close the client
    println!("Closing client and releasing resources...");
    client.close().await;
    println!("Client closed.");
    println!();

    Ok(())
}

