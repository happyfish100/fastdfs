/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#ifndef FASTDFS_INTERNAL_OPERATIONS_HPP
#define FASTDFS_INTERNAL_OPERATIONS_HPP

#include "fastdfs/types.hpp"
#include "connection_pool.hpp"
#include <string>
#include <vector>
#include <chrono>

namespace fastdfs {
namespace internal {

/**
 * Low-level operations for FastDFS protocol
 */
class Operations {
public:
    Operations(ConnectionPool& tracker_pool,
              ConnectionPool& storage_pool,
              std::chrono::milliseconds network_timeout,
              int retry_count);

    std::string upload_file(const std::string& local_filename,
                           const Metadata* metadata,
                           bool is_appender);

    std::string upload_buffer(const std::vector<uint8_t>& data,
                             const std::string& file_ext_name,
                             const Metadata* metadata,
                             bool is_appender);

    std::string upload_slave_file(const std::string& master_file_id,
                                 const std::string& prefix_name,
                                 const std::string& file_ext_name,
                                 const std::vector<uint8_t>& data,
                                 const Metadata* metadata);

    std::vector<uint8_t> download_file(const std::string& file_id,
                                      int64_t offset,
                                      int64_t length);

    void download_to_file(const std::string& file_id,
                         const std::string& local_filename);

    void delete_file(const std::string& file_id);

    void append_file(const std::string& file_id,
                    const std::vector<uint8_t>& data);

    void modify_file(const std::string& file_id,
                    int64_t offset,
                    const std::vector<uint8_t>& data);

    void truncate_file(const std::string& file_id, int64_t size);

    void set_metadata(const std::string& file_id,
                     const Metadata& metadata,
                     MetadataFlag flag);

    Metadata get_metadata(const std::string& file_id);

    FileInfo get_file_info(const std::string& file_id);

private:
    ConnectionPool& tracker_pool_;
    ConnectionPool& storage_pool_;
    std::chrono::milliseconds network_timeout_;
    int retry_count_;

    struct StorageServer {
        std::string group_name;
        std::string ip_addr;
        uint16_t port;
    };

    StorageServer query_storage_store(const std::string& group_name = "");
    StorageServer query_storage_fetch(const std::string& file_id);
    StorageServer query_storage_update(const std::string& file_id);
};

} // namespace internal
} // namespace fastdfs

#endif // FASTDFS_INTERNAL_OPERATIONS_HPP

