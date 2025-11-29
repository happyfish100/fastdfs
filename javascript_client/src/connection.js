/**
 * FastDFS Connection Management
 * 
 * This module handles TCP connections to FastDFS servers with connection pooling,
 * automatic reconnection, and health checking.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

'use strict';

const net = require('net');
const { NetworkError, ConnectionTimeoutError, ClientClosedError } = require('./errors');

/**
 * Represents a TCP connection to a FastDFS server (tracker or storage)
 * 
 * It wraps a net.Socket with additional metadata and provides async/await
 * friendly methods for sending and receiving data. Each connection tracks
 * its last usage time for idle timeout management.
 */
class Connection {
  /**
   * Creates a new Connection instance
   * 
   * @param {net.Socket} socket - The underlying TCP socket
   * @param {string} addr - Server address in format "host:port"
   */
  constructor(socket, addr) {
    this.socket = socket;
    this.addr = addr;
    this.lastUsed = Date.now();
    this.closed = false;
  }

  /**
   * Transmits data to the server with optional timeout
   * 
   * This method updates the lastUsed timestamp and returns a promise
   * that resolves when the data has been written to the socket.
   * 
   * @param {Buffer} data - Data to send
   * @param {number} [timeout=30000] - Timeout in milliseconds
   * @returns {Promise<void>}
   * @throws {NetworkError} If write fails or connection is closed
   */
  async send(data, timeout = 30000) {
    if (this.closed) {
      throw new NetworkError('write', this.addr, new Error('Connection closed'));
    }

    return new Promise((resolve, reject) => {
      // Set up timeout timer
      const timer = setTimeout(() => {
        reject(new NetworkError('write', this.addr, new Error('Write timeout')));
      }, timeout);

      // Write data to socket
      this.socket.write(data, (err) => {
        clearTimeout(timer);
        if (err) {
          reject(new NetworkError('write', this.addr, err));
        } else {
          this.lastUsed = Date.now();
          resolve();
        }
      });
    });
  }

  /**
   * Reads up to 'size' bytes from the server
   * 
   * This method may return fewer bytes than requested if the server
   * sends less data. Use receiveFull if you need exactly 'size' bytes.
   * 
   * @param {number} size - Maximum number of bytes to read
   * @param {number} [timeout=30000] - Timeout in milliseconds
   * @returns {Promise<Buffer>} The received data
   * @throws {NetworkError} If read fails or connection is closed
   */
  async receive(size, timeout = 30000) {
    if (this.closed) {
      throw new NetworkError('read', this.addr, new Error('Connection closed'));
    }

    return new Promise((resolve, reject) => {
      // Set up timeout timer
      const timer = setTimeout(() => {
        reject(new NetworkError('read', this.addr, new Error('Read timeout')));
      }, timeout);

      // Set up data handler
      const onData = (data) => {
        clearTimeout(timer);
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        this.lastUsed = Date.now();
        resolve(data.slice(0, size));
      };

      // Set up error handler
      const onError = (err) => {
        clearTimeout(timer);
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        reject(new NetworkError('read', this.addr, err));
      };

      // Attach event listeners
      this.socket.once('data', onData);
      this.socket.once('error', onError);
    });
  }

  /**
   * Reads exactly 'size' bytes from the server
   * 
   * This method blocks until all bytes are received or an error occurs.
   * The timeout applies to the entire operation, not individual reads.
   * This is the most commonly used receive method for protocol communication.
   * 
   * @param {number} size - Exact number of bytes to read
   * @param {number} [timeout=30000] - Timeout in milliseconds
   * @returns {Promise<Buffer>} The received data (exactly 'size' bytes)
   * @throws {NetworkError} If read fails, connection is closed, or timeout occurs
   */
  async receiveFull(size, timeout = 30000) {
    if (this.closed) {
      throw new NetworkError('read', this.addr, new Error('Connection closed'));
    }

    return new Promise((resolve, reject) => {
      const chunks = [];
      let totalReceived = 0;

      // Set up timeout timer
      const timer = setTimeout(() => {
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        reject(new NetworkError('read', this.addr, new Error('Read timeout')));
      }, timeout);

      // Set up data handler - accumulates chunks until we have enough
      const onData = (data) => {
        chunks.push(data);
        totalReceived += data.length;

        // Check if we have received enough data
        if (totalReceived >= size) {
          clearTimeout(timer);
          this.socket.removeListener('data', onData);
          this.socket.removeListener('error', onError);
          this.lastUsed = Date.now();
          
          // Concatenate all chunks and return exactly 'size' bytes
          const result = Buffer.concat(chunks);
          resolve(result.slice(0, size));
        }
      };

      // Set up error handler
      const onError = (err) => {
        clearTimeout(timer);
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        reject(new NetworkError('read', this.addr, err));
      };

      // Attach event listeners
      this.socket.on('data', onData);
      this.socket.once('error', onError);
    });
  }

  /**
   * Terminates the connection and releases resources
   * 
   * It's safe to call close multiple times. Once closed, the connection
   * cannot be reused.
   */
  close() {
    if (!this.closed) {
      this.closed = true;
      this.socket.destroy();
    }
  }

  /**
   * Performs a check to determine if the connection is still valid
   * 
   * A connection is considered alive if it hasn't been closed and
   * the underlying socket is still writable and readable.
   * 
   * @returns {boolean} True if connection is alive, false otherwise
   */
  isAlive() {
    return !this.closed && 
           !this.socket.destroyed && 
           this.socket.writable && 
           this.socket.readable;
  }

  /**
   * Returns the timestamp of the last send or receive operation
   * 
   * This is used by the connection pool to determine if a connection
   * has been idle for too long.
   * 
   * @returns {number} Timestamp in milliseconds
   */
  getLastUsed() {
    return this.lastUsed;
  }

  /**
   * Returns the server address this connection is connected to
   * 
   * @returns {string} Address in format "host:port"
   */
  getAddr() {
    return this.addr;
  }
}

/**
 * Manages a pool of reusable connections to multiple servers
 * 
 * The connection pool maintains separate pools for each server address and handles:
 *   - Connection reuse to minimize overhead
 *   - Idle connection cleanup
 *   - Thread-safe concurrent access
 *   - Automatic connection health checking
 *   - Dynamic server address addition
 * 
 * Connections are stored in a LIFO (Last In, First Out) order to maximize
 * connection reuse and minimize the number of active connections.
 */
class ConnectionPool {
  /**
   * Creates a new ConnectionPool instance
   * 
   * @param {string[]} addrs - List of server addresses in format "host:port"
   * @param {number} [maxConns=10] - Maximum connections per server
   * @param {number} [connectTimeout=5000] - Connection timeout in milliseconds
   * @param {number} [idleTimeout=60000] - Idle timeout in milliseconds
   */
  constructor(addrs, maxConns = 10, connectTimeout = 5000, idleTimeout = 60000) {
    this.addrs = addrs;
    this.maxConns = maxConns;
    this.connectTimeout = connectTimeout;
    this.idleTimeout = idleTimeout;
    this.pools = new Map();
    this.closed = false;

    // Initialize empty pools for each server
    for (const addr of addrs) {
      this.pools.set(addr, []);
    }
  }

  /**
   * Retrieves a connection from the pool or creates a new one
   * 
   * It prefers reusing existing idle connections but will create new ones if needed.
   * Stale connections are automatically discarded. If no specific address is requested,
   * the first server in the list is used.
   * 
   * @param {string} [addr] - Server address to connect to
   * @returns {Promise<Connection>} A connection to the server
   * @throws {ClientClosedError} If the pool has been closed
   * @throws {NetworkError} If no addresses are available
   * @throws {ConnectionTimeoutError} If connection attempt times out
   */
  async get(addr) {
    if (this.closed) {
      throw new ClientClosedError();
    }

    // If no specific address requested, use the first server
    if (!addr) {
      if (this.addrs.length === 0) {
        throw new NetworkError('connect', '', new Error('No addresses available'));
      }
      addr = this.addrs[0];
    }

    // Ensure pool exists for this address
    if (!this.pools.has(addr)) {
      this.pools.set(addr, []);
    }

    const pool = this.pools.get(addr);

    // Try to reuse an existing connection (LIFO order)
    while (pool.length > 0) {
      const conn = pool.pop();
      if (conn.isAlive()) {
        return conn;
      }
      // Connection is dead, close it and try next one
      conn.close();
    }

    // No reusable connection available; create a new one
    return this._createConnection(addr);
  }

  /**
   * Creates a new TCP connection to a server
   * 
   * This is an internal method that establishes a new TCP connection
   * with timeout handling.
   * 
   * @private
   * @param {string} addr - Server address in format "host:port"
   * @returns {Promise<Connection>} A new connection
   * @throws {ConnectionTimeoutError} If connection times out
   * @throws {NetworkError} If connection fails
   */
  async _createConnection(addr) {
    const [host, portStr] = addr.split(':');
    const port = parseInt(portStr, 10);

    return new Promise((resolve, reject) => {
      const socket = new net.Socket();
      
      // Set up connection timeout
      const timer = setTimeout(() => {
        socket.destroy();
        reject(new ConnectionTimeoutError(addr));
      }, this.connectTimeout);

      // Handle successful connection
      socket.connect(port, host, () => {
        clearTimeout(timer);
        // Optimize socket for FastDFS protocol
        socket.setNoDelay(true);  // Disable Nagle's algorithm for low latency
        socket.setKeepAlive(true); // Enable TCP keep-alive
        resolve(new Connection(socket, addr));
      });

      // Handle connection errors
      socket.on('error', (err) => {
        clearTimeout(timer);
        reject(new NetworkError('connect', addr, err));
      });
    });
  }

  /**
   * Returns a connection to the pool for reuse
   * 
   * The connection is only kept if:
   *   - The pool is not closed
   *   - The pool is not full
   *   - The connection hasn't been idle too long
   *   - The connection is still alive
   * 
   * Otherwise, the connection is closed and discarded.
   * 
   * @param {Connection|null} conn - Connection to return to pool
   */
  put(conn) {
    if (!conn) {
      return;
    }

    // Don't accept connections if pool is closed
    if (this.closed) {
      conn.close();
      return;
    }

    const addr = conn.getAddr();
    const pool = this.pools.get(addr);

    // Close connection if pool doesn't exist for this address
    if (!pool) {
      conn.close();
      return;
    }

    // Discard connection if pool is at capacity
    if (pool.length >= this.maxConns) {
      conn.close();
      return;
    }

    // Discard connection if it's been idle too long
    if (Date.now() - conn.getLastUsed() > this.idleTimeout) {
      conn.close();
      return;
    }

    // Discard connection if it's not alive
    if (!conn.isAlive()) {
      conn.close();
      return;
    }

    // Connection is healthy and pool has space; add it back
    pool.push(conn);

    // Trigger periodic cleanup to remove stale connections
    this._cleanPool(addr);
  }

  /**
   * Removes stale and dead connections from a server pool
   * 
   * This method is called periodically when connections are returned
   * to the pool. It removes connections that have been idle too long
   * or are no longer alive.
   * 
   * @private
   * @param {string} addr - Server address
   */
  _cleanPool(addr) {
    const pool = this.pools.get(addr);
    if (!pool) return;

    const now = Date.now();
    const validConns = [];

    // Filter out stale and dead connections
    for (const conn of pool) {
      if (now - conn.getLastUsed() > this.idleTimeout || !conn.isAlive()) {
        conn.close();
      } else {
        validConns.push(conn);
      }
    }

    // Update pool with only valid connections
    this.pools.set(addr, validConns);
  }

  /**
   * Dynamically adds a new server address to the pool
   * 
   * This is useful for adding storage servers discovered at runtime.
   * If the address already exists, this is a no-op.
   * 
   * @param {string} addr - Server address in format "host:port"
   */
  addAddr(addr) {
    if (this.closed) {
      return;
    }

    if (this.pools.has(addr)) {
      return;
    }

    this.addrs.push(addr);
    this.pools.set(addr, []);
  }

  /**
   * Shuts down the connection pool and closes all connections
   * 
   * After close is called, get will throw ClientClosedError.
   * It's safe to call close multiple times.
   */
  close() {
    if (this.closed) {
      return;
    }

    this.closed = true;

    // Close all connections in all pools
    for (const pool of this.pools.values()) {
      for (const conn of pool) {
        conn.close();
      }
      pool.length = 0; // Clear the array
    }
  }
}

// Export classes
module.exports = {
  Connection,
  ConnectionPool,
};
