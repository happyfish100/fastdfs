/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#include "internal/connection.hpp"
#include "fastdfs/errors.hpp"
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace fastdfs {
namespace internal {

Connection::Connection(const std::string& address,
                      std::chrono::milliseconds connect_timeout)
    : address_(address)
    , connect_timeout_(connect_timeout)
    , socket_fd_(-1)
    , last_used_(std::chrono::steady_clock::now())
    , connected_(false) {
}

Connection::~Connection() {
    close();
}

Connection::Connection(Connection&& other) noexcept
    : address_(std::move(other.address_))
    , connect_timeout_(other.connect_timeout_)
    , socket_fd_(other.socket_fd_)
    , last_used_(other.last_used_)
    , connected_(other.connected_) {
    other.socket_fd_ = -1;
    other.connected_ = false;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        address_ = std::move(other.address_);
        connect_timeout_ = other.connect_timeout_;
        socket_fd_ = other.socket_fd_;
        last_used_ = other.last_used_;
        connected_ = other.connected_;
        other.socket_fd_ = -1;
        other.connected_ = false;
    }
    return *this;
}

void Connection::parse_address(const std::string& address,
                               std::string& host,
                               uint16_t& port) {
    size_t pos = address.find(':');
    if (pos == std::string::npos) {
        throw InvalidArgumentException("Invalid address format: " + address);
    }
    
    host = address.substr(0, pos);
    std::string port_str = address.substr(pos + 1);
    
    try {
        port = static_cast<uint16_t>(std::stoul(port_str));
    } catch (...) {
        throw InvalidArgumentException("Invalid port in address: " + address);
    }
}

void Connection::connect() {
    if (connected_ && is_open()) {
        update_last_used();
        return;
    }
    
    close();
    
    std::string host;
    uint16_t port;
    parse_address(address_, host, port);
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw ConnectionException("WSAStartup failed");
    }
    
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == INVALID_SOCKET) {
        WSACleanup();
        throw ConnectionException("Failed to create socket");
    }
    
    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(socket_fd_, FIONBIO, &mode);
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        // Try DNS resolution
        hostent* he = gethostbyname(host.c_str());
        if (he == nullptr) {
            closesocket(socket_fd_);
            WSACleanup();
            throw ConnectionException("Failed to resolve host: " + host);
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Connect with timeout
    int result = ::connect(socket_fd_, (sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            closesocket(socket_fd_);
            WSACleanup();
            throw ConnectionException("Failed to connect: " + std::to_string(error));
        }
        
        // Wait for connection with timeout
        fd_set write_set;
        FD_ZERO(&write_set);
        FD_SET(socket_fd_, &write_set);
        
        timeval timeout;
        timeout.tv_sec = connect_timeout_.count() / 1000;
        timeout.tv_usec = (connect_timeout_.count() % 1000) * 1000;
        
        result = select(0, nullptr, &write_set, nullptr, &timeout);
        if (result <= 0) {
            closesocket(socket_fd_);
            WSACleanup();
            throw TimeoutException("Connection timeout");
        }
    }
    
    // Set blocking mode
    mode = 0;
    ioctlsocket(socket_fd_, FIONBIO, &mode);
    
    connected_ = true;
    update_last_used();
#else
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        throw ConnectionException("Failed to create socket");
    }
    
    // Set non-blocking mode
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        // Try DNS resolution
        hostent* he = gethostbyname(host.c_str());
        if (he == nullptr) {
            ::close(socket_fd_);
            throw ConnectionException("Failed to resolve host: " + host);
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Connect with timeout
    int result = ::connect(socket_fd_, (sockaddr*)&addr, sizeof(addr));
    if (result < 0) {
        if (errno != EINPROGRESS) {
            ::close(socket_fd_);
            throw ConnectionException("Failed to connect");
        }
        
        // Wait for connection with timeout
        fd_set write_set;
        FD_ZERO(&write_set);
        FD_SET(socket_fd_, &write_set);
        
        timeval timeout;
        timeout.tv_sec = connect_timeout_.count() / 1000;
        timeout.tv_usec = (connect_timeout_.count() % 1000) * 1000;
        
        result = select(socket_fd_ + 1, nullptr, &write_set, nullptr, &timeout);
        if (result <= 0) {
            ::close(socket_fd_);
            throw TimeoutException("Connection timeout");
        }
    }
    
    // Set blocking mode
    fcntl(socket_fd_, F_SETFL, flags);
    
    connected_ = true;
    update_last_used();
#endif
}

void Connection::close() {
    if (socket_fd_ >= 0) {
#ifdef _WIN32
        closesocket(socket_fd_);
        WSACleanup();
#else
        ::close(socket_fd_);
#endif
        socket_fd_ = -1;
    }
    connected_ = false;
}

bool Connection::is_open() const {
    return socket_fd_ >= 0 && connected_;
}

void Connection::send(const std::vector<uint8_t>& data) {
    if (!is_open()) {
        throw ConnectionException("Connection is not open");
    }
    
    size_t total_sent = 0;
    while (total_sent < data.size()) {
#ifdef _WIN32
        int sent = ::send(socket_fd_,
                         reinterpret_cast<const char*>(data.data() + total_sent),
                         static_cast<int>(data.size() - total_sent),
                         0);
#else
        ssize_t sent = ::send(socket_fd_,
                             data.data() + total_sent,
                             data.size() - total_sent,
                             0);
#endif
        if (sent <= 0) {
            throw ConnectionException("Failed to send data");
        }
        total_sent += sent;
    }
}

std::vector<uint8_t> Connection::recv(size_t n) {
    if (!is_open()) {
        throw ConnectionException("Connection is not open");
    }
    
    std::vector<uint8_t> data(n);
    size_t total_received = 0;
    
    while (total_received < n) {
#ifdef _WIN32
        int received = ::recv(socket_fd_,
                              reinterpret_cast<char*>(data.data() + total_received),
                              static_cast<int>(n - total_received),
                              0);
#else
        ssize_t received = ::recv(socket_fd_,
                                 data.data() + total_received,
                                 n - total_received,
                                 0);
#endif
        if (received <= 0) {
            throw ConnectionException("Failed to receive data");
        }
        total_received += received;
    }
    
    return data;
}

void Connection::update_last_used() {
    last_used_ = std::chrono::steady_clock::now();
}

} // namespace internal
} // namespace fastdfs

