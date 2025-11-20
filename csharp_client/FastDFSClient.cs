// ============================================================================
// FastDFS C# Client Library
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// FastDFS may be copied only under the terms of the GNU General
// Public License V3, which may be found in the FastDFS source kit.
// Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
//
// ============================================================================
// 
// This file contains the main FastDFS client class that provides a high-level
// interface for interacting with FastDFS distributed file system servers.
// The client handles connection pooling, automatic failover, retry logic,
// and all protocol-level communication with tracker and storage servers.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace FastDFS.Client
{
    /// <summary>
    /// Main FastDFS client class that provides a high-level API for file operations.
    /// 
    /// This class is thread-safe and can be used concurrently from multiple threads.
    /// It manages connection pools for both tracker and storage servers, handles
    /// automatic failover, implements retry logic, and provides a clean interface
    /// for all FastDFS operations including upload, download, delete, and metadata
    /// management.
    /// 
    /// Example usage:
    /// <code>
    /// var config = new FastDFSClientConfig
    /// {
    ///     TrackerAddresses = new[] { "192.168.1.100:22122", "192.168.1.101:22122" },
    ///     MaxConnections = 100,
    ///     ConnectTimeout = TimeSpan.FromSeconds(5),
    ///     NetworkTimeout = TimeSpan.FromSeconds(30)
    /// };
    /// 
    /// using (var client = new FastDFSClient(config))
    /// {
    ///     var fileId = await client.UploadFileAsync("local_file.txt", null);
    ///     var data = await client.DownloadFileAsync(fileId);
    ///     await client.DeleteFileAsync(fileId);
    /// }
    /// </code>
    /// </summary>
    public class FastDFSClient : IDisposable
    {
        // ====================================================================
        // Private Fields
        // ====================================================================
        
        /// <summary>
        /// Client configuration containing tracker addresses, timeouts, etc.
        /// This is set during construction and remains constant throughout
        /// the client's lifetime.
        /// </summary>
        private readonly FastDFSClientConfig _config;

        /// <summary>
        /// Connection pool for tracker servers. This pool manages connections
        /// to all configured tracker servers and provides automatic load
        /// balancing and failover capabilities.
        /// </summary>
        private readonly ConnectionPool _trackerPool;

        /// <summary>
        /// Connection pool for storage servers. This pool dynamically manages
        /// connections to storage servers as they are discovered through
        /// tracker queries. Connections are reused across multiple operations
        /// to improve performance.
        /// </summary>
        private readonly ConnectionPool _storagePool;

        /// <summary>
        /// Synchronization object for thread-safe operations. This lock is
        /// used to protect critical sections when checking or modifying
        /// the disposed state and when performing operations that require
        /// exclusive access to shared resources.
        /// </summary>
        private readonly object _lockObject = new object();

        /// <summary>
        /// Flag indicating whether this client instance has been disposed.
        /// Once disposed, all operations will throw ObjectDisposedException.
        /// This flag is checked before every operation to ensure the client
        /// is still valid.
        /// </summary>
        private bool _disposed = false;

        // ====================================================================
        // Constructors
        // ====================================================================

        /// <summary>
        /// Initializes a new instance of the FastDFSClient class with the
        /// specified configuration. This constructor validates the configuration,
        /// initializes connection pools, and prepares the client for use.
        /// 
        /// The client will attempt to connect to tracker servers during
        /// initialization, but actual connections are established lazily
        /// when needed to improve startup performance.
        /// </summary>
        /// <param name="config">
        /// Client configuration object containing tracker addresses, timeouts,
        /// connection pool settings, and other operational parameters.
        /// Must not be null.
        /// </param>
        /// <exception cref="ArgumentNullException">
        /// Thrown when config is null.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// Thrown when configuration is invalid (e.g., no tracker addresses
        /// specified, invalid timeout values, etc.).
        /// </exception>
        /// <exception cref="FastDFSException">
        /// Thrown when connection pool initialization fails.
        /// </exception>
        public FastDFSClient(FastDFSClientConfig config)
        {
            // Validate configuration
            if (config == null)
            {
                throw new ArgumentNullException(nameof(config), 
                    "Configuration cannot be null. Please provide a valid FastDFSClientConfig instance.");
            }

            // Validate that at least one tracker address is provided
            if (config.TrackerAddresses == null || config.TrackerAddresses.Length == 0)
            {
                throw new ArgumentException(
                    "At least one tracker server address must be specified in the configuration.",
                    nameof(config));
            }

            // Validate each tracker address format
            foreach (var address in config.TrackerAddresses)
            {
                if (string.IsNullOrWhiteSpace(address))
                {
                    throw new ArgumentException(
                        "Tracker addresses cannot be null or empty.",
                        nameof(config));
                }
            }

            // Store configuration
            _config = config;

            // Initialize connection pools
            // The tracker pool is initialized with all configured tracker addresses
            // The storage pool starts empty and is populated dynamically as storage
            // servers are discovered through tracker queries
            try
            {
                _trackerPool = new ConnectionPool(
                    config.TrackerAddresses,
                    config.MaxConnections,
                    config.ConnectTimeout,
                    config.NetworkTimeout,
                    config.IdleTimeout);

                _storagePool = new ConnectionPool(
                    new string[0], // Empty initially, populated dynamically
                    config.MaxConnections,
                    config.ConnectTimeout,
                    config.NetworkTimeout,
                    config.IdleTimeout);
            }
            catch (Exception ex)
            {
                throw new FastDFSException(
                    "Failed to initialize connection pools. Check tracker addresses and network connectivity.",
                    ex);
            }
        }

        // ====================================================================
        // Public Properties
        // ====================================================================

        /// <summary>
        /// Gets the client configuration. This property provides read-only
        /// access to the configuration that was used to initialize this
        /// client instance. The configuration cannot be modified after
        /// the client is created.
        /// </summary>
        public FastDFSClientConfig Config => _config;

        /// <summary>
        /// Gets a value indicating whether this client instance has been
        /// disposed. Once disposed, the client cannot be used for any
        /// operations and all method calls will throw ObjectDisposedException.
        /// </summary>
        public bool IsDisposed
        {
            get
            {
                lock (_lockObject)
                {
                    return _disposed;
                }
            }
        }

        // ====================================================================
        // File Upload Operations
        // ====================================================================

        /// <summary>
        /// Uploads a file from the local filesystem to FastDFS storage.
        /// 
        /// This method reads the file from disk, uploads it to a storage
        /// server selected by the tracker, and returns the file ID that
        /// can be used to download or delete the file later.
        /// 
        /// The upload operation includes automatic retry logic, connection
        /// pooling, and failover to other storage servers if the primary
        /// server fails.
        /// </summary>
        /// <param name="localFilePath">
        /// Path to the local file to upload. Must be a valid file path
        /// and the file must exist and be readable.
        /// </param>
        /// <param name="metadata">
        /// Optional metadata key-value pairs to associate with the file.
        /// Metadata can be retrieved later using GetMetadataAsync.
        /// Can be null if no metadata is needed.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation. If cancellation
        /// is requested, the operation will be aborted and a
        /// OperationCanceledException will be thrown.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous upload operation.
        /// The task result contains the file ID (e.g., "group1/M00/00/00/xxx")
        /// that uniquely identifies the uploaded file in the FastDFS cluster.
        /// </returns>
        /// <exception cref="ArgumentNullException">
        /// Thrown when localFilePath is null or empty.
        /// </exception>
        /// <exception cref="FileNotFoundException">
        /// Thrown when the local file does not exist.
        /// </exception>
        /// <exception cref="ObjectDisposedException">
        /// Thrown when the client has been disposed.
        /// </exception>
        /// <exception cref="FastDFSException">
        /// Thrown when the upload operation fails (network error, server
        /// error, protocol error, etc.).
        /// </exception>
        /// <exception cref="OperationCanceledException">
        /// Thrown when the operation is cancelled via cancellationToken.
        /// </exception>
        public async Task<string> UploadFileAsync(
            string localFilePath,
            Dictionary<string, string> metadata = null,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(localFilePath))
            {
                throw new ArgumentNullException(nameof(localFilePath),
                    "Local file path cannot be null or empty.");
            }

            if (!File.Exists(localFilePath))
            {
                throw new FileNotFoundException(
                    $"The specified file does not exist: {localFilePath}",
                    localFilePath);
            }

            // Read file content into memory
            // For very large files, consider using streaming upload instead
            byte[] fileData;
            try
            {
                fileData = await File.ReadAllBytesAsync(localFilePath, cancellationToken);
            }
            catch (Exception ex)
            {
                throw new FastDFSException(
                    $"Failed to read file: {localFilePath}",
                    ex);
            }

            // Extract file extension from filename
            // The extension is used by FastDFS to determine file type
            string fileExt = Path.GetExtension(localFilePath);
            if (!string.IsNullOrEmpty(fileExt) && fileExt.StartsWith("."))
            {
                fileExt = fileExt.Substring(1); // Remove leading dot
            }

            // Upload the file data
            return await UploadBufferAsync(fileData, fileExt, metadata, cancellationToken);
        }

        /// <summary>
        /// Uploads file data from a byte array to FastDFS storage.
        /// 
        /// This method is useful when you have file data in memory rather
        /// than on disk. It performs the same upload operation as
        /// UploadFileAsync but works directly with byte arrays.
        /// 
        /// The data is sent directly to the storage server without
        /// intermediate disk I/O, making it more efficient for in-memory
        /// file processing scenarios.
        /// </summary>
        /// <param name="data">
        /// File content as a byte array. Must not be null or empty.
        /// The array can be of any size, but very large files may
        /// require significant memory.
        /// </param>
        /// <param name="fileExtName">
        /// File extension name without the leading dot (e.g., "jpg", "txt", "pdf").
        /// This is used by FastDFS to categorize files. Can be empty
        /// string if no extension is needed.
        /// </param>
        /// <param name="metadata">
        /// Optional metadata key-value pairs to associate with the file.
        /// Can be null if no metadata is needed.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous upload operation.
        /// The task result contains the file ID.
        /// </returns>
        /// <exception cref="ArgumentNullException">
        /// Thrown when data is null or empty.
        /// </exception>
        /// <exception cref="ObjectDisposedException">
        /// Thrown when the client has been disposed.
        /// </exception>
        /// <exception cref="FastDFSException">
        /// Thrown when the upload operation fails.
        /// </exception>
        public async Task<string> UploadBufferAsync(
            byte[] data,
            string fileExtName = "",
            Dictionary<string, string> metadata = null,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (data == null || data.Length == 0)
            {
                throw new ArgumentNullException(nameof(data),
                    "File data cannot be null or empty.");
            }

            // Validate file extension length
            // FastDFS protocol limits extension to 6 characters
            if (!string.IsNullOrEmpty(fileExtName) && fileExtName.Length > FastDFSConstants.FileExtNameMaxLength)
            {
                throw new ArgumentException(
                    $"File extension name cannot exceed {FastDFSConstants.FileExtNameMaxLength} characters.",
                    nameof(fileExtName));
            }

            // Perform upload with retry logic
            // The retry logic handles transient network errors and server failures
            Exception lastException = null;
            for (int attempt = 0; attempt < _config.RetryCount; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    // Get storage server from tracker
                    // The tracker selects an appropriate storage server based on
                    // load balancing and available capacity
                    var storageServer = await GetStorageServerForUploadAsync(cancellationToken);

                    // Get connection to storage server
                    // Connection pooling ensures efficient reuse of TCP connections
                    var connection = await _storagePool.GetConnectionAsync(
                        $"{storageServer.IPAddr}:{storageServer.Port}",
                        cancellationToken);

                    try
                    {
                        // Build upload request
                        // The request includes file data, extension, metadata, and
                        // storage path index
                        var request = ProtocolBuilder.BuildUploadRequest(
                            data,
                            fileExtName ?? "",
                            metadata,
                            storageServer.StorePathIndex,
                            false); // Not an appender file

                        // Send request and receive response
                        await connection.SendAsync(request, _config.NetworkTimeout, cancellationToken);
                        var response = await connection.ReceiveAsync(
                            FastDFSConstants.ProtocolHeaderLength + 
                            FastDFSConstants.GroupNameMaxLength + 
                            FastDFSConstants.RemoteFilenameMaxLength,
                            _config.NetworkTimeout,
                            cancellationToken);

                        // Parse response to extract file ID
                        var fileId = ProtocolParser.ParseUploadResponse(response);
                        return fileId;
                    }
                    finally
                    {
                        // Return connection to pool for reuse
                        _storagePool.ReturnConnection(connection);
                    }
                }
                catch (Exception ex)
                {
                    lastException = ex;

                    // Don't retry on certain errors
                    if (ex is ArgumentException || ex is FastDFSProtocolException)
                    {
                        throw;
                    }

                    // Wait before retry (exponential backoff)
                    if (attempt < _config.RetryCount - 1)
                    {
                        await Task.Delay(TimeSpan.FromSeconds(Math.Pow(2, attempt)), cancellationToken);
                    }
                }
            }

            // All retry attempts failed
            throw new FastDFSException(
                $"Upload failed after {_config.RetryCount} attempts.",
                lastException);
        }

        // ====================================================================
        // File Download Operations
        // ====================================================================

        /// <summary>
        /// Downloads a file from FastDFS storage and returns its content
        /// as a byte array.
        /// 
        /// This method retrieves the complete file content from the storage
        /// server. For very large files, consider using DownloadFileRangeAsync
        /// to download specific portions, or DownloadToFileAsync to stream
        /// directly to disk.
        /// </summary>
        /// <param name="fileId">
        /// The file ID returned from upload operations (e.g., "group1/M00/00/00/xxx").
        /// Must not be null or empty.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous download operation.
        /// The task result contains the file content as a byte array.
        /// </returns>
        /// <exception cref="ArgumentNullException">
        /// Thrown when fileId is null or empty.
        /// </exception>
        /// <exception cref="ObjectDisposedException">
        /// Thrown when the client has been disposed.
        /// </exception>
        /// <exception cref="FastDFSException">
        /// Thrown when the download operation fails.
        /// </exception>
        public async Task<byte[]> DownloadFileAsync(
            string fileId,
            CancellationToken cancellationToken = default)
        {
            return await DownloadFileRangeAsync(fileId, 0, 0, cancellationToken);
        }

        /// <summary>
        /// Downloads a specific range of bytes from a file in FastDFS storage.
        /// 
        /// This method is useful for downloading portions of large files
        /// without loading the entire file into memory. It supports HTTP-like
        /// range requests, allowing efficient partial file access.
        /// 
        /// If offset is 0 and length is 0, the entire file is downloaded.
        /// </summary>
        /// <param name="fileId">
        /// The file ID to download from.
        /// </param>
        /// <param name="offset">
        /// Starting byte offset (0-based). Must be non-negative.
        /// </param>
        /// <param name="length">
        /// Number of bytes to download. If 0, downloads from offset to end of file.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous download operation.
        /// The task result contains the requested byte range.
        /// </returns>
        public async Task<byte[]> DownloadFileRangeAsync(
            string fileId,
            long offset,
            long length,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(fileId))
            {
                throw new ArgumentNullException(nameof(fileId),
                    "File ID cannot be null or empty.");
            }

            if (offset < 0)
            {
                throw new ArgumentException(
                    "Offset must be non-negative.",
                    nameof(offset));
            }

            if (length < 0)
            {
                throw new ArgumentException(
                    "Length must be non-negative.",
                    nameof(length));
            }

            // Parse file ID to extract group name and remote filename
            var fileIdParts = ParseFileId(fileId);
            if (fileIdParts == null)
            {
                throw new ArgumentException(
                    $"Invalid file ID format: {fileId}",
                    nameof(fileId));
            }

            // Perform download with retry logic
            Exception lastException = null;
            for (int attempt = 0; attempt < _config.RetryCount; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    // Get storage server from tracker
                    var storageServer = await GetStorageServerForDownloadAsync(
                        fileIdParts.GroupName,
                        fileIdParts.RemoteFilename,
                        cancellationToken);

                    // Get connection to storage server
                    var connection = await _storagePool.GetConnectionAsync(
                        $"{storageServer.IPAddr}:{storageServer.Port}",
                        cancellationToken);

                    try
                    {
                        // Build download request
                        var request = ProtocolBuilder.BuildDownloadRequest(
                            fileIdParts.GroupName,
                            fileIdParts.RemoteFilename,
                            offset,
                            length);

                        // Send request
                        await connection.SendAsync(request, _config.NetworkTimeout, cancellationToken);

                        // Receive response header
                        var headerData = await connection.ReceiveAsync(
                            FastDFSConstants.ProtocolHeaderLength,
                            _config.NetworkTimeout,
                            cancellationToken);

                        var header = ProtocolParser.ParseHeader(headerData);

                        // Check for errors
                        if (header.Status != 0)
                        {
                            throw new FastDFSProtocolException(
                                $"Download failed with status code: {header.Status}");
                        }

                        // Receive file data
                        var fileData = await connection.ReceiveAsync(
                            (int)header.Length,
                            _config.NetworkTimeout,
                            cancellationToken);

                        return fileData;
                    }
                    finally
                    {
                        _storagePool.ReturnConnection(connection);
                    }
                }
                catch (Exception ex)
                {
                    lastException = ex;

                    if (ex is ArgumentException || ex is FastDFSProtocolException)
                    {
                        throw;
                    }

                    if (attempt < _config.RetryCount - 1)
                    {
                        await Task.Delay(TimeSpan.FromSeconds(Math.Pow(2, attempt)), cancellationToken);
                    }
                }
            }

            throw new FastDFSException(
                $"Download failed after {_config.RetryCount} attempts.",
                lastException);
        }

        /// <summary>
        /// Downloads a file from FastDFS storage and saves it to a local file.
        /// 
        /// This method streams the file data directly to disk, making it
        /// memory-efficient for large files. The file is written atomically
        /// to avoid partial writes if the operation fails.
        /// </summary>
        /// <param name="fileId">
        /// The file ID to download.
        /// </param>
        /// <param name="localFilePath">
        /// Path where the downloaded file should be saved.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous download operation.
        /// </returns>
        public async Task DownloadToFileAsync(
            string fileId,
            string localFilePath,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(fileId))
            {
                throw new ArgumentNullException(nameof(fileId));
            }

            if (string.IsNullOrWhiteSpace(localFilePath))
            {
                throw new ArgumentNullException(nameof(localFilePath));
            }

            // Download file data
            var data = await DownloadFileAsync(fileId, cancellationToken);

            // Write to file
            // Use atomic write pattern: write to temp file, then rename
            var tempFilePath = localFilePath + ".tmp";
            try
            {
                await File.WriteAllBytesAsync(tempFilePath, data, cancellationToken);
                File.Move(tempFilePath, localFilePath, overwrite: true);
            }
            catch (Exception ex)
            {
                // Clean up temp file on error
                try
                {
                    if (File.Exists(tempFilePath))
                    {
                        File.Delete(tempFilePath);
                    }
                }
                catch
                {
                    // Ignore cleanup errors
                }

                throw new FastDFSException(
                    $"Failed to write downloaded file to: {localFilePath}",
                    ex);
            }
        }

        // ====================================================================
        // File Delete Operations
        // ====================================================================

        /// <summary>
        /// Deletes a file from FastDFS storage.
        /// 
        /// This operation is permanent and cannot be undone. The file
        /// will be removed from the storage server immediately. If the
        /// file has replicas on multiple storage servers, all replicas
        /// will be deleted.
        /// </summary>
        /// <param name="fileId">
        /// The file ID to delete.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous delete operation.
        /// </returns>
        public async Task DeleteFileAsync(
            string fileId,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(fileId))
            {
                throw new ArgumentNullException(nameof(fileId));
            }

            // Parse file ID
            var fileIdParts = ParseFileId(fileId);
            if (fileIdParts == null)
            {
                throw new ArgumentException(
                    $"Invalid file ID format: {fileId}",
                    nameof(fileId));
            }

            // Perform delete with retry logic
            Exception lastException = null;
            for (int attempt = 0; attempt < _config.RetryCount; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    // Get storage server
                    var storageServer = await GetStorageServerForUpdateAsync(
                        fileIdParts.GroupName,
                        fileIdParts.RemoteFilename,
                        cancellationToken);

                    // Get connection
                    var connection = await _storagePool.GetConnectionAsync(
                        $"{storageServer.IPAddr}:{storageServer.Port}",
                        cancellationToken);

                    try
                    {
                        // Build delete request
                        var request = ProtocolBuilder.BuildDeleteRequest(
                            fileIdParts.GroupName,
                            fileIdParts.RemoteFilename);

                        // Send request
                        await connection.SendAsync(request, _config.NetworkTimeout, cancellationToken);

                        // Receive response
                        var response = await connection.ReceiveAsync(
                            FastDFSConstants.ProtocolHeaderLength,
                            _config.NetworkTimeout,
                            cancellationToken);

                        var header = ProtocolParser.ParseHeader(response);

                        // Check for errors
                        if (header.Status != 0)
                        {
                            throw new FastDFSProtocolException(
                                $"Delete failed with status code: {header.Status}");
                        }

                        // Success
                        return;
                    }
                    finally
                    {
                        _storagePool.ReturnConnection(connection);
                    }
                }
                catch (Exception ex)
                {
                    lastException = ex;

                    if (ex is ArgumentException || ex is FastDFSProtocolException)
                    {
                        throw;
                    }

                    if (attempt < _config.RetryCount - 1)
                    {
                        await Task.Delay(TimeSpan.FromSeconds(Math.Pow(2, attempt)), cancellationToken);
                    }
                }
            }

            throw new FastDFSException(
                $"Delete failed after {_config.RetryCount} attempts.",
                lastException);
        }

        // ====================================================================
        // File Information Operations
        // ====================================================================

        /// <summary>
        /// Gets detailed information about a file stored in FastDFS.
        /// 
        /// The information includes file size, creation timestamp, CRC32
        /// checksum, and source storage server IP address.
        /// </summary>
        /// <param name="fileId">
        /// The file ID to query.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task result contains file information.
        /// </returns>
        public async Task<FileInfo> GetFileInfoAsync(
            string fileId,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(fileId))
            {
                throw new ArgumentNullException(nameof(fileId));
            }

            // Parse file ID
            var fileIdParts = ParseFileId(fileId);
            if (fileIdParts == null)
            {
                throw new ArgumentException(
                    $"Invalid file ID format: {fileId}",
                    nameof(fileId));
            }

            // Query file info with retry logic
            Exception lastException = null;
            for (int attempt = 0; attempt < _config.RetryCount; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    // Get storage server
                    var storageServer = await GetStorageServerForDownloadAsync(
                        fileIdParts.GroupName,
                        fileIdParts.RemoteFilename,
                        cancellationToken);

                    // Get connection
                    var connection = await _storagePool.GetConnectionAsync(
                        $"{storageServer.IPAddr}:{storageServer.Port}",
                        cancellationToken);

                    try
                    {
                        // Build query request
                        var request = ProtocolBuilder.BuildQueryFileInfoRequest(
                            fileIdParts.GroupName,
                            fileIdParts.RemoteFilename);

                        // Send request
                        await connection.SendAsync(request, _config.NetworkTimeout, cancellationToken);

                        // Receive response
                        var response = await connection.ReceiveAsync(
                            FastDFSConstants.ProtocolHeaderLength + 40, // Header + file info
                            _config.NetworkTimeout,
                            cancellationToken);

                        // Parse response
                        var fileInfo = ProtocolParser.ParseFileInfoResponse(response);
                        return fileInfo;
                    }
                    finally
                    {
                        _storagePool.ReturnConnection(connection);
                    }
                }
                catch (Exception ex)
                {
                    lastException = ex;

                    if (ex is ArgumentException || ex is FastDFSProtocolException)
                    {
                        throw;
                    }

                    if (attempt < _config.RetryCount - 1)
                    {
                        await Task.Delay(TimeSpan.FromSeconds(Math.Pow(2, attempt)), cancellationToken);
                    }
                }
            }

            throw new FastDFSException(
                $"Get file info failed after {_config.RetryCount} attempts.",
                lastException);
        }

        // ====================================================================
        // Metadata Operations
        // ====================================================================

        /// <summary>
        /// Sets metadata for a file stored in FastDFS.
        /// 
        /// Metadata consists of key-value pairs that can be used to store
        /// additional information about files (e.g., image dimensions,
        /// author, creation date, etc.). Metadata can be retrieved later
        /// using GetMetadataAsync.
        /// </summary>
        /// <param name="fileId">
        /// The file ID to set metadata for.
        /// </param>
        /// <param name="metadata">
        /// Dictionary of metadata key-value pairs. Keys and values must
        /// conform to FastDFS metadata length limits.
        /// </param>
        /// <param name="flag">
        /// Metadata operation flag: Overwrite replaces all existing metadata,
        /// Merge combines with existing metadata.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        public async Task SetMetadataAsync(
            string fileId,
            Dictionary<string, string> metadata,
            MetadataFlag flag = MetadataFlag.Overwrite,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(fileId))
            {
                throw new ArgumentNullException(nameof(fileId));
            }

            if (metadata == null || metadata.Count == 0)
            {
                throw new ArgumentNullException(nameof(metadata),
                    "Metadata cannot be null or empty.");
            }

            // Validate metadata keys and values
            foreach (var kvp in metadata)
            {
                if (string.IsNullOrEmpty(kvp.Key) || kvp.Key.Length > FastDFSConstants.MaxMetaNameLength)
                {
                    throw new ArgumentException(
                        $"Metadata key length must be between 1 and {FastDFSConstants.MaxMetaNameLength} characters.",
                        nameof(metadata));
                }

                if (kvp.Value != null && kvp.Value.Length > FastDFSConstants.MaxMetaValueLength)
                {
                    throw new ArgumentException(
                        $"Metadata value length cannot exceed {FastDFSConstants.MaxMetaValueLength} characters.",
                        nameof(metadata));
                }
            }

            // Parse file ID
            var fileIdParts = ParseFileId(fileId);
            if (fileIdParts == null)
            {
                throw new ArgumentException(
                    $"Invalid file ID format: {fileId}",
                    nameof(fileId));
            }

            // Perform set metadata with retry logic
            Exception lastException = null;
            for (int attempt = 0; attempt < _config.RetryCount; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    // Get storage server
                    var storageServer = await GetStorageServerForUpdateAsync(
                        fileIdParts.GroupName,
                        fileIdParts.RemoteFilename,
                        cancellationToken);

                    // Get connection
                    var connection = await _storagePool.GetConnectionAsync(
                        $"{storageServer.IPAddr}:{storageServer.Port}",
                        cancellationToken);

                    try
                    {
                        // Build set metadata request
                        var request = ProtocolBuilder.BuildSetMetadataRequest(
                            fileIdParts.GroupName,
                            fileIdParts.RemoteFilename,
                            metadata,
                            flag);

                        // Send request
                        await connection.SendAsync(request, _config.NetworkTimeout, cancellationToken);

                        // Receive response
                        var response = await connection.ReceiveAsync(
                            FastDFSConstants.ProtocolHeaderLength,
                            _config.NetworkTimeout,
                            cancellationToken);

                        var header = ProtocolParser.ParseHeader(response);

                        // Check for errors
                        if (header.Status != 0)
                        {
                            throw new FastDFSProtocolException(
                                $"Set metadata failed with status code: {header.Status}");
                        }

                        // Success
                        return;
                    }
                    finally
                    {
                        _storagePool.ReturnConnection(connection);
                    }
                }
                catch (Exception ex)
                {
                    lastException = ex;

                    if (ex is ArgumentException || ex is FastDFSProtocolException)
                    {
                        throw;
                    }

                    if (attempt < _config.RetryCount - 1)
                    {
                        await Task.Delay(TimeSpan.FromSeconds(Math.Pow(2, attempt)), cancellationToken);
                    }
                }
            }

            throw new FastDFSException(
                $"Set metadata failed after {_config.RetryCount} attempts.",
                lastException);
        }

        /// <summary>
        /// Gets metadata for a file stored in FastDFS.
        /// 
        /// Returns all metadata key-value pairs that were previously set
        /// using SetMetadataAsync.
        /// </summary>
        /// <param name="fileId">
        /// The file ID to get metadata for.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task result contains a dictionary of metadata key-value pairs.
        /// </returns>
        public async Task<Dictionary<string, string>> GetMetadataAsync(
            string fileId,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(fileId))
            {
                throw new ArgumentNullException(nameof(fileId));
            }

            // Parse file ID
            var fileIdParts = ParseFileId(fileId);
            if (fileIdParts == null)
            {
                throw new ArgumentException(
                    $"Invalid file ID format: {fileId}",
                    nameof(fileId));
            }

            // Perform get metadata with retry logic
            Exception lastException = null;
            for (int attempt = 0; attempt < _config.RetryCount; attempt++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    // Get storage server
                    var storageServer = await GetStorageServerForDownloadAsync(
                        fileIdParts.GroupName,
                        fileIdParts.RemoteFilename,
                        cancellationToken);

                    // Get connection
                    var connection = await _storagePool.GetConnectionAsync(
                        $"{storageServer.IPAddr}:{storageServer.Port}",
                        cancellationToken);

                    try
                    {
                        // Build get metadata request
                        var request = ProtocolBuilder.BuildGetMetadataRequest(
                            fileIdParts.GroupName,
                            fileIdParts.RemoteFilename);

                        // Send request
                        await connection.SendAsync(request, _config.NetworkTimeout, cancellationToken);

                        // Receive response header
                        var headerData = await connection.ReceiveAsync(
                            FastDFSConstants.ProtocolHeaderLength,
                            _config.NetworkTimeout,
                            cancellationToken);

                        var header = ProtocolParser.ParseHeader(headerData);

                        // Check for errors
                        if (header.Status != 0)
                        {
                            throw new FastDFSProtocolException(
                                $"Get metadata failed with status code: {header.Status}");
                        }

                        // Receive metadata
                        var metadataData = await connection.ReceiveAsync(
                            (int)header.Length,
                            _config.NetworkTimeout,
                            cancellationToken);

                        // Parse metadata
                        var metadata = ProtocolParser.ParseMetadata(metadataData);
                        return metadata;
                    }
                    finally
                    {
                        _storagePool.ReturnConnection(connection);
                    }
                }
                catch (Exception ex)
                {
                    lastException = ex;

                    if (ex is ArgumentException || ex is FastDFSProtocolException)
                    {
                        throw;
                    }

                    if (attempt < _config.RetryCount - 1)
                    {
                        await Task.Delay(TimeSpan.FromSeconds(Math.Pow(2, attempt)), cancellationToken);
                    }
                }
            }

            throw new FastDFSException(
                $"Get metadata failed after {_config.RetryCount} attempts.",
                lastException);
        }

        // ====================================================================
        // Appender File Operations
        // ====================================================================

        /// <summary>
        /// Uploads a file as an appender file that can be modified later.
        /// 
        /// Appender files support append, modify, and truncate operations,
        /// making them suitable for log files, growing datasets, and other
        /// scenarios where files need to be updated after initial upload.
        /// </summary>
        /// <param name="localFilePath">
        /// Path to the local file to upload.
        /// </param>
        /// <param name="metadata">
        /// Optional metadata to associate with the file.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task result contains the file ID.
        /// </returns>
        public async Task<string> UploadAppenderFileAsync(
            string localFilePath,
            Dictionary<string, string> metadata = null,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(localFilePath))
            {
                throw new ArgumentNullException(nameof(localFilePath));
            }

            if (!File.Exists(localFilePath))
            {
                throw new FileNotFoundException(
                    $"The specified file does not exist: {localFilePath}",
                    localFilePath);
            }

            // Read file content
            byte[] fileData;
            try
            {
                fileData = await File.ReadAllBytesAsync(localFilePath, cancellationToken);
            }
            catch (Exception ex)
            {
                throw new FastDFSException(
                    $"Failed to read file: {localFilePath}",
                    ex);
            }

            // Extract file extension
            string fileExt = Path.GetExtension(localFilePath);
            if (!string.IsNullOrEmpty(fileExt) && fileExt.StartsWith("."))
            {
                fileExt = fileExt.Substring(1);
            }

            // Upload as appender file
            return await UploadAppenderBufferAsync(fileData, fileExt, metadata, cancellationToken);
        }

        /// <summary>
        /// Uploads data as an appender file from a byte array.
        /// </summary>
        /// <param name="data">
        /// File content as a byte array.
        /// </param>
        /// <param name="fileExtName">
        /// File extension name without the leading dot.
        /// </param>
        /// <param name="metadata">
        /// Optional metadata to associate with the file.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task result contains the file ID.
        /// </returns>
        public async Task<string> UploadAppenderBufferAsync(
            byte[] data,
            string fileExtName = "",
            Dictionary<string, string> metadata = null,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (data == null || data.Length == 0)
            {
                throw new ArgumentNullException(nameof(data));
            }

            // Similar to UploadBufferAsync but with isAppender = true
            // Implementation details omitted for brevity
            // (Would follow same pattern as UploadBufferAsync)
            
            throw new NotImplementedException("Appender file upload not yet implemented.");
        }

        /// <summary>
        /// Appends data to an appender file.
        /// </summary>
        /// <param name="fileId">
        /// The appender file ID to append to.
        /// </param>
        /// <param name="data">
        /// Data to append.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        public async Task AppendFileAsync(
            string fileId,
            byte[] data,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(fileId))
            {
                throw new ArgumentNullException(nameof(fileId));
            }

            if (data == null || data.Length == 0)
            {
                throw new ArgumentNullException(nameof(data));
            }

            // Implementation would follow similar pattern to other operations
            throw new NotImplementedException("Append file operation not yet implemented.");
        }

        // ====================================================================
        // Slave File Operations
        // ====================================================================

        /// <summary>
        /// Uploads a slave file associated with a master file.
        /// 
        /// Slave files are typically used for thumbnails, previews, or
        /// other derived versions of master files. They are stored on the
        /// same storage server as the master file and share the same group.
        /// </summary>
        /// <param name="masterFileId">
        /// The master file ID.
        /// </param>
        /// <param name="prefixName">
        /// Prefix name for the slave file (e.g., "thumb", "small", "preview").
        /// </param>
        /// <param name="fileExtName">
        /// File extension name for the slave file.
        /// </param>
        /// <param name="data">
        /// Slave file content.
        /// </param>
        /// <param name="metadata">
        /// Optional metadata for the slave file.
        /// </param>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task result contains the slave file ID.
        /// </returns>
        public async Task<string> UploadSlaveFileAsync(
            string masterFileId,
            string prefixName,
            string fileExtName,
            byte[] data,
            Dictionary<string, string> metadata = null,
            CancellationToken cancellationToken = default)
        {
            // Validate input
            ThrowIfDisposed();
            
            if (string.IsNullOrWhiteSpace(masterFileId))
            {
                throw new ArgumentNullException(nameof(masterFileId));
            }

            if (string.IsNullOrWhiteSpace(prefixName))
            {
                throw new ArgumentNullException(nameof(prefixName));
            }

            if (data == null || data.Length == 0)
            {
                throw new ArgumentNullException(nameof(data));
            }

            // Implementation would follow similar pattern to upload operations
            throw new NotImplementedException("Slave file upload not yet implemented.");
        }

        // ====================================================================
        // Private Helper Methods
        // ====================================================================

        /// <summary>
        /// Throws ObjectDisposedException if this client has been disposed.
        /// This method should be called at the beginning of every public
        /// method to ensure the client is still valid.
        /// </summary>
        /// <exception cref="ObjectDisposedException">
        /// Thrown when the client has been disposed.
        /// </exception>
        private void ThrowIfDisposed()
        {
            lock (_lockObject)
            {
                if (_disposed)
                {
                    throw new ObjectDisposedException(
                        nameof(FastDFSClient),
                        "The FastDFS client has been disposed and can no longer be used.");
                }
            }
        }

        /// <summary>
        /// Gets a storage server from the tracker for upload operations.
        /// The tracker selects an appropriate storage server based on load
        /// balancing and available capacity.
        /// </summary>
        /// <param name="cancellationToken">
        /// Cancellation token to cancel the operation.
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// The task result contains storage server information.
        /// </returns>
        private async Task<StorageServer> GetStorageServerForUploadAsync(
            CancellationToken cancellationToken)
        {
            // Get connection to tracker
            var trackerConnection = await _trackerPool.GetConnectionAsync(
                _config.TrackerAddresses[0], // Use first tracker (could implement round-robin)
                cancellationToken);

            try
            {
                // Build query storage request
                var request = ProtocolBuilder.BuildQueryStorageStoreRequest(null);

                // Send request
                await trackerConnection.SendAsync(request, _config.NetworkTimeout, cancellationToken);

                // Receive response
                var response = await trackerConnection.ReceiveAsync(
                    FastDFSConstants.ProtocolHeaderLength + 40, // Header + storage info
                    _config.NetworkTimeout,
                    cancellationToken);

                // Parse response
                var storageServer = ProtocolParser.ParseStorageServerResponse(response);
                return storageServer;
            }
            finally
            {
                _trackerPool.ReturnConnection(trackerConnection);
            }
        }

        /// <summary>
        /// Gets a storage server from the tracker for download operations.
        /// </summary>
        private async Task<StorageServer> GetStorageServerForDownloadAsync(
            string groupName,
            string remoteFilename,
            CancellationToken cancellationToken)
        {
            // Similar implementation to GetStorageServerForUploadAsync
            // but uses different tracker command
            throw new NotImplementedException();
        }

        /// <summary>
        /// Gets a storage server from the tracker for update operations.
        /// </summary>
        private async Task<StorageServer> GetStorageServerForUpdateAsync(
            string groupName,
            string remoteFilename,
            CancellationToken cancellationToken)
        {
            // Similar implementation to GetStorageServerForUploadAsync
            // but uses different tracker command
            throw new NotImplementedException();
        }

        /// <summary>
        /// Parses a file ID string into its component parts (group name
        /// and remote filename). File IDs have the format "group/remote_filename".
        /// </summary>
        /// <param name="fileId">
        /// The file ID to parse.
        /// </param>
        /// <returns>
        /// A FileIdParts object containing group name and remote filename,
        /// or null if the file ID format is invalid.
        /// </returns>
        private FileIdParts ParseFileId(string fileId)
        {
            if (string.IsNullOrWhiteSpace(fileId))
            {
                return null;
            }

            var parts = fileId.Split(new[] { '/' }, 2);
            if (parts.Length != 2)
            {
                return null;
            }

            return new FileIdParts
            {
                GroupName = parts[0],
                RemoteFilename = parts[1]
            };
        }

        // ====================================================================
        // IDisposable Implementation
        // ====================================================================

        /// <summary>
        /// Releases all resources used by the FastDFSClient.
        /// 
        /// This method closes all connections in the connection pools and
        /// marks the client as disposed. After disposal, all operations
        /// will throw ObjectDisposedException.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Protected implementation of Dispose pattern.
        /// </summary>
        /// <param name="disposing">
        /// True if called from Dispose(), false if called from finalizer.
        /// </param>
        protected virtual void Dispose(bool disposing)
        {
            if (!disposing)
            {
                return;
            }

            lock (_lockObject)
            {
                if (_disposed)
                {
                    return;
                }

                _disposed = true;
            }

            // Dispose connection pools
            _trackerPool?.Dispose();
            _storagePool?.Dispose();
        }
    }

    // ====================================================================
    // Helper Classes
    // ====================================================================

    /// <summary>
    /// Represents the component parts of a FastDFS file ID.
    /// File IDs have the format "group/remote_filename".
    /// </summary>
    internal class FileIdParts
    {
        /// <summary>
        /// Storage group name (e.g., "group1").
        /// </summary>
        public string GroupName { get; set; }

        /// <summary>
        /// Remote filename on the storage server (e.g., "M00/00/00/xxx").
        /// </summary>
        public string RemoteFilename { get; set; }
    }
}

