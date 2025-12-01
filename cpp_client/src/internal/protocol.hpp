/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS may be copied only under the terms of the GNU General
 * Public License V3, which may be found in the FastDFS source kit.
 */

#ifndef FASTDFS_INTERNAL_PROTOCOL_HPP
#define FASTDFS_INTERNAL_PROTOCOL_HPP

#include "fastdfs/types.hpp"
#include <string>
#include <vector>
#include <map>

namespace fastdfs {
namespace internal {

/**
 * Protocol header structure
 */
struct ProtocolHeader {
    int64_t length;
    uint8_t cmd;
    uint8_t status;
};

/**
 * Encodes a protocol header
 */
std::vector<uint8_t> encode_header(int64_t length, uint8_t cmd, uint8_t status = 0);

/**
 * Decodes a protocol header
 */
ProtocolHeader decode_header(const std::vector<uint8_t>& data);

/**
 * Splits a file ID into group name and remote filename
 */
void split_file_id(const std::string& file_id,
                   std::string& group_name,
                   std::string& remote_filename);

/**
 * Joins group name and remote filename into a file ID
 */
std::string join_file_id(const std::string& group_name,
                        const std::string& remote_filename);

/**
 * Encodes metadata into FastDFS wire format
 */
std::vector<uint8_t> encode_metadata(const Metadata& metadata);

/**
 * Decodes metadata from FastDFS wire format
 */
Metadata decode_metadata(const std::vector<uint8_t>& data);

} // namespace internal
} // namespace fastdfs

#endif // FASTDFS_INTERNAL_PROTOCOL_HPP

