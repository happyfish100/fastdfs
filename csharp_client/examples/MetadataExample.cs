// ============================================================================
// FastDFS C# Client - Metadata Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates metadata operations in FastDFS, including
// setting metadata, getting metadata, and using metadata flags.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating FastDFS metadata operations.
    /// 
    /// This example shows:
    /// - How to upload files with metadata
    /// - How to set metadata for existing files
    /// - How to get metadata from files
    /// - How to use metadata flags (Overwrite vs Merge)
    /// </summary>
    class MetadataExample
    {
        /// <summary>
        /// Main entry point for the metadata example.
        /// </summary>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Metadata Example");
            Console.WriteLine("======================================");
            Console.WriteLine();

            // Create client configuration
            var config = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30)
            };

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // Example 1: Upload file with metadata
                    Console.WriteLine("Example 1: Upload file with metadata");
                    Console.WriteLine("------------------------------------");

                    var metadata = new Dictionary<string, string>
                    {
                        { "author", "John Doe" },
                        { "date", "2025-01-01" },
                        { "description", "Test file with metadata" }
                    };

                    var localFile = "test_metadata.txt";
                    if (!File.Exists(localFile))
                    {
                        await File.WriteAllTextAsync(localFile, "This is a test file with metadata.");
                    }

                    var fileId = await client.UploadFileAsync(localFile, metadata);
                    Console.WriteLine($"File uploaded: {fileId}");
                    Console.WriteLine();

                    // Example 2: Get metadata
                    Console.WriteLine("Example 2: Get metadata");
                    Console.WriteLine("------------------------");

                    var retrievedMetadata = await client.GetMetadataAsync(fileId);
                    Console.WriteLine("Retrieved metadata:");
                    foreach (var kvp in retrievedMetadata)
                    {
                        Console.WriteLine($"  {kvp.Key}: {kvp.Value}");
                    }
                    Console.WriteLine();

                    // Example 3: Set metadata with Overwrite flag
                    Console.WriteLine("Example 3: Set metadata with Overwrite flag");
                    Console.WriteLine("-------------------------------------------");

                    var newMetadata = new Dictionary<string, string>
                    {
                        { "author", "Jane Smith" },
                        { "version", "2.0" }
                    };

                    await client.SetMetadataAsync(fileId, newMetadata, MetadataFlag.Overwrite);
                    Console.WriteLine("Metadata overwritten");

                    var updatedMetadata = await client.GetMetadataAsync(fileId);
                    Console.WriteLine("Updated metadata:");
                    foreach (var kvp in updatedMetadata)
                    {
                        Console.WriteLine($"  {kvp.Key}: {kvp.Value}");
                    }
                    Console.WriteLine("Note: Only 'author' and 'version' remain (Overwrite removed other keys)");
                    Console.WriteLine();

                    // Example 4: Set metadata with Merge flag
                    Console.WriteLine("Example 4: Set metadata with Merge flag");
                    Console.WriteLine("----------------------------------------");

                    var additionalMetadata = new Dictionary<string, string>
                    {
                        { "author", "Bob Johnson" },
                        { "category", "Documentation" },
                        { "tags", "fastdfs, csharp, example" }
                    };

                    await client.SetMetadataAsync(fileId, additionalMetadata, MetadataFlag.Merge);
                    Console.WriteLine("Metadata merged");

                    var mergedMetadata = await client.GetMetadataAsync(fileId);
                    Console.WriteLine("Merged metadata:");
                    foreach (var kvp in mergedMetadata)
                    {
                        Console.WriteLine($"  {kvp.Key}: {kvp.Value}");
                    }
                    Console.WriteLine("Note: 'author' was updated, 'category' and 'tags' were added, 'version' was kept");
                    Console.WriteLine();

                    // Clean up
                    await client.DeleteFileAsync(fileId);
                    if (File.Exists(localFile))
                    {
                        File.Delete(localFile);
                    }

                    Console.WriteLine("Example completed successfully!");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error: {ex.Message}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}

