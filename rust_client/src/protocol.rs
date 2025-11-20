//! FastDFS Protocol Encoding and Decoding
//!
//! This module handles all protocol-level encoding and decoding operations
//! for communication with FastDFS servers.

use bytes::{Buf, BufMut, Bytes, BytesMut};
use std::collections::HashMap;
use std::path::Path;

use crate::errors::{FastDFSError, Result};
use crate::types::*;

/// Encodes a FastDFS protocol header into a 10-byte buffer
///
/// The header format is:
///   - Bytes 0-7: Body length (8 bytes, big-endian uint64)
///   - Byte 8: Command code
///   - Byte 9: Status code (0 for request, error code for response)
pub fn encode_header(length: u64, cmd: u8, status: u8) -> Bytes {
    let mut buf = BytesMut::with_capacity(FDFS_PROTO_HEADER_LEN);
    buf.put_u64(length);
    buf.put_u8(cmd);
    buf.put_u8(status);
    buf.freeze()
}

/// Decodes a FastDFS protocol header from a buffer
///
/// The header must be exactly 10 bytes long.
pub fn decode_header(data: &[u8]) -> Result<TrackerHeader> {
    if data.len() < FDFS_PROTO_HEADER_LEN {
        return Err(FastDFSError::InvalidResponse(format!(
            "Header too short: {} bytes",
            data.len()
        )));
    }

    let mut buf = &data[..FDFS_PROTO_HEADER_LEN];
    let length = buf.get_u64();
    let cmd = buf.get_u8();
    let status = buf.get_u8();

    Ok(TrackerHeader { length, cmd, status })
}

/// Splits a FastDFS file ID into its components
///
/// A file ID has the format: "groupName/path/to/file"
/// For example: "group1/M00/00/00/wKgBcFxyz.jpg"
pub fn split_file_id(file_id: &str) -> Result<(String, String)> {
    if file_id.is_empty() {
        return Err(FastDFSError::InvalidFileId(file_id.to_string()));
    }

    let parts: Vec<&str> = file_id.splitn(2, '/').collect();
    if parts.len() != 2 {
        return Err(FastDFSError::InvalidFileId(file_id.to_string()));
    }

    let group_name = parts[0];
    let remote_filename = parts[1];

    if group_name.is_empty() || group_name.len() > FDFS_GROUP_NAME_MAX_LEN {
        return Err(FastDFSError::InvalidFileId(file_id.to_string()));
    }

    if remote_filename.is_empty() {
        return Err(FastDFSError::InvalidFileId(file_id.to_string()));
    }

    Ok((group_name.to_string(), remote_filename.to_string()))
}

/// Constructs a complete file ID from its components
///
/// This is the inverse operation of split_file_id.
pub fn join_file_id(group_name: &str, remote_filename: &str) -> String {
    format!("{}/{}", group_name, remote_filename)
}

/// Encodes metadata key-value pairs into FastDFS wire format
///
/// The format uses special separators:
///   - Field separator (0x02) between key and value
///   - Record separator (0x01) between different key-value pairs
///
/// Format: key1<0x02>value1<0x01>key2<0x02>value2<0x01>
///
/// Keys are truncated to 64 bytes and values to 256 bytes if they exceed limits.
pub fn encode_metadata(metadata: &Metadata) -> Bytes {
    if metadata.is_empty() {
        return Bytes::new();
    }

    let mut buf = BytesMut::new();

    for (key, value) in metadata {
        let key_bytes = key.as_bytes();
        let value_bytes = value.as_bytes();

        // Truncate if necessary
        let key_len = key_bytes.len().min(FDFS_MAX_META_NAME_LEN);
        let value_len = value_bytes.len().min(FDFS_MAX_META_VALUE_LEN);

        buf.put_slice(&key_bytes[..key_len]);
        buf.put_u8(FDFS_FIELD_SEPARATOR);
        buf.put_slice(&value_bytes[..value_len]);
        buf.put_u8(FDFS_RECORD_SEPARATOR);
    }

    buf.freeze()
}

/// Decodes FastDFS wire format metadata into a HashMap
///
/// This is the inverse operation of encode_metadata.
///
/// The function parses records separated by 0x01 and fields separated by 0x02.
/// Invalid records (not exactly 2 fields) are silently skipped.
pub fn decode_metadata(data: &[u8]) -> Result<Metadata> {
    if data.is_empty() {
        return Ok(HashMap::new());
    }

    let mut metadata = HashMap::new();
    let records: Vec<&[u8]> = data.split(|&b| b == FDFS_RECORD_SEPARATOR).collect();

    for record in records {
        if record.is_empty() {
            continue;
        }

        let fields: Vec<&[u8]> = record.split(|&b| b == FDFS_FIELD_SEPARATOR).collect();
        if fields.len() != 2 {
            continue;
        }

        let key = String::from_utf8_lossy(fields[0]).to_string();
        let value = String::from_utf8_lossy(fields[1]).to_string();
        metadata.insert(key, value);
    }

    Ok(metadata)
}

/// Extracts and validates the file extension from a filename
///
/// The extension is extracted without the leading dot and truncated to 6 characters
/// if it exceeds the FastDFS maximum.
///
/// Examples:
///   - "test.jpg" -> "jpg"
///   - "file.tar.gz" -> "gz"
///   - "noext" -> ""
///   - "file.verylongext" -> "verylo" (truncated)
pub fn get_file_ext_name(filename: &str) -> String {
    let path = Path::new(filename);
    let ext = path
        .extension()
        .and_then(|s| s.to_str())
        .unwrap_or("")
        .to_string();

    if ext.len() > FDFS_FILE_EXT_NAME_MAX_LEN {
        ext[..FDFS_FILE_EXT_NAME_MAX_LEN].to_string()
    } else {
        ext
    }
}

/// Reads the entire contents of a file from the filesystem
pub fn read_file_content(filename: &str) -> Result<Bytes> {
    let data = std::fs::read(filename)?;
    Ok(Bytes::from(data))
}

/// Writes data to a file, creating parent directories if needed
///
/// If the file already exists, it will be truncated.
pub fn write_file_content(filename: &str, data: &[u8]) -> Result<()> {
    let path = Path::new(filename);
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    std::fs::write(filename, data)?;
    Ok(())
}

/// Pads a string to a fixed length with null bytes (0x00)
///
/// This is used to create fixed-width fields in the FastDFS protocol.
/// If the string is longer than length, it will be truncated.
pub fn pad_string(s: &str, length: usize) -> Bytes {
    let mut buf = BytesMut::with_capacity(length);
    let bytes = s.as_bytes();
    let copy_len = bytes.len().min(length);
    buf.put_slice(&bytes[..copy_len]);
    buf.resize(length, 0);
    buf.freeze()
}

/// Removes trailing null bytes from a byte slice
///
/// This is the inverse of pad_string, used to extract strings from
/// fixed-width protocol fields.
pub fn unpad_string(data: &[u8]) -> String {
    let end = data.iter().rposition(|&b| b != 0).map(|i| i + 1).unwrap_or(0);
    String::from_utf8_lossy(&data[..end]).to_string()
}

/// Encodes a 64-bit integer to an 8-byte big-endian representation
///
/// FastDFS protocol uses big-endian byte order for all numeric fields.
pub fn encode_int64(n: u64) -> Bytes {
    let mut buf = BytesMut::with_capacity(8);
    buf.put_u64(n);
    buf.freeze()
}

/// Decodes an 8-byte big-endian representation to a 64-bit integer
///
/// This is the inverse of encode_int64.
pub fn decode_int64(data: &[u8]) -> u64 {
    if data.len() < 8 {
        return 0;
    }
    let mut buf = &data[..8];
    buf.get_u64()
}

/// Encodes a 32-bit integer to a 4-byte big-endian representation
pub fn encode_int32(n: u32) -> Bytes {
    let mut buf = BytesMut::with_capacity(4);
    buf.put_u32(n);
    buf.freeze()
}

/// Decodes a 4-byte big-endian representation to a 32-bit integer
pub fn decode_int32(data: &[u8]) -> u32 {
    if data.len() < 4 {
        return 0;
    }
    let mut buf = &data[..4];
    buf.get_u32()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_decode_header() {
        let length = 1024;
        let cmd = 11;
        let status = 0;

        let encoded = encode_header(length, cmd, status);
        assert_eq!(encoded.len(), FDFS_PROTO_HEADER_LEN);

        let decoded = decode_header(&encoded).unwrap();
        assert_eq!(decoded.length, length);
        assert_eq!(decoded.cmd, cmd);
        assert_eq!(decoded.status, status);
    }

    #[test]
    fn test_split_file_id() {
        let file_id = "group1/M00/00/00/test.jpg";
        let (group_name, remote_filename) = split_file_id(file_id).unwrap();

        assert_eq!(group_name, "group1");
        assert_eq!(remote_filename, "M00/00/00/test.jpg");
    }

    #[test]
    fn test_join_file_id() {
        let group_name = "group1";
        let remote_filename = "M00/00/00/test.jpg";

        let file_id = join_file_id(group_name, remote_filename);
        assert_eq!(file_id, "group1/M00/00/00/test.jpg");
    }

    #[test]
    fn test_encode_decode_metadata() {
        let mut metadata = HashMap::new();
        metadata.insert("author".to_string(), "John Doe".to_string());
        metadata.insert("date".to_string(), "2025-01-15".to_string());

        let encoded = encode_metadata(&metadata);
        assert!(!encoded.is_empty());

        let decoded = decode_metadata(&encoded).unwrap();
        assert_eq!(decoded.len(), metadata.len());
        assert_eq!(decoded.get("author"), Some(&"John Doe".to_string()));
    }

    #[test]
    fn test_get_file_ext_name() {
        assert_eq!(get_file_ext_name("test.jpg"), "jpg");
        assert_eq!(get_file_ext_name("file.tar.gz"), "gz");
        assert_eq!(get_file_ext_name("noext"), "");
    }

    #[test]
    fn test_pad_unpad_string() {
        let test_str = "test";
        let length = 16;

        let padded = pad_string(test_str, length);
        assert_eq!(padded.len(), length);

        let unpadded = unpad_string(&padded);
        assert_eq!(unpadded, test_str);
    }
}