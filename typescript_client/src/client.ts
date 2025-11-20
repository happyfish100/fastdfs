/**
 * FastDFS TypeScript Client
 * 
 * Main client class for interacting with FastDFS distributed file system.
 */

import { ConnectionPool } from './connection';
import { Operations } from './operations';
import { ClientConfig, FileInfo, MetadataFlag, Metadata } from './types';
import { ClientClosedError, InvalidArgumentError } from './errors';

/**
 * FastDFS client for file operations
 * 
 * This client provides a high-level, TypeScript API for interacting with FastDFS servers.
 * It handles connection pooling, automatic retries, and error handling.
 * 
 * @example
 * ```typescript
 * const config: ClientConfig = {
 *   trackerAddrs: ['192.168.1.100:22122']
 * };
 * const client = new Client(config);
 * const fileId = await client.uploadFile('test.jpg');
 * const data = await client.downloadFile(fileId);
 * await client.deleteFile(fileId);
 * await client.close();
 * ```
 */
export class Client {
  private config: Required<ClientConfig>;
  private trackerPool: ConnectionPool;
  private storagePool: ConnectionPool;
  private ops: Operations;
  private closed: boolean = false;

  constructor(config: ClientConfig) {
    this.validateConfig(config);

    // Set defaults
    this.config = {
      trackerAddrs: config.trackerAddrs,
      maxConns: config.maxConns ?? 10,
      connectTimeout: config.connectTimeout ?? 5000,
      networkTimeout: config.networkTimeout ?? 30000,
      idleTimeout: config.idleTimeout ?? 60000,
      retryCount: config.retryCount ?? 3,
    };

    // Initialize connection pools
    this.trackerPool = new ConnectionPool(
      this.config.trackerAddrs,
      this.config.maxConns,
      this.config.connectTimeout,
      this.config.idleTimeout
    );

    this.storagePool = new ConnectionPool(
      [], // Storage servers are discovered dynamically
      this.config.maxConns,
      this.config.connectTimeout,
      this.config.idleTimeout
    );

    // Initialize operations handler
    this.ops = new Operations(
      this.trackerPool,
      this.storagePool,
      this.config.networkTimeout,
      this.config.retryCount
    );
  }

  /**
   * Validates the client configuration
   */
  private validateConfig(config: ClientConfig): void {
    if (!config) {
      throw new InvalidArgumentError('Config is required');
    }

    if (!config.trackerAddrs || config.trackerAddrs.length === 0) {
      throw new InvalidArgumentError('Tracker addresses are required');
    }

    for (const addr of config.trackerAddrs) {
      if (!addr || !addr.includes(':')) {
        throw new InvalidArgumentError(`Invalid tracker address: ${addr}`);
      }
    }
  }

  /**
   * Checks if the client is closed and throws an error if so
   */
  private checkClosed(): void {
    if (this.closed) {
      throw new ClientClosedError();
    }
  }

  /**
   * Uploads a file from the local filesystem to FastDFS
   */
  async uploadFile(localFilename: string, metadata?: Metadata): Promise<string> {
    this.checkClosed();
    return this.ops.uploadFile(localFilename, metadata, false);
  }

  /**
   * Uploads data from a buffer to FastDFS
   */
  async uploadBuffer(data: Buffer, fileExtName: string, metadata?: Metadata): Promise<string> {
    this.checkClosed();
    return this.ops.uploadBuffer(data, fileExtName, metadata, false);
  }

  /**
   * Uploads an appender file that can be modified later
   */
  async uploadAppenderFile(localFilename: string, metadata?: Metadata): Promise<string> {
    this.checkClosed();
    return this.ops.uploadFile(localFilename, metadata, true);
  }

  /**
   * Uploads an appender file from buffer
   */
  async uploadAppenderBuffer(
    data: Buffer,
    fileExtName: string,
    metadata?: Metadata
  ): Promise<string> {
    this.checkClosed();
    return this.ops.uploadBuffer(data, fileExtName, metadata, true);
  }

  /**
   * Downloads a file from FastDFS and returns its content
   */
  async downloadFile(fileId: string): Promise<Buffer> {
    this.checkClosed();
    return this.ops.downloadFile(fileId, 0, 0);
  }

  /**
   * Downloads a specific range of bytes from a file
   */
  async downloadFileRange(fileId: string, offset: number, length: number): Promise<Buffer> {
    this.checkClosed();
    return this.ops.downloadFile(fileId, offset, length);
  }

  /**
   * Downloads a file and saves it to the local filesystem
   */
  async downloadToFile(fileId: string, localFilename: string): Promise<void> {
    this.checkClosed();
    await this.ops.downloadToFile(fileId, localFilename);
  }

  /**
   * Deletes a file from FastDFS
   */
  async deleteFile(fileId: string): Promise<void> {
    this.checkClosed();
    await this.ops.deleteFile(fileId);
  }

  /**
   * Sets metadata for a file
   */
  async setMetadata(
    fileId: string,
    metadata: Metadata,
    flag: MetadataFlag = MetadataFlag.OVERWRITE
  ): Promise<void> {
    this.checkClosed();
    await this.ops.setMetadata(fileId, metadata, flag);
  }

  /**
   * Retrieves metadata for a file
   */
  async getMetadata(fileId: string): Promise<Metadata> {
    this.checkClosed();
    return this.ops.getMetadata(fileId);
  }

  /**
   * Retrieves file information including size, create time, and CRC32
   */
  async getFileInfo(fileId: string): Promise<FileInfo> {
    this.checkClosed();
    return this.ops.getFileInfo(fileId);
  }

  /**
   * Checks if a file exists on the storage server
   */
  async fileExists(fileId: string): Promise<boolean> {
    this.checkClosed();
    try {
      await this.ops.getFileInfo(fileId);
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Closes the client and releases all resources
   * 
   * After calling close, all operations will throw ClientClosedError.
   * It's safe to call close multiple times.
   */
  async close(): Promise<void> {
    if (this.closed) {
      return;
    }

    this.closed = true;

    this.trackerPool.close();
    this.storagePool.close();
  }
}