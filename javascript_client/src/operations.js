/**
 * FastDFS Operations Handler
 * 
 * This module implements all FastDFS file operations with automatic retry logic,
 * error handling, and protocol communication.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

'use strict';

const {
  TrackerCommand,
  StorageCommand,
  FDFS_GROUP_NAME_MAX_LEN,
  FDFS_FILE_EXT_NAME_MAX_LEN,
  FDFS_FILE_PREFIX_MAX_LEN,
  IP_ADDRESS_SIZE,
} = require('./types');

const {
  encodeHeader,
  decodeHeader,
  padString,
  parseFileId,
  encodeMetadata,
  decodeMetadata,
  encodeUploadRequest,
  decodeUploadResponse,
  encodeDownloadRequest,
  decodeFileInfo,
} = require('./protocol');

const { mapStatusToError, NetworkError } = require('./errors');

/**
 * Operations handler for FastDFS file operations
 * 
 * This class implements all file operations with retry logic and error handling.
 * It communicates with tracker and storage servers using the FastDFS protocol.
 */
class Operations {
  /**
   * Creates a new Operations instance
   * 
   * @param {ConnectionPool} trackerPool - Connection pool for tracker servers
   * @param {ConnectionPool} storagePool - Connection pool for storage servers
   * @param {number} networkTimeout - Network I/O timeout in milliseconds
   * @param {number} retryCount - Number of retries for failed operations
   */
  constructor(trackerPool, storagePool, networkTimeout, retryCount) {
    this.trackerPool = trackerPool;
    this.storagePool = storagePool;
    this.networkTimeout = networkTimeout;
    this.retryCount = retryCount;
  }

  /**
   * Executes an operation with automatic retry logic
   * 
   * @private
   * @param {Function} operation - Async function to execute
   * @returns {Promise<*>} Result of the operation
   */
  async _withRetry(operation) {
    let lastError;
    
    for (let attempt = 0; attempt <= this.retryCount; attempt++) {
      try {
        return await operation();
      } catch (error) {
        lastError = error;
        
        // Don't retry on certain errors
        if (error.name === 'FileNotFoundError' ||
            error.name === 'InvalidFileIDError' ||
            error.name === 'InvalidArgumentError' ||
            error.name === 'ClientClosedError') {
          throw error;
        }
        
        // If this was the last attempt, throw the error
        if (attempt === this.retryCount) {
          break;
        }
        
        // Wait a bit before retrying (exponential backoff)
        await new Promise(resolve => setTimeout(resolve, Math.min(1000 * Math.pow(2, attempt), 5000)));
      }
    }
    
    throw lastError;
  }

  /**
   * Queries tracker for a storage server to upload to
   * 
   * @private
   * @param {string} [groupName] - Optional group name
   * @returns {Promise<{ipAddr: string, port: number, storePathIndex: number}>} Storage server info
   */
  async _queryStorageForUpload(groupName = null) {
    const conn = await this.trackerPool.get();
    
    try {
      let cmd, bodyLen, body;
      
      if (groupName) {
        // Query with specific group
        cmd = TrackerCommand.SERVICE_QUERY_STORE_WITH_GROUP_ONE;
        body = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
        bodyLen = body.length;
      } else {
        // Query without group
        cmd = TrackerCommand.SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE;
        bodyLen = 0;
        body = Buffer.alloc(0);
      }
      
      // Send request
      const header = encodeHeader(bodyLen, cmd);
      await conn.send(Buffer.concat([header, body]), this.networkTimeout);
      
      // Receive response header
      const respHeader = await conn.receiveFull(10, this.networkTimeout);
      const { length, status } = decodeHeader(respHeader);
      
      // Check for errors
      const error = mapStatusToError(status);
      if (error) throw error;
      
      // Receive response body
      const respBody = await conn.receiveFull(length, this.networkTimeout);
      
      // Parse storage server info
      const groupNameResp = respBody.toString('utf8', 0, FDFS_GROUP_NAME_MAX_LEN).replace(/\0/g, '');
      const ipAddr = respBody.toString('utf8', FDFS_GROUP_NAME_MAX_LEN, FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE).replace(/\0/g, '');
      const port = respBody.readBigInt64BE(FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE);
      const storePathIndex = respBody.readUInt8(FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 8);
      
      // Add storage server to pool
      const storageAddr = `${ipAddr}:${port}`;
      this.storagePool.addAddr(storageAddr);
      
      return { ipAddr, port: Number(port), storePathIndex };
    } finally {
      this.trackerPool.put(conn);
    }
  }

  /**
   * Queries tracker for a storage server to download/update from
   * 
   * @private
   * @param {string} groupName - Group name
   * @param {string} remoteFilename - Remote filename
   * @returns {Promise<{ipAddr: string, port: number}>} Storage server info
   */
  async _queryStorageForUpdate(groupName, remoteFilename) {
    const conn = await this.trackerPool.get();
    
    try {
      const filenameBuf = Buffer.from(remoteFilename, 'utf8');
      const bodyLen = FDFS_GROUP_NAME_MAX_LEN + filenameBuf.length;
      const body = Buffer.alloc(bodyLen);
      
      // Encode group name
      const groupBuf = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
      groupBuf.copy(body, 0);
      
      // Encode filename
      filenameBuf.copy(body, FDFS_GROUP_NAME_MAX_LEN);
      
      // Send request
      const header = encodeHeader(bodyLen, TrackerCommand.SERVICE_QUERY_UPDATE);
      await conn.send(Buffer.concat([header, body]), this.networkTimeout);
      
      // Receive response
      const respHeader = await conn.receiveFull(10, this.networkTimeout);
      const { length, status } = decodeHeader(respHeader);
      
      const error = mapStatusToError(status);
      if (error) throw error;
      
      const respBody = await conn.receiveFull(length, this.networkTimeout);
      
      // Parse storage server info
      const groupNameResp = respBody.toString('utf8', 0, FDFS_GROUP_NAME_MAX_LEN).replace(/\0/g, '');
      const ipAddr = respBody.toString('utf8', FDFS_GROUP_NAME_MAX_LEN, FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE).replace(/\0/g, '');
      const port = respBody.readBigInt64BE(FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE);
      
      // Add storage server to pool
      const storageAddr = `${ipAddr}:${port}`;
      this.storagePool.addAddr(storageAddr);
      
      return { ipAddr, port: Number(port) };
    } finally {
      this.trackerPool.put(conn);
    }
  }

  /**
   * Uploads a buffer to FastDFS
   * 
   * @param {Buffer} fileData - File content
   * @param {string} fileExtName - File extension without dot
   * @param {Object.<string, string>} metadata - Optional metadata
   * @param {boolean} isAppender - Whether to upload as appender file
   * @returns {Promise<string>} File ID
   */
  async uploadBuffer(fileData, fileExtName, metadata, isAppender) {
    return this._withRetry(async () => {
      // Query tracker for storage server
      const storageInfo = await this._queryStorageForUpload();
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      // Get connection to storage server
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        // Prepare upload request
        const fileSize = fileData.length;
        const body = encodeUploadRequest(storageInfo.storePathIndex, fileSize, fileExtName, fileData);
        
        // Choose command based on file type
        const cmd = isAppender ? StorageCommand.UPLOAD_APPENDER_FILE : StorageCommand.UPLOAD_FILE;
        
        // Send request
        const header = encodeHeader(body.length, cmd);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { length, status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
        
        const respBody = await conn.receiveFull(length, this.networkTimeout);
        
        // Parse upload response
        const { groupName, remoteFilename } = decodeUploadResponse(respBody);
        const fileId = `${groupName}/${remoteFilename}`;
        
        // Set metadata if provided
        if (metadata && Object.keys(metadata).length > 0) {
          await this.setMetadata(fileId, metadata, 0x4f); // OVERWRITE
        }
        
        return fileId;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Uploads a slave file
   * 
   * @param {string} masterFileId - Master file ID
   * @param {string} prefixName - Prefix for slave file
   * @param {string} fileExtName - File extension
   * @param {Buffer} fileData - File content
   * @param {Object.<string, string>} metadata - Optional metadata
   * @returns {Promise<string>} Slave file ID
   */
  async uploadSlaveFile(masterFileId, prefixName, fileExtName, fileData, metadata) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(masterFileId);
      
      // Query tracker for storage server
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      // Get connection to storage server
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const masterFilenameBuf = Buffer.from(remoteFilename, 'utf8');
        const fileSize = fileData.length;
        
        // Calculate body length
        const bodyLen = 8 + 8 + FDFS_FILE_PREFIX_MAX_LEN + FDFS_FILE_EXT_NAME_MAX_LEN + 
                       masterFilenameBuf.length + fileSize;
        const body = Buffer.alloc(bodyLen);
        
        let offset = 0;
        
        // Master filename length
        const high = Math.floor(masterFilenameBuf.length / 0x100000000);
        const low = masterFilenameBuf.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, offset);
        body.writeUInt32BE(low, offset + 4);
        offset += 8;
        
        // File size
        const sizeHigh = Math.floor(fileSize / 0x100000000);
        const sizeLow = fileSize & 0xFFFFFFFF;
        body.writeUInt32BE(sizeHigh, offset);
        body.writeUInt32BE(sizeLow, offset + 4);
        offset += 8;
        
        // Prefix name
        const prefixBuf = padString(prefixName, FDFS_FILE_PREFIX_MAX_LEN);
        prefixBuf.copy(body, offset);
        offset += FDFS_FILE_PREFIX_MAX_LEN;
        
        // File extension
        const extBuf = padString(fileExtName, FDFS_FILE_EXT_NAME_MAX_LEN);
        extBuf.copy(body, offset);
        offset += FDFS_FILE_EXT_NAME_MAX_LEN;
        
        // Master filename
        masterFilenameBuf.copy(body, offset);
        offset += masterFilenameBuf.length;
        
        // File data
        fileData.copy(body, offset);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.UPLOAD_SLAVE_FILE);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { length, status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
        
        const respBody = await conn.receiveFull(length, this.networkTimeout);
        
        // Parse response
        const { groupName: respGroup, remoteFilename: slaveFilename } = decodeUploadResponse(respBody);
        const fileId = `${respGroup}/${slaveFilename}`;
        
        // Set metadata if provided
        if (metadata && Object.keys(metadata).length > 0) {
          await this.setMetadata(fileId, metadata, 0x4f);
        }
        
        return fileId;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Downloads a file from FastDFS
   * 
   * @param {string} fileId - File ID
   * @param {number} offset - Starting offset
   * @param {number} downloadBytes - Number of bytes to download (0 = all)
   * @returns {Promise<Buffer>} File content
   */
  async downloadFile(fileId, offset, downloadBytes) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      // Query tracker for storage server
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      // Get connection to storage server
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        // Prepare download request
        const body = encodeDownloadRequest(offset, downloadBytes, groupName, remoteFilename);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.DOWNLOAD_FILE);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { length, status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
        
        // Receive file data
        const fileData = await conn.receiveFull(length, this.networkTimeout);
        
        return fileData;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Deletes a file from FastDFS
   * 
   * @param {string} fileId - File ID
   * @returns {Promise<void>}
   */
  async deleteFile(fileId) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      // Query tracker for storage server
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      // Get connection to storage server
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const filenameBuf = Buffer.from(remoteFilename, 'utf8');
        const bodyLen = FDFS_GROUP_NAME_MAX_LEN + filenameBuf.length;
        const body = Buffer.alloc(bodyLen);
        
        // Encode group name
        const groupBuf = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
        groupBuf.copy(body, 0);
        
        // Encode filename
        filenameBuf.copy(body, FDFS_GROUP_NAME_MAX_LEN);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.DELETE_FILE);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Appends data to an appender file
   * 
   * @param {string} fileId - File ID
   * @param {Buffer} data - Data to append
   * @returns {Promise<void>}
   */
  async appendFile(fileId, data) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const filenameBuf = Buffer.from(remoteFilename, 'utf8');
        const bodyLen = 8 + 8 + filenameBuf.length + data.length;
        const body = Buffer.alloc(bodyLen);
        
        let offset = 0;
        
        // Filename length
        let high = Math.floor(filenameBuf.length / 0x100000000);
        let low = filenameBuf.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, offset);
        body.writeUInt32BE(low, offset + 4);
        offset += 8;
        
        // Data length
        high = Math.floor(data.length / 0x100000000);
        low = data.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, offset);
        body.writeUInt32BE(low, offset + 4);
        offset += 8;
        
        // Filename
        filenameBuf.copy(body, offset);
        offset += filenameBuf.length;
        
        // Data
        data.copy(body, offset);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.APPEND_FILE);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Modifies an appender file
   * 
   * @param {string} fileId - File ID
   * @param {number} offset - Offset to modify at
   * @param {Buffer} data - New data
   * @returns {Promise<void>}
   */
  async modifyFile(fileId, offset, data) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const filenameBuf = Buffer.from(remoteFilename, 'utf8');
        const bodyLen = 8 + 8 + 8 + filenameBuf.length + data.length;
        const body = Buffer.alloc(bodyLen);
        
        let pos = 0;
        
        // Filename length
        let high = Math.floor(filenameBuf.length / 0x100000000);
        let low = filenameBuf.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, pos);
        body.writeUInt32BE(low, pos + 4);
        pos += 8;
        
        // Offset
        high = Math.floor(offset / 0x100000000);
        low = offset & 0xFFFFFFFF;
        body.writeUInt32BE(high, pos);
        body.writeUInt32BE(low, pos + 4);
        pos += 8;
        
        // Data length
        high = Math.floor(data.length / 0x100000000);
        low = data.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, pos);
        body.writeUInt32BE(low, pos + 4);
        pos += 8;
        
        // Filename
        filenameBuf.copy(body, pos);
        pos += filenameBuf.length;
        
        // Data
        data.copy(body, pos);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.MODIFY_FILE);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Truncates an appender file
   * 
   * @param {string} fileId - File ID
   * @param {number} truncatedFileSize - New file size
   * @returns {Promise<void>}
   */
  async truncateFile(fileId, truncatedFileSize) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const filenameBuf = Buffer.from(remoteFilename, 'utf8');
        const bodyLen = 8 + 8 + filenameBuf.length;
        const body = Buffer.alloc(bodyLen);
        
        let offset = 0;
        
        // Filename length
        let high = Math.floor(filenameBuf.length / 0x100000000);
        let low = filenameBuf.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, offset);
        body.writeUInt32BE(low, offset + 4);
        offset += 8;
        
        // Truncated file size
        high = Math.floor(truncatedFileSize / 0x100000000);
        low = truncatedFileSize & 0xFFFFFFFF;
        body.writeUInt32BE(high, offset);
        body.writeUInt32BE(low, offset + 4);
        offset += 8;
        
        // Filename
        filenameBuf.copy(body, offset);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.TRUNCATE_FILE);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Sets metadata for a file
   * 
   * @param {string} fileId - File ID
   * @param {Object.<string, string>} metadata - Metadata
   * @param {number} flag - Metadata flag (OVERWRITE or MERGE)
   * @returns {Promise<void>}
   */
  async setMetadata(fileId, metadata, flag) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const filenameBuf = Buffer.from(remoteFilename, 'utf8');
        const metaBuf = encodeMetadata(metadata);
        
        const bodyLen = 8 + 8 + 1 + FDFS_GROUP_NAME_MAX_LEN + filenameBuf.length + metaBuf.length;
        const body = Buffer.alloc(bodyLen);
        
        let offset = 0;
        
        // Filename length
        let high = Math.floor(filenameBuf.length / 0x100000000);
        let low = filenameBuf.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, offset);
        body.writeUInt32BE(low, offset + 4);
        offset += 8;
        
        // Metadata length
        high = Math.floor(metaBuf.length / 0x100000000);
        low = metaBuf.length & 0xFFFFFFFF;
        body.writeUInt32BE(high, offset);
        body.writeUInt32BE(low, offset + 4);
        offset += 8;
        
        // Flag
        body.writeUInt8(flag, offset);
        offset += 1;
        
        // Group name
        const groupBuf = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
        groupBuf.copy(body, offset);
        offset += FDFS_GROUP_NAME_MAX_LEN;
        
        // Filename
        filenameBuf.copy(body, offset);
        offset += filenameBuf.length;
        
        // Metadata
        metaBuf.copy(body, offset);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.SET_METADATA);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Gets metadata for a file
   * 
   * @param {string} fileId - File ID
   * @returns {Promise<Object.<string, string>>} Metadata
   */
  async getMetadata(fileId) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const filenameBuf = Buffer.from(remoteFilename, 'utf8');
        const bodyLen = FDFS_GROUP_NAME_MAX_LEN + filenameBuf.length;
        const body = Buffer.alloc(bodyLen);
        
        // Group name
        const groupBuf = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
        groupBuf.copy(body, 0);
        
        // Filename
        filenameBuf.copy(body, FDFS_GROUP_NAME_MAX_LEN);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.GET_METADATA);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { length, status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
        
        const respBody = await conn.receiveFull(length, this.networkTimeout);
        
        // Decode metadata
        return decodeMetadata(respBody);
      } finally {
        this.storagePool.put(conn);
      }
    });
  }

  /**
   * Gets file information
   * 
   * @param {string} fileId - File ID
   * @returns {Promise<{fileSize: number, createTime: Date, crc32: number, sourceIpAddr: string}>} File info
   */
  async getFileInfo(fileId) {
    return this._withRetry(async () => {
      const { groupName, remoteFilename } = parseFileId(fileId);
      
      const storageInfo = await this._queryStorageForUpdate(groupName, remoteFilename);
      const storageAddr = `${storageInfo.ipAddr}:${storageInfo.port}`;
      
      const conn = await this.storagePool.get(storageAddr);
      
      try {
        const filenameBuf = Buffer.from(remoteFilename, 'utf8');
        const bodyLen = FDFS_GROUP_NAME_MAX_LEN + filenameBuf.length;
        const body = Buffer.alloc(bodyLen);
        
        // Group name
        const groupBuf = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
        groupBuf.copy(body, 0);
        
        // Filename
        filenameBuf.copy(body, FDFS_GROUP_NAME_MAX_LEN);
        
        // Send request
        const header = encodeHeader(body.length, StorageCommand.QUERY_FILE_INFO);
        await conn.send(Buffer.concat([header, body]), this.networkTimeout);
        
        // Receive response
        const respHeader = await conn.receiveFull(10, this.networkTimeout);
        const { length, status } = decodeHeader(respHeader);
        
        const error = mapStatusToError(status);
        if (error) throw error;
        
        const respBody = await conn.receiveFull(length, this.networkTimeout);
        
        // Decode file info
        return decodeFileInfo(respBody);
      } finally {
        this.storagePool.put(conn);
      }
    });
  }
}

module.exports = { Operations };
