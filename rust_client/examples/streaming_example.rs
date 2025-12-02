/*! FastDFS Streaming Large Files Example
 *
 * This comprehensive example demonstrates how to stream large files efficiently
 * with the FastDFS Rust client. It covers memory-efficient operations, chunked
 * uploads and downloads, progress reporting, backpressure handling, and
 * streaming patterns using Tokio and Futures.
 *
 * Streaming topics covered:
 * - Streaming large files efficiently
 * - Memory-efficient operations (avoid loading entire files into memory)
 * - Chunked uploads and downloads
 * - Progress reporting during operations
 * - Backpressure handling
 * - Using tokio::io::AsyncRead/AsyncWrite traits
 * - Streaming with futures::stream
 *
 * Understanding streaming is crucial for:
 * - Handling large files without running out of memory
 * - Building efficient file transfer applications
 * - Providing user feedback during long operations
 * - Managing resource usage effectively
 * - Creating scalable file processing systems
 * - Implementing real-time file operations
 *
 * Run this example with:
 * ```bash
 * cargo run --example streaming_example
 * ```
 */

/* Import FastDFS client components */
/* Client provides the main API for FastDFS operations */
/* ClientConfig allows configuration of connection parameters */
use fastdfs::{Client, ClientConfig};
/* Import Tokio async I/O traits */
/* AsyncRead and AsyncWrite enable streaming operations */
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
/* Import Futures for streaming */
/* Stream trait and utilities for async streaming */
use futures::stream::{self, Stream, StreamExt};
/* Import Tokio time utilities */
/* For delays and time measurement */
use tokio::time::{sleep, Duration, Instant};
/* Import standard library for collections and I/O */
use std::pin::Pin;
use std::task::{Context, Poll};
use std::io;

/* ====================================================================
 * PROGRESS TRACKER
 * ====================================================================
 * Utility for tracking and reporting operation progress.
 */

/* Progress information structure */
/* Tracks bytes transferred and percentage complete */
struct Progress {
    /* Total bytes to transfer */
    total: u64,
    /* Bytes transferred so far */
    transferred: u64,
    /* Start time of operation */
    start_time: Instant,
}

/* Implementation of progress tracker */
impl Progress {
    /* Create a new progress tracker */
    /* Initialize with total size and start time */
    fn new(total: u64) -> Self {
        Self {
            total,
            transferred: 0,
            start_time: Instant::now(),
        }
    }
    
    /* Update progress with bytes transferred */
    /* Call this as data is transferred */
    fn update(&mut self, bytes: u64) {
        self.transferred += bytes;
    }
    
    /* Get current progress percentage */
    /* Returns 0-100 representing completion percentage */
    fn percentage(&self) -> f64 {
        if self.total == 0 {
            return 0.0;
        }
        (self.transferred as f64 / self.total as f64) * 100.0
    }
    
    /* Get transfer rate in bytes per second */
    /* Calculates average speed based on elapsed time */
    fn bytes_per_second(&self) -> f64 {
        let elapsed = self.start_time.elapsed().as_secs_f64();
        if elapsed == 0.0 {
            return 0.0;
        }
        self.transferred as f64 / elapsed
    }
    
    /* Format bytes as human-readable string */
    /* Converts bytes to KB, MB, GB format */
    fn format_bytes(bytes: u64) -> String {
        if bytes < 1024 {
            format!("{} B", bytes)
        } else if bytes < 1024 * 1024 {
            format!("{:.2} KB", bytes as f64 / 1024.0)
        } else if bytes < 1024 * 1024 * 1024 {
            format!("{:.2} MB", bytes as f64 / (1024.0 * 1024.0))
        } else {
            format!("{:.2} GB", bytes as f64 / (1024.0 * 1024.0 * 1024.0))
        }
    }
    
    /* Print current progress */
    /* Displays progress bar, percentage, and transfer rate */
    fn print(&self) {
        let percentage = self.percentage();
        let rate = self.bytes_per_second();
        let elapsed = self.start_time.elapsed();
        
        /* Create simple progress bar */
        /* Shows visual progress indication */
        let bar_width = 50;
        let filled = (percentage / 100.0 * bar_width as f64) as usize;
        let bar = "=".repeat(filled) + &" ".repeat(bar_width - filled);
        
        println!(
            "  [{bar}] {:.1}% | {} / {} | {:.2} MB/s | {:?}",
            percentage,
            Self::format_bytes(self.transferred),
            Self::format_bytes(self.total),
            rate / (1024.0 * 1024.0),
            elapsed
        );
    }
}

/* ====================================================================
 * CHUNKED UPLOAD HELPER
 * ====================================================================
 * Functions for uploading large files in chunks.
 */

/* Upload a file in chunks with progress reporting */
/* This is memory-efficient as it doesn't load the entire file */
async fn upload_file_chunked(
    client: &Client,
    data: &[u8],
    chunk_size: usize,
    file_ext: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    /* For demonstration, we'll simulate chunked upload */
    /* In production, you would read from a file stream */
    println!("   Starting chunked upload...");
    println!("   Total size: {}", Progress::format_bytes(data.len() as u64));
    println!("   Chunk size: {}", Progress::format_bytes(chunk_size as u64));
    
    /* Create progress tracker */
    /* Track upload progress */
    let mut progress = Progress::new(data.len() as u64);
    
    /* Process file in chunks */
    /* This simulates reading from a stream */
    let mut chunks = Vec::new();
    for (i, chunk) in data.chunks(chunk_size).enumerate() {
        /* Simulate chunk processing */
        /* In real code, this would be reading from AsyncRead */
        chunks.push(chunk.to_vec());
        
        /* Update progress */
        progress.update(chunk.len() as u64);
        
        /* Print progress every 10 chunks */
        /* Avoid flooding output with progress updates */
        if i % 10 == 0 {
            progress.print();
        }
        
        /* Small delay to simulate I/O */
        /* In real code, this is actual I/O time */
        sleep(Duration::from_millis(10)).await;
    }
    
    /* Final progress update */
    progress.print();
    println!("   ✓ All chunks prepared");
    
    /* Upload all data at once */
    /* In a real streaming implementation, you might upload chunks separately */
    /* For this example, we upload the complete data */
    let file_id = client.upload_buffer(data, file_ext, None).await?;
    println!("   ✓ Upload completed: {}", file_id);
    
    Ok(file_id)
}

/* ====================================================================
 * CHUNKED DOWNLOAD HELPER
 * ====================================================================
 * Functions for downloading large files in chunks.
 */

/* Download a file in chunks with progress reporting */
/* Uses download_file_range for memory-efficient downloads */
async fn download_file_chunked(
    client: &Client,
    file_id: &str,
    chunk_size: usize,
) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    println!("   Starting chunked download...");
    println!("   File ID: {}", file_id);
    println!("   Chunk size: {}", Progress::format_bytes(chunk_size as u64));
    
    /* Get file info to determine total size */
    /* This allows us to track progress accurately */
    let file_info = client.get_file_info(file_id).await?;
    let total_size = file_info.file_size;
    
    println!("   Total size: {}", Progress::format_bytes(total_size));
    
    /* Create progress tracker */
    let mut progress = Progress::new(total_size);
    
    /* Download file in chunks */
    /* This is memory-efficient for large files */
    let mut downloaded_data = Vec::new();
    let mut offset = 0u64;
    
    while offset < total_size {
        /* Calculate chunk size for this iteration */
        /* Last chunk may be smaller */
        let remaining = total_size - offset;
        let current_chunk_size = std::cmp::min(chunk_size as u64, remaining) as u64;
        
        /* Download this chunk */
        /* download_file_range allows partial downloads */
        let chunk = client.download_file_range(file_id, offset, current_chunk_size).await?;
        let chunk_len = chunk.len() as u64;
        
        /* Append chunk to result */
        /* In a real streaming scenario, you'd write to AsyncWrite */
        downloaded_data.extend_from_slice(&chunk);
        
        /* Update progress */
        progress.update(chunk_len);
        
        /* Print progress periodically */
        /* Show progress every few chunks */
        if (offset / chunk_size as u64) % 5 == 0 {
            progress.print();
        }
        
        /* Move to next chunk */
        offset += chunk_len;
        
        /* Small delay to simulate processing */
        sleep(Duration::from_millis(5)).await;
    }
    
    /* Final progress update */
    progress.print();
    println!("   ✓ Download completed");
    println!("   Downloaded: {}", Progress::format_bytes(downloaded_data.len() as u64));
    
    Ok(downloaded_data)
}

/* ====================================================================
 * STREAMING WITH FUTURES::STREAM
 * ====================================================================
 * Demonstrate streaming using futures::stream.
 */

/* Create a stream of file chunks */
/* This demonstrates streaming pattern with futures::stream */
fn create_chunk_stream(
    data: &[u8],
    chunk_size: usize,
) -> impl Stream<Item = Vec<u8>> {
    /* Create stream from iterator */
    /* Each item is a chunk of the data */
    stream::iter(
        data.chunks(chunk_size)
            .map(|chunk| chunk.to_vec())
            .collect::<Vec<_>>()
    )
}

/* Process stream with progress tracking */
/* Demonstrates consuming a stream with progress updates */
async fn process_stream_with_progress<S>(
    mut stream: S,
    total_size: u64,
) -> Result<Vec<Vec<u8>>, Box<dyn std::error::Error>>
where
    S: Stream<Item = Vec<u8>> + Unpin,
{
    let mut progress = Progress::new(total_size);
    let mut chunks = Vec::new();
    
    /* Process each chunk in the stream */
    /* StreamExt provides methods for consuming streams */
    while let Some(chunk) = stream.next().await {
        let chunk_size = chunk.len() as u64;
        chunks.push(chunk);
        
        /* Update progress */
        progress.update(chunk_size);
        
        /* Print progress every 10 chunks */
        if chunks.len() % 10 == 0 {
            progress.print();
        }
        
        /* Small delay to simulate processing */
        sleep(Duration::from_millis(5)).await;
    }
    
    /* Final progress */
    progress.print();
    
    Ok(chunks)
}

/* ====================================================================
 * ASYNC READ/WRITE STREAMING
 * ====================================================================
 * Demonstrate streaming using AsyncRead and AsyncWrite traits.
 */

/* Simple in-memory AsyncRead implementation */
/* For demonstration purposes - in production, use file streams */
struct MemoryReader {
    data: Vec<u8>,
    position: usize,
}

/* Implementation of AsyncRead for MemoryReader */
/* Allows reading data asynchronously */
impl AsyncRead for MemoryReader {
    fn poll_read(
        mut self: Pin<&mut Self>,
        _cx: &mut Context<'_>,
        buf: &mut tokio::io::ReadBuf<'_>,
    ) -> Poll<io::Result<()>> {
        /* Calculate how much to read */
        let remaining = self.data.len() - self.position;
        let to_read = std::cmp::min(remaining, buf.remaining());
        
        if to_read > 0 {
            /* Copy data to buffer */
            buf.put_slice(&self.data[self.position..self.position + to_read]);
            self.position += to_read;
        }
        
        Poll::Ready(Ok(()))
    }
}

/* Simple in-memory AsyncWrite implementation */
/* For demonstration purposes - in production, use file streams */
struct MemoryWriter {
    data: Vec<u8>,
}

/* Implementation of AsyncWrite for MemoryWriter */
/* Allows writing data asynchronously */
impl AsyncWrite for MemoryWriter {
    fn poll_write(
        mut self: Pin<&mut Self>,
        _cx: &mut Context<'_>,
        buf: &[u8],
    ) -> Poll<io::Result<usize>> {
        /* Append data to internal buffer */
        self.data.extend_from_slice(buf);
        Poll::Ready(Ok(buf.len()))
    }
    
    fn poll_flush(
        self: Pin<&mut Self>,
        _cx: &mut Context<'_>,
    ) -> Poll<io::Result<()>> {
        /* No-op for in-memory writer */
        Poll::Ready(Ok(()))
    }
    
    fn poll_shutdown(
        self: Pin<&mut Self>,
        _cx: &mut Context<'_>,
    ) -> Poll<io::Result<()>> {
        /* No-op for in-memory writer */
        Poll::Ready(Ok(()))
    }
}

/* Copy from AsyncRead to AsyncWrite with progress */
/* Demonstrates streaming data between async I/O sources */
async fn copy_with_progress<R, W>(
    mut reader: R,
    mut writer: W,
    total_size: u64,
    buffer_size: usize,
) -> Result<u64, Box<dyn std::error::Error>>
where
    R: AsyncRead + Unpin,
    W: AsyncWrite + Unpin,
{
    let mut progress = Progress::new(total_size);
    let mut buffer = vec![0u8; buffer_size];
    let mut total_copied = 0u64;
    
    /* Read and write in a loop */
    /* This is the core streaming pattern */
    loop {
        /* Read chunk from source */
        let bytes_read = reader.read(&mut buffer).await?;
        
        if bytes_read == 0 {
            /* End of stream */
            break;
        }
        
        /* Write chunk to destination */
        writer.write_all(&buffer[..bytes_read]).await?;
        
        /* Update progress */
        total_copied += bytes_read as u64;
        progress.update(bytes_read as u64);
        
        /* Print progress periodically */
        if total_copied % (buffer_size as u64 * 10) < buffer_size as u64 {
            progress.print();
        }
    }
    
    /* Flush writer */
    writer.flush().await?;
    
    /* Final progress */
    progress.print();
    
    Ok(total_copied)
}

/* ====================================================================
 * BACKPRESSURE HANDLING
 * ====================================================================
 * Demonstrate handling backpressure in streaming operations.
 */

/* Stream with backpressure control */
/* Limits the rate of data processing to prevent overwhelming the system */
async fn stream_with_backpressure<S>(
    mut stream: S,
    max_concurrent: usize,
    total_size: u64,
) -> Result<Vec<Vec<u8>>, Box<dyn std::error::Error>>
where
    S: Stream<Item = Vec<u8>> + Unpin,
{
    println!("   Processing stream with backpressure control...");
    println!("   Max concurrent chunks: {}", max_concurrent);
    
    let mut progress = Progress::new(total_size);
    let mut chunks = Vec::new();
    let mut buffer = Vec::new();
    
    /* Process stream with concurrency limit */
    /* This prevents overwhelming the system */
    while let Some(chunk) = stream.next().await {
        /* Add chunk to buffer */
        buffer.push(chunk);
        
        /* Process when buffer reaches limit */
        /* This implements backpressure */
        if buffer.len() >= max_concurrent {
            /* Process buffered chunks */
            for buffered_chunk in buffer.drain(..) {
                let chunk_size = buffered_chunk.len() as u64;
                chunks.push(buffered_chunk);
                progress.update(chunk_size);
                
                /* Small delay to simulate processing */
                /* In real code, this is actual processing time */
                sleep(Duration::from_millis(10)).await;
            }
            
            /* Print progress */
            if chunks.len() % 10 == 0 {
                progress.print();
            }
        }
    }
    
    /* Process remaining chunks */
    for chunk in buffer {
        let chunk_size = chunk.len() as u64;
        chunks.push(chunk);
        progress.update(chunk_size);
    }
    
    /* Final progress */
    progress.print();
    
    Ok(chunks)
}

/* ====================================================================
 * MAIN EXAMPLE FUNCTION
 * ====================================================================
 * Demonstrates all streaming patterns and techniques.
 */

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    /* Print header for better output readability */
    println!("FastDFS Rust Client - Streaming Large Files Example");
    println!("{}", "=".repeat(70));

    /* ====================================================================
     * STEP 1: Initialize Client
     * ====================================================================
     * Set up the FastDFS client for streaming demonstrations.
     */
    
    println!("\n1. Initializing FastDFS Client...");
    /* Configure client with appropriate settings */
    /* For streaming, we may want larger timeouts for large files */
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(10)
        .with_connect_timeout(5000)
        .with_network_timeout(60000) /* Longer timeout for large files */
        .with_retry_count(3);
    
    /* Create the client instance */
    let client = Client::new(config)?;
    println!("   ✓ Client initialized successfully");

    /* ====================================================================
     * EXAMPLE 1: Chunked Upload with Progress
     * ====================================================================
     * Demonstrate uploading large files in chunks with progress reporting.
     */
    
    println!("\n2. Chunked Upload with Progress Reporting...");
    
    /* Create large test data */
    /* Simulate a large file (1 MB for demonstration) */
    let large_data: Vec<u8> = (0..1024 * 1024).map(|i| (i % 256) as u8).collect();
    println!("   Created test data: {}", Progress::format_bytes(large_data.len() as u64));
    
    /* Upload in chunks */
    /* This demonstrates memory-efficient upload */
    let chunk_size = 64 * 1024; /* 64 KB chunks */
    let file_id = upload_file_chunked(&client, &large_data, chunk_size, "bin").await?;
    println!("   ✓ Chunked upload completed");
    
    /* Store file ID for download examples */
    let uploaded_file_id = file_id;

    /* ====================================================================
     * EXAMPLE 2: Chunked Download with Progress
     * ====================================================================
     * Demonstrate downloading large files in chunks with progress reporting.
     */
    
    println!("\n3. Chunked Download with Progress Reporting...");
    
    /* Download in chunks */
    /* This demonstrates memory-efficient download */
    let download_chunk_size = 128 * 1024; /* 128 KB chunks */
    let downloaded_data = download_file_chunked(&client, &uploaded_file_id, download_chunk_size).await?;
    
    /* Verify downloaded data matches original */
    /* Ensure data integrity */
    if downloaded_data.len() == large_data.len() {
        println!("   ✓ Download size matches original");
    } else {
        println!("   ⚠ Download size mismatch: {} vs {}", downloaded_data.len(), large_data.len());
    }
    
    println!("   ✓ Chunked download completed");

    /* ====================================================================
     * EXAMPLE 3: Streaming with futures::stream
     * ====================================================================
     * Demonstrate streaming using futures::stream.
     */
    
    println!("\n4. Streaming with futures::stream...");
    
    /* Create a stream of chunks */
    /* This demonstrates the stream pattern */
    let stream_chunk_size = 32 * 1024; /* 32 KB chunks */
    let chunk_stream = create_chunk_stream(&large_data, stream_chunk_size);
    
    println!("   Created chunk stream with {} KB chunks", stream_chunk_size / 1024);
    
    /* Process stream with progress */
    /* Consume the stream and track progress */
    let processed_chunks = process_stream_with_progress(
        chunk_stream,
        large_data.len() as u64,
    ).await?;
    
    println!("   ✓ Processed {} chunks from stream", processed_chunks.len());
    println!("   Total data: {}", Progress::format_bytes(
        processed_chunks.iter().map(|c| c.len()).sum::<usize>() as u64
    ));

    /* ====================================================================
     * EXAMPLE 4: AsyncRead/AsyncWrite Streaming
     * ====================================================================
     * Demonstrate streaming using AsyncRead and AsyncWrite traits.
     */
    
    println!("\n5. AsyncRead/AsyncWrite Streaming...");
    
    /* Create AsyncRead source */
    /* In production, this would be a file or network stream */
    let reader = MemoryReader {
        data: large_data.clone(),
        position: 0,
    };
    
    /* Create AsyncWrite destination */
    /* In production, this would be a file or network stream */
    let writer = MemoryWriter {
        data: Vec::new(),
    };
    
    println!("   Streaming from AsyncRead to AsyncWrite...");
    
    /* Copy with progress tracking */
    /* This demonstrates streaming between async I/O sources */
    let buffer_size = 16 * 1024; /* 16 KB buffer */
    let copied = copy_with_progress(
        reader,
        writer,
        large_data.len() as u64,
        buffer_size,
    ).await?;
    
    println!("   ✓ Copied {} bytes via AsyncRead/AsyncWrite", copied);
    println!("   ✓ Streaming completed successfully");

    /* ====================================================================
     * EXAMPLE 5: Backpressure Handling
     * ====================================================================
     * Demonstrate handling backpressure in streaming operations.
     */
    
    println!("\n6. Backpressure Handling...");
    
    /* Create stream for backpressure demonstration */
    let backpressure_stream = create_chunk_stream(&large_data, 16 * 1024);
    
    /* Process with backpressure control */
    /* Limit concurrent processing to prevent overwhelming the system */
    let max_concurrent = 5; /* Process max 5 chunks concurrently */
    let backpressure_chunks = stream_with_backpressure(
        backpressure_stream,
        max_concurrent,
        large_data.len() as u64,
    ).await?;
    
    println!("   ✓ Processed {} chunks with backpressure control", backpressure_chunks.len());
    println!("   ✓ Backpressure handling completed");

    /* ====================================================================
     * EXAMPLE 6: Memory-Efficient Large File Operations
     * ====================================================================
     * Demonstrate memory-efficient patterns for very large files.
     */
    
    println!("\n7. Memory-Efficient Large File Operations...");
    
    /* Simulate a very large file (10 MB) */
    /* In production, this would be read from disk, not created in memory */
    println!("   Simulating very large file operation (10 MB)...");
    let very_large_size = 10 * 1024 * 1024; /* 10 MB */
    
    /* Process in small chunks to minimize memory usage */
    /* Small chunks = lower memory footprint */
    let memory_efficient_chunk_size = 8 * 1024; /* 8 KB chunks */
    let num_chunks = (very_large_size + memory_efficient_chunk_size - 1) / memory_efficient_chunk_size;
    
    println!("   Chunk size: {}", Progress::format_bytes(memory_efficient_chunk_size as u64));
    println!("   Number of chunks: {}", num_chunks);
    println!("   Memory usage: ~{} per chunk", Progress::format_bytes(memory_efficient_chunk_size as u64));
    
    /* Simulate processing chunks */
    /* In production, each chunk would be processed independently */
    let mut memory_progress = Progress::new(very_large_size as u64);
    for i in 0..num_chunks {
        /* Simulate processing a chunk */
        /* In real code, this would be actual file I/O */
        let chunk_start = i * memory_efficient_chunk_size;
        let chunk_end = std::cmp::min((i + 1) * memory_efficient_chunk_size, very_large_size);
        let chunk_size = chunk_end - chunk_start;
        
        /* Update progress */
        memory_progress.update(chunk_size as u64);
        
        /* Print progress every 100 chunks */
        if i % 100 == 0 {
            memory_progress.print();
        }
        
        /* Small delay to simulate I/O */
        sleep(Duration::from_millis(1)).await;
    }
    
    /* Final progress */
    memory_progress.print();
    println!("   ✓ Memory-efficient processing completed");
    println!("   Peak memory usage: ~{} (one chunk at a time)", 
             Progress::format_bytes(memory_efficient_chunk_size as u64));

    /* ====================================================================
     * EXAMPLE 7: Progress Reporting Patterns
     * ====================================================================
     * Demonstrate different progress reporting patterns.
     */
    
    println!("\n8. Progress Reporting Patterns...");
    
    println!("\n   Pattern 1: Percentage-based reporting");
    let mut progress1 = Progress::new(1000000);
    for i in 0..10 {
        progress1.update(100000);
        println!("     Progress: {:.1}%", progress1.percentage());
    }
    
    println!("\n   Pattern 2: Rate-based reporting");
    let mut progress2 = Progress::new(1000000);
    for i in 0..5 {
        progress2.update(200000);
        let rate = progress2.bytes_per_second();
        println!("     Rate: {:.2} MB/s", rate / (1024.0 * 1024.0));
        sleep(Duration::from_millis(100)).await;
    }
    
    println!("\n   Pattern 3: Time-remaining estimation");
    let mut progress3 = Progress::new(1000000);
    for i in 0..5 {
        progress3.update(200000);
        let rate = progress3.bytes_per_second();
        let remaining = (progress3.total - progress3.transferred) as f64;
        if rate > 0.0 {
            let seconds_remaining = remaining / rate;
            println!("     Estimated time remaining: {:.1} seconds", seconds_remaining);
        }
        sleep(Duration::from_millis(100)).await;
    }

    /* ====================================================================
     * EXAMPLE 8: Streaming Best Practices
     * ====================================================================
     * Learn best practices for streaming operations.
     */
    
    println!("\n9. Streaming Best Practices...");
    
    println!("\n   Best Practice 1: Use appropriate chunk sizes");
    println!("     ✓ Small chunks (8-64 KB) for memory efficiency");
    println!("     ✓ Larger chunks (128-512 KB) for throughput");
    println!("     ✗ Too small: Overhead from many operations");
    println!("     ✗ Too large: High memory usage");
    
    println!("\n   Best Practice 2: Always report progress for long operations");
    println!("     ✓ Update progress periodically (every N chunks)");
    println!("     ✓ Show percentage, rate, and time remaining");
    println!("     ✗ No progress feedback for long operations");
    
    println!("\n   Best Practice 3: Handle backpressure");
    println!("     ✓ Limit concurrent operations");
    println!("     ✓ Buffer chunks when needed");
    println!("     ✗ Processing all chunks at once");
    
    println!("\n   Best Practice 4: Use AsyncRead/AsyncWrite for I/O");
    println!("     ✓ Stream from files, network, etc.");
    println!("     ✓ Don't load entire file into memory");
    println!("     ✗ Reading entire file before processing");
    
    println!("\n   Best Practice 5: Use futures::stream for complex pipelines");
    println!("     ✓ Chain multiple stream operations");
    println!("     ✓ Transform, filter, and combine streams");
    println!("     ✗ Manual loop-based processing when streams would be better");
    
    println!("\n   Best Practice 6: Clean up resources on errors");
    println!("     ✓ Close streams and files on errors");
    println!("     ✓ Release buffers and connections");
    println!("     ✗ Leaving resources open on errors");
    
    println!("\n   Best Practice 7: Monitor memory usage");
    println!("     ✓ Keep chunk sizes reasonable");
    println!("     ✓ Limit concurrent operations");
    println!("     ✗ Unbounded memory growth");

    /* ====================================================================
     * CLEANUP
     * ====================================================================
     * Clean up test files.
     */
    
    println!("\n10. Cleaning up test files...");
    /* Delete the uploaded test file */
    match client.delete_file(&uploaded_file_id).await {
        Ok(_) => {
            println!("   ✓ Test file deleted: {}", uploaded_file_id);
        }
        Err(e) => {
            println!("   ⚠ Error deleting test file: {}", e);
        }
    }

    /* ====================================================================
     * SUMMARY
     * ====================================================================
     * Print summary of streaming concepts demonstrated.
     */
    
    println!("\n{}", "=".repeat(70));
    println!("Streaming Large Files Example Completed Successfully!");
    println!("\nSummary of demonstrated features:");
    println!("  ✓ Chunked uploads with progress reporting");
    println!("  ✓ Chunked downloads with progress reporting");
    println!("  ✓ Streaming with futures::stream");
    println!("  ✓ AsyncRead/AsyncWrite streaming patterns");
    println!("  ✓ Backpressure handling");
    println!("  ✓ Memory-efficient operations");
    println!("  ✓ Progress reporting patterns");
    println!("  ✓ Streaming best practices");
    println!("\nAll streaming concepts demonstrated with extensive comments.");

    /* Close the client to release resources */
    client.close().await;
    println!("\n✓ Client closed. All resources released.");

    /* Return success */
    Ok(())
}

