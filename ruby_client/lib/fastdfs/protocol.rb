# FastDFS Protocol Implementation
#
# This module implements the FastDFS protocol for communication with tracker
# and storage servers. It handles protocol header encoding/decoding, request
# building, response parsing, and all protocol-level operations.
#
# The protocol is binary-based and uses a fixed 10-byte header followed by
# a variable-length body. All integers are in network byte order (big-endian).
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

require_relative 'types'
require_relative 'errors'

module FastDFS
  # Protocol helper methods for encoding and decoding FastDFS protocol messages.
  #
  # This module provides low-level protocol operations for building requests
  # and parsing responses according to the FastDFS protocol specification.
  #
  # All methods are module methods and can be called without instantiating
  # a class.
  module Protocol
    module_function
    
    # Encodes a protocol header.
    #
    # The protocol header is 10 bytes: 8 bytes for body length (big-endian),
    # 1 byte for command code, 1 byte for status.
    #
    # @param body_len [Integer] Length of the message body in bytes.
    # @param cmd [Integer] Command code (byte value 0-255).
    # @param status [Integer] Status code (byte value 0-255, default: 0).
    #
    # @return [String] Encoded header as binary string (10 bytes).
    def encode_header(body_len, cmd, status = 0)
      # Build header bytes
      # 8 bytes for body length (big-endian)
      # 1 byte for command code
      # 1 byte for status
      header = ''
      
      # Encode body length as 8-byte big-endian integer
      # This allows for very large message bodies
      8.times do |i|
        # Extract byte from body length
        # Shift right by 8*i bits and mask to get byte
        byte = (body_len >> (8 * (7 - i))) & 0xFF
        header << byte.chr
      end
      
      # Encode command code as single byte
      # Must be in range 0-255
      header << (cmd & 0xFF).chr
      
      # Encode status as single byte
      # Must be in range 0-255
      header << (status & 0xFF).chr
      
      # Return encoded header
      # Should be exactly 10 bytes
      header
    end
    
    # Decodes a protocol header.
    #
    # Parses a 10-byte header into its components: body length, command code,
    # and status.
    #
    # @param header [String] Encoded header as binary string (must be 10 bytes).
    #
    # @return [TrackerHeader] Decoded header object.
    #
    # @raise [InvalidResponseError] If header is invalid.
    def decode_header(header)
      # Validate header size
      # Must be exactly 10 bytes
      if header.bytesize != FDFS_PROTO_HEADER_LEN
        raise InvalidResponseError.new("invalid header size: expected #{FDFS_PROTO_HEADER_LEN}, got #{header.bytesize}")
      end
      
      # Decode body length from first 8 bytes (big-endian)
      # Reconstruct 64-bit integer from bytes
      body_len = 0
      8.times do |i|
        # Get byte at position i
        # Convert character to integer
        byte = header[i].ord
        
        # Shift byte to correct position
        # Combine bytes to form integer
        body_len = (body_len << 8) | byte
      end
      
      # Decode command code from byte 8
      # Extract single byte value
      cmd = header[8].ord
      
      # Decode status from byte 9
      # Extract single byte value
      status = header[9].ord
      
      # Create and return header object
      # Contains all decoded values
      TrackerHeader.new(body_len, cmd, status)
    end
    
    # Encodes a 64-bit integer to 8-byte big-endian format.
    #
    # This is used for encoding offsets and lengths in protocol messages.
    #
    # @param value [Integer] The integer value to encode.
    #
    # @return [String] Encoded integer as 8-byte binary string.
    def encode_int64(value)
      # Build 8-byte representation
      # Encode as big-endian (network byte order)
      result = ''
      
      # Encode each byte
      # Shift right by 8*i bits to extract byte
      8.times do |i|
        # Extract byte at position i
        # Mask to get single byte
        byte = (value >> (8 * (7 - i))) & 0xFF
        result << byte.chr
      end
      
      # Return encoded integer
      # Should be exactly 8 bytes
      result
    end
    
    # Decodes an 8-byte big-endian integer.
    #
    # This is used for decoding offsets and lengths from protocol messages.
    #
    # @param data [String] Encoded integer as 8-byte binary string.
    #
    # @return [Integer] Decoded integer value.
    #
    # @raise [InvalidResponseError] If data is invalid.
    def decode_int64(data)
      # Validate data size
      # Must be exactly 8 bytes
      if data.bytesize != 8
        raise InvalidResponseError.new("invalid int64 size: expected 8, got #{data.bytesize}")
      end
      
      # Decode from big-endian format
      # Reconstruct 64-bit integer from bytes
      value = 0
      8.times do |i|
        # Get byte at position i
        # Convert character to integer
        byte = data[i].ord
        
        # Shift byte to correct position
        # Combine bytes to form integer
        value = (value << 8) | byte
      end
      
      # Return decoded integer
      # Should be 64-bit signed integer
      value
    end
    
    # Pads a string to a fixed length.
    #
    # This is used for protocol fields that must be a fixed size, such as
    # group names and file extensions. The string is padded with null bytes
    # to the required length.
    #
    # @param str [String] The string to pad.
    # @param len [Integer] The target length.
    #
    # @return [String] Padded string (exactly 'len' bytes).
    def pad_string(str, len)
      # Convert to string if necessary
      # Ensure we have a string object
      str = str.to_s
      
      # Truncate if too long
      # Protocol fields have maximum lengths
      if str.bytesize > len
        str = str[0, len]
      end
      
      # Pad with null bytes
      # Fill remaining space with zeros
      if str.bytesize < len
        # Calculate padding needed
        # Need to add this many null bytes
        padding = len - str.bytesize
        
        # Add null bytes
        # Use \0 character for padding
        str += "\0" * padding
      end
      
      # Return padded string
      # Should be exactly 'len' bytes
      str
    end
    
    # Removes padding from a string.
    #
    # This removes trailing null bytes that were added during padding.
    # Used when decoding protocol responses.
    #
    # @param str [String] The padded string.
    #
    # @return [String] String with padding removed.
    def unpad_string(str)
      # Convert to string if necessary
      # Ensure we have a string object
      str = str.to_s
      
      # Remove trailing null bytes
      # Protocol fields are null-padded
      str = str.gsub(/\0+$/, '')
      
      # Return unpadded string
      # Should have no trailing nulls
      str
    end
    
    # Splits a file ID into group name and remote filename.
    #
    # File IDs are in the format "group/remote_filename" where group is the
    # storage group name and remote_filename is the path on the storage server.
    #
    # @param file_id [String] The file ID to split.
    #
    # @return [Array<String>] Array containing [group_name, remote_filename].
    #
    # @raise [InvalidFileIDError] If file ID format is invalid.
    def split_file_id(file_id)
      # Validate file ID
      # Must not be nil or empty
      if file_id.nil? || file_id.empty?
        raise InvalidFileIDError.new("file_id cannot be nil or empty")
      end
      
      # Find first slash
      # This separates group name from filename
      slash_index = file_id.index('/')
      
      # Check if slash exists
      # File IDs must have format "group/remote_filename"
      if slash_index.nil?
        raise InvalidFileIDError.new("invalid file_id format: missing '/' separator")
      end
      
      # Extract group name
      # Everything before the first slash
      group_name = file_id[0, slash_index]
      
      # Validate group name
      # Must not be empty
      if group_name.empty?
        raise InvalidFileIDError.new("invalid file_id format: empty group name")
      end
      
      # Extract remote filename
      # Everything after the first slash
      remote_filename = file_id[(slash_index + 1)..-1]
      
      # Validate remote filename
      # Must not be empty
      if remote_filename.empty?
        raise InvalidFileIDError.new("invalid file_id format: empty remote filename")
      end
      
      # Return split components
      # Array with group name and remote filename
      [group_name, remote_filename]
    end
    
    # Joins group name and remote filename into a file ID.
    #
    # This is the inverse of split_file_id.
    #
    # @param group_name [String] The storage group name.
    # @param remote_filename [String] The remote filename.
    #
    # @return [String] The file ID in "group/remote_filename" format.
    def join_file_id(group_name, remote_filename)
      # Validate group name
      # Must not be nil or empty
      if group_name.nil? || group_name.empty?
        raise InvalidArgumentError.new("group_name cannot be nil or empty")
      end
      
      # Validate remote filename
      # Must not be nil or empty
      if remote_filename.nil? || remote_filename.empty?
        raise InvalidArgumentError.new("remote_filename cannot be nil or empty")
      end
      
      # Join with slash separator
      # This forms the complete file ID
      "#{group_name}/#{remote_filename}"
    end
    
    # Encodes metadata as a binary string.
    #
    # Metadata is encoded as key-value pairs separated by record separator
    # (0x01), with keys and values separated by field separator (0x02).
    #
    # @param metadata [Hash<String, String>] Metadata key-value pairs.
    #
    # @return [String] Encoded metadata as binary string.
    #
    # @raise [InvalidMetadataError] If metadata is invalid.
    def encode_metadata(metadata)
      # Validate metadata
      # Must be a hash
      unless metadata.is_a?(Hash)
        raise InvalidMetadataError.new("metadata must be a hash")
      end
      
      # Build encoded string
      # Will contain all key-value pairs
      encoded = ''
      first = true
      
      # Iterate through metadata entries
      # Encode each key-value pair
      metadata.each do |key, value|
        # Validate key
        # Must be a string and within length limit
        unless key.is_a?(String)
          raise InvalidMetadataError.new("metadata key must be a string")
        end
        
        if key.bytesize > FDFS_MAX_META_NAME_LEN
          raise InvalidMetadataError.new("metadata key too long: maximum #{FDFS_MAX_META_NAME_LEN} bytes")
        end
        
        # Validate value
        # Must be a string and within length limit
        unless value.is_a?(String)
          raise InvalidMetadataError.new("metadata value must be a string")
        end
        
        if value.bytesize > FDFS_MAX_META_VALUE_LEN
          raise InvalidMetadataError.new("metadata value too long: maximum #{FDFS_MAX_META_VALUE_LEN} bytes")
        end
        
        # Add record separator (except for first entry)
        # Separates different key-value pairs
        encoded << FDFS_RECORD_SEPARATOR unless first
        
        # Add key
        # First part of key-value pair
        encoded << key
        
        # Add field separator
        # Separates key from value
        encoded << FDFS_FIELD_SEPARATOR
        
        # Add value
        # Second part of key-value pair
        encoded << value
        
        # Mark that first entry is done
        # Subsequent entries need record separator
        first = false
      end
      
      # Return encoded metadata
      # Should be properly formatted binary string
      encoded
    end
    
    # Decodes metadata from a binary string.
    #
    # This is the inverse of encode_metadata.
    #
    # @param data [String] Encoded metadata as binary string.
    #
    # @return [Hash<String, String>] Decoded metadata key-value pairs.
    #
    # @raise [InvalidMetadataError] If data is invalid.
    def decode_metadata(data)
      # Validate data
      # Must be a string
      unless data.is_a?(String)
        raise InvalidMetadataError.new("metadata data must be a string")
      end
      
      # Handle empty metadata
      # Return empty hash
      return {} if data.empty?
      
      # Build metadata hash
      # Will contain all decoded key-value pairs
      metadata = {}
      
      # Split by record separator
      # Each record is a key-value pair
      records = data.split(FDFS_RECORD_SEPARATOR, -1)
      
      # Process each record
      # Decode key-value pairs
      records.each do |record|
        # Skip empty records
        # Can occur at beginning or end
        next if record.empty?
        
        # Split by field separator
        # Separates key from value
        parts = record.split(FDFS_FIELD_SEPARATOR, 2)
        
        # Validate record format
        # Must have exactly two parts (key and value)
        unless parts.size == 2
          raise InvalidMetadataError.new("invalid metadata record format")
        end
        
        # Extract key and value
        # First part is key, second is value
        key = parts[0]
        value = parts[1]
        
        # Validate key and value
        # Both must be non-empty
        if key.empty?
          raise InvalidMetadataError.new("metadata key cannot be empty")
        end
        
        # Add to metadata hash
        # Store key-value pair
        metadata[key] = value
      end
      
      # Return decoded metadata
      # Should be a hash with all key-value pairs
      metadata
    end
  end
end

