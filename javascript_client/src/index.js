/**
 * FastDFS JavaScript Client
 * 
 * Main entry point for the FastDFS JavaScript client library.
 * 
 * Copyright (C) 2025 FastDFS JavaScript Client Contributors
 * 
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 * 
 * @module fastdfs-client
 */

'use strict';

// Export main client class
const { Client } = require('./client');

// Export error classes
const {
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
} = require('./errors');

// Export types and constants
const {
  TRACKER_DEFAULT_PORT,
  STORAGE_DEFAULT_PORT,
  TrackerCommand,
  StorageCommand,
  StorageStatus,
  MetadataFlag,
} = require('./types');

// Export all
module.exports = {
  // Main client
  Client,
  
  // Error classes
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
  
  // Constants
  TRACKER_DEFAULT_PORT,
  STORAGE_DEFAULT_PORT,
  TrackerCommand,
  StorageCommand,
  StorageStatus,
  MetadataFlag,
};
