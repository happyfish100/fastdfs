"""
FastDFS Connection Management

This module handles TCP connections to FastDFS servers with connection pooling,
automatic reconnection, and health checking.
"""

import socket
import threading
import time
from typing import List, Optional, Dict
from contextlib import contextmanager

from .errors import NetworkError, ConnectionTimeoutError, ClientClosedError


class Connection:
    """
    Represents a TCP connection to a FastDFS server (tracker or storage).
    
    It wraps a socket with additional metadata and thread-safe operations.
    Each connection tracks its last usage time for idle timeout management.
    """
    
    def __init__(self, sock: socket.socket, addr: str):
        """
        Initialize a connection with an established socket.
        
        Args:
            sock: Connected socket
            addr: Server address in "host:port" format
        """
        self.sock = sock
        self.addr = addr
        self.last_used = time.time()
        self.lock = threading.Lock()
    
    def send(self, data: bytes, timeout: float = 30.0) -> None:
        """
        Transmits data to the server with optional timeout.
        
        This method is thread-safe and updates the lastUsed timestamp.
        
        Args:
            data: Bytes to send (must be complete message)
            timeout: Write timeout in seconds (0 means no timeout)
            
        Raises:
            NetworkError: If write fails or incomplete
        """
        with self.lock:
            try:
                if timeout > 0:
                    self.sock.settimeout(timeout)
                
                total_sent = 0
                while total_sent < len(data):
                    sent = self.sock.send(data[total_sent:])
                    if sent == 0:
                        raise NetworkError("write", self.addr, 
                                         Exception("Socket connection broken"))
                    total_sent += sent
                
                self.last_used = time.time()
            except socket.timeout as e:
                raise NetworkError("write", self.addr, e)
            except Exception as e:
                raise NetworkError("write", self.addr, e)
    
    def receive(self, size: int, timeout: float = 30.0) -> bytes:
        """
        Reads up to 'size' bytes from the server.
        
        This method may return fewer bytes than requested.
        Use receive_full if you need exactly 'size' bytes.
        
        Args:
            size: Maximum number of bytes to read
            timeout: Read timeout in seconds
            
        Returns:
            Received data (may be less than 'size')
            
        Raises:
            NetworkError: If read fails
        """
        with self.lock:
            try:
                if timeout > 0:
                    self.sock.settimeout(timeout)
                
                data = self.sock.recv(size)
                if not data:
                    raise NetworkError("read", self.addr, 
                                     Exception("Connection closed by peer"))
                
                self.last_used = time.time()
                return data
            except socket.timeout as e:
                raise NetworkError("read", self.addr, e)
            except Exception as e:
                raise NetworkError("read", self.addr, e)
    
    def receive_full(self, size: int, timeout: float = 30.0) -> bytes:
        """
        Reads exactly 'size' bytes from the server.
        
        This method blocks until all bytes are received or an error occurs.
        The timeout applies to the entire operation, not individual reads.
        
        Args:
            size: Exact number of bytes to read
            timeout: Total timeout for the operation
            
        Returns:
            Exactly 'size' bytes
            
        Raises:
            NetworkError: If read fails before receiving all bytes
        """
        with self.lock:
            try:
                if timeout > 0:
                    self.sock.settimeout(timeout)
                
                data = b''
                while len(data) < size:
                    chunk = self.sock.recv(size - len(data))
                    if not chunk:
                        raise NetworkError("read", self.addr,
                                         Exception("Connection closed by peer"))
                    data += chunk
                
                self.last_used = time.time()
                return data
            except socket.timeout as e:
                raise NetworkError("read", self.addr, e)
            except Exception as e:
                raise NetworkError("read", self.addr, e)
    
    def close(self) -> None:
        """
        Terminates the connection and releases resources.
        
        It's safe to call close multiple times.
        """
        with self.lock:
            try:
                if self.sock:
                    self.sock.close()
            except:
                pass
    
    def is_alive(self) -> bool:
        """
        Performs a non-blocking check to determine if the connection is still valid.
        
        Returns:
            True if connection appears to be alive, False otherwise
        """
        try:
            # Try to peek at the socket without removing data
            self.sock.setblocking(False)
            data = self.sock.recv(1, socket.MSG_PEEK)
            self.sock.setblocking(True)
            return True
        except BlockingIOError:
            # No data available, but connection is alive
            self.sock.setblocking(True)
            return True
        except:
            return False
    
    def get_last_used(self) -> float:
        """
        Returns the timestamp of the last send or receive operation.
        
        Returns:
            Last usage timestamp (Unix time)
        """
        with self.lock:
            return self.last_used
    
    def get_addr(self) -> str:
        """
        Returns the server address this connection is connected to.
        
        Returns:
            Address in "host:port" format
        """
        return self.addr


class ConnectionPool:
    """
    Manages a pool of reusable connections to multiple servers.
    
    It maintains separate pools for each server address and handles:
        - Connection reuse to minimize overhead
        - Idle connection cleanup
        - Thread-safe concurrent access
        - Automatic connection health checking
    """
    
    def __init__(self, addrs: List[str], max_conns: int = 10,
                 connect_timeout: float = 5.0, idle_timeout: float = 60.0):
        """
        Creates a new connection pool for the specified servers.
        
        Args:
            addrs: List of server addresses in "host:port" format
            max_conns: Maximum connections to maintain per server
            connect_timeout: Timeout for establishing new connections
            idle_timeout: How long connections can be idle before cleanup
        """
        self.addrs = addrs
        self.max_conns = max_conns
        self.connect_timeout = connect_timeout
        self.idle_timeout = idle_timeout
        self.pools: Dict[str, List[Connection]] = {}
        self.lock = threading.RLock()
        self.closed = False
        
        # Initialize empty pools for each server
        for addr in addrs:
            self.pools[addr] = []
    
    def get(self, addr: Optional[str] = None) -> Connection:
        """
        Retrieves a connection from the pool or creates a new one.
        
        It prefers reusing existing idle connections but will create new ones if needed.
        Stale connections are automatically discarded.
        
        Args:
            addr: Specific server address, or None to use the first available server
            
        Returns:
            Ready-to-use connection
            
        Raises:
            ClientClosedError: If pool is closed
            NetworkError: If connection cannot be established
        """
        with self.lock:
            if self.closed:
                raise ClientClosedError()
            
            # If no specific address requested, use the first server
            if addr is None:
                if not self.addrs:
                    raise NetworkError("connect", "", Exception("No addresses available"))
                addr = self.addrs[0]
            
            # Ensure pool exists for this address
            if addr not in self.pools:
                self.pools[addr] = []
            
            pool = self.pools[addr]
            
            # Try to reuse an existing connection (LIFO order)
            while pool:
                conn = pool.pop()
                if conn.is_alive():
                    return conn
                conn.close()
            
            # No reusable connection available; create a new one
            return self._create_connection(addr)
    
    def _create_connection(self, addr: str) -> Connection:
        """
        Creates a new TCP connection to a server.
        
        Args:
            addr: Server address in "host:port" format
            
        Returns:
            New connection
            
        Raises:
            NetworkError: If connection fails
        """
        try:
            host, port_str = addr.rsplit(':', 1)
            port = int(port_str)
            
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(self.connect_timeout)
            sock.connect((host, port))
            sock.settimeout(None)  # Reset to blocking mode
            
            return Connection(sock, addr)
        except socket.timeout as e:
            raise ConnectionTimeoutError(addr)
        except Exception as e:
            raise NetworkError("connect", addr, e)
    
    def put(self, conn: Optional[Connection]) -> None:
        """
        Returns a connection to the pool for reuse.
        
        The connection is only kept if:
            - The pool is not closed
            - The pool is not full
            - The connection hasn't been idle too long
        
        Otherwise, the connection is closed.
        
        Args:
            conn: Connection to return (None is safe)
        """
        if conn is None:
            return
        
        with self.lock:
            if self.closed:
                conn.close()
                return
            
            addr = conn.get_addr()
            if addr not in self.pools:
                conn.close()
                return
            
            pool = self.pools[addr]
            
            # Discard connection if pool is at capacity
            if len(pool) >= self.max_conns:
                conn.close()
                return
            
            # Discard connection if it's been idle too long
            if time.time() - conn.get_last_used() > self.idle_timeout:
                conn.close()
                return
            
            # Connection is healthy and pool has space; add it back
            pool.append(conn)
            
            # Trigger periodic cleanup
            self._clean_pool(addr)
    
    def _clean_pool(self, addr: str) -> None:
        """
        Removes stale and dead connections from a server pool.
        
        Args:
            addr: Server address
        """
        if addr not in self.pools:
            return
        
        pool = self.pools[addr]
        now = time.time()
        valid_conns = []
        
        for conn in pool:
            if now - conn.get_last_used() > self.idle_timeout or not conn.is_alive():
                conn.close()
            else:
                valid_conns.append(conn)
        
        self.pools[addr] = valid_conns
    
    def add_addr(self, addr: str) -> None:
        """
        Dynamically adds a new server address to the pool.
        
        This is useful for adding storage servers discovered at runtime.
        If the address already exists, this is a no-op.
        
        Args:
            addr: Server address in "host:port" format
        """
        with self.lock:
            if self.closed:
                return
            
            if addr in self.pools:
                return
            
            self.addrs.append(addr)
            self.pools[addr] = []
    
    def close(self) -> None:
        """
        Shuts down the connection pool and closes all connections.
        
        After close is called, get will raise ClientClosedError.
        It's safe to call close multiple times.
        """
        with self.lock:
            if self.closed:
                return
            
            self.closed = True
            
            for pool in self.pools.values():
                for conn in pool:
                    conn.close()
                pool.clear()