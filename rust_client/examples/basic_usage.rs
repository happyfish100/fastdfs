//! Basic FastDFS Client Usage Example
//!
//! This example demonstrates the fundamental operations of the FastDFS client:
//! - Uploading files from buffers
//! - Downloading files
//! - Getting file information
//! - Checking file existence
//! - Deleting files
//!
//! Run this example with:
//! ```bash
//! cargo run --example basic_usage
//! ```

use fastdfs::{Client, ClientConfig};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Basic Usage Example");
    println!("{}", "=".repeat(50));

    // Step 1: Configure the client
    // Replace with your actual tracker server address
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    // Step 2: Create the client instance
    // The client manages connection pools and handles retries automatically
    let client = Client::new(config)?;

    // Example 1: Upload from buffer
    // This demonstrates uploading data directly from memory
    println!("\n1. Uploading data from buffer...");
    let test_data = b"Hello, FastDFS! This is a test file.";
    let file_id = client.upload_buffer(test_data, "txt", None).await?;
    println!("   Uploaded successfully!");
    println!("   File ID: {}", file_id);

    // Example 2: Download file
    // This retrieves the file content back into memory
    println!("\n2. Downloading file...");
    let downloaded_data = client.download_file(&file_id).await?;
    println!("   Downloaded {} bytes", downloaded_data.len());
    println!(
        "   Content: {}",
        String::from_utf8_lossy(&downloaded_data)
    );

    // Example 3: Get file information
    // This retrieves metadata about the file without downloading it
    println!("\n3. Getting file information...");
    let file_info = client.get_file_info(&file_id).await?;
    println!("   File size: {} bytes", file_info.file_size);
    println!("   Create time: {:?}", file_info.create_time);
    println!("   CRC32: {}", file_info.crc32);
    println!("   Source IP: {}", file_info.source_ip_addr);

    // Example 4: Check if file exists
    // This is a lightweight way to verify file existence
    println!("\n4. Checking file existence...");
    let exists = client.file_exists(&file_id).await;
    println!("   File exists: {}", exists);

    // Example 5: Delete file
    // This removes the file from the storage system
    println!("\n5. Deleting file...");
    client.delete_file(&file_id).await?;
    println!("   File deleted successfully!");

    // Verify deletion
    // Confirm that the file no longer exists
    let exists = client.file_exists(&file_id).await;
    println!("   File exists after deletion: {}", exists);

    println!("\n{}", "=".repeat(50));
    println!("Example completed successfully!");

    // Step 3: Close the client
    // This releases all connections and resources
    client.close().await;
    println!("\nClient closed.");

    Ok(())
}