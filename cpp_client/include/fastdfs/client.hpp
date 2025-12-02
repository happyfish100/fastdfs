/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#ifndef FASTDFS_CLIENT_HPP
#define FASTDFS_CLIENT_HPP

#include "fastdfs/types.hpp"
#include "fastdfs/errors.hpp"
#include <string>
#include <vector>
#include <memory>

namespace fastdfs {

// Forward declarations
class ClientImpl;

/**
 * FastDFS Client
 *
 * This class provides a high-level C++ API for interacting with FastDFS servers.
 * It handles connection pooling, automatic retries, and error handling.
 *
 * Example usage:
 * @code
 *   fastdfs::ClientConfig config;
 *   config.tracker_addrs = {"192.168.1.100:22122"};
 *   
 *   fastdfs::Client client(config);
 *   
 *   std::string file_id = client.upload_file("test.jpg", nullptr);
 *   std::vector<uint8_t> data = client.download_file(file_id);
 *   client.delete_file(file_id);
 * @endcode
 */
class Client {
public:
    /**
     * Constructs a new FastDFS client with the given configuration
     * @param config Client configuration
     * @throws InvalidArgumentException if configuration is invalid
     * @throws ConnectionException if connection to tracker fails
     */
    explicit Client(const ClientConfig& config);

    /**
     * Destructor - closes the client and releases all resources
     */
    ~Client();

    // Non-copyable
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Movable
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    /**
     * Uploads a file from the local filesystem to FastDFS
     * @param local_filename Path to the local file
     * @param metadata Optional metadata key-value pairs (can be nullptr)
     * @return File ID on success
     * @throws FileNotFoundException if local file doesn't exist
     * @throws ConnectionException on connection errors
     * @throws ProtocolException on protocol errors
     */
    std::string upload_file(const std::string& local_filename,
                           const Metadata* metadata = nullptr);

    /**
     * Uploads data from a buffer to FastDFS
     * @param data File content as byte vector
     * @param file_ext_name File extension without dot (e.g., "jpg", "txt")
     * @param metadata Optional metadata key-value pairs (can be nullptr)
     * @return File ID on success
     */
    std::string upload_buffer(const std::vector<uint8_t>& data,
                             const std::string& file_ext_name,
                             const Metadata* metadata = nullptr);

    /**
     * Uploads an appender file that can be modified later
     * @param local_filename Path to the local file
     * @param metadata Optional metadata
     * @return File ID on success
     */
    std::string upload_appender_file(const std::string& local_filename,
                                    const Metadata* metadata = nullptr);

    /**
     * Uploads an appender file from buffer
     * @param data File content
     * @param file_ext_name File extension
     * @param metadata Optional metadata
     * @return File ID on success
     */
    std::string upload_appender_buffer(const std::vector<uint8_t>& data,
                                      const std::string& file_ext_name,
                                      const Metadata* metadata = nullptr);

    /**
     * Uploads a slave file associated with a master file
     * @param master_file_id The master file ID
     * @param prefix_name Prefix for the slave file (e.g., "thumb", "small")
     * @param file_ext_name File extension
     * @param data File content
     * @param metadata Optional metadata
     * @return Slave file ID on success
     */
    std::string upload_slave_file(const std::string& master_file_id,
                                  const std::string& prefix_name,
                                  const std::string& file_ext_name,
                                  const std::vector<uint8_t>& data,
                                  const Metadata* metadata = nullptr);

    /**
     * Downloads a file from FastDFS and returns its content
     * @param file_id The file ID to download
     * @return File content as byte vector
     * @throws FileNotFoundException if file doesn't exist
     */
    std::vector<uint8_t> download_file(const std::string& file_id);

    /**
     * Downloads a specific range of bytes from a file
     * @param file_id The file ID
     * @param offset Starting byte offset
     * @param length Number of bytes to download (0 means to end of file)
     * @return File content as byte vector
     */
    std::vector<uint8_t> download_file_range(const std::string& file_id,
                                            int64_t offset,
                                            int64_t length);

    /**
     * Downloads a file and saves it to the local filesystem
     * @param file_id The file ID
     * @param local_filename Path where to save the file
     */
    void download_to_file(const std::string& file_id,
                         const std::string& local_filename);

    /**
     * Deletes a file from FastDFS
     * @param file_id The file ID to delete
     * @throws FileNotFoundException if file doesn't exist
     */
    void delete_file(const std::string& file_id);

    /**
     * Appends data to an appender file
     * @param file_id The appender file ID
     * @param data Data to append
     */
    void append_file(const std::string& file_id,
                    const std::vector<uint8_t>& data);

    /**
     * Modifies content of an appender file at specified offset
     * @param file_id The appender file ID
     * @param offset Byte offset where to modify
     * @param data New data to write
     */
    void modify_file(const std::string& file_id,
                    int64_t offset,
                    const std::vector<uint8_t>& data);

    /**
     * Truncates an appender file to specified size
     * @param file_id The appender file ID
     * @param size New file size
     */
    void truncate_file(const std::string& file_id, int64_t size);

    /**
     * Sets metadata for a file
     * @param file_id The file ID
     * @param metadata Metadata key-value pairs
     * @param flag Metadata operation flag (Overwrite or Merge)
     */
    void set_metadata(const std::string& file_id,
                     const Metadata& metadata,
                     MetadataFlag flag = MetadataFlag::OVERWRITE);

    /**
     * Retrieves metadata for a file
     * @param file_id The file ID
     * @return Metadata as key-value map
     */
    Metadata get_metadata(const std::string& file_id);

    /**
     * Retrieves file information including size, create time, and CRC32
     * @param file_id The file ID
     * @return FileInfo structure
     */
    FileInfo get_file_info(const std::string& file_id);

    /**
     * Checks if a file exists on the storage server
     * @param file_id The file ID
     * @return true if file exists, false otherwise
     */
    bool file_exists(const std::string& file_id);

    /**
     * Closes the client and releases all resources
     */
    void close();

private:
    std::unique_ptr<ClientImpl> impl_;
};

} // namespace fastdfs

#endif // FASTDFS_CLIENT_HPP

