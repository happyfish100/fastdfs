package fdfs

import (
	"context"
	"fmt"
	"net"
	"sync"
	"time"
)

// Connection represents a TCP connection to a FastDFS server (tracker or storage).
// It wraps a net.Conn with additional metadata and thread-safe operations.
// Each connection tracks its last usage time for idle timeout management.
type Connection struct {
	conn     net.Conn   // underlying TCP connection
	addr     string     // server address in "host:port" format
	lastUsed time.Time  // timestamp of last Send/Receive operation
	mu       sync.Mutex // protects concurrent access to the connection
}

// NewConnection establishes a new TCP connection to a FastDFS server.
// The connection is established with the specified timeout and is ready for use.
//
// Parameters:
//   - addr: server address in "host:port" format (e.g., "192.168.1.100:22122")
//   - timeout: maximum time to wait for connection establishment
//
// Returns:
//   - *Connection: ready-to-use connection
//   - error: NetworkError if connection fails
func NewConnection(addr string, timeout time.Duration) (*Connection, error) {
	conn, err := net.DialTimeout("tcp", addr, timeout)
	if err != nil {
		return nil, &NetworkError{
			Op:   "dial",
			Addr: addr,
			Err:  err,
		}
	}

	return &Connection{
		conn:     conn,
		addr:     addr,
		lastUsed: time.Now(),
	}, nil
}

// Send transmits data to the server with optional timeout.
// This method is thread-safe and updates the lastUsed timestamp.
//
// Parameters:
//   - data: bytes to send (must be complete message)
//   - timeout: write timeout (0 means no timeout)
//
// Returns:
//   - error: NetworkError if write fails or incomplete
func (c *Connection) Send(data []byte, timeout time.Duration) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if timeout > 0 {
		c.conn.SetWriteDeadline(time.Now().Add(timeout))
	}

	n, err := c.conn.Write(data)
	if err != nil {
		return &NetworkError{
			Op:   "write",
			Addr: c.addr,
			Err:  err,
		}
	}

	if n != len(data) {
		return &NetworkError{
			Op:   "write",
			Addr: c.addr,
			Err:  fmt.Errorf("incomplete write: %d/%d bytes", n, len(data)),
		}
	}

	c.lastUsed = time.Now()
	return nil
}

// Receive reads up to 'size' bytes from the server.
// This method may return fewer bytes than requested.
// Use ReceiveFull if you need exactly 'size' bytes.
//
// Parameters:
//   - size: maximum number of bytes to read
//   - timeout: read timeout (0 means no timeout)
//
// Returns:
//   - []byte: received data (may be less than 'size')
//   - error: NetworkError if read fails
func (c *Connection) Receive(size int, timeout time.Duration) ([]byte, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if timeout > 0 {
		c.conn.SetReadDeadline(time.Now().Add(timeout))
	}

	buf := make([]byte, size)
	n, err := c.conn.Read(buf)
	if err != nil {
		return nil, &NetworkError{
			Op:   "read",
			Addr: c.addr,
			Err:  err,
		}
	}

	c.lastUsed = time.Now()
	return buf[:n], nil
}

// ReceiveFull reads exactly 'size' bytes from the server.
// This method blocks until all bytes are received or an error occurs.
// The timeout applies to the entire operation, not individual reads.
//
// Parameters:
//   - size: exact number of bytes to read
//   - timeout: total timeout for the operation
//
// Returns:
//   - []byte: exactly 'size' bytes
//   - error: NetworkError if read fails before receiving all bytes
func (c *Connection) ReceiveFull(size int, timeout time.Duration) ([]byte, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if timeout > 0 {
		c.conn.SetReadDeadline(time.Now().Add(timeout))
	}

	buf := make([]byte, size)
	offset := 0

	// Read in a loop until we have all requested bytes
	for offset < size {
		n, err := c.conn.Read(buf[offset:])
		if err != nil {
			return nil, &NetworkError{
				Op:   "read",
				Addr: c.addr,
				Err:  err,
			}
		}
		offset += n
	}

	c.lastUsed = time.Now()
	return buf, nil
}

// Close terminates the connection and releases resources.
// It's safe to call Close multiple times.
//
// Returns an error if the underlying connection close fails.
func (c *Connection) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.conn != nil {
		return c.conn.Close()
	}
	return nil
}

// IsAlive performs a non-blocking check to determine if the connection is still valid.
// It attempts a 1ms read with timeout; if it times out, the connection is considered alive.
// This is a heuristic check and may not detect all failure modes.
//
// Returns:
//   - true: connection appears to be alive
//   - false: connection is closed or broken
func (c *Connection) IsAlive() bool {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.conn == nil {
		return false
	}

	// Try to set a deadline to check if connection is alive.
	// We attempt a read with a very short timeout.
	// If it times out, the connection is still open.
	one := []byte{0}
	c.conn.SetReadDeadline(time.Now().Add(1 * time.Millisecond))
	_, err := c.conn.Read(one)
	c.conn.SetReadDeadline(time.Time{}) // clear deadline

	// If we get a timeout, the connection is alive
	if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
		return true
	}

	return err == nil
}

// LastUsed returns the timestamp of the last Send or Receive operation.
// This is used by the connection pool for idle timeout management.
//
// Returns the last usage timestamp.
func (c *Connection) LastUsed() time.Time {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.lastUsed
}

// Addr returns the server address this connection is connected to.
//
// Returns the address in "host:port" format.
func (c *Connection) Addr() string {
	return c.addr
}

// ConnectionPool manages a pool of reusable connections to multiple servers.
// It maintains separate pools for each server address and handles:
//   - Connection reuse to minimize overhead
//   - Idle connection cleanup
//   - Thread-safe concurrent access
//   - Automatic connection health checking
type ConnectionPool struct {
	addrs          []string               // list of server addresses
	maxConns       int                    // max connections per server
	connectTimeout time.Duration          // timeout for new connections
	idleTimeout    time.Duration          // max idle time before cleanup
	pools          map[string]*serverPool // per-server connection pools
	mu             sync.RWMutex           // protects pools map and closed flag
	closed         bool                   // true if pool is closed
}

// serverPool holds connections for a single server.
// It's an internal structure used by ConnectionPool.
type serverPool struct {
	addr      string        // server address
	conns     []*Connection // available connections (LIFO stack)
	mu        sync.Mutex    // protects conns slice
	lastClean time.Time     // last time idle connections were cleaned
}

// NewConnectionPool creates a new connection pool for the specified servers.
// The pool starts empty; connections are created on-demand when Get is called.
// If addrs is empty, servers can be added later using AddAddr.
//
// Parameters:
//   - addrs: list of server addresses in "host:port" format (can be empty)
//   - maxConns: maximum connections to maintain per server
//   - connectTimeout: timeout for establishing new connections
//   - idleTimeout: how long connections can be idle before cleanup
//
// Returns:
//   - *ConnectionPool: initialized pool
//   - error: never returns error (for API compatibility)
func NewConnectionPool(addrs []string, maxConns int, connectTimeout, idleTimeout time.Duration) (*ConnectionPool, error) {

	pool := &ConnectionPool{
		addrs:          addrs,
		maxConns:       maxConns,
		connectTimeout: connectTimeout,
		idleTimeout:    idleTimeout,
		pools:          make(map[string]*serverPool),
	}

	// Initialize empty pools for each server address
	for _, addr := range addrs {
		pool.pools[addr] = &serverPool{
			addr:      addr,
			conns:     make([]*Connection, 0, maxConns),
			lastClean: time.Now(),
		}
	}

	return pool, nil
}

// Get retrieves a connection from the pool or creates a new one.
// It prefers reusing existing idle connections but will create new ones if needed.
// Stale connections are automatically discarded.
//
// Parameters:
//   - ctx: context for cancellation (currently not used, for future enhancement)
//   - addr: specific server address, or "" to use the first available server
//
// Returns:
//   - *Connection: ready-to-use connection
//   - error: if pool is closed or connection cannot be established
func (p *ConnectionPool) Get(ctx context.Context, addr string) (*Connection, error) {
	p.mu.RLock()
	if p.closed {
		p.mu.RUnlock()
		return nil, ErrClientClosed
	}
	p.mu.RUnlock()

	// If no specific address requested, use the first server in the list
	if addr == "" {
		if len(p.addrs) == 0 {
			return nil, fmt.Errorf("no addresses available")
		}
		addr = p.addrs[0]
	}

	p.mu.RLock()
	sp, ok := p.pools[addr]
	p.mu.RUnlock()

	if !ok {
		// Server not in pool yet; create a new pool for it dynamically
		p.mu.Lock()
		sp = &serverPool{
			addr:      addr,
			conns:     make([]*Connection, 0, p.maxConns),
			lastClean: time.Now(),
		}
		p.pools[addr] = sp
		p.mu.Unlock()
	}

	// Try to reuse an existing connection from the pool (LIFO order)
	sp.mu.Lock()
	for len(sp.conns) > 0 {
		conn := sp.conns[len(sp.conns)-1]
		sp.conns = sp.conns[:len(sp.conns)-1]
		sp.mu.Unlock()

		// Verify the connection is still healthy before returning it
		if conn.IsAlive() {
			return conn, nil
		}
		conn.Close()

		sp.mu.Lock()
	}
	sp.mu.Unlock()

	// No reusable connection available; create a new one
	conn, err := NewConnection(addr, p.connectTimeout)
	if err != nil {
		return nil, err
	}

	return conn, nil
}

// Put returns a connection to the pool for reuse.
// The connection is only kept if:
//   - The pool is not closed
//   - The pool is not full
//   - The connection hasn't been idle too long
//
// Otherwise, the connection is closed.
//
// Parameters:
//   - conn: connection to return (nil is safe)
//
// Returns an error if closing the connection fails.
func (p *ConnectionPool) Put(conn *Connection) error {
	if conn == nil {
		return nil
	}

	p.mu.RLock()
	if p.closed {
		p.mu.RUnlock()
		return conn.Close()
	}

	sp, ok := p.pools[conn.Addr()]
	p.mu.RUnlock()

	if !ok {
		return conn.Close()
	}

	sp.mu.Lock()
	defer sp.mu.Unlock()

	// Discard connection if pool is at capacity
	if len(sp.conns) >= p.maxConns {
		return conn.Close()
	}

	// Discard connection if it's been idle too long
	if time.Since(conn.LastUsed()) > p.idleTimeout {
		return conn.Close()
	}

	// Connection is healthy and pool has space; add it back
	sp.conns = append(sp.conns, conn)

	// Trigger periodic cleanup if it's been a while
	if time.Since(sp.lastClean) > p.idleTimeout {
		p.cleanPool(sp)
	}

	return nil
}

// cleanPool removes stale and dead connections from a server pool.
// This is called periodically when connections are returned to the pool.
// The serverPool must be locked by the caller.
//
// Parameters:
//   - sp: the server pool to clean
func (p *ConnectionPool) cleanPool(sp *serverPool) {
	now := time.Now()
	validConns := make([]*Connection, 0, len(sp.conns))

	// Check each connection and keep only the healthy ones
	for _, conn := range sp.conns {
		if now.Sub(conn.LastUsed()) > p.idleTimeout || !conn.IsAlive() {
			conn.Close()
		} else {
			validConns = append(validConns, conn)
		}
	}

	sp.conns = validConns
	sp.lastClean = now
}

// AddAddr dynamically adds a new server address to the pool.
// This is useful for adding storage servers discovered at runtime.
// If the address already exists, this is a no-op.
//
// Parameters:
//   - addr: server address in "host:port" format
func (p *ConnectionPool) AddAddr(addr string) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		return
	}

	// Check if address already exists
	for _, a := range p.addrs {
		if a == addr {
			return
		}
	}

	p.addrs = append(p.addrs, addr)
	p.pools[addr] = &serverPool{
		addr:      addr,
		conns:     make([]*Connection, 0, p.maxConns),
		lastClean: time.Now(),
	}
}

// Close shuts down the connection pool and closes all connections.
// After Close is called, Get will return ErrClientClosed.
// It's safe to call Close multiple times.
//
// Returns nil on success, or an error if closing connections fails.
func (p *ConnectionPool) Close() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.closed {
		return nil
	}

	p.closed = true

	for _, sp := range p.pools {
		sp.mu.Lock()
		for _, conn := range sp.conns {
			conn.Close()
		}
		sp.conns = nil
		sp.mu.Unlock()
	}

	return nil
}
