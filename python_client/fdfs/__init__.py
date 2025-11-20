"""
FastDFS Python Client Library

Official Python client for FastDFS distributed file system.
Provides a high-level, Pythonic API for interacting with FastDFS servers.

Copyright (C) 2025 FastDFS Python Client Contributors
License: GNU General Public License V3

Example:
    >>> from fdfs import Client, ClientConfig
    >>> config = ClientConfig(tracker_addrs=['192.168.1.100:22122'])
    >>> client = Client(config)
    >>> file_id = client.upload_file('test.jpg')
    >>> data = client.download_file(file_id)
    >>> client.delete_file(file_id)
    >>> client.close()
"""

__version__ = '1.0.0'
__author__ = 'FastDFS Python Client Contributors'
__license__ = 'GPL-3.0'

from .client import Client, ClientConfig
from .errors import (
    FastDFSError,
    ClientClosedError,
    FileNotFoundError,
    NoStorageServerError,
    ConnectionTimeoutError,
    NetworkTimeoutError,
    InvalidFileIDError,
    InvalidResponseError,
    StorageServerOfflineError,
    TrackerServerOfflineError,
    InsufficientSpaceError,
    FileAlreadyExistsError,
    InvalidMetadataError,
    OperationNotSupportedError,
    InvalidArgumentError,
    ProtocolError,
    NetworkError,
)
from .types import (
    FileInfo,
    StorageServer,
    MetadataFlag,
)

__all__ = [
    'Client',
    'ClientConfig',
    # Errors
    'FastDFSError',
    'ClientClosedError',
    'FileNotFoundError',
    'NoStorageServerError',
    'ConnectionTimeoutError',
    'NetworkTimeoutError',
    'InvalidFileIDError',
    'InvalidResponseError',
    'StorageServerOfflineError',
    'TrackerServerOfflineError',
    'InsufficientSpaceError',
    'FileAlreadyExistsError',
    'InvalidMetadataError',
    'OperationNotSupportedError',
    'InvalidArgumentError',
    'ProtocolError',
    'NetworkError',
    # Types
    'FileInfo',
    'StorageServer',
    'MetadataFlag',
]