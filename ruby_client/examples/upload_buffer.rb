#!/usr/bin/env ruby
# FastDFS Upload Buffer Example
#
# This example demonstrates uploading data from memory (byte buffer) to FastDFS,
# without requiring a file on the local filesystem.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

# Require the FastDFS client library
# This loads all necessary modules and classes
require 'fastdfs'

# Main example function
# Demonstrates buffer upload operations
def main
  # Print example header
  # This helps identify the example output
  puts "=" * 60
  puts "FastDFS Ruby Client - Upload Buffer Example"
  puts "=" * 60
  puts
  
  # Create client configuration
  # This specifies tracker servers and connection settings
  puts "Creating client configuration..."
  config = FastDFS::ClientConfig.new(
    # Tracker server addresses
    # These are the FastDFS tracker servers to connect to
    tracker_addrs: ['127.0.0.1:22122'],
    
    # Maximum connections per server
    # This limits the connection pool size
    max_conns: 10,
    
    # Connection timeout in seconds
    # Maximum time to wait when establishing connections
    connect_timeout: 5.0,
    
    # Network I/O timeout in seconds
    # Maximum time to wait for network operations
    network_timeout: 30.0
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
    # Example 1: Upload text data
    # This uploads a simple text string
    puts "Example 1: Uploading text data"
    text_data = "Hello, FastDFS! This is text data uploaded from memory."
    puts "Text data: #{text_data}"
    begin
      file_id = client.upload_buffer(text_data, 'txt')
      puts "File uploaded successfully"
      puts "File ID: #{file_id}"
      puts
    rescue => e
      puts "Error uploading text data: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 2: Upload JSON data
    # This uploads JSON formatted data
    puts "Example 2: Uploading JSON data"
    json_data = '{"name": "FastDFS", "version": "1.0.0", "language": "Ruby"}'
    puts "JSON data: #{json_data}"
    begin
      file_id = client.upload_buffer(json_data, 'json')
      puts "File uploaded successfully"
      puts "File ID: #{file_id}"
      puts
    rescue => e
      puts "Error uploading JSON data: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 3: Upload binary data
    # This uploads binary data (simulating an image)
    puts "Example 3: Uploading binary data"
    binary_data = "\xFF\xD8\xFF\xE0" + "\x00" * 100  # Fake JPEG header
    puts "Binary data size: #{binary_data.bytesize} bytes"
    begin
      file_id = client.upload_buffer(binary_data, 'jpg')
      puts "File uploaded successfully"
      puts "File ID: #{file_id}"
      puts
    rescue => e
      puts "Error uploading binary data: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 4: Upload with metadata
    # This uploads data with associated metadata
    puts "Example 4: Uploading with metadata"
    data = "This file has metadata attached."
    metadata = {
      'author' => 'John Doe',
      'date' => '2025-01-01',
      'description' => 'Example file with metadata'
    }
    puts "Data: #{data}"
    puts "Metadata: #{metadata.inspect}"
    begin
      file_id = client.upload_buffer(data, 'txt', metadata)
      puts "File uploaded successfully"
      puts "File ID: #{file_id}"
      puts
    rescue => e
      puts "Error uploading with metadata: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 5: Upload large data
    # This uploads a larger amount of data
    puts "Example 5: Uploading large data"
    large_data = "Large data chunk\n" * 10000
    puts "Large data size: #{large_data.bytesize} bytes"
    begin
      file_id = client.upload_buffer(large_data, 'txt')
      puts "File uploaded successfully"
      puts "File ID: #{file_id}"
      puts
    rescue => e
      puts "Error uploading large data: #{e.message}"
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

