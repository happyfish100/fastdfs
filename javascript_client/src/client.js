/**
 * FastDFS JavaScript Client
 * 
 * Main client class for interacting with FastDFS distributed file system.
 * 
 * This client provides a high-level JavaScript API for FastDFS operations including
 * file upload, download, deletion, metadata management, and appender file operations.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * 
 * @example Basic usage
 * const { Client } = require('./client');
 * 
 * // Create client configuration
 * const config = {
 *   trackerAddrs: ['192.168.1.100:22122', '192.168.1.101:22122'],
 *   maxConns: 100,
 *   connectTimeout: 5000,
 *   networkTimeout: 30000
 * };
 * 
 * // Initialize client
 * const client = new Client(config);
 * 
 * // Upload a file
 * const fileId = await client.uploadFile('test.jpg');
 * 
 * // Download the file
 * const data = await client.downloadFile(fileId);
 * 
 * // Delete the file
 * await client.deleteFile(fileId);
 * 
 * // Close the client
 * await client.close();
 */

'use strict';

const fs = require('fs').promises;
const { ConnectionPool } = require('./connection');
const { Operations } = require('./operations');
const { ClientClosedError, InvalidArgumentError } = require('./errors');
const { MetadataFlag } = require('./types');

/**
 * FastDFS client for file operations
 * 
 * This class provides a high-level JavaScript API for interacting with FastDFS servers.
 * It handles connection pooling, automatic retries, and error handling.
 * 
 * The client is designed to be used with async/await and provides Promise-based APIs
 * for all operations. It's safe to use the client concurrently from multiple async
 * functions.
 */
class Client {
  /**
   * Creates a new FastDFS client with the given configuration
   * 
   * @param {Object} config - Client configuration
   * @param {string[]} config.trackerAddrs - List of tracker server addresses in format "host:port"
   * @param {number} [config.maxConns=10] - Maximum number of connections per tracker server
   * @param {number} [config.connectTimeout=5000] - Timeout for establishing connections in milliseconds
   * @param {number} [config.networkTimeout=30000] - Timeout for network I/O operations in milliseconds
   * @param {number} [config.idleTimeout=60000] - Timeout for idle connections in the pool in milliseconds
   * @param {number} [config.retryCount=3] - Number of retries for failed operations
   * 
   * @throws {InvalidArgumentError} If configuration is invalid
   * 
   * @example
   * const client = new Client({
   *   trackerAddrs: ['192.168.1.100:22122']
   * });
   */
  constructor(config) {
    // Validate configuration
    this._validateConfig(config);
    
    // Apply defaults
    this.config = {
      trackerAddrs: config.trackerAddrs,
      maxConns: config.maxConns || 10,
      connectTimeout: config.connectTimeout || 5000,
      networkTimeout: config.networkTimeout || 30000,
      idleTimeout: config.idleTimeout || 60000,
      retryCount: config.retryCount || 3,
    };
    
    // Track whether the client has been closed
    this.closed = false;
    
    // Initialize connection pool for tracker servers
    // Tracker servers are used to locate storage servers for operations
    this.trackerPool = new ConnectionPool(
      this.config.trackerAddrs,
      this.config.maxConns,
      this.config.connectTimeout,
      this.config.idleTimeout
    );
    
    // Initialize connection pool for storage servers
    // Storage servers are discovered dynamically through tracker queries
    this.storagePool = new ConnectionPool(
      [], // Storage servers are discovered dynamically
      this.config.maxConns,
      this.config.connectTimeout,
      this.config.idleTimeout
    );
    
    // Initialize operations handler
    // This object handles all file operations such as upload, download, delete
    this.operations = new Operations(
      this.trackerPool,
      this.storagePool,
      this.config.networkTimeout,
      this.config.retryCount
    );
  }
  
  /**
   * Validates the client configuration
   * 
   * @private
   * @param {Object} config - Configuration to validate
   * @throws {InvalidArgumentError} If configuration is invalid
   */
  _validateConfig(config) {
    if (!config) {
      throw new InvalidArgumentError('Config is required');
    }
    
    if (!config.trackerAddrs || !Array.isArray(config.trackerAddrs) || config.trackerAddrs.length === 0) {
      throw new InvalidArgumentError('Tracker addresses are required');
    }
    
    for (const addr of config.trackerAddrs) {
      if (!addr || typeof addr !== 'string' || !addr.includes(':')) {
        throw new InvalidArgumentError(`Invalid tracker address: ${addr}`);
      }
    }
  }
  
  /**
   * Checks if the client is closed and throws an error if so
   * 
   * @private
   * @throws {ClientClosedError} If client is closed
   */
  _checkClosed() {
    if (this.closed) {
      throw new ClientClosedError();
    }
  }
  
  /**
   * Uploads a file from the local filesystem to FastDFS
   * 
   * This method reads the file from the local filesystem, uploads it to a
   * storage server, and returns a file ID that can be used to reference the
   * file in subsequent operations.
   * 
   * @param {string} localFilename - Path to the local file to upload
   * @param {Object.<string, string>} [metadata] - Optional metadata key-value pairs
   * @returns {Promise<string>} The file ID in the format "group/remote_filename"
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {Error} If the local file does not exist or cannot be read
   * @throws {NetworkError} If network communication fails after retries
   * 
   * @example
   * const fileId = await client.uploadFile('test.jpg');
   * console.log('File uploaded:', fileId);
   * 
   * @example With metadata
   * const fileId = await client.uploadFile('document.pdf', {
   *   author: 'John Doe',
   *   date: '2025-01-01'
   * });
   */
  async uploadFile(localFilename, metadata = null) {
    this._checkClosed();
    
    // Read file content
    const fileData = await fs.readFile(localFilename);
    
    // Extract file extension
    const extMatch = localFilename.match(/\.([^.]+)$/);
    const fileExtName = extMatch ? extMatch[1] : '';
    
    // Delegate to operations handler
    return this.operations.uploadBuffer(fileData, fileExtName, metadata, false);
  }
  
  /**
   * Uploads data from a buffer to FastDFS
   * 
   * This method uploads raw binary data directly to FastDFS without requiring
   * a file on the local filesystem. This is useful for in-memory data such as
   * generated content or data received from network requests.
   * 
   * @param {Buffer} data - File content as a Buffer
   * @param {string} fileExtName - File extension without dot (e.g., "jpg", "txt")
   * @param {Object.<string, string>} [metadata] - Optional metadata key-value pairs
   * @returns {Promise<string>} The file ID for the uploaded file
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {InvalidArgumentError} If data or fileExtName is invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example
   * const data = Buffer.from('Hello, FastDFS!');
   * const fileId = await client.uploadBuffer(data, 'txt');
   * 
   * @example Upload image data
   * const imageData = await fs.readFile('image.jpg');
   * const fileId = await client.uploadBuffer(imageData, 'jpg', {
   *   width: '1920',
   *   height: '1080'
   * });
   */
  async uploadBuffer(data, fileExtName, metadata = null) {
    this._checkClosed();
    
    if (!Buffer.isBuffer(data)) {
      throw new InvalidArgumentError('data must be a Buffer');
    }
    if (!fileExtName || typeof fileExtName !== 'string') {
      throw new InvalidArgumentError('fileExtName is required');
    }
    
    return this.operations.uploadBuffer(data, fileExtName, metadata, false);
  }
  
  /**
   * Uploads an appender file from the local filesystem
   * 
   * Appender files can be modified after upload using append, modify, and
   * truncate operations. They are useful for log files or files that need
   * to grow over time.
   * 
   * @param {string} localFilename - Path to the local file to upload
   * @param {Object.<string, string>} [metadata] - Optional metadata key-value pairs
   * @returns {Promise<string>} The file ID for the appender file
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {Error} If the local file does not exist
   * @throws {NetworkError} If network communication fails
   * 
   * @example
   * const fileId = await client.uploadAppenderFile('log.txt');
   * await client.appendFile(fileId, Buffer.from('New log entry\n'));
   */
  async uploadAppenderFile(localFilename, metadata = null) {
    this._checkClosed();
    
    const fileData = await fs.readFile(localFilename);
    const extMatch = localFilename.match(/\.([^.]+)$/);
    const fileExtName = extMatch ? extMatch[1] : '';
    
    return this.operations.uploadBuffer(fileData, fileExtName, metadata, true);
  }
  
  /**
   * Uploads an appender file from a buffer
   * 
   * @param {Buffer} data - File content as a Buffer
   * @param {string} fileExtName - File extension without dot
   * @param {Object.<string, string>} [metadata] - Optional metadata
   * @returns {Promise<string>} The file ID for the appender file
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {InvalidArgumentError} If parameters are invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example
   * const data = Buffer.from('Initial log content\n');
   * const fileId = await client.uploadAppenderBuffer(data, 'log');
   */
  async uploadAppenderBuffer(data, fileExtName, metadata = null) {
    this._checkClosed();
    
    if (!Buffer.isBuffer(data)) {
      throw new InvalidArgumentError('data must be a Buffer');
    }
    if (!fileExtName || typeof fileExtName !== 'string') {
      throw new InvalidArgumentError('fileExtName is required');
    }
    
    return this.operations.uploadBuffer(data, fileExtName, metadata, true);
  }
  
  /**
   * Uploads a slave file associated with a master file
   * 
   * Slave files are typically thumbnails, previews, or other variants of a
   * master file. They are stored on the same storage server as the master
   * file and share the same group.
   * 
   * @param {string} masterFileId - The file ID of the master file
   * @param {string} prefixName - Prefix for the slave file (e.g., "thumb", "small")
   * @param {string} fileExtName - File extension without dot
   * @param {Buffer} data - The slave file content as a Buffer
   * @param {Object.<string, string>} [metadata] - Optional metadata
   * @returns {Promise<string>} The file ID for the slave file
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {InvalidArgumentError} If parameters are invalid
   * @throws {FileNotFoundError} If the master file does not exist
   * @throws {NetworkError} If network communication fails
   * 
   * @example Upload a thumbnail
   * const masterId = await client.uploadFile('photo.jpg');
   * const thumbnailData = await generateThumbnail(photoData);
   * const thumbId = await client.uploadSlaveFile(masterId, 'thumb', 'jpg', thumbnailData);
   */
  async uploadSlaveFile(masterFileId, prefixName, fileExtName, data, metadata = null) {
    this._checkClosed();
    
    if (!masterFileId || typeof masterFileId !== 'string') {
      throw new InvalidArgumentError('masterFileId is required');
    }
    if (!prefixName || typeof prefixName !== 'string') {
      throw new InvalidArgumentError('prefixName is required');
    }
    if (!fileExtName || typeof fileExtName !== 'string') {
      throw new InvalidArgumentError('fileExtName is required');
    }
    if (!Buffer.isBuffer(data)) {
      throw new InvalidArgumentError('data must be a Buffer');
    }
    
    return this.operations.uploadSlaveFile(masterFileId, prefixName, fileExtName, data, metadata);
  }
  
  /**
   * Downloads a file from FastDFS and returns its content
   * 
   * @param {string} fileId - The file ID to download
   * @returns {Promise<Buffer>} File content as a Buffer
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example
   * const data = await client.downloadFile(fileId);
   * await fs.writeFile('downloaded.jpg', data);
   */
  async downloadFile(fileId) {
    this._checkClosed();
    return this.operations.downloadFile(fileId, 0, 0);
  }
  
  /**
   * Downloads a specific range of bytes from a file
   * 
   * This method allows partial file downloads, which is useful for large files
   * or when implementing resumable downloads.
   * 
   * @param {string} fileId - The file ID to download
   * @param {number} offset - Starting byte offset (0-based)
   * @param {number} length - Number of bytes to download (0 means to end of file)
   * @returns {Promise<Buffer>} Requested file content as a Buffer
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example Download first 1024 bytes
   * const header = await client.downloadFileRange(fileId, 0, 1024);
   * 
   * @example Download from offset to end
   * const tail = await client.downloadFileRange(fileId, 1000, 0);
   */
  async downloadFileRange(fileId, offset, length) {
    this._checkClosed();
    
    if (typeof offset !== 'number' || offset < 0) {
      throw new InvalidArgumentError('offset must be >= 0');
    }
    if (typeof length !== 'number' || length < 0) {
      throw new InvalidArgumentError('length must be >= 0');
    }
    
    return this.operations.downloadFile(fileId, offset, length);
  }
  
  /**
   * Downloads a file and saves it to the local filesystem
   * 
   * @param {string} fileId - The file ID to download
   * @param {string} localFilename - Path where to save the file
   * @returns {Promise<void>}
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {NetworkError} If network communication fails
   * @throws {Error} If local file cannot be written
   * 
   * @example
   * await client.downloadToFile(fileId, '/path/to/save/image.jpg');
   */
  async downloadToFile(fileId, localFilename) {
    this._checkClosed();
    
    if (!localFilename || typeof localFilename !== 'string') {
      throw new InvalidArgumentError('localFilename is required');
    }
    
    const data = await this.operations.downloadFile(fileId, 0, 0);
    await fs.writeFile(localFilename, data);
  }
  
  /**
   * Deletes a file from FastDFS
   * 
   * @param {string} fileId - The file ID to delete
   * @returns {Promise<void>}
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example
   * await client.deleteFile(fileId);
   */
  async deleteFile(fileId) {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    
    return this.operations.deleteFile(fileId);
  }
  
  /**
   * Appends data to an appender file
   * 
   * This method adds data to the end of an appender file. The file must have
   * been uploaded as an appender file using uploadAppenderFile or
   * uploadAppenderBuffer.
   * 
   * @param {string} fileId - The file ID of the appender file
   * @param {Buffer} data - The data to append as a Buffer
   * @returns {Promise<void>}
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {OperationNotSupportedError} If the file is not an appender file
   * @throws {NetworkError} If network communication fails
   * 
   * @example Append to a log file
   * const fileId = await client.uploadAppenderFile('log.txt');
   * await client.appendFile(fileId, Buffer.from('Entry 1\n'));
   * await client.appendFile(fileId, Buffer.from('Entry 2\n'));
   */
  async appendFile(fileId, data) {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    if (!Buffer.isBuffer(data)) {
      throw new InvalidArgumentError('data must be a Buffer');
    }
    
    return this.operations.appendFile(fileId, data);
  }
  
  /**
   * Modifies content of an appender file at specified offset
   * 
   * This method overwrites data in an appender file starting at the given
   * offset. The file must be an appender file.
   * 
   * @param {string} fileId - The file ID of the appender file
   * @param {number} offset - Byte offset where to start modifying (0-based)
   * @param {Buffer} data - The new data as a Buffer
   * @returns {Promise<void>}
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {OperationNotSupportedError} If the file is not an appender file
   * @throws {NetworkError} If network communication fails
   * 
   * @example Modify file content
   * await client.modifyFile(fileId, 0, Buffer.from('New header\n'));
   */
  async modifyFile(fileId, offset, data) {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    if (typeof offset !== 'number' || offset < 0) {
      throw new InvalidArgumentError('offset must be >= 0');
    }
    if (!Buffer.isBuffer(data)) {
      throw new InvalidArgumentError('data must be a Buffer');
    }
    
    return this.operations.modifyFile(fileId, offset, data);
  }
  
  /**
   * Truncates an appender file to specified size
   * 
   * This method reduces the size of an appender file to the given length.
   * Data beyond the new size is permanently lost.
   * 
   * @param {string} fileId - The file ID of the appender file
   * @param {number} size - The new size in bytes
   * @returns {Promise<void>}
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {OperationNotSupportedError} If the file is not an appender file
   * @throws {NetworkError} If network communication fails
   * 
   * @example Truncate a file
   * await client.truncateFile(fileId, 1024); // Truncate to 1KB
   */
  async truncateFile(fileId, size) {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    if (typeof size !== 'number' || size < 0) {
      throw new InvalidArgumentError('size must be >= 0');
    }
    
    return this.operations.truncateFile(fileId, size);
  }
  
  /**
   * Sets metadata for a file
   * 
   * Metadata can be used to store custom key-value pairs associated with a
   * file. Keys are limited to 64 characters and values to 256 characters.
   * 
   * @param {string} fileId - The file ID
   * @param {Object.<string, string>} metadata - Metadata key-value pairs
   * @param {string} [flag='OVERWRITE'] - Metadata operation flag: 'OVERWRITE' or 'MERGE'
   * @returns {Promise<void>}
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {InvalidMetadataError} If metadata format is invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example Set metadata with overwrite
   * await client.setMetadata(fileId, {
   *   author: 'John Doe',
   *   date: '2025-01-01'
   * }, 'OVERWRITE');
   * 
   * @example Merge metadata
   * await client.setMetadata(fileId, { version: '2.0' }, 'MERGE');
   */
  async setMetadata(fileId, metadata, flag = 'OVERWRITE') {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    if (!metadata || typeof metadata !== 'object') {
      throw new InvalidArgumentError('metadata is required');
    }
    
    const flagValue = flag === 'MERGE' ? MetadataFlag.MERGE : MetadataFlag.OVERWRITE;
    return this.operations.setMetadata(fileId, metadata, flagValue);
  }
  
  /**
   * Retrieves metadata for a file
   * 
   * @param {string} fileId - The file ID
   * @returns {Promise<Object.<string, string>>} Dictionary of metadata key-value pairs
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example
   * const metadata = await client.getMetadata(fileId);
   * console.log('Author:', metadata.author);
   */
  async getMetadata(fileId) {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    
    return this.operations.getMetadata(fileId);
  }
  
  /**
   * Retrieves file information including size, create time, and CRC32
   * 
   * @param {string} fileId - The file ID
   * @returns {Promise<{fileSize: number, createTime: Date, crc32: number, sourceIpAddr: string}>} File information
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {FileNotFoundError} If the file does not exist
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {NetworkError} If network communication fails
   * 
   * @example
   * const info = await client.getFileInfo(fileId);
   * console.log('Size:', info.fileSize, 'bytes');
   * console.log('Created:', info.createTime);
   * console.log('CRC32:', info.crc32);
   */
  async getFileInfo(fileId) {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    
    return this.operations.getFileInfo(fileId);
  }
  
  /**
   * Checks if a file exists on the storage server
   * 
   * This method attempts to retrieve file information. If successful, the
   * file exists. If FileNotFoundError is raised, the file does not exist.
   * 
   * @param {string} fileId - The file ID to check
   * @returns {Promise<boolean>} True if file exists, false otherwise
   * 
   * @throws {ClientClosedError} If the client has been closed
   * @throws {InvalidFileIDError} If file ID format is invalid
   * @throws {NetworkError} If network communication fails (not file not found)
   * 
   * @example
   * const exists = await client.fileExists(fileId);
   * if (exists) {
   *   console.log('File exists');
   * } else {
   *   console.log('File does not exist');
   * }
   */
  async fileExists(fileId) {
    this._checkClosed();
    
    if (!fileId || typeof fileId !== 'string') {
      throw new InvalidArgumentError('fileId is required');
    }
    
    try {
      await this.operations.getFileInfo(fileId);
      return true;
    } catch (error) {
      if (error.name === 'FileNotFoundError') {
        return false;
      }
      throw error;
    }
  }
  
  /**
   * Closes the client and releases all resources
   * 
   * After calling close, all operations will throw ClientClosedError.
   * It's safe to call close multiple times.
   * 
   * @returns {Promise<void>}
   * 
   * @example
   * await client.close();
   * 
   * @example Use with try-finally
   * const client = new Client(config);
   * try {
   *   // Use client...
   * } finally {
   *   await client.close();
   * }
   */
  async close() {
    if (this.closed) {
      return;
    }
    
    this.closed = true;
    
    // Close connection pools
    if (this.trackerPool) {
      this.trackerPool.close();
    }
    
    if (this.storagePool) {
      this.storagePool.close();
    }
  }
}

// Export the Client class
module.exports = { Client };
