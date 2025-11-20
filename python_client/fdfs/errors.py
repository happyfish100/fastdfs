"""
FastDFS Error Definitions

This module defines all error types and error handling utilities for the FastDFS client.
Errors are categorized into common errors, protocol errors, network errors, and server errors.
"""


class FastDFSError(Exception):
    """Base exception for all FastDFS errors"""
    pass


class ClientClosedError(FastDFSError):
    """Client has been closed"""
    def __init__(self):
        super().__init__("Client is closed")


class FileNotFoundError(FastDFSError):
    """Requested file does not exist"""
    def __init__(self, file_id: str = ""):
        msg = f"File not found: {file_id}" if file_id else "File not found"
        super().__init__(msg)


class NoStorageServerError(FastDFSError):
    """No storage server is available"""
    def __init__(self):
        super().__init__("No storage server available")


class ConnectionTimeoutError(FastDFSError):
    """Connection timeout"""
    def __init__(self, addr: str = ""):
        msg = f"Connection timeout to {addr}" if addr else "Connection timeout"
        super().__init__(msg)


class NetworkTimeoutError(FastDFSError):
    """Network I/O timeout"""
    def __init__(self, operation: str = ""):
        msg = f"Network timeout during {operation}" if operation else "Network timeout"
        super().__init__(msg)


class InvalidFileIDError(FastDFSError):
    """File ID format is invalid"""
    def __init__(self, file_id: str = ""):
        msg = f"Invalid file ID: {file_id}" if file_id else "Invalid file ID"
        super().__init__(msg)


class InvalidResponseError(FastDFSError):
    """Server response is invalid"""
    def __init__(self, details: str = ""):
        msg = f"Invalid response from server: {details}" if details else "Invalid response from server"
        super().__init__(msg)


class StorageServerOfflineError(FastDFSError):
    """Storage server is offline"""
    def __init__(self, addr: str = ""):
        msg = f"Storage server is offline: {addr}" if addr else "Storage server is offline"
        super().__init__(msg)


class TrackerServerOfflineError(FastDFSError):
    """Tracker server is offline"""
    def __init__(self, addr: str = ""):
        msg = f"Tracker server is offline: {addr}" if addr else "Tracker server is offline"
        super().__init__(msg)


class InsufficientSpaceError(FastDFSError):
    """Insufficient storage space"""
    def __init__(self):
        super().__init__("Insufficient storage space")


class FileAlreadyExistsError(FastDFSError):
    """File already exists"""
    def __init__(self, file_id: str = ""):
        msg = f"File already exists: {file_id}" if file_id else "File already exists"
        super().__init__(msg)


class InvalidMetadataError(FastDFSError):
    """Invalid metadata format"""
    def __init__(self, details: str = ""):
        msg = f"Invalid metadata: {details}" if details else "Invalid metadata"
        super().__init__(msg)


class OperationNotSupportedError(FastDFSError):
    """Operation is not supported"""
    def __init__(self, operation: str = ""):
        msg = f"Operation not supported: {operation}" if operation else "Operation not supported"
        super().__init__(msg)


class InvalidArgumentError(FastDFSError):
    """Invalid argument was provided"""
    def __init__(self, details: str = ""):
        msg = f"Invalid argument: {details}" if details else "Invalid argument"
        super().__init__(msg)


class ProtocolError(FastDFSError):
    """
    Protocol-level error returned by the FastDFS server.
    
    Attributes:
        code: Error code from the protocol status field
        message: Human-readable error description
    """
    def __init__(self, code: int, message: str = ""):
        self.code = code
        self.message = message or f"Unknown error code: {code}"
        super().__init__(f"Protocol error (code {code}): {self.message}")


class NetworkError(FastDFSError):
    """
    Network-related error during communication.
    
    Attributes:
        operation: Operation being performed ("dial", "read", "write")
        addr: Server address where the error occurred
        original_error: Underlying network error
    """
    def __init__(self, operation: str, addr: str, original_error: Exception):
        self.operation = operation
        self.addr = addr
        self.original_error = original_error
        super().__init__(f"Network error during {operation} to {addr}: {original_error}")


def map_status_to_error(status: int) -> Optional[FastDFSError]:
    """
    Maps FastDFS protocol status codes to Python exceptions.
    
    Status code 0 indicates success (no error).
    Other status codes are mapped to predefined errors or a ProtocolError.
    
    Common status codes:
        0: Success
        2: File not found (ENOENT)
        6: File already exists (EEXIST)
        22: Invalid argument (EINVAL)
        28: Insufficient space (ENOSPC)
    
    Args:
        status: The status byte from the protocol header
        
    Returns:
        The corresponding exception, or None for success
    """
    if status == 0:
        return None
    elif status == 2:
        return FileNotFoundError()
    elif status == 6:
        return FileAlreadyExistsError()
    elif status == 22:
        return InvalidArgumentError()
    elif status == 28:
        return InsufficientSpaceError()
    else:
        return ProtocolError(status, f"Unknown error code: {status}")