"""
Unit tests for the FastDFS client.
"""

import unittest
from unittest.mock import Mock, patch, MagicMock
from fdfs import Client, ClientConfig
from fdfs.errors import (
    ClientClosedError,
    InvalidArgumentError,
    FileNotFoundError,
)


class TestClientConfig(unittest.TestCase):
    """Test ClientConfig class."""
    
    def test_config_defaults(self):
        """Test default configuration values."""
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        
        self.assertEqual(config.max_conns, 10)
        self.assertEqual(config.connect_timeout, 5.0)
        self.assertEqual(config.network_timeout, 30.0)
        self.assertEqual(config.idle_timeout, 60.0)
        self.assertEqual(config.retry_count, 3)
    
    def test_config_custom_values(self):
        """Test custom configuration values."""
        config = ClientConfig(
            tracker_addrs=['127.0.0.1:22122'],
            max_conns=20,
            connect_timeout=10.0,
            network_timeout=60.0,
            idle_timeout=120.0,
            retry_count=5
        )
        
        self.assertEqual(config.max_conns, 20)
        self.assertEqual(config.connect_timeout, 10.0)
        self.assertEqual(config.network_timeout, 60.0)
        self.assertEqual(config.idle_timeout, 120.0)
        self.assertEqual(config.retry_count, 5)


class TestClient(unittest.TestCase):
    """Test Client class."""
    
    def test_client_creation_valid_config(self):
        """Test creating client with valid configuration."""
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        client = Client(config)
        
        self.assertIsNotNone(client)
        self.assertFalse(client.closed)
        
        client.close()
    
    def test_client_creation_invalid_config(self):
        """Test creating client with invalid configuration."""
        # No tracker addresses
        with self.assertRaises(InvalidArgumentError):
            config = ClientConfig(tracker_addrs=[])
            Client(config)
        
        # Invalid tracker address format
        with self.assertRaises(InvalidArgumentError):
            config = ClientConfig(tracker_addrs=['invalid'])
            Client(config)
    
    def test_client_close(self):
        """Test closing the client."""
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        client = Client(config)
        
        client.close()
        self.assertTrue(client.closed)
        
        # Operations after close should raise error
        with self.assertRaises(ClientClosedError):
            client.upload_buffer(b'test', 'txt')
    
    def test_client_close_idempotent(self):
        """Test that closing client multiple times is safe."""
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        client = Client(config)
        
        client.close()
        client.close()  # Should not raise error
    
    def test_client_context_manager(self):
        """Test using client as context manager."""
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        
        with Client(config) as client:
            self.assertFalse(client.closed)
        
        self.assertTrue(client.closed)
    
    @patch('fdfs.operations.Operations.upload_buffer')
    def test_upload_buffer(self, mock_upload):
        """Test uploading buffer."""
        mock_upload.return_value = 'group1/M00/00/00/test.jpg'
        
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        client = Client(config)
        
        file_id = client.upload_buffer(b'test data', 'jpg')
        
        self.assertEqual(file_id, 'group1/M00/00/00/test.jpg')
        mock_upload.assert_called_once()
        
        client.close()
    
    @patch('fdfs.operations.Operations.download_file')
    def test_download_file(self, mock_download):
        """Test downloading file."""
        mock_download.return_value = b'test data'
        
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        client = Client(config)
        
        data = client.download_file('group1/M00/00/00/test.jpg')
        
        self.assertEqual(data, b'test data')
        mock_download.assert_called_once()
        
        client.close()
    
    @patch('fdfs.operations.Operations.delete_file')
    def test_delete_file(self, mock_delete):
        """Test deleting file."""
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        client = Client(config)
        
        client.delete_file('group1/M00/00/00/test.jpg')
        
        mock_delete.assert_called_once()
        
        client.close()
    
    @patch('fdfs.operations.Operations.get_file_info')
    def test_file_exists(self, mock_get_info):
        """Test checking if file exists."""
        from fdfs.types import FileInfo
        from datetime import datetime
        
        mock_get_info.return_value = FileInfo(
            file_size=1024,
            create_time=datetime.now(),
            crc32=12345,
            source_ip_addr='127.0.0.1'
        )
        
        config = ClientConfig(tracker_addrs=['127.0.0.1:22122'])
        client = Client(config)
        
        exists = client.file_exists('group1/M00/00/00/test.jpg')
        self.assertTrue(exists)
        
        client.close()


if __name__ == '__main__':
    unittest.main()