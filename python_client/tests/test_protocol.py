"""
Unit tests for protocol encoding and decoding functions.
"""

import unittest
from fdfs.protocol import (
    encode_header,
    decode_header,
    split_file_id,
    join_file_id,
    encode_metadata,
    decode_metadata,
    get_file_ext_name,
    pad_string,
    unpad_string,
    encode_int64,
    decode_int64,
)
from fdfs.errors import InvalidFileIDError, InvalidResponseError


class TestProtocol(unittest.TestCase):
    """Test protocol encoding and decoding functions."""
    
    def test_encode_decode_header(self):
        """Test header encoding and decoding."""
        length = 1024
        cmd = 11
        status = 0
        
        encoded = encode_header(length, cmd, status)
        self.assertEqual(len(encoded), 10)
        
        decoded = decode_header(encoded)
        self.assertEqual(decoded.length, length)
        self.assertEqual(decoded.cmd, cmd)
        self.assertEqual(decoded.status, status)
    
    def test_decode_header_short_data(self):
        """Test decoding header with insufficient data."""
        with self.assertRaises(InvalidResponseError):
            decode_header(b'short')
    
    def test_split_file_id_valid(self):
        """Test splitting valid file IDs."""
        file_id = "group1/M00/00/00/test.jpg"
        group_name, remote_filename = split_file_id(file_id)
        
        self.assertEqual(group_name, "group1")
        self.assertEqual(remote_filename, "M00/00/00/test.jpg")
    
    def test_split_file_id_invalid(self):
        """Test splitting invalid file IDs."""
        invalid_ids = [
            "",
            "group1",
            "/M00/00/00/test.jpg",
            "group1/",
            "verylonggroupname123/M00/00/00/test.jpg",
        ]
        
        for file_id in invalid_ids:
            with self.assertRaises(InvalidFileIDError):
                split_file_id(file_id)
    
    def test_join_file_id(self):
        """Test joining file ID components."""
        group_name = "group1"
        remote_filename = "M00/00/00/test.jpg"
        
        file_id = join_file_id(group_name, remote_filename)
        self.assertEqual(file_id, "group1/M00/00/00/test.jpg")
    
    def test_encode_decode_metadata(self):
        """Test metadata encoding and decoding."""
        metadata = {
            "author": "John Doe",
            "date": "2025-01-15",
            "version": "1.0",
        }
        
        encoded = encode_metadata(metadata)
        self.assertIsInstance(encoded, bytes)
        self.assertGreater(len(encoded), 0)
        
        decoded = decode_metadata(encoded)
        self.assertEqual(len(decoded), len(metadata))
        for key, value in metadata.items():
            self.assertEqual(decoded[key], value)
    
    def test_encode_metadata_empty(self):
        """Test encoding empty metadata."""
        encoded = encode_metadata(None)
        self.assertEqual(encoded, b'')
        
        encoded = encode_metadata({})
        self.assertEqual(encoded, b'')
    
    def test_decode_metadata_empty(self):
        """Test decoding empty metadata."""
        decoded = decode_metadata(b'')
        self.assertEqual(decoded, {})
    
    def test_get_file_ext_name(self):
        """Test file extension extraction."""
        test_cases = [
            ("test.jpg", "jpg"),
            ("file.tar.gz", "gz"),
            ("noext", ""),
            ("file.verylongext", "verylo"),  # Truncated to 6 chars
            (".hidden", "hidden"),
        ]
        
        for filename, expected_ext in test_cases:
            ext = get_file_ext_name(filename)
            self.assertEqual(ext, expected_ext)
    
    def test_pad_unpad_string(self):
        """Test string padding and unpadding."""
        test_string = "test"
        length = 16
        
        padded = pad_string(test_string, length)
        self.assertEqual(len(padded), length)
        
        unpadded = unpad_string(padded)
        self.assertEqual(unpadded, test_string)
    
    def test_pad_string_truncate(self):
        """Test padding with truncation."""
        test_string = "verylongstringthatexceedslength"
        length = 10
        
        padded = pad_string(test_string, length)
        self.assertEqual(len(padded), length)
    
    def test_encode_decode_int64(self):
        """Test 64-bit integer encoding and decoding."""
        test_values = [0, 1, 1024, 2**32, 2**63 - 1]
        
        for value in test_values:
            encoded = encode_int64(value)
            self.assertEqual(len(encoded), 8)
            
            decoded = decode_int64(encoded)
            self.assertEqual(decoded, value)
    
    def test_decode_int64_short_data(self):
        """Test decoding int64 with insufficient data."""
        result = decode_int64(b'short')
        self.assertEqual(result, 0)


if __name__ == '__main__':
    unittest.main()