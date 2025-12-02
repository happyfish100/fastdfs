/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#include "internal/operations.hpp"
#include "internal/protocol.hpp"
#include "internal/connection.hpp"
#include "fastdfs/errors.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>

namespace fastdfs {
namespace internal {

Operations::Operations(ConnectionPool& tracker_pool,
                      ConnectionPool& storage_pool,
                      std::chrono::milliseconds network_timeout,
                      int retry_count)
    : tracker_pool_(tracker_pool)
    , storage_pool_(storage_pool)
    , network_timeout_(network_timeout)
    , retry_count_(retry_count) {
}

static std::string pad_string(const std::string& str, size_t len) {
    std::string result = str;
    if (result.length() > len) {
        result = result.substr(0, len);
    }
    result.resize(len, '\0');
    return result;
}

static std::string unpad_string(const std::vector<uint8_t>& data) {
    std::string result;
    for (uint8_t byte : data) {
        if (byte == 0) {
            break;
        }
        result += static_cast<char>(byte);
    }
    return result;
}

static std::string get_file_ext_name(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos || pos == filename.length() - 1) {
        return "";
    }
    return filename.substr(pos + 1);
}

static std::vector<uint8_t> read_file_content(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw FileNotFoundException("Cannot open file: " + filename);
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw FileNotFoundException("Cannot read file: " + filename);
    }
    
    return buffer;
}

Operations::StorageServer Operations::query_storage_store(const std::string& group_name) {
    auto conn = tracker_pool_.acquire();
    try {
        uint8_t cmd;
        int64_t body_len;
        
        if (group_name.empty()) {
            cmd = static_cast<uint8_t>(TrackerCommand::SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE);
            body_len = 0;
        } else {
            cmd = static_cast<uint8_t>(TrackerCommand::SERVICE_QUERY_STORE_WITH_GROUP_ONE);
            body_len = FDFS_GROUP_NAME_MAX_LEN;
        }
        
        auto header = encode_header(body_len, cmd, 0);
        conn->send(header);
        
        if (!group_name.empty()) {
            auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
            conn->send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        }
        
        // Receive response
        auto resp_header_data = conn->recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Tracker returned error: " + std::to_string(resp_header.status));
        }
        
        if (resp_header.length < FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 8) {
            throw ProtocolException("Invalid response from tracker");
        }
        
        auto resp_body = conn->recv(resp_header.length);
        
        StorageServer server;
        server.group_name = unpad_string(std::vector<uint8_t>(
            resp_body.begin(), resp_body.begin() + FDFS_GROUP_NAME_MAX_LEN));
        
        std::string ip_addr(reinterpret_cast<const char*>(resp_body.data() + FDFS_GROUP_NAME_MAX_LEN),
                           IP_ADDRESS_SIZE);
        // Remove null terminators
        ip_addr = ip_addr.substr(0, ip_addr.find('\0'));
        server.ip_addr = ip_addr;
        
        // Port is stored as big-endian int64_t after IP address
        uint16_t port = 0;
        if (resp_body.size() >= FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 2) {
            port = (static_cast<uint16_t>(resp_body[FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE]) << 8) |
                   static_cast<uint16_t>(resp_body[FDFS_GROUP_NAME_MAX_LEN + IP_ADDRESS_SIZE + 1]);
        } else {
            port = STORAGE_DEFAULT_PORT;
        }
        server.port = port;
        
        tracker_pool_.release(conn);
        return server;
    } catch (...) {
        tracker_pool_.release(conn);
        throw;
    }
}

Operations::StorageServer Operations::query_storage_fetch(const std::string& file_id) {
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    auto conn = tracker_pool_.acquire();
    try {
        uint8_t cmd = static_cast<uint8_t>(TrackerCommand::SERVICE_QUERY_FETCH_ONE);
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length());
        
        auto header = encode_header(body_len, cmd, 0);
        conn->send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        conn->send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        conn->send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        // Receive response (similar to query_storage_store)
        auto resp_header_data = conn->recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Tracker returned error: " + std::to_string(resp_header.status));
        }
        
        auto resp_body = conn->recv(resp_header.length);
        
        StorageServer server;
        std::string ip_addr(reinterpret_cast<const char*>(resp_body.data()), IP_ADDRESS_SIZE);
        ip_addr = ip_addr.substr(0, ip_addr.find('\0'));
        server.ip_addr = ip_addr;
        
        uint16_t port = STORAGE_DEFAULT_PORT;
        if (resp_body.size() >= IP_ADDRESS_SIZE + 2) {
            port = (static_cast<uint16_t>(resp_body[IP_ADDRESS_SIZE]) << 8) |
                   static_cast<uint16_t>(resp_body[IP_ADDRESS_SIZE + 1]);
        }
        server.port = port;
        server.group_name = group_name;
        
        tracker_pool_.release(conn);
        return server;
    } catch (...) {
        tracker_pool_.release(conn);
        throw;
    }
}

Operations::StorageServer Operations::query_storage_update(const std::string& file_id) {
    // Similar to query_storage_fetch but uses SERVICE_QUERY_UPDATE command
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    auto conn = tracker_pool_.acquire();
    try {
        uint8_t cmd = static_cast<uint8_t>(TrackerCommand::SERVICE_QUERY_UPDATE);
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length());
        
        auto header = encode_header(body_len, cmd, 0);
        conn->send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        conn->send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        conn->send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        auto resp_header_data = conn->recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Tracker returned error: " + std::to_string(resp_header.status));
        }
        
        auto resp_body = conn->recv(resp_header.length);
        
        StorageServer server;
        std::string ip_addr(reinterpret_cast<const char*>(resp_body.data()), IP_ADDRESS_SIZE);
        ip_addr = ip_addr.substr(0, ip_addr.find('\0'));
        server.ip_addr = ip_addr;
        server.port = STORAGE_DEFAULT_PORT;
        server.group_name = group_name;
        
        tracker_pool_.release(conn);
        return server;
    } catch (...) {
        tracker_pool_.release(conn);
        throw;
    }
}

std::string Operations::upload_file(const std::string& local_filename,
                                  const Metadata* metadata,
                                  bool is_appender) {
    auto data = read_file_content(local_filename);
    auto ext_name = get_file_ext_name(local_filename);
    return upload_buffer(data, ext_name, metadata, is_appender);
}

std::string Operations::upload_buffer(const std::vector<uint8_t>& data,
                                     const std::string& file_ext_name,
                                     const Metadata* metadata,
                                     bool is_appender) {
    // Get storage server
    auto server = query_storage_store();
    
    // Get connection to storage
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    // Note: In a full implementation, we'd add this address to storage_pool_ dynamically
    // For now, we'll create a temporary connection
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = is_appender ?
            static_cast<uint8_t>(StorageCommand::UPLOAD_APPENDER_FILE) :
            static_cast<uint8_t>(StorageCommand::UPLOAD_FILE);
        
        auto ext_bytes = pad_string(file_ext_name, FDFS_FILE_EXT_NAME_MAX_LEN);
        int64_t body_len = 1 + FDFS_FILE_EXT_NAME_MAX_LEN + static_cast<int64_t>(data.size());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        // Store path index (0 for now)
        temp_conn.send({0});
        
        // File extension
        temp_conn.send(std::vector<uint8_t>(ext_bytes.begin(), ext_bytes.end()));
        
        // File data
        temp_conn.send(data);
        
        // Receive response
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
        
        auto resp_body = temp_conn.recv(resp_header.length);
        
        if (resp_body.size() < FDFS_GROUP_NAME_MAX_LEN) {
            throw ProtocolException("Invalid response from storage");
        }
        
        std::string group_name = unpad_string(std::vector<uint8_t>(
            resp_body.begin(), resp_body.begin() + FDFS_GROUP_NAME_MAX_LEN));
        std::string remote_filename(reinterpret_cast<const char*>(resp_body.data() + FDFS_GROUP_NAME_MAX_LEN),
                                   resp_body.size() - FDFS_GROUP_NAME_MAX_LEN);
        
        std::string file_id = join_file_id(group_name, remote_filename);
        
        // Set metadata if provided
        if (metadata && !metadata->empty()) {
            try {
                set_metadata(file_id, *metadata, MetadataFlag::OVERWRITE);
            } catch (...) {
                // Metadata setting failed, but file is uploaded
                // Return file_id anyway
            }
        }
        
        return file_id;
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

std::string Operations::upload_slave_file(const std::string& master_file_id,
                                         const std::string& prefix_name,
                                         const std::string& file_ext_name,
                                         const std::vector<uint8_t>& data,
                                         const Metadata* metadata) {
    std::string group_name, remote_filename;
    split_file_id(master_file_id, group_name, remote_filename);
    
    auto server = query_storage_fetch(master_file_id);
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::UPLOAD_SLAVE_FILE);
        
        auto prefix_bytes = pad_string(prefix_name, FDFS_FILE_PREFIX_MAX_LEN);
        auto ext_bytes = pad_string(file_ext_name, FDFS_FILE_EXT_NAME_MAX_LEN);
        auto master_bytes = pad_string(remote_filename, 128); // Master filename length
        
        int64_t body_len = FDFS_FILE_PREFIX_MAX_LEN + FDFS_FILE_EXT_NAME_MAX_LEN +
                          static_cast<int64_t>(master_bytes.length()) +
                          static_cast<int64_t>(data.size());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        temp_conn.send(std::vector<uint8_t>(prefix_bytes.begin(), prefix_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(ext_bytes.begin(), ext_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(master_bytes.begin(), master_bytes.end()));
        temp_conn.send(data);
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
        
        auto resp_body = temp_conn.recv(resp_header.length);
        
        std::string remote_filename_slave(reinterpret_cast<const char*>(resp_body.data()),
                                         resp_body.size());
        
        return join_file_id(group_name, remote_filename_slave);
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

std::vector<uint8_t> Operations::download_file(const std::string& file_id,
                                              int64_t offset,
                                              int64_t length) {
    auto server = query_storage_fetch(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::DOWNLOAD_FILE);
        
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length()) + 8 + 8;
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        // Send offset (big-endian int64_t)
        std::vector<uint8_t> offset_bytes(8);
        int64_t offset_val = offset;
        for (int i = 7; i >= 0; --i) {
            offset_bytes[i] = static_cast<uint8_t>(offset_val & 0xFF);
            offset_val >>= 8;
        }
        temp_conn.send(offset_bytes);
        
        // Send length (big-endian int64_t)
        std::vector<uint8_t> length_bytes(8);
        int64_t length_val = length;
        for (int i = 7; i >= 0; --i) {
            length_bytes[i] = static_cast<uint8_t>(length_val & 0xFF);
            length_val >>= 8;
        }
        temp_conn.send(length_bytes);
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            if (resp_header.status == 2) {
                throw FileNotFoundException("File not found: " + file_id);
            }
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
        
        if (resp_header.length <= 0) {
            return std::vector<uint8_t>();
        }
        
        return temp_conn.recv(resp_header.length);
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

void Operations::download_to_file(const std::string& file_id,
                                 const std::string& local_filename) {
    auto data = download_file(file_id, 0, 0);
    
    std::ofstream file(local_filename, std::ios::binary);
    if (!file.is_open()) {
        throw ConnectionException("Cannot create file: " + local_filename);
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void Operations::delete_file(const std::string& file_id) {
    auto server = query_storage_update(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::DELETE_FILE);
        
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            if (resp_header.status == 2) {
                throw FileNotFoundException("File not found: " + file_id);
            }
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

void Operations::append_file(const std::string& file_id,
                             const std::vector<uint8_t>& data) {
    auto server = query_storage_update(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::APPEND_FILE);
        
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length()) +
                          static_cast<int64_t>(data.size());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        temp_conn.send(data);
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

void Operations::modify_file(const std::string& file_id,
                            int64_t offset,
                            const std::vector<uint8_t>& data) {
    auto server = query_storage_update(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::MODIFY_FILE);
        
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length()) +
                          8 + static_cast<int64_t>(data.size());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        // Send offset
        std::vector<uint8_t> offset_bytes(8);
        int64_t offset_val = offset;
        for (int i = 7; i >= 0; --i) {
            offset_bytes[i] = static_cast<uint8_t>(offset_val & 0xFF);
            offset_val >>= 8;
        }
        temp_conn.send(offset_bytes);
        temp_conn.send(data);
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

void Operations::truncate_file(const std::string& file_id, int64_t size) {
    auto server = query_storage_update(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::TRUNCATE_FILE);
        
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length()) + 8;
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        // Send size
        std::vector<uint8_t> size_bytes(8);
        int64_t size_val = size;
        for (int i = 7; i >= 0; --i) {
            size_bytes[i] = static_cast<uint8_t>(size_val & 0xFF);
            size_val >>= 8;
        }
        temp_conn.send(size_bytes);
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

void Operations::set_metadata(const std::string& file_id,
                              const Metadata& metadata,
                              MetadataFlag flag) {
    auto server = query_storage_update(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::SET_METADATA);
        
        auto meta_data = encode_metadata(metadata);
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length()) +
                          1 + static_cast<int64_t>(meta_data.size());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        temp_conn.send({static_cast<uint8_t>(flag)});
        temp_conn.send(meta_data);
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

Metadata Operations::get_metadata(const std::string& file_id) {
    auto server = query_storage_fetch(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::GET_METADATA);
        
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            if (resp_header.status == 2) {
                throw FileNotFoundException("File not found: " + file_id);
            }
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
        
        if (resp_header.length <= 0) {
            return Metadata();
        }
        
        auto resp_body = temp_conn.recv(resp_header.length);
        return decode_metadata(resp_body);
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

FileInfo Operations::get_file_info(const std::string& file_id) {
    auto server = query_storage_fetch(file_id);
    std::string group_name, remote_filename;
    split_file_id(file_id, group_name, remote_filename);
    
    std::string storage_addr = server.ip_addr + ":" + std::to_string(server.port);
    Connection temp_conn(storage_addr, std::chrono::milliseconds(5000));
    temp_conn.connect();
    
    try {
        uint8_t cmd = static_cast<uint8_t>(StorageCommand::QUERY_FILE_INFO);
        
        int64_t body_len = FDFS_GROUP_NAME_MAX_LEN + static_cast<int64_t>(remote_filename.length());
        
        auto header = encode_header(body_len, cmd, 0);
        temp_conn.send(header);
        
        auto group_bytes = pad_string(group_name, FDFS_GROUP_NAME_MAX_LEN);
        temp_conn.send(std::vector<uint8_t>(group_bytes.begin(), group_bytes.end()));
        temp_conn.send(std::vector<uint8_t>(remote_filename.begin(), remote_filename.end()));
        
        auto resp_header_data = temp_conn.recv(FDFS_PROTO_HEADER_LEN);
        auto resp_header = decode_header(resp_header_data);
        
        if (resp_header.status != 0) {
            if (resp_header.status == 2) {
                throw FileNotFoundException("File not found: " + file_id);
            }
            throw ProtocolException("Storage returned error: " + std::to_string(resp_header.status));
        }
        
        if (resp_header.length < 40) { // Minimum size for file info
            throw ProtocolException("Invalid response from storage");
        }
        
        auto resp_body = temp_conn.recv(resp_header.length);
        
        FileInfo info;
        info.group_name = group_name;
        info.remote_filename = remote_filename;
        
        // Parse file info (simplified - actual format may vary)
        // File size (8 bytes, big-endian)
        info.file_size = 0;
        for (int i = 0; i < 8 && i < static_cast<int>(resp_body.size()); ++i) {
            info.file_size = (info.file_size << 8) | resp_body[i];
        }
        
        // Create time (8 bytes, big-endian)
        info.create_time = 0;
        for (int i = 0; i < 8 && i < static_cast<int>(resp_body.size()) - 8; ++i) {
            info.create_time = (info.create_time << 8) | resp_body[8 + i];
        }
        
        // CRC32 (4 bytes, big-endian)
        info.crc32 = 0;
        for (int i = 0; i < 4 && i < static_cast<int>(resp_body.size()) - 16; ++i) {
            info.crc32 = (info.crc32 << 8) | resp_body[16 + i];
        }
        
        info.source_ip_addr = server.ip_addr;
        info.storage_id = "";
        
        return info;
    } catch (...) {
        temp_conn.close();
        throw;
    }
}

} // namespace internal
} // namespace fastdfs

