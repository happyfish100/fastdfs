/**
 * Unit tests for protocol encoding and decoding functions
 */

import {
  encodeHeader,
  decodeHeader,
  splitFileId,
  joinFileId,
  encodeMetadata,
  decodeMetadata,
  getFileExtName,
  padString,
  unpadString,
  encodeInt64,
  decodeInt64,
} from '../src/protocol';
import { InvalidFileIDError, InvalidResponseError } from '../src/errors';

describe('Protocol', () => {
  describe('encodeHeader and decodeHeader', () => {
    it('should encode and decode header correctly', () => {
      const length = 1024;
      const cmd = 11;
      const status = 0;

      const encoded = encodeHeader(length, cmd, status);
      expect(encoded.length).toBe(10);

      const decoded = decodeHeader(encoded);
      expect(decoded.length).toBe(length);
      expect(decoded.cmd).toBe(cmd);
      expect(decoded.status).toBe(status);
    });

    it('should throw error for short header data', () => {
      expect(() => decodeHeader(Buffer.from('short'))).toThrow(InvalidResponseError);
    });
  });

  describe('splitFileId', () => {
    it('should split valid file IDs', () => {
      const fileId = 'group1/M00/00/00/test.jpg';
      const [groupName, remoteFilename] = splitFileId(fileId);

      expect(groupName).toBe('group1');
      expect(remoteFilename).toBe('M00/00/00/test.jpg');
    });

    it('should throw error for invalid file IDs', () => {
      const invalidIds = [
        '',
        'group1',
        '/M00/00/00/test.jpg',
        'group1/',
        'verylonggroupname123/M00/00/00/test.jpg',
      ];

      invalidIds.forEach((fileId) => {
        expect(() => splitFileId(fileId)).toThrow(InvalidFileIDError);
      });
    });
  });

  describe('joinFileId', () => {
    it('should join file ID components', () => {
      const groupName = 'group1';
      const remoteFilename = 'M00/00/00/test.jpg';

      const fileId = joinFileId(groupName, remoteFilename);
      expect(fileId).toBe('group1/M00/00/00/test.jpg');
    });
  });

  describe('encodeMetadata and decodeMetadata', () => {
    it('should encode and decode metadata correctly', () => {
      const metadata = {
        author: 'John Doe',
        date: '2025-01-15',
        version: '1.0',
      };

      const encoded = encodeMetadata(metadata);
      expect(encoded).toBeInstanceOf(Buffer);
      expect(encoded.length).toBeGreaterThan(0);

      const decoded = decodeMetadata(encoded);
      expect(Object.keys(decoded).length).toBe(Object.keys(metadata).length);
      Object.entries(metadata).forEach(([key, value]) => {
        expect(decoded[key]).toBe(value);
      });
    });

    it('should handle empty metadata', () => {
      const encoded = encodeMetadata(undefined);
      expect(encoded.length).toBe(0);

      const encoded2 = encodeMetadata({});
      expect(encoded2.length).toBe(0);

      const decoded = decodeMetadata(Buffer.alloc(0));
      expect(Object.keys(decoded).length).toBe(0);
    });
  });

  describe('getFileExtName', () => {
    it('should extract file extensions correctly', () => {
      const testCases: [string, string][] = [
        ['test.jpg', 'jpg'],
        ['file.tar.gz', 'gz'],
        ['noext', ''],
        ['file.verylongext', 'verylo'], // Truncated to 6 chars
        ['.hidden', 'hidden'],
      ];

      testCases.forEach(([filename, expectedExt]) => {
        const ext = getFileExtName(filename);
        expect(ext).toBe(expectedExt);
      });
    });
  });

  describe('padString and unpadString', () => {
    it('should pad and unpad strings correctly', () => {
      const testString = 'test';
      const length = 16;

      const padded = padString(testString, length);
      expect(padded.length).toBe(length);

      const unpadded = unpadString(padded);
      expect(unpadded).toBe(testString);
    });

    it('should truncate long strings when padding', () => {
      const testString = 'verylongstringthatexceedslength';
      const length = 10;

      const padded = padString(testString, length);
      expect(padded.length).toBe(length);
    });
  });

  describe('encodeInt64 and decodeInt64', () => {
    it('should encode and decode 64-bit integers', () => {
      const testValues = [0, 1, 1024, Math.pow(2, 32), Math.pow(2, 53) - 1];

      testValues.forEach((value) => {
        const encoded = encodeInt64(value);
        expect(encoded.length).toBe(8);

        const decoded = decodeInt64(encoded);
        expect(decoded).toBe(value);
      });
    });

    it('should return 0 for short data', () => {
      const result = decodeInt64(Buffer.from('short'));
      expect(result).toBe(0);
    });
  });
});