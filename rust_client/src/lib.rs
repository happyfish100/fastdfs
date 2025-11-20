//! FastDFS Rust Client Library
//!
//! Official Rust client for FastDFS distributed file system.
//! Provides a high-level, async, type-safe API for interacting with FastDFS servers.
//!
//! # Features
//!
//! - File upload (normal, appender, slave files)
//! - File download (full and partial)
//! - File deletion
//! - Metadata operations (set, get)
//! - Connection pooling
//! - Automatic failover
//! - Async/await support with Tokio
//! - Comprehensive error handling
//! - Thread-safe operations
//!
//! # Example
//!
//! ```no_run
//! use fastdfs::{Client, ClientConfig};
//!
//! #[tokio::main]
//! async fn main() -> Result<(), Box<dyn std::error::Error>> {
//!     let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()]);
//!     let client = Client::new(config)?;
//!     
//!     let file_id = client.upload_buffer(b"Hello, FastDFS!", "txt", None).await?;
//!     let data = client.download_file(&file_id).await?;
//!     client.delete_file(&file_id).await?;
//!     
//!     client.close().await;
//!     Ok(())
//! }
//! ```

#![warn(missing_docs)]
#![warn(rustdoc::missing_crate_level_docs)]

mod client;
mod connection;
mod errors;
mod operations;
mod protocol;
mod types;

// Re-export public API
pub use client::Client;
pub use errors::{FastDFSError, Result};
pub use types::{
    ClientConfig, FileInfo, Metadata, MetadataFlag, StorageCommand, StorageServer, StorageStatus,
    TrackerCommand,
};