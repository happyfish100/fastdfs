// ============================================================================
// FastDFS C# Client - Advanced Metadata Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates advanced metadata operations in FastDFS, including
// complex metadata scenarios, metadata validation, metadata search patterns,
// metadata-driven workflows, and performance considerations. It shows how to
// effectively use metadata for building sophisticated file management systems.
//
// Advanced metadata operations enable applications to implement rich file
// management features such as tagging, categorization, search, workflow
// automation, and intelligent file processing based on metadata attributes.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating advanced metadata operations in FastDFS.
    /// 
    /// This example shows:
    /// - Complex metadata scenarios and patterns
    /// - Metadata validation and verification
    /// - Metadata search and filtering patterns
    /// - Metadata-driven workflow automation
    /// - Performance considerations for metadata operations
    /// - Best practices for advanced metadata usage
    /// 
    /// Advanced metadata patterns demonstrated:
    /// 1. Complex metadata structures and hierarchies
    /// 2. Metadata validation and schema enforcement
    /// 3. Metadata search and filtering
    /// 4. Metadata-driven processing workflows
    /// 5. Performance optimization for metadata operations
    /// 6. Metadata versioning and migration
    /// 7. Metadata indexing and caching
    /// </summary>
    class AdvancedMetadataExample
    {
        /// <summary>
        /// Main entry point for the advanced metadata example.
        /// 
        /// This method demonstrates various advanced metadata patterns through
        /// a series of examples, each showing different aspects of advanced
        /// metadata operations in FastDFS.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Advanced Metadata Example");
            Console.WriteLine("================================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates advanced metadata operations,");
            Console.WriteLine("including complex scenarios, validation, search, and workflows.");
            Console.WriteLine();

            // ====================================================================
            // Step 1: Create Client Configuration
            // ====================================================================
            // 
            // The configuration specifies tracker server addresses, timeouts,
            // connection pool settings, and other operational parameters.
            // For advanced metadata operations, standard configuration is sufficient.
            // ====================================================================

            var config = new FastDFSClientConfig
            {
                // Specify tracker server addresses
                // Tracker servers coordinate file storage and retrieval operations
                TrackerAddresses = new[]
                {
                    "192.168.1.100:22122",  // Primary tracker server
                    "192.168.1.101:22122"   // Secondary tracker server (for redundancy)
                },

                // Maximum number of connections per server
                // Standard connection pool size is sufficient for metadata operations
                MaxConnections = 100,

                // Connection timeout: maximum time to wait when establishing connections
                // Standard timeout is usually sufficient
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Network timeout: maximum time for read/write operations
                // Metadata operations are typically fast, so standard timeout works
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Idle timeout: time before idle connections are closed
                // Standard idle timeout works well for metadata operations
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Retry count: number of retry attempts for failed operations
                // Retry logic is important for metadata operations to handle
                // transient network errors
                RetryCount = 3
            };

            // ====================================================================
            // Step 2: Initialize the FastDFS Client
            // ====================================================================
            // 
            // The client manages connections to tracker and storage servers,
            // handles connection pooling, and provides a high-level API for
            // file operations including advanced metadata operations.
            // ====================================================================

            using (var client = new FastDFSClient(config))
            {
                try
                {
                    // ============================================================
                    // Example 1: Complex Metadata Scenarios
                    // ============================================================
                    // 
                    // This example demonstrates complex metadata scenarios with
                    // hierarchical structures, multiple value types, and rich
                    // metadata schemas. Complex metadata enables sophisticated
                    // file management and categorization.
                    // 
                    // Complex metadata patterns:
                    // - Hierarchical metadata structures
                    // - Multi-value metadata fields
                    // - Structured metadata values
                    // - Metadata relationships
                    // ============================================================

                    Console.WriteLine("Example 1: Complex Metadata Scenarios");
                    Console.WriteLine("=======================================");
                    Console.WriteLine();

                    // Create test files for complex metadata examples
                    Console.WriteLine("Creating test files for complex metadata scenarios...");
                    Console.WriteLine();

                    var complexTestFiles = new List<string>();

                    // Create different types of files with complex metadata
                    var imageFile = "complex_image.jpg";
                    var documentFile = "complex_document.pdf";
                    var videoFile = "complex_video.mp4";

                    await File.WriteAllTextAsync(imageFile, "Image file content");
                    await File.WriteAllTextAsync(documentFile, "Document file content");
                    await File.WriteAllTextAsync(videoFile, "Video file content");

                    complexTestFiles.AddRange(new[] { imageFile, documentFile, videoFile });

                    Console.WriteLine("Test files created.");
                    Console.WriteLine();

                    // Scenario 1: Hierarchical metadata structure
                    // Metadata organized in a hierarchical manner using dot notation
                    Console.WriteLine("Scenario 1: Hierarchical Metadata Structure");
                    Console.WriteLine("---------------------------------------------");
                    Console.WriteLine();

                    var hierarchicalMetadata = new Dictionary<string, string>
                    {
                        // Top-level metadata
                        { "type", "image" },
                        { "format", "jpeg" },
                        
                        // Hierarchical metadata using dot notation
                        { "image.width", "1920" },
                        { "image.height", "1080" },
                        { "image.resolution", "1920x1080" },
                        { "image.color_space", "RGB" },
                        { "image.bit_depth", "24" },
                        
                        // Metadata categories
                        { "category.primary", "photography" },
                        { "category.secondary", "landscape" },
                        { "category.tags", "nature,outdoor,mountain" },
                        
                        // Processing metadata
                        { "processing.software", "PhotoEditor v2.0" },
                        { "processing.date", DateTime.UtcNow.ToString("yyyy-MM-dd") },
                        { "processing.operations", "crop,resize,enhance" },
                        
                        // Ownership metadata
                        { "owner.name", "John Doe" },
                        { "owner.email", "john.doe@example.com" },
                        { "owner.organization", "Example Corp" },
                        
                        // Version metadata
                        { "version.number", "1.0" },
                        { "version.created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") },
                        { "version.modified", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                    };

                    Console.WriteLine("Uploading file with hierarchical metadata...");
                    var imageFileId = await client.UploadFileAsync(imageFile, hierarchicalMetadata);
                    Console.WriteLine($"File uploaded: {imageFileId}");
                    Console.WriteLine();

                    // Retrieve and display hierarchical metadata
                    Console.WriteLine("Retrieved hierarchical metadata:");
                    var retrievedHierarchical = await client.GetMetadataAsync(imageFileId);
                    foreach (var kvp in retrievedHierarchical.OrderBy(k => k.Key))
                    {
                        Console.WriteLine($"  {kvp.Key}: {kvp.Value}");
                    }
                    Console.WriteLine();

                    // Scenario 2: Multi-value metadata using delimiters
                    // Store multiple values in a single metadata field using delimiters
                    Console.WriteLine("Scenario 2: Multi-Value Metadata");
                    Console.WriteLine("----------------------------------");
                    Console.WriteLine();

                    var multiValueMetadata = new Dictionary<string, string>
                    {
                        { "type", "document" },
                        { "format", "pdf" },
                        
                        // Multi-value fields using comma delimiter
                        { "tags", "important,confidential,legal,contract" },
                        { "categories", "legal,finance,hr" },
                        { "keywords", "agreement,terms,conditions,liability" },
                        
                        // Multi-value fields using semicolon delimiter
                        { "authors", "John Doe;Jane Smith;Bob Johnson" },
                        { "reviewers", "Alice Brown;Charlie Wilson" },
                        
                        // Multi-value fields using pipe delimiter
                        { "departments", "Legal|Finance|HR" },
                        { "access_levels", "internal|confidential|restricted" },
                        
                        // Structured multi-value data
                        { "file_history", "2025-01-01:created|2025-01-15:modified|2025-01-20:reviewed" },
                        { "related_files", "doc_001.pdf|doc_002.pdf|doc_003.pdf" }
                    };

                    Console.WriteLine("Uploading file with multi-value metadata...");
                    var documentFileId = await client.UploadFileAsync(documentFile, multiValueMetadata);
                    Console.WriteLine($"File uploaded: {documentFileId}");
                    Console.WriteLine();

                    // Retrieve and parse multi-value metadata
                    Console.WriteLine("Retrieved multi-value metadata:");
                    var retrievedMultiValue = await client.GetMetadataAsync(documentFileId);
                    foreach (var kvp in retrievedMultiValue)
                    {
                        Console.WriteLine($"  {kvp.Key}: {kvp.Value}");
                        
                        // Parse multi-value fields
                        if (kvp.Key == "tags")
                        {
                            var tags = kvp.Value.Split(',').Select(t => t.Trim()).ToList();
                            Console.WriteLine($"    Parsed tags ({tags.Count}): {string.Join(", ", tags)}");
                        }
                        else if (kvp.Key == "authors")
                        {
                            var authors = kvp.Value.Split(';').Select(a => a.Trim()).ToList();
                            Console.WriteLine($"    Parsed authors ({authors.Count}): {string.Join(", ", authors)}");
                        }
                    }
                    Console.WriteLine();

                    // Scenario 3: Structured metadata with JSON-like values
                    // Store structured data in metadata values
                    Console.WriteLine("Scenario 3: Structured Metadata Values");
                    Console.WriteLine("----------------------------------------");
                    Console.WriteLine();

                    var structuredMetadata = new Dictionary<string, string>
                    {
                        { "type", "video" },
                        { "format", "mp4" },
                        
                        // Structured metadata as delimited strings
                        { "video.resolution", "1920x1080" },
                        { "video.duration", "120" },
                        { "video.fps", "30" },
                        { "video.codec", "h264" },
                        { "video.bitrate", "5000" },
                        
                        // Complex structured data
                        { "metadata.schema_version", "2.0" },
                        { "metadata.created_by", "VideoProcessor" },
                        { "metadata.creation_timestamp", DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ") },
                        
                        // Relationship metadata
                        { "relationships.parent", "project_001" },
                        { "relationships.children", "video_001_thumb.mp4,video_001_preview.mp4" },
                        { "relationships.related", "audio_001.mp3,script_001.txt" },
                        
                        // Workflow metadata
                        { "workflow.stage", "processing" },
                        { "workflow.status", "in_progress" },
                        { "workflow.steps", "uploaded|transcoding|quality_check|published" },
                        { "workflow.current_step", "transcoding" }
                    };

                    Console.WriteLine("Uploading file with structured metadata...");
                    var videoFileId = await client.UploadFileAsync(videoFile, structuredMetadata);
                    Console.WriteLine($"File uploaded: {videoFileId}");
                    Console.WriteLine();

                    // Retrieve and analyze structured metadata
                    Console.WriteLine("Retrieved structured metadata:");
                    var retrievedStructured = await client.GetMetadataAsync(videoFileId);
                    
                    // Group metadata by prefix for better organization
                    var groupedMetadata = retrievedStructured
                        .GroupBy(kvp => kvp.Key.Contains('.') ? kvp.Key.Split('.')[0] : "root")
                        .OrderBy(g => g.Key);

                    foreach (var group in groupedMetadata)
                    {
                        Console.WriteLine($"  [{group.Key}]");
                        foreach (var kvp in group)
                        {
                            Console.WriteLine($"    {kvp.Key}: {kvp.Value}");
                        }
                    }
                    Console.WriteLine();

                    // ============================================================
                    // Example 2: Metadata Validation
                    // ============================================================
                    // 
                    // This example demonstrates metadata validation patterns,
                    // including schema validation, value validation, and
                    // constraint checking. Metadata validation ensures data
                    // quality and consistency.
                    // 
                    // Validation patterns:
                    // - Schema validation
                    // - Value type validation
                    // - Constraint validation
                    // - Required field validation
                    // ============================================================

                    Console.WriteLine("Example 2: Metadata Validation");
                    Console.WriteLine("=================================");
                    Console.WriteLine();

                    // Define metadata schema for validation
                    // In a real application, this would be a formal schema definition
                    Console.WriteLine("Defining metadata schema for validation...");
                    Console.WriteLine();

                    var metadataSchema = new MetadataSchema
                    {
                        RequiredFields = new[] { "type", "format", "created_date" },
                        FieldTypes = new Dictionary<string, MetadataFieldType>
                        {
                            { "type", MetadataFieldType.String },
                            { "format", MetadataFieldType.String },
                            { "size", MetadataFieldType.Number },
                            { "created_date", MetadataFieldType.DateTime },
                            { "tags", MetadataFieldType.StringArray },
                            { "rating", MetadataFieldType.Number }
                        },
                        FieldConstraints = new Dictionary<string, MetadataConstraint>
                        {
                            { "type", new MetadataConstraint { AllowedValues = new[] { "image", "document", "video", "audio" } } },
                            { "size", new MetadataConstraint { MinValue = 0, MaxValue = 104857600 } }, // 0-100MB
                            { "rating", new MetadataConstraint { MinValue = 1, MaxValue = 5 } }
                        }
                    };

                    Console.WriteLine("Metadata schema defined:");
                    Console.WriteLine($"  Required fields: {string.Join(", ", metadataSchema.RequiredFields)}");
                    Console.WriteLine($"  Field types: {metadataSchema.FieldTypes.Count} fields");
                    Console.WriteLine($"  Field constraints: {metadataSchema.FieldConstraints.Count} fields");
                    Console.WriteLine();

                    // Create test file for validation
                    var validationTestFile = "validation_test.txt";
                    await File.WriteAllTextAsync(validationTestFile, "Validation test file content");

                    // Test case 1: Valid metadata
                    Console.WriteLine("Test Case 1: Valid Metadata");
                    Console.WriteLine("----------------------------");
                    Console.WriteLine();

                    var validMetadata = new Dictionary<string, string>
                    {
                        { "type", "document" },
                        { "format", "txt" },
                        { "size", "1024" },
                        { "created_date", DateTime.UtcNow.ToString("yyyy-MM-dd") },
                        { "tags", "test,validation" },
                        { "rating", "4" }
                    };

                    var validationResult1 = ValidateMetadata(validMetadata, metadataSchema);
                    Console.WriteLine($"Validation result: {(validationResult1.IsValid ? "VALID" : "INVALID")}");
                    if (!validationResult1.IsValid)
                    {
                        Console.WriteLine("Validation errors:");
                        foreach (var error in validationResult1.Errors)
                        {
                            Console.WriteLine($"  - {error}");
                        }
                    }
                    Console.WriteLine();

                    // Test case 2: Missing required fields
                    Console.WriteLine("Test Case 2: Missing Required Fields");
                    Console.WriteLine("--------------------------------------");
                    Console.WriteLine();

                    var invalidMetadata1 = new Dictionary<string, string>
                    {
                        { "type", "document" }
                        // Missing required fields: format, created_date
                    };

                    var validationResult2 = ValidateMetadata(invalidMetadata1, metadataSchema);
                    Console.WriteLine($"Validation result: {(validationResult2.IsValid ? "VALID" : "INVALID")}");
                    if (!validationResult2.IsValid)
                    {
                        Console.WriteLine("Validation errors:");
                        foreach (var error in validationResult2.Errors)
                        {
                            Console.WriteLine($"  - {error}");
                        }
                    }
                    Console.WriteLine();

                    // Test case 3: Invalid field values
                    Console.WriteLine("Test Case 3: Invalid Field Values");
                    Console.WriteLine("-----------------------------------");
                    Console.WriteLine();

                    var invalidMetadata2 = new Dictionary<string, string>
                    {
                        { "type", "invalid_type" },  // Not in allowed values
                        { "format", "txt" },
                        { "size", "-100" },  // Negative value
                        { "created_date", DateTime.UtcNow.ToString("yyyy-MM-dd") },
                        { "rating", "10" }  // Exceeds max value
                    };

                    var validationResult3 = ValidateMetadata(invalidMetadata2, metadataSchema);
                    Console.WriteLine($"Validation result: {(validationResult3.IsValid ? "VALID" : "INVALID")}");
                    if (!validationResult3.IsValid)
                    {
                        Console.WriteLine("Validation errors:");
                        foreach (var error in validationResult3.Errors)
                        {
                            Console.WriteLine($"  - {error}");
                        }
                    }
                    Console.WriteLine();

                    // Upload file with validated metadata
                    Console.WriteLine("Uploading file with validated metadata...");
                    var validatedFileId = await client.UploadFileAsync(validationTestFile, validMetadata);
                    Console.WriteLine($"File uploaded: {validatedFileId}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 3: Metadata Search Patterns
                    // ============================================================
                    // 
                    // This example demonstrates metadata search and filtering
                    // patterns. While FastDFS doesn't provide built-in search,
                    // applications can implement search by retrieving metadata
                    // and filtering in memory or using external indexing.
                    // 
                    // Search patterns:
                    // - Exact match search
                    // - Partial match search
                    // - Range search
                    // - Multi-criteria search
                    // - Tag-based search
                    // ============================================================

                    Console.WriteLine("Example 3: Metadata Search Patterns");
                    Console.WriteLine("=====================================");
                    Console.WriteLine();

                    // Create multiple files with different metadata for search examples
                    Console.WriteLine("Creating files with diverse metadata for search examples...");
                    Console.WriteLine();

                    var searchTestFiles = new List<(string FileName, Dictionary<string, string> Metadata)>();

                    // File 1: Image with photography metadata
                    searchTestFiles.Add((
                        "search_image_1.jpg",
                        new Dictionary<string, string>
                        {
                            { "type", "image" },
                            { "format", "jpeg" },
                            { "category", "photography" },
                            { "tags", "nature,landscape,mountain" },
                            { "location", "Alps" },
                            { "photographer", "John Doe" },
                            { "date", "2025-01-15" },
                            { "rating", "5" }
                        }
                    ));

                    // File 2: Document with business metadata
                    searchTestFiles.Add((
                        "search_doc_1.pdf",
                        new Dictionary<string, string>
                        {
                            { "type", "document" },
                            { "format", "pdf" },
                            { "category", "business" },
                            { "tags", "report,quarterly,financial" },
                            { "department", "Finance" },
                            { "author", "Jane Smith" },
                            { "date", "2025-01-20" },
                            { "rating", "4" }
                        }
                    ));

                    // File 3: Video with entertainment metadata
                    searchTestFiles.Add((
                        "search_video_1.mp4",
                        new Dictionary<string, string>
                        {
                            { "type", "video" },
                            { "format", "mp4" },
                            { "category", "entertainment" },
                            { "tags", "movie,action,adventure" },
                            { "genre", "Action" },
                            { "director", "Bob Johnson" },
                            { "date", "2025-01-25" },
                            { "rating", "5" }
                        }
                    ));

                    // File 4: Image with portrait metadata
                    searchTestFiles.Add((
                        "search_image_2.jpg",
                        new Dictionary<string, string>
                        {
                            { "type", "image" },
                            { "format", "jpeg" },
                            { "category", "photography" },
                            { "tags", "portrait,people,studio" },
                            { "location", "Studio" },
                            { "photographer", "Alice Brown" },
                            { "date", "2025-02-01" },
                            { "rating", "4" }
                        }
                    ));

                    // Upload files with metadata
                    var searchFileIds = new List<(string FileId, Dictionary<string, string> Metadata)>();

                    foreach (var (fileName, metadata) in searchTestFiles)
                    {
                        await File.WriteAllTextAsync(fileName, $"Content for {fileName}");
                        var fileId = await client.UploadFileAsync(fileName, metadata);
                        searchFileIds.Add((fileId, metadata));
                        Console.WriteLine($"  Uploaded: {fileName} -> {fileId}");
                    }

                    Console.WriteLine();
                    Console.WriteLine($"Uploaded {searchFileIds.Count} files with metadata");
                    Console.WriteLine();

                    // Search Pattern 1: Exact match search
                    Console.WriteLine("Search Pattern 1: Exact Match Search");
                    Console.WriteLine("-------------------------------------");
                    Console.WriteLine();

                    var searchType = "image";
                    var exactMatchResults = searchFileIds
                        .Where(f => f.Metadata.ContainsKey("type") && f.Metadata["type"] == searchType)
                        .ToList();

                    Console.WriteLine($"Searching for type='{searchType}':");
                    Console.WriteLine($"  Found {exactMatchResults.Count} files");
                    foreach (var result in exactMatchResults)
                    {
                        Console.WriteLine($"    {result.FileId}");
                    }
                    Console.WriteLine();

                    // Search Pattern 2: Partial match search (tag search)
                    Console.WriteLine("Search Pattern 2: Partial Match Search (Tags)");
                    Console.WriteLine("------------------------------------------------");
                    Console.WriteLine();

                    var searchTag = "nature";
                    var tagMatchResults = searchFileIds
                        .Where(f => f.Metadata.ContainsKey("tags") && 
                                   f.Metadata["tags"].Contains(searchTag))
                        .ToList();

                    Console.WriteLine($"Searching for tag='{searchTag}':");
                    Console.WriteLine($"  Found {tagMatchResults.Count} files");
                    foreach (var result in tagMatchResults)
                    {
                        var tags = result.Metadata["tags"];
                        Console.WriteLine($"    {result.FileId} (tags: {tags})");
                    }
                    Console.WriteLine();

                    // Search Pattern 3: Multi-criteria search
                    Console.WriteLine("Search Pattern 3: Multi-Criteria Search");
                    Console.WriteLine("----------------------------------------");
                    Console.WriteLine();

                    var multiCriteriaResults = searchFileIds
                        .Where(f => f.Metadata.ContainsKey("type") && f.Metadata["type"] == "image" &&
                                   f.Metadata.ContainsKey("category") && f.Metadata["category"] == "photography" &&
                                   f.Metadata.ContainsKey("rating") && int.Parse(f.Metadata["rating"]) >= 4)
                        .ToList();

                    Console.WriteLine("Searching for: type='image' AND category='photography' AND rating>=4");
                    Console.WriteLine($"  Found {multiCriteriaResults.Count} files");
                    foreach (var result in multiCriteriaResults)
                    {
                        Console.WriteLine($"    {result.FileId}");
                        Console.WriteLine($"      Type: {result.Metadata["type"]}");
                        Console.WriteLine($"      Category: {result.Metadata["category"]}");
                        Console.WriteLine($"      Rating: {result.Metadata["rating"]}");
                    }
                    Console.WriteLine();

                    // Search Pattern 4: Range search
                    Console.WriteLine("Search Pattern 4: Range Search");
                    Console.WriteLine("-------------------------------");
                    Console.WriteLine();

                    var minRating = 4;
                    var maxRating = 5;
                    var rangeResults = searchFileIds
                        .Where(f => f.Metadata.ContainsKey("rating") &&
                                   int.TryParse(f.Metadata["rating"], out int rating) &&
                                   rating >= minRating && rating <= maxRating)
                        .ToList();

                    Console.WriteLine($"Searching for rating between {minRating} and {maxRating}:");
                    Console.WriteLine($"  Found {rangeResults.Count} files");
                    foreach (var result in rangeResults)
                    {
                        Console.WriteLine($"    {result.FileId} (rating: {result.Metadata["rating"]})");
                    }
                    Console.WriteLine();

                    // Search Pattern 5: Retrieve and search metadata from FastDFS
                    // In a real scenario, you would retrieve metadata from FastDFS
                    // and then filter in memory or use an external search index
                    Console.WriteLine("Search Pattern 5: Retrieve and Filter from FastDFS");
                    Console.WriteLine("----------------------------------------------------");
                    Console.WriteLine();

                    var retrievedMetadataList = new List<(string FileId, Dictionary<string, string> Metadata)>();

                    foreach (var (fileId, _) in searchFileIds)
                    {
                        try
                        {
                            var metadata = await client.GetMetadataAsync(fileId);
                            retrievedMetadataList.Add((fileId, metadata));
                        }
                        catch
                        {
                            // Skip files without metadata
                        }
                    }

                    // Filter retrieved metadata
                    var filteredResults = retrievedMetadataList
                        .Where(f => f.Metadata.ContainsKey("type") && f.Metadata["type"] == "image")
                        .ToList();

                    Console.WriteLine($"Retrieved metadata for {retrievedMetadataList.Count} files");
                    Console.WriteLine($"Filtered results: {filteredResults.Count} files match criteria");
                    Console.WriteLine();

                    // ============================================================
                    // Example 4: Metadata-Driven Workflows
                    // ============================================================
                    // 
                    // This example demonstrates metadata-driven workflows where
                    // file processing and operations are determined by metadata
                    // values. This enables automated processing, routing, and
                    // workflow automation based on file characteristics.
                    // 
                    // Workflow patterns:
                    // - Automated processing based on metadata
                    // - Workflow routing
                    // - Status tracking
                    // - Conditional processing
                    // ============================================================

                    Console.WriteLine("Example 4: Metadata-Driven Workflows");
                    Console.WriteLine("======================================");
                    Console.WriteLine();

                    // Create files for workflow examples
                    Console.WriteLine("Creating files for workflow examples...");
                    Console.WriteLine();

                    var workflowTestFile = "workflow_test.txt";
                    await File.WriteAllTextAsync(workflowTestFile, "Workflow test file content");

                    // Upload file with workflow metadata
                    var workflowMetadata = new Dictionary<string, string>
                    {
                        { "type", "document" },
                        { "format", "txt" },
                        { "workflow.stage", "uploaded" },
                        { "workflow.status", "pending" },
                        { "workflow.priority", "high" },
                        { "workflow.assigned_to", "processor_01" },
                        { "workflow.required_actions", "validate,transform,notify" }
                    };

                    Console.WriteLine("Uploading file with workflow metadata...");
                    var workflowFileId = await client.UploadFileAsync(workflowTestFile, workflowMetadata);
                    Console.WriteLine($"File uploaded: {workflowFileId}");
                    Console.WriteLine();

                    // Workflow Pattern 1: Stage-based processing
                    Console.WriteLine("Workflow Pattern 1: Stage-Based Processing");
                    Console.WriteLine("--------------------------------------------");
                    Console.WriteLine();

                    var workflowMetadata1 = await client.GetMetadataAsync(workflowFileId);
                    var currentStage = workflowMetadata1.ContainsKey("workflow.stage") 
                        ? workflowMetadata1["workflow.stage"] 
                        : "unknown";

                    Console.WriteLine($"Current workflow stage: {currentStage}");
                    Console.WriteLine();

                    // Process based on stage
                    var nextStage = ProcessWorkflowStage(currentStage, workflowMetadata1);
                    Console.WriteLine($"Processing stage: {currentStage} -> {nextStage}");

                    // Update metadata to reflect workflow progress
                    var updatedWorkflowMetadata = new Dictionary<string, string>
                    {
                        { "workflow.stage", nextStage },
                        { "workflow.status", "processing" },
                        { "workflow.last_updated", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                    };

                    await client.SetMetadataAsync(workflowFileId, updatedWorkflowMetadata, MetadataFlag.Merge);
                    Console.WriteLine($"Workflow metadata updated: stage={nextStage}, status=processing");
                    Console.WriteLine();

                    // Workflow Pattern 2: Priority-based routing
                    Console.WriteLine("Workflow Pattern 2: Priority-Based Routing");
                    Console.WriteLine("--------------------------------------------");
                    Console.WriteLine();

                    var workflowMetadata2 = await client.GetMetadataAsync(workflowFileId);
                    var priority = workflowMetadata2.ContainsKey("workflow.priority")
                        ? workflowMetadata2["workflow.priority"]
                        : "normal";

                    Console.WriteLine($"File priority: {priority}");

                    // Route based on priority
                    var processor = RouteByPriority(priority);
                    Console.WriteLine($"Routed to processor: {processor}");
                    Console.WriteLine();

                    // Workflow Pattern 3: Action-based processing
                    Console.WriteLine("Workflow Pattern 3: Action-Based Processing");
                    Console.WriteLine("----------------------------------------------");
                    Console.WriteLine();

                    var workflowMetadata3 = await client.GetMetadataAsync(workflowFileId);
                    var requiredActions = workflowMetadata3.ContainsKey("workflow.required_actions")
                        ? workflowMetadata3["workflow.required_actions"].Split(',')
                        : new string[0];

                    Console.WriteLine($"Required actions: {string.Join(", ", requiredActions)}");
                    Console.WriteLine();

                    var completedActions = new List<string>();

                    foreach (var action in requiredActions)
                    {
                        var actionName = action.Trim();
                        Console.WriteLine($"  Processing action: {actionName}");

                        // Simulate action processing
                        await Task.Delay(100);  // Simulate processing time

                        completedActions.Add(actionName);
                        Console.WriteLine($"    Action completed: {actionName}");

                        // Update metadata with completed action
                        var actionMetadata = new Dictionary<string, string>
                        {
                            { $"workflow.action.{actionName}.status", "completed" },
                            { $"workflow.action.{actionName}.completed_at", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                        };

                        await client.SetMetadataAsync(workflowFileId, actionMetadata, MetadataFlag.Merge);
                    }

                    Console.WriteLine();
                    Console.WriteLine($"Completed {completedActions.Count} actions: {string.Join(", ", completedActions)}");
                    Console.WriteLine();

                    // Workflow Pattern 4: Status tracking
                    Console.WriteLine("Workflow Pattern 4: Status Tracking");
                    Console.WriteLine("-------------------------------------");
                    Console.WriteLine();

                    // Update final status
                    var finalStatusMetadata = new Dictionary<string, string>
                    {
                        { "workflow.stage", "completed" },
                        { "workflow.status", "success" },
                        { "workflow.completed_at", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") },
                        { "workflow.completed_actions", string.Join(",", completedActions) }
                    };

                    await client.SetMetadataAsync(workflowFileId, finalStatusMetadata, MetadataFlag.Merge);

                    var finalMetadata = await client.GetMetadataAsync(workflowFileId);
                    Console.WriteLine("Final workflow status:");
                    Console.WriteLine($"  Stage: {finalMetadata.GetValueOrDefault("workflow.stage", "unknown")}");
                    Console.WriteLine($"  Status: {finalMetadata.GetValueOrDefault("workflow.status", "unknown")}");
                    Console.WriteLine($"  Completed at: {finalMetadata.GetValueOrDefault("workflow.completed_at", "unknown")}");
                    Console.WriteLine();

                    // ============================================================
                    // Example 5: Performance Considerations
                    // ============================================================
                    // 
                    // This example demonstrates performance considerations for
                    // metadata operations, including caching, batching, and
                    // optimization techniques. Performance optimization is
                    // important for applications that work with many files
                    // and metadata operations.
                    // 
                    // Performance patterns:
                    // - Metadata caching
                    // - Batch metadata operations
                    // - Minimize metadata operations
                    // - Optimize metadata size
                    // ============================================================

                    Console.WriteLine("Example 5: Performance Considerations");
                    Console.WriteLine("=======================================");
                    Console.WriteLine();

                    // Create files for performance testing
                    Console.WriteLine("Creating files for performance testing...");
                    Console.WriteLine();

                    var perfTestFiles = new List<string>();
                    for (int i = 1; i <= 20; i++)
                    {
                        var perfTestFile = $"perf_test_{i}.txt";
                        await File.WriteAllTextAsync(perfTestFile, $"Performance test file {i}");
                        perfTestFiles.Add(perfTestFile);
                    }

                    Console.WriteLine($"Created {perfTestFiles.Count} test files");
                    Console.WriteLine();

                    // Performance Pattern 1: Metadata caching
                    Console.WriteLine("Performance Pattern 1: Metadata Caching");
                    Console.WriteLine("----------------------------------------");
                    Console.WriteLine();

                    var metadataCache = new Dictionary<string, Dictionary<string, string>>();

                    // Upload files with metadata
                    var perfFileIds = new List<string>();
                    foreach (var perfTestFile in perfTestFiles)
                    {
                        var perfMetadata = new Dictionary<string, string>
                        {
                            { "type", "document" },
                            { "index", perfTestFile.Replace("perf_test_", "").Replace(".txt", "") },
                            { "created", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") }
                        };

                        var perfFileId = await client.UploadFileAsync(perfTestFile, perfMetadata);
                        perfFileIds.Add(perfFileId);

                        // Cache metadata immediately after upload
                        metadataCache[perfFileId] = perfMetadata;
                    }

                    Console.WriteLine($"Uploaded {perfFileIds.Count} files and cached metadata");
                    Console.WriteLine();

                    // Measure performance: Without cache (retrieve from FastDFS)
                    Console.WriteLine("Measuring performance: Retrieving metadata from FastDFS (no cache)...");
                    var noCacheStopwatch = System.Diagnostics.Stopwatch.StartNew();

                    foreach (var fileId in perfFileIds.Take(10))
                    {
                        var metadata = await client.GetMetadataAsync(fileId);
                    }

                    noCacheStopwatch.Stop();
                    var noCacheTime = noCacheStopwatch.ElapsedMilliseconds;

                    Console.WriteLine($"  Time for 10 retrievals: {noCacheTime} ms");
                    Console.WriteLine($"  Average per retrieval: {noCacheTime / 10.0:F2} ms");
                    Console.WriteLine();

                    // Measure performance: With cache (retrieve from memory)
                    Console.WriteLine("Measuring performance: Retrieving metadata from cache...");
                    var cacheStopwatch = System.Diagnostics.Stopwatch.StartNew();

                    foreach (var fileId in perfFileIds.Take(10))
                    {
                        var metadata = metadataCache.ContainsKey(fileId) 
                            ? metadataCache[fileId] 
                            : await client.GetMetadataAsync(fileId);
                    }

                    cacheStopwatch.Stop();
                    var cacheTime = cacheStopwatch.ElapsedMilliseconds;

                    Console.WriteLine($"  Time for 10 retrievals: {cacheTime} ms");
                    Console.WriteLine($"  Average per retrieval: {cacheTime / 10.0:F2} ms");
                    Console.WriteLine();

                    var speedup = noCacheTime / (double)cacheTime;
                    Console.WriteLine($"Performance improvement: {speedup:F2}x faster with caching");
                    Console.WriteLine();

                    // Performance Pattern 2: Batch metadata operations
                    Console.WriteLine("Performance Pattern 2: Batch Metadata Operations");
                    Console.WriteLine("--------------------------------------------------");
                    Console.WriteLine();

                    // Update metadata for multiple files concurrently
                    Console.WriteLine("Updating metadata for multiple files concurrently...");
                    var batchUpdateStopwatch = System.Diagnostics.Stopwatch.StartNew();

                    var batchUpdateTasks = perfFileIds.Take(10).Select(async fileId =>
                    {
                        var updateMetadata = new Dictionary<string, string>
                        {
                            { "updated", DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") },
                            { "batch_update", "true" }
                        };

                        await client.SetMetadataAsync(fileId, updateMetadata, MetadataFlag.Merge);
                    }).ToArray();

                    await Task.WhenAll(batchUpdateTasks);

                    batchUpdateStopwatch.Stop();
                    var batchUpdateTime = batchUpdateStopwatch.ElapsedMilliseconds;

                    Console.WriteLine($"  Time for 10 concurrent updates: {batchUpdateTime} ms");
                    Console.WriteLine($"  Average per update: {batchUpdateTime / 10.0:F2} ms");
                    Console.WriteLine();

                    // Performance Pattern 3: Optimize metadata size
                    Console.WriteLine("Performance Pattern 3: Optimize Metadata Size");
                    Console.WriteLine("-----------------------------------------------");
                    Console.WriteLine();

                    // Compare minimal vs verbose metadata
                    var minimalMetadata = new Dictionary<string, string>
                    {
                        { "t", "img" },  // type -> t
                        { "f", "jpg" },  // format -> f
                        { "c", "photo" } // category -> c
                    };

                    var verboseMetadata = new Dictionary<string, string>
                        {
                            { "type", "image" },
                            { "format", "jpeg" },
                            { "category", "photography" },
                            { "description", "This is a detailed description of the image file" },
                            { "long_description", "This is a very long and detailed description that contains a lot of information about the image file, its contents, and various attributes that might be useful for searching and categorization purposes." }
                        };

                    var minimalSize = CalculateMetadataSize(minimalMetadata);
                    var verboseSize = CalculateMetadataSize(verboseMetadata);

                    Console.WriteLine("Metadata size comparison:");
                    Console.WriteLine($"  Minimal metadata: {minimalSize} bytes ({minimalMetadata.Count} fields)");
                    Console.WriteLine($"  Verbose metadata: {verboseSize} bytes ({verboseMetadata.Count} fields)");
                    Console.WriteLine($"  Size difference: {verboseSize - minimalSize} bytes ({((verboseSize - minimalSize) * 100.0 / minimalSize):F1}% larger)");
                    Console.WriteLine();
                    Console.WriteLine("Optimization recommendations:");
                    Console.WriteLine("  - Use short key names when possible");
                    Console.WriteLine("  - Avoid redundant or verbose values");
                    Console.WriteLine("  - Store detailed information in separate storage if needed");
                    Console.WriteLine("  - Balance between readability and size");
                    Console.WriteLine();

                    // ============================================================
                    // Best Practices Summary
                    // ============================================================
                    // 
                    // This section summarizes best practices for advanced metadata
                    // operations in FastDFS applications, based on the examples above.
                    // ============================================================

                    Console.WriteLine("Best Practices for Advanced Metadata Operations");
                    Console.WriteLine("================================================");
                    Console.WriteLine();
                    Console.WriteLine("1. Complex Metadata Design:");
                    Console.WriteLine("   - Use hierarchical structures with dot notation");
                    Console.WriteLine("   - Implement multi-value fields with delimiters");
                    Console.WriteLine("   - Structure metadata for your use case");
                    Console.WriteLine("   - Document metadata schema and conventions");
                    Console.WriteLine("   - Consider metadata versioning");
                    Console.WriteLine();
                    Console.WriteLine("2. Metadata Validation:");
                    Console.WriteLine("   - Implement schema validation");
                    Console.WriteLine("   - Validate field types and constraints");
                    Console.WriteLine("   - Check required fields");
                    Console.WriteLine("   - Validate value ranges and formats");
                    Console.WriteLine("   - Provide clear validation error messages");
                    Console.WriteLine();
                    Console.WriteLine("3. Metadata Search:");
                    Console.WriteLine("   - Implement in-memory filtering for small datasets");
                    Console.WriteLine("   - Use external search indexes for large datasets");
                    Console.WriteLine("   - Cache frequently searched metadata");
                    Console.WriteLine("   - Optimize search queries");
                    Console.WriteLine("   - Consider metadata indexing strategies");
                    Console.WriteLine();
                    Console.WriteLine("4. Metadata-Driven Workflows:");
                    Console.WriteLine("   - Use metadata for workflow state tracking");
                    Console.WriteLine("   - Implement stage-based processing");
                    Console.WriteLine("   - Route files based on metadata");
                    Console.WriteLine("   - Track workflow progress in metadata");
                    Console.WriteLine("   - Update metadata as workflow progresses");
                    Console.WriteLine();
                    Console.WriteLine("5. Performance Optimization:");
                    Console.WriteLine("   - Cache metadata when appropriate");
                    Console.WriteLine("   - Batch metadata operations when possible");
                    Console.WriteLine("   - Minimize metadata size");
                    Console.WriteLine("   - Use short key names");
                    Console.WriteLine("   - Avoid redundant metadata");
                    Console.WriteLine();
                    Console.WriteLine("6. Metadata Size Management:");
                    Console.WriteLine("   - Keep metadata concise");
                    Console.WriteLine("   - Use abbreviations for common fields");
                    Console.WriteLine("   - Store detailed info separately if needed");
                    Console.WriteLine("   - Monitor metadata size");
                    Console.WriteLine("   - Balance between size and readability");
                    Console.WriteLine();
                    Console.WriteLine("7. Metadata Schema Design:");
                    Console.WriteLine("   - Define clear metadata schemas");
                    Console.WriteLine("   - Document field meanings and formats");
                    Console.WriteLine("   - Version metadata schemas");
                    Console.WriteLine("   - Plan for schema evolution");
                    Console.WriteLine("   - Maintain backward compatibility");
                    Console.WriteLine();
                    Console.WriteLine("8. Workflow Integration:");
                    Console.WriteLine("   - Use metadata for workflow state");
                    Console.WriteLine("   - Track processing stages");
                    Console.WriteLine("   - Implement status tracking");
                    Console.WriteLine("   - Enable conditional processing");
                    Console.WriteLine("   - Support workflow automation");
                    Console.WriteLine();
                    Console.WriteLine("9. Search and Filtering:");
                    Console.WriteLine("   - Implement efficient search patterns");
                    Console.WriteLine("   - Support multi-criteria searches");
                    Console.WriteLine("   - Enable tag-based filtering");
                    Console.WriteLine("   - Consider external search solutions");
                    Console.WriteLine("   - Optimize search performance");
                    Console.WriteLine();
                    Console.WriteLine("10. Best Practices Summary:");
                    Console.WriteLine("    - Design metadata schemas carefully");
                    Console.WriteLine("    - Implement validation and error handling");
                    Console.WriteLine("    - Optimize for performance and size");
                    Console.WriteLine("    - Use metadata for workflows and automation");
                    Console.WriteLine("    - Monitor and optimize based on usage");
                    Console.WriteLine();

                    // ============================================================
                    // Cleanup
                    // ============================================================
                    // 
                    // Clean up uploaded files and local test files
                    // ============================================================

                    Console.WriteLine("Cleaning up...");
                    Console.WriteLine();

                    // Collect all uploaded file IDs
                    var allFileIds = new List<string>
                    {
                        imageFileId,
                        documentFileId,
                        videoFileId,
                        validatedFileId,
                        workflowFileId
                    };

                    allFileIds.AddRange(searchFileIds.Select(f => f.FileId));
                    allFileIds.AddRange(perfFileIds);

                    Console.WriteLine($"Deleting {allFileIds.Count} uploaded files from FastDFS...");
                    var deleteTasks = allFileIds.Select(async fileId =>
                    {
                        try
                        {
                            await client.DeleteFileAsync(fileId);
                            return true;
                        }
                        catch
                        {
                            return false;
                        }
                    }).ToArray();

                    await Task.WhenAll(deleteTasks);
                    Console.WriteLine("Files deleted");
                    Console.WriteLine();

                    // Delete local test files
                    var allLocalFiles = new List<string>
                    {
                        imageFile, documentFile, videoFile,
                        validationTestFile, workflowTestFile
                    };

                    allLocalFiles.AddRange(searchTestFiles.Select(f => f.FileName));
                    allLocalFiles.AddRange(perfTestFiles);

                    Console.WriteLine("Deleting local test files...");
                    foreach (var fileName in allLocalFiles.Distinct())
                    {
                        try
                        {
                            if (File.Exists(fileName))
                            {
                                File.Delete(fileName);
                            }
                        }
                        catch
                        {
                            // Ignore deletion errors
                        }
                    }

                    Console.WriteLine("Cleanup completed.");
                    Console.WriteLine();
                    Console.WriteLine("All examples completed successfully!");
                }
                catch (Exception ex)
                {
                    // Handle unexpected errors
                    Console.WriteLine($"Unexpected Error: {ex.Message}");
                    Console.WriteLine($"Stack Trace: {ex.StackTrace}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }

        // ====================================================================
        // Helper Methods
        // ====================================================================

        /// <summary>
        /// Validates metadata against a schema definition.
        /// 
        /// This method performs comprehensive validation including required
        /// field checking, type validation, and constraint validation.
        /// </summary>
        /// <param name="metadata">
        /// The metadata dictionary to validate.
        /// </param>
        /// <param name="schema">
        /// The metadata schema to validate against.
        /// </param>
        /// <returns>
        /// A validation result containing validation status and error messages.
        /// </returns>
        static ValidationResult ValidateMetadata(Dictionary<string, string> metadata, MetadataSchema schema)
        {
            var errors = new List<string>();

            // Check required fields
            foreach (var requiredField in schema.RequiredFields)
            {
                if (!metadata.ContainsKey(requiredField) || string.IsNullOrEmpty(metadata[requiredField]))
                {
                    errors.Add($"Required field '{requiredField}' is missing or empty");
                }
            }

            // Validate field types and constraints
            foreach (var kvp in metadata)
            {
                var fieldName = kvp.Key;
                var fieldValue = kvp.Value;

                // Check field type
                if (schema.FieldTypes.ContainsKey(fieldName))
                {
                    var expectedType = schema.FieldTypes[fieldName];
                    var typeValid = ValidateFieldType(fieldValue, expectedType);

                    if (!typeValid)
                    {
                        errors.Add($"Field '{fieldName}' has invalid type (expected: {expectedType})");
                    }
                }

                // Check field constraints
                if (schema.FieldConstraints.ContainsKey(fieldName))
                {
                    var constraint = schema.FieldConstraints[fieldName];
                    var constraintValid = ValidateFieldConstraint(fieldValue, constraint);

                    if (!constraintValid.IsValid)
                    {
                        errors.Add($"Field '{fieldName}': {constraintValid.ErrorMessage}");
                    }
                }
            }

            return new ValidationResult
            {
                IsValid = errors.Count == 0,
                Errors = errors
            };
        }

        /// <summary>
        /// Validates a field value against its expected type.
        /// </summary>
        static bool ValidateFieldType(string value, MetadataFieldType expectedType)
        {
            switch (expectedType)
            {
                case MetadataFieldType.String:
                    return true;  // All values are strings in metadata
                case MetadataFieldType.Number:
                    return int.TryParse(value, out _) || long.TryParse(value, out _) || double.TryParse(value, out _);
                case MetadataFieldType.DateTime:
                    return DateTime.TryParse(value, out _);
                case MetadataFieldType.StringArray:
                    return true;  // Arrays are stored as delimited strings
                default:
                    return true;
            }
        }

        /// <summary>
        /// Validates a field value against its constraints.
        /// </summary>
        static (bool IsValid, string ErrorMessage) ValidateFieldConstraint(string value, MetadataConstraint constraint)
        {
            // Check allowed values
            if (constraint.AllowedValues != null && constraint.AllowedValues.Length > 0)
            {
                if (!constraint.AllowedValues.Contains(value))
                {
                    return (false, $"Value '{value}' is not in allowed values: {string.Join(", ", constraint.AllowedValues)}");
                }
            }

            // Check numeric range
            if (constraint.MinValue.HasValue || constraint.MaxValue.HasValue)
            {
                if (double.TryParse(value, out double numValue))
                {
                    if (constraint.MinValue.HasValue && numValue < constraint.MinValue.Value)
                    {
                        return (false, $"Value {numValue} is less than minimum {constraint.MinValue.Value}");
                    }

                    if (constraint.MaxValue.HasValue && numValue > constraint.MaxValue.Value)
                    {
                        return (false, $"Value {numValue} is greater than maximum {constraint.MaxValue.Value}");
                    }
                }
            }

            return (true, null);
        }

        /// <summary>
        /// Processes a workflow stage and returns the next stage.
        /// </summary>
        static string ProcessWorkflowStage(string currentStage, Dictionary<string, string> metadata)
        {
            // Simple workflow progression
            switch (currentStage.ToLower())
            {
                case "uploaded":
                    return "validating";
                case "validating":
                    return "processing";
                case "processing":
                    return "reviewing";
                case "reviewing":
                    return "completed";
                default:
                    return "unknown";
            }
        }

        /// <summary>
        /// Routes a file to a processor based on priority.
        /// </summary>
        static string RouteByPriority(string priority)
        {
            switch (priority.ToLower())
            {
                case "high":
                    return "high_priority_processor";
                case "medium":
                    return "medium_priority_processor";
                case "low":
                    return "low_priority_processor";
                default:
                    return "default_processor";
            }
        }

        /// <summary>
        /// Calculates the total size of metadata in bytes.
        /// </summary>
        static int CalculateMetadataSize(Dictionary<string, string> metadata)
        {
            int size = 0;
            foreach (var kvp in metadata)
            {
                size += Encoding.UTF8.GetByteCount(kvp.Key);
                size += Encoding.UTF8.GetByteCount(kvp.Value ?? "");
            }
            return size;
        }
    }

    // ====================================================================
    // Helper Classes
    // ====================================================================

    /// <summary>
    /// Represents a metadata schema for validation.
    /// 
    /// This class defines the structure and constraints for metadata,
    /// including required fields, field types, and validation rules.
    /// </summary>
    class MetadataSchema
    {
        /// <summary>
        /// Gets or sets the list of required metadata fields.
        /// </summary>
        public string[] RequiredFields { get; set; }

        /// <summary>
        /// Gets or sets the dictionary mapping field names to their types.
        /// </summary>
        public Dictionary<string, MetadataFieldType> FieldTypes { get; set; }

        /// <summary>
        /// Gets or sets the dictionary mapping field names to their constraints.
        /// </summary>
        public Dictionary<string, MetadataConstraint> FieldConstraints { get; set; }
    }

    /// <summary>
    /// Represents the type of a metadata field.
    /// </summary>
    enum MetadataFieldType
    {
        /// <summary>
        /// String type (default for metadata values).
        /// </summary>
        String,

        /// <summary>
        /// Numeric type (integer or decimal).
        /// </summary>
        Number,

        /// <summary>
        /// DateTime type.
        /// </summary>
        DateTime,

        /// <summary>
        /// String array type (stored as delimited string).
        /// </summary>
        StringArray
    }

    /// <summary>
    /// Represents constraints for a metadata field.
    /// 
    /// This class defines validation constraints such as allowed values,
    /// minimum and maximum values, and other validation rules.
    /// </summary>
    class MetadataConstraint
    {
        /// <summary>
        /// Gets or sets the allowed values for the field.
        /// </summary>
        public string[] AllowedValues { get; set; }

        /// <summary>
        /// Gets or sets the minimum value for numeric fields.
        /// </summary>
        public double? MinValue { get; set; }

        /// <summary>
        /// Gets or sets the maximum value for numeric fields.
        /// </summary>
        public double? MaxValue { get; set; }
    }

    /// <summary>
    /// Represents the result of metadata validation.
    /// 
    /// This class contains the validation status and any error messages
    /// that occurred during validation.
    /// </summary>
    class ValidationResult
    {
        /// <summary>
        /// Gets or sets a value indicating whether the metadata is valid.
        /// </summary>
        public bool IsValid { get; set; }

        /// <summary>
        /// Gets or sets the list of validation error messages.
        /// </summary>
        public List<string> Errors { get; set; } = new List<string>();
    }
}

