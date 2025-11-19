"""
FastDFS Protocol Encoding and Decoding

This module handles all protocol-level encoding and decoding operations
for communication with FastDFS servers.
"""

import os
import struct
from pathlib import Path
from typing import Dict, Optional, Tuple

from .types import (
    FDFS_PROTO_HEADER_LEN,
    FDFS_GROUP_NAME_MAX_LEN,
    FDFS_FILE_EXT_NAME_MAX_LEN,
    FDFS_MAX_META_NAME_LEN,
    FDFS_MAX_META_VALUE_LEN,
    FDFS_RECORD_SEPARATOR,
    FDFS_FIELD_SEPARATOR,
    TrackerHeader,
)
from .errors import InvalidResponseError, InvalidFileIDError


def encode_header(length: int, cmd: int, status: int = 0) -> bytes:
    """
    Encodes a FastDFS protocol header into a 10-byte array.
    
    The header format is:
        - Bytes 0-7: Body length (8 bytes, big-endian uint64)
        - Byte 8: Command code
        - Byte 9: Status code (0 for request, error code for response)
    
    Args:
        length: The length of the message body in bytes
        cmd: The protocol command code
        status: The status code (typically 0 for requests)
        
    Returns:
        10-byte header ready to be sent to the server
    """
    header = struct.pack('>Q', length)  # 8 bytes, big-endian
    header += struct.pack('B', cmd)     # 1 byte
    header += struct.pack('B', status)  # 1 byte
    return header


def decode_header(data: bytes) -> TrackerHeader:
    """
    Decodes a FastDFS protocol header from a byte array.
    
    The header must be exactly 10 bytes long.
    
    Args:
        data: The raw header bytes (must be at least 10 bytes)
        
    Returns:
        TrackerHeader with parsed length, command, and status
        
    Raises:
        InvalidResponseError: If data is too short
    """
    if len(data) < FDFS_PROTO_HEADER_LEN:
        raise InvalidResponseError(f"Header too short: {len(data)} bytes")
    
    length = struct.unpack('>Q', data[0:8])[0]
    cmd = struct.unpack('B', data[8:9])[0]
    status = struct.unpack('B', data[9:10])[0]
    
    return TrackerHeader(length=length, cmd=cmd, status=status)


def split_file_id(file_id: str) -> Tuple[str, str]:
    """
    Splits a FastDFS file ID into its components.
    
    A file ID has the format: "groupName/path/to/file"
    For example: "group1/M00/00/00/wKgBcFxyz.jpg"
    
    Args:
        file_id: The complete file ID string
        
    Returns:
        Tuple of (group_name, remote_filename)
        
    Raises:
        InvalidFileIDError: If the format is invalid
    """
    if not file_id:
        raise InvalidFileIDError(file_id)
    
    parts = file_id.split('/', 1)
    if len(parts) != 2:
        raise InvalidFileIDError(file_id)
    
    group_name, remote_filename = parts
    
    if not group_name or len(group_name) > FDFS_GROUP_NAME_MAX_LEN:
        raise InvalidFileIDError(file_id)
    
    if not remote_filename:
        raise InvalidFileIDError(file_id)
    
    return group_name, remote_filename


def join_file_id(group_name: str, remote_filename: str) -> str:
    """
    Constructs a complete file ID from its components.
    
    This is the inverse operation of split_file_id.
    
    Args:
        group_name: The storage group name
        remote_filename: The path and filename on the storage server
        
    Returns:
        Complete file ID in the format "groupName/remoteFilename"
    """
    return f"{group_name}/{remote_filename}"


def encode_metadata(metadata: Optional[Dict[str, str]]) -> bytes:
    """
    Encodes metadata key-value pairs into FastDFS wire format.
    
    The format uses special separators:
        - Field separator (0x02) between key and value
        - Record separator (0x01) between different key-value pairs
    
    Format: key1<0x02>value1<0x01>key2<0x02>value2<0x01>
    
    Keys are truncated to 64 bytes and values to 256 bytes if they exceed limits.
    
    Args:
        metadata: Dictionary of key-value pairs to encode
        
    Returns:
        Encoded byte array, or empty bytes if metadata is None/empty
    """
    if not metadata:
        return b''
    
    result = b''
    for key, value in metadata.items():
        # Truncate if necessary
        key_bytes = key.encode('utf-8')[:FDFS_MAX_META_NAME_LEN]
        value_bytes = value.encode('utf-8')[:FDFS_MAX_META_VALUE_LEN]
        
        result += key_bytes
        result += FDFS_FIELD_SEPARATOR
        result += value_bytes
        result += FDFS_RECORD_SEPARATOR
    
    return result


def decode_metadata(data: bytes) -> Dict[str, str]:
    """
    Decodes FastDFS wire format metadata into a dictionary.
    
    This is the inverse operation of encode_metadata.
    
    The function parses records separated by 0x01 and fields separated by 0x02.
    Invalid records (not exactly 2 fields) are silently skipped.
    
    Args:
        data: The raw metadata bytes from the server
        
    Returns:
        Dictionary of decoded key-value pairs
    """
    if not data:
        return {}
    
    metadata = {}
    records = data.split(FDFS_RECORD_SEPARATOR)
    
    for record in records:
        if not record:
            continue
        
        fields = record.split(FDFS_FIELD_SEPARATOR)
        if len(fields) != 2:
            continue
        
        key = fields[0].decode('utf-8', errors='ignore')
        value = fields[1].decode('utf-8', errors='ignore')
        metadata[key] = value
    
    return metadata


def get_file_ext_name(filename: str) -> str:
    """
    Extracts and validates the file extension from a filename.
    
    The extension is extracted without the leading dot and truncated to 6 characters
    if it exceeds the FastDFS maximum.
    
    Examples:
        "test.jpg" -> "jpg"
        "file.tar.gz" -> "gz"
        "noext" -> ""
        "file.verylongext" -> "verylo" (truncated)
    
    Args:
        filename: The filename to extract extension from
        
    Returns:
        File extension without the dot, truncated to 6 chars max
    """
    ext = Path(filename).suffix
    if ext.startswith('.'):
        ext = ext[1:]
    
    if len(ext) > FDFS_FILE_EXT_NAME_MAX_LEN:
        ext = ext[:FDFS_FILE_EXT_NAME_MAX_LEN]
    
    return ext


def read_file_content(filename: str) -> bytes:
    """
    Reads the entire contents of a file from the filesystem.
    
    Args:
        filename: Path to the file to read
        
    Returns:
        The complete file contents as bytes
        
    Raises:
        FileNotFoundError: If the file doesn't exist
        IOError: If the file cannot be read
    """
    with open(filename, 'rb') as f:
        return f.read()


def write_file_content(filename: str, data: bytes) -> None:
    """
    Writes data to a file, creating parent directories if needed.
    
    If the file already exists, it will be truncated.
    
    Args:
        filename: Path where the file should be written
        data: The content to write
        
    Raises:
        IOError: If directories cannot be created or file cannot be written
    """
    path = Path(filename)
    path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(filename, 'wb') as f:
        f.write(data)


def pad_string(s: str, length: int) -> bytes:
    """
    Pads a string to a fixed length with null bytes (0x00).
    
    This is used to create fixed-width fields in the FastDFS protocol.
    If the string is longer than length, it will be truncated.
    
    Args:
        s: The string to pad
        length: The desired length in bytes
        
    Returns:
        Byte array of exactly 'length' bytes
    """
    s_bytes = s.encode('utf-8')
    if len(s_bytes) > length:
        s_bytes = s_bytes[:length]
    
    return s_bytes.ljust(length, b'\x00')


def unpad_string(data: bytes) -> str:
    """
    Removes trailing null bytes from a byte slice.
    
    This is the inverse of pad_string, used to extract strings from
    fixed-width protocol fields.
    
    Args:
        data: Byte array with potential trailing nulls
        
    Returns:
        String with trailing null bytes removed
    """
    return data.rstrip(b'\x00').decode('utf-8', errors='ignore')


def encode_int64(n: int) -> bytes:
    """
    Encodes a 64-bit integer to an 8-byte big-endian representation.
    
    FastDFS protocol uses big-endian byte order for all numeric fields.
    
    Args:
        n: The integer to encode
        
    Returns:
        8-byte array in big-endian format
    """
    return struct.pack('>Q', n)


def decode_int64(data: bytes) -> int:
    """
    Decodes an 8-byte big-endian representation to a 64-bit integer.
    
    This is the inverse of encode_int64.
    
    Args:
        data: Byte array (must be at least 8 bytes)
        
    Returns:
        The decoded integer, or 0 if data is too short
    """
    if len(data) < 8:
        return 0
    return struct.unpack('>Q', data[:8])[0]


def encode_int32(n: int) -> bytes:
    """
    Encodes a 32-bit integer to a 4-byte big-endian representation.
    
    Args:
        n: The integer to encode
        
    Returns:
        4-byte array in big-endian format
    """
    return struct.pack('>I', n)


def decode_int32(data: bytes) -> int:
    """
    Decodes a 4-byte big-endian representation to a 32-bit integer.
    
    Args:
        data: Byte array (must be at least 4 bytes)
        
    Returns:
        The decoded integer, or 0 if data is too short
    """
    if len(data) < 4:
        return 0
    return struct.unpack('>I', data[:4])[0]