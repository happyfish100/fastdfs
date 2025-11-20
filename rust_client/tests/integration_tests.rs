//! Integration tests for FastDFS client
//!
//! These tests require a running FastDFS cluster.
//! Set the environment variable FASTDFS_TRACKER_ADDR to run these tests.
//!
//! Example: FASTDFS_TRACKER_ADDR=192.168.1.100:22122 cargo test --test integration_tests

use fastdfs::{Client, ClientConfig, MetadataFlag};
use std::collections::HashMap;
use std::env;

/// Helper function to get tracker address from environment
///
/// This function reads the FASTDFS_TRACKER_ADDR environment variable
/// and returns it, or a default value for local testing.
fn get_tracker_addr() -> String {
    env::var("FASTDFS_TRACKER_ADDR").unwrap_or_else(|_| "127.0.0.1:22122".to_string())
}

/// Helper function to check if integration tests should run
///
/// Integration tests are only run when the FASTDFS_TRACKER_ADDR
/// environment variable is set, indicating a cluster is available.
fn should_run_integration_tests() -> bool {
    env::var("FASTDFS_TRACKER_ADDR").is_ok()
}

/// Test complete upload, download, and delete cycle
///
/// This integration test verifies the entire lifecycle of a file:
/// 1. Upload data to FastDFS
/// 2. Download the data back
/// 3. Verify the downloaded data matches the original
/// 4. Delete the file
/// 5. Verify the file no longer exists
#[tokio::test]
async fn test_upload_download_delete_cycle() {
    // Skip if no tracker address is configured
    if !should_run_integration_tests() {
        println!("Skipping integration test - set FASTDFS_TRACKER_ADDR to run");
        return;
    }

    // Arrange: Create client
    let config = ClientConfig::new(vec![get_tracker_addr()]);
    let client = Client::new(config).unwrap();

    // Arrange: Create test data
    let test_data = b"Hello, FastDFS! This is a test file.";

    // Act: Upload the file
    let file_id = client
        .upload_buffer(test_data, "txt", None)
        .await
        .expect("Upload should succeed");

    // Assert: Verify file ID is returned
    assert!(
        !file_id.is_empty(),
        "File ID should not be empty after upload"
    );
    assert!(
        file_id.contains('/'),
        "File ID should contain group separator"
    );

    // Act: Download the file
    let downloaded_data = client
        .download_file(&file_id)
        .await
        .expect("Download should succeed");

    // Assert: Verify downloaded data matches original
    assert_eq!(
        downloaded_data.as_ref(),
        test_data,
        "Downloaded data should match uploaded data"
    );

    // Act: Delete the file
    client
        .delete_file(&file_id)
        .await
        .expect("Delete should succeed");

    // Assert: Verify file no longer exists
    let exists = client.file_exists(&file_id).await;
    assert!(!exists, "File should not exist after deletion");

    // Cleanup
    client.close().await;
}

/// Test uploading file from disk
///
/// This integration test verifies that files can be uploaded directly
/// from the filesystem without loading them into memory first.
#[tokio::test]
async fn test_upload_file_from_disk() {
    // Skip if no tracker address is configured
    if !should_run_integration_tests() {
        println!("Skipping integration test - set FASTDFS_TRACKER_ADDR to run");
        return;
    }

    // Arrange: Create client
    let config = ClientConfig::new(vec![get_tracker_addr()]);
    let client = Client::new(config).unwrap();

    // Arrange: Create temporary file
    let temp_dir = std::env::temp_dir();
    let temp_file = temp_dir.join(format!("test-{}.txt", chrono::Utc::now().timestamp()));
    let test_data = b"Test file content from disk";
    std::fs::write(&temp_file, test_data).expect("Failed to write temp file");

    // Act: Upload the file
    let file_id = client
        .upload_file(temp_file.to_str().unwrap(), None)
        .await
        .expect("Upload should succeed");

    // Assert: Verify file ID is returned
    assert!(!file_id.is_empty(), "File ID should not be empty");

    // Act: Download and verify
    let downloaded_data = client
        .download_file(&file_id)
        .await
        .expect("Download should succeed");

    // Assert: Verify data matches
    assert_eq!(
        downloaded_data.as_ref(),
        test_data,
        "Downloaded data should match file content"
    );

    // Cleanup
    client.delete_file(&file_id).await.ok();
    std::fs::remove_file(&temp_file).ok();
    client.close().await;
}

/// Test downloading file to disk
///
/// This integration test verifies that files can be downloaded directly
/// to the filesystem without loading them entirely into memory.
#[tokio::test]
async fn test_download_to_file() {
    // Skip if no tracker address is configured
    if !should_run_integration_tests() {
        println!("Skipping integration test - set FASTDFS_TRACKER_ADDR to run");
        return;
    }

    // Arrange: Create client
    let config = ClientConfig::new(vec![get_tracker_addr()]);
    let client = Client::new(config).unwrap();

    // Arrange: Upload test data
    let test_data = b"Test data for download to file";
    let file_id = client
        .upload_buffer(test_data, "bin", None)
        .await
        .expect("Upload should succeed");

    // Arrange: Create temp file path
    let temp_dir = std::env::temp_dir();
    let temp_file = temp_dir.join(format!("download-{}.bin", chrono::Utc::now().timestamp()));

    // Act: Download to file
    client
        .download_to_file(&file_id, temp_file.to_str().unwrap())
        .await
        .expect("Download to file should succeed");

    // Assert: Verify file was created and contains correct data
    let downloaded_data = std::fs::read(&temp_file).expect("Failed to read downloaded file");
    assert_eq!(
        downloaded_data.as_slice(),
        test_data,
        "Downloaded file should contain correct data"
    );

    // Cleanup
    std::fs::remove_file(&temp_file).ok();
    client.delete_file(&file_id).await.ok();
    client.close().await;
}

/// Test metadata operations
///
/// This integration test verifies that metadata can be set, retrieved,
/// and updated using both overwrite and merge modes.
#[tokio::test]
async fn test_metadata_operations() {
    // Skip if no tracker address is configured
    if !should_run_integration_tests() {
        println!("Skipping integration test - set FASTDFS_TRACKER_ADDR to run");
        return;
    }

    // Arrange: Create client
    let config = ClientConfig::new(vec![get_tracker_addr()]);
    let client = Client::new(config).unwrap();

    // Arrange: Create test data with metadata
    let test_data = b"File with metadata";
    let mut metadata = HashMap::new();
    metadata.insert("author".to_string(), "Test User".to_string());
    metadata.insert("date".to_string(), "2025-01-15".to_string());
    metadata.insert("version".to_string(), "1.0".to_string());

    // Act: Upload file with metadata
    let file_id = client
        .upload_buffer(test_data, "txt", Some(&metadata))
        .await
        .expect("Upload should succeed");

    // Act: Get metadata
    let retrieved_metadata = client
        .get_metadata(&file_id)
        .await
        .expect("Get metadata should succeed");

    // Assert: Verify metadata was stored correctly
    assert_eq!(
        retrieved_metadata.len(),
        metadata.len(),
        "Retrieved metadata should have same number of entries"
    );
    for (key, value) in &metadata {
        assert_eq!(
            retrieved_metadata.get(key),
            Some(value),
            "Metadata key '{}' should have correct value",
            key
        );
    }

    // Act: Update metadata (overwrite mode)
    let mut new_metadata = HashMap::new();
    new_metadata.insert("author".to_string(), "Updated User".to_string());
    new_metadata.insert("status".to_string(), "modified".to_string());

    client
        .set_metadata(&file_id, &new_metadata, MetadataFlag::Overwrite)
        .await
        .expect("Set metadata should succeed");

    // Act: Get updated metadata
    let updated_metadata = client
        .get_metadata(&file_id)
        .await
        .expect("Get metadata should succeed");

    // Assert: Verify metadata was overwritten
    assert_eq!(
        updated_metadata.len(),
        new_metadata.len(),
        "Updated metadata should have new number of entries"
    );
    assert_eq!(
        updated_metadata.get("author"),
        Some(&"Updated User".to_string()),
        "Author should be updated"
    );
    assert_eq!(
        updated_metadata.get("status"),
        Some(&"modified".to_string()),
        "Status should be set"
    );

    // Cleanup
    client.delete_file(&file_id).await.ok();
    client.close().await;
}

/// Test getting file information
///
/// This integration test verifies that file information including size,
/// creation time, CRC32, and source IP can be correctly retrieved.
#[tokio::test]
async fn test_get_file_info() {
    // Skip if no tracker address is configured
    if !should_run_integration_tests() {
        println!("Skipping integration test - set FASTDFS_TRACKER_ADDR to run");
        return;
    }

    // Arrange: Create client
    let config = ClientConfig::new(vec![get_tracker_addr()]);
    let client = Client::new(config).unwrap();

    // Arrange: Upload test file
    let test_data = b"Test data for file info";
    let file_id = client
        .upload_buffer(test_data, "bin", None)
        .await
        .expect("Upload should succeed");

    // Act: Get file information
    let file_info = client
        .get_file_info(&file_id)
        .await
        .expect("Get file info should succeed");

    // Assert: Verify file information is correct
    assert_eq!(
        file_info.file_size,
        test_data.len() as u64,
        "File size should match uploaded data size"
    );
    assert!(
        file_info.crc32 > 0,
        "CRC32 should be calculated and non-zero"
    );
    assert!(
        !file_info.source_ip_addr.is_empty(),
        "Source IP address should be set"
    );

    // Cleanup
    client.delete_file(&file_id).await.ok();
    client.close().await;
}

/// Test file existence checking
///
/// This integration test verifies that the file_exists method correctly
/// reports whether a file exists in the storage system.
#[tokio::test]
async fn test_file_exists() {
    // Skip if no tracker address is configured
    if !should_run_integration_tests() {
        println!("Skipping integration test - set FASTDFS_TRACKER_ADDR to run");
        return;
    }

    // Arrange: Create client
    let config = ClientConfig::new(vec![get_tracker_addr()]);
    let client = Client::new(config).unwrap();

    // Arrange: Upload test file
    let test_data = b"Test existence check";
    let file_id = client
        .upload_buffer(test_data, "txt", None)
        .await
        .expect("Upload should succeed");

    // Act: Check if file exists
    let exists = client.file_exists(&file_id).await;

    // Assert: File should exist
    assert!(exists, "File should exist after upload");

    // Act: Delete the file
    client
        .delete_file(&file_id)
        .await
        .expect("Delete should succeed");

    // Act: Check existence again
    let exists_after_delete = client.file_exists(&file_id).await;

    // Assert: File should not exist after deletion
    assert!(
        !exists_after_delete,
        "File should not exist after deletion"
    );

    // Cleanup
    client.close().await;
}

/// Test downloading file range
///
/// This integration test verifies that partial file downloads work correctly,
/// allowing clients to download specific byte ranges from files.
#[tokio::test]
async fn test_download_range() {
    // Skip if no tracker address is configured
    if !should_run_integration_tests() {
        println!("Skipping integration test - set FASTDFS_TRACKER_ADDR to run");
        return;
    }

    // Arrange: Create client
    let config = ClientConfig::new(vec![get_tracker_addr()]);
    let client = Client::new(config).unwrap();

    // Arrange: Upload test file with known content
    let test_data = b"0123456789".repeat(10); // 100 bytes total
    let file_id = client
        .upload_buffer(&test_data, "bin", None)
        .await
        .expect("Upload should succeed");

    // Act: Download a specific range
    let offset = 10u64;
    let length = 20u64;
    let range_data = client
        .download_file_range(&file_id, offset, length)
        .await
        .expect("Range download should succeed");

    // Assert: Verify downloaded range is correct
    assert_eq!(
        range_data.len(),
        length as usize,
        "Downloaded range should have requested length"
    );
    assert_eq!(
        range_data.as_ref(),
        &test_data[offset as usize..(offset + length) as usize],
        "Downloaded range should match original data slice"
    );

    // Cleanup
    client.delete_file(&file_id).await.ok();
    client.close().await;
}