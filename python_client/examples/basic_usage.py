"""
Basic FastDFS Client Usage Example

This example demonstrates the basic operations:
- Uploading files
- Downloading files
- Deleting files
"""

from fdfs import Client, ClientConfig


def main():
    """Main example function."""
    # Configure client
    config = ClientConfig(
        tracker_addrs=['192.168.1.100:22122'],  # Replace with your tracker address
        max_conns=10,
        connect_timeout=5.0,
        network_timeout=30.0
    )
    
    # Create client
    client = Client(config)
    
    try:
        print("FastDFS Python Client - Basic Usage Example")
        print("=" * 50)
        
        # Example 1: Upload from buffer
        print("\n1. Uploading data from buffer...")
        test_data = b"Hello, FastDFS! This is a test file."
        file_id = client.upload_buffer(test_data, 'txt')
        print(f"   Uploaded successfully!")
        print(f"   File ID: {file_id}")
        
        # Example 2: Download file
        print("\n2. Downloading file...")
        downloaded_data = client.download_file(file_id)
        print(f"   Downloaded {len(downloaded_data)} bytes")
        print(f"   Content: {downloaded_data.decode('utf-8')}")
        
        # Example 3: Get file information
        print("\n3. Getting file information...")
        file_info = client.get_file_info(file_id)
        print(f"   File size: {file_info.file_size} bytes")
        print(f"   Create time: {file_info.create_time}")
        print(f"   CRC32: {file_info.crc32}")
        print(f"   Source IP: {file_info.source_ip_addr}")
        
        # Example 4: Check if file exists
        print("\n4. Checking file existence...")
        exists = client.file_exists(file_id)
        print(f"   File exists: {exists}")
        
        # Example 5: Delete file
        print("\n5. Deleting file...")
        client.delete_file(file_id)
        print("   File deleted successfully!")
        
        # Verify deletion
        exists = client.file_exists(file_id)
        print(f"   File exists after deletion: {exists}")
        
        print("\n" + "=" * 50)
        print("Example completed successfully!")
        
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
    
    finally:
        # Always close the client
        client.close()
        print("\nClient closed.")


if __name__ == '__main__':
    main()