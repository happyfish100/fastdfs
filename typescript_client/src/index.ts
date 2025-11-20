/**
 * FastDFS TypeScript Client Library
 * 
 * Official TypeScript client for FastDFS distributed file system.
 * Provides a high-level, type-safe API for interacting with FastDFS servers.
 * 
 * @packageDocumentation
 */

export { Client } from './client';

export {
  ClientConfig,
  FileInfo,
  StorageServer,
  MetadataFlag,
  Metadata,
  TrackerCommand,
  StorageCommand,
  StorageStatus,
} from './types';

export {
  FastDFSError,
  ClientClosedError,
  FileNotFoundError,
  NoStorageServerError,
  ConnectionTimeoutError,
  NetworkTimeoutError,
  InvalidFileIDError,
  InvalidResponseError,
  StorageServerOfflineError,
  TrackerServerOfflineError,
  InsufficientSpaceError,
  FileAlreadyExistsError,
  InvalidMetadataError,
  OperationNotSupportedError,
  InvalidArgumentError,
  ProtocolError,
  NetworkError,
} from './errors';