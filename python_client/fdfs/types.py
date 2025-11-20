"""
FastDFS Protocol Types and Constants

This module defines all protocol-level constants, command codes, and data structures
used in communication with FastDFS tracker and storage servers.
"""

from dataclasses import dataclass
from datetime import datetime
from enum import IntEnum
from typing import Optional


# Protocol Constants
TRACKER_DEFAULT_PORT = 22122
STORAGE_DEFAULT_PORT = 23000

# Protocol Header Size
FDFS_PROTO_HEADER_LEN = 10  # 8 bytes length + 1 byte cmd + 1 byte status

# Field Size Limits
FDFS_GROUP_NAME_MAX_LEN = 16
FDFS_FILE_EXT_NAME_MAX_LEN = 6
FDFS_MAX_META_NAME_LEN = 64
FDFS_MAX_META_VALUE_LEN = 256
FDFS_FILE_PREFIX_MAX_LEN = 16
FDFS_STORAGE_ID_MAX_SIZE = 16
FDFS_VERSION_SIZE = 8
IP_ADDRESS_SIZE = 16

# Protocol Separators
FDFS_RECORD_SEPARATOR = b'\x01'
FDFS_FIELD_SEPARATOR = b'\x02'


class TrackerCommand(IntEnum):
    """Tracker protocol commands"""
    SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE = 101
    SERVICE_QUERY_FETCH_ONE = 102
    SERVICE_QUERY_UPDATE = 103
    SERVICE_QUERY_STORE_WITH_GROUP_ONE = 104
    SERVICE_QUERY_FETCH_ALL = 105
    SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL = 106
    SERVICE_QUERY_STORE_WITH_GROUP_ALL = 107
    SERVER_LIST_ONE_GROUP = 90
    SERVER_LIST_ALL_GROUPS = 91
    SERVER_LIST_STORAGE = 92
    SERVER_DELETE_STORAGE = 93
    STORAGE_REPORT_IP_CHANGED = 94
    STORAGE_REPORT_STATUS = 95
    STORAGE_REPORT_DISK_USAGE = 96
    STORAGE_SYNC_TIMESTAMP = 97
    STORAGE_SYNC_REPORT = 98


class StorageCommand(IntEnum):
    """Storage protocol commands"""
    UPLOAD_FILE = 11
    DELETE_FILE = 12
    SET_METADATA = 13
    DOWNLOAD_FILE = 14
    GET_METADATA = 15
    UPLOAD_SLAVE_FILE = 21
    QUERY_FILE_INFO = 22
    UPLOAD_APPENDER_FILE = 23
    APPEND_FILE = 24
    MODIFY_FILE = 34
    TRUNCATE_FILE = 36


class StorageStatus(IntEnum):
    """Storage server status codes"""
    INIT = 0
    WAIT_SYNC = 1
    SYNCING = 2
    IP_CHANGED = 3
    DELETED = 4
    OFFLINE = 5
    ONLINE = 6
    ACTIVE = 7
    RECOVERY = 9
    NONE = 99


class MetadataFlag(IntEnum):
    """Metadata operation flags"""
    OVERWRITE = ord('O')  # Replace all existing metadata
    MERGE = ord('M')      # Merge with existing metadata


@dataclass
class FileInfo:
    """
    Information about a file stored in FastDFS.
    
    Attributes:
        file_size: Size of the file in bytes
        create_time: Timestamp when the file was created
        crc32: CRC32 checksum of the file
        source_ip_addr: IP address of the source storage server
    """
    file_size: int
    create_time: datetime
    crc32: int
    source_ip_addr: str


@dataclass
class StorageServer:
    """
    Represents a storage server in the FastDFS cluster.
    
    Attributes:
        ip_addr: IP address of the storage server
        port: Port number of the storage server
        store_path_index: Index of the storage path to use (0-based)
    """
    ip_addr: str
    port: int
    store_path_index: int = 0


@dataclass
class TrackerHeader:
    """
    FastDFS protocol header (10 bytes).
    
    Attributes:
        length: Length of the message body (not including header)
        cmd: Command code (request type or response type)
        status: Status code (0 for success, error code otherwise)
    """
    length: int
    cmd: int
    status: int


@dataclass
class UploadResponse:
    """
    Response from an upload operation.
    
    Attributes:
        group_name: Storage group where the file was stored
        remote_filename: Path and filename on the storage server
    """
    group_name: str
    remote_filename: str