"""
FastDFS Python Client

Main client class for interacting with FastDFS distributed file system.
"""

import threading
from typing import Optional, Dict, List
from dataclasses import dataclass

from .connection import ConnectionPool
from .operations import Operations
from .types import FileInfo, MetadataFlag
from .errors import ClientClosedError, InvalidArgumentError


@dataclass
class ClientConfig:
    """
    Configuration for FastDFS client.
    
    Attributes:
        tracker_addrs: List of tracker server addresses in format "host:port"
        max_conns: Maximum number of connections per tracker server (default: 10)
        connect_timeout: Timeout for establishing connections in seconds (default: 5.0)
        network_timeout: Timeout for network I/O operations in seconds (default: 30.0)
        idle_timeout: Timeout for idle connections in the pool in seconds (default: 60.0)
        retry_count: Number of retries for failed operations (default: 3)
    """
    tracker_addrs: List[str]
    max_conns: int = 10
    connect_timeout: float = 5.0
    network_timeout: float = 30.0
    idle_timeout: float = 60.0
    retry_count: int = 3


class Client:
    """
    FastDFS client for file operations.
    
    This client provides a high-level, Pythonic API for interacting with FastDFS servers.
    It handles connection pooling, automatic retries, and error handling.
    
    Example:
        >>> config = ClientConfig(tracker_addrs=['192.168.1.100:22122'])
        >>> client = Client(config)
        >>> file_id = client.upload_file('test.jpg')
        >>> data = client.download_file(file_id)
        >>> client.delete_file(file_id)
        >>> client.close()
    """
    
    def __init__(self, config: ClientConfig):
        """
        Creates a new FastDFS client with the given configuration.
        
        Args:
            config: Client configuration
            
        Raises:
            InvalidArgumentError: If configuration is invalid
        """
        self._validate_config(config)
        
        self.config = config
        self.closed = False
        self.lock = threading.RLock()
        
        # Initialize connection pools
        self.tracker_pool = ConnectionPool(
            addrs=config.tracker_addrs,
            max_conns=config.max_conns,
            connect_timeout=config.connect_timeout,
            idle_timeout=config.idle_timeout
        )
        
        self.storage_pool = ConnectionPool(
            addrs=[],  # Storage servers are discovered dynamically
            max_conns=config.max_conns,
            connect_timeout=config.connect_timeout,
            idle_timeout=config.idle_timeout
        )
        
        # Initialize operations handler
        self.ops = Operations(
            tracker_pool=self.tracker_pool,
            storage_pool=self.storage_pool,
            network_timeout=config.network_timeout,
            retry_count=config.retry_count
        )
    
    def _validate_config(self, config: ClientConfig) -> None:
        """Validates the client configuration."""
        if not config:
            raise InvalidArgumentError("Config is required")
        
        if not config.tracker_addrs:
            raise InvalidArgumentError("Tracker addresses are required")
        
        for addr in config.tracker_addrs:
            if not addr or ':' not in addr:
                raise InvalidArgumentError(f"Invalid tracker address: {addr}")
    
    def _check_closed(self) -> None:
        """Checks if the client is closed and raises an error if so."""
        with self.lock:
            if self.closed:
                raise ClientClosedError()
    
    def upload_file(self, local_filename: str, metadata: Optional[Dict[str, str]] = None) -> str:
        """
        Uploads a file from the local filesystem to FastDFS.
        
        Args:
            local_filename: Path to the local file
            metadata: Optional metadata key-value pairs
            
        Returns:
            File ID that can be used to download or delete the file
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If local file doesn't exist
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.upload_file(local_filename, metadata, is_appender=False)
    
    def upload_buffer(self, data: bytes, file_ext_name: str, 
                     metadata: Optional[Dict[str, str]] = None) -> str:
        """
        Uploads data from a byte buffer to FastDFS.
        
        Args:
            data: File content as bytes
            file_ext_name: File extension without dot (e.g., "jpg", "txt")
            metadata: Optional metadata key-value pairs
            
        Returns:
            File ID that can be used to download or delete the file
            
        Raises:
            ClientClosedError: If client is closed
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.upload_buffer(data, file_ext_name, metadata, is_appender=False)
    
    def upload_appender_file(self, local_filename: str, 
                            metadata: Optional[Dict[str, str]] = None) -> str:
        """
        Uploads an appender file that can be modified later.
        
        Appender files support append, modify, and truncate operations.
        
        Args:
            local_filename: Path to the local file
            metadata: Optional metadata key-value pairs
            
        Returns:
            File ID of the appender file
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If local file doesn't exist
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.upload_file(local_filename, metadata, is_appender=True)
    
    def upload_appender_buffer(self, data: bytes, file_ext_name: str,
                               metadata: Optional[Dict[str, str]] = None) -> str:
        """
        Uploads an appender file from buffer.
        
        Args:
            data: File content as bytes
            file_ext_name: File extension without dot
            metadata: Optional metadata key-value pairs
            
        Returns:
            File ID of the appender file
            
        Raises:
            ClientClosedError: If client is closed
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.upload_buffer(data, file_ext_name, metadata, is_appender=True)
    
    def download_file(self, file_id: str) -> bytes:
        """
        Downloads a file from FastDFS and returns its content.
        
        Args:
            file_id: The file ID to download
            
        Returns:
            File content as bytes
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If file doesn't exist
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.download_file(file_id, 0, 0)
    
    def download_file_range(self, file_id: str, offset: int, length: int) -> bytes:
        """
        Downloads a specific range of bytes from a file.
        
        Args:
            file_id: The file ID to download
            offset: Starting byte offset
            length: Number of bytes to download (0 means to end of file)
            
        Returns:
            Requested file content as bytes
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If file doesn't exist
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.download_file(file_id, offset, length)
    
    def download_to_file(self, file_id: str, local_filename: str) -> None:
        """
        Downloads a file and saves it to the local filesystem.
        
        Args:
            file_id: The file ID to download
            local_filename: Path where to save the file
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If file doesn't exist
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
            IOError: If local file cannot be written
        """
        self._check_closed()
        self.ops.download_to_file(file_id, local_filename)
    
    def delete_file(self, file_id: str) -> None:
        """
        Deletes a file from FastDFS.
        
        Args:
            file_id: The file ID to delete
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If file doesn't exist
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
        """
        self._check_closed()
        self.ops.delete_file(file_id)
    
    def set_metadata(self, file_id: str, metadata: Dict[str, str], 
                    flag: MetadataFlag = MetadataFlag.OVERWRITE) -> None:
        """
        Sets metadata for a file.
        
        Args:
            file_id: The file ID
            metadata: Metadata key-value pairs
            flag: Metadata operation flag (OVERWRITE or MERGE)
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If file doesn't exist
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
        """
        self._check_closed()
        self.ops.set_metadata(file_id, metadata, flag)
    
    def get_metadata(self, file_id: str) -> Dict[str, str]:
        """
        Retrieves metadata for a file.
        
        Args:
            file_id: The file ID
            
        Returns:
            Dictionary of metadata key-value pairs
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If file doesn't exist
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.get_metadata(file_id)
    
    def get_file_info(self, file_id: str) -> FileInfo:
        """
        Retrieves file information including size, create time, and CRC32.
        
        Args:
            file_id: The file ID
            
        Returns:
            FileInfo object with file details
            
        Raises:
            ClientClosedError: If client is closed
            FileNotFoundError: If file doesn't exist
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
        """
        self._check_closed()
        return self.ops.get_file_info(file_id)
    
    def file_exists(self, file_id: str) -> bool:
        """
        Checks if a file exists on the storage server.
        
        Args:
            file_id: The file ID to check
            
        Returns:
            True if file exists, False otherwise
            
        Raises:
            ClientClosedError: If client is closed
            InvalidFileIDError: If file ID format is invalid
            NetworkError: If network communication fails
        """
        self._check_closed()
        try:
            self.ops.get_file_info(file_id)
            return True
        except Exception:
            return False
    
    def close(self) -> None:
        """
        Closes the client and releases all resources.
        
        After calling close, all operations will raise ClientClosedError.
        It's safe to call close multiple times.
        """
        with self.lock:
            if self.closed:
                return
            
            self.closed = True
            
            if self.tracker_pool:
                self.tracker_pool.close()
            
            if self.storage_pool:
                self.storage_pool.close()
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
        return False