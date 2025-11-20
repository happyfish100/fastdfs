// ============================================================================
// FastDFS Exception Classes
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This file defines all exception classes used by the FastDFS client.
// These exceptions provide detailed error information for different types
// of failures that can occur during FastDFS operations.
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
    /// Base exception class for all FastDFS-related errors.
    /// 
    /// This exception is thrown when a FastDFS operation fails due to
    /// protocol errors, server errors, or other FastDFS-specific issues.
    /// It provides a base class for more specific exception types and
    /// includes error code and message information from the server.
    /// </summary>
    public class FastDFSException : Exception
    {
        /// <summary>
        /// Gets the error code returned by the FastDFS server, if available.
        /// Error codes are defined by the FastDFS protocol and indicate
        /// the specific type of error that occurred.
        /// </summary>
        public byte? ErrorCode { get; }

        /// <summary>
        /// Initializes a new instance of the FastDFSException class with
        /// a default error message.
        /// </summary>
        public FastDFSException()
            : base("A FastDFS operation failed.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSException class with
        /// a specified error message.
        /// </summary>
        /// <param name="message">
        /// The error message that explains the reason for the exception.
        /// </param>
        public FastDFSException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSException class with
        /// a specified error message and a reference to the inner exception
        /// that is the cause of this exception.
        /// </summary>
        /// <param name="message">
        /// The error message that explains the reason for the exception.
        /// </param>
        /// <param name="innerException">
        /// The exception that is the cause of the current exception, or
        /// null if no inner exception is specified.
        /// </param>
        public FastDFSException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSException class with
        /// a specified error message, error code, and inner exception.
        /// </summary>
        /// <param name="message">
        /// The error message that explains the reason for the exception.
        /// </param>
        /// <param name="errorCode">
        /// The error code returned by the FastDFS server.
        /// </param>
        /// <param name="innerException">
        /// The exception that is the cause of the current exception, or
        /// null if no inner exception is specified.
        /// </param>
        public FastDFSException(string message, byte errorCode, Exception innerException = null)
            : base(message, innerException)
        {
            ErrorCode = errorCode;
        }
    }

    /// <summary>
    /// Exception thrown when a FastDFS protocol error occurs.
    /// 
    /// This exception is thrown when communication with FastDFS servers
    /// fails due to protocol violations, invalid message formats, or
    /// unexpected protocol responses. It typically indicates a bug in
    /// the client implementation or incompatibility with the server version.
    /// </summary>
    public class FastDFSProtocolException : FastDFSException
    {
        /// <summary>
        /// Initializes a new instance of the FastDFSProtocolException class
        /// with a default error message.
        /// </summary>
        public FastDFSProtocolException()
            : base("A FastDFS protocol error occurred.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSProtocolException class
        /// with a specified error message.
        /// </summary>
        /// <param name="message">
        /// The error message that explains the reason for the exception.
        /// </param>
        public FastDFSProtocolException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSProtocolException class
        /// with a specified error message and a reference to the inner exception.
        /// </summary>
        /// <param name="message">
        /// The error message that explains the reason for the exception.
        /// </param>
        /// <param name="innerException">
        /// The exception that is the cause of the current exception, or
        /// null if no inner exception is specified.
        /// </param>
        public FastDFSProtocolException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }

    /// <summary>
    /// Exception thrown when a network error occurs during FastDFS operations.
    /// 
    /// This exception is thrown when network communication fails, such as
    /// connection timeouts, connection refused, network unreachable, or
    /// other network-related errors. It wraps the underlying network exception
    /// and provides FastDFS-specific context.
    /// </summary>
    public class FastDFSNetworkException : FastDFSException
    {
        /// <summary>
        /// Gets the network operation that failed (e.g., "connect", "read", "write").
        /// </summary>
        public string Operation { get; }

        /// <summary>
        /// Gets the server address where the network error occurred.
        /// </summary>
        public string Address { get; }

        /// <summary>
        /// Initializes a new instance of the FastDFSNetworkException class
        /// with operation and address information.
        /// </summary>
        /// <param name="operation">
        /// The network operation that failed (e.g., "connect", "read", "write").
        /// </param>
        /// <param name="address">
        /// The server address where the network error occurred.
        /// </param>
        /// <param name="innerException">
        /// The underlying network exception that caused this error.
        /// </param>
        public FastDFSNetworkException(string operation, string address, Exception innerException)
            : base($"Network error during {operation} to {address}: {innerException?.Message}", innerException)
        {
            Operation = operation;
            Address = address;
        }
    }

    /// <summary>
    /// Exception thrown when a file is not found in FastDFS storage.
    /// 
    /// This exception is thrown when attempting to download, delete, or
    /// query information about a file that does not exist in the FastDFS cluster.
    /// It indicates that the file ID is invalid or the file has been deleted.
    /// </summary>
    public class FastDFSFileNotFoundException : FastDFSException
    {
        /// <summary>
        /// Gets the file ID that was not found.
        /// </summary>
        public string FileId { get; }

        /// <summary>
        /// Initializes a new instance of the FastDFSFileNotFoundException class
        /// with the file ID that was not found.
        /// </summary>
        /// <param name="fileId">
        /// The file ID that was not found.
        /// </param>
        public FastDFSFileNotFoundException(string fileId)
            : base($"File not found: {fileId}")
        {
            FileId = fileId;
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSFileNotFoundException class
        /// with the file ID and a reference to the inner exception.
        /// </summary>
        /// <param name="fileId">
        /// The file ID that was not found.
        /// </param>
        /// <param name="innerException">
        /// The exception that is the cause of the current exception, or
        /// null if no inner exception is specified.
        /// </param>
        public FastDFSFileNotFoundException(string fileId, Exception innerException)
            : base($"File not found: {fileId}", innerException)
        {
            FileId = fileId;
        }
    }

    /// <summary>
    /// Exception thrown when no storage server is available for an operation.
    /// 
    /// This exception is thrown when the tracker cannot provide a storage
    /// server for the requested operation. This can occur when all storage
    /// servers are offline, overloaded, or when no storage servers exist
    /// in the specified group.
    /// </summary>
    public class FastDFSNoStorageServerException : FastDFSException
    {
        /// <summary>
        /// Gets the group name that was requested, if any.
        /// </summary>
        public string GroupName { get; }

        /// <summary>
        /// Initializes a new instance of the FastDFSNoStorageServerException class
        /// with a default error message.
        /// </summary>
        public FastDFSNoStorageServerException()
            : base("No storage server is available for the requested operation.")
        {
        }

        /// <summary>
        /// Initializes a new instance of the FastDFSNoStorageServerException class
        /// with a specified group name.
        /// </summary>
        /// <param name="groupName">
        /// The group name that was requested, or null if no specific group was requested.
        /// </param>
        public FastDFSNoStorageServerException(string groupName)
            : base($"No storage server is available in group: {groupName ?? "(any)"}")
        {
            GroupName = groupName;
        }
    }

    /// <summary>
    /// Exception thrown when a connection timeout occurs.
    /// 
    /// This exception is thrown when attempting to establish a connection
    /// to a FastDFS server exceeds the configured connection timeout.
    /// It indicates that the server is not responding or is unreachable.
    /// </summary>
    public class FastDFSConnectionTimeoutException : FastDFSNetworkException
    {
        /// <summary>
        /// Gets the timeout duration that was exceeded.
        /// </summary>
        public TimeSpan Timeout { get; }

        /// <summary>
        /// Initializes a new instance of the FastDFSConnectionTimeoutException class
        /// with timeout and address information.
        /// </summary>
        /// <param name="address">
        /// The server address where the timeout occurred.
        /// </param>
        /// <param name="timeout">
        /// The timeout duration that was exceeded.
        /// </param>
        public FastDFSConnectionTimeoutException(string address, TimeSpan timeout)
            : base("connect", address, new TimeoutException($"Connection timeout after {timeout.TotalSeconds} seconds"))
        {
            Timeout = timeout;
        }
    }

    /// <summary>
    /// Exception thrown when a network I/O timeout occurs.
    /// 
    /// This exception is thrown when a read or write operation on an
    /// established connection exceeds the configured network timeout.
    /// It indicates that the server is not responding to requests.
    /// </summary>
    public class FastDFSNetworkTimeoutException : FastDFSNetworkException
    {
        /// <summary>
        /// Gets the timeout duration that was exceeded.
        /// </summary>
        public TimeSpan Timeout { get; }

        /// <summary>
        /// Initializes a new instance of the FastDFSNetworkTimeoutException class
        /// with operation, address, and timeout information.
        /// </summary>
        /// <param name="operation">
        /// The network operation that timed out (e.g., "read", "write").
        /// </param>
        /// <param name="address">
        /// The server address where the timeout occurred.
        /// </param>
        /// <param name="timeout">
        /// The timeout duration that was exceeded.
        /// </param>
        public FastDFSNetworkTimeoutException(string operation, string address, TimeSpan timeout)
            : base(operation, address, new TimeoutException($"{operation} timeout after {timeout.TotalSeconds} seconds"))
        {
            Timeout = timeout;
        }
    }
}

