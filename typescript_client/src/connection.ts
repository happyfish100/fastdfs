/**
 * FastDFS Connection Management
 * 
 * This module handles TCP connections to FastDFS servers with connection pooling,
 * automatic reconnection, and health checking.
 */

import * as net from 'net';
import { NetworkError, ConnectionTimeoutError, ClientClosedError } from './errors';

/**
 * Represents a TCP connection to a FastDFS server (tracker or storage)
 * 
 * It wraps a net.Socket with additional metadata and thread-safe operations.
 * Each connection tracks its last usage time for idle timeout management.
 */
export class Connection {
  private socket: net.Socket;
  private addr: string;
  private lastUsed: number;
  private closed: boolean = false;

  constructor(socket: net.Socket, addr: string) {
    this.socket = socket;
    this.addr = addr;
    this.lastUsed = Date.now();
  }

  /**
   * Transmits data to the server with optional timeout
   * 
   * This method updates the lastUsed timestamp.
   */
  async send(data: Buffer, timeout: number = 30000): Promise<void> {
    if (this.closed) {
      throw new NetworkError('write', this.addr, new Error('Connection closed'));
    }

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        reject(new NetworkError('write', this.addr, new Error('Write timeout')));
      }, timeout);

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
   * This method may return fewer bytes than requested.
   * Use receiveFull if you need exactly 'size' bytes.
   */
  async receive(size: number, timeout: number = 30000): Promise<Buffer> {
    if (this.closed) {
      throw new NetworkError('read', this.addr, new Error('Connection closed'));
    }

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        reject(new NetworkError('read', this.addr, new Error('Read timeout')));
      }, timeout);

      const onData = (data: Buffer) => {
        clearTimeout(timer);
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        this.lastUsed = Date.now();
        resolve(data.slice(0, size));
      };

      const onError = (err: Error) => {
        clearTimeout(timer);
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        reject(new NetworkError('read', this.addr, err));
      };

      this.socket.once('data', onData);
      this.socket.once('error', onError);
    });
  }

  /**
   * Reads exactly 'size' bytes from the server
   * 
   * This method blocks until all bytes are received or an error occurs.
   * The timeout applies to the entire operation, not individual reads.
   */
  async receiveFull(size: number, timeout: number = 30000): Promise<Buffer> {
    if (this.closed) {
      throw new NetworkError('read', this.addr, new Error('Connection closed'));
    }

    return new Promise((resolve, reject) => {
      const chunks: Buffer[] = [];
      let totalReceived = 0;

      const timer = setTimeout(() => {
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        reject(new NetworkError('read', this.addr, new Error('Read timeout')));
      }, timeout);

      const onData = (data: Buffer) => {
        chunks.push(data);
        totalReceived += data.length;

        if (totalReceived >= size) {
          clearTimeout(timer);
          this.socket.removeListener('data', onData);
          this.socket.removeListener('error', onError);
          this.lastUsed = Date.now();
          
          const result = Buffer.concat(chunks);
          resolve(result.slice(0, size));
        }
      };

      const onError = (err: Error) => {
        clearTimeout(timer);
        this.socket.removeListener('data', onData);
        this.socket.removeListener('error', onError);
        reject(new NetworkError('read', this.addr, err));
      };

      this.socket.on('data', onData);
      this.socket.once('error', onError);
    });
  }

  /**
   * Terminates the connection and releases resources
   * 
   * It's safe to call close multiple times.
   */
  close(): void {
    if (!this.closed) {
      this.closed = true;
      this.socket.destroy();
    }
  }

  /**
   * Performs a check to determine if the connection is still valid
   */
  isAlive(): boolean {
    return !this.closed && !this.socket.destroyed && this.socket.writable && this.socket.readable;
  }

  /**
   * Returns the timestamp of the last send or receive operation
   */
  getLastUsed(): number {
    return this.lastUsed;
  }

  /**
   * Returns the server address this connection is connected to
   */
  getAddr(): string {
    return this.addr;
  }
}

/**
 * Manages a pool of reusable connections to multiple servers
 * 
 * It maintains separate pools for each server address and handles:
 *   - Connection reuse to minimize overhead
 *   - Idle connection cleanup
 *   - Thread-safe concurrent access
 *   - Automatic connection health checking
 */
export class ConnectionPool {
  private addrs: string[];
  private maxConns: number;
  private connectTimeout: number;
  private idleTimeout: number;
  private pools: Map<string, Connection[]>;
  private closed: boolean = false;

  constructor(
    addrs: string[],
    maxConns: number = 10,
    connectTimeout: number = 5000,
    idleTimeout: number = 60000
  ) {
    this.addrs = addrs;
    this.maxConns = maxConns;
    this.connectTimeout = connectTimeout;
    this.idleTimeout = idleTimeout;
    this.pools = new Map();

    // Initialize empty pools for each server
    for (const addr of addrs) {
      this.pools.set(addr, []);
    }
  }

  /**
   * Retrieves a connection from the pool or creates a new one
   * 
   * It prefers reusing existing idle connections but will create new ones if needed.
   * Stale connections are automatically discarded.
   */
  async get(addr?: string): Promise<Connection> {
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

    const pool = this.pools.get(addr)!;

    // Try to reuse an existing connection (LIFO order)
    while (pool.length > 0) {
      const conn = pool.pop()!;
      if (conn.isAlive()) {
        return conn;
      }
      conn.close();
    }

    // No reusable connection available; create a new one
    return this.createConnection(addr);
  }

  /**
   * Creates a new TCP connection to a server
   */
  private async createConnection(addr: string): Promise<Connection> {
    const [host, portStr] = addr.split(':');
    const port = parseInt(portStr, 10);

    return new Promise((resolve, reject) => {
      const socket = new net.Socket();
      const timer = setTimeout(() => {
        socket.destroy();
        reject(new ConnectionTimeoutError(addr));
      }, this.connectTimeout);

      socket.connect(port, host, () => {
        clearTimeout(timer);
        socket.setNoDelay(true);
        socket.setKeepAlive(true);
        resolve(new Connection(socket, addr));
      });

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
   * 
   * Otherwise, the connection is closed.
   */
  put(conn: Connection | null): void {
    if (!conn) {
      return;
    }

    if (this.closed) {
      conn.close();
      return;
    }

    const addr = conn.getAddr();
    const pool = this.pools.get(addr);

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

    // Connection is healthy and pool has space; add it back
    pool.push(conn);

    // Trigger periodic cleanup
    this.cleanPool(addr);
  }

  /**
   * Removes stale and dead connections from a server pool
   */
  private cleanPool(addr: string): void {
    const pool = this.pools.get(addr);
    if (!pool) return;

    const now = Date.now();
    const validConns: Connection[] = [];

    for (const conn of pool) {
      if (now - conn.getLastUsed() > this.idleTimeout || !conn.isAlive()) {
        conn.close();
      } else {
        validConns.push(conn);
      }
    }

    this.pools.set(addr, validConns);
  }

  /**
   * Dynamically adds a new server address to the pool
   * 
   * This is useful for adding storage servers discovered at runtime.
   * If the address already exists, this is a no-op.
   */
  addAddr(addr: string): void {
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
  close(): void {
    if (this.closed) {
      return;
    }

    this.closed = true;

    for (const pool of this.pools.values()) {
      for (const conn of pool) {
        conn.close();
      }
      pool.length = 0;
    }
  }
}