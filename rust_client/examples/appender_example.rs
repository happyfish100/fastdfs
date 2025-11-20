//! FastDFS Appender File Operations Example
//!
//! This example demonstrates working with appender files in FastDFS.
//! Appender files are special files that support modification operations
//! like append, modify, and truncate, making them suitable for log files
//! or other files that need to be updated after creation.
//!
//! Note: Appender file operations require proper storage server configuration.
//! Not all FastDFS deployments may have this feature enabled.
//!
//! Run this example with:
//! ```bash
//! cargo run --example appender_example
//! ```

use fastdfs::{Client, ClientConfig};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Appender File Example");
    println!("{}", "=".repeat(50));

    // Step 1: Configure and create client
    // Set up the client with your tracker server address
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()]);
    let client = Client::new(config)?;

    // Example 1: Upload appender file
    // Appender files are created using a special upload command
    // that marks them as modifiable
    println!("\n1. Uploading appender file...");
    let initial_data = b"Initial log entry\n";
    let file_id = client
        .upload_appender_buffer(initial_data, "log", None)
        .await?;
    println!("   Uploaded successfully!");
    println!("   File ID: {}", file_id);

    // Example 2: Get initial file info
    // Retrieve information about the newly created appender file
    println!("\n2. Getting initial file information...");
    let file_info = client.get_file_info(&file_id).await?;
    println!("   File size: {} bytes", file_info.file_size);
    println!("   Create time: {:?}", file_info.create_time);
    println!("   CRC32: {}", file_info.crc32);

    // Example 3: Download and display content
    // Verify the initial content of the appender file
    println!("\n3. Downloading file content...");
    let content = client.download_file(&file_id).await?;
    println!("   Content:");
    println!("{}", String::from_utf8_lossy(&content));

    // Example 4: Information about appender operations
    // Note: The actual append, modify, and truncate operations require
    // storage server support and are not demonstrated here
    println!("\n4. Appender file operations:");
    println!("   - Append: Adds data to the end of the file");
    println!("     Usage: Ideal for log files that grow over time");
    println!("   - Modify: Changes data at a specific offset");
    println!("     Usage: Update specific sections without rewriting entire file");
    println!("   - Truncate: Reduces file size to specified length");
    println!("     Usage: Remove old log entries or resize files");
    println!("\n   Note: These operations require storage server support");
    println!("   Check your FastDFS storage configuration to enable appender files");

    // Example 5: Clean up
    // Delete the appender file
    println!("\n5. Cleaning up...");
    client.delete_file(&file_id).await?;
    println!("   File deleted successfully!");

    println!("\n{}", "=".repeat(50));
    println!("Example completed successfully!");

    // Close the client and release all resources
    client.close().await;

    Ok(())
}