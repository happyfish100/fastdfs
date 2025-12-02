/*! FastDFS File Information Retrieval Example
 *
 * This comprehensive example demonstrates how to retrieve and work with
 * detailed file information from FastDFS storage servers. File information
 * is essential for validation, monitoring, auditing, and understanding
 * the state of files in your distributed storage system.
 *
 * The FileInfo struct provides critical metadata about files including:
 * - File size in bytes (useful for capacity planning and validation)
 * - Creation timestamp (for auditing and lifecycle management)
 * - CRC32 checksum (for data integrity verification)
 * - Source server IP address (for tracking and troubleshooting)
 *
 * Use cases for file information retrieval:
 * - Validation: Verify file size matches expected values
 * - Monitoring: Track file creation times and storage usage
 * - Auditing: Maintain records of when files were created and where
 * - Integrity checking: Use CRC32 to verify file hasn't been corrupted
 * - Troubleshooting: Identify which storage server holds a file
 *
 * Run this example with:
 * ```bash
 * cargo run --example file_info_example
 * ```
 */

/* Import the FastDFS client library components */
/* Client is the main entry point for all FastDFS operations */
/* ClientConfig allows us to configure connection parameters */
/* FileInfo contains the detailed file metadata we'll be working with */
use fastdfs::{Client, ClientConfig, FileInfo};
/* Standard library imports for error handling and time formatting */
use std::time::SystemTime;

/* Main async function that demonstrates file information operations */
/* The tokio::main macro sets up the async runtime for our application */
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    /* Print a header to make the output more readable */
    println!("FastDFS Rust Client - File Information Example");
    println!("{}", "=".repeat(60));

    /* ====================================================================
     * STEP 1: Configure the FastDFS Client
     * ====================================================================
     * Before we can retrieve file information, we need to set up a client
     * connection to the FastDFS tracker server. The tracker server acts
     * as a coordinator that knows where files are stored in the cluster.
     */
    
    /* Create a client configuration with tracker server address */
    /* Replace "192.168.1.100:22122" with your actual tracker server */
    /* The tracker server typically runs on port 22122 by default */
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        /* Set maximum number of connections per server */
        /* More connections allow better concurrency but use more resources */
        .with_max_conns(10)
        /* Connection timeout in milliseconds */
        /* How long to wait when establishing a new connection */
        .with_connect_timeout(5000)
        /* Network timeout in milliseconds */
        /* How long to wait for network operations to complete */
        .with_network_timeout(30000);

    /* ====================================================================
     * STEP 2: Create the Client Instance
     * ====================================================================
     * The client manages connection pools and handles automatic retries.
     * It's safe to use across multiple async tasks (it's thread-safe).
     */
    
    /* Initialize the FastDFS client with our configuration */
    /* This will validate the config and set up connection pools */
    let client = Client::new(config)?;
    /* Print confirmation that client was created successfully */
    println!("\n✓ Client initialized successfully");

    /* ====================================================================
     * EXAMPLE 1: Upload a File and Get Its Information
     * ====================================================================
     * First, we'll upload a test file so we have something to inspect.
     * Then we'll retrieve detailed information about that file.
     */
    
    println!("\n1. Uploading a test file...");
    /* Create some test data to upload */
    /* In a real application, this would come from a file or user input */
    let test_data = b"This is a test file for demonstrating file information retrieval. \
                      It contains sample content that we can use to verify the file info \
                      operations work correctly.";
    
    /* Upload the data to FastDFS */
    /* The upload_buffer method takes: data, file extension, and optional metadata */
    /* It returns a file_id (also called file path) that uniquely identifies the file */
    let file_id = client.upload_buffer(test_data, "txt", None).await?;
    /* Print the file ID for reference */
    /* The file ID format is: group_name/remote_filename */
    println!("   ✓ File uploaded successfully!");
    println!("   File ID: {}", file_id);

    /* ====================================================================
     * EXAMPLE 2: Retrieve Basic File Information
     * ====================================================================
     * The get_file_info method retrieves comprehensive information about
     * a file without downloading the actual file content. This is efficient
     * for validation and monitoring purposes.
     */
    
    println!("\n2. Retrieving file information...");
    /* Call get_file_info to retrieve all available file metadata */
    /* This operation queries the storage server where the file is located */
    /* It does NOT download the file content, making it very efficient */
    let file_info: FileInfo = client.get_file_info(&file_id).await?;
    
    /* Print a separator for better readability */
    println!("   File Information Details:");
    println!("   {}", "-".repeat(50));

    /* ====================================================================
     * EXAMPLE 3: Display File Size Information
     * ====================================================================
     * File size is crucial for:
     * - Validating uploads completed successfully
     * - Capacity planning and quota management
     * - Detecting truncated or corrupted uploads
     */
    
    /* Display the file size in bytes */
    /* The file_size field is a u64, so it can handle very large files */
    println!("   File Size: {} bytes", file_info.file_size);
    /* Convert to kilobytes for human readability */
    /* This helps when dealing with larger files */
    let size_kb = file_info.file_size as f64 / 1024.0;
    println!("   File Size: {:.2} KB", size_kb);
    /* Convert to megabytes if the file is large enough */
    if file_info.file_size > 1024 * 1024 {
        let size_mb = file_info.file_size as f64 / (1024.0 * 1024.0);
        println!("   File Size: {:.2} MB", size_mb);
    }
    /* Validate that the file size matches our uploaded data */
    /* This is a common validation use case */
    let expected_size = test_data.len() as u64;
    if file_info.file_size == expected_size {
        println!("   ✓ File size validation passed (matches uploaded data)");
    } else {
        println!("   ⚠ Warning: File size mismatch!");
        println!("     Expected: {} bytes", expected_size);
        println!("     Actual: {} bytes", file_info.file_size);
    }

    /* ====================================================================
     * EXAMPLE 4: Display Creation Time Information
     * ====================================================================
     * Creation time is important for:
     * - Auditing: Knowing when files were created
     * - Lifecycle management: Identifying old files for archival
     * - Debugging: Understanding the timeline of file operations
     */
    
    println!("\n   Creation Time Information:");
    /* Display the raw SystemTime value */
    /* SystemTime represents an opaque point in time */
    println!("   Create Time: {:?}", file_info.create_time);
    
    /* Convert SystemTime to a human-readable format */
    /* This makes it easier to understand when the file was created */
    match file_info.create_time.duration_since(SystemTime::UNIX_EPOCH) {
        Ok(duration) => {
            /* Calculate seconds since Unix epoch */
            let seconds = duration.as_secs();
            /* Calculate the timestamp components */
            /* This helps with formatting and understanding the time */
            let days = seconds / 86400;
            let hours = (seconds % 86400) / 3600;
            let minutes = (seconds % 3600) / 60;
            let secs = seconds % 60;
            
            /* Display in a readable format */
            println!("   Created: {} days, {} hours, {} minutes, {} seconds since epoch", 
                     days, hours, minutes, secs);
            
            /* Try to format as a standard date-time string */
            /* This requires converting to a DateTime, which we'll approximate */
            println!("   Timestamp: {} seconds since Unix epoch", seconds);
        }
        Err(e) => {
            /* Handle the case where time is before Unix epoch */
            /* This is unlikely but good to handle gracefully */
            println!("   ⚠ Could not calculate time: {:?}", e);
        }
    }
    
    /* Calculate the age of the file (time since creation) */
    /* This is useful for monitoring and lifecycle management */
    match SystemTime::now().duration_since(file_info.create_time) {
        Ok(age) => {
            /* Display how long ago the file was created */
            let age_secs = age.as_secs();
            if age_secs < 60 {
                println!("   File Age: {} seconds old", age_secs);
            } else if age_secs < 3600 {
                println!("   File Age: {} minutes old", age_secs / 60);
            } else if age_secs < 86400 {
                println!("   File Age: {} hours old", age_secs / 3600);
            } else {
                println!("   File Age: {} days old", age_secs / 86400);
            }
        }
        Err(_) => {
            /* This shouldn't happen for newly created files */
            /* But we handle it gracefully just in case */
            println!("   ⚠ Could not calculate file age");
        }
    }

    /* ====================================================================
     * EXAMPLE 5: Display CRC32 Checksum Information
     * ====================================================================
     * CRC32 is a checksum used for:
     * - Data integrity verification
     * - Detecting corruption or transmission errors
     * - Validating that files haven't been modified
     */
    
    println!("\n   CRC32 Checksum Information:");
    /* Display the CRC32 value in hexadecimal format */
    /* Hexadecimal is the standard format for displaying checksums */
    println!("   CRC32: 0x{:08X}", file_info.crc32);
    /* Also display in decimal format for reference */
    println!("   CRC32: {} (decimal)", file_info.crc32);
    
    /* Note about CRC32 usage */
    /* CRC32 is useful for quick integrity checks but not cryptographically secure */
    println!("   Note: CRC32 can be used to verify file integrity");
    println!("         Compare this value before and after operations");

    /* ====================================================================
     * EXAMPLE 6: Display Source Server Information
     * ====================================================================
     * Source server information is valuable for:
     * - Troubleshooting: Knowing which server stores the file
     * - Load balancing: Understanding file distribution
     * - Monitoring: Tracking server-specific issues
     */
    
    println!("\n   Source Server Information:");
    /* Display the IP address of the storage server */
    /* This is the server where the file is physically stored */
    println!("   Source IP Address: {}", file_info.source_ip_addr);
    
    /* Additional context about source server */
    /* This helps understand the file's location in the cluster */
    println!("   Note: This is the storage server that holds the file");
    println!("         Useful for troubleshooting and monitoring");

    /* ====================================================================
     * EXAMPLE 7: Complete FileInfo Struct Display
     * ====================================================================
     * Display the entire FileInfo struct using Debug formatting.
     * This is useful for debugging and comprehensive inspection.
     */
    
    println!("\n3. Complete FileInfo struct:");
    println!("   {:#?}", file_info);
    /* The Debug format shows all fields in a structured way */
    /* This is helpful when you need to see everything at once */

    /* ====================================================================
     * EXAMPLE 8: File Information for Validation Use Case
     * ====================================================================
     * Demonstrate how file information can be used for validation.
     * This is a common pattern in production applications.
     */
    
    println!("\n4. Validation Use Case:");
    /* Perform various validation checks using file information */
    /* These checks ensure the file meets our requirements */
    
    /* Check 1: Verify file size is within acceptable range */
    /* This prevents accidentally storing files that are too large or too small */
    let min_size: u64 = 1; /* Minimum acceptable file size in bytes */
    let max_size: u64 = 100 * 1024 * 1024; /* Maximum: 100 MB */
    if file_info.file_size >= min_size && file_info.file_size <= max_size {
        println!("   ✓ File size validation: PASSED (within acceptable range)");
    } else {
        println!("   ✗ File size validation: FAILED");
        println!("     Size: {} bytes (acceptable range: {} - {} bytes)", 
                 file_info.file_size, min_size, max_size);
    }
    
    /* Check 2: Verify file was created recently (for new uploads) */
    /* This ensures we're working with a fresh file, not an old one */
    match SystemTime::now().duration_since(file_info.create_time) {
        Ok(age) => {
            /* Consider files created within the last hour as "recent" */
            let max_age_seconds = 3600;
            if age.as_secs() < max_age_seconds {
                println!("   ✓ File age validation: PASSED (file is recent)");
            } else {
                println!("   ⚠ File age validation: WARNING (file is older than 1 hour)");
            }
        }
        Err(_) => {
            println!("   ⚠ File age validation: Could not determine age");
        }
    }
    
    /* Check 3: Verify source server is accessible */
    /* This is a basic connectivity check */
    if !file_info.source_ip_addr.is_empty() {
        println!("   ✓ Source server validation: PASSED (server IP available)");
    } else {
        println!("   ✗ Source server validation: FAILED (no server IP)");
    }

    /* ====================================================================
     * EXAMPLE 9: File Information for Monitoring Use Case
     * ====================================================================
     * Demonstrate how file information can be used for monitoring.
     * This helps track storage usage and file distribution.
     */
    
    println!("\n5. Monitoring Use Case:");
    /* Collect statistics that would be useful for monitoring */
    /* These metrics help understand storage patterns */
    
    /* Calculate storage efficiency metrics */
    /* Understanding file sizes helps with capacity planning */
    println!("   Storage Metrics:");
    println!("     - File size: {} bytes", file_info.file_size);
    println!("     - Storage efficiency: {}% of 1KB block", 
             (file_info.file_size as f64 / 1024.0 * 100.0) as u64);
    
    /* Track file creation patterns */
    /* This helps identify peak upload times */
    println!("   Creation Pattern:");
    println!("     - File created at: {:?}", file_info.create_time);
    println!("     - Source server: {}", file_info.source_ip_addr);

    /* ====================================================================
     * EXAMPLE 10: File Information for Auditing Use Case
     * ====================================================================
     * Demonstrate how file information supports auditing requirements.
     * Auditing is important for compliance and security.
     */
    
    println!("\n6. Auditing Use Case:");
    /* Create an audit log entry using file information */
    /* This demonstrates how file info supports compliance */
    println!("   Audit Log Entry:");
    println!("     Timestamp: {:?}", SystemTime::now());
    println!("     Operation: File Information Retrieval");
    println!("     File ID: {}", file_id);
    println!("     File Size: {} bytes", file_info.file_size);
    println!("     Created: {:?}", file_info.create_time);
    println!("     CRC32: 0x{:08X}", file_info.crc32);
    println!("     Source Server: {}", file_info.source_ip_addr);
    println!("     Status: Retrieved successfully");

    /* ====================================================================
     * EXAMPLE 11: Working with Multiple Files
     * ====================================================================
     * Demonstrate retrieving information for multiple files.
     * This is common in batch processing scenarios.
     */
    
    println!("\n7. Batch File Information Retrieval:");
    /* Upload a few more files to demonstrate batch operations */
    let file_ids = vec![
        client.upload_buffer(b"First batch file", "txt", None).await?,
        client.upload_buffer(b"Second batch file with more content", "txt", None).await?,
        client.upload_buffer(b"Third batch file", "txt", None).await?,
    ];
    
    println!("   Retrieved information for {} files:", file_ids.len());
    /* Iterate through each file and retrieve its information */
    /* This shows how to process multiple files efficiently */
    for (index, file_id) in file_ids.iter().enumerate() {
        /* Retrieve file info for each file */
        match client.get_file_info(file_id).await {
            Ok(info) => {
                /* Display summary information for each file */
                println!("   File {}: {} bytes, CRC32: 0x{:08X}", 
                         index + 1, info.file_size, info.crc32);
            }
            Err(e) => {
                /* Handle errors gracefully */
                println!("   File {}: Error retrieving info - {}", index + 1, e);
            }
        }
    }
    
    /* Clean up the batch files */
    /* Always clean up test files after demonstrations */
    for file_id in &file_ids {
        let _ = client.delete_file(file_id).await;
    }
    println!("   ✓ Batch files cleaned up");

    /* ====================================================================
     * EXAMPLE 12: Error Handling for File Information
     * ====================================================================
     * Demonstrate proper error handling when retrieving file information.
     * This is important for robust applications.
     */
    
    println!("\n8. Error Handling Example:");
    /* Try to get information for a non-existent file */
    /* This demonstrates how errors are handled */
    let non_existent_file = "group1/nonexistent_file.txt";
    match client.get_file_info(non_existent_file).await {
        Ok(info) => {
            /* This shouldn't happen for a non-existent file */
            println!("   ⚠ Unexpected: Retrieved info for non-existent file");
            println!("     Info: {:?}", info);
        }
        Err(e) => {
            /* This is the expected behavior */
            println!("   ✓ Correctly handled error for non-existent file");
            println!("     Error: {}", e);
        }
    }

    /* ====================================================================
     * EXAMPLE 13: FileInfo Struct Field Access
     * ====================================================================
     * Demonstrate accessing individual fields of the FileInfo struct.
     * This shows the structure and how to use each field.
     */
    
    println!("\n9. FileInfo Struct Field Access:");
    /* Re-retrieve file info to demonstrate field access */
    let file_info = client.get_file_info(&file_id).await?;
    
    /* Access file_size field */
    /* This is a u64 representing the file size in bytes */
    let size = file_info.file_size;
    println!("   file_info.file_size = {} (type: u64)", size);
    
    /* Access create_time field */
    /* This is a SystemTime representing when the file was created */
    let create_time = file_info.create_time;
    println!("   file_info.create_time = {:?} (type: SystemTime)", create_time);
    
    /* Access crc32 field */
    /* This is a u32 containing the CRC32 checksum */
    let crc32 = file_info.crc32;
    println!("   file_info.crc32 = 0x{:08X} (type: u32)", crc32);
    
    /* Access source_ip_addr field */
    /* This is a String containing the IP address of the storage server */
    let source_ip = file_info.source_ip_addr;
    println!("   file_info.source_ip_addr = \"{}\" (type: String)", source_ip);

    /* ====================================================================
     * CLEANUP: Delete Test File
     * ====================================================================
     * Always clean up test files to avoid cluttering the storage system.
     */
    
    println!("\n10. Cleaning up test file...");
    /* Delete the file we created for testing */
    client.delete_file(&file_id).await?;
    println!("   ✓ Test file deleted successfully");
    
    /* Verify the file is gone by trying to get its info */
    /* This confirms the deletion was successful */
    match client.get_file_info(&file_id).await {
        Ok(_) => {
            println!("   ⚠ Warning: File still exists after deletion");
        }
        Err(_) => {
            println!("   ✓ Confirmed: File no longer exists");
        }
    }

    /* ====================================================================
     * SUMMARY
     * ====================================================================
     * Print a summary of what we've demonstrated.
     */
    
    println!("\n{}", "=".repeat(60));
    println!("Example completed successfully!");
    println!("\nSummary of demonstrated features:");
    println!("  ✓ File information retrieval");
    println!("  ✓ File size inspection and validation");
    println!("  ✓ Creation time analysis");
    println!("  ✓ CRC32 checksum usage");
    println!("  ✓ Source server information");
    println!("  ✓ Validation use cases");
    println!("  ✓ Monitoring use cases");
    println!("  ✓ Auditing use cases");
    println!("  ✓ Batch file processing");
    println!("  ✓ Error handling");
    println!("  ✓ FileInfo struct field access");

    /* ====================================================================
     * CLOSE CLIENT
     * ====================================================================
     * Always close the client when done to release resources.
     * This closes all connections and cleans up connection pools.
     */
    
    /* Close the client and release all resources */
    /* This is important for proper resource management */
    client.close().await;
    println!("\n✓ Client closed. All resources released.");

    /* Return success */
    /* The Ok(()) indicates the program completed without errors */
    Ok(())
}

