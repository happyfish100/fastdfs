//! Unit tests for protocol encoding and decoding functions
//!
//! This test module verifies the correctness of all protocol-level operations
//! including header encoding/decoding, file ID parsing, metadata encoding,
//! and various utility functions used in FastDFS protocol communication.

use fastdfs::*;
use std::collections::HashMap;

/// Test suite for header encoding and decoding operations
///
/// These tests verify that protocol headers are correctly encoded to the
/// 10-byte wire format and can be decoded back to their original values.
#[cfg(test)]
mod header_tests {
    use super::*;

    /// Test that a header can be encoded and decoded correctly
    ///
    /// This test creates a header with specific values, encodes it to bytes,
    /// then decodes it back and verifies all fields match the original values.
    #[test]
    fn test_encode_decode_header() {
        // Arrange: Create test values for header fields
        let length = 1024u64;
        let cmd = 11u8;
        let status = 0u8;

        // Act: Encode the header to bytes
        let encoded = fastdfs::protocol::encode_header(length, cmd, status);

        // Assert: Verify encoded length is correct
        assert_eq!(
            encoded.len(),
            fastdfs::types::FDFS_PROTO_HEADER_LEN,
            "Encoded header should be exactly 10 bytes"
        );

        // Act: Decode the header back
        let decoded = fastdfs::protocol::decode_header(&encoded).unwrap();

        // Assert: Verify all fields match original values
        assert_eq!(
            decoded.length, length,
            "Decoded length should match original"
        );
        assert_eq!(decoded.cmd, cmd, "Decoded cmd should match original");
        assert_eq!(
            decoded.status, status,
            "Decoded status should match original"
        );
    }

    /// Test that decoding fails with insufficient data
    ///
    /// This test verifies that the decoder properly rejects headers
    /// that are shorter than the required 10 bytes.
    #[test]
    fn test_decode_header_short_data() {
        // Arrange: Create data that is too short to be a valid header
        let short_data = b"short";

        // Act & Assert: Decoding should fail with InvalidResponse error
        let result = fastdfs::protocol::decode_header(short_data);
        assert!(
            result.is_err(),
            "Decoding short data should return an error"
        );
    }

    /// Test header encoding with maximum values
    ///
    /// This test verifies that the encoder can handle maximum possible values
    /// for all header fields without overflow or truncation.
    #[test]
    fn test_encode_header_max_values() {
        // Arrange: Use maximum values for all fields
        let length = u64::MAX;
        let cmd = u8::MAX;
        let status = u8::MAX;

        // Act: Encode with maximum values
        let encoded = fastdfs::protocol::encode_header(length, cmd, status);

        // Assert: Verify encoding succeeds and has correct length
        assert_eq!(encoded.len(), fastdfs::types::FDFS_PROTO_HEADER_LEN);

        // Act: Decode back
        let decoded = fastdfs::protocol::decode_header(&encoded).unwrap();

        // Assert: Verify values are preserved
        assert_eq!(decoded.length, length);
        assert_eq!(decoded.cmd, cmd);
        assert_eq!(decoded.status, status);
    }

    /// Test header encoding with zero values
    ///
    /// This test verifies that zero values are handled correctly,
    /// which is important for certain protocol messages.
    #[test]
    fn test_encode_header_zero_values() {
        // Arrange: Use zero for all fields
        let length = 0u64;
        let cmd = 0u8;
        let status = 0u8;

        // Act: Encode with zero values
        let encoded = fastdfs::protocol::encode_header(length, cmd, status);

        // Assert: Verify encoding succeeds
        assert_eq!(encoded.len(), fastdfs::types::FDFS_PROTO_HEADER_LEN);

        // Act: Decode back
        let decoded = fastdfs::protocol::decode_header(&encoded).unwrap();

        // Assert: Verify zero values are preserved
        assert_eq!(decoded.length, 0);
        assert_eq!(decoded.cmd, 0);
        assert_eq!(decoded.status, 0);
    }
}

/// Test suite for file ID operations
///
/// These tests verify that file IDs can be correctly split into components
/// and reconstructed, which is essential for routing requests to the correct
/// storage servers.
#[cfg(test)]
mod file_id_tests {
    use super::*;

    /// Test splitting a valid file ID
    ///
    /// This test verifies that a properly formatted file ID can be split
    /// into its group name and remote filename components.
    #[test]
    fn test_split_file_id_valid() {
        // Arrange: Create a valid file ID
        let file_id = "group1/M00/00/00/test.jpg";

        // Act: Split the file ID
        let (group_name, remote_filename) = fastdfs::protocol::split_file_id(file_id).unwrap();

        // Assert: Verify components are correct
        assert_eq!(group_name, "group1", "Group name should be extracted correctly");
        assert_eq!(
            remote_filename, "M00/00/00/test.jpg",
            "Remote filename should be extracted correctly"
        );
    }

    /// Test splitting invalid file IDs
    ///
    /// This test verifies that various invalid file ID formats are properly
    /// rejected with appropriate errors.
    #[test]
    fn test_split_file_id_invalid() {
        // Arrange: Create a list of invalid file IDs
        let invalid_ids = vec![
            "",                                          // Empty string
            "group1",                                    // No separator
            "/M00/00/00/test.jpg",                      // Empty group name
            "group1/",                                   // Empty filename
            "verylonggroupname123/M00/00/00/test.jpg", // Group name too long
        ];

        // Act & Assert: Each invalid ID should fail to split
        for file_id in invalid_ids {
            let result = fastdfs::protocol::split_file_id(file_id);
            assert!(
                result.is_err(),
                "Invalid file ID '{}' should fail to split",
                file_id
            );
        }
    }

    /// Test joining file ID components
    ///
    /// This test verifies that group name and remote filename can be
    /// correctly joined to form a complete file ID.
    #[test]
    fn test_join_file_id() {
        // Arrange: Create file ID components
        let group_name = "group1";
        let remote_filename = "M00/00/00/test.jpg";

        // Act: Join the components
        let file_id = fastdfs::protocol::join_file_id(group_name, remote_filename);

        // Assert: Verify the joined file ID is correct
        assert_eq!(
            file_id, "group1/M00/00/00/test.jpg",
            "Joined file ID should have correct format"
        );
    }

    /// Test round-trip file ID operations
    ///
    /// This test verifies that splitting and joining are inverse operations,
    /// ensuring data integrity throughout the process.
    #[test]
    fn test_file_id_round_trip() {
        // Arrange: Create an original file ID
        let original_file_id = "group1/M00/00/00/test.jpg";

        // Act: Split and then join
        let (group_name, remote_filename) = fastdfs::protocol::split_file_id(original_file_id).unwrap();
        let reconstructed_file_id = fastdfs::protocol::join_file_id(&group_name, &remote_filename);

        // Assert: Verify we get back the original file ID
        assert_eq!(
            reconstructed_file_id, original_file_id,
            "Round-trip should preserve file ID"
        );
    }
}

/// Test suite for metadata encoding and decoding
///
/// These tests verify that metadata key-value pairs can be correctly encoded
/// to the FastDFS wire format and decoded back to their original values.
#[cfg(test)]
mod metadata_tests {
    use super::*;

    /// Test encoding and decoding metadata
    ///
    /// This test verifies that metadata can be encoded to bytes and decoded
    /// back to the original key-value pairs without data loss.
    #[test]
    fn test_encode_decode_metadata() {
        // Arrange: Create test metadata
        let mut metadata = HashMap::new();
        metadata.insert("author".to_string(), "John Doe".to_string());
        metadata.insert("date".to_string(), "2025-01-15".to_string());
        metadata.insert("version".to_string(), "1.0".to_string());

        // Act: Encode the metadata
        let encoded = fastdfs::protocol::encode_metadata(&metadata);

        // Assert: Verify encoded data is not empty
        assert!(
            !encoded.is_empty(),
            "Encoded metadata should not be empty"
        );

        // Act: Decode the metadata back
        let decoded = fastdfs::protocol::decode_metadata(&encoded).unwrap();

        // Assert: Verify all key-value pairs are preserved
        assert_eq!(
            decoded.len(),
            metadata.len(),
            "Decoded metadata should have same number of entries"
        );
        for (key, value) in &metadata {
            assert_eq!(
                decoded.get(key),
                Some(value),
                "Decoded metadata should contain key '{}' with correct value",
                key
            );
        }
    }

    /// Test encoding empty metadata
    ///
    /// This test verifies that empty metadata is handled correctly
    /// and produces an empty byte array.
    #[test]
    fn test_encode_metadata_empty() {
        // Arrange: Create empty metadata
        let metadata = HashMap::new();

        // Act: Encode empty metadata
        let encoded = fastdfs::protocol::encode_metadata(&metadata);

        // Assert: Verify result is empty
        assert!(
            encoded.is_empty(),
            "Encoded empty metadata should be empty"
        );
    }

    /// Test decoding empty metadata
    ///
    /// This test verifies that empty byte arrays are correctly decoded
    /// to empty metadata maps.
    #[test]
    fn test_decode_metadata_empty() {
        // Arrange: Create empty byte array
        let empty_data = &[];

        // Act: Decode empty data
        let decoded = fastdfs::protocol::decode_metadata(empty_data).unwrap();

        // Assert: Verify result is empty
        assert!(
            decoded.is_empty(),
            "Decoded empty data should produce empty metadata"
        );
    }

    /// Test metadata with special characters
    ///
    /// This test verifies that metadata values containing special characters
    /// are correctly encoded and decoded without corruption.
    #[test]
    fn test_metadata_with_special_chars() {
        // Arrange: Create metadata with special characters
        let mut metadata = HashMap::new();
        metadata.insert("path".to_string(), "/home/user/file.txt".to_string());
        metadata.insert("description".to_string(), "Test: with, special; chars!".to_string());

        // Act: Encode and decode
        let encoded = fastdfs::protocol::encode_metadata(&metadata);
        let decoded = fastdfs::protocol::decode_metadata(&encoded).unwrap();

        // Assert: Verify special characters are preserved
        assert_eq!(decoded.len(), metadata.len());
        for (key, value) in &metadata {
            assert_eq!(decoded.get(key), Some(value));
        }
    }

    /// Test metadata truncation for long values
    ///
    /// This test verifies that metadata keys and values that exceed
    /// the maximum allowed lengths are properly truncated.
    #[test]
    fn test_metadata_truncation() {
        // Arrange: Create metadata with very long key and value
        let mut metadata = HashMap::new();
        let long_key = "a".repeat(100); // Exceeds 64 byte limit
        let long_value = "b".repeat(300); // Exceeds 256 byte limit
        metadata.insert(long_key.clone(), long_value.clone());

        // Act: Encode the metadata
        let encoded = fastdfs::protocol::encode_metadata(&metadata);

        // Assert: Verify encoding succeeds (truncation happens silently)
        assert!(!encoded.is_empty());

        // Act: Decode back
        let decoded = fastdfs::protocol::decode_metadata(&encoded).unwrap();

        // Assert: Verify we have one entry (though truncated)
        assert_eq!(decoded.len(), 1, "Should have one metadata entry");
    }
}

/// Test suite for file extension extraction
///
/// These tests verify that file extensions are correctly extracted from
/// filenames and properly truncated if they exceed the maximum length.
#[cfg(test)]
mod extension_tests {
    use super::*;

    /// Test extracting various file extensions
    ///
    /// This test verifies that common file extensions are correctly
    /// extracted from different filename formats.
    #[test]
    fn test_get_file_ext_name() {
        // Arrange & Act & Assert: Test various filename formats
        assert_eq!(
            fastdfs::protocol::get_file_ext_name("test.jpg"),
            "jpg",
            "Should extract 'jpg' extension"
        );
        assert_eq!(
            fastdfs::protocol::get_file_ext_name("file.tar.gz"),
            "gz",
            "Should extract last extension from multi-dot filename"
        );
        assert_eq!(
            fastdfs::protocol::get_file_ext_name("noext"),
            "",
            "Should return empty string for files without extension"
        );
        assert_eq!(
            fastdfs::protocol::get_file_ext_name(".hidden"),
            "hidden",
            "Should extract extension from hidden files"
        );
    }

    /// Test extension truncation
    ///
    /// This test verifies that file extensions longer than 6 characters
    /// are properly truncated to meet FastDFS protocol requirements.
    #[test]
    fn test_get_file_ext_name_truncation() {
        // Arrange: Create filename with very long extension
        let filename = "file.verylongextension";

        // Act: Extract extension
        let ext = fastdfs::protocol::get_file_ext_name(filename);

        // Assert: Verify extension is truncated to 6 characters
        assert_eq!(
            ext.len(),
            6,
            "Extension should be truncated to 6 characters"
        );
        assert_eq!(ext, "verylo", "Truncated extension should be 'verylo'");
    }

    /// Test extension extraction from paths
    ///
    /// This test verifies that extensions are correctly extracted even
    /// when the filename includes directory paths.
    #[test]
    fn test_get_file_ext_name_with_path() {
        // Arrange: Create filename with directory path
        let filename = "/path/to/file.txt";

        // Act: Extract extension
        let ext = fastdfs::protocol::get_file_ext_name(filename);

        // Assert: Verify extension is extracted correctly
        assert_eq!(ext, "txt", "Should extract extension from full path");
    }
}

/// Test suite for string padding and unpadding operations
///
/// These tests verify that strings can be correctly padded to fixed lengths
/// with null bytes and that the padding can be removed to recover the original string.
#[cfg(test)]
mod padding_tests {
    use super::*;

    /// Test padding and unpadding a string
    ///
    /// This test verifies that a string can be padded to a fixed length
    /// and then unpadded to recover the original string.
    #[test]
    fn test_pad_unpad_string() {
        // Arrange: Create test string and target length
        let test_string = "test";
        let length = 16;

        // Act: Pad the string
        let padded = fastdfs::protocol::pad_string(test_string, length);

        // Assert: Verify padded length is correct
        assert_eq!(
            padded.len(),
            length,
            "Padded string should have exact target length"
        );

        // Act: Unpad the string
        let unpadded = fastdfs::protocol::unpad_string(&padded);

        // Assert: Verify original string is recovered
        assert_eq!(
            unpadded, test_string,
            "Unpadded string should match original"
        );
    }

    /// Test padding with truncation
    ///
    /// This test verifies that strings longer than the target length
    /// are properly truncated during padding.
    #[test]
    fn test_pad_string_truncate() {
        // Arrange: Create string longer than target length
        let test_string = "verylongstringthatexceedslength";
        let length = 10;

        // Act: Pad the string (should truncate)
        let padded = fastdfs::protocol::pad_string(test_string, length);

        // Assert: Verify result has exact target length
        assert_eq!(
            padded.len(),
            length,
            "Padded string should be truncated to target length"
        );
    }

    /// Test padding empty string
    ///
    /// This test verifies that empty strings are correctly padded
    /// to produce a buffer filled entirely with null bytes.
    #[test]
    fn test_pad_empty_string() {
        // Arrange: Create empty string
        let test_string = "";
        let length = 16;

        // Act: Pad the empty string
        let padded = fastdfs::protocol::pad_string(test_string, length);

        // Assert: Verify result is all null bytes
        assert_eq!(padded.len(), length);
        assert!(
            padded.iter().all(|&b| b == 0),
            "Padded empty string should be all null bytes"
        );
    }

    /// Test unpadding string with no padding
    ///
    /// This test verifies that strings without trailing null bytes
    /// are returned unchanged by the unpad operation.
    #[test]
    fn test_unpad_string_no_padding() {
        // Arrange: Create buffer with no null bytes
        let data = b"test";

        // Act: Unpad the data
        let unpadded = fastdfs::protocol::unpad_string(data);

        // Assert: Verify string is unchanged
        assert_eq!(unpadded, "test", "String without padding should be unchanged");
    }
}

/// Test suite for integer encoding and decoding
///
/// These tests verify that integers can be correctly encoded to big-endian
/// byte representation and decoded back to their original values.
#[cfg(test)]
mod integer_tests {
    use super::*;

    /// Test encoding and decoding 64-bit integers
    ///
    /// This test verifies that various 64-bit integer values can be
    /// correctly encoded and decoded without data loss.
    #[test]
    fn test_encode_decode_int64() {
        // Arrange: Create test values covering different ranges
        let test_values = vec![
            0u64,
            1u64,
            1024u64,
            u32::MAX as u64,
            u64::MAX,
        ];

        // Act & Assert: Test each value
        for value in test_values {
            // Act: Encode the value
            let encoded = fastdfs::protocol::encode_int64(value);

            // Assert: Verify encoded length is 8 bytes
            assert_eq!(
                encoded.len(),
                8,
                "Encoded int64 should be 8 bytes for value {}",
                value
            );

            // Act: Decode the value
            let decoded = fastdfs::protocol::decode_int64(&encoded);

            // Assert: Verify decoded value matches original
            assert_eq!(
                decoded, value,
                "Decoded value should match original for {}",
                value
            );
        }
    }

    /// Test decoding int64 from short data
    ///
    /// This test verifies that attempting to decode an int64 from
    /// insufficient data returns zero rather than panicking.
    #[test]
    fn test_decode_int64_short_data() {
        // Arrange: Create data shorter than 8 bytes
        let short_data = b"short";

        // Act: Attempt to decode
        let result = fastdfs::protocol::decode_int64(short_data);

        // Assert: Verify result is zero (safe default)
        assert_eq!(result, 0, "Decoding short data should return 0");
    }

    /// Test encoding and decoding 32-bit integers
    ///
    /// This test verifies that 32-bit integers are correctly encoded
    /// and decoded, which is used for CRC32 values in the protocol.
    #[test]
    fn test_encode_decode_int32() {
        // Arrange: Create test values
        let test_values = vec![0u32, 1u32, 1024u32, u32::MAX];

        // Act & Assert: Test each value
        for value in test_values {
            // Act: Encode the value
            let encoded = fastdfs::protocol::encode_int32(value);

            // Assert: Verify encoded length is 4 bytes
            assert_eq!(
                encoded.len(),
                4,
                "Encoded int32 should be 4 bytes for value {}",
                value
            );

            // Act: Decode the value
            let decoded = fastdfs::protocol::decode_int32(&encoded);

            // Assert: Verify decoded value matches original
            assert_eq!(
                decoded, value,
                "Decoded value should match original for {}",
                value
            );
        }
    }

    /// Test decoding int32 from short data
    ///
    /// This test verifies safe handling of insufficient data when
    /// decoding 32-bit integers.
    #[test]
    fn test_decode_int32_short_data() {
        // Arrange: Create data shorter than 4 bytes
        let short_data = b"ab";

        // Act: Attempt to decode
        let result = fastdfs::protocol::decode_int32(short_data);

        // Assert: Verify result is zero (safe default)
        assert_eq!(result, 0, "Decoding short data should return 0");
    }
}