// ============================================================================
// FastDFS Protocol Parser
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This file implements the protocol message parser for parsing FastDFS
// protocol responses. It handles decoding of responses from upload, download,
// delete, metadata operations, and other FastDFS protocol operations.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FastDFS.Client
{
    /// <summary>
    /// Parser class for parsing FastDFS protocol messages.
    /// 
    /// This class provides static methods for parsing protocol messages
    /// received from FastDFS servers. It extracts information from response
    /// messages and converts them into strongly-typed objects.
    /// 
    /// Protocol messages consist of a 10-byte header followed by message-specific
    /// data. The parser validates message format and extracts relevant information.
    /// </summary>
    internal static class ProtocolParser
    {
        // ====================================================================
        // Header Parsing
        // ====================================================================

        /// <summary>
        /// Parses a protocol header from a byte array.
        /// </summary>
        /// <param name="data">
        /// Byte array containing the protocol header (must be at least 10 bytes).
        /// </param>
        /// <returns>
        /// Parsed protocol header object.
        /// </returns>
        /// <exception cref="ArgumentException">
        /// Thrown when data is too short or invalid.
        /// </exception>
        public static ProtocolHeader ParseHeader(byte[] data)
        {
            if (data == null || data.Length < FastDFSConstants.ProtocolHeaderLength)
            {
                throw new ArgumentException(
                    $"Protocol header must be at least {FastDFSConstants.ProtocolHeaderLength} bytes.",
                    nameof(data));
            }

            // Extract length (8 bytes, big-endian)
            var lengthBytes = new byte[8];
            Array.Copy(data, 0, lengthBytes, 0, 8);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(lengthBytes);
            }
            var length = BitConverter.ToInt64(lengthBytes, 0);

            // Extract command code (1 byte)
            var cmd = data[8];

            // Extract status code (1 byte)
            var status = data[9];

            return new ProtocolHeader
            {
                Length = length,
                Cmd = cmd,
                Status = status
            };
        }

        // ====================================================================
        // Tracker Response Parsing
        // ====================================================================

        /// <summary>
        /// Parses a storage server response from a tracker query.
        /// </summary>
        /// <param name="data">
        /// Byte array containing the protocol response.
        /// </param>
        /// <returns>
        /// Parsed storage server information.
        /// </returns>
        public static StorageServer ParseStorageServerResponse(byte[] data)
        {
            if (data == null || data.Length < FastDFSConstants.ProtocolHeaderLength + 40)
            {
                throw new ArgumentException(
                    "Response data is too short to contain storage server information.",
                    nameof(data));
            }

            // Parse header
            var header = ParseHeader(data);
            if (header.Status != 0)
            {
                throw new FastDFSProtocolException(
                    $"Tracker query failed with status code: {header.Status}");
            }

            // Extract storage server information from body
            var bodyOffset = FastDFSConstants.ProtocolHeaderLength;

            // Extract group name (16 bytes)
            var groupNameBytes = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(data, bodyOffset, groupNameBytes, 0, FastDFSConstants.GroupNameMaxLength);
            var groupName = Encoding.UTF8.GetString(groupNameBytes).TrimEnd('\0');

            // Extract IP address (16 bytes)
            var ipBytes = new byte[FastDFSConstants.IPAddressSize];
            Array.Copy(data, bodyOffset + FastDFSConstants.GroupNameMaxLength, ipBytes, 0, FastDFSConstants.IPAddressSize);
            var ipAddr = Encoding.UTF8.GetString(ipBytes).TrimEnd('\0');

            // Extract port (8 bytes, big-endian)
            var portBytes = new byte[8];
            Array.Copy(data, bodyOffset + FastDFSConstants.GroupNameMaxLength + FastDFSConstants.IPAddressSize, portBytes, 0, 8);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(portBytes);
            }
            var port = (int)BitConverter.ToInt64(portBytes, 0);

            // Extract store path index (1 byte)
            var storePathIndex = data[bodyOffset + FastDFSConstants.GroupNameMaxLength + FastDFSConstants.IPAddressSize + 8];

            return new StorageServer
            {
                GroupName = groupName,
                IPAddr = ipAddr,
                Port = port,
                StorePathIndex = storePathIndex
            };
        }

        // ====================================================================
        // Storage Response Parsing
        // ====================================================================

        /// <summary>
        /// Parses an upload response from a storage server.
        /// </summary>
        /// <param name="data">
        /// Byte array containing the protocol response.
        /// </param>
        /// <returns>
        /// File ID in "group/remote_filename" format.
        /// </returns>
        public static string ParseUploadResponse(byte[] data)
        {
            if (data == null || data.Length < FastDFSConstants.ProtocolHeaderLength + 
                FastDFSConstants.GroupNameMaxLength + FastDFSConstants.RemoteFilenameMaxLength)
            {
                throw new ArgumentException(
                    "Response data is too short to contain upload response.",
                    nameof(data));
            }

            // Parse header
            var header = ParseHeader(data);
            if (header.Status != 0)
            {
                throw new FastDFSProtocolException(
                    $"Upload failed with status code: {header.Status}");
            }

            // Extract group name and remote filename from body
            var bodyOffset = FastDFSConstants.ProtocolHeaderLength;

            var groupNameBytes = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(data, bodyOffset, groupNameBytes, 0, FastDFSConstants.GroupNameMaxLength);
            var groupName = Encoding.UTF8.GetString(groupNameBytes).TrimEnd('\0');

            var filenameBytes = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(data, bodyOffset + FastDFSConstants.GroupNameMaxLength, filenameBytes, 0, FastDFSConstants.RemoteFilenameMaxLength);
            var remoteFilename = Encoding.UTF8.GetString(filenameBytes).TrimEnd('\0');

            return $"{groupName}/{remoteFilename}";
        }

        /// <summary>
        /// Parses a file information response from a storage server.
        /// </summary>
        /// <param name="data">
        /// Byte array containing the protocol response.
        /// </param>
        /// <returns>
        /// Parsed file information object.
        /// </returns>
        public static FileInfo ParseFileInfoResponse(byte[] data)
        {
            if (data == null || data.Length < FastDFSConstants.ProtocolHeaderLength + 40)
            {
                throw new ArgumentException(
                    "Response data is too short to contain file information.",
                    nameof(data));
            }

            // Parse header
            var header = ParseHeader(data);
            if (header.Status != 0)
            {
                throw new FastDFSProtocolException(
                    $"Query file info failed with status code: {header.Status}");
            }

            // Extract file information from body
            var bodyOffset = FastDFSConstants.ProtocolHeaderLength;

            // Extract file size (8 bytes, big-endian)
            var sizeBytes = new byte[8];
            Array.Copy(data, bodyOffset, sizeBytes, 0, 8);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(sizeBytes);
            }
            var fileSize = BitConverter.ToInt64(sizeBytes, 0);

            // Extract creation timestamp (8 bytes, big-endian)
            var timestampBytes = new byte[8];
            Array.Copy(data, bodyOffset + 8, timestampBytes, 0, 8);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(timestampBytes);
            }
            var timestamp = BitConverter.ToInt64(timestampBytes, 0);
            var createTime = DateTimeOffset.FromUnixTimeSeconds(timestamp).DateTime;

            // Extract CRC32 (4 bytes, big-endian)
            var crc32Bytes = new byte[4];
            Array.Copy(data, bodyOffset + 16, crc32Bytes, 0, 4);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(crc32Bytes);
            }
            var crc32 = BitConverter.ToUInt32(crc32Bytes, 0);

            // Extract source IP address (16 bytes)
            var ipBytes = new byte[FastDFSConstants.IPAddressSize];
            Array.Copy(data, bodyOffset + 20, ipBytes, 0, FastDFSConstants.IPAddressSize);
            var sourceIP = Encoding.UTF8.GetString(ipBytes).TrimEnd('\0');

            return new FileInfo
            {
                FileSize = fileSize,
                CreateTime = createTime,
                CRC32 = crc32,
                SourceIPAddr = sourceIP
            };
        }

        /// <summary>
        /// Parses a metadata response from a storage server.
        /// </summary>
        /// <param name="data">
        /// Byte array containing the encoded metadata.
        /// </param>
        /// <returns>
        /// Dictionary of metadata key-value pairs.
        /// </returns>
        public static Dictionary<string, string> ParseMetadata(byte[] data)
        {
            if (data == null || data.Length == 0)
            {
                return new Dictionary<string, string>();
            }

            var metadata = new Dictionary<string, string>();
            var metadataString = Encoding.UTF8.GetString(data);

            // Split by record separator
            var records = metadataString.Split((char)FastDFSConstants.RecordSeparator);
            foreach (var record in records)
            {
                if (string.IsNullOrEmpty(record))
                {
                    continue;
                }

                // Split by field separator
                var parts = record.Split((char)FastDFSConstants.FieldSeparator, 2);
                if (parts.Length == 2)
                {
                    var key = parts[0];
                    var value = parts[1];
                    metadata[key] = value;
                }
            }

            return metadata;
        }
    }
}

