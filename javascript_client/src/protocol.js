/**
 * FastDFS Protocol Utilities
 * 
 * This module provides low-level protocol encoding and decoding functions
 * for communicating with FastDFS tracker and storage servers.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

'use strict';

const {
  FDFS_PROTO_HEADER_LEN,
  FDFS_GROUP_NAME_MAX_LEN,
  FDFS_FILE_EXT_NAME_MAX_LEN,
  FDFS_FILE_PREFIX_MAX_LEN,
  FDFS_RECORD_SEPARATOR,
  FDFS_FIELD_SEPARATOR,
  IP_ADDRESS_SIZE,
} = require('./types');

const { InvalidResponseError, InvalidFileIDError, InvalidMetadataError } = require('./errors');

/**
 * Encodes a FastDFS protocol header
 * 
 * The header is 10 bytes:
 *   - 8 bytes: body length (big-endian 64-bit integer)
 *   - 1 byte: command code
 *   - 1 byte: status code (0 for requests)
 * 
 * @param {number} bodyLength - Length of the message body
 * @param {number} cmd - Command code
 * @param {number} [status=0] - Status code (usually 0 for requests)
 * @returns {Buffer} The encoded header (10 bytes)
 */
function encodeHeader(bodyLength, cmd, status = 0) {
  const header = Buffer.alloc(FDFS_PROTO_HEADER_LEN);
  
  // Write body length as 64-bit big-endian integer
  // JavaScript doesn't have native 64-bit integers, so we split it
  const high = Math.floor(bodyLength / 0x100000000);
  const low = bodyLength & 0xFFFFFFFF;
  header.writeUInt32BE(high, 0);
  header.writeUInt32BE(low, 4);
  
  // Write command and status bytes
  header.writeUInt8(cmd, 8);
  header.writeUInt8(status, 9);
  
  return header;
}

/**
 * Decodes a FastDFS protocol header
 * 
 * @param {Buffer} headerBuf - The header buffer (must be 10 bytes)
 * @returns {{length: number, cmd: number, status: number}} Decoded header
 * @throws {InvalidResponseError} If header is invalid
 */
function decodeHeader(headerBuf) {
  if (!headerBuf || headerBuf.length < FDFS_PROTO_HEADER_LEN) {
    throw new InvalidResponseError('Header too short');
  }
  
  // Read 64-bit body length
  const high = headerBuf.readUInt32BE(0);
  const low = headerBuf.readUInt32BE(4);
  const length = high * 0x100000000 + low;
  
  // Read command and status
  const cmd = headerBuf.readUInt8(8);
  const status = headerBuf.readUInt8(9);
  
  return { length, cmd, status };
}

/**
 * Pads a string to a fixed length with null bytes
 * 
 * If the string is longer than maxLen, it will be truncated.
 * 
 * @param {string} str - String to pad
 * @param {number} maxLen - Maximum length
 * @returns {Buffer} Padded buffer
 */
function padString(str, maxLen) {
  const buf = Buffer.alloc(maxLen);
  if (str) {
    const strBuf = Buffer.from(str, 'utf8');
    strBuf.copy(buf, 0, 0, Math.min(strBuf.length, maxLen));
  }
  return buf;
}

/**
 * Reads a null-terminated or fixed-length string from a buffer
 * 
 * @param {Buffer} buf - Buffer to read from
 * @param {number} [offset=0] - Offset to start reading
 * @param {number} [maxLen] - Maximum length to read
 * @returns {string} The extracted string
 */
function readString(buf, offset = 0, maxLen) {
  if (!buf) return '';
  
  const endOffset = maxLen ? offset + maxLen : buf.length;
  let nullIndex = buf.indexOf(0, offset);
  
  // If no null byte found or it's beyond maxLen, use endOffset
  if (nullIndex === -1 || nullIndex > endOffset) {
    nullIndex = endOffset;
  }
  
  return buf.toString('utf8', offset, nullIndex);
}

/**
 * Parses a file ID into group name and remote filename
 * 
 * File IDs are in the format "group/path/filename" or "group/M00/path/filename"
 * 
 * @param {string} fileId - The file ID to parse
 * @returns {{groupName: string, remoteFilename: string}} Parsed components
 * @throws {InvalidFileIDError} If file ID format is invalid
 */
function parseFileId(fileId) {
  if (!fileId || typeof fileId !== 'string') {
    throw new InvalidFileIDError(fileId);
  }
  
  const slashIndex = fileId.indexOf('/');
  if (slashIndex === -1 || slashIndex === 0 || slashIndex === fileId.length - 1) {
    throw new InvalidFileIDError(fileId);
  }
  
  const groupName = fileId.substring(0, slashIndex);
  const remoteFilename = fileId.substring(slashIndex + 1);
  
  if (!groupName || !remoteFilename) {
    throw new InvalidFileIDError(fileId);
  }
  
  return { groupName, remoteFilename };
}

/**
 * Encodes metadata into FastDFS protocol format
 * 
 * Metadata is encoded as: key1\x02value1\x01key2\x02value2\x01...
 * where \x02 is the field separator and \x01 is the record separator.
 * 
 * @param {Object.<string, string>} metadata - Metadata key-value pairs
 * @returns {Buffer} Encoded metadata buffer
 * @throws {InvalidMetadataError} If metadata format is invalid
 */
function encodeMetadata(metadata) {
  if (!metadata || typeof metadata !== 'object') {
    return Buffer.alloc(0);
  }
  
  const parts = [];
  
  for (const [key, value] of Object.entries(metadata)) {
    if (!key || typeof key !== 'string') {
      throw new InvalidMetadataError('Metadata key must be a non-empty string');
    }
    if (value === null || value === undefined) {
      throw new InvalidMetadataError(`Metadata value for key "${key}" cannot be null or undefined`);
    }
    
    const keyBuf = Buffer.from(key, 'utf8');
    const valueBuf = Buffer.from(String(value), 'utf8');
    
    // Format: key\x02value\x01
    parts.push(keyBuf);
    parts.push(Buffer.from([FDFS_FIELD_SEPARATOR]));
    parts.push(valueBuf);
    parts.push(Buffer.from([FDFS_RECORD_SEPARATOR]));
  }
  
  return Buffer.concat(parts);
}

/**
 * Decodes metadata from FastDFS protocol format
 * 
 * @param {Buffer} metaBuf - Encoded metadata buffer
 * @returns {Object.<string, string>} Decoded metadata key-value pairs
 */
function decodeMetadata(metaBuf) {
  if (!metaBuf || metaBuf.length === 0) {
    return {};
  }
  
  const metadata = {};
  const records = [];
  let start = 0;
  
  // Split by record separator
  for (let i = 0; i < metaBuf.length; i++) {
    if (metaBuf[i] === FDFS_RECORD_SEPARATOR) {
      if (i > start) {
        records.push(metaBuf.slice(start, i));
      }
      start = i + 1;
    }
  }
  
  // Process each record
  for (const record of records) {
    // Find field separator
    const sepIndex = record.indexOf(FDFS_FIELD_SEPARATOR);
    if (sepIndex === -1) continue;
    
    const key = record.toString('utf8', 0, sepIndex);
    const value = record.toString('utf8', sepIndex + 1);
    
    if (key) {
      metadata[key] = value;
    }
  }
  
  return metadata;
}

/**
 * Encodes an upload request body
 * 
 * @param {number} storePathIndex - Storage path index
 * @param {number} fileSize - Size of the file
 * @param {string} fileExtName - File extension without dot
 * @param {Buffer} fileData - File content
 * @returns {Buffer} Encoded request body
 */
function encodeUploadRequest(storePathIndex, fileSize, fileExtName, fileData) {
  const bodyLen = 1 + 8 + FDFS_FILE_EXT_NAME_MAX_LEN + fileSize;
  const body = Buffer.alloc(bodyLen);
  
  let offset = 0;
  
  // Store path index (1 byte)
  body.writeUInt8(storePathIndex, offset);
  offset += 1;
  
  // File size (8 bytes, big-endian)
  const high = Math.floor(fileSize / 0x100000000);
  const low = fileSize & 0xFFFFFFFF;
  body.writeUInt32BE(high, offset);
  body.writeUInt32BE(low, offset + 4);
  offset += 8;
  
  // File extension (padded to FDFS_FILE_EXT_NAME_MAX_LEN)
  const extBuf = padString(fileExtName, FDFS_FILE_EXT_NAME_MAX_LEN);
  extBuf.copy(body, offset);
  offset += FDFS_FILE_EXT_NAME_MAX_LEN;
  
  // File data
  fileData.copy(body, offset);
  
  return body;
}

/**
 * Decodes an upload response
 * 
 * @param {Buffer} responseBuf - Response buffer
 * @returns {{groupName: string, remoteFilename: string}} Upload response
 * @throws {InvalidResponseError} If response format is invalid
 */
function decodeUploadResponse(responseBuf) {
  if (!responseBuf || responseBuf.length < FDFS_GROUP_NAME_MAX_LEN + 1) {
    throw new InvalidResponseError('Upload response too short');
  }
  
  const groupName = readString(responseBuf, 0, FDFS_GROUP_NAME_MAX_LEN);
  const remoteFilename = readString(responseBuf, FDFS_GROUP_NAME_MAX_LEN);
  
  return { groupName, remoteFilename };
}

/**
 * Encodes a download request body
 * 
 * @param {number} offset - Starting byte offset
 * @param {number} downloadBytes - Number of bytes to download (0 = all)
 * @param {string} groupName - Storage group name
 * @param {string} remoteFilename - Remote filename
 * @returns {Buffer} Encoded request body
 */
function encodeDownloadRequest(offset, downloadBytes, groupName, remoteFilename) {
  const filenameBuf = Buffer.from(remoteFilename, 'utf8');
  const bodyLen = 8 + 8 + FDFS_GROUP_NAME_MAX_LEN + filenameBuf.length;
  const body = Buffer.alloc(bodyLen);
  
  let pos = 0;
  
  // Offset (8 bytes)
  let high = Math.floor(offset / 0x100000000);
  let low = offset & 0xFFFFFFFF;
  body.writeUInt32BE(high, pos);
  body.writeUInt32BE(low, pos + 4);
  pos += 8;
  
  // Download bytes (8 bytes)
  high = Math.floor(downloadBytes / 0x100000000);
  low = downloadBytes & 0xFFFFFFFF;
  body.writeUInt32BE(high, pos);
  body.writeUInt32BE(low, pos + 4);
  pos += 8;
  
  // Group name (padded)
  const groupBuf = padString(groupName, FDFS_GROUP_NAME_MAX_LEN);
  groupBuf.copy(body, pos);
  pos += FDFS_GROUP_NAME_MAX_LEN;
  
  // Remote filename
  filenameBuf.copy(body, pos);
  
  return body;
}

/**
 * Decodes file info response
 * 
 * @param {Buffer} responseBuf - Response buffer
 * @returns {{fileSize: number, createTime: Date, crc32: number, sourceIpAddr: string}} File info
 * @throws {InvalidResponseError} If response format is invalid
 */
function decodeFileInfo(responseBuf) {
  const expectedLen = 8 + 8 + 8 + IP_ADDRESS_SIZE;
  if (!responseBuf || responseBuf.length < expectedLen) {
    throw new InvalidResponseError('File info response too short');
  }
  
  let offset = 0;
  
  // File size (8 bytes)
  const sizeHigh = responseBuf.readUInt32BE(offset);
  const sizeLow = responseBuf.readUInt32BE(offset + 4);
  const fileSize = sizeHigh * 0x100000000 + sizeLow;
  offset += 8;
  
  // Create timestamp (8 bytes)
  const timeHigh = responseBuf.readUInt32BE(offset);
  const timeLow = responseBuf.readUInt32BE(offset + 4);
  const createTimestamp = timeHigh * 0x100000000 + timeLow;
  const createTime = new Date(createTimestamp * 1000);
  offset += 8;
  
  // CRC32 (8 bytes, but only lower 4 bytes are used)
  offset += 4; // Skip high 4 bytes
  const crc32 = responseBuf.readUInt32BE(offset);
  offset += 4;
  
  // Source IP address
  const sourceIpAddr = readString(responseBuf, offset, IP_ADDRESS_SIZE);
  
  return { fileSize, createTime, crc32, sourceIpAddr };
}

// Export all protocol functions
module.exports = {
  encodeHeader,
  decodeHeader,
  padString,
  readString,
  parseFileId,
  encodeMetadata,
  decodeMetadata,
  encodeUploadRequest,
  decodeUploadResponse,
  encodeDownloadRequest,
  decodeFileInfo,
};
