#!/usr/bin/env ruby
# FastDFS Metadata Operations Example
#
# This example demonstrates metadata operations with FastDFS,
# including setting and retrieving metadata for files.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

# Require the FastDFS client library
# This loads all necessary modules and classes
require 'fastdfs'

# Main example function
# Demonstrates metadata operations
def main
  # Print example header
  # This helps identify the example output
  puts "=" * 60
  puts "FastDFS Ruby Client - Metadata Operations Example"
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
    # Example 1: Upload file with metadata
    # This uploads a file with initial metadata
    puts "Example 1: Upload file with metadata"
    test_file = 'test_metadata.txt'
    
    # Create test file if it doesn't exist
    # This ensures we have a file to upload
    unless File.exist?(test_file)
      puts "Creating test file: #{test_file}"
      File.write(test_file, "Test file with metadata\n")
      puts "Test file created successfully"
      puts
    end
    
    # Define initial metadata
    # These are key-value pairs to associate with the file
    initial_metadata = {
      'author' => 'John Doe',
      'created_date' => '2025-01-01',
      'file_type' => 'text',
      'description' => 'Example file with metadata'
    }
    
    puts "Uploading file with metadata..."
    puts "Metadata: #{initial_metadata.inspect}"
    begin
      file_id = client.upload_file(test_file, initial_metadata)
      puts "File uploaded successfully"
      puts "File ID: #{file_id}"
      puts
    rescue => e
      puts "Error uploading file: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 2: Retrieve metadata
    # This retrieves the metadata associated with the file
    puts "Example 2: Retrieve metadata"
    puts "Retrieving metadata for file: #{file_id}"
    begin
      metadata = client.get_metadata(file_id)
      puts "Metadata retrieved successfully"
      puts "Metadata: #{metadata.inspect}"
      puts
    rescue => e
      puts "Error retrieving metadata: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 3: Update metadata (overwrite)
    # This replaces all existing metadata with new values
    puts "Example 3: Update metadata (overwrite)"
    new_metadata = {
      'author' => 'Jane Smith',
      'updated_date' => '2025-01-02',
      'version' => '2.0'
    }
    puts "Updating metadata (overwrite mode)..."
    puts "New metadata: #{new_metadata.inspect}"
    begin
      client.set_metadata(file_id, new_metadata, :overwrite)
      puts "Metadata updated successfully"
      puts
    rescue => e
      puts "Error updating metadata: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Verify metadata was overwritten
    # This confirms that old metadata was replaced
    puts "Verifying metadata was overwritten..."
    begin
      updated_metadata = client.get_metadata(file_id)
      puts "Current metadata: #{updated_metadata.inspect}"
      
      # Check that old metadata is gone
      if updated_metadata.key?('created_date')
        puts "Warning: Old metadata still present (should have been overwritten)"
      else
        puts "Metadata successfully overwritten"
      end
      puts
    rescue => e
      puts "Error verifying metadata: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 4: Merge metadata
    # This merges new metadata with existing metadata
    puts "Example 4: Merge metadata"
    merge_metadata = {
      'tags' => 'example, test, metadata',
      'category' => 'documentation'
    }
    puts "Merging metadata..."
    puts "Merge metadata: #{merge_metadata.inspect}"
    begin
      client.set_metadata(file_id, merge_metadata, :merge)
      puts "Metadata merged successfully"
      puts
    rescue => e
      puts "Error merging metadata: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Verify metadata was merged
    # This confirms that new metadata was added to existing metadata
    puts "Verifying metadata was merged..."
    begin
      merged_metadata = client.get_metadata(file_id)
      puts "Current metadata: #{merged_metadata.inspect}"
      
      # Check that both old and new metadata are present
      if merged_metadata.key?('author') && merged_metadata.key?('tags')
        puts "Metadata successfully merged"
      else
        puts "Warning: Metadata merge may not have worked correctly"
      end
      puts
    rescue => e
      puts "Error verifying metadata: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Example 5: Metadata with special characters
    # This demonstrates handling of special characters in metadata
    puts "Example 5: Metadata with special characters"
    special_metadata = {
      'name' => 'Test File with Special Characters: !@#$%^&*()',
      'unicode' => '测试文件 - файл тест',
      'multiline' => "Line 1\nLine 2\nLine 3"
    }
    puts "Setting metadata with special characters..."
    puts "Special metadata: #{special_metadata.inspect}"
    begin
      client.set_metadata(file_id, special_metadata, :overwrite)
      puts "Special metadata set successfully"
      puts
    rescue => e
      puts "Error setting special metadata: #{e.message}"
      puts e.backtrace
      raise
    end
    
    # Clean up: Delete the file
    # This removes the file from FastDFS
    puts "Cleaning up: Deleting file..."
    begin
      client.delete_file(file_id)
      puts "File deleted successfully"
      puts
    rescue => e
      puts "Error deleting file: #{e.message}"
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

