//! FastDFS Operations
//!
//! This module implements all file operations (upload, download, delete, etc.)
//! for the FastDFS client.

use bytes::{Bytes, BytesMut, BufMut};
use std::sync::Arc;
use tokio::time::{sleep, Duration};

use crate::connection::ConnectionPool;
use crate::errors::{map_status_to_error, FastDFSError, Result};
use crate::protocol::*;
use crate::types::*;

/// Handles all FastDFS file operations
///
/// This struct is used internally by the Client.
pub struct Operations {
    tracker_pool: Arc<ConnectionPool>,
    storage_pool: Arc<ConnectionPool>,
    network_timeout: u64,
    retry_count: usize,
}

impl Operations {
    /// Creates a new Operations handler
    pub fn new(
        tracker_pool: Arc<ConnectionPool>,
        storage_pool: Arc<ConnectionPool>,
        network_timeout: u64,
        retry_count: usize,
    ) -> Self {
        Self {
            tracker_pool,
            storage_pool,
            network_timeout,
            retry_count,
        }
    }

    /// Uploads a file from the local filesystem
    pub async fn upload_file(
        &self,
        local_filename: &str,
        metadata: Option<&Metadata>,
        is_appender: bool,
    ) -> Result<String> {
        let file_data = read_file_content(local_filename)?;
        let ext_name = get_file_ext_name(local_filename);
        self.upload_buffer(&file_data, &ext_name, metadata, is_appender)
            .await
    }

    /// Uploads data from a buffer
    pub async fn upload_buffer(
        &self,
        data: &[u8],
        file_ext_name: &str,
        metadata: Option<&Metadata>,
        is_appender: bool,
    ) -> Result<String> {
        for attempt in 0..self.retry_count {
            match self
                .upload_buffer_internal(data, file_ext_name, metadata, is_appender)
                .await
            {
                Ok(file_id) => return Ok(file_id),
                Err(e) => {
                    if attempt == self.retry_count - 1 {
                        return Err(e);
                    }
                    sleep(Duration::from_secs((attempt + 1) as u64)).await;
                }
            }
        }
        Err(FastDFSError::InvalidArgument(
            "Upload failed after retries".to_string(),
        ))
    }

    /// Internal implementation of buffer upload
    async fn upload_buffer_internal(
        &self,
        data: &[u8],
        file_ext_name: &str,
        metadata: Option<&Metadata>,
        is_appender: bool,
    ) -> Result<String> {
        // Get storage server from tracker
        let storage_server = self.get_storage_server("").await?;

        // Get connection to storage server
        let storage_addr = format!("{}:{}", storage_server.ip_addr, storage_server.port);
        self.storage_pool.add_addr(storage_addr.clone()).await;
        let mut conn = self.storage_pool.get(Some(&storage_addr)).await?;

        // Prepare upload command
        let cmd = if is_appender {
            StorageCommand::UploadAppenderFile as u8
        } else {
            StorageCommand::UploadFile as u8
        };

        // Build request
        let ext_name_bytes = pad_string(file_ext_name, FDFS_FILE_EXT_NAME_MAX_LEN);
        let store_path_index = storage_server.store_path_index;

        let body_len = 1 + FDFS_FILE_EXT_NAME_MAX_LEN + data.len();
        let req_header = encode_header(body_len as u64, cmd, 0);

        // Send request
        conn.send(&req_header, self.network_timeout).await?;
        conn.send(&[store_path_index], self.network_timeout).await?;
        conn.send(&ext_name_bytes, self.network_timeout).await?;
        conn.send(data, self.network_timeout).await?;

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.storage_pool.put(conn).await;
                return Err(err);
            }
        }

        if resp_header.length == 0 {
            self.storage_pool.put(conn).await;
            return Err(FastDFSError::InvalidResponse("Empty response body".to_string()));
        }

        let resp_body = conn.receive_full(resp_header.length as usize, self.network_timeout).await?;

        // Parse response
        if resp_body.len() < FDFS_GROUP_NAME_MAX_LEN {
            self.storage_pool.put(conn).await;
            return Err(FastDFSError::InvalidResponse(
                "Response body too short".to_string(),
            ));
        }

        let group_name = unpad_string(&resp_body[..FDFS_GROUP_NAME_MAX_LEN]);
        let remote_filename = String::from_utf8_lossy(&resp_body[FDFS_GROUP_NAME_MAX_LEN..]).to_string();

        let file_id = join_file_id(&group_name, &remote_filename);

        // Return connection to pool
        self.storage_pool.put(conn).await;

        // Set metadata if provided
        if let Some(meta) = metadata {
            if !meta.is_empty() {
                let _ = self.set_metadata(&file_id, meta, MetadataFlag::Overwrite).await;
            }
        }

        Ok(file_id)
    }

    /// Gets a storage server from tracker for upload
    async fn get_storage_server(&self, group_name: &str) -> Result<StorageServer> {
        let mut conn = self.tracker_pool.get(None).await?;

        // Prepare request
        let (cmd, body_len) = if group_name.is_empty() {
            (TrackerCommand::ServiceQueryStoreWithoutGroupOne as u8, 0)
        } else {
            (TrackerCommand::ServiceQueryStoreWithGroupOne as u8, FDFS_GROUP_NAME_MAX_LEN)
        };

        let header = encode_header(body_len as u64, cmd, 0);
        conn.send(&header, self.network_timeout).await?;

        if !group_name.is_empty() {
            let group_name_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
            conn.send(&group_name_bytes, self.network_timeout).await?;
        }

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.tracker_pool.put(conn).await;
                return Err(err);
            }
        }

        if resp_header.length == 0 {
            self.tracker_pool.put(conn).await;
            return Err(FastDFSError::NoStorageServer);
        }

        let resp_body = conn.receive_full(resp_header.length as usize, self.network_timeout).await?;

        // Parse storage server info
        if resp_body.len() < FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 9 {
            self.tracker_pool.put(conn).await;
            return Err(FastDFSError::InvalidResponse(
                "Storage server response too short".to_string(),
            ));
        }

        let mut offset = FDFS_GROUP_NAME_MAX_LEN;
        let ip_addr = unpad_string(&resp_body[offset..offset + IP_ADDRESS_SIZE]);
        offset += IP_ADDRESS_SIZE;

        let port = decode_int64(&resp_body[offset..offset + 8]) as u16;
        offset += 8;

        let store_path_index = resp_body[offset];

        self.tracker_pool.put(conn).await;

        Ok(StorageServer {
            ip_addr,
            port,
            store_path_index,
        })
    }

    /// Downloads a file from FastDFS
    pub async fn download_file(&self, file_id: &str, offset: u64, length: u64) -> Result<Bytes> {
        for attempt in 0..self.retry_count {
            match self.download_file_internal(file_id, offset, length).await {
                Ok(data) => return Ok(data),
                Err(e) => {
                    if attempt == self.retry_count - 1 {
                        return Err(e);
                    }
                    sleep(Duration::from_secs((attempt + 1) as u64)).await;
                }
            }
        }
        Err(FastDFSError::InvalidArgument(
            "Download failed after retries".to_string(),
        ))
    }

    /// Internal implementation of file download
    async fn download_file_internal(&self, file_id: &str, offset: u64, length: u64) -> Result<Bytes> {
        let (group_name, remote_filename) = split_file_id(file_id)?;

        // Get storage server for download
        let storage_server = self.get_download_storage_server(&group_name, &remote_filename).await?;

        // Get connection
        let storage_addr = format!("{}:{}", storage_server.ip_addr, storage_server.port);
        self.storage_pool.add_addr(storage_addr.clone()).await;
        let mut conn = self.storage_pool.get(Some(&storage_addr)).await?;

        // Build request
        let remote_filename_bytes = remote_filename.as_bytes();
        let body_len = 16 + remote_filename_bytes.len();
        let header = encode_header(body_len as u64, StorageCommand::DownloadFile as u8, 0);

        let mut body = BytesMut::new();
        body.put(encode_int64(offset).as_ref());
        body.put(encode_int64(length).as_ref());
        body.put_slice(remote_filename_bytes);

        // Send request
        conn.send(&header, self.network_timeout).await?;
        conn.send(&body, self.network_timeout).await?;

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.storage_pool.put(conn).await;
                return Err(err);
            }
        }

        if resp_header.length == 0 {
            self.storage_pool.put(conn).await;
            return Ok(Bytes::new());
        }

        // Receive file data
        let data = conn.receive_full(resp_header.length as usize, self.network_timeout).await?;

        self.storage_pool.put(conn).await;

        Ok(data)
    }

    /// Gets a storage server from tracker for download
    async fn get_download_storage_server(
        &self,
        group_name: &str,
        remote_filename: &str,
    ) -> Result<StorageServer> {
        let mut conn = self.tracker_pool.get(None).await?;

        // Build request
        let remote_filename_bytes = remote_filename.as_bytes();
        let body_len = FDFS_GROUP_NAME_MAX_LEN + remote_filename_bytes.len();
        let header = encode_header(body_len as u64, TrackerCommand::ServiceQueryFetchOne as u8, 0);

        let mut body = BytesMut::new();
        body.put(pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN).as_ref());
        body.put_slice(remote_filename_bytes);

        // Send request
        conn.send(&header, self.network_timeout).await?;
        conn.send(&body, self.network_timeout).await?;

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.tracker_pool.put(conn).await;
                return Err(err);
            }
        }

        let resp_body = conn.receive_full(resp_header.length as usize, self.network_timeout).await?;

        // Parse response
        if resp_body.len() < FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 8 {
            self.tracker_pool.put(conn).await;
            return Err(FastDFSError::InvalidResponse(
                "Download storage server response too short".to_string(),
            ));
        }

        let mut offset = FDFS_GROUP_NAME_MAX_LEN;
        let ip_addr = unpad_string(&resp_body[offset..offset + IP_ADDRESS_SIZE]);
        offset += IP_ADDRESS_SIZE;

        let port = decode_int64(&resp_body[offset..offset + 8]) as u16;

        self.tracker_pool.put(conn).await;

        Ok(StorageServer {
            ip_addr,
            port,
            store_path_index: 0,
        })
    }

    /// Downloads a file and saves it to the local filesystem
    pub async fn download_to_file(&self, file_id: &str, local_filename: &str) -> Result<()> {
        let data = self.download_file(file_id, 0, 0).await?;
        write_file_content(local_filename, &data)?;
        Ok(())
    }

    /// Deletes a file from FastDFS
    pub async fn delete_file(&self, file_id: &str) -> Result<()> {
        for attempt in 0..self.retry_count {
            match self.delete_file_internal(file_id).await {
                Ok(()) => return Ok(()),
                Err(e) => {
                    if attempt == self.retry_count - 1 {
                        return Err(e);
                    }
                    sleep(Duration::from_secs((attempt + 1) as u64)).await;
                }
            }
        }
        Err(FastDFSError::InvalidArgument(
            "Delete failed after retries".to_string(),
        ))
    }

    /// Internal implementation of file deletion
    async fn delete_file_internal(&self, file_id: &str) -> Result<()> {
        let (group_name, remote_filename) = split_file_id(file_id)?;

        // Get storage server
        let storage_server = self.get_download_storage_server(&group_name, &remote_filename).await?;

        // Get connection
        let storage_addr = format!("{}:{}", storage_server.ip_addr, storage_server.port);
        self.storage_pool.add_addr(storage_addr.clone()).await;
        let mut conn = self.storage_pool.get(Some(&storage_addr)).await?;

        // Build request
        let remote_filename_bytes = remote_filename.as_bytes();
        let body_len = FDFS_GROUP_NAME_MAX_LEN + remote_filename_bytes.len();
        let header = encode_header(body_len as u64, StorageCommand::DeleteFile as u8, 0);

        let mut body = BytesMut::new();
        body.put(pad_string(&group_name, FDFS_GROUP_NAME_MAX_LEN).as_ref());
        body.put_slice(remote_filename_bytes);

        // Send request
        conn.send(&header, self.network_timeout).await?;
        conn.send(&body, self.network_timeout).await?;

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.storage_pool.put(conn).await;
                return Err(err);
            }
        }

        self.storage_pool.put(conn).await;
        Ok(())
    }

    /// Sets metadata for a file
    pub async fn set_metadata(
        &self,
        file_id: &str,
        metadata: &Metadata,
        flag: MetadataFlag,
    ) -> Result<()> {
        let (group_name, remote_filename) = split_file_id(file_id)?;

        // Get storage server
        let storage_server = self.get_download_storage_server(&group_name, &remote_filename).await?;

        // Get connection
        let storage_addr = format!("{}:{}", storage_server.ip_addr, storage_server.port);
        self.storage_pool.add_addr(storage_addr.clone()).await;
        let mut conn = self.storage_pool.get(Some(&storage_addr)).await?;

        // Encode metadata
        let metadata_bytes = encode_metadata(metadata);
        let remote_filename_bytes = remote_filename.as_bytes();

        // Build request
        let body_len = 2 * 8 + 1 + FDFS_GROUP_NAME_MAX_LEN + remote_filename_bytes.len() + metadata_bytes.len();
        let header = encode_header(body_len as u64, StorageCommand::SetMetadata as u8, 0);

        let mut body = BytesMut::new();
        body.put(encode_int64(remote_filename_bytes.len() as u64).as_ref());
        body.put(encode_int64(metadata_bytes.len() as u64).as_ref());
        body.put_u8(flag as u8);
        body.put(pad_string(&group_name, FDFS_GROUP_NAME_MAX_LEN).as_ref());
        body.put_slice(remote_filename_bytes);
        body.put(metadata_bytes.as_ref());

        // Send request
        conn.send(&header, self.network_timeout).await?;
        conn.send(&body, self.network_timeout).await?;

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.storage_pool.put(conn).await;
                return Err(err);
            }
        }

        self.storage_pool.put(conn).await;
        Ok(())
    }

    /// Retrieves metadata for a file
    pub async fn get_metadata(&self, file_id: &str) -> Result<Metadata> {
        let (group_name, remote_filename) = split_file_id(file_id)?;

        // Get storage server
        let storage_server = self.get_download_storage_server(&group_name, &remote_filename).await?;

        // Get connection
        let storage_addr = format!("{}:{}", storage_server.ip_addr, storage_server.port);
        self.storage_pool.add_addr(storage_addr.clone()).await;
        let mut conn = self.storage_pool.get(Some(&storage_addr)).await?;

        // Build request
        let remote_filename_bytes = remote_filename.as_bytes();
        let body_len = FDFS_GROUP_NAME_MAX_LEN + remote_filename_bytes.len();
        let header = encode_header(body_len as u64, StorageCommand::GetMetadata as u8, 0);

        let mut body = BytesMut::new();
        body.put(pad_string(&group_name, FDFS_GROUP_NAME_MAX_LEN).as_ref());
        body.put_slice(remote_filename_bytes);

        // Send request
        conn.send(&header, self.network_timeout).await?;
        conn.send(&body, self.network_timeout).await?;

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.storage_pool.put(conn).await;
                return Err(err);
            }
        }

        if resp_header.length == 0 {
            self.storage_pool.put(conn).await;
            return Ok(Metadata::new());
        }

        let resp_body = conn.receive_full(resp_header.length as usize, self.network_timeout).await?;

        // Decode metadata
        let metadata = decode_metadata(&resp_body)?;

        self.storage_pool.put(conn).await;
        Ok(metadata)
    }

    /// Retrieves file information
    pub async fn get_file_info(&self, file_id: &str) -> Result<FileInfo> {
        let (group_name, remote_filename) = split_file_id(file_id)?;

        // Get storage server
        let storage_server = self.get_download_storage_server(&group_name, &remote_filename).await?;

        // Get connection
        let storage_addr = format!("{}:{}", storage_server.ip_addr, storage_server.port);
        self.storage_pool.add_addr(storage_addr.clone()).await;
        let mut conn = self.storage_pool.get(Some(&storage_addr)).await?;

        // Build request
        let remote_filename_bytes = remote_filename.as_bytes();
        let body_len = FDFS_GROUP_NAME_MAX_LEN + remote_filename_bytes.len();
        let header = encode_header(body_len as u64, StorageCommand::QueryFileInfo as u8, 0);

        let mut body = BytesMut::new();
        body.put(pad_string(&group_name, FDFS_GROUP_NAME_MAX_LEN).as_ref());
        body.put_slice(remote_filename_bytes);

        // Send request
        conn.send(&header, self.network_timeout).await?;
        conn.send(&body, self.network_timeout).await?;

        // Receive response
        let resp_header_data = conn.receive_full(FDFS_PROTO_HEADER_LEN, self.network_timeout).await?;
        let resp_header = decode_header(&resp_header_data)?;

        if resp_header.status != 0 {
            if let Some(err) = map_status_to_error(resp_header.status) {
                self.storage_pool.put(conn).await;
                return Err(err);
            }
        }

        let resp_body = conn.receive_full(resp_header.length as usize, self.network_timeout).await?;

        // Parse file info
        if resp_body.len() < 8 + 8 + 4 + IP_ADDRESS_SIZE {
            self.storage_pool.put(conn).await;
            return Err(FastDFSError::InvalidResponse(
                "File info response too short".to_string(),
            ));
        }

        let file_size = decode_int64(&resp_body[0..8]);
        let create_timestamp = decode_int64(&resp_body[8..16]);
        let crc32 = decode_int32(&resp_body[16..20]);
        let source_ip = unpad_string(&resp_body[20..20 + IP_ADDRESS_SIZE]);

        let create_time = SystemTime::UNIX_EPOCH + std::time::Duration::from_secs(create_timestamp);

        self.storage_pool.put(conn).await;

        Ok(FileInfo {
            file_size,
            create_time,
            crc32,
            source_ip_addr: source_ip,
        })
    }
}