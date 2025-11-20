// ============================================================================
// FastDFS Protocol Builder
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This file implements the protocol message builder for constructing FastDFS
// protocol messages. It handles encoding of requests for upload, download,
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
    /// Builder class for constructing FastDFS protocol messages.
    /// 
    /// This class provides static methods for building protocol messages
    /// according to the FastDFS protocol specification. All messages are
    /// constructed as byte arrays that can be sent directly to FastDFS servers.
    /// 
    /// Protocol messages consist of a 10-byte header followed by message-specific
    /// data. The header contains message length, command code, and status code.
    /// </summary>
    internal static class ProtocolBuilder
    {
        // ====================================================================
        // Tracker Protocol Builders
        // ====================================================================

        /// <summary>
        /// Builds a request message for querying a storage server for upload.
        /// </summary>
        /// <param name="groupName">
        /// Optional group name. If null, tracker selects any available group.
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildQueryStorageStoreRequest(string groupName)
        {
            // Determine command code based on whether group name is specified
            byte cmd = string.IsNullOrEmpty(groupName)
                ? FastDFSConstants.TrackerProtoCmdServiceQueryStoreWithoutGroupOne
                : FastDFSConstants.TrackerProtoCmdServiceQueryStoreWithGroupOne;

            // Build message body
            var body = new List<byte>();
            
            if (!string.IsNullOrEmpty(groupName))
            {
                // Add group name (padded to GroupNameMaxLength)
                var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
                var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
                Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
                body.AddRange(paddedGroupName);
            }

            // Build header and combine with body
            return BuildMessage(cmd, body.ToArray());
        }

        /// <summary>
        /// Builds a request message for querying a storage server for download.
        /// </summary>
        /// <param name="groupName">
        /// Group name containing the file.
        /// </param>
        /// <param name="remoteFilename">
        /// Remote filename on the storage server.
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildQueryStorageFetchRequest(string groupName, string remoteFilename)
        {
            // Build message body
            var body = new List<byte>();

            // Add group name (padded)
            var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
            var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
            body.AddRange(paddedGroupName);

            // Add remote filename (padded)
            var filenameBytes = Encoding.UTF8.GetBytes(remoteFilename);
            var paddedFilename = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(filenameBytes, paddedFilename, Math.Min(filenameBytes.Length, FastDFSConstants.RemoteFilenameMaxLength));
            body.AddRange(paddedFilename);

            // Build header and combine with body
            return BuildMessage(FastDFSConstants.TrackerProtoCmdServiceQueryFetchOne, body.ToArray());
        }

        /// <summary>
        /// Builds a request message for querying a storage server for update.
        /// </summary>
        /// <param name="groupName">
        /// Group name containing the file.
        /// </param>
        /// <param name="remoteFilename">
        /// Remote filename on the storage server.
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildQueryStorageUpdateRequest(string groupName, string remoteFilename)
        {
            // Similar to BuildQueryStorageFetchRequest but with different command code
            var body = new List<byte>();

            var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
            var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
            body.AddRange(paddedGroupName);

            var filenameBytes = Encoding.UTF8.GetBytes(remoteFilename);
            var paddedFilename = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(filenameBytes, paddedFilename, Math.Min(filenameBytes.Length, FastDFSConstants.RemoteFilenameMaxLength));
            body.AddRange(paddedFilename);

            return BuildMessage(FastDFSConstants.TrackerProtoCmdServiceQueryUpdate, body.ToArray());
        }

        // ====================================================================
        // Storage Protocol Builders
        // ====================================================================

        /// <summary>
        /// Builds a request message for uploading a file to a storage server.
        /// </summary>
        /// <param name="data">
        /// File content as byte array.
        /// </param>
        /// <param name="fileExtName">
        /// File extension name without leading dot.
        /// </param>
        /// <param name="metadata">
        /// Optional metadata key-value pairs.
        /// </param>
        /// <param name="storePathIndex">
        /// Storage path index on the storage server.
        /// </param>
        /// <param name="isAppender">
        /// True if this is an appender file, false for regular file.
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildUploadRequest(
            byte[] data,
            string fileExtName,
            Dictionary<string, string> metadata,
            byte storePathIndex,
            bool isAppender)
        {
            // Determine command code
            byte cmd = isAppender
                ? FastDFSConstants.StorageProtoCmdUploadAppenderFile
                : FastDFSConstants.StorageProtoCmdUploadFile;

            // Build message body
            var body = new List<byte>();

            // Add store path index
            body.Add(storePathIndex);

            // Add file size (8 bytes, big-endian)
            var fileSizeBytes = BitConverter.GetBytes((long)data.Length);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(fileSizeBytes);
            }
            body.AddRange(fileSizeBytes);

            // Add file extension (padded)
            var extBytes = Encoding.UTF8.GetBytes(fileExtName ?? "");
            var paddedExt = new byte[FastDFSConstants.FileExtNameMaxLength];
            Array.Copy(extBytes, paddedExt, Math.Min(extBytes.Length, FastDFSConstants.FileExtNameMaxLength));
            body.AddRange(paddedExt);

            // Add metadata if provided
            if (metadata != null && metadata.Count > 0)
            {
                var metadataBytes = EncodeMetadata(metadata);
                body.AddRange(BitConverter.GetBytes((long)metadataBytes.Length));
                if (BitConverter.IsLittleEndian)
                {
                    Array.Reverse(BitConverter.GetBytes((long)metadataBytes.Length));
                }
                body.AddRange(metadataBytes);
            }
            else
            {
                // No metadata - add 0 length
                body.AddRange(new byte[8]); // 8 bytes for 0 length
            }

            // Add file data
            body.AddRange(data);

            // Build header and combine with body
            return BuildMessage(cmd, body.ToArray());
        }

        /// <summary>
        /// Builds a request message for downloading a file from a storage server.
        /// </summary>
        /// <param name="groupName">
        /// Group name containing the file.
        /// </param>
        /// <param name="remoteFilename">
        /// Remote filename on the storage server.
        /// </param>
        /// <param name="offset">
        /// Starting byte offset (0-based).
        /// </param>
        /// <param name="length">
        /// Number of bytes to download (0 means to end of file).
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildDownloadRequest(
            string groupName,
            string remoteFilename,
            long offset,
            long length)
        {
            // Build message body
            var body = new List<byte>();

            // Add group name (padded)
            var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
            var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
            body.AddRange(paddedGroupName);

            // Add remote filename (padded)
            var filenameBytes = Encoding.UTF8.GetBytes(remoteFilename);
            var paddedFilename = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(filenameBytes, paddedFilename, Math.Min(filenameBytes.Length, FastDFSConstants.RemoteFilenameMaxLength));
            body.AddRange(paddedFilename);

            // Add offset (8 bytes, big-endian)
            var offsetBytes = BitConverter.GetBytes(offset);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(offsetBytes);
            }
            body.AddRange(offsetBytes);

            // Add length (8 bytes, big-endian)
            var lengthBytes = BitConverter.GetBytes(length);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(lengthBytes);
            }
            body.AddRange(lengthBytes);

            // Build header and combine with body
            return BuildMessage(FastDFSConstants.StorageProtoCmdDownloadFile, body.ToArray());
        }

        /// <summary>
        /// Builds a request message for deleting a file from a storage server.
        /// </summary>
        /// <param name="groupName">
        /// Group name containing the file.
        /// </param>
        /// <param name="remoteFilename">
        /// Remote filename on the storage server.
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildDeleteRequest(string groupName, string remoteFilename)
        {
            // Build message body
            var body = new List<byte>();

            // Add group name (padded)
            var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
            var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
            body.AddRange(paddedGroupName);

            // Add remote filename (padded)
            var filenameBytes = Encoding.UTF8.GetBytes(remoteFilename);
            var paddedFilename = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(filenameBytes, paddedFilename, Math.Min(filenameBytes.Length, FastDFSConstants.RemoteFilenameMaxLength));
            body.AddRange(paddedFilename);

            // Build header and combine with body
            return BuildMessage(FastDFSConstants.StorageProtoCmdDeleteFile, body.ToArray());
        }

        /// <summary>
        /// Builds a request message for querying file information from a storage server.
        /// </summary>
        /// <param name="groupName">
        /// Group name containing the file.
        /// </param>
        /// <param name="remoteFilename">
        /// Remote filename on the storage server.
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildQueryFileInfoRequest(string groupName, string remoteFilename)
        {
            // Similar to BuildDeleteRequest but with different command code
            var body = new List<byte>();

            var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
            var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
            body.AddRange(paddedGroupName);

            var filenameBytes = Encoding.UTF8.GetBytes(remoteFilename);
            var paddedFilename = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(filenameBytes, paddedFilename, Math.Min(filenameBytes.Length, FastDFSConstants.RemoteFilenameMaxLength));
            body.AddRange(paddedFilename);

            return BuildMessage(FastDFSConstants.StorageProtoCmdQueryFileInfo, body.ToArray());
        }

        /// <summary>
        /// Builds a request message for setting metadata on a storage server.
        /// </summary>
        /// <param name="groupName">
        /// Group name containing the file.
        /// </param>
        /// <param name="remoteFilename">
        /// Remote filename on the storage server.
        /// </param>
        /// <param name="metadata">
        /// Metadata key-value pairs to set.
        /// </param>
        /// <param name="flag">
        /// Metadata operation flag (Overwrite or Merge).
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildSetMetadataRequest(
            string groupName,
            string remoteFilename,
            Dictionary<string, string> metadata,
            MetadataFlag flag)
        {
            // Build message body
            var body = new List<byte>();

            // Add group name (padded)
            var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
            var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
            body.AddRange(paddedGroupName);

            // Add remote filename (padded)
            var filenameBytes = Encoding.UTF8.GetBytes(remoteFilename);
            var paddedFilename = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(filenameBytes, paddedFilename, Math.Min(filenameBytes.Length, FastDFSConstants.RemoteFilenameMaxLength));
            body.AddRange(paddedFilename);

            // Add operation flag
            body.Add((byte)flag);

            // Add metadata
            var metadataBytes = EncodeMetadata(metadata);
            body.AddRange(metadataBytes);

            // Build header and combine with body
            return BuildMessage(FastDFSConstants.StorageProtoCmdSetMetadata, body.ToArray());
        }

        /// <summary>
        /// Builds a request message for getting metadata from a storage server.
        /// </summary>
        /// <param name="groupName">
        /// Group name containing the file.
        /// </param>
        /// <param name="remoteFilename">
        /// Remote filename on the storage server.
        /// </param>
        /// <returns>
        /// Byte array containing the protocol message.
        /// </returns>
        public static byte[] BuildGetMetadataRequest(string groupName, string remoteFilename)
        {
            // Similar to BuildDeleteRequest but with different command code
            var body = new List<byte>();

            var groupNameBytes = Encoding.UTF8.GetBytes(groupName);
            var paddedGroupName = new byte[FastDFSConstants.GroupNameMaxLength];
            Array.Copy(groupNameBytes, paddedGroupName, Math.Min(groupNameBytes.Length, FastDFSConstants.GroupNameMaxLength));
            body.AddRange(paddedGroupName);

            var filenameBytes = Encoding.UTF8.GetBytes(remoteFilename);
            var paddedFilename = new byte[FastDFSConstants.RemoteFilenameMaxLength];
            Array.Copy(filenameBytes, paddedFilename, Math.Min(filenameBytes.Length, FastDFSConstants.RemoteFilenameMaxLength));
            body.AddRange(paddedFilename);

            return BuildMessage(FastDFSConstants.StorageProtoCmdGetMetadata, body.ToArray());
        }

        // ====================================================================
        // Helper Methods
        // ====================================================================

        /// <summary>
        /// Builds a complete protocol message with header and body.
        /// </summary>
        /// <param name="cmd">
        /// Command code for the message.
        /// </param>
        /// <param name="body">
        /// Message body as byte array.
        /// </param>
        /// <returns>
        /// Complete protocol message with header and body.
        /// </returns>
        private static byte[] BuildMessage(byte cmd, byte[] body)
        {
            var message = new List<byte>();

            // Build header
            // Length (8 bytes, big-endian)
            var lengthBytes = BitConverter.GetBytes((long)body.Length);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(lengthBytes);
            }
            message.AddRange(lengthBytes);

            // Command code (1 byte)
            message.Add(cmd);

            // Status code (1 byte, 0 for requests)
            message.Add(0);

            // Add body
            message.AddRange(body);

            return message.ToArray();
        }

        /// <summary>
        /// Encodes metadata dictionary into FastDFS metadata format.
        /// 
        /// FastDFS metadata format uses special separator characters:
        /// - RecordSeparator (0x01) separates key-value pairs
        /// - FieldSeparator (0x02) separates keys from values
        /// </summary>
        /// <param name="metadata">
        /// Dictionary of metadata key-value pairs.
        /// </param>
        /// <returns>
        /// Encoded metadata as byte array.
        /// </returns>
        private static byte[] EncodeMetadata(Dictionary<string, string> metadata)
        {
            if (metadata == null || metadata.Count == 0)
            {
                return new byte[0];
            }

            var parts = new List<string>();
            foreach (var kvp in metadata)
            {
                var key = kvp.Key ?? "";
                var value = kvp.Value ?? "";
                parts.Add($"{key}{(char)FastDFSConstants.FieldSeparator}{value}");
            }

            var metadataString = string.Join($"{(char)FastDFSConstants.RecordSeparator}", parts);
            return Encoding.UTF8.GetBytes(metadataString);
        }
    }
}

