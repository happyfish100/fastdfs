"""
FastDFS Metadata Operations Example

This example demonstrates metadata operations:
- Uploading files with metadata
- Setting metadata
- Getting metadata
- Merging metadata
"""

from fdfs import Client, ClientConfig
from fdfs.types import MetadataFlag


def main():
    """Main example function."""
    # Configure client
    config = ClientConfig(
        tracker_addrs=['192.168.1.100:22122'],  # Replace with your tracker address
    )
    
    # Create client using context manager
    with Client(config) as client:
        print("FastDFS Python Client - Metadata Example")
        print("=" * 50)
        
        try:
            # Example 1: Upload with metadata
            print("\n1. Uploading file with metadata...")
            test_data = b"Document content with metadata"
            metadata = {
                'author': 'John Doe',
                'date': '2025-01-15',
                'version': '1.0',
                'department': 'Engineering'
            }
            
            file_id = client.upload_buffer(test_data, 'txt', metadata)
            print(f"   Uploaded successfully!")
            print(f"   File ID: {file_id}")
            
            # Example 2: Get metadata
            print("\n2. Getting metadata...")
            retrieved_metadata = client.get_metadata(file_id)
            print("   Metadata:")
            for key, value in retrieved_metadata.items():
                print(f"     {key}: {value}")
            
            # Example 3: Update metadata (overwrite)
            print("\n3. Updating metadata (overwrite mode)...")
            new_metadata = {
                'author': 'Jane Smith',
                'date': '2025-01-16',
                'status': 'reviewed'
            }
            client.set_metadata(file_id, new_metadata, MetadataFlag.OVERWRITE)
            
            retrieved_metadata = client.get_metadata(file_id)
            print("   Updated metadata:")
            for key, value in retrieved_metadata.items():
                print(f"     {key}: {value}")
            
            # Example 4: Merge metadata
            print("\n4. Merging metadata...")
            merge_metadata = {
                'reviewer': 'Bob Johnson',
                'comments': 'Approved'
            }
            client.set_metadata(file_id, merge_metadata, MetadataFlag.MERGE)
            
            retrieved_metadata = client.get_metadata(file_id)
            print("   Merged metadata:")
            for key, value in retrieved_metadata.items():
                print(f"     {key}: {value}")
            
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