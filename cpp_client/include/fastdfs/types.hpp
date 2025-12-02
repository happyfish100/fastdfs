/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#ifndef FASTDFS_TYPES_HPP
#define FASTDFS_TYPES_HPP

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <cstdint>

namespace fastdfs {

// Default network ports for FastDFS servers
constexpr uint16_t TRACKER_DEFAULT_PORT = 22122;
constexpr uint16_t STORAGE_DEFAULT_PORT = 23000;

// Protocol header size
constexpr size_t FDFS_PROTO_HEADER_LEN = 10;

// Field size limits
constexpr size_t FDFS_GROUP_NAME_MAX_LEN = 16;
constexpr size_t FDFS_FILE_EXT_NAME_MAX_LEN = 6;
constexpr size_t FDFS_MAX_META_NAME_LEN = 64;
constexpr size_t FDFS_MAX_META_VALUE_LEN = 256;
constexpr size_t FDFS_FILE_PREFIX_MAX_LEN = 16;
constexpr size_t FDFS_STORAGE_ID_MAX_SIZE = 16;
constexpr size_t FDFS_VERSION_SIZE = 8;
constexpr size_t IP_ADDRESS_SIZE = 16;

// Protocol separators
constexpr uint8_t FDFS_RECORD_SEPARATOR = 0x01;
constexpr uint8_t FDFS_FIELD_SEPARATOR = 0x02;

// Tracker protocol commands
enum class TrackerCommand : uint8_t {
    SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE = 101,
    SERVICE_QUERY_FETCH_ONE = 102,
    SERVICE_QUERY_UPDATE = 103,
    SERVICE_QUERY_STORE_WITH_GROUP_ONE = 104,
    SERVICE_QUERY_FETCH_ALL = 105,
    SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL = 106,
    SERVICE_QUERY_STORE_WITH_GROUP_ALL = 107,
    SERVER_LIST_ONE_GROUP = 90,
    SERVER_LIST_ALL_GROUPS = 91,
    SERVER_LIST_STORAGE = 92,
    SERVER_DELETE_STORAGE = 93,
    STORAGE_REPORT_IP_CHANGED = 94,
    STORAGE_REPORT_STATUS = 95,
    STORAGE_REPORT_DISK_USAGE = 96,
    STORAGE_SYNC_TIMESTAMP = 97,
    STORAGE_SYNC_REPORT = 98
};

// Storage protocol commands
enum class StorageCommand : uint8_t {
    UPLOAD_FILE = 11,
    DELETE_FILE = 12,
    SET_METADATA = 13,
    DOWNLOAD_FILE = 14,
    GET_METADATA = 15,
    UPLOAD_SLAVE_FILE = 21,
    QUERY_FILE_INFO = 22,
    UPLOAD_APPENDER_FILE = 23,
    APPEND_FILE = 24,
    MODIFY_FILE = 34,
    TRUNCATE_FILE = 36
};

// Metadata operation flags
enum class MetadataFlag : uint8_t {
    OVERWRITE = 'O',  // Replace all existing metadata
    MERGE = 'M'       // Merge with existing metadata
};

// File information structure
struct FileInfo {
    std::string group_name;
    std::string remote_filename;
    int64_t file_size;
    int64_t create_time;
    uint32_t crc32;
    std::string source_ip_addr;
    std::string storage_id;
};

// Client configuration
struct ClientConfig {
    std::vector<std::string> tracker_addrs;
    int max_conns = 10;
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds network_timeout{30000};
    std::chrono::milliseconds idle_timeout{60000};
    bool enable_pool = true;
    int retry_count = 3;
};

// Metadata type alias
using Metadata = std::map<std::string, std::string>;

} // namespace fastdfs

#endif // FASTDFS_TYPES_HPP

