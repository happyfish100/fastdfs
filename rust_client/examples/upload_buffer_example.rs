/*! FastDFS Upload from Memory Buffer Example
 *
 * This comprehensive example demonstrates uploading data from memory buffers
 * to FastDFS. It covers various data types, use cases, and patterns for
 * uploading in-memory data efficiently.
 *
 * Buffer upload topics covered:
 * - Upload from memory buffers (byte arrays)
 * - Upload generated content (strings, JSON, XML)
 * - Compare buffer vs file upload approaches
 * - Use cases: API responses, generated content, in-memory data
 * - Supports all data types: text, JSON, XML, binary
 * - Include metadata for better organization
 *
 * Understanding buffer uploads is crucial for:
 * - Uploading data generated in memory (no file system needed)
 * - Storing API responses and generated content
 * - Efficient handling of in-memory data structures
 * - Avoiding temporary file creation
 * - Direct upload from application memory
 * - Working with various data formats
 *
 * Run this example with:
 * ```bash
 * cargo run --example upload_buffer_example
 * ```
 */

/* Import FastDFS client components */
/* Client provides the main API for FastDFS operations */
/* ClientConfig allows configuration of connection parameters */
use fastdfs::{Client, ClientConfig, Metadata};
/* Import standard library for collections and string handling */
use std::collections::HashMap;
/* Import standard library for I/O operations */
/* For comparing buffer uploads with file uploads */
use std::io::Write;
/* Import Tokio for async runtime */
use tokio::fs;

/* ====================================================================
 * HELPER FUNCTIONS FOR DATA GENERATION
 * ====================================================================
 * Utility functions for generating various types of content.
 */

/* Generate JSON content */
/* Creates a JSON string from structured data */
fn generate_json_content() -> String {
    /* Create a JSON object */
    /* This simulates generating JSON from application data */
    format!(r#"{{
  "id": 12345,
  "name": "Example Document",
  "type": "json",
  "timestamp": "2025-01-15T10:30:00Z",
  "data": {{
    "field1": "value1",
    "field2": 42,
    "field3": true
  }},
  "tags": ["example", "json", "test"]
}}"#)
}

/* Generate XML content */
/* Creates an XML string from structured data */
fn generate_xml_content() -> String {
    /* Create an XML document */
    /* This simulates generating XML from application data */
    format!(r#"<?xml version="1.0" encoding="UTF-8"?>
<document>
  <id>12345</id>
  <name>Example Document</name>
  <type>xml</type>
  <timestamp>2025-01-15T10:30:00Z</timestamp>
  <data>
    <field1>value1</field1>
    <field2>42</field2>
    <field3>true</field3>
  </data>
  <tags>
    <tag>example</tag>
    <tag>xml</tag>
    <tag>test</tag>
  </tags>
</document>"#)
}

/* Generate CSV content */
/* Creates a CSV string from structured data */
fn generate_csv_content() -> String {
    /* Create CSV data */
    /* This simulates generating CSV from application data */
    let mut csv = String::from("id,name,type,value,active\n");
    for i in 1..=10 {
        csv.push_str(&format!("{},{},type{},{},{}\n", 
            i, 
            format!("Item{}", i),
            i % 3,
            i * 10,
            i % 2 == 0
        ));
    }
    csv
}

/* Generate binary content */
/* Creates binary data (simulated) */
fn generate_binary_content(size: usize) -> Vec<u8> {
    /* Generate binary data */
    /* This simulates binary data like images, PDFs, etc. */
    (0..size).map(|i| (i % 256) as u8).collect()
}

/* Generate text content */
/* Creates plain text content */
fn generate_text_content() -> String {
    /* Create text content */
    /* This simulates generating text from application logic */
    format!(r#"FastDFS Buffer Upload Example

This document demonstrates uploading content from memory buffers.
The content can be generated dynamically without needing to write
to the file system first.

Key Benefits:
- No temporary files needed
- Direct upload from memory
- Efficient for generated content
- Supports all data types

Generated at: 2025-01-15 10:30:00
Type: Text Document
Status: Active
"#)
}

/* ====================================================================
 * MAIN EXAMPLE FUNCTION
 * ====================================================================
 * Demonstrates all buffer upload patterns and techniques.
 */

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    /* Print header for better output readability */
    println!("FastDFS Rust Client - Upload from Memory Buffer Example");
    println!("{}", "=".repeat(70));

    /* ====================================================================
     * STEP 1: Initialize Client
     * ====================================================================
     * Set up the FastDFS client for buffer upload demonstrations.
     */
    
    println!("\n1. Initializing FastDFS Client...");
    /* Configure client with appropriate settings */
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    /* Create the client instance */
    let client = Client::new(config)?;
    println!("   ✓ Client initialized successfully");

    /* ====================================================================
     * EXAMPLE 1: Basic Buffer Upload
     * ====================================================================
     * Demonstrate the simplest form of buffer upload.
     */
    
    println!("\n2. Basic Buffer Upload...");
    
    println!("\n   Example 1.1: Upload from byte array");
    /* Create a simple byte array */
    /* This is the most basic form of buffer upload */
    let simple_data = b"Hello, FastDFS! This is uploaded from a byte array.";
    let file_id = client.upload_buffer(simple_data, "txt", None).await?;
    println!("     ✓ Uploaded {} bytes", simple_data.len());
    println!("     File ID: {}", file_id);
    
    /* Verify the upload by downloading */
    let downloaded = client.download_file(&file_id).await?;
    println!("     ✓ Verified: Downloaded {} bytes", downloaded.len());
    
    /* Clean up */
    client.delete_file(&file_id).await?;
    println!("     ✓ Test file cleaned up");
    
    println!("\n   Example 1.2: Upload from Vec<u8>");
    /* Create data in a Vec<u8> */
    /* Vec<u8> is commonly used for binary data */
    let vec_data: Vec<u8> = (0..1000).map(|i| (i % 256) as u8).collect();
    let vec_file_id = client.upload_buffer(&vec_data, "bin", None).await?;
    println!("     ✓ Uploaded {} bytes from Vec<u8>", vec_data.len());
    println!("     File ID: {}", vec_file_id);
    
    /* Clean up */
    client.delete_file(&vec_file_id).await?;

    /* ====================================================================
     * EXAMPLE 2: Upload Text Content
     * ====================================================================
     * Demonstrate uploading text/string content.
     */
    
    println!("\n3. Upload Text Content...");
    
    println!("\n   Example 2.1: Upload plain text string");
    /* Convert string to bytes */
    /* Strings need to be converted to &[u8] for upload */
    let text_content = "This is plain text content that will be uploaded.";
    let text_bytes = text_content.as_bytes();
    let text_file_id = client.upload_buffer(text_bytes, "txt", None).await?;
    println!("     ✓ Uploaded text: {} bytes", text_bytes.len());
    println!("     File ID: {}", text_file_id);
    
    /* Verify by downloading and converting back to string */
    let downloaded_text = client.download_file(&text_file_id).await?;
    let text_string = String::from_utf8_lossy(&downloaded_text);
    println!("     ✓ Verified content: {}", text_string);
    
    /* Clean up */
    client.delete_file(&text_file_id).await?;
    
    println!("\n   Example 2.2: Upload generated text content");
    /* Generate text content dynamically */
    /* This simulates generating content in your application */
    let generated_text = generate_text_content();
    let generated_bytes = generated_text.as_bytes();
    let generated_file_id = client.upload_buffer(generated_bytes, "txt", None).await?;
    println!("     ✓ Uploaded generated text: {} bytes", generated_bytes.len());
    println!("     File ID: {}", generated_file_id);
    
    /* Clean up */
    client.delete_file(&generated_file_id).await?;
    
    println!("\n   Example 2.3: Upload multi-line text");
    /* Create multi-line text content */
    /* Demonstrates handling complex text structures */
    let multiline_text = format!(r#"Line 1: First line of content
Line 2: Second line with some data
Line 3: Third line with numbers: 12345
Line 4: Fourth line with special chars: !@#$%^&*()
Line 5: Final line of the document"#);
    let multiline_bytes = multiline_text.as_bytes();
    let multiline_file_id = client.upload_buffer(multiline_bytes, "txt", None).await?;
    println!("     ✓ Uploaded multi-line text: {} bytes", multiline_bytes.len());
    println!("     File ID: {}", multiline_file_id);
    
    /* Clean up */
    client.delete_file(&multiline_file_id).await?;

    /* ====================================================================
     * EXAMPLE 3: Upload JSON Content
     * ====================================================================
     * Demonstrate uploading JSON data.
     */
    
    println!("\n4. Upload JSON Content...");
    
    println!("\n   Example 3.1: Upload JSON string");
    /* Generate JSON content */
    /* This simulates API responses or generated JSON */
    let json_content = generate_json_content();
    let json_bytes = json_content.as_bytes();
    let json_file_id = client.upload_buffer(json_bytes, "json", None).await?;
    println!("     ✓ Uploaded JSON: {} bytes", json_bytes.len());
    println!("     File ID: {}", json_file_id);
    
    /* Verify JSON content */
    let downloaded_json = client.download_file(&json_file_id).await?;
    let json_string = String::from_utf8_lossy(&downloaded_json);
    println!("     ✓ Verified JSON content (first 100 chars): {}", 
             &json_string.chars().take(100).collect::<String>());
    
    /* Clean up */
    client.delete_file(&json_file_id).await?;
    
    println!("\n   Example 3.2: Upload JSON with metadata");
    /* Create metadata for JSON file */
    /* Metadata helps organize and search files */
    let mut json_metadata = HashMap::new();
    json_metadata.insert("content_type".to_string(), "application/json".to_string());
    json_metadata.insert("source".to_string(), "api_response".to_string());
    json_metadata.insert("version".to_string(), "1.0".to_string());
    
    /* Upload JSON with metadata */
    let json_with_meta = generate_json_content();
    let json_with_meta_bytes = json_with_meta.as_bytes();
    let json_meta_file_id = client.upload_buffer(
        json_with_meta_bytes, 
        "json", 
        Some(&json_metadata)
    ).await?;
    println!("     ✓ Uploaded JSON with metadata: {} bytes", json_with_meta_bytes.len());
    println!("     File ID: {}", json_meta_file_id);
    
    /* Verify metadata */
    let retrieved_metadata = client.get_metadata(&json_meta_file_id).await?;
    println!("     ✓ Metadata:");
    for (key, value) in &retrieved_metadata {
        println!("       {}: {}", key, value);
    }
    
    /* Clean up */
    client.delete_file(&json_meta_file_id).await?;

    /* ====================================================================
     * EXAMPLE 4: Upload XML Content
     * ====================================================================
     * Demonstrate uploading XML data.
     */
    
    println!("\n5. Upload XML Content...");
    
    println!("\n   Example 4.1: Upload XML string");
    /* Generate XML content */
    /* This simulates XML documents or SOAP messages */
    let xml_content = generate_xml_content();
    let xml_bytes = xml_content.as_bytes();
    let xml_file_id = client.upload_buffer(xml_bytes, "xml", None).await?;
    println!("     ✓ Uploaded XML: {} bytes", xml_bytes.len());
    println!("     File ID: {}", xml_file_id);
    
    /* Verify XML content */
    let downloaded_xml = client.download_file(&xml_file_id).await?;
    let xml_string = String::from_utf8_lossy(&downloaded_xml);
    println!("     ✓ Verified XML content (first 100 chars): {}", 
             &xml_string.chars().take(100).collect::<String>());
    
    /* Clean up */
    client.delete_file(&xml_file_id).await?;
    
    println!("\n   Example 4.2: Upload XML with metadata");
    /* Create metadata for XML file */
    let mut xml_metadata = HashMap::new();
    xml_metadata.insert("content_type".to_string(), "application/xml".to_string());
    xml_metadata.insert("encoding".to_string(), "UTF-8".to_string());
    xml_metadata.insert("source".to_string(), "document_generator".to_string());
    
    /* Upload XML with metadata */
    let xml_with_meta = generate_xml_content();
    let xml_with_meta_bytes = xml_with_meta.as_bytes();
    let xml_meta_file_id = client.upload_buffer(
        xml_with_meta_bytes,
        "xml",
        Some(&xml_metadata)
    ).await?;
    println!("     ✓ Uploaded XML with metadata: {} bytes", xml_with_meta_bytes.len());
    println!("     File ID: {}", xml_meta_file_id);
    
    /* Clean up */
    client.delete_file(&xml_meta_file_id).await?;

    /* ====================================================================
     * EXAMPLE 5: Upload CSV Content
     * ====================================================================
     * Demonstrate uploading CSV data.
     */
    
    println!("\n6. Upload CSV Content...");
    
    println!("\n   Example 5.1: Upload CSV string");
    /* Generate CSV content */
    /* This simulates exporting data to CSV format */
    let csv_content = generate_csv_content();
    let csv_bytes = csv_content.as_bytes();
    let csv_file_id = client.upload_buffer(csv_bytes, "csv", None).await?;
    println!("     ✓ Uploaded CSV: {} bytes", csv_bytes.len());
    println!("     File ID: {}", csv_file_id);
    
    /* Verify CSV content */
    let downloaded_csv = client.download_file(&csv_file_id).await?;
    let csv_string = String::from_utf8_lossy(&downloaded_csv);
    println!("     ✓ Verified CSV content (first 200 chars):");
    println!("       {}", csv_string.lines().take(3).collect::<Vec<_>>().join("\n       "));
    
    /* Clean up */
    client.delete_file(&csv_file_id).await?;

    /* ====================================================================
     * EXAMPLE 6: Upload Binary Content
     * ====================================================================
     * Demonstrate uploading binary data.
     */
    
    println!("\n7. Upload Binary Content...");
    
    println!("\n   Example 6.1: Upload small binary data");
    /* Generate small binary content */
    /* This simulates small binary files like icons */
    let small_binary = generate_binary_content(1024); /* 1 KB */
    let small_binary_file_id = client.upload_buffer(&small_binary, "bin", None).await?;
    println!("     ✓ Uploaded small binary: {} bytes", small_binary.len());
    println!("     File ID: {}", small_binary_file_id);
    
    /* Clean up */
    client.delete_file(&small_binary_file_id).await?;
    
    println!("\n   Example 6.2: Upload medium binary data");
    /* Generate medium binary content */
    /* This simulates medium binary files like images */
    let medium_binary = generate_binary_content(100 * 1024); /* 100 KB */
    let medium_binary_file_id = client.upload_buffer(&medium_binary, "bin", None).await?;
    println!("     ✓ Uploaded medium binary: {} bytes", medium_binary.len());
    println!("     File ID: {}", medium_binary_file_id);
    
    /* Clean up */
    client.delete_file(&medium_binary_file_id).await?;
    
    println!("\n   Example 6.3: Upload large binary data");
    /* Generate large binary content */
    /* This simulates large binary files like videos or archives */
    println!("     Generating large binary data (1 MB)...");
    let large_binary = generate_binary_content(1024 * 1024); /* 1 MB */
    let large_binary_file_id = client.upload_buffer(&large_binary, "bin", None).await?;
    println!("     ✓ Uploaded large binary: {} bytes", large_binary.len());
    println!("     File ID: {}", large_binary_file_id);
    
    /* Clean up */
    client.delete_file(&large_binary_file_id).await?;

    /* ====================================================================
     * EXAMPLE 7: Upload with Metadata
     * ====================================================================
     * Demonstrate uploading buffers with metadata for organization.
     */
    
    println!("\n8. Upload with Metadata...");
    
    println!("\n   Example 7.1: Upload with descriptive metadata");
    /* Create comprehensive metadata */
    /* Metadata helps organize and search files */
    let mut comprehensive_metadata = HashMap::new();
    comprehensive_metadata.insert("title".to_string(), "Example Document".to_string());
    comprehensive_metadata.insert("author".to_string(), "System".to_string());
    comprehensive_metadata.insert("content_type".to_string(), "text/plain".to_string());
    comprehensive_metadata.insert("source".to_string(), "buffer_upload".to_string());
    comprehensive_metadata.insert("created_at".to_string(), "2025-01-15T10:30:00Z".to_string());
    
    /* Upload with metadata */
    let metadata_content = "This is content uploaded with comprehensive metadata.";
    let metadata_bytes = metadata_content.as_bytes();
    let metadata_file_id = client.upload_buffer(
        metadata_bytes,
        "txt",
        Some(&comprehensive_metadata)
    ).await?;
    println!("     ✓ Uploaded with metadata: {} bytes", metadata_bytes.len());
    println!("     File ID: {}", metadata_file_id);
    
    /* Retrieve and display metadata */
    let retrieved_meta = client.get_metadata(&metadata_file_id).await?;
    println!("     ✓ Retrieved metadata:");
    for (key, value) in &retrieved_meta {
        println!("       {}: {}", key, value);
    }
    
    /* Clean up */
    client.delete_file(&metadata_file_id).await?;
    
    println!("\n   Example 7.2: Upload API response with metadata");
    /* Simulate uploading an API response */
    /* This is a common use case for buffer uploads */
    let api_response = generate_json_content();
    let mut api_metadata = HashMap::new();
    api_metadata.insert("content_type".to_string(), "application/json".to_string());
    api_metadata.insert("source".to_string(), "api_endpoint".to_string());
    api_metadata.insert("endpoint".to_string(), "/api/v1/data".to_string());
    api_metadata.insert("status".to_string(), "success".to_string());
    
    let api_bytes = api_response.as_bytes();
    let api_file_id = client.upload_buffer(
        api_bytes,
        "json",
        Some(&api_metadata)
    ).await?;
    println!("     ✓ Uploaded API response: {} bytes", api_bytes.len());
    println!("     File ID: {}", api_file_id);
    
    /* Clean up */
    client.delete_file(&api_file_id).await?;

    /* ====================================================================
     * EXAMPLE 8: Compare Buffer vs File Upload
     * ====================================================================
     * Demonstrate the differences and use cases for each approach.
     */
    
    println!("\n9. Compare Buffer vs File Upload...");
    
    println!("\n   Example 8.1: Buffer upload (no file system)");
    /* Upload directly from memory */
    /* No temporary file needed */
    let buffer_content = "Content uploaded directly from memory buffer.";
    let buffer_start = std::time::Instant::now();
    let buffer_file_id = client.upload_buffer(buffer_content.as_bytes(), "txt", None).await?;
    let buffer_duration = buffer_start.elapsed();
    println!("     ✓ Buffer upload completed in {:?}", buffer_duration);
    println!("     File ID: {}", buffer_file_id);
    println!("     Advantages:");
    println!("       - No temporary file creation");
    println!("       - Faster for in-memory data");
    println!("       - No file system I/O overhead");
    println!("       - Better for generated content");
    
    /* Clean up */
    client.delete_file(&buffer_file_id).await?;
    
    println!("\n   Example 8.2: File upload (requires file system)");
    /* Create a temporary file and upload it */
    /* This demonstrates the file upload approach */
    let file_content = "Content uploaded from a file.";
    let temp_file = "temp_upload_test.txt";
    
    /* Write content to temporary file */
    let file_start = std::time::Instant::now();
    let mut file = std::fs::File::create(temp_file)?;
    file.write_all(file_content.as_bytes())?;
    file.sync_all()?;
    drop(file); /* Close file */
    
    /* Upload from file */
    /* Note: This uses upload_file which reads from disk */
    /* For comparison, we'll simulate by reading and uploading as buffer */
    let file_data = std::fs::read(temp_file)?;
    let file_file_id = client.upload_buffer(&file_data, "txt", None).await?;
    let file_duration = file_start.elapsed();
    
    /* Clean up temporary file */
    std::fs::remove_file(temp_file)?;
    
    println!("     ✓ File upload completed in {:?}", file_duration);
    println!("     File ID: {}", file_file_id);
    println!("     When to use:");
    println!("       - Data already exists in file system");
    println!("       - Need to preserve original file");
    println!("       - Working with existing files");
    
    /* Clean up */
    client.delete_file(&file_file_id).await?;
    
    println!("\n   Comparison Summary:");
    println!("     Buffer Upload:");
    println!("       ✓ Best for: Generated content, API responses, in-memory data");
    println!("       ✓ Advantages: No file I/O, faster, no temp files");
    println!("       ✗ Limitations: Requires data in memory");
    println!("     File Upload:");
    println!("       ✓ Best for: Existing files, large files from disk");
    println!("       ✓ Advantages: Can handle very large files, preserves originals");
    println!("       ✗ Limitations: Requires file I/O, temporary files");

    /* ====================================================================
     * EXAMPLE 9: Use Cases for Buffer Uploads
     * ====================================================================
     * Demonstrate real-world use cases for buffer uploads.
     */
    
    println!("\n10. Use Cases for Buffer Uploads...");
    
    println!("\n   Use Case 1: API Response Storage");
    /* Store API responses directly */
    /* Common pattern: receive API response, store immediately */
    println!("     Scenario: Storing API response without writing to disk");
    let api_response_data = generate_json_content();
    let api_response_id = client.upload_buffer(
        api_response_data.as_bytes(),
        "json",
        None
    ).await?;
    println!("     ✓ API response stored: {}", api_response_id);
    client.delete_file(&api_response_id).await?;
    
    println!("\n   Use Case 2: Generated Content Storage");
    /* Store dynamically generated content */
    /* Common pattern: generate content, store immediately */
    println!("     Scenario: Storing generated report without temp file");
    let generated_report = generate_csv_content();
    let report_id = client.upload_buffer(
        generated_report.as_bytes(),
        "csv",
        None
    ).await?;
    println!("     ✓ Generated report stored: {}", report_id);
    client.delete_file(&report_id).await?;
    
    println!("\n   Use Case 3: In-Memory Data Processing");
    /* Process and store data without file I/O */
    /* Common pattern: process data in memory, store result */
    println!("     Scenario: Processing data and storing result");
    let processed_data = "Processed data result from in-memory operations.";
    let processed_id = client.upload_buffer(
        processed_data.as_bytes(),
        "txt",
        None
    ).await?;
    println!("     ✓ Processed data stored: {}", processed_id);
    client.delete_file(&processed_id).await?;
    
    println!("\n   Use Case 4: Multi-Format Content Storage");
    /* Store different formats from same source */
    /* Common pattern: convert data to multiple formats */
    println!("     Scenario: Storing same data in multiple formats");
    let base_data = r#"{"key": "value", "number": 42}"#;
    
    /* Store as JSON */
    let json_id = client.upload_buffer(base_data.as_bytes(), "json", None).await?;
    println!("     ✓ Stored as JSON: {}", json_id);
    
    /* Store as XML (converted) */
    let xml_data = format!(r#"<data><key>value</key><number>42</number></data>"#);
    let xml_id = client.upload_buffer(xml_data.as_bytes(), "xml", None).await?;
    println!("     ✓ Stored as XML: {}", xml_id);
    
    /* Store as text */
    let text_data = "key=value, number=42";
    let text_id = client.upload_buffer(text_data.as_bytes(), "txt", None).await?;
    println!("     ✓ Stored as text: {}", text_id);
    
    /* Clean up */
    client.delete_file(&json_id).await?;
    client.delete_file(&xml_id).await?;
    client.delete_file(&text_id).await?;

    /* ====================================================================
     * EXAMPLE 10: Data Type Support
     * ====================================================================
     * Demonstrate support for all data types.
     */
    
    println!("\n11. Data Type Support...");
    
    println!("\n   Supported Data Types:");
    println!("     ✓ Text: Plain text, markdown, code");
    println!("     ✓ JSON: API responses, configuration, data exchange");
    println!("     ✓ XML: Documents, SOAP messages, data structures");
    println!("     ✓ CSV: Tabular data, exports, reports");
    println!("     ✓ Binary: Images, PDFs, archives, executables");
    println!("     ✓ Custom: Any byte array can be uploaded");
    
    println!("\n   Example: Upload various data types");
    /* Demonstrate uploading different types */
    let types = vec![
        ("text", generate_text_content().as_bytes()),
        ("json", generate_json_content().as_bytes()),
        ("xml", generate_xml_content().as_bytes()),
        ("csv", generate_csv_content().as_bytes()),
        ("binary", &generate_binary_content(1024)),
    ];
    
    let mut uploaded_files = Vec::new();
    for (data_type, data) in types {
        let file_id = client.upload_buffer(data, data_type, None).await?;
        println!("     ✓ Uploaded {}: {} bytes -> {}", 
                 data_type, data.len(), file_id);
        uploaded_files.push(file_id);
    }
    
    /* Clean up */
    for file_id in uploaded_files {
        client.delete_file(&file_id).await?;
    }

    /* ====================================================================
     * EXAMPLE 11: Best Practices
     * ====================================================================
     * Learn best practices for buffer uploads.
     */
    
    println!("\n12. Buffer Upload Best Practices...");
    
    println!("\n   Best Practice 1: Choose appropriate file extensions");
    println!("     ✓ Use meaningful extensions: .txt, .json, .xml, .csv, .bin");
    println!("     ✓ Extensions help identify file types");
    println!("     ✗ Generic extensions like .dat, .tmp");
    
    println!("\n   Best Practice 2: Add metadata for organization");
    println!("     ✓ Include content_type, source, timestamp");
    println!("     ✓ Metadata enables better search and organization");
    println!("     ✗ Uploading without metadata");
    
    println!("\n   Best Practice 3: Use buffer uploads for generated content");
    println!("     ✓ API responses, reports, dynamically generated data");
    println!("     ✓ Avoids temporary file creation");
    println!("     ✗ Writing to disk just to upload");
    
    println!("\n   Best Practice 4: Consider memory usage for large data");
    println!("     ✓ For very large data, consider chunked uploads");
    println!("     ✓ Monitor memory usage");
    println!("     ✗ Loading entire large files into memory");
    
    println!("\n   Best Practice 5: Validate data before uploading");
    println!("     ✓ Check data size and format");
    println!("     ✓ Ensure data is valid before upload");
    println!("     ✗ Uploading invalid or corrupted data");
    
    println!("\n   Best Practice 6: Handle errors gracefully");
    println!("     ✓ Check upload results");
    println!("     ✓ Clean up on errors");
    println!("     ✗ Ignoring upload errors");

    /* ====================================================================
     * SUMMARY
     * ====================================================================
     * Print summary of buffer upload concepts demonstrated.
     */
    
    println!("\n{}", "=".repeat(70));
    println!("Upload from Memory Buffer Example Completed Successfully!");
    println!("\nSummary of demonstrated features:");
    println!("  ✓ Basic buffer upload from byte arrays");
    println!("  ✓ Upload text content (plain text, multi-line)");
    println!("  ✓ Upload JSON content with and without metadata");
    println!("  ✓ Upload XML content with and without metadata");
    println!("  ✓ Upload CSV content");
    println!("  ✓ Upload binary content (small, medium, large)");
    println!("  ✓ Upload with metadata for organization");
    println!("  ✓ Comparison of buffer vs file upload approaches");
    println!("  ✓ Real-world use cases (API responses, generated content)");
    println!("  ✓ Support for all data types");
    println!("  ✓ Best practices for buffer uploads");
    println!("\nAll buffer upload concepts demonstrated with extensive comments.");

    /* Close the client to release resources */
    client.close().await;
    println!("\n✓ Client closed. All resources released.");

    /* Return success */
    Ok(())
}

