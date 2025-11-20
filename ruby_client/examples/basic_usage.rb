#!/usr/bin/env ruby
# Basic FastDFS Client Usage Example
#
# This example demonstrates basic usage of the FastDFS Ruby client,
# including client initialization, file upload, download, and deletion.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

# Require the FastDFS client library
# This loads all necessary modules and classes
require 'fastdfs'

# Main example function
# Demonstrates basic client operations
def main
  # Print example header
  # This helps identify the example output
  puts "=" * 60
  puts "FastDFS Ruby Client - Basic Usage Example"
  puts "=" * 60
  puts
  
  # Create client configuration
  # This specifies tracker servers and connection settings
  puts "Creating client configuration..."
  config = FastDFS::ClientConfig.new(
    # Tracker server addresses
    # These are the FastDFS tracker servers to connect to
    tracker_addrs: [
      '192.168.1.100:22122',
      '192.168.1.101:22122'
    ],
    
    # Maximum connections per server
    # This limits the connection pool size
    max_conns: 10,
    
    # Connection timeout in seconds
    # Maximum time to wait when establishing connections
    connect_timeout: 5.0,
    
    # Network I/O timeout in seconds
    # Maximum time to wait for network operations
    network_timeout: 30.0,
    
    # Idle connection timeout in seconds
    # Connections idle longer than this will be closed
    idle_timeout: 60.0,
    
    # Retry count for failed operations
    # Number of times to retry on transient failures
    retry_count: 3
  )
  puts "Configuration created successfully"
  puts
  
  # Initialize client
  # This creates connection pools and prepares the client
  puts "Initializing FastDFS client..."
  begin
    client = FastDFS::Client.new(config)
    puts "Client initialized successfully"
    puts
  rescue => e
    puts "Error initializing client: #{e.message}"
    puts e.backtrace
    exit 1
  end
  
  # Use client with ensure block
  # This ensures the client is closed even if an error occurs
  begin
    # Example file to upload
    # This is a test file that will be uploaded
    test_file = 'test.txt'
    
    # Create test file if it doesn't exist
    # This ensures we have a file to upload
    unless File.exist?(test_file)
      puts "Creating test file: #{test_file}"
      File.write(test_file, "Hello, FastDFS! This is a test file.\n" * 100)
      puts "Test file created successfully"
      puts
    end
    
    # Upload a file
    # This uploads the file to FastDFS and returns a file ID
    puts "Uploading file: #{test_file}"
    begin
      file_id = client.upload_file(test_file)
      puts "File uploaded successfully"
      puts "File ID: #{file_id}"
      puts
    rescue => e
      puts "Error uploading file: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Download the file
    # This downloads the file content from FastDFS
    puts "Downloading file: #{file_id}"
    begin
      data = client.download_file(file_id)
      puts "File downloaded successfully"
      puts "File size: #{data.bytesize} bytes"
      puts
    rescue => e
      puts "Error downloading file: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Download to file
    # This downloads the file and saves it locally
    downloaded_file = 'downloaded.txt'
    puts "Downloading file to: #{downloaded_file}"
    begin
      client.download_to_file(file_id, downloaded_file)
      puts "File downloaded to local filesystem successfully"
      puts "Local file: #{downloaded_file}"
      puts
    rescue => e
      puts "Error downloading to file: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Check if file exists
    # This verifies the file is still available
    puts "Checking if file exists: #{file_id}"
    begin
      exists = client.file_exists?(file_id)
      if exists
        puts "File exists"
      else
        puts "File does not exist"
      end
      puts
    rescue => e
      puts "Error checking file existence: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Delete the file
    # This removes the file from FastDFS
    puts "Deleting file: #{file_id}"
    begin
      client.delete_file(file_id)
      puts "File deleted successfully"
      puts
    rescue => e
      puts "Error deleting file: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Verify file is deleted
    # This confirms the file was actually deleted
    puts "Verifying file deletion: #{file_id}"
    begin
      exists = client.file_exists?(file_id)
      if exists
        puts "Warning: File still exists after deletion"
      else
        puts "File successfully deleted"
      end
      puts
    rescue => e
      puts "Error verifying file deletion: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Print success message
    # Example completed successfully
    puts "=" * 60
    puts "Example completed successfully!"
    puts "=" * 60
    
  rescue => e
    # Print error message
    # Example failed with error
    puts "=" * 60
    puts "Example failed with error: #{e.message}"
    puts "=" * 60
    puts e.backtrace
    exit 1
    
  ensure
    # Close the client
    # This releases all resources and connections
    puts
    puts "Closing client..."
    begin
      client.close
      puts "Client closed successfully"
    rescue => e
      puts "Error closing client: #{e.message}"
    end
  end
end

# Run the example
# Execute main function if this script is run directly
if __FILE__ == $0
  main
end

