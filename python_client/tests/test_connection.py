"""
Unit tests for connection management.
"""

import unittest
import socket
import threading
import time
from fdfs.connection import Connection, ConnectionPool
from fdfs.errors import ClientClosedError


class TestConnection(unittest.TestCase):
    """Test Connection class."""
    
    def test_connection_creation(self):
        """Test creating a connection."""
        # Create a simple echo server for testing
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_sock.bind(('127.0.0.1', 0))
        server_sock.listen(1)
        port = server_sock.getsockname()[1]
        
        def server_thread():
            conn, addr = server_sock.accept()
            data = conn.recv(1024)
            conn.send(data)  # Echo back
            conn.close()
        
        thread = threading.Thread(target=server_thread, daemon=True)
        thread.start()
        
        # Create client connection
        client_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_sock.connect(('127.0.0.1', port))
        
        conn = Connection(client_sock, f'127.0.0.1:{port}')
        
        # Test send and receive
        test_data = b'Hello, FastDFS!'
        conn.send(test_data, timeout=1.0)
        received = conn.receive_full(len(test_data), timeout=1.0)
        
        self.assertEqual(received, test_data)
        
        conn.close()
        server_sock.close()
    
    def test_connection_last_used(self):
        """Test last used timestamp tracking."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        conn = Connection(sock, '127.0.0.1:22122')
        
        initial_time = conn.get_last_used()
        time.sleep(0.1)
        current_time = conn.get_last_used()
        
        self.assertEqual(initial_time, current_time)
        
        conn.close()


class TestConnectionPool(unittest.TestCase):
    """Test ConnectionPool class."""
    
    def test_pool_creation(self):
        """Test creating a connection pool."""
        addrs = ['127.0.0.1:22122', '127.0.0.1:22123']
        pool = ConnectionPool(addrs, max_conns=10)
        
        self.assertEqual(len(pool.addrs), 2)
        self.assertEqual(pool.max_conns, 10)
        
        pool.close()
    
    def test_pool_add_addr(self):
        """Test dynamically adding addresses."""
        pool = ConnectionPool([], max_conns=10)
        
        pool.add_addr('127.0.0.1:22122')
        self.assertIn('127.0.0.1:22122', pool.addrs)
        
        # Adding same address again should be no-op
        pool.add_addr('127.0.0.1:22122')
        self.assertEqual(pool.addrs.count('127.0.0.1:22122'), 1)
        
        pool.close()
    
    def test_pool_close(self):
        """Test closing the pool."""
        pool = ConnectionPool(['127.0.0.1:22122'], max_conns=10)
        pool.close()
        
        # Getting connection after close should raise error
        with self.assertRaises(ClientClosedError):
            pool.get()
    
    def test_pool_close_idempotent(self):
        """Test that closing pool multiple times is safe."""
        pool = ConnectionPool(['127.0.0.1:22122'], max_conns=10)
        pool.close()
        pool.close()  # Should not raise error


if __name__ == '__main__':
    unittest.main()