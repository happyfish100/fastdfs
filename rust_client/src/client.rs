//! FastDFS Rust Client
//!
//! Main client struct for interacting with FastDFS distributed file system.

use bytes::Bytes;
use std::sync::Arc;
use tokio::sync::RwLock;

use crate::connection::ConnectionPool;
use crate::errors::{FastDFSError, Result};
use crate::operations::Operations;
use crate::types::{ClientConfig, FileInfo, Metadata, MetadataFlag};

/// FastDFS client for file operations
///
/// This client provides a high-level, async Rust API for interacting with FastDFS servers.
/// It handles connection pooling, automatic retries, and error handling.
///
/// # Example
///
/// ```no_run
/// use fastdfs::{Client, ClientConfig};
///
/// #[tokio::main]
/// async fn main() -> Result<(), Box<dyn std::error::Error>> {
///     let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()]);
///     let client = Client::new(config)?;
///     
///     let file_id = client.upload_file("test.jpg", None).await?;
///     let data = client.download_file(&file_id).await?;
///     client.delete_file(&file_id).await?;
///     
///     client.close().await;
///     Ok(())
/// }
/// ```
pub struct Client {
    config: ClientConfig,
    tracker_pool: Arc<ConnectionPool>,
    storage_pool: Arc<ConnectionPool>,
    ops: Arc<Operations>,
    closed: Arc<RwLock<bool>>,
}

impl Client {
    /// Creates a new FastDFS client with the given configuration
    pub fn new(config: ClientConfig) -> Result<Self> {
        Self::validate_config(&config)?;

        let tracker_pool = Arc::new(ConnectionPool::new(
            config.tracker_addrs.clone(),
            config.max_conns,
            std::time::Duration::from_millis(config.connect_timeout),
            std::time::Duration::from_millis(config.idle_timeout),
        ));

        let storage_pool = Arc::new(ConnectionPool::new(
            Vec::new(), // Storage servers are discovered dynamically
            config.max_conns,
            std::time::Duration::from_millis(config.connect_timeout),
            std::time::Duration::from_millis(config.idle_timeout),
        ));

        let ops = Arc::new(Operations::new(
            tracker_pool.clone(),
            storage_pool.clone(),
            config.network_timeout,
            config.retry_count,
        ));

        Ok(Self {
            config,
            tracker_pool,
            storage_pool,
            ops,
            closed: Arc::new(RwLock::new(false)),
        })
    }

    /// Validates the client configuration
    fn validate_config(config: &ClientConfig) -> Result<()> {
        if config.tracker_addrs.is_empty() {
            return Err(FastDFSError::InvalidArgument(
                "Tracker addresses are required".to_string(),
            ));
        }

        for addr in &config.tracker_addrs {
            if addr.is_empty() || !addr.contains(':') {
                return Err(FastDFSError::InvalidArgument(format!(
                    "Invalid tracker address: {}",
                    addr
                )));
            }
        }

        Ok(())
    }

    /// Checks if the client is closed
    async fn check_closed(&self) -> Result<()> {
        let closed = self.closed.read().await;
        if *closed {
            return Err(FastDFSError::ClientClosed);
        }
        Ok(())
    }

    /// Uploads a file from the local filesystem to FastDFS
    pub async fn upload_file(&self, local_filename: &str, metadata: Option<&Metadata>) -> Result<String> {
        self.check_closed().await?;
        self.ops.upload_file(local_filename, metadata, false).await
    }

    /// Uploads data from a buffer to FastDFS
    pub async fn upload_buffer(
        &self,
        data: &[u8],
        file_ext_name: &str,
        metadata: Option<&Metadata>,
    ) -> Result<String> {
        self.check_closed().await?;
        self.ops.upload_buffer(data, file_ext_name, metadata, false).await
    }

    /// Uploads an appender file that can be modified later
    pub async fn upload_appender_file(
        &self,
        local_filename: &str,
        metadata: Option<&Metadata>,
    ) -> Result<String> {
        self.check_closed().await?;
        self.ops.upload_file(local_filename, metadata, true).await
    }

    /// Uploads an appender file from buffer
    pub async fn upload_appender_buffer(
        &self,
        data: &[u8],
        file_ext_name: &str,
        metadata: Option<&Metadata>,
    ) -> Result<String> {
        self.check_closed().await?;
        self.ops.upload_buffer(data, file_ext_name, metadata, true).await
    }

    /// Downloads a file from FastDFS and returns its content
    pub async fn download_file(&self, file_id: &str) -> Result<Bytes> {
        self.check_closed().await?;
        self.ops.download_file(file_id, 0, 0).await
    }

    /// Downloads a specific range of bytes from a file
    pub async fn download_file_range(&self, file_id: &str, offset: u64, length: u64) -> Result<Bytes> {
        self.check_closed().await?;
        self.ops.download_file(file_id, offset, length).await
    }

    /// Downloads a file and saves it to the local filesystem
    pub async fn download_to_file(&self, file_id: &str, local_filename: &str) -> Result<()> {
        self.check_closed().await?;
        self.ops.download_to_file(file_id, local_filename).await
    }

    /// Deletes a file from FastDFS
    pub async fn delete_file(&self, file_id: &str) -> Result<()> {
        self.check_closed().await?;
        self.ops.delete_file(file_id).await
    }

    /// Sets metadata for a file
    pub async fn set_metadata(
        &self,
        file_id: &str,
        metadata: &Metadata,
        flag: MetadataFlag,
    ) -> Result<()> {
        self.check_closed().await?;
        self.ops.set_metadata(file_id, metadata, flag).await
    }

    /// Retrieves metadata for a file
    pub async fn get_metadata(&self, file_id: &str) -> Result<Metadata> {
        self.check_closed().await?;
        self.ops.get_metadata(file_id).await
    }

    /// Retrieves file information including size, create time, and CRC32
    pub async fn get_file_info(&self, file_id: &str) -> Result<FileInfo> {
        self.check_closed().await?;
        self.ops.get_file_info(file_id).await
    }

    /// Checks if a file exists on the storage server
    pub async fn file_exists(&self, file_id: &str) -> bool {
        self.check_closed().await.is_ok() && self.ops.get_file_info(file_id).await.is_ok()
    }

    /// Closes the client and releases all resources
    ///
    /// After calling close, all operations will return ClientClosed error.
    /// It's safe to call close multiple times.
    pub async fn close(&self) {
        let mut closed = self.closed.write().await;
        if *closed {
            return;
        }
        *closed = true;
        drop(closed);

        self.tracker_pool.close().await;
        self.storage_pool.close().await;
    }
}