/**
 * Integration tests for FastDFS client
 * 
 * These tests require a running FastDFS cluster.
 * Set the environment variable FASTDFS_TRACKER_ADDR to run these tests.
 */

import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import { Client } from '../src/client';
import { ClientConfig, MetadataFlag } from '../src/types';
import { FileNotFoundError } from '../src/errors';

const TRACKER_ADDR = process.env.FASTDFS_TRACKER_ADDR || '127.0.0.1:22122';
const RUN_INTEGRATION_TESTS = process.env.FASTDFS_TRACKER_ADDR !== undefined;

const describeIf = RUN_INTEGRATION_TESTS ? describe : describe.skip;

describeIf('Integration Tests', () => {
  let client: Client;

  beforeAll(() => {
    const config: ClientConfig = {
      trackerAddrs: [TRACKER_ADDR],
    };
    client = new Client(config);
  });

  afterAll(async () => {
    await client.close();
  });

  it('should complete upload, download, delete cycle', async () => {
    // Upload
    const testData = Buffer.from('Hello, FastDFS! This is a test file.');
    const fileId = await client.uploadBuffer(testData, 'txt');

    expect(fileId).toBeDefined();
    expect(fileId).toContain('/');

    // Download
    const downloadedData = await client.downloadFile(fileId);
    expect(downloadedData.toString()).toBe(testData.toString());

    // Delete
    await client.deleteFile(fileId);

    // Verify deletion
    await expect(client.downloadFile(fileId)).rejects.toThrow(FileNotFoundError);
  });

  it('should upload file from disk', async () => {
    // Create temporary file
    const tempDir = os.tmpdir();
    const tempFile = path.join(tempDir, `test-${Date.now()}.txt`);
    const testData = Buffer.from('Test file content from disk');
    fs.writeFileSync(tempFile, testData);

    try {
      // Upload
      const fileId = await client.uploadFile(tempFile);
      expect(fileId).toBeDefined();

      // Download and verify
      const downloadedData = await client.downloadFile(fileId);
      expect(downloadedData.toString()).toBe(testData.toString());

      // Clean up
      await client.deleteFile(fileId);
    } finally {
      fs.unlinkSync(tempFile);
    }
  });

  it('should download file to disk', async () => {
    // Upload
    const testData = Buffer.from('Test data for download to file');
    const fileId = await client.uploadBuffer(testData, 'bin');

    // Download to file
    const tempDir = os.tmpdir();
    const tempFile = path.join(tempDir, `download-${Date.now()}.bin`);

    try {
      await client.downloadToFile(fileId, tempFile);

      // Verify
      const downloadedData = fs.readFileSync(tempFile);
      expect(downloadedData.toString()).toBe(testData.toString());
    } finally {
      fs.unlinkSync(tempFile);
      await client.deleteFile(fileId);
    }
  });

  it('should handle metadata operations', async () => {
    // Upload file with metadata
    const testData = Buffer.from('File with metadata');
    const metadata = {
      author: 'Test User',
      date: '2025-01-15',
      version: '1.0',
    };
    const fileId = await client.uploadBuffer(testData, 'txt', metadata);

    try {
      // Get metadata
      const retrievedMetadata = await client.getMetadata(fileId);
      expect(Object.keys(retrievedMetadata).length).toBe(Object.keys(metadata).length);
      Object.entries(metadata).forEach(([key, value]) => {
        expect(retrievedMetadata[key]).toBe(value);
      });

      // Update metadata (overwrite)
      const newMetadata = {
        author: 'Updated User',
        status: 'modified',
      };
      await client.setMetadata(fileId, newMetadata, MetadataFlag.OVERWRITE);

      const updatedMetadata = await client.getMetadata(fileId);
      expect(Object.keys(updatedMetadata).length).toBe(Object.keys(newMetadata).length);
      expect(updatedMetadata.author).toBe('Updated User');
      expect(updatedMetadata.status).toBe('modified');
    } finally {
      await client.deleteFile(fileId);
    }
  });

  it('should get file information', async () => {
    // Upload file
    const testData = Buffer.from('Test data for file info');
    const fileId = await client.uploadBuffer(testData, 'bin');

    try {
      // Get file info
      const fileInfo = await client.getFileInfo(fileId);

      expect(fileInfo.fileSize).toBe(testData.length);
      expect(fileInfo.createTime).toBeInstanceOf(Date);
      expect(fileInfo.crc32).toBeGreaterThan(0);
      expect(fileInfo.sourceIpAddr).toBeDefined();
    } finally {
      await client.deleteFile(fileId);
    }
  });

  it('should check file existence', async () => {
    // Upload file
    const testData = Buffer.from('Test existence check');
    const fileId = await client.uploadBuffer(testData, 'txt');

    // Check existence
    let exists = await client.fileExists(fileId);
    expect(exists).toBe(true);

    // Delete and check again
    await client.deleteFile(fileId);
    exists = await client.fileExists(fileId);
    expect(exists).toBe(false);
  });

  it('should download file range', async () => {
    // Upload file
    const testData = Buffer.from('0123456789'.repeat(10)); // 100 bytes
    const fileId = await client.uploadBuffer(testData, 'bin');

    try {
      // Download range
      const offset = 10;
      const length = 20;
      const rangeData = await client.downloadFileRange(fileId, offset, length);

      expect(rangeData.length).toBe(length);
      expect(rangeData.toString()).toBe(testData.slice(offset, offset + length).toString());
    } finally {
      await client.deleteFile(fileId);
    }
  });
});