/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#ifndef FASTDFS_INTERNAL_CONNECTION_POOL_HPP
#define FASTDFS_INTERNAL_CONNECTION_POOL_HPP

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <chrono>

namespace fastdfs {
namespace internal {

class Connection;

/**
 * Connection pool for managing TCP connections to FastDFS servers
 */
class ConnectionPool {
public:
    ConnectionPool(const std::vector<std::string>& addresses,
                  int max_conns,
                  std::chrono::milliseconds connect_timeout,
                  std::chrono::milliseconds idle_timeout);

    ~ConnectionPool();

    // Non-copyable
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    // Movable
    ConnectionPool(ConnectionPool&&) noexcept;
    ConnectionPool& operator=(ConnectionPool&&) noexcept;

    /**
     * Gets a connection from the pool or creates a new one
     */
    std::shared_ptr<Connection> acquire();

    /**
     * Returns a connection to the pool
     */
    void release(std::shared_ptr<Connection> conn);

    /**
     * Closes all connections and clears the pool
     */
    void close();

private:
    std::vector<std::string> addresses_;
    int max_conns_;
    std::chrono::milliseconds connect_timeout_;
    std::chrono::milliseconds idle_timeout_;
    
    std::mutex mutex_;
    std::queue<std::shared_ptr<Connection>> available_;
    std::vector<std::shared_ptr<Connection>> all_connections_;
    bool closed_;
};

} // namespace internal
} // namespace fastdfs

#endif // FASTDFS_INTERNAL_CONNECTION_POOL_HPP

