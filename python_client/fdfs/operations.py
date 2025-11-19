"""
FastDFS Operations

This module implements all file operations (upload, download, delete, etc.)
for the FastDFS client.
"""

import time
from typing import Optional, Dict

from .protocol import (
    encode_header,
    decode_header,
    split_file_id,
    join_file_id,
    encode_metadata,
    decode_metadata,
    get_file_ext_name,
    read_file_content,
    write_file_content,
    pad_string,
    unpad_string,
    encode_int64,
    decode_int64,
    encode_int32,
    decode_int32,
)
from .types import (
    FDFS_GROUP_NAME_MAX_LEN,
    FDFS_FILE_EXT_NAME_MAX_LEN,
    IP_ADDRESS_SIZE,
    TrackerCommand,
    StorageCommand,
    StorageServer,
    FileInfo,
    MetadataFlag,
)
from .errors import (
    InvalidResponseError,
    NoStorageServerError,
    map_status_to_error,
)
from .connection import ConnectionPool


class Operations:
    """
    Handles all FastDFS file operations.
    
    This class is used internally by the Client class.
    """
    
    def __init__(self, tracker_pool: ConnectionPool, storage_pool: ConnectionPool,
                 network_timeout: float, retry_count: int):
        """
        Initialize operations handler.
        
        Args:
            tracker_pool: Connection pool for tracker servers
            storage_pool: Connection pool for storage servers
            network_timeout: Network I/O timeout in seconds
            retry_count: Number of retries for failed operations
        """
        self.tracker_pool = tracker_pool
        self.storage_pool = storage_pool
        self.network_timeout = network_timeout
        self.retry_count = retry_count
    
    def upload_file(self, local_filename: str, metadata: Optional[Dict[str, str]] = None,
                   is_appender: bool = False) -> str:
        """
        Uploads a file from the local filesystem.
        
        Args:
            local_filename: Path to the local file
            metadata: Optional metadata key-value pairs
            is_appender: Whether to create an appender file
            
        Returns:
            File ID
        """
        file_data = read_file_content(local_filename)
        ext_name = get_file_ext_name(local_filename)
        return self.upload_buffer(file_data, ext_name, metadata, is_appender)
    
    def upload_buffer(self, data: bytes, file_ext_name: str,
                     metadata: Optional[Dict[str, str]] = None,
                     is_appender: bool = False) -> str:
        """
        Uploads data from a byte buffer.
        
        Args:
            data: File content as bytes
            file_ext_name: File extension without dot (e.g., "jpg")
            metadata: Optional metadata key-value pairs
            is_appender: Whether to create an appender file
            
        Returns:
            File ID
        """
        for attempt in range(self.retry_count):
            try:
                return self._upload_buffer_internal(data, file_ext_name, metadata, is_appender)
            except Exception as e:
                if attempt == self.retry_count - 1:
                    raise
                time.sleep(attempt + 1)
    
    def _upload_buffer_internal(self, data: bytes, file_ext_name: str,
                                metadata: Optional[Dict[str, str]],
                                is_appender: bool) -> str:
        """Internal implementation of buffer upload."""
        # Get storage server from tracker
        storage_server = self._get_storage_server("")
        
        # Get connection to storage server
        storage_addr = f"{storage_server.ip_addr}:{storage_server.port}"
        self.storage_pool.add_addr(storage_addr)
        conn = self.storage_pool.get(storage_addr)
        
        try:
            # Prepare upload command
            cmd = StorageCommand.UPLOAD_APPENDER_FILE if is_appender else StorageCommand.UPLOAD_FILE
            
            # Build request
            ext_name_bytes = pad_string(file_ext_name, FDFS_FILE_EXT_NAME_MAX_LEN)
            store_path_index = storage_server.store_path_index
            
            body_len = 1 + FDFS_FILE_EXT_NAME_MAX_LEN + len(data)
            req_header = encode_header(body_len, cmd, 0)
            
            # Send request
            conn.send(req_header, self.network_timeout)
            conn.send(bytes([store_path_index]), self.network_timeout)
            conn.send(ext_name_bytes, self.network_timeout)
            conn.send(data, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
            
            if resp_header.length <= 0:
                raise InvalidResponseError("Empty response body")
            
            resp_body = conn.receive_full(resp_header.length, self.network_timeout)
            
            # Parse response
            if len(resp_body) < FDFS_GROUP_NAME_MAX_LEN:
                raise InvalidResponseError("Response body too short")
            
            group_name = unpad_string(resp_body[:FDFS_GROUP_NAME_MAX_LEN])
            remote_filename = resp_body[FDFS_GROUP_NAME_MAX_LEN:].decode('utf-8')
            
            file_id = join_file_id(group_name, remote_filename)
            
            # Set metadata if provided
            if metadata:
                try:
                    self.set_metadata(file_id, metadata, MetadataFlag.OVERWRITE)
                except:
                    pass  # Metadata setting failed, but file is uploaded
            
            return file_id
        finally:
            self.storage_pool.put(conn)
    
    def _get_storage_server(self, group_name: str) -> StorageServer:
        """
        Gets a storage server from tracker for upload.
        
        Args:
            group_name: Storage group name, or empty for any group
            
        Returns:
            StorageServer information
        """
        conn = self.tracker_pool.get()
        
        try:
            # Prepare request
            if group_name:
                cmd = TrackerCommand.SERVICE_QUERY_STORE_WITH_GROUP_ONE
                body_len = FDFS_GROUP_NAME_MAX_LEN
            else:
                cmd = TrackerCommand.SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE
                body_len = 0
            
            header = encode_header(body_len, cmd, 0)
            conn.send(header, self.network_timeout)
            
            if group_name:
                group_name_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN)
                conn.send(group_name_bytes, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
            
            if resp_header.length <= 0:
                raise NoStorageServerError()
            
            resp_body = conn.receive_full(resp_header.length, self.network_timeout)
            
            # Parse storage server info
            if len(resp_body) < FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 9:
                raise InvalidResponseError("Storage server response too short")
            
            offset = FDFS_GROUP_NAME_MAX_LEN
            ip_addr = unpad_string(resp_body[offset:offset + IP_ADDRESS_SIZE])
            offset += IP_ADDRESS_SIZE
            
            port = decode_int64(resp_body[offset:offset + 8])
            offset += 8
            
            store_path_index = resp_body[offset]
            
            return StorageServer(
                ip_addr=ip_addr,
                port=port,
                store_path_index=store_path_index
            )
        finally:
            self.tracker_pool.put(conn)
    
    def download_file(self, file_id: str, offset: int = 0, length: int = 0) -> bytes:
        """
        Downloads a file from FastDFS.
        
        Args:
            file_id: The file ID to download
            offset: Starting byte offset (0 for beginning)
            length: Number of bytes to download (0 for entire file)
            
        Returns:
            File content as bytes
        """
        for attempt in range(self.retry_count):
            try:
                return self._download_file_internal(file_id, offset, length)
            except Exception as e:
                if attempt == self.retry_count - 1:
                    raise
                time.sleep(attempt + 1)
    
    def _download_file_internal(self, file_id: str, offset: int, length: int) -> bytes:
        """Internal implementation of file download."""
        group_name, remote_filename = split_file_id(file_id)
        
        # Get storage server for download
        storage_server = self._get_download_storage_server(group_name, remote_filename)
        
        # Get connection
        storage_addr = f"{storage_server.ip_addr}:{storage_server.port}"
        self.storage_pool.add_addr(storage_addr)
        conn = self.storage_pool.get(storage_addr)
        
        try:
            # Build request
            remote_filename_bytes = remote_filename.encode('utf-8')
            body_len = 16 + len(remote_filename_bytes)
            header = encode_header(body_len, StorageCommand.DOWNLOAD_FILE, 0)
            
            body = encode_int64(offset) + encode_int64(length) + remote_filename_bytes
            
            # Send request
            conn.send(header, self.network_timeout)
            conn.send(body, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
            
            if resp_header.length <= 0:
                return b''
            
            # Receive file data
            data = conn.receive_full(resp_header.length, self.network_timeout)
            return data
        finally:
            self.storage_pool.put(conn)
    
    def _get_download_storage_server(self, group_name: str, remote_filename: str) -> StorageServer:
        """
        Gets a storage server from tracker for download.
        
        Args:
            group_name: Storage group name
            remote_filename: Remote filename
            
        Returns:
            StorageServer information
        """
        conn = self.tracker_pool.get()
        
        try:
            # Build request
            remote_filename_bytes = remote_filename.encode('utf-8')
            body_len = FDFS_GROUP_NAME_MAX_LEN + len(remote_filename_bytes)
            header = encode_header(body_len, TrackerCommand.SERVICE_QUERY_FETCH_ONE, 0)
            
            body = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN) + remote_filename_bytes
            
            # Send request
            conn.send(header, self.network_timeout)
            conn.send(body, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
            
            resp_body = conn.receive_full(resp_header.length, self.network_timeout)
            
            # Parse response
            if len(resp_body) < FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 8:
                raise InvalidResponseError("Download storage server response too short")
            
            offset = FDFS_GROUP_NAME_MAX_LEN
            ip_addr = unpad_string(resp_body[offset:offset + IP_ADDRESS_SIZE])
            offset += IP_ADDRESS_SIZE
            
            port = decode_int64(resp_body[offset:offset + 8])
            
            return StorageServer(ip_addr=ip_addr, port=port, store_path_index=0)
        finally:
            self.tracker_pool.put(conn)
    
    def download_to_file(self, file_id: str, local_filename: str) -> None:
        """
        Downloads a file and saves it to the local filesystem.
        
        Args:
            file_id: The file ID to download
            local_filename: Path where to save the file
        """
        data = self.download_file(file_id, 0, 0)
        write_file_content(local_filename, data)
    
    def delete_file(self, file_id: str) -> None:
        """
        Deletes a file from FastDFS.
        
        Args:
            file_id: The file ID to delete
        """
        for attempt in range(self.retry_count):
            try:
                self._delete_file_internal(file_id)
                return
            except Exception as e:
                if attempt == self.retry_count - 1:
                    raise
                time.sleep(attempt + 1)
    
    def _delete_file_internal(self, file_id: str) -> None:
        """Internal implementation of file deletion."""
        group_name, remote_filename = split_file_id(file_id)
        
        # Get storage server
        storage_server = self._get_download_storage_server(group_name, remote_filename)
        
        # Get connection
        storage_addr = f"{storage_server.ip_addr}:{storage_server.port}"
        self.storage_pool.add_addr(storage_addr)
        conn = self.storage_pool.get(storage_addr)
        
        try:
            # Build request
            remote_filename_bytes = remote_filename.encode('utf-8')
            body_len = FDFS_GROUP_NAME_MAX_LEN + len(remote_filename_bytes)
            header = encode_header(body_len, StorageCommand.DELETE_FILE, 0)
            
            body = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN) + remote_filename_bytes
            
            # Send request
            conn.send(header, self.network_timeout)
            conn.send(body, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
        finally:
            self.storage_pool.put(conn)
    
    def set_metadata(self, file_id: str, metadata: Dict[str, str], flag: MetadataFlag) -> None:
        """
        Sets metadata for a file.
        
        Args:
            file_id: The file ID
            metadata: Metadata key-value pairs
            flag: Metadata operation flag (OVERWRITE or MERGE)
        """
        group_name, remote_filename = split_file_id(file_id)
        
        # Get storage server
        storage_server = self._get_download_storage_server(group_name, remote_filename)
        
        # Get connection
        storage_addr = f"{storage_server.ip_addr}:{storage_server.port}"
        self.storage_pool.add_addr(storage_addr)
        conn = self.storage_pool.get(storage_addr)
        
        try:
            # Encode metadata
            metadata_bytes = encode_metadata(metadata)
            remote_filename_bytes = remote_filename.encode('utf-8')
            
            # Build request
            body_len = (2 * 8 + 1 + FDFS_GROUP_NAME_MAX_LEN + 
                       len(remote_filename_bytes) + len(metadata_bytes))
            header = encode_header(body_len, StorageCommand.SET_METADATA, 0)
            
            body = (encode_int64(len(remote_filename_bytes)) +
                   encode_int64(len(metadata_bytes)) +
                   bytes([flag]) +
                   pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN) +
                   remote_filename_bytes +
                   metadata_bytes)
            
            # Send request
            conn.send(header, self.network_timeout)
            conn.send(body, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
        finally:
            self.storage_pool.put(conn)
    
    def get_metadata(self, file_id: str) -> Dict[str, str]:
        """
        Retrieves metadata for a file.
        
        Args:
            file_id: The file ID
            
        Returns:
            Dictionary of metadata key-value pairs
        """
        group_name, remote_filename = split_file_id(file_id)
        
        # Get storage server
        storage_server = self._get_download_storage_server(group_name, remote_filename)
        
        # Get connection
        storage_addr = f"{storage_server.ip_addr}:{storage_server.port}"
        self.storage_pool.add_addr(storage_addr)
        conn = self.storage_pool.get(storage_addr)
        
        try:
            # Build request
            remote_filename_bytes = remote_filename.encode('utf-8')
            body_len = FDFS_GROUP_NAME_MAX_LEN + len(remote_filename_bytes)
            header = encode_header(body_len, StorageCommand.GET_METADATA, 0)
            
            body = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN) + remote_filename_bytes
            
            # Send request
            conn.send(header, self.network_timeout)
            conn.send(body, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
            
            if resp_header.length <= 0:
                return {}
            
            resp_body = conn.receive_full(resp_header.length, self.network_timeout)
            
            # Decode metadata
            return decode_metadata(resp_body)
        finally:
            self.storage_pool.put(conn)
    
    def get_file_info(self, file_id: str) -> FileInfo:
        """
        Retrieves file information.
        
        Args:
            file_id: The file ID
            
        Returns:
            FileInfo object with file details
        """
        group_name, remote_filename = split_file_id(file_id)
        
        # Get storage server
        storage_server = self._get_download_storage_server(group_name, remote_filename)
        
        # Get connection
        storage_addr = f"{storage_server.ip_addr}:{storage_server.port}"
        self.storage_pool.add_addr(storage_addr)
        conn = self.storage_pool.get(storage_addr)
        
        try:
            # Build request
            remote_filename_bytes = remote_filename.encode('utf-8')
            body_len = FDFS_GROUP_NAME_MAX_LEN + len(remote_filename_bytes)
            header = encode_header(body_len, StorageCommand.QUERY_FILE_INFO, 0)
            
            body = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN) + remote_filename_bytes
            
            # Send request
            conn.send(header, self.network_timeout)
            conn.send(body, self.network_timeout)
            
            # Receive response
            resp_header_data = conn.receive_full(10, self.network_timeout)
            resp_header = decode_header(resp_header_data)
            
            if resp_header.status != 0:
                error = map_status_to_error(resp_header.status)
                if error:
                    raise error
            
            resp_body = conn.receive_full(resp_header.length, self.network_timeout)
            
            # Parse file info (file_size, create_time, crc32, source_ip)
            if len(resp_body) < 8 + 8 + 4 + IP_ADDRESS_SIZE:
                raise InvalidResponseError("File info response too short")
            
            file_size = decode_int64(resp_body[0:8])
            create_timestamp = decode_int64(resp_body[8:16])
            crc32 = decode_int32(resp_body[16:20])
            source_ip = unpad_string(resp_body[20:20 + IP_ADDRESS_SIZE])
            
            from datetime import datetime
            create_time = datetime.fromtimestamp(create_timestamp)
            
            return FileInfo(
                file_size=file_size,
                create_time=create_time,
                crc32=crc32,
                source_ip_addr=source_ip
            )
        finally:
            self.storage_pool.put(conn)