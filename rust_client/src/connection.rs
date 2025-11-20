//! FastDFS Connection Management
//!
//! This module handles TCP connections to FastDFS servers with connection pooling,
//! automatic reconnection, and health checking.

use bytes::Bytes;
use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio::sync::Mutex;
use tokio::time::timeout;

use crate::errors::{FastDFSError, Result};

/// Represents a TCP connection to a FastDFS server (tracker or storage)
///
/// It wraps a TcpStream with additional metadata and async operations.
/// Each connection tracks its last usage time for idle timeout management.
pub struct Connection {
    stream: TcpStream,
    addr: String,
    last_used: Instant,
}

impl Connection {
    /// Creates a new connection with an established TCP stream
    pub fn new(stream: TcpStream, addr: String) -> Self {
        Self {
            stream,
            addr,
            last_used: Instant::now(),
        }
    }

    /// Transmits data to the server with optional timeout
    ///
    /// This method updates the last_used timestamp.
    pub async fn send(&mut self, data: &[u8], timeout_ms: u64) -> Result<()> {
        let result = timeout(
            Duration::from_millis(timeout_ms),
            self.stream.write_all(data),
        )
        .await;

        match result {
            Ok(Ok(())) => {
                self.last_used = Instant::now();
                Ok(())
            }
            Ok(Err(e)) => Err(FastDFSError::Network {
                operation: "write".to_string(),
                addr: self.addr.clone(),
                source: e,
            }),
            Err(_) => Err(FastDFSError::NetworkTimeout("write".to_string())),
        }
    }

    /// Reads exactly 'size' bytes from the server
    ///
    /// This method blocks until all bytes are received or an error occurs.
    /// The timeout applies to the entire operation, not individual reads.
    pub async fn receive_full(&mut self, size: usize, timeout_ms: u64) -> Result<Bytes> {
        let mut buf = vec![0u8; size];

        let result = timeout(
            Duration::from_millis(timeout_ms),
            self.stream.read_exact(&mut buf),
        )
        .await;

        match result {
            Ok(Ok(())) => {
                self.last_used = Instant::now();
                Ok(Bytes::from(buf))
            }
            Ok(Err(e)) => Err(FastDFSError::Network {
                operation: "read".to_string(),
                addr: self.addr.clone(),
                source: e,
            }),
            Err(_) => Err(FastDFSError::NetworkTimeout("read".to_string())),
        }
    }

    /// Returns the timestamp of the last send or receive operation
    pub fn last_used(&self) -> Instant {
        self.last_used
    }

    /// Returns the server address this connection is connected to
    pub fn addr(&self) -> &str {
        &self.addr
    }
}

/// Manages a pool of reusable connections to multiple servers
///
/// It maintains separate pools for each server address and handles:
///   - Connection reuse to minimize overhead
///   - Idle connection cleanup
///   - Thread-safe concurrent access
///   - Automatic connection health checking
pub struct ConnectionPool {
    addrs: Vec<String>,
    max_conns: usize,
    connect_timeout: Duration,
    idle_timeout: Duration,
    pools: Arc<Mutex<HashMap<String, Vec<Connection>>>>,
    closed: Arc<Mutex<bool>>,
}

impl ConnectionPool {
    /// Creates a new connection pool for the specified servers
    ///
    /// The pool starts empty; connections are created on-demand when get is called.
    pub fn new(
        addrs: Vec<String>,
        max_conns: usize,
        connect_timeout: Duration,
        idle_timeout: Duration,
    ) -> Self {
        let mut pools = HashMap::new();
        for addr in &addrs {
            pools.insert(addr.clone(), Vec::new());
        }

        Self {
            addrs,
            max_conns,
            connect_timeout,
            idle_timeout,
            pools: Arc::new(Mutex::new(pools)),
            closed: Arc::new(Mutex::new(false)),
        }
    }

    /// Retrieves a connection from the pool or creates a new one
    ///
    /// It prefers reusing existing idle connections but will create new ones if needed.
    /// Stale connections are automatically discarded.
    pub async fn get(&self, addr: Option<&str>) -> Result<Connection> {
        // Check if closed
        {
            let closed = self.closed.lock().await;
            if *closed {
                return Err(FastDFSError::ClientClosed);
            }
        }

        // Determine address to use
        let addr = match addr {
            Some(a) => a.to_string(),
            None => {
                if self.addrs.is_empty() {
                    return Err(FastDFSError::InvalidArgument(
                        "No addresses available".to_string(),
                    ));
                }
                self.addrs[0].clone()
            }
        };

        // Try to get connection from pool
        {
            let mut pools = self.pools.lock().await;

            // Ensure pool exists for this address
            if !pools.contains_key(&addr) {
                pools.insert(addr.clone(), Vec::new());
            }

            let pool = pools.get_mut(&addr).unwrap();

            // Try to reuse an existing connection (LIFO order)
            while let Some(conn) = pool.pop() {
                // Check if connection is still fresh
                if conn.last_used().elapsed() < self.idle_timeout {
                    return Ok(conn);
                }
                // Connection is stale, drop it
            }
        }

        // No reusable connection available; create a new one
        self.create_connection(&addr).await
    }

    /// Creates a new TCP connection to a server
    async fn create_connection(&self, addr: &str) -> Result<Connection> {
        let result = timeout(self.connect_timeout, TcpStream::connect(addr)).await;

        match result {
            Ok(Ok(stream)) => {
                stream.set_nodelay(true)?;
                Ok(Connection::new(stream, addr.to_string()))
            }
            Ok(Err(e)) => Err(FastDFSError::Network {
                operation: "connect".to_string(),
                addr: addr.to_string(),
                source: e,
            }),
            Err(_) => Err(FastDFSError::ConnectionTimeout(addr.to_string())),
        }
    }

    /// Returns a connection to the pool for reuse
    ///
    /// The connection is only kept if:
    ///   - The pool is not closed
    ///   - The pool is not full
    ///   - The connection hasn't been idle too long
    ///
    /// Otherwise, the connection is dropped.
    pub async fn put(&self, conn: Connection) {
        // Check if closed
        {
            let closed = self.closed.lock().await;
            if *closed {
                return;
            }
        }

        let addr = conn.addr().to_string();

        let mut pools = self.pools.lock().await;

        if let Some(pool) = pools.get_mut(&addr) {
            // Discard connection if pool is at capacity
            if pool.len() >= self.max_conns {
                return;
            }

            // Discard connection if it's been idle too long
            if conn.last_used().elapsed() > self.idle_timeout {
                return;
            }

            // Connection is healthy and pool has space; add it back
            pool.push(conn);

            // Trigger periodic cleanup
            self.clean_pool(&mut pool);
        }
    }

    /// Removes stale connections from a server pool
    fn clean_pool(&self, pool: &mut Vec<Connection>) {
        let now = Instant::now();
        pool.retain(|conn| now.duration_since(conn.last_used()) <= self.idle_timeout);
    }

    /// Dynamically adds a new server address to the pool
    ///
    /// This is useful for adding storage servers discovered at runtime.
    /// If the address already exists, this is a no-op.
    pub async fn add_addr(&self, addr: String) {
        let closed = self.closed.lock().await;
        if *closed {
            return;
        }
        drop(closed);

        let mut pools = self.pools.lock().await;
        if !pools.contains_key(&addr) {
            pools.insert(addr, Vec::new());
        }
    }

    /// Shuts down the connection pool and closes all connections
    ///
    /// After close is called, get will return ClientClosed error.
    /// It's safe to call close multiple times.
    pub async fn close(&self) {
        let mut closed = self.closed.lock().await;
        if *closed {
            return;
        }
        *closed = true;
        drop(closed);

        let mut pools = self.pools.lock().await;
        pools.clear();
    }
}