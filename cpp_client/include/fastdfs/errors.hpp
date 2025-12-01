/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#ifndef FASTDFS_ERRORS_HPP
#define FASTDFS_ERRORS_HPP

#include <stdexcept>
#include <string>

namespace fastdfs {

/**
 * Base exception class for FastDFS errors
 */
class FastDFSException : public std::runtime_error {
public:
    explicit FastDFSException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * File not found error
 */
class FileNotFoundException : public FastDFSException {
public:
    explicit FileNotFoundException(const std::string& message)
        : FastDFSException("File not found: " + message) {}
};

/**
 * Connection error
 */
class ConnectionException : public FastDFSException {
public:
    explicit ConnectionException(const std::string& message)
        : FastDFSException("Connection error: " + message) {}
};

/**
 * Timeout error
 */
class TimeoutException : public FastDFSException {
public:
    explicit TimeoutException(const std::string& message)
        : FastDFSException("Timeout: " + message) {}
};

/**
 * Invalid argument error
 */
class InvalidArgumentException : public FastDFSException {
public:
    explicit InvalidArgumentException(const std::string& message)
        : FastDFSException("Invalid argument: " + message) {}
};

/**
 * Protocol error
 */
class ProtocolException : public FastDFSException {
public:
    explicit ProtocolException(const std::string& message)
        : FastDFSException("Protocol error: " + message) {}
};

/**
 * No storage server available
 */
class NoStorageServerException : public FastDFSException {
public:
    explicit NoStorageServerException(const std::string& message)
        : FastDFSException("No storage server available: " + message) {}
};

/**
 * Client closed error
 */
class ClientClosedException : public FastDFSException {
public:
    ClientClosedException()
        : FastDFSException("Client is closed") {}
};

} // namespace fastdfs

#endif // FASTDFS_ERRORS_HPP

