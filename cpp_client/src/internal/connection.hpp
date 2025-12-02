/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#ifndef FASTDFS_INTERNAL_CONNECTION_HPP
#define FASTDFS_INTERNAL_CONNECTION_HPP

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace fastdfs {
namespace internal {

/**
 * TCP connection to a FastDFS server
 */
class Connection {
public:
    Connection(const std::string& address,
              std::chrono::milliseconds connect_timeout);
    
    ~Connection();

    // Non-copyable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Movable
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    /**
     * Connects to the server
     */
    void connect();

    /**
     * Closes the connection
     */
    void close();

    /**
     * Checks if connection is open
     */
    bool is_open() const;

    /**
     * Sends data
     */
    void send(const std::vector<uint8_t>& data);

    /**
     * Receives exactly n bytes
     */
    std::vector<uint8_t> recv(size_t n);

    /**
     * Gets the server address
     */
    const std::string& address() const { return address_; }

    /**
     * Gets the last used time
     */
    std::chrono::steady_clock::time_point last_used() const { return last_used_; }

    /**
     * Updates the last used time
     */
    void update_last_used();

private:
    std::string address_;
    std::chrono::milliseconds connect_timeout_;
    int socket_fd_;
    std::chrono::steady_clock::time_point last_used_;
    bool connected_;

    void parse_address(const std::string& address, std::string& host, uint16_t& port);
};

} // namespace internal
} // namespace fastdfs

#endif // FASTDFS_INTERNAL_CONNECTION_HPP

