/**
 * FastDFS Protocol Encoding and Decoding
 * 
 * This module handles all protocol-level encoding and decoding operations
 * for communication with FastDFS servers.
 */

import * as fs from 'fs';
import * as path from 'path';
import {
  FDFS_PROTO_HEADER_LEN,
  FDFS_GROUP_NAME_MAX_LEN,
  FDFS_FILE_EXT_NAME_MAX_LEN,
  FDFS_MAX_META_NAME_LEN,
  FDFS_MAX_META_VALUE_LEN,
  FDFS_RECORD_SEPARATOR,
  FDFS_FIELD_SEPARATOR,
  TrackerHeader,
  Metadata,
} from './types';
import { InvalidResponseError, InvalidFileIDError } from './errors';

/**
 * Encodes a FastDFS protocol header into a 10-byte buffer
 * 
 * The header format is:
 *   - Bytes 0-7: Body length (8 bytes, big-endian uint64)
 *   - Byte 8: Command code
 *   - Byte 9: Status code (0 for request, error code for response)
 */
export function encodeHeader(length: number, cmd: number, status: number = 0): Buffer {
  const header = Buffer.alloc(FDFS_PROTO_HEADER_LEN);
  
  // Write length as big-endian 64-bit integer
  header.writeBigUInt64BE(BigInt(length), 0);
  
  // Write command and status
  header.writeUInt8(cmd, 8);
  header.writeUInt8(status, 9);
  
  return header;
}

/**
 * Decodes a FastDFS protocol header from a buffer
 * 
 * The header must be exactly 10 bytes long.
 */
export function decodeHeader(data: Buffer): TrackerHeader {
  if (data.length < FDFS_PROTO_HEADER_LEN) {
    throw new InvalidResponseError(`Header too short: ${data.length} bytes`);
  }
  
  const length = Number(data.readBigUInt64BE(0));
  const cmd = data.readUInt8(8);
  const status = data.readUInt8(9);
  
  return { length, cmd, status };
}

/**
 * Splits a FastDFS file ID into its components
 * 
 * A file ID has the format: "groupName/path/to/file"
 * For example: "group1/M00/00/00/wKgBcFxyz.jpg"
 */
export function splitFileId(fileId: string): [string, string] {
  if (!fileId) {
    throw new InvalidFileIDError(fileId);
  }
  
  const slashIndex = fileId.indexOf('/');
  if (slashIndex === -1) {
    throw new InvalidFileIDError(fileId);
  }
  
  const groupName = fileId.substring(0, slashIndex);
  const remoteFilename = fileId.substring(slashIndex + 1);
  
  if (!groupName || groupName.length > FDFS_GROUP_NAME_MAX_LEN) {
    throw new InvalidFileIDError(fileId);
  }
  
  if (!remoteFilename) {
    throw new InvalidFileIDError(fileId);
  }
  
  return [groupName, remoteFilename];
}

/**
 * Constructs a complete file ID from its components
 * 
 * This is the inverse operation of splitFileId.
 */
export function joinFileId(groupName: string, remoteFilename: string): string {
  return `${groupName}/${remoteFilename}`;
}

/**
 * Encodes metadata key-value pairs into FastDFS wire format
 * 
 * The format uses special separators:
 *   - Field separator (0x02) between key and value
 *   - Record separator (0x01) between different key-value pairs
 * 
 * Format: key1<0x02>value1<0x01>key2<0x02>value2<0x01>
 * 
 * Keys are truncated to 64 bytes and values to 256 bytes if they exceed limits.
 */
export function encodeMetadata(metadata?: Metadata): Buffer {
  if (!metadata || Object.keys(metadata).length === 0) {
    return Buffer.alloc(0);
  }
  
  const buffers: Buffer[] = [];
  
  for (const [key, value] of Object.entries(metadata)) {
    // Truncate if necessary
    const keyBuffer = Buffer.from(key, 'utf8').slice(0, FDFS_MAX_META_NAME_LEN);
    const valueBuffer = Buffer.from(value, 'utf8').slice(0, FDFS_MAX_META_VALUE_LEN);
    
    buffers.push(keyBuffer);
    buffers.push(Buffer.from([FDFS_FIELD_SEPARATOR]));
    buffers.push(valueBuffer);
    buffers.push(Buffer.from([FDFS_RECORD_SEPARATOR]));
  }
  
  return Buffer.concat(buffers);
}

/**
 * Decodes FastDFS wire format metadata into an object
 * 
 * This is the inverse operation of encodeMetadata.
 * 
 * The function parses records separated by 0x01 and fields separated by 0x02.
 * Invalid records (not exactly 2 fields) are silently skipped.
 */
export function decodeMetadata(data: Buffer): Metadata {
  if (!data || data.length === 0) {
    return {};
  }
  
  const metadata: Metadata = {};
  const records = data.toString('binary').split(String.fromCharCode(FDFS_RECORD_SEPARATOR));
  
  for (const record of records) {
    if (!record) continue;
    
    const fields = record.split(String.fromCharCode(FDFS_FIELD_SEPARATOR));
    if (fields.length !== 2) continue;
    
    const key = Buffer.from(fields[0], 'binary').toString('utf8');
    const value = Buffer.from(fields[1], 'binary').toString('utf8');
    metadata[key] = value;
  }
  
  return metadata;
}

/**
 * Extracts and validates the file extension from a filename
 * 
 * The extension is extracted without the leading dot and truncated to 6 characters
 * if it exceeds the FastDFS maximum.
 * 
 * Examples:
 *   "test.jpg" -> "jpg"
 *   "file.tar.gz" -> "gz"
 *   "noext" -> ""
 *   "file.verylongext" -> "verylo" (truncated)
 */
export function getFileExtName(filename: string): string {
  const ext = path.extname(filename);
  let extName = ext.startsWith('.') ? ext.substring(1) : ext;
  
  if (extName.length > FDFS_FILE_EXT_NAME_MAX_LEN) {
    extName = extName.substring(0, FDFS_FILE_EXT_NAME_MAX_LEN);
  }
  
  return extName;
}

/**
 * Reads the entire contents of a file from the filesystem
 */
export function readFileContent(filename: string): Buffer {
  return fs.readFileSync(filename);
}

/**
 * Writes data to a file, creating parent directories if needed
 * 
 * If the file already exists, it will be truncated.
 */
export function writeFileContent(filename: string, data: Buffer): void {
  const dir = path.dirname(filename);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(filename, data);
}

/**
 * Pads a string to a fixed length with null bytes (0x00)
 * 
 * This is used to create fixed-width fields in the FastDFS protocol.
 * If the string is longer than length, it will be truncated.
 */
export function padString(s: string, length: number): Buffer {
  const buffer = Buffer.alloc(length);
  const bytes = Buffer.from(s, 'utf8');
  bytes.copy(buffer, 0, 0, Math.min(bytes.length, length));
  return buffer;
}

/**
 * Removes trailing null bytes from a buffer
 * 
 * This is the inverse of padString, used to extract strings from
 * fixed-width protocol fields.
 */
export function unpadString(data: Buffer): string {
  let end = data.length;
  while (end > 0 && data[end - 1] === 0) {
    end--;
  }
  return data.slice(0, end).toString('utf8');
}

/**
 * Encodes a 64-bit integer to an 8-byte big-endian representation
 * 
 * FastDFS protocol uses big-endian byte order for all numeric fields.
 */
export function encodeInt64(n: number): Buffer {
  const buffer = Buffer.alloc(8);
  buffer.writeBigUInt64BE(BigInt(n), 0);
  return buffer;
}

/**
 * Decodes an 8-byte big-endian representation to a 64-bit integer
 * 
 * This is the inverse of encodeInt64.
 */
export function decodeInt64(data: Buffer): number {
  if (data.length < 8) {
    return 0;
  }
  return Number(data.readBigUInt64BE(0));
}

/**
 * Encodes a 32-bit integer to a 4-byte big-endian representation
 */
export function encodeInt32(n: number): Buffer {
  const buffer = Buffer.alloc(4);
  buffer.writeUInt32BE(n, 0);
  return buffer;
}

/**
 * Decodes a 4-byte big-endian representation to a 32-bit integer
 */
export function decodeInt32(data: Buffer): number {
  if (data.length < 4) {
    return 0;
  }
  return data.readUInt32BE(0);
}