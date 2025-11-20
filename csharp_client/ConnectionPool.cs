// ============================================================================
// FastDFS Connection Pool
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This file implements a connection pool for managing TCP connections to
// FastDFS servers. The pool provides connection reuse, automatic cleanup of
// idle connections, and thread-safe access for concurrent operations.
//
// ============================================================================

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace FastDFS.Client
{
    /// <summary>
    /// Manages a pool of TCP connections to FastDFS servers.
    /// 
    /// Connection pooling improves performance by reusing TCP connections
    /// across multiple operations, reducing connection overhead and improving
    /// throughput. The pool automatically manages connection lifecycle,
    /// cleans up idle connections, and provides thread-safe access.
    /// 
    /// The pool maintains separate connection queues for each server address,
    /// allowing efficient connection reuse while supporting multiple servers.
    /// </summary>
    internal class ConnectionPool : IDisposable
    {
        // ====================================================================
        // Private Fields
        // ====================================================================

        /// <summary>
        /// Dictionary mapping server addresses to connection queues.
        /// Each server address has its own queue of available connections.
        /// </summary>
        private readonly ConcurrentDictionary<string, ConcurrentQueue<PooledConnection>> _connections;

        /// <summary>
        /// Dictionary tracking the number of active connections per server.
        /// Active connections are connections that are currently in use.
        /// </summary>
        private readonly ConcurrentDictionary<string, int> _activeCounts;

        /// <summary>
        /// Array of server addresses that this pool manages connections for.
        /// </summary>
        private readonly string[] _serverAddresses;

        /// <summary>
        /// Maximum number of connections per server.
        /// </summary>
        private readonly int _maxConnections;

        /// <summary>
        /// Timeout for establishing new connections.
        /// </summary>
        private readonly TimeSpan _connectTimeout;

        /// <summary>
        /// Timeout for network I/O operations.
        /// </summary>
        private readonly TimeSpan _networkTimeout;

        /// <summary>
        /// Timeout for idle connections before they are closed.
        /// </summary>
        private readonly TimeSpan _idleTimeout;

        /// <summary>
        /// Synchronization object for thread-safe operations.
        /// </summary>
        private readonly object _lockObject = new object();

        /// <summary>
        /// Flag indicating whether this pool has been disposed.
        /// </summary>
        private bool _disposed = false;

        /// <summary>
        /// Timer for cleaning up idle connections periodically.
        /// </summary>
        private Timer _cleanupTimer;

        // ====================================================================
        // Constructors
        // ====================================================================

        /// <summary>
        /// Initializes a new instance of the ConnectionPool class.
        /// </summary>
        /// <param name="serverAddresses">
        /// Array of server addresses in "host:port" format.
        /// </param>
        /// <param name="maxConnections">
        /// Maximum number of connections per server.
        /// </param>
        /// <param name="connectTimeout">
        /// Timeout for establishing new connections.
        /// </param>
        /// <param name="networkTimeout">
        /// Timeout for network I/O operations.
        /// </param>
        /// <param name="idleTimeout">
        /// Timeout for idle connections before they are closed.
        /// </param>
        public ConnectionPool(
            string[] serverAddresses,
            int maxConnections,
            TimeSpan connectTimeout,
            TimeSpan networkTimeout,
            TimeSpan idleTimeout)
        {
            _serverAddresses = serverAddresses ?? throw new ArgumentNullException(nameof(serverAddresses));
            _maxConnections = maxConnections;
            _connectTimeout = connectTimeout;
            _networkTimeout = networkTimeout;
            _idleTimeout = idleTimeout;

            _connections = new ConcurrentDictionary<string, ConcurrentQueue<PooledConnection>>();
            _activeCounts = new ConcurrentDictionary<string, int>();

            // Initialize connection queues for each server
            foreach (var address in _serverAddresses)
            {
                _connections[address] = new ConcurrentQueue<PooledConnection>();
                _activeCounts[address] = 0;
            }

            // Start cleanup timer
            _cleanupTimer = new Timer(CleanupIdleConnections, null, idleTimeout, idleTimeout);
        }

        // ====================================================================
        // Public Methods
        // ====================================================================

        /// <summary>
        /// Gets a connection from the pool for the specified server address.
        /// 
        /// This method attempts to reuse an existing connection from the pool.
        /// If no connection is available, it creates a new one (up to the maximum
        /// limit). If the maximum is reached, it waits for a connection to become
        /// available.
        /// </summary>
        /// <param name="address">
        /// Server address in "host:port" format.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task result contains a pooled connection.
        /// </returns>
        public async Task<PooledConnection> GetConnectionAsync(
            string address,
            CancellationToken cancellationToken = default)
        {
            ThrowIfDisposed();

            if (string.IsNullOrWhiteSpace(address))
            {
                throw new ArgumentNullException(nameof(address));
            }

            // Try to get an existing connection from the pool
            if (_connections.TryGetValue(address, out var queue))
            {
                if (queue.TryDequeue(out var connection))
                {
                    // Check if connection is still valid
                    if (IsConnectionValid(connection))
                    {
                        _activeCounts[address] = _activeCounts.GetOrAdd(address, 0) + 1;
                        return connection;
                    }
                    else
                    {
                        // Connection is invalid, close it
                        connection.Dispose();
                    }
                }
            }

            // Check if we can create a new connection
            var activeCount = _activeCounts.GetOrAdd(address, 0);
            if (activeCount < _maxConnections)
            {
                // Create a new connection
                var newConnection = await CreateConnectionAsync(address, cancellationToken);
                _activeCounts[address] = activeCount + 1;
                return newConnection;
            }

            // Wait for a connection to become available
            // This is a simplified implementation - in production, you might
            // want to use SemaphoreSlim or similar for proper waiting
            await Task.Delay(100, cancellationToken);
            return await GetConnectionAsync(address, cancellationToken);
        }

        /// <summary>
        /// Returns a connection to the pool for reuse.
        /// 
        /// This method should be called when a connection is no longer needed.
        /// The connection is returned to the pool and can be reused by other
        /// operations. If the connection is invalid or the pool is full, it
        /// is disposed instead.
        /// </summary>
        /// <param name="connection">
        /// The connection to return to the pool.
        /// </param>
        public void ReturnConnection(PooledConnection connection)
        {
            if (connection == null)
            {
                return;
            }

            ThrowIfDisposed();

            var address = connection.Address;

            // Check if connection is still valid
            if (!IsConnectionValid(connection))
            {
                // Connection is invalid, dispose it
                connection.Dispose();
                _activeCounts[address] = Math.Max(0, _activeCounts.GetOrAdd(address, 0) - 1);
                return;
            }

            // Update last used time
            connection.LastUsed = DateTime.UtcNow;

            // Return to pool if there's space
            if (_connections.TryGetValue(address, out var queue))
            {
                var activeCount = _activeCounts.GetOrAdd(address, 0);
                if (activeCount <= _maxConnections)
                {
                    queue.Enqueue(connection);
                    return;
                }
            }

            // Pool is full or address not found, dispose connection
            connection.Dispose();
            _activeCounts[address] = Math.Max(0, _activeCounts.GetOrAdd(address, 0) - 1);
        }

        // ====================================================================
        // Private Methods
        // ====================================================================

        /// <summary>
        /// Creates a new TCP connection to the specified server address.
        /// </summary>
        private async Task<PooledConnection> CreateConnectionAsync(
            string address,
            CancellationToken cancellationToken)
        {
            var parts = address.Split(':');
            if (parts.Length != 2)
            {
                throw new ArgumentException($"Invalid address format: {address}", nameof(address));
            }

            var host = parts[0];
            if (!int.TryParse(parts[1], out int port))
            {
                throw new ArgumentException($"Invalid port number: {parts[1]}", nameof(address));
            }

            try
            {
                var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                var connectTask = socket.ConnectAsync(host, port);
                var timeoutTask = Task.Delay(_connectTimeout, cancellationToken);

                var completedTask = await Task.WhenAny(connectTask, timeoutTask);
                if (completedTask == timeoutTask)
                {
                    socket.Close();
                    throw new FastDFSConnectionTimeoutException(address, _connectTimeout);
                }

                await connectTask;

                return new PooledConnection
                {
                    Socket = socket,
                    Address = address,
                    LastUsed = DateTime.UtcNow,
                    NetworkTimeout = _networkTimeout
                };
            }
            catch (Exception ex)
            {
                throw new FastDFSNetworkException("connect", address, ex);
            }
        }

        /// <summary>
        /// Checks if a connection is still valid and usable.
        /// </summary>
        private bool IsConnectionValid(PooledConnection connection)
        {
            if (connection == null || connection.Socket == null)
            {
                return false;
            }

            // Check if socket is still connected
            try
            {
                return connection.Socket.Connected;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Cleans up idle connections that have exceeded the idle timeout.
        /// This method is called periodically by the cleanup timer.
        /// </summary>
        private void CleanupIdleConnections(object state)
        {
            if (_disposed)
            {
                return;
            }

            var now = DateTime.UtcNow;
            var expiredConnections = new List<PooledConnection>();

            foreach (var kvp in _connections)
            {
                var address = kvp.Key;
                var queue = kvp.Value;

                // Remove expired connections from queue
                var validConnections = new ConcurrentQueue<PooledConnection>();
                while (queue.TryDequeue(out var connection))
                {
                    if (IsConnectionValid(connection) &&
                        (now - connection.LastUsed) < _idleTimeout)
                    {
                        validConnections.Enqueue(connection);
                    }
                    else
                    {
                        expiredConnections.Add(connection);
                    }
                }

                // Replace queue with valid connections
                _connections[address] = validConnections;

                // Update active count
                var expiredCount = expiredConnections.Count;
                if (expiredCount > 0)
                {
                    _activeCounts[address] = Math.Max(0, _activeCounts.GetOrAdd(address, 0) - expiredCount);
                }
            }

            // Dispose expired connections
            foreach (var connection in expiredConnections)
            {
                try
                {
                    connection.Dispose();
                }
                catch
                {
                    // Ignore disposal errors
                }
            }
        }

        /// <summary>
        /// Throws ObjectDisposedException if this pool has been disposed.
        /// </summary>
        private void ThrowIfDisposed()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(ConnectionPool));
            }
        }

        // ====================================================================
        // IDisposable Implementation
        // ====================================================================

        /// <summary>
        /// Releases all resources used by the ConnectionPool.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Protected implementation of Dispose pattern.
        /// </summary>
        protected virtual void Dispose(bool disposing)
        {
            if (!disposing || _disposed)
            {
                return;
            }

            _disposed = true;

            // Stop cleanup timer
            _cleanupTimer?.Dispose();
            _cleanupTimer = null;

            // Close all connections
            foreach (var queue in _connections.Values)
            {
                while (queue.TryDequeue(out var connection))
                {
                    try
                    {
                        connection.Dispose();
                    }
                    catch
                    {
                        // Ignore disposal errors
                    }
                }
            }

            _connections.Clear();
            _activeCounts.Clear();
        }
    }

    /// <summary>
    /// Represents a pooled TCP connection to a FastDFS server.
    /// </summary>
    internal class PooledConnection : IDisposable
    {
        /// <summary>
        /// Gets or sets the underlying socket connection.
        /// </summary>
        public Socket Socket { get; set; }

        /// <summary>
        /// Gets or sets the server address this connection is for.
        /// </summary>
        public string Address { get; set; }

        /// <summary>
        /// Gets or sets the last time this connection was used.
        /// </summary>
        public DateTime LastUsed { get; set; }

        /// <summary>
        /// Gets or sets the network timeout for I/O operations.
        /// </summary>
        public TimeSpan NetworkTimeout { get; set; }

        /// <summary>
        /// Releases the connection resources.
        /// </summary>
        public void Dispose()
        {
            try
            {
                Socket?.Close();
                Socket?.Dispose();
            }
            catch
            {
                // Ignore disposal errors
            }
        }
    }
}

