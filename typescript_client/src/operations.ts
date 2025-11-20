/**
 * FastDFS Operations
 * 
 * This module implements all file operations (upload, download, delete, etc.)
 * for the FastDFS client.
 */

import {
  encodeHeader,
  decodeHeader,
  splitFileId,
  joinFileId,
  encodeMetadata,
  decodeMetadata,
  getFileExtName,
  readFileContent,
  writeFileContent,
  padString,
  unpadString,
  encodeInt64,
  decodeInt64,
  encodeInt32,
  decodeInt32,
} from './protocol';
import {
  FDFS_GROUP_NAME_MAX_LEN,
  FDFS_FILE_EXT_NAME_MAX_LEN,
  IP_ADDRESS_SIZE,
  TrackerCommand,
  StorageCommand,
  StorageServer,
  FileInfo,
  MetadataFlag,
  Metadata,
} from './types';
import {
  InvalidResponseError,
  NoStorageServerError,
  mapStatusToError,
} from './errors';
import { ConnectionPool } from './connection';

/**
 * Handles all FastDFS file operations
 * 
 * This class is used internally by the Client class.
 */
export class Operations {
  private trackerPool: ConnectionPool;
  private storagePool: ConnectionPool;
  private networkTimeout: number;
  private retryCount: number;

  constructor(
    trackerPool: ConnectionPool,
    storagePool: ConnectionPool,
    networkTimeout: number,
    retryCount: number
  ) {
    this.trackerPool = trackerPool;
    this.storagePool = storagePool;
    this.networkTimeout = networkTimeout;
    this.retryCount = retryCount;
  }

  /**
   * Uploads a file from the local filesystem
   */
  async uploadFile(
    localFilename: string,
    metadata?: Metadata,
    isAppender: boolean = false
  ): Promise<string> {
    const fileData = readFileContent(localFilename);
    const extName = getFileExtName(localFilename);
    return this.uploadBuffer(fileData, extName, metadata, isAppender);
  }

  /**
   * Uploads data from a buffer
   */
  async uploadBuffer(
    data: Buffer,
    fileExtName: string,
    metadata?: Metadata,
    isAppender: boolean = false
  ): Promise<string> {
    for (let attempt = 0; attempt < this.retryCount; attempt++) {
      try {
        return await this.uploadBufferInternal(data, fileExtName, metadata, isAppender);
      } catch (error) {
        if (attempt === this.retryCount - 1) {
          throw error;
        }
        await this.sleep((attempt + 1) * 1000);
      }
    }
    throw new Error('Upload failed after retries');
  }

  /**
   * Internal implementation of buffer upload
   */
  private async uploadBufferInternal(
    data: Buffer,
    fileExtName: string,
    metadata?: Metadata,
    isAppender: boolean = false
  ): Promise<string> {
    // Get storage server from tracker
    const storageServer = await this.getStorageServer('');

    // Get connection to storage server
    const storageAddr = `${storageServer.ipAddr}:${storageServer.port}`;
    this.storagePool.addAddr(storageAddr);
    const conn = await this.storagePool.get(storageAddr);

    try {
      // Prepare upload command
      const cmd = isAppender ? StorageCommand.UPLOAD_APPENDER_FILE : StorageCommand.UPLOAD_FILE;

      // Build request
      const extNameBytes = padString(fileExtName, FDFS_FILE_EXT_NAME_MAX_LEN);
      const storePathIndex = storageServer.storePathIndex;

      const bodyLen = 1 + FDFS_FILE_EXT_NAME_MAX_LEN + data.length;
      const reqHeader = encodeHeader(bodyLen, cmd, 0);

      // Send request
      await conn.send(reqHeader, this.networkTimeout);
      await conn.send(Buffer.from([storePathIndex]), this.networkTimeout);
      await conn.send(extNameBytes, this.networkTimeout);
      await conn.send(data, this.networkTimeout);

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }

      if (respHeader.length <= 0) {
        throw new InvalidResponseError('Empty response body');
      }

      const respBody = await conn.receiveFull(respHeader.length, this.networkTimeout);

      // Parse response
      if (respBody.length < FDFS_GROUP_NAME_MAX_LEN) {
        throw new InvalidResponseError('Response body too short');
      }

      const groupName = unpadString(respBody.slice(0, FDFS_GROUP_NAME_MAX_LEN));
      const remoteFilename = respBody.slice(FDFS_GROUP_NAME_MAX_LEN).toString('utf8');

      const fileId = joinFileId(groupName, remoteFilename);

      // Set metadata if provided
      if (metadata && Object.keys(metadata).length > 0) {
        try {
          await this.setMetadata(fileId, metadata, MetadataFlag.OVERWRITE);
        } catch {
          // Metadata setting failed, but file is uploaded
        }
      }

      return fileId;
    } finally {
      this.storagePool.put(conn);
    }
  }

  /**
   * Gets a storage server from tracker for upload
   */
  private async getStorageServer(groupName: string): Promise<StorageServer> {
    const conn = await this.trackerPool.get();

    try {
      // Prepare request
      const cmd = groupName
        ? TrackerCommand.SERVICE_QUERY_STORE_WITH_GROUP_ONE
        : TrackerCommand.SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE;
      const bodyLen = groupName ? FDFS_GROUP_NAME_MAX_LEN : 0;

      const header = encodeHeader(bodyLen, cmd, 0);
      await conn.send(header, this.networkTimeout);

      if (groupName) {
        const groupNameBytes = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
        await conn.send(groupNameBytes, this.networkTimeout);
      }

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }

      if (respHeader.length <= 0) {
        throw new NoStorageServerError();
      }

      const respBody = await conn.receiveFull(respHeader.length, this.networkTimeout);

      // Parse storage server info
      if (respBody.length < FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 9) {
        throw new InvalidResponseError('Storage server response too short');
      }

      let offset = FDFS_GROUP_NAME_MAX_LEN;
      const ipAddr = unpadString(respBody.slice(offset, offset + IP_ADDRESS_SIZE));
      offset += IP_ADDRESS_SIZE;

      const port = decodeInt64(respBody.slice(offset, offset + 8));
      offset += 8;

      const storePathIndex = respBody.readUInt8(offset);

      return {
        ipAddr,
        port,
        storePathIndex,
      };
    } finally {
      this.trackerPool.put(conn);
    }
  }

  /**
   * Downloads a file from FastDFS
   */
  async downloadFile(fileId: string, offset: number = 0, length: number = 0): Promise<Buffer> {
    for (let attempt = 0; attempt < this.retryCount; attempt++) {
      try {
        return await this.downloadFileInternal(fileId, offset, length);
      } catch (error) {
        if (attempt === this.retryCount - 1) {
          throw error;
        }
        await this.sleep((attempt + 1) * 1000);
      }
    }
    throw new Error('Download failed after retries');
  }

  /**
   * Internal implementation of file download
   */
  private async downloadFileInternal(
    fileId: string,
    offset: number,
    length: number
  ): Promise<Buffer> {
    const [groupName, remoteFilename] = splitFileId(fileId);

    // Get storage server for download
    const storageServer = await this.getDownloadStorageServer(groupName, remoteFilename);

    // Get connection
    const storageAddr = `${storageServer.ipAddr}:${storageServer.port}`;
    this.storagePool.addAddr(storageAddr);
    const conn = await this.storagePool.get(storageAddr);

    try {
      // Build request
      const remoteFilenameBytes = Buffer.from(remoteFilename, 'utf8');
      const bodyLen = 16 + remoteFilenameBytes.length;
      const header = encodeHeader(bodyLen, StorageCommand.DOWNLOAD_FILE, 0);

      const body = Buffer.concat([
        encodeInt64(offset),
        encodeInt64(length),
        remoteFilenameBytes,
      ]);

      // Send request
      await conn.send(header, this.networkTimeout);
      await conn.send(body, this.networkTimeout);

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }

      if (respHeader.length <= 0) {
        return Buffer.alloc(0);
      }

      // Receive file data
      const data = await conn.receiveFull(respHeader.length, this.networkTimeout);
      return data;
    } finally {
      this.storagePool.put(conn);
    }
  }

  /**
   * Gets a storage server from tracker for download
   */
  private async getDownloadStorageServer(
    groupName: string,
    remoteFilename: string
  ): Promise<StorageServer> {
    const conn = await this.trackerPool.get();

    try {
      // Build request
      const remoteFilenameBytes = Buffer.from(remoteFilename, 'utf8');
      const bodyLen = FDFS_GROUP_NAME_MAX_LEN + remoteFilenameBytes.length;
      const header = encodeHeader(bodyLen, TrackerCommand.SERVICE_QUERY_FETCH_ONE, 0);

      const body = Buffer.concat([
        padString(groupName, FDFS_GROUP_NAME_MAX_LEN),
        remoteFilenameBytes,
      ]);

      // Send request
      await conn.send(header, this.networkTimeout);
      await conn.send(body, this.networkTimeout);

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }

      const respBody = await conn.receiveFull(respHeader.length, this.networkTimeout);

      // Parse response
      if (respBody.length < FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 8) {
        throw new InvalidResponseError('Download storage server response too short');
      }

      let offset = FDFS_GROUP_NAME_MAX_LEN;
      const ipAddr = unpadString(respBody.slice(offset, offset + IP_ADDRESS_SIZE));
      offset += IP_ADDRESS_SIZE;

      const port = decodeInt64(respBody.slice(offset, offset + 8));

      return { ipAddr, port, storePathIndex: 0 };
    } finally {
      this.trackerPool.put(conn);
    }
  }

  /**
   * Downloads a file and saves it to the local filesystem
   */
  async downloadToFile(fileId: string, localFilename: string): Promise<void> {
    const data = await this.downloadFile(fileId, 0, 0);
    writeFileContent(localFilename, data);
  }

  /**
   * Deletes a file from FastDFS
   */
  async deleteFile(fileId: string): Promise<void> {
    for (let attempt = 0; attempt < this.retryCount; attempt++) {
      try {
        await this.deleteFileInternal(fileId);
        return;
      } catch (error) {
        if (attempt === this.retryCount - 1) {
          throw error;
        }
        await this.sleep((attempt + 1) * 1000);
      }
    }
  }

  /**
   * Internal implementation of file deletion
   */
  private async deleteFileInternal(fileId: string): Promise<void> {
    const [groupName, remoteFilename] = splitFileId(fileId);

    // Get storage server
    const storageServer = await this.getDownloadStorageServer(groupName, remoteFilename);

    // Get connection
    const storageAddr = `${storageServer.ipAddr}:${storageServer.port}`;
    this.storagePool.addAddr(storageAddr);
    const conn = await this.storagePool.get(storageAddr);

    try {
      // Build request
      const remoteFilenameBytes = Buffer.from(remoteFilename, 'utf8');
      const bodyLen = FDFS_GROUP_NAME_MAX_LEN + remoteFilenameBytes.length;
      const header = encodeHeader(bodyLen, StorageCommand.DELETE_FILE, 0);

      const body = Buffer.concat([
        padString(groupName, FDFS_GROUP_NAME_MAX_LEN),
        remoteFilenameBytes,
      ]);

      // Send request
      await conn.send(header, this.networkTimeout);
      await conn.send(body, this.networkTimeout);

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }
    } finally {
      this.storagePool.put(conn);
    }
  }

  /**
   * Sets metadata for a file
   */
  async setMetadata(fileId: string, metadata: Metadata, flag: MetadataFlag): Promise<void> {
    const [groupName, remoteFilename] = splitFileId(fileId);

    // Get storage server
    const storageServer = await this.getDownloadStorageServer(groupName, remoteFilename);

    // Get connection
    const storageAddr = `${storageServer.ipAddr}:${storageServer.port}`;
    this.storagePool.addAddr(storageAddr);
    const conn = await this.storagePool.get(storageAddr);

    try {
      // Encode metadata
      const metadataBytes = encodeMetadata(metadata);
      const remoteFilenameBytes = Buffer.from(remoteFilename, 'utf8');

      // Build request
      const bodyLen =
        2 * 8 + 1 + FDFS_GROUP_NAME_MAX_LEN + remoteFilenameBytes.length + metadataBytes.length;
      const header = encodeHeader(bodyLen, StorageCommand.SET_METADATA, 0);

      const body = Buffer.concat([
        encodeInt64(remoteFilenameBytes.length),
        encodeInt64(metadataBytes.length),
        Buffer.from([flag]),
        padString(groupName, FDFS_GROUP_NAME_MAX_LEN),
        remoteFilenameBytes,
        metadataBytes,
      ]);

      // Send request
      await conn.send(header, this.networkTimeout);
      await conn.send(body, this.networkTimeout);

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }
    } finally {
      this.storagePool.put(conn);
    }
  }

  /**
   * Retrieves metadata for a file
   */
  async getMetadata(fileId: string): Promise<Metadata> {
    const [groupName, remoteFilename] = splitFileId(fileId);

    // Get storage server
    const storageServer = await this.getDownloadStorageServer(groupName, remoteFilename);

    // Get connection
    const storageAddr = `${storageServer.ipAddr}:${storageServer.port}`;
    this.storagePool.addAddr(storageAddr);
    const conn = await this.storagePool.get(storageAddr);

    try {
      // Build request
      const remoteFilenameBytes = Buffer.from(remoteFilename, 'utf8');
      const bodyLen = FDFS_GROUP_NAME_MAX_LEN + remoteFilenameBytes.length;
      const header = encodeHeader(bodyLen, StorageCommand.GET_METADATA, 0);

      const body = Buffer.concat([
        padString(groupName, FDFS_GROUP_NAME_MAX_LEN),
        remoteFilenameBytes,
      ]);

      // Send request
      await conn.send(header, this.networkTimeout);
      await conn.send(body, this.networkTimeout);

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }

      if (respHeader.length <= 0) {
        return {};
      }

      const respBody = await conn.receiveFull(respHeader.length, this.networkTimeout);

      // Decode metadata
      return decodeMetadata(respBody);
    } finally {
      this.storagePool.put(conn);
    }
  }

  /**
   * Retrieves file information
   */
  async getFileInfo(fileId: string): Promise<FileInfo> {
    const [groupName, remoteFilename] = splitFileId(fileId);

    // Get storage server
    const storageServer = await this.getDownloadStorageServer(groupName, remoteFilename);

    // Get connection
    const storageAddr = `${storageServer.ipAddr}:${storageServer.port}`;
    this.storagePool.addAddr(storageAddr);
    const conn = await this.storagePool.get(storageAddr);

    try {
      // Build request
      const remoteFilenameBytes = Buffer.from(remoteFilename, 'utf8');
      const bodyLen = FDFS_GROUP_NAME_MAX_LEN + remoteFilenameBytes.length;
      const header = encodeHeader(bodyLen, StorageCommand.QUERY_FILE_INFO, 0);

      const body = Buffer.concat([
        padString(groupName, FDFS_GROUP_NAME_MAX_LEN),
        remoteFilenameBytes,
      ]);

      // Send request
      await conn.send(header, this.networkTimeout);
      await conn.send(body, this.networkTimeout);

      // Receive response
      const respHeaderData = await conn.receiveFull(10, this.networkTimeout);
      const respHeader = decodeHeader(respHeaderData);

      if (respHeader.status !== 0) {
        const error = mapStatusToError(respHeader.status);
        if (error) throw error;
      }

      const respBody = await conn.receiveFull(respHeader.length, this.networkTimeout);

      // Parse file info (file_size, create_time, crc32, source_ip)
      if (respBody.length < 8 + 8 + 4 + IP_ADDRESS_SIZE) {
        throw new InvalidResponseError('File info response too short');
      }

      const fileSize = decodeInt64(respBody.slice(0, 8));
      const createTimestamp = decodeInt64(respBody.slice(8, 16));
      const crc32 = decodeInt32(respBody.slice(16, 20));
      const sourceIp = unpadString(respBody.slice(20, 20 + IP_ADDRESS_SIZE));

      const createTime = new Date(createTimestamp * 1000);

      return {
        fileSize,
        createTime,
        crc32,
        sourceIpAddr: sourceIp,
      };
    } finally {
      this.storagePool.put(conn);
    }
  }

  /**
   * Sleep utility for retry delays
   */
  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }
}