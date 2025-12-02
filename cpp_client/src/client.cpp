/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#include "fastdfs/client.hpp"
#include "internal/connection_pool.hpp"
#include "internal/protocol.hpp"
#include "internal/operations.hpp"
#include <mutex>
#include <atomic>

namespace fastdfs {

class ClientImpl {
public:
    ClientImpl(const ClientConfig& config)
        : config_(config)
        , tracker_pool_(config.tracker_addrs, config.max_conns,
                       config.connect_timeout, config.idle_timeout)
        , storage_pool_(std::vector<std::string>{}, config.max_conns,
                       config.connect_timeout, config.idle_timeout)
        , operations_(tracker_pool_, storage_pool_,
                     config.network_timeout, config.retry_count)
        , closed_(false) {
    }

    ~ClientImpl() {
        close();
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) {
            return;
        }
        closed_ = true;
        tracker_pool_.close();
        storage_pool_.close();
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    void check_closed() const {
        if (is_closed()) {
            throw ClientClosedException();
        }
    }

    std::string upload_file(const std::string& local_filename,
                           const Metadata* metadata) {
        check_closed();
        return operations_.upload_file(local_filename, metadata, false);
    }

    std::string upload_buffer(const std::vector<uint8_t>& data,
                             const std::string& file_ext_name,
                             const Metadata* metadata) {
        check_closed();
        return operations_.upload_buffer(data, file_ext_name, metadata, false);
    }

    std::string upload_appender_file(const std::string& local_filename,
                                    const Metadata* metadata) {
        check_closed();
        return operations_.upload_file(local_filename, metadata, true);
    }

    std::string upload_appender_buffer(const std::vector<uint8_t>& data,
                                      const std::string& file_ext_name,
                                      const Metadata* metadata) {
        check_closed();
        return operations_.upload_buffer(data, file_ext_name, metadata, true);
    }

    std::string upload_slave_file(const std::string& master_file_id,
                                  const std::string& prefix_name,
                                  const std::string& file_ext_name,
                                  const std::vector<uint8_t>& data,
                                  const Metadata* metadata) {
        check_closed();
        return operations_.upload_slave_file(master_file_id, prefix_name,
                                            file_ext_name, data, metadata);
    }

    std::vector<uint8_t> download_file(const std::string& file_id) {
        check_closed();
        return operations_.download_file(file_id, 0, 0);
    }

    std::vector<uint8_t> download_file_range(const std::string& file_id,
                                            int64_t offset,
                                            int64_t length) {
        check_closed();
        return operations_.download_file(file_id, offset, length);
    }

    void download_to_file(const std::string& file_id,
                         const std::string& local_filename) {
        check_closed();
        operations_.download_to_file(file_id, local_filename);
    }

    void delete_file(const std::string& file_id) {
        check_closed();
        operations_.delete_file(file_id);
    }

    void append_file(const std::string& file_id,
                    const std::vector<uint8_t>& data) {
        check_closed();
        operations_.append_file(file_id, data);
    }

    void modify_file(const std::string& file_id,
                    int64_t offset,
                    const std::vector<uint8_t>& data) {
        check_closed();
        operations_.modify_file(file_id, offset, data);
    }

    void truncate_file(const std::string& file_id, int64_t size) {
        check_closed();
        operations_.truncate_file(file_id, size);
    }

    void set_metadata(const std::string& file_id,
                     const Metadata& metadata,
                     MetadataFlag flag) {
        check_closed();
        operations_.set_metadata(file_id, metadata, flag);
    }

    Metadata get_metadata(const std::string& file_id) {
        check_closed();
        return operations_.get_metadata(file_id);
    }

    FileInfo get_file_info(const std::string& file_id) {
        check_closed();
        return operations_.get_file_info(file_id);
    }

    bool file_exists(const std::string& file_id) {
        check_closed();
        try {
            get_file_info(file_id);
            return true;
        } catch (const FileNotFoundException&) {
            return false;
        }
    }

private:
    ClientConfig config_;
    internal::ConnectionPool tracker_pool_;
    internal::ConnectionPool storage_pool_;
    internal::Operations operations_;
    mutable std::mutex mutex_;
    std::atomic<bool> closed_;
};

// Client implementation

Client::Client(const ClientConfig& config) {
    // Validate configuration
    if (config.tracker_addrs.empty()) {
        throw InvalidArgumentException("Tracker addresses are required");
    }
    for (const auto& addr : config.tracker_addrs) {
        if (addr.empty() || addr.find(':') == std::string::npos) {
            throw InvalidArgumentException("Invalid tracker address: " + addr);
        }
    }

    impl_ = std::make_unique<ClientImpl>(config);
}

Client::~Client() = default;

Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

std::string Client::upload_file(const std::string& local_filename,
                               const Metadata* metadata) {
    return impl_->upload_file(local_filename, metadata);
}

std::string Client::upload_buffer(const std::vector<uint8_t>& data,
                                 const std::string& file_ext_name,
                                 const Metadata* metadata) {
    return impl_->upload_buffer(data, file_ext_name, metadata);
}

std::string Client::upload_appender_file(const std::string& local_filename,
                                        const Metadata* metadata) {
    return impl_->upload_appender_file(local_filename, metadata);
}

std::string Client::upload_appender_buffer(const std::vector<uint8_t>& data,
                                          const std::string& file_ext_name,
                                          const Metadata* metadata) {
    return impl_->upload_appender_buffer(data, file_ext_name, metadata);
}

std::string Client::upload_slave_file(const std::string& master_file_id,
                                     const std::string& prefix_name,
                                     const std::string& file_ext_name,
                                     const std::vector<uint8_t>& data,
                                     const Metadata* metadata) {
    return impl_->upload_slave_file(master_file_id, prefix_name,
                                   file_ext_name, data, metadata);
}

std::vector<uint8_t> Client::download_file(const std::string& file_id) {
    return impl_->download_file(file_id);
}

std::vector<uint8_t> Client::download_file_range(const std::string& file_id,
                                                 int64_t offset,
                                                 int64_t length) {
    return impl_->download_file_range(file_id, offset, length);
}

void Client::download_to_file(const std::string& file_id,
                             const std::string& local_filename) {
    impl_->download_to_file(file_id, local_filename);
}

void Client::delete_file(const std::string& file_id) {
    impl_->delete_file(file_id);
}

void Client::append_file(const std::string& file_id,
                        const std::vector<uint8_t>& data) {
    impl_->append_file(file_id, data);
}

void Client::modify_file(const std::string& file_id,
                        int64_t offset,
                        const std::vector<uint8_t>& data) {
    impl_->modify_file(file_id, offset, data);
}

void Client::truncate_file(const std::string& file_id, int64_t size) {
    impl_->truncate_file(file_id, size);
}

void Client::set_metadata(const std::string& file_id,
                         const Metadata& metadata,
                         MetadataFlag flag) {
    impl_->set_metadata(file_id, metadata, flag);
}

Metadata Client::get_metadata(const std::string& file_id) {
    return impl_->get_metadata(file_id);
}

FileInfo Client::get_file_info(const std::string& file_id) {
    return impl_->get_file_info(file_id);
}

bool Client::file_exists(const std::string& file_id) {
    return impl_->file_exists(file_id);
}

void Client::close() {
    impl_->close();
}

} // namespace fastdfs

