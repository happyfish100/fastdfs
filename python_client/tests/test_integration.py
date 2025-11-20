"""
Integration tests for FastDFS client.

These tests require a running FastDFS cluster.
Set the environment variable FASTDFS_TRACKER_ADDR to run these tests.
"""

import unittest
import os
import tempfile
from fdfs import Client, ClientConfig
from fdfs.types import MetadataFlag
from fdfs.errors import FileNotFoundError


@unittest.skipUnless(
    os.environ.get('FASTDFS_TRACKER_ADDR'),
    "Set FASTDFS_TRACKER_ADDR environment variable to run integration tests"
)
class TestIntegration(unittest.TestCase):
    """Integration tests with real FastDFS cluster."""
    
    @classmethod
    def setUpClass(cls):
        """Set up test client."""
        tracker_addr = os.environ.get('FASTDFS_TRACKER_ADDR', '127.0.0.1:22122')
        cls.config = ClientConfig(tracker_addrs=[tracker_addr])
        cls.client = Client(cls.config)
    
    @classmethod
    def tearDownClass(cls):
        """Clean up test client."""
        cls.client.close()
    
    def test_upload_download_delete_cycle(self):
        """Test complete upload, download, delete cycle."""
        # Upload
        test_data = b'Hello, FastDFS! This is a test file.'
        file_id = self.client.upload_buffer(test_data, 'txt')
        
        self.assertIsNotNone(file_id)
        self.assertIn('/', file_id)
        
        # Download
        downloaded_data = self.client.download_file(file_id)
        self.assertEqual(downloaded_data, test_data)
        
        # Delete
        self.client.delete_file(file_id)
        
        # Verify deletion
        with self.assertRaises(FileNotFoundError):
            self.client.download_file(file_id)
    
    def test_upload_file_from_disk(self):
        """Test uploading file from disk."""
        # Create temporary file
        with tempfile.NamedTemporaryFile(mode='wb', delete=False, suffix='.txt') as f:
            test_data = b'Test file content from disk'
            f.write(test_data)
            temp_path = f.name
        
        try:
            # Upload
            file_id = self.client.upload_file(temp_path)
            self.assertIsNotNone(file_id)
            
            # Download and verify
            downloaded_data = self.client.download_file(file_id)
            self.assertEqual(downloaded_data, test_data)
            
            # Clean up
            self.client.delete_file(file_id)
        finally:
            os.unlink(temp_path)
    
    def test_download_to_file(self):
        """Test downloading file to disk."""
        # Upload
        test_data = b'Test data for download to file'
        file_id = self.client.upload_buffer(test_data, 'bin')
        
        # Download to file
        with tempfile.NamedTemporaryFile(mode='rb', delete=False) as f:
            temp_path = f.name
        
        try:
            self.client.download_to_file(file_id, temp_path)
            
            # Verify
            with open(temp_path, 'rb') as f:
                downloaded_data = f.read()
            
            self.assertEqual(downloaded_data, test_data)
        finally:
            os.unlink(temp_path)
            self.client.delete_file(file_id)
    
    def test_metadata_operations(self):
        """Test metadata set and get operations."""
        # Upload file
        test_data = b'File with metadata'
        metadata = {
            'author': 'Test User',
            'date': '2025-01-15',
            'version': '1.0'
        }
        file_id = self.client.upload_buffer(test_data, 'txt', metadata)
        
        try:
            # Get metadata
            retrieved_metadata = self.client.get_metadata(file_id)
            self.assertEqual(len(retrieved_metadata), len(metadata))
            for key, value in metadata.items():
                self.assertEqual(retrieved_metadata[key], value)
            
            # Update metadata (overwrite)
            new_metadata = {
                'author': 'Updated User',
                'status': 'modified'
            }
            self.client.set_metadata(file_id, new_metadata, MetadataFlag.OVERWRITE)
            
            retrieved_metadata = self.client.get_metadata(file_id)
            self.assertEqual(len(retrieved_metadata), len(new_metadata))
            self.assertEqual(retrieved_metadata['author'], 'Updated User')
            self.assertEqual(retrieved_metadata['status'], 'modified')
        finally:
            self.client.delete_file(file_id)
    
    def test_file_info(self):
        """Test getting file information."""
        # Upload file
        test_data = b'Test data for file info'
        file_id = self.client.upload_buffer(test_data, 'bin')
        
        try:
            # Get file info
            file_info = self.client.get_file_info(file_id)
            
            self.assertEqual(file_info.file_size, len(test_data))
            self.assertIsNotNone(file_info.create_time)
            self.assertIsNotNone(file_info.crc32)
            self.assertIsNotNone(file_info.source_ip_addr)
        finally:
            self.client.delete_file(file_id)
    
    def test_file_exists(self):
        """Test checking file existence."""
        # Upload file
        test_data = b'Test existence check'
        file_id = self.client.upload_buffer(test_data, 'txt')
        
        # Check existence
        self.assertTrue(self.client.file_exists(file_id))
        
        # Delete and check again
        self.client.delete_file(file_id)
        self.assertFalse(self.client.file_exists(file_id))
    
    def test_download_range(self):
        """Test downloading file range."""
        # Upload file
        test_data = b'0123456789' * 10  # 100 bytes
        file_id = self.client.upload_buffer(test_data, 'bin')
        
        try:
            # Download range
            offset = 10
            length = 20
            range_data = self.client.download_file_range(file_id, offset, length)
            
            self.assertEqual(len(range_data), length)
            self.assertEqual(range_data, test_data[offset:offset + length])
        finally:
            self.client.delete_file(file_id)


if __name__ == '__main__':
    unittest.main()