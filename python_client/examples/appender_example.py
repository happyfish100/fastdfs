"""
FastDFS Appender File Operations Example

This example demonstrates appender file operations:
- Uploading appender files
- Appending data (Note: requires storage server support)
"""

from fdfs import Client, ClientConfig


def main():
    """Main example function."""
    # Configure client
    config = ClientConfig(
        tracker_addrs=['192.168.1.100:22122'],  # Replace with your tracker address
    )
    
    with Client(config) as client:
        print("FastDFS Python Client - Appender File Example")
        print("=" * 50)
        
        try:
            # Example 1: Upload appender file
            print("\n1. Uploading appender file...")
            initial_data = b"Initial log entry\n"
            file_id = client.upload_appender_buffer(initial_data, 'log')
            print(f"   Uploaded successfully!")
            print(f"   File ID: {file_id}")
            
            # Example 2: Get initial file info
            print("\n2. Getting initial file information...")
            file_info = client.get_file_info(file_id)
            print(f"   File size: {file_info.file_size} bytes")
            print(f"   Create time: {file_info.create_time}")
            
            # Example 3: Download and display content
            print("\n3. Downloading file content...")
            content = client.download_file(file_id)
            print(f"   Content:\n{content.decode('utf-8')}")
            
            # Note: Append, modify, and truncate operations require
            # storage server configuration to support appender files.
            # These operations are not demonstrated here as they may
            # not be available in all FastDFS deployments.
            
            print("\n4. Appender file operations:")
            print("   - Append: Adds data to the end of the file")
            print("   - Modify: Changes data at a specific offset")
            print("   - Truncate: Reduces file size to specified length")
            print("   Note: These operations require storage server support")
            
            # Clean up
            print("\n5. Cleaning up...")
            client.delete_file(file_id)
            print("   File deleted successfully!")
            
            print("\n" + "=" * 50)
            print("Example completed successfully!")
            
        except Exception as e:
            print(f"\nError: {e}")
            import traceback
            traceback.print_exc()


if __name__ == '__main__':
    main()