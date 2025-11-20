// ============================================================================
// FastDFS Client Configuration
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This file defines the configuration class for the FastDFS client.
// Configuration includes tracker server addresses, connection pool settings,
// timeout values, retry counts, and other operational parameters.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FastDFS.Client
{
    /// <summary>
    /// Configuration class for FastDFS client initialization.
    /// 
    /// This class contains all configurable parameters that control the
    /// behavior of the FastDFS client, including network settings, connection
    /// pool management, retry logic, and timeout values.
    /// 
    /// Example usage:
    /// <code>
    /// var config = new FastDFSClientConfig
    /// {
    ///     TrackerAddresses = new[] { "192.168.1.100:22122", "192.168.1.101:22122" },
    ///     MaxConnections = 100,
    ///     ConnectTimeout = TimeSpan.FromSeconds(5),
    ///     NetworkTimeout = TimeSpan.FromSeconds(30),
    ///     IdleTimeout = TimeSpan.FromMinutes(5),
    ///     RetryCount = 3
    /// };
    /// </code>
    /// </summary>
    public class FastDFSClientConfig
    {
        // ====================================================================
        // Private Fields
        // ====================================================================

        /// <summary>
        /// Array of tracker server addresses in "host:port" format.
        /// At least one tracker address must be specified.
        /// </summary>
        private string[] _trackerAddresses;

        /// <summary>
        /// Maximum number of connections per server in the connection pool.
        /// Default value is 10 connections per server.
        /// </summary>
        private int _maxConnections = 10;

        /// <summary>
        /// Timeout for establishing TCP connections to servers.
        /// Default value is 5 seconds.
        /// </summary>
        private TimeSpan _connectTimeout = TimeSpan.FromSeconds(5);

        /// <summary>
        /// Timeout for network I/O operations (read/write).
        /// Default value is 30 seconds.
        /// </summary>
        private TimeSpan _networkTimeout = TimeSpan.FromSeconds(30);

        /// <summary>
        /// Timeout for idle connections in the connection pool.
        /// Idle connections are closed after this timeout to free resources.
        /// Default value is 5 minutes.
        /// </summary>
        private TimeSpan _idleTimeout = TimeSpan.FromMinutes(5);

        /// <summary>
        /// Number of retry attempts for failed operations.
        /// Default value is 3 attempts.
        /// </summary>
        private int _retryCount = 3;

        /// <summary>
        /// Enable or disable connection pooling.
        /// When enabled, connections are reused across operations.
        /// Default value is true.
        /// </summary>
        private bool _enablePool = true;

        // ====================================================================
        // Public Properties
        // ====================================================================

        /// <summary>
        /// Gets or sets the tracker server addresses.
        /// 
        /// Tracker servers are the coordination servers in a FastDFS cluster.
        /// They maintain information about storage servers and handle client
        /// queries for file locations. At least one tracker address must be
        /// specified. Multiple trackers provide redundancy and load balancing.
        /// 
        /// Addresses should be in "host:port" format (e.g., "192.168.1.100:22122").
        /// The default tracker port is 22122.
        /// </summary>
        /// <value>
        /// Array of tracker server addresses. Must not be null or empty.
        /// </value>
        /// <exception cref="ArgumentNullException">
        /// Thrown when setting to null or empty array.
        /// </exception>
        public string[] TrackerAddresses
        {
            get => _trackerAddresses;
            set
            {
                if (value == null || value.Length == 0)
                {
                    throw new ArgumentNullException(
                        nameof(TrackerAddresses),
                        "At least one tracker server address must be specified.");
                }

                // Validate each address format
                foreach (var address in value)
                {
                    if (string.IsNullOrWhiteSpace(address))
                    {
                        throw new ArgumentException(
                            "Tracker addresses cannot contain null or empty strings.",
                            nameof(TrackerAddresses));
                    }

                    // Basic format validation (host:port)
                    var parts = address.Split(':');
                    if (parts.Length != 2)
                    {
                        throw new ArgumentException(
                            $"Invalid tracker address format: {address}. Expected format: host:port",
                            nameof(TrackerAddresses));
                    }

                    // Validate port number
                    if (!int.TryParse(parts[1], out int port) || port <= 0 || port > 65535)
                    {
                        throw new ArgumentException(
                            $"Invalid port number in tracker address: {address}",
                            nameof(TrackerAddresses));
                    }
                }

                _trackerAddresses = value;
            }
        }

        /// <summary>
        /// Gets or sets the maximum number of connections per server in the
        /// connection pool.
        /// 
        /// This value controls how many TCP connections can be maintained
        /// simultaneously to each server (tracker or storage). Higher values
        /// allow more concurrent operations but consume more system resources.
        /// 
        /// The default value is 10 connections per server. For high-throughput
        /// scenarios, consider increasing this value. For low-resource
        /// environments, decrease it.
        /// </summary>
        /// <value>
        /// Maximum number of connections per server. Must be greater than 0.
        /// </value>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when setting to a value less than or equal to 0.
        /// </exception>
        public int MaxConnections
        {
            get => _maxConnections;
            set
            {
                if (value <= 0)
                {
                    throw new ArgumentOutOfRangeException(
                        nameof(MaxConnections),
                        value,
                        "Maximum connections must be greater than 0.");
                }

                _maxConnections = value;
            }
        }

        /// <summary>
        /// Gets or sets the timeout for establishing TCP connections to servers.
        /// 
        /// This timeout applies when creating new connections to tracker or
        /// storage servers. If a connection cannot be established within this
        /// time, the operation will fail.
        /// 
        /// The default value is 5 seconds. Increase this value for slow
        /// networks or decrease it for faster failure detection.
        /// </summary>
        /// <value>
        /// Connection timeout. Must be greater than TimeSpan.Zero.
        /// </value>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when setting to TimeSpan.Zero or negative value.
        /// </exception>
        public TimeSpan ConnectTimeout
        {
            get => _connectTimeout;
            set
            {
                if (value <= TimeSpan.Zero)
                {
                    throw new ArgumentOutOfRangeException(
                        nameof(ConnectTimeout),
                        value,
                        "Connect timeout must be greater than TimeSpan.Zero.");
                }

                _connectTimeout = value;
            }
        }

        /// <summary>
        /// Gets or sets the timeout for network I/O operations (read/write).
        /// 
        /// This timeout applies to all network read and write operations when
        /// communicating with FastDFS servers. If an operation does not complete
        /// within this time, it will be aborted and an exception will be thrown.
        /// 
        /// The default value is 30 seconds. For large file operations, consider
        /// increasing this value. For faster failure detection, decrease it.
        /// </summary>
        /// <value>
        /// Network I/O timeout. Must be greater than TimeSpan.Zero.
        /// </value>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when setting to TimeSpan.Zero or negative value.
        /// </exception>
        public TimeSpan NetworkTimeout
        {
            get => _networkTimeout;
            set
            {
                if (value <= TimeSpan.Zero)
                {
                    throw new ArgumentOutOfRangeException(
                        nameof(NetworkTimeout),
                        value,
                        "Network timeout must be greater than TimeSpan.Zero.");
                }

                _networkTimeout = value;
            }
        }

        /// <summary>
        /// Gets or sets the timeout for idle connections in the connection pool.
        /// 
        /// Idle connections are connections that have not been used for a
        /// period of time. These connections are automatically closed after
        /// the idle timeout to free system resources. When a connection is
        /// needed again, a new one will be created.
        /// 
        /// The default value is 5 minutes. Increase this value to reduce
        /// connection churn, or decrease it to free resources faster.
        /// </summary>
        /// <value>
        /// Idle connection timeout. Must be greater than TimeSpan.Zero.
        /// </value>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when setting to TimeSpan.Zero or negative value.
        /// </exception>
        public TimeSpan IdleTimeout
        {
            get => _idleTimeout;
            set
            {
                if (value <= TimeSpan.Zero)
                {
                    throw new ArgumentOutOfRangeException(
                        nameof(IdleTimeout),
                        value,
                        "Idle timeout must be greater than TimeSpan.Zero.");
                }

                _idleTimeout = value;
            }
        }

        /// <summary>
        /// Gets or sets the number of retry attempts for failed operations.
        /// 
        /// When an operation fails due to a transient error (network timeout,
        /// server temporarily unavailable, etc.), the client will automatically
        /// retry the operation up to this many times. Each retry uses exponential
        /// backoff to avoid overwhelming the server.
        /// 
        /// The default value is 3 attempts. Increase this value for unreliable
        /// networks, or decrease it for faster failure reporting.
        /// 
        /// Note: Certain errors (e.g., invalid arguments, file not found) are
        /// not retried regardless of this setting.
        /// </summary>
        /// <value>
        /// Number of retry attempts. Must be greater than or equal to 0.
        /// </value>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when setting to a negative value.
        /// </exception>
        public int RetryCount
        {
            get => _retryCount;
            set
            {
                if (value < 0)
                {
                    throw new ArgumentOutOfRangeException(
                        nameof(RetryCount),
                        value,
                        "Retry count must be greater than or equal to 0.");
                }

                _retryCount = value;
            }
        }

        /// <summary>
        /// Gets or sets whether connection pooling is enabled.
        /// 
        /// When connection pooling is enabled, TCP connections to servers are
        /// reused across multiple operations, improving performance and reducing
        /// connection overhead. When disabled, a new connection is created for
        /// each operation and closed immediately after use.
        /// 
        /// The default value is true (enabled). Disable only for debugging
        /// or special use cases.
        /// </summary>
        /// <value>
        /// True if connection pooling is enabled, false otherwise.
        /// </value>
        public bool EnablePool
        {
            get => _enablePool;
            set => _enablePool = value;
        }

        // ====================================================================
        // Constructors
        // ====================================================================

        /// <summary>
        /// Initializes a new instance of the FastDFSClientConfig class with
        /// default values for all settings.
        /// 
        /// After construction, you must set the TrackerAddresses property
        /// before using the configuration to create a client.
        /// </summary>
        public FastDFSClientConfig()
        {
            // All fields are initialized with default values
            // TrackerAddresses must be set by the caller
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSClientConfig class with
        /// the specified tracker addresses and default values for other settings.
        /// 
        /// This constructor provides a convenient way to create a configuration
        /// with the minimum required settings.
        /// </summary>
        /// <param name="trackerAddresses">
        /// Array of tracker server addresses in "host:port" format.
        /// Must not be null or empty.
        /// </param>
        /// <exception cref="ArgumentNullException">
        /// Thrown when trackerAddresses is null or empty.
        /// </exception>
        public FastDFSClientConfig(string[] trackerAddresses)
        {
            TrackerAddresses = trackerAddresses;
        }

        // ====================================================================
        // Validation Methods
        // ====================================================================

        /// <summary>
        /// Validates the configuration and throws an exception if invalid.
        /// 
        /// This method checks that all required properties are set and that
        /// all values are within acceptable ranges. It is called automatically
        /// by the FastDFSClient constructor, but can also be called manually
        /// to validate configuration before use.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// Thrown when the configuration is invalid.
        /// </exception>
        public void Validate()
        {
            if (_trackerAddresses == null || _trackerAddresses.Length == 0)
            {
                throw new InvalidOperationException(
                    "TrackerAddresses must be set before using the configuration.");
            }

            if (_maxConnections <= 0)
            {
                throw new InvalidOperationException(
                    "MaxConnections must be greater than 0.");
            }

            if (_connectTimeout <= TimeSpan.Zero)
            {
                throw new InvalidOperationException(
                    "ConnectTimeout must be greater than TimeSpan.Zero.");
            }

            if (_networkTimeout <= TimeSpan.Zero)
            {
                throw new InvalidOperationException(
                    "NetworkTimeout must be greater than TimeSpan.Zero.");
            }

            if (_idleTimeout <= TimeSpan.Zero)
            {
                throw new InvalidOperationException(
                    "IdleTimeout must be greater than TimeSpan.Zero.");
            }

            if (_retryCount < 0)
            {
                throw new InvalidOperationException(
                    "RetryCount must be greater than or equal to 0.");
            }
        }

        // ====================================================================
        // Helper Methods
        // ====================================================================

        /// <summary>
        /// Creates a copy of this configuration object.
        /// 
        /// This method creates a new FastDFSClientConfig instance with the
        /// same property values as this instance. The copy is independent
        /// and can be modified without affecting the original.
        /// </summary>
        /// <returns>
        /// A new FastDFSClientConfig instance with copied property values.
        /// </returns>
        public FastDFSClientConfig Clone()
        {
            return new FastDFSClientConfig
            {
                TrackerAddresses = _trackerAddresses?.ToArray(),
                MaxConnections = _maxConnections,
                ConnectTimeout = _connectTimeout,
                NetworkTimeout = _networkTimeout,
                IdleTimeout = _idleTimeout,
                RetryCount = _retryCount,
                EnablePool = _enablePool
            };
        }
    }
}

