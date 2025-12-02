/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#include "internal/connection_pool.hpp"
#include "internal/connection.hpp"
#include <algorithm>
#include <random>

namespace fastdfs {
namespace internal {

ConnectionPool::ConnectionPool(const std::vector<std::string>& addresses,
                              int max_conns,
                              std::chrono::milliseconds connect_timeout,
                              std::chrono::milliseconds idle_timeout)
    : addresses_(addresses)
    , max_conns_(max_conns)
    , connect_timeout_(connect_timeout)
    , idle_timeout_(idle_timeout)
    , closed_(false) {
}

ConnectionPool::~ConnectionPool() {
    close();
}

ConnectionPool::ConnectionPool(ConnectionPool&& other) noexcept
    : addresses_(std::move(other.addresses_))
    , max_conns_(other.max_conns_)
    , connect_timeout_(other.connect_timeout_)
    , idle_timeout_(other.idle_timeout_)
    , available_(std::move(other.available_))
    , all_connections_(std::move(other.all_connections_))
    , closed_(other.closed_) {
    other.closed_ = true;
}

ConnectionPool& ConnectionPool::operator=(ConnectionPool&& other) noexcept {
    if (this != &other) {
        close();
        addresses_ = std::move(other.addresses_);
        max_conns_ = other.max_conns_;
        connect_timeout_ = other.connect_timeout_;
        idle_timeout_ = other.idle_timeout_;
        available_ = std::move(other.available_);
        all_connections_ = std::move(other.all_connections_);
        closed_ = other.closed_;
        other.closed_ = true;
    }
    return *this;
}

std::shared_ptr<Connection> ConnectionPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (closed_) {
        throw ConnectionException("Connection pool is closed");
    }
    
    // Try to get an available connection
    while (!available_.empty()) {
        auto conn = available_.front();
        available_.pop();
        
        // Check if connection is still valid and not idle too long
        auto now = std::chrono::steady_clock::now();
        auto idle_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - conn->last_used());
        
        if (conn->is_open() && idle_duration < idle_timeout_) {
            conn->update_last_used();
            return conn;
        }
    }
    
    // Create new connection if under limit
    if (static_cast<int>(all_connections_.size()) < max_conns_) {
        // Select random address if multiple available
        std::string address;
        if (addresses_.empty()) {
            throw ConnectionException("No addresses available");
        } else if (addresses_.size() == 1) {
            address = addresses_[0];
        } else {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, addresses_.size() - 1);
            address = addresses_[dis(gen)];
        }
        
        auto conn = std::make_shared<Connection>(address, connect_timeout_);
        conn->connect();
        all_connections_.push_back(conn);
        return conn;
    }
    
    // Pool is full, reuse oldest connection
    if (!all_connections_.empty()) {
        auto conn = all_connections_[0];
        if (!conn->is_open()) {
            conn->connect();
        }
        conn->update_last_used();
        return conn;
    }
    
    throw ConnectionException("Failed to acquire connection");
}

void ConnectionPool::release(std::shared_ptr<Connection> conn) {
    if (!conn) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (closed_) {
        return;
    }
    
    // Check if connection is still valid
    if (conn->is_open()) {
        conn->update_last_used();
        available_.push(conn);
    }
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (closed_) {
        return;
    }
    
    closed_ = true;
    
    // Close all connections
    while (!available_.empty()) {
        available_.pop();
    }
    
    for (auto& conn : all_connections_) {
        if (conn) {
            conn->close();
        }
    }
    
    all_connections_.clear();
}

} // namespace internal
} // namespace fastdfs

