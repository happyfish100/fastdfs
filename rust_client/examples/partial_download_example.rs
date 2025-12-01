//! FastDFS Partial Download Example
//!
//! This example demonstrates partial file download capabilities with the FastDFS client.
//! It covers downloading specific byte ranges, resuming interrupted downloads,
//! extracting portions of files, and memory-efficient download patterns.
//!
//! Key Topics Covered:
//! - Download specific byte ranges
//! - Resume interrupted downloads
//! - Extract portions of files
//! - Streaming large files
//! - Memory-efficient downloads
//! - Range request patterns
//! - Chunked downloads
//!
//! Run this example with:
//! ```bash
//! cargo run --example partial_download_example
//! ```

use fastdfs::{Client, ClientConfig};
use std::time::{Duration, Instant};
use tokio::time::sleep;

/// Main entry point for the partial download example
/// 
/// This function demonstrates various patterns for downloading portions of files
/// from FastDFS, including range requests, resuming downloads, and chunked processing.
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Partial Download Example");
    println!("{}", "=".repeat(50));
    println!();

    /// Step 1: Configure and Create Client
    /// 
    /// The client configuration determines connection behavior and timeouts.
    /// For partial downloads, we may want longer network timeouts for large files.
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);

    let client = Client::new(config)?;

    /// Step 2: Prepare Test File
    /// 
    /// We'll upload a test file with known content to demonstrate partial downloads.
    /// The file will contain sequential data that makes it easy to verify ranges.
    println!("Preparing test file for partial download examples...");
    println!();

    /// Create test data with sequential bytes
    /// 
    /// This makes it easy to verify that downloaded ranges are correct.
    /// Each byte represents its position in the file (modulo 256).
    let test_data: Vec<u8> = (0..1000)
        .map(|i| (i % 256) as u8)
        .collect();

    let file_id = match client.upload_buffer(&test_data, "bin", None).await {
        Ok(id) => {
            println!("✓ Test file uploaded: {}", id);
            println!("  File size: {} bytes", test_data.len());
            id
        }
        Err(e) => {
            println!("✗ Failed to upload test file: {}", e);
            return Ok(());
        }
    };

    println!();

    /// Example 1: Download Specific Byte Ranges
    /// 
    /// This example demonstrates how to download specific byte ranges from a file.
    /// Range requests are useful when you only need a portion of a file, such as
    /// reading a specific section of a log file or extracting metadata from a file header.
    println!("\n1. Download Specific Byte Ranges");
    println!("-------------------------------");
    println!();
    println!("   Range requests allow downloading only the bytes you need.");
    println!("   This is efficient for large files when you only need a portion.");
    println!();

    /// Range 1: Download from the beginning
    /// 
    /// Download the first 100 bytes of the file.
    /// This is useful for reading file headers or metadata.
    println!("   Range 1: First 100 bytes (header/metadata)");
    println!("   → Offset: 0, Length: 100");
    println!();

    let range1_start = Instant::now();
    match client.download_file_range(&file_id, 0, 100).await {
        Ok(data) => {
            let range1_elapsed = range1_start.elapsed();
            println!("   ✓ Downloaded {} bytes in {:?}", data.len(), range1_elapsed);
            println!("   → First 10 bytes: {:?}", &data[..10.min(data.len())]);
            println!("   → Use case: Reading file headers or metadata");
        }
        Err(e) => {
            println!("   ✗ Download failed: {}", e);
        }
    }

    println!();

    /// Range 2: Download from the middle
    /// 
    /// Download bytes from the middle of the file.
    /// This is useful for accessing specific sections of a file.
    println!("   Range 2: Middle section (bytes 400-500)");
    println!("   → Offset: 400, Length: 100");
    println!();

    let range2_start = Instant::now();
    match client.download_file_range(&file_id, 400, 100).await {
        Ok(data) => {
            let range2_elapsed = range2_start.elapsed();
            println!("   ✓ Downloaded {} bytes in {:?}", data.len(), range2_elapsed);
            println!("   → First 10 bytes: {:?}", &data[..10.min(data.len())]);
            println!("   → Use case: Accessing specific file sections");
        }
        Err(e) => {
            println!("   ✗ Download failed: {}", e);
        }
    }

    println!();

    /// Range 3: Download from the end
    /// 
    /// Download the last portion of the file.
    /// This is useful for reading file trailers or the most recent data.
    println!("   Range 3: Last 100 bytes (trailer/recent data)");
    println!("   → Offset: 900, Length: 100");
    println!();

    let range3_start = Instant::now();
    match client.download_file_range(&file_id, 900, 100).await {
        Ok(data) => {
            let range3_elapsed = range3_start.elapsed();
            println!("   ✓ Downloaded {} bytes in {:?}", data.len(), range3_elapsed);
            println!("   → First 10 bytes: {:?}", &data[..10.min(data.len())]);
            println!("   → Use case: Reading file trailers or recent data");
        }
        Err(e) => {
            println!("   ✗ Download failed: {}", e);
        }
    }

    println!();

    /// Range 4: Download a single byte
    /// 
    /// Download just one byte to demonstrate minimal range requests.
    /// This is useful for checking file existence or reading a single value.
    println!("   Range 4: Single byte (byte at offset 500)");
    println!("   → Offset: 500, Length: 1");
    println!();

    match client.download_file_range(&file_id, 500, 1).await {
        Ok(data) => {
            println!("   ✓ Downloaded {} byte", data.len());
            println!("   → Value: {}", data[0]);
            println!("   → Use case: Reading a single value or checking file");
        }
        Err(e) => {
            println!("   ✗ Download failed: {}", e);
        }
    }

    println!();

    /// Example 2: Resume Interrupted Downloads
    /// 
    /// This example demonstrates how to resume a download that was interrupted.
    /// This is useful for large files where network issues or timeouts may occur.
    /// The pattern involves tracking downloaded bytes and continuing from where it stopped.
    println!("\n2. Resume Interrupted Downloads");
    println!("-------------------------------");
    println!();
    println!("   Resuming downloads allows continuing from where a download stopped.");
    println!("   This is essential for large files and unreliable networks.");
    println!();

    /// Simulate an interrupted download scenario
    /// 
    /// We'll simulate downloading a file in chunks, where one chunk fails.
    /// Then we'll resume from the last successfully downloaded byte.
    let total_file_size = test_data.len() as u64;
    let chunk_size = 200u64;
    let mut downloaded_bytes = 0u64;
    let mut downloaded_data = Vec::new();

    println!("   Simulating interrupted download...");
    println!("   → Total file size: {} bytes", total_file_size);
    println!("   → Chunk size: {} bytes", chunk_size);
    println!();

    /// Download chunks until we simulate an interruption
    /// 
    /// In a real scenario, this might be due to network timeout, connection loss, etc.
    loop {
        let remaining = total_file_size - downloaded_bytes;
        if remaining == 0 {
            break;
        }

        let current_chunk_size = chunk_size.min(remaining);
        println!("   → Downloading chunk: offset {}, length {}", 
                downloaded_bytes, current_chunk_size);

        match client.download_file_range(&file_id, downloaded_bytes, current_chunk_size).await {
            Ok(chunk_data) => {
                downloaded_bytes += chunk_data.len() as u64;
                downloaded_data.extend_from_slice(&chunk_data);
                println!("     ✓ Chunk downloaded: {} bytes (total: {}/{})", 
                        chunk_data.len(), downloaded_bytes, total_file_size);

                /// Simulate interruption after 3 chunks
                /// 
                /// In a real scenario, this would be an actual network error.
                /// For demonstration, we'll simulate it after downloading 3 chunks.
                if downloaded_bytes >= 3 * chunk_size {
                    println!("     → Simulating download interruption...");
                    println!("     → Download stopped at byte {}", downloaded_bytes);
                    break;
                }
            }
            Err(e) => {
                println!("     ✗ Chunk download failed: {}", e);
                println!("     → Download interrupted, will resume from byte {}", downloaded_bytes);
                break;
            }
        }
    }

    println!();
    println!("   Resuming download from byte {}...", downloaded_bytes);
    println!();

    /// Resume the download from where it stopped
    /// 
    /// Continue downloading the remaining bytes of the file.
    while downloaded_bytes < total_file_size {
        let remaining = total_file_size - downloaded_bytes;
        let current_chunk_size = chunk_size.min(remaining);

        println!("   → Resuming: offset {}, length {}", 
                downloaded_bytes, current_chunk_size);

        match client.download_file_range(&file_id, downloaded_bytes, current_chunk_size).await {
            Ok(chunk_data) => {
                downloaded_bytes += chunk_data.len() as u64;
                downloaded_data.extend_from_slice(&chunk_data);
                println!("     ✓ Chunk downloaded: {} bytes (total: {}/{})", 
                        chunk_data.len(), downloaded_bytes, total_file_size);
            }
            Err(e) => {
                println!("     ✗ Chunk download failed: {}", e);
                println!("     → Can retry from byte {}", downloaded_bytes);
                break;
            }
        }
    }

    println!();
    if downloaded_bytes == total_file_size {
        println!("   ✓ Download completed successfully!");
        println!("   → Total bytes downloaded: {}", downloaded_data.len());
        println!("   → File integrity: {}", 
                if downloaded_data == test_data { "Verified" } else { "Mismatch" });
    } else {
        println!("   ✗ Download incomplete: {}/{} bytes", downloaded_bytes, total_file_size);
    }

    println!();

    /// Example 3: Extract Portions of Files
    /// 
    /// This example shows how to extract specific portions of files, such as
    /// reading specific records from a binary file or extracting sections
    /// from structured data files.
    println!("\n3. Extract Portions of Files");
    println!("----------------------------");
    println!();
    println!("   Extracting portions is useful for structured files where you");
    println!("   know the layout and only need specific sections.");
    println!();

    /// Extract multiple non-contiguous ranges
    /// 
    /// This demonstrates extracting several different sections from a file.
    /// Useful for reading specific records or sections from structured files.
    let extract_ranges = vec![
        (0u64, 50u64, "Header section"),
        (200u64, 50u64, "Data section 1"),
        (400u64, 50u64, "Data section 2"),
        (800u64, 50u64, "Trailer section"),
    ];

    println!("   Extracting {} non-contiguous ranges...", extract_ranges.len());
    println!();

    let mut extracted_sections = Vec::new();

    for (index, (offset, length, description)) in extract_ranges.iter().enumerate() {
        println!("   Range {}: {} (offset: {}, length: {})", 
                index + 1, description, offset, length);

        match client.download_file_range(&file_id, *offset, *length).await {
            Ok(data) => {
                extracted_sections.push((*description, data.clone()));
                println!("     ✓ Extracted {} bytes", data.len());
            }
            Err(e) => {
                println!("     ✗ Extraction failed: {}", e);
            }
        }
        println!();
    }

    println!("   Extraction Summary:");
    println!("   → Extracted {} sections", extracted_sections.len());
    for (desc, data) in &extracted_sections {
        println!("     - {}: {} bytes", desc, data.len());
    }

    println!();

    /// Example 4: Streaming Large Files
    /// 
    /// This example demonstrates how to stream large files in chunks,
    /// processing them as they're downloaded rather than loading the
    /// entire file into memory at once.
    println!("\n4. Streaming Large Files");
    println!("------------------------");
    println!();
    println!("   Streaming processes files in chunks, avoiding loading entire");
    println!("   files into memory. Essential for very large files.");
    println!();

    /// Get file size first
    /// 
    /// We need to know the file size to determine how many chunks to download.
    let file_info = match client.get_file_info(&file_id).await {
        Ok(info) => {
            println!("   File size: {} bytes", info.file_size);
            info
        }
        Err(e) => {
            println!("   ✗ Failed to get file info: {}", e);
            return Ok(());
        }
    };

    let stream_chunk_size = 100u64;
    let total_chunks = (file_info.file_size as u64 + stream_chunk_size - 1) / stream_chunk_size;

    println!("   Streaming file in {} byte chunks...", stream_chunk_size);
    println!("   → Total chunks: {}", total_chunks);
    println!();

    let stream_start = Instant::now();
    let mut streamed_bytes = 0u64;
    let mut processed_chunks = 0usize;

    /// Stream the file chunk by chunk
    /// 
    /// Each chunk is downloaded and can be processed immediately,
    /// then discarded to free memory. This allows processing files
    /// larger than available memory.
    for chunk_index in 0..total_chunks {
        let offset = chunk_index * stream_chunk_size;
        let remaining = file_info.file_size as u64 - streamed_bytes;
        let current_chunk_size = stream_chunk_size.min(remaining);

        match client.download_file_range(&file_id, offset, current_chunk_size).await {
            Ok(chunk_data) => {
                streamed_bytes += chunk_data.len() as u64;
                processed_chunks += 1;

                /// Process the chunk (in real scenario, this might be parsing, writing, etc.)
                /// 
                /// Here we just verify the chunk, but in production you might:
                /// - Parse the chunk data
                /// - Write to a file
                /// - Process and discard
                /// - Send to another system
                println!("   → Chunk {}/{}: {} bytes processed (total: {}/{})", 
                        chunk_index + 1, total_chunks, chunk_data.len(), 
                        streamed_bytes, file_info.file_size);

                /// In a real streaming scenario, you would process the chunk here
                /// and then discard it to free memory. For this example, we just
                /// track that we've processed it.
            }
            Err(e) => {
                println!("   ✗ Chunk {} failed: {}", chunk_index + 1, e);
                break;
            }
        }
    }

    let stream_elapsed = stream_start.elapsed();

    println!();
    println!("   Streaming Summary:");
    println!("   → Chunks processed: {}/{}", processed_chunks, total_chunks);
    println!("   → Bytes streamed: {}/{}", streamed_bytes, file_info.file_size);
    println!("   → Total time: {:?}", stream_elapsed);
    println!("   → Streaming rate: {:.2} KB/s", 
             (streamed_bytes as f64 / 1024.0) / stream_elapsed.as_secs_f64());
    println!();
    println!("   → Memory-efficient: Only one chunk in memory at a time");
    println!("   → Can handle files larger than available memory");
    println!("   → Processing can begin before entire file is downloaded");
    println!();

    /// Example 5: Memory-Efficient Downloads
    /// 
    /// This example demonstrates memory-efficient download patterns,
    /// comparing full downloads vs chunked downloads in terms of memory usage.
    println!("\n5. Memory-Efficient Downloads");
    println!("----------------------------");
    println!();
    println!("   Memory-efficient patterns are crucial for large files and");
    println!("   resource-constrained environments.");
    println!();

    /// Pattern 1: Full Download (Memory Intensive)
    /// 
    /// Downloading the entire file at once loads everything into memory.
    /// This is simple but uses more memory.
    println!("   Pattern 1: Full Download (Memory Intensive)");
    println!("   → Downloads entire file into memory");
    println!("   → Simple but uses more memory");
    println!();

    let full_start = Instant::now();
    match client.download_file(&file_id).await {
        Ok(full_data) => {
            let full_elapsed = full_start.elapsed();
            println!("   ✓ Full download completed");
            println!("   → Size: {} bytes ({:.2} KB)", 
                    full_data.len(), full_data.len() as f64 / 1024.0);
            println!("   → Time: {:?}", full_elapsed);
            println!("   → Memory: Entire file in memory at once");
        }
        Err(e) => {
            println!("   ✗ Full download failed: {}", e);
        }
    }

    println!();

    /// Pattern 2: Chunked Download (Memory Efficient)
    /// 
    /// Downloading in chunks processes data incrementally,
    /// using less memory overall.
    println!("   Pattern 2: Chunked Download (Memory Efficient)");
    println!("   → Downloads file in smaller chunks");
    println!("   → Processes each chunk and discards it");
    println!("   → Uses less memory overall");
    println!();

    let chunked_chunk_size = 100u64;
    let chunked_start = Instant::now();
    let mut chunked_total = 0u64;
    let mut chunked_count = 0usize;

    let mut chunked_offset = 0u64;
    while chunked_offset < file_info.file_size as u64 {
        let remaining = file_info.file_size as u64 - chunked_offset;
        let current_size = chunked_chunk_size.min(remaining);

        match client.download_file_range(&file_id, chunked_offset, current_size).await {
            Ok(chunk) => {
                chunked_total += chunk.len() as u64;
                chunked_count += 1;
                chunked_offset += chunk.len() as u64;

                /// Process chunk and discard
                /// 
                /// In a real scenario, you would process the chunk here
                /// (e.g., write to file, parse, transform) and then it
                /// goes out of scope and is freed.
            }
            Err(e) => {
                println!("   ✗ Chunk download failed: {}", e);
                break;
            }
        }
    }

    let chunked_elapsed = chunked_start.elapsed();

    println!("   ✓ Chunked download completed");
    println!("   → Chunks: {}", chunked_count);
    println!("   → Total: {} bytes ({:.2} KB)", 
            chunked_total, chunked_total as f64 / 1024.0);
    println!("   → Time: {:?}", chunked_elapsed);
    println!("   → Memory: Only one chunk ({:.2} KB) in memory at a time", 
            chunked_chunk_size as f64 / 1024.0);
    println!();

    /// Memory Comparison
    /// 
    /// Compare memory usage between full and chunked downloads.
    println!("   Memory Comparison:");
    println!("   → Full download: {} KB in memory", file_info.file_size as f64 / 1024.0);
    println!("   → Chunked download: {:.2} KB in memory (max)", 
            chunked_chunk_size as f64 / 1024.0);
    println!("   → Memory savings: {:.1}%", 
            (1.0 - chunked_chunk_size as f64 / file_info.file_size as f64) * 100.0);
    println!();

    /// Example 6: Range Request Patterns
    /// 
    /// This example demonstrates common range request patterns used
    /// in real-world applications.
    println!("\n6. Range Request Patterns");
    println!("-------------------------");
    println!();
    println!("   Different range request patterns serve different use cases.");
    println!("   This example demonstrates common patterns.");
    println!();

    /// Pattern 1: Sequential Ranges
    /// 
    /// Downloading ranges sequentially, one after another.
    /// Useful when you need multiple sections in order.
    println!("   Pattern 1: Sequential Ranges");
    println!("   → Download ranges one after another");
    println!("   → Useful for processing file sections in order");
    println!();

    let sequential_ranges = vec![(0u64, 100u64), (100u64, 100u64), (200u64, 100u64)];
    let mut sequential_data = Vec::new();

    for (index, (offset, length)) in sequential_ranges.iter().enumerate() {
        match client.download_file_range(&file_id, *offset, *length).await {
            Ok(data) => {
                sequential_data.push(data);
                println!("   → Range {}: offset {}, length {} - downloaded", 
                        index + 1, offset, length);
            }
            Err(e) => {
                println!("   → Range {} failed: {}", index + 1, e);
            }
        }
    }

    println!("   → Total sequential ranges: {}", sequential_data.len());
    println!();

    /// Pattern 2: Parallel Ranges
    /// 
    /// Downloading multiple ranges concurrently.
    /// Faster when ranges are independent.
    println!("   Pattern 2: Parallel Ranges");
    println!("   → Download multiple ranges concurrently");
    println!("   → Faster when ranges are independent");
    println!();

    let parallel_ranges = vec![(0u64, 100u64), (200u64, 100u64), (400u64, 100u64), (600u64, 100u64)];

    let parallel_start = Instant::now();
    let parallel_tasks: Vec<_> = parallel_ranges
        .iter()
        .enumerate()
        .map(|(index, (offset, length))| {
            let client_ref = &client;
            let file_id_ref = &file_id;
            async move {
                let result = client_ref.download_file_range(file_id_ref, *offset, *length).await;
                (index + 1, *offset, *length, result)
            }
        })
        .collect();

    let parallel_results = futures::future::join_all(parallel_tasks).await;
    let parallel_elapsed = parallel_start.elapsed();

    let mut parallel_successful = 0;
    for (index, offset, length, result) in parallel_results {
        match result {
            Ok(data) => {
                parallel_successful += 1;
                println!("   → Range {}: offset {}, length {} - downloaded {} bytes", 
                        index, offset, length, data.len());
            }
            Err(e) => {
                println!("   → Range {} failed: {}", index, e);
            }
        }
    }

    println!("   → Parallel download time: {:?}", parallel_elapsed);
    println!("   → Successful ranges: {}/{}", parallel_successful, parallel_ranges.len());
    println!();

    /// Pattern 3: Overlapping Ranges
    /// 
    /// Downloading ranges that overlap can be useful for redundancy
    /// or when you need to ensure you have all data.
    println!("   Pattern 3: Overlapping Ranges");
    println!("   → Ranges overlap to ensure data coverage");
    println!("   → Useful for redundancy or data verification");
    println!();

    let overlapping_ranges = vec![(0u64, 150u64), (100u64, 150u64), (200u64, 150u64)];

    for (index, (offset, length)) in overlapping_ranges.iter().enumerate() {
        match client.download_file_range(&file_id, *offset, *length).await {
            Ok(data) => {
                println!("   → Range {}: offset {}, length {} - downloaded {} bytes", 
                        index + 1, offset, length, data.len());
            }
            Err(e) => {
                println!("   → Range {} failed: {}", index + 1, e);
            }
        }
    }

    println!("   → Overlapping ranges provide redundancy");
    println!("   → Overlap ensures no data is missed");
    println!();

    /// Example 7: Chunked Downloads
    /// 
    /// This example demonstrates downloading files in fixed-size chunks,
    /// which is a common pattern for processing large files or implementing
    /// progress tracking.
    println!("\n7. Chunked Downloads");
    println!("------------------");
    println!();
    println!("   Chunked downloads break files into fixed-size pieces.");
    println!("   Useful for progress tracking and processing large files.");
    println!();

    /// Download file in fixed-size chunks
    /// 
    /// This pattern is useful for:
    /// - Progress tracking (know how many chunks completed)
    /// - Processing large files incrementally
    /// - Implementing retry logic per chunk
    let fixed_chunk_size = 150u64;
    let total_file_bytes = file_info.file_size as u64;
    let total_fixed_chunks = (total_file_bytes + fixed_chunk_size - 1) / fixed_chunk_size;

    println!("   Downloading file in {} byte chunks...", fixed_chunk_size);
    println!("   → Total chunks: {}", total_fixed_chunks);
    println!();

    let chunked_start = Instant::now();
    let mut chunked_results = Vec::new();
    let mut chunked_progress = 0u64;

    for chunk_num in 0..total_fixed_chunks {
        let chunk_offset = chunk_num * fixed_chunk_size;
        let remaining = total_file_bytes - chunked_progress;
        let current_chunk_size = fixed_chunk_size.min(remaining);

        println!("   → Chunk {}/{}: offset {}, length {}", 
                chunk_num + 1, total_fixed_chunks, chunk_offset, current_chunk_size);

        match client.download_file_range(&file_id, chunk_offset, current_chunk_size).await {
            Ok(chunk_data) => {
                chunked_progress += chunk_data.len() as u64;
                chunked_results.push(Ok(chunk_data.len()));
                
                let progress_pct = (chunked_progress as f64 / total_file_bytes as f64) * 100.0;
                println!("     ✓ Downloaded {} bytes (progress: {:.1}%)", 
                        chunk_data.len(), progress_pct);
            }
            Err(e) => {
                chunked_results.push(Err(e.to_string()));
                println!("     ✗ Failed: {}", e);
            }
        }
    }

    let chunked_elapsed = chunked_start.elapsed();

    let chunked_successful: usize = chunked_results.iter()
        .filter(|r| r.is_ok())
        .count();

    println!();
    println!("   Chunked Download Summary:");
    println!("   → Total chunks: {}", total_fixed_chunks);
    println!("   → Successful: {}", chunked_successful);
    println!("   → Failed: {}", total_fixed_chunks - chunked_successful);
    println!("   → Total bytes: {}/{}", chunked_progress, total_file_bytes);
    println!("   → Total time: {:?}", chunked_elapsed);
    println!("   → Average chunk time: {:?}", 
            chunked_elapsed / total_fixed_chunks as u32);
    println!();
    println!("   → Chunked downloads enable progress tracking");
    println!("   → Each chunk can be processed independently");
    println!("   → Failed chunks can be retried individually");
    println!();

    /// Summary and Key Takeaways
    /// 
    /// Provide comprehensive summary of partial download patterns.
    println!("\n{}", "=".repeat(50));
    println!("Partial download example completed!");
    println!("{}", "=".repeat(50));
    println!();

    println!("Key Takeaways:");
    println!();
    println!("  • Range requests are efficient for partial file access");
    println!("    → Download only the bytes you need");
    println!("    → Reduces bandwidth and memory usage");
    println!("    → Faster than downloading entire files");
    println!();
    println!("  • Resume interrupted downloads by tracking progress");
    println!("    → Store the last successfully downloaded byte offset");
    println!("    → Resume from that offset when retrying");
    println!("    → Essential for large files and unreliable networks");
    println!();
    println!("  • Extract portions for structured file access");
    println!("    → Read specific sections without downloading everything");
    println!("    → Useful for binary formats with known layouts");
    println!("    → Can extract multiple non-contiguous ranges");
    println!();
    println!("  • Stream large files to avoid memory issues");
    println!("    → Process files chunk by chunk");
    println!("    → Only one chunk in memory at a time");
    println!("    → Can handle files larger than available memory");
    println!();
    println!("  • Use chunked downloads for progress tracking");
    println!("    → Fixed chunk size enables progress calculation");
    println!("    → Each chunk can be processed independently");
    println!("    → Failed chunks can be retried without re-downloading others");
    println!();
    println!("  • Parallel range requests improve performance");
    println!("    → Download multiple ranges concurrently");
    println!("    → Faster when ranges are independent");
    println!("    → Connection pool handles concurrent requests");
    println!();
    println!("  • Memory-efficient patterns are crucial for large files");
    println!("    → Chunked downloads use less memory");
    println!("    → Streaming processes data incrementally");
    println!("    → Essential for resource-constrained environments");
    println!();

    /// Clean up test file
    println!("Cleaning up test file...");
    let _ = client.delete_file(&file_id).await;

    /// Close the client
    println!("Closing client and releasing resources...");
    client.close().await;
    println!("Client closed.");
    println!();

    Ok(())
}

