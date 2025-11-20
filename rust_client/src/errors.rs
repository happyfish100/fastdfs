//! FastDFS Error Definitions
//!
//! This module defines all error types and error handling utilities for the FastDFS client.
//! Errors are categorized into common errors, protocol errors, network errors, and server errors.

use thiserror::Error;

/// Result type alias for FastDFS operations
pub type Result<T> = std::result::Result<T, FastDFSError>;

/// Base error type for all FastDFS errors
#[derive(Error, Debug)]
pub enum FastDFSError {
    /// Client has been closed
    #[error("Client is closed")]
    ClientClosed,

    /// Requested file does not exist
    #[error("File not found: {0}")]
    FileNotFound(String),

    /// No storage server is available
    #[error("No storage server available")]
    NoStorageServer,

    /// Connection timeout
    #[error("Connection timeout to {0}")]
    ConnectionTimeout(String),

    /// Network I/O timeout
    #[error("Network timeout during {0}")]
    NetworkTimeout(String),

    /// File ID format is invalid
    #[error("Invalid file ID: {0}")]
    InvalidFileId(String),

    /// Server response is invalid
    #[error("Invalid response from server: {0}")]
    InvalidResponse(String),

    /// Storage server is offline
    #[error("Storage server is offline: {0}")]
    StorageServerOffline(String),

    /// Tracker server is offline
    #[error("Tracker server is offline: {0}")]
    TrackerServerOffline(String),

    /// Insufficient storage space
    #[error("Insufficient storage space")]
    InsufficientSpace,

    /// File already exists
    #[error("File already exists: {0}")]
    FileAlreadyExists(String),

    /// Invalid metadata format
    #[error("Invalid metadata: {0}")]
    InvalidMetadata(String),

    /// Operation is not supported
    #[error("Operation not supported: {0}")]
    OperationNotSupported(String),

    /// Invalid argument was provided
    #[error("Invalid argument: {0}")]
    InvalidArgument(String),

    /// Protocol-level error
    #[error("Protocol error (code {code}): {message}")]
    Protocol { code: u8, message: String },

    /// Network-related error
    #[error("Network error during {operation} to {addr}: {source}")]
    Network {
        operation: String,
        addr: String,
        #[source]
        source: std::io::Error,
    },

    /// I/O error
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),

    /// UTF-8 conversion error
    #[error("UTF-8 error: {0}")]
    Utf8(#[from] std::string::FromUtf8Error),
}

/// Maps FastDFS protocol status codes to Rust errors
///
/// Status code 0 indicates success (no error).
/// Other status codes are mapped to predefined errors or a Protocol error.
///
/// Common status codes:
///   - 0: Success
///   - 2: File not found (ENOENT)
///   - 6: File already exists (EEXIST)
///   - 22: Invalid argument (EINVAL)
///   - 28: Insufficient space (ENOSPC)
pub fn map_status_to_error(status: u8) -> Option<FastDFSError> {
    match status {
        0 => None,
        2 => Some(FastDFSError::FileNotFound(String::new())),
        6 => Some(FastDFSError::FileAlreadyExists(String::new())),
        22 => Some(FastDFSError::InvalidArgument(String::new())),
        28 => Some(FastDFSError::InsufficientSpace),
        _ => Some(FastDFSError::Protocol {
            code: status,
            message: format!("Unknown error code: {}", status),
        }),
    }
}