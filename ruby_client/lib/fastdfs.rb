# FastDFS Ruby Client
#
# This is the main module for the FastDFS Ruby client library.
# It provides a Ruby interface for interacting with FastDFS distributed file system.
#
# The client handles connection pooling, automatic retries, error handling,
# and provides a simple Ruby-like API for uploading, downloading, deleting,
# and managing files stored in a FastDFS cluster.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.
#
# @example Basic usage
#   require 'fastdfs'
#
#   # Create client configuration
#   config = FastDFS::ClientConfig.new(
#     tracker_addrs: ['192.168.1.100:22122'],
#     max_conns: 10,
#     connect_timeout: 5.0,
#     network_timeout: 30.0
#   )
#
#   # Initialize client
#   client = FastDFS::Client.new(config)
#
#   # Upload a file
#   file_id = client.upload_file('test.jpg')
#
#   # Download the file
#   data = client.download_file(file_id)
#
#   # Delete the file
#   client.delete_file(file_id)
#
#   # Close the client
#   client.close

# Require all module files
# Load all components of the FastDFS client

# Core client classes
require_relative 'fastdfs/client'

# Configuration classes
require_relative 'fastdfs/client_config'

# Connection management
require_relative 'fastdfs/connection_pool'

# Protocol implementation
require_relative 'fastdfs/protocol'

# Type definitions
require_relative 'fastdfs/types'

# Error definitions
require_relative 'fastdfs/errors'

# Operations implementation
require_relative 'fastdfs/operations'

# FastDFS module
# This is the main namespace for all FastDFS client functionality
module FastDFS
  # Version string for the FastDFS Ruby client
  # This follows semantic versioning: MAJOR.MINOR.PATCH
  # MAJOR: Breaking changes
  # MINOR: New features (backwards compatible)
  # PATCH: Bug fixes
  VERSION = '1.0.0'
  
  # Module-level methods can be added here
  # These are utility methods that don't require a client instance
  
  # Placeholder for future module-level methods
  # This section can be expanded as needed
end

