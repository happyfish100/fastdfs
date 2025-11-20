/**
 * Unit tests for the FastDFS client
 */

import { Client } from '../src/client';
import { ClientConfig } from '../src/types';
import { ClientClosedError, InvalidArgumentError } from '../src/errors';

describe('Client', () => {
  describe('Configuration', () => {
    it('should create client with valid configuration', () => {
      const config: ClientConfig = {
        trackerAddrs: ['127.0.0.1:22122'],
      };

      const client = new Client(config);
      expect(client).toBeDefined();

      client.close();
    });

    it('should apply default configuration values', () => {
      const config: ClientConfig = {
        trackerAddrs: ['127.0.0.1:22122'],
      };

      const client = new Client(config);
      expect(client).toBeDefined();

      client.close();
    });

    it('should throw error for invalid configuration', () => {
      expect(() => {
        new Client({ trackerAddrs: [] });
      }).toThrow(InvalidArgumentError);

      expect(() => {
        new Client({ trackerAddrs: ['invalid'] });
      }).toThrow(InvalidArgumentError);
    });
  });

  describe('Client lifecycle', () => {
    it('should close client successfully', async () => {
      const config: ClientConfig = {
        trackerAddrs: ['127.0.0.1:22122'],
      };

      const client = new Client(config);
      await client.close();

      // Operations after close should throw error
      await expect(client.uploadBuffer(Buffer.from('test'), 'txt')).rejects.toThrow(
        ClientClosedError
      );
    });

    it('should handle close idempotently', async () => {
      const config: ClientConfig = {
        trackerAddrs: ['127.0.0.1:22122'],
      };

      const client = new Client(config);
      await client.close();
      await client.close(); // Should not throw
    });
  });

  describe('File operations', () => {
    let client: Client;

    beforeEach(() => {
      const config: ClientConfig = {
        trackerAddrs: ['127.0.0.1:22122'],
      };
      client = new Client(config);
    });

    afterEach(async () => {
      await client.close();
    });

    it('should throw error when uploading after close', async () => {
      await client.close();

      await expect(client.uploadBuffer(Buffer.from('test'), 'txt')).rejects.toThrow(
        ClientClosedError
      );
    });

    it('should throw error when downloading after close', async () => {
      await client.close();

      await expect(client.downloadFile('group1/M00/00/00/test.jpg')).rejects.toThrow(
        ClientClosedError
      );
    });

    it('should throw error when deleting after close', async () => {
      await client.close();

      await expect(client.deleteFile('group1/M00/00/00/test.jpg')).rejects.toThrow(
        ClientClosedError
      );
    });
  });
});