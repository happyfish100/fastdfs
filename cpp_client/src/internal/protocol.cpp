/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#include "internal/protocol.hpp"
#include "fastdfs/errors.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>

namespace fastdfs {
namespace internal {

std::vector<uint8_t> encode_header(int64_t length, uint8_t cmd, uint8_t status) {
    std::vector<uint8_t> header(FDFS_PROTO_HEADER_LEN);
    
    // Encode length as big-endian 64-bit integer
    for (int i = 7; i >= 0; --i) {
        header[i] = static_cast<uint8_t>(length & 0xFF);
        length >>= 8;
    }
    
    header[8] = cmd;
    header[9] = status;
    
    return header;
}

ProtocolHeader decode_header(const std::vector<uint8_t>& data) {
    if (data.size() < FDFS_PROTO_HEADER_LEN) {
        throw ProtocolException("Header too short");
    }
    
    ProtocolHeader header;
    
    // Decode length as big-endian 64-bit integer
    header.length = 0;
    for (int i = 0; i < 8; ++i) {
        header.length = (header.length << 8) | data[i];
    }
    
    header.cmd = data[8];
    header.status = data[9];
    
    return header;
}

void split_file_id(const std::string& file_id,
                   std::string& group_name,
                   std::string& remote_filename) {
    if (file_id.empty()) {
        throw InvalidArgumentException("File ID cannot be empty");
    }
    
    size_t pos = file_id.find('/');
    if (pos == std::string::npos || pos == 0) {
        throw InvalidArgumentException("Invalid file ID format: " + file_id);
    }
    
    group_name = file_id.substr(0, pos);
    remote_filename = file_id.substr(pos + 1);
    
    if (group_name.empty() || group_name.length() > FDFS_GROUP_NAME_MAX_LEN) {
        throw InvalidArgumentException("Invalid group name in file ID: " + file_id);
    }
    
    if (remote_filename.empty()) {
        throw InvalidArgumentException("Invalid remote filename in file ID: " + file_id);
    }
}

std::string join_file_id(const std::string& group_name,
                        const std::string& remote_filename) {
    return group_name + "/" + remote_filename;
}

std::vector<uint8_t> encode_metadata(const Metadata& metadata) {
    if (metadata.empty()) {
        return std::vector<uint8_t>();
    }
    
    std::vector<uint8_t> result;
    
    for (const auto& pair : metadata) {
        std::string key = pair.first;
        std::string value = pair.second;
        
        // Truncate if necessary
        if (key.length() > FDFS_MAX_META_NAME_LEN) {
            key = key.substr(0, FDFS_MAX_META_NAME_LEN);
        }
        if (value.length() > FDFS_MAX_META_VALUE_LEN) {
            value = value.substr(0, FDFS_MAX_META_VALUE_LEN);
        }
        
        // Append key + field separator + value + record separator
        result.insert(result.end(), key.begin(), key.end());
        result.push_back(FDFS_FIELD_SEPARATOR);
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(FDFS_RECORD_SEPARATOR);
    }
    
    return result;
}

Metadata decode_metadata(const std::vector<uint8_t>& data) {
    Metadata metadata;
    
    if (data.empty()) {
        return metadata;
    }
    
    std::string current;
    std::string key;
    bool in_key = true;
    
    for (uint8_t byte : data) {
        if (byte == FDFS_FIELD_SEPARATOR) {
            if (in_key) {
                key = current;
                current.clear();
                in_key = false;
            } else {
                // Invalid format, skip this record
                current.clear();
                in_key = true;
            }
        } else if (byte == FDFS_RECORD_SEPARATOR) {
            if (!in_key && !key.empty()) {
                metadata[key] = current;
            }
            current.clear();
            key.clear();
            in_key = true;
        } else {
            current += static_cast<char>(byte);
        }
    }
    
    // Handle last record if not terminated by separator
    if (!in_key && !key.empty() && !current.empty()) {
        metadata[key] = current;
    }
    
    return metadata;
}

} // namespace internal
} // namespace fastdfs

