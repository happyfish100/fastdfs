//! Unit tests for the FastDFS client
//!
//! This test module verifies the client's behavior including configuration validation,
//! lifecycle management, and error handling for various edge cases.

use fastdfs::{Client, ClientConfig};

/// Test suite for client configuration
///
/// These tests verify that the client correctly validates configuration
/// and applies default values where appropriate.
#[cfg(test)]
mod config_tests {
    use super::*;

    /// Test creating client with valid configuration
    ///
    /// This test verifies that a client can be successfully created
    /// when provided with valid tracker addresses.
    #[test]
    fn test_client_creation_valid_config() {
        // Arrange: Create valid configuration
        let config = ClientConfig::new(vec!["127.0.0.1:22122".to_string()]);

        // Act: Create client
        let result = Client::new(config);

        // Assert: Verify client creation succeeds
        assert!(
            result.is_ok(),
            "Client should be created with valid config"
        );
    }

    /// Test creating client with empty tracker addresses
    ///
    /// This test verifies that the client properly rejects configurations
    /// with no tracker addresses specified.
    #[test]
    fn test_client_creation_empty_trackers() {
        // Arrange: Create config with empty tracker list
        let config = ClientConfig::new(vec![]);

        // Act: Attempt to create client
        let result = Client::new(config);

        // Assert: Verify creation fails with appropriate error
        assert!(
            result.is_err(),
            "Client creation should fail with empty tracker addresses"
        );
    }

    /// Test creating client with invalid tracker address format
    ///
    /// This test verifies that the client rejects tracker addresses
    /// that don't follow the "host:port" format.
    #[test]
    fn test_client_creation_invalid_address() {
        // Arrange: Create config with invalid address format
        let config = ClientConfig::new(vec!["invalid".to_string()]);

        // Act: Attempt to create client
        let result = Client::new(config);

        // Assert: Verify creation fails
        assert!(
            result.is_err(),
            "Client creation should fail with invalid address format"
        );
    }

    /// Test configuration builder pattern
    ///
    /// This test verifies that the configuration builder methods
    /// correctly set custom values for all configuration options.
    #[test]
    fn test_config_builder() {
        // Arrange & Act: Build config with custom values
        let config = ClientConfig::new(vec!["127.0.0.1:22122".to_string()])
            .with_max_conns(20)
            .with_connect_timeout(10000)
            .with_network_timeout(60000)
            .with_idle_timeout(120000)
            .with_retry_count(5);

        // Assert: Verify all custom values are set
        assert_eq!(config.max_conns, 20, "Max conns should be set to 20");
        assert_eq!(
            config.connect_timeout, 10000,
            "Connect timeout should be set to 10000"
        );
        assert_eq!(
            config.network_timeout, 60000,
            "Network timeout should be set to 60000"
        );
        assert_eq!(
            config.idle_timeout, 120000,
            "Idle timeout should be set to 120000"
        );
        assert_eq!(config.retry_count, 5, "Retry count should be set to 5");
    }

    /// Test default configuration values
    ///
    /// This test verifies that the default configuration values are
    /// correctly applied when not explicitly specified.
    #[test]
    fn test_config_defaults() {
        // Arrange & Act: Create config with only tracker addresses
        let config = ClientConfig::new(vec!["127.0.0.1:22122".to_string()]);

        // Assert: Verify default values are applied
        assert_eq!(config.max_conns, 10, "Default max_conns should be 10");
        assert_eq!(
            config.connect_timeout, 5000,
            "Default connect_timeout should be 5000ms"
        );
        assert_eq!(
            config.network_timeout, 30000,
            "Default network_timeout should be 30000ms"
        );
        assert_eq!(
            config.idle_timeout, 60000,
            "Default idle_timeout should be 60000ms"
        );
        assert_eq!(
            config.retry_count, 3,
            "Default retry_count should be 3"
        );
    }
}

/// Test suite for client lifecycle management
///
/// These tests verify that the client properly manages its lifecycle,
/// including initialization, operation, and shutdown.
#[cfg(test)]
mod lifecycle_tests {
    use super::*;

    /// Test closing the client
    ///
    /// This test verifies that the client can be closed and that
    /// subsequent operations fail with appropriate errors.
    #[tokio::test]
    async fn test_client_close() {
        // Arrange: Create client
        let config = ClientConfig::new(vec!["127.0.0.1:22122".to_string()]);
        let client = Client::new(config).unwrap();

        // Act: Close the client
        client.close().await;

        // Assert: Operations after close should fail
        let result = client.upload_buffer(b"test", "txt", None).await;
        assert!(
            result.is_err(),
            "Operations after close should return error"
        );
    }

    /// Test that close is idempotent
    ///
    /// This test verifies that calling close multiple times is safe
    /// and doesn't cause errors or panics.
    #[tokio::test]
    async fn test_client_close_idempotent() {
        // Arrange: Create client
        let config = ClientConfig::new(vec!["127.0.0.1:22122".to_string()]);
        let client = Client::new(config).unwrap();

        // Act: Close multiple times
        client.close().await;
        client.close().await;
        client.close().await;

        // Assert: No panic should occur (test passes if we reach here)
    }
}

/// Test suite for error handling
///
/// These tests verify that the client properly handles various error
/// conditions and returns appropriate error types.
#[cfg(test)]
mod error_tests {
    use super::*;

    /// Test file ID validation
    ///
    /// This test verifies that operations with invalid file IDs
    /// fail with InvalidFileID errors.
    #[tokio::test]
    async fn test_invalid_file_id() {
        // Arrange: Create client
        let config = ClientConfig::new(vec!["127.0.0.1:22122".to_string()]);
        let client = Client::new(config).unwrap();

        // Act: Attempt to download with invalid file ID
        let result = client.download_file("invalid").await;

        // Assert: Should fail with InvalidFileID error
        assert!(result.is_err(), "Invalid file ID should cause error");

        // Cleanup
        client.close().await;
    }
}