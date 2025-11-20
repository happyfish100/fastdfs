//! FastDFS Metadata Operations Example
//!
//! This example demonstrates how to work with file metadata in FastDFS:
//! - Uploading files with initial metadata
//! - Retrieving metadata from stored files
//! - Updating metadata using overwrite mode
//! - Merging new metadata with existing metadata
//!
//! Metadata in FastDFS allows you to store arbitrary key-value pairs
//! associated with files, useful for storing file attributes, tags,
//! or application-specific information.
//!
//! Run this example with:
//! ```bash
//! cargo run --example metadata_example
//! ```

use fastdfs::{Client, ClientConfig, MetadataFlag};
use std::collections::HashMap;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Metadata Example");
    println!("{}", "=".repeat(50));

    // Step 1: Configure and create client
    // Set up the client with your tracker server address
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()]);
    let client = Client::new(config)?;

    // Example 1: Upload with metadata
    // Metadata can be attached during the initial upload operation
    println!("\n1. Uploading file with metadata...");
    let test_data = b"Document content with metadata";

    // Create initial metadata
    // These key-value pairs will be stored alongside the file
    let mut metadata = HashMap::new();
    metadata.insert("author".to_string(), "John Doe".to_string());
    metadata.insert("date".to_string(), "2025-01-15".to_string());
    metadata.insert("version".to_string(), "1.0".to_string());
    metadata.insert("department".to_string(), "Engineering".to_string());

    // Upload the file with metadata
    let file_id = client.upload_buffer(test_data, "txt", Some(&metadata)).await?;
    println!("   Uploaded successfully!");
    println!("   File ID: {}", file_id);

    // Example 2: Get metadata
    // Retrieve all metadata associated with the file
    println!("\n2. Getting metadata...");
    let retrieved_metadata = client.get_metadata(&file_id).await?;
    println!("   Metadata:");
    for (key, value) in &retrieved_metadata {
        println!("     {}: {}", key, value);
    }

    // Example 3: Update metadata (overwrite mode)
    // Overwrite mode replaces ALL existing metadata with new values
    // Any keys not in the new metadata will be removed
    println!("\n3. Updating metadata (overwrite mode)...");
    let mut new_metadata = HashMap::new();
    new_metadata.insert("author".to_string(), "Jane Smith".to_string());
    new_metadata.insert("date".to_string(), "2025-01-16".to_string());
    new_metadata.insert("status".to_string(), "reviewed".to_string());

    // Set the new metadata using overwrite mode
    client
        .set_metadata(&file_id, &new_metadata, MetadataFlag::Overwrite)
        .await?;

    // Retrieve and display the updated metadata
    let updated_metadata = client.get_metadata(&file_id).await?;
    println!("   Updated metadata:");
    for (key, value) in &updated_metadata {
        println!("     {}: {}", key, value);
    }

    // Example 4: Merge metadata
    // Merge mode adds new keys and updates existing ones,
    // but preserves keys that aren't in the new metadata
    println!("\n4. Merging metadata...");
    let mut merge_metadata = HashMap::new();
    merge_metadata.insert("reviewer".to_string(), "Bob Johnson".to_string());
    merge_metadata.insert("comments".to_string(), "Approved".to_string());

    // Merge the new metadata with existing metadata
    client
        .set_metadata(&file_id, &merge_metadata, MetadataFlag::Merge)
        .await?;

    // Retrieve and display the merged metadata
    let merged_metadata = client.get_metadata(&file_id).await?;
    println!("   Merged metadata:");
    for (key, value) in &merged_metadata {
        println!("     {}: {}", key, value);
    }

    // Example 5: Clean up
    // Delete the file and its associated metadata
    println!("\n5. Cleaning up...");
    client.delete_file(&file_id).await?;
    println!("   File deleted successfully!");

    println!("\n{}", "=".repeat(50));
    println!("Example completed successfully!");

    // Close the client and release resources
    client.close().await;

    Ok(())
}