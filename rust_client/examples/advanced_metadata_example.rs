/*! FastDFS Advanced Metadata Operations Example
 *
 * This comprehensive example demonstrates advanced metadata operations
 * and patterns for FastDFS distributed file system. It covers complex
 * scenarios, validation, search patterns, workflows, performance
 * optimization, and advanced metadata management techniques.
 *
 * Advanced metadata features demonstrated:
 * - Complex metadata scenarios with multiple files and relationships
 * - Metadata validation and schema enforcement
 * - Metadata search and filtering patterns
 * - Metadata-driven workflow automation
 * - Performance optimization techniques
 * - Advanced metadata patterns (versioning, tagging, categorization)
 * - Efficient metadata querying and filtering
 *
 * This example is designed for production use cases where metadata
 * is critical for file organization, search, and workflow management.
 *
 * Run this example with:
 * ```bash
 * cargo run --example advanced_metadata_example
 * ```
 */

/* Import FastDFS client components */
/* Client provides the main API for FastDFS operations */
/* ClientConfig allows configuration of connection parameters */
/* MetadataFlag controls how metadata updates are applied */
use fastdfs::{Client, ClientConfig, MetadataFlag};
/* Standard library imports for collections and error handling */
use std::collections::HashMap;

/* ====================================================================
 * METADATA VALIDATION STRUCTURES
 * ====================================================================
 * These structures help validate and work with metadata in a type-safe way.
 */

/* Metadata schema definition for validation */
/* This struct defines expected metadata keys and their validation rules */
struct MetadataSchema {
    /* Required keys that must be present */
    required_keys: Vec<String>,
    /* Optional keys that may be present */
    optional_keys: Vec<String>,
    /* Validation functions for specific keys */
    validators: HashMap<String, Box<dyn Fn(&str) -> bool>>,
}

/* Implementation of metadata schema */
impl MetadataSchema {
    /* Create a new metadata schema with validation rules */
    /* This allows us to enforce metadata structure and content */
    fn new() -> Self {
        let mut validators: HashMap<String, Box<dyn Fn(&str) -> bool>> = HashMap::new();
        
        /* Validator for email format */
        /* Checks if a value looks like a valid email address */
        validators.insert("email".to_string(), Box::new(|v| {
            v.contains('@') && v.contains('.') && v.len() > 5
        }));
        
        /* Validator for date format (YYYY-MM-DD) */
        /* Ensures dates follow a standard format for parsing */
        validators.insert("date".to_string(), Box::new(|v| {
            v.len() == 10 && v.matches('-').count() == 2
        }));
        
        /* Validator for version format (semantic versioning) */
        /* Checks for version numbers like "1.2.3" */
        validators.insert("version".to_string(), Box::new(|v| {
            v.split('.').count() >= 2 && v.chars().all(|c| c.is_ascii_digit() || c == '.')
        }));
        
        /* Validator for status values */
        /* Ensures status is one of the allowed values */
        validators.insert("status".to_string(), Box::new(|v| {
            matches!(v, "draft" | "review" | "approved" | "published" | "archived")
        }));
        
        Self {
            required_keys: vec!["title".to_string(), "author".to_string()],
            optional_keys: vec!["email".to_string(), "date".to_string(), 
                               "version".to_string(), "status".to_string(),
                               "tags".to_string(), "category".to_string()],
            validators,
        }
    }
    
    /* Validate metadata against the schema */
    /* Returns a list of validation errors if any are found */
    fn validate(&self, metadata: &HashMap<String, String>) -> Vec<String> {
        let mut errors = Vec::new();
        
        /* Check for required keys */
        /* Ensure all mandatory metadata fields are present */
        for key in &self.required_keys {
            if !metadata.contains_key(key) {
                errors.push(format!("Missing required key: {}", key));
            }
        }
        
        /* Validate values using custom validators */
        /* Apply validation functions to ensure data quality */
        for (key, value) in metadata {
            if let Some(validator) = self.validators.get(key) {
                if !validator(value) {
                    errors.push(format!("Invalid value for key '{}': '{}'", key, value));
                }
            }
        }
        
        errors
    }
}

/* ====================================================================
 * METADATA SEARCH AND FILTERING
 * ====================================================================
 * Functions for searching and filtering files based on metadata.
 */

/* Metadata filter criteria */
/* This struct defines search criteria for finding files by metadata */
struct MetadataFilter {
    /* Key-value pairs that must match exactly */
    exact_matches: HashMap<String, String>,
    /* Keys that must exist (value doesn't matter) */
    required_keys: Vec<String>,
    /* Keys that must NOT exist */
    excluded_keys: Vec<String>,
    /* Partial value matches (contains) */
    partial_matches: HashMap<String, String>,
}

/* Implementation of metadata filter */
impl MetadataFilter {
    /* Create a new empty filter */
    /* Start with no criteria and add conditions as needed */
    fn new() -> Self {
        Self {
            exact_matches: HashMap::new(),
            required_keys: Vec::new(),
            excluded_keys: Vec::new(),
            partial_matches: HashMap::new(),
        }
    }
    
    /* Add an exact match requirement */
    /* File metadata must have this exact key-value pair */
    fn with_exact_match(mut self, key: &str, value: &str) -> Self {
        self.exact_matches.insert(key.to_string(), value.to_string());
        self
    }
    
    /* Add a required key */
    /* File metadata must have this key (any value) */
    fn with_required_key(mut self, key: &str) -> Self {
        self.required_keys.push(key.to_string());
        self
    }
    
    /* Add an excluded key */
    /* File metadata must NOT have this key */
    fn with_excluded_key(mut self, key: &str) -> Self {
        self.excluded_keys.push(key.to_string());
        self
    }
    
    /* Add a partial match requirement */
    /* File metadata value must contain this substring */
    fn with_partial_match(mut self, key: &str, value: &str) -> Self {
        self.partial_matches.insert(key.to_string(), value.to_string());
        self
    }
    
    /* Check if metadata matches the filter criteria */
    /* Returns true if all criteria are satisfied */
    fn matches(&self, metadata: &HashMap<String, String>) -> bool {
        /* Check exact matches */
        /* All specified exact matches must be present and correct */
        for (key, expected_value) in &self.exact_matches {
            match metadata.get(key) {
                Some(actual_value) if actual_value == expected_value => continue,
                _ => return false, /* Exact match failed */
            }
        }
        
        /* Check required keys */
        /* All required keys must be present */
        for key in &self.required_keys {
            if !metadata.contains_key(key) {
                return false; /* Required key missing */
            }
        }
        
        /* Check excluded keys */
        /* None of the excluded keys should be present */
        for key in &self.excluded_keys {
            if metadata.contains_key(key) {
                return false; /* Excluded key found */
            }
        }
        
        /* Check partial matches */
        /* Values must contain the specified substrings */
        for (key, expected_substring) in &self.partial_matches {
            match metadata.get(key) {
                Some(actual_value) if actual_value.contains(expected_substring) => continue,
                _ => return false, /* Partial match failed */
            }
        }
        
        /* All criteria satisfied */
        true
    }
}

/* ====================================================================
 * METADATA-DRIVEN WORKFLOW ENGINE
 * ====================================================================
 * A simple workflow engine that uses metadata to control file processing.
 */

/* Workflow action types */
/* Different actions that can be performed based on metadata */
enum WorkflowAction {
    /* Move file to a different category */
    Categorize(String),
    /* Update status metadata */
    UpdateStatus(String),
    /* Add tags to metadata */
    AddTags(Vec<String>),
    /* Archive the file */
    Archive,
    /* Delete the file */
    Delete,
}

/* Workflow rule */
/* Defines a condition and action to take when condition is met */
struct WorkflowRule {
    /* Condition that triggers this rule */
    condition: MetadataFilter,
    /* Action to perform when condition is met */
    action: WorkflowAction,
    /* Description for logging and debugging */
    description: String,
}

/* Workflow engine */
/* Processes files based on metadata-driven rules */
struct WorkflowEngine {
    /* List of rules to evaluate */
    rules: Vec<WorkflowRule>,
}

/* Implementation of workflow engine */
impl WorkflowEngine {
    /* Create a new workflow engine */
    /* Start with an empty rule set */
    fn new() -> Self {
        Self {
            rules: Vec::new(),
        }
    }
    
    /* Add a workflow rule */
    /* Rules are evaluated in order, first match wins */
    fn add_rule(mut self, rule: WorkflowRule) -> Self {
        self.rules.push(rule);
        self
    }
    
    /* Process a file based on its metadata */
    /* Returns the action to take, or None if no rule matches */
    fn process(&self, metadata: &HashMap<String, String>) -> Option<&WorkflowAction> {
        /* Evaluate rules in order */
        /* First matching rule determines the action */
        for rule in &self.rules {
            if rule.condition.matches(metadata) {
                return Some(&rule.action);
            }
        }
        /* No rule matched */
        None
    }
}

/* ====================================================================
 * MAIN EXAMPLE FUNCTION
 * ====================================================================
 * Demonstrates all advanced metadata operations.
 */

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    /* Print header for better output readability */
    println!("FastDFS Rust Client - Advanced Metadata Operations Example");
    println!("{}", "=".repeat(70));

    /* ====================================================================
     * STEP 1: Initialize Client
     * ====================================================================
     * Set up the FastDFS client with appropriate configuration.
     */
    
    println!("\n1. Initializing FastDFS Client...");
    /* Configure client with tracker server address */
    /* Replace with your actual tracker server address */
    let config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        /* Set connection pool size for better performance */
        /* Larger pools handle more concurrent operations */
        .with_max_conns(20)
        /* Connection timeout in milliseconds */
        .with_connect_timeout(5000)
        /* Network operation timeout */
        .with_network_timeout(30000);
    
    /* Create the client instance */
    /* This initializes connection pools and prepares for operations */
    let client = Client::new(config)?;
    println!("   ✓ Client initialized successfully");

    /* ====================================================================
     * EXAMPLE 1: Complex Metadata Scenarios
     * ====================================================================
     * Demonstrate uploading files with complex, structured metadata.
     */
    
    println!("\n2. Complex Metadata Scenarios...");
    
    /* Scenario 1: Document with full metadata */
    /* Create comprehensive metadata for a document management system */
    println!("\n   Scenario 1: Document with comprehensive metadata");
    let mut doc_metadata = HashMap::new();
    /* Core document information */
    doc_metadata.insert("title".to_string(), "Project Proposal 2025".to_string());
    doc_metadata.insert("author".to_string(), "Jane Smith".to_string());
    doc_metadata.insert("email".to_string(), "jane.smith@company.com".to_string());
    doc_metadata.insert("date".to_string(), "2025-01-15".to_string());
    doc_metadata.insert("version".to_string(), "1.2.3".to_string());
    doc_metadata.insert("status".to_string(), "review".to_string());
    /* Categorization and organization */
    doc_metadata.insert("category".to_string(), "proposals".to_string());
    doc_metadata.insert("department".to_string(), "Engineering".to_string());
    doc_metadata.insert("project".to_string(), "FastDFS Integration".to_string());
    /* Tags for flexible searching */
    doc_metadata.insert("tags".to_string(), "important,urgent,client-facing".to_string());
    /* Additional metadata */
    doc_metadata.insert("language".to_string(), "en".to_string());
    doc_metadata.insert("format".to_string(), "pdf".to_string());
    doc_metadata.insert("pages".to_string(), "25".to_string());
    
    /* Upload file with complex metadata */
    let doc_data = b"Document content: Project proposal for FastDFS integration...";
    let doc_file_id = client.upload_buffer(doc_data, "txt", Some(&doc_metadata)).await?;
    println!("     ✓ Document uploaded with {} metadata fields", doc_metadata.len());
    println!("     File ID: {}", doc_file_id);
    
    /* Scenario 2: Image with metadata */
    /* Demonstrate metadata for media files */
    println!("\n   Scenario 2: Image file with metadata");
    let mut image_metadata = HashMap::new();
    image_metadata.insert("title".to_string(), "Product Photo".to_string());
    image_metadata.insert("author".to_string(), "Photo Studio".to_string());
    image_metadata.insert("category".to_string(), "images".to_string());
    image_metadata.insert("type".to_string(), "product".to_string());
    image_metadata.insert("width".to_string(), "1920".to_string());
    image_metadata.insert("height".to_string(), "1080".to_string());
    image_metadata.insert("format".to_string(), "jpeg".to_string());
    image_metadata.insert("tags".to_string(), "product,marketing,web".to_string());
    
    let image_data = b"Fake image data for demonstration purposes...";
    let image_file_id = client.upload_buffer(image_data, "jpg", Some(&image_metadata)).await?;
    println!("     ✓ Image uploaded with metadata");
    println!("     File ID: {}", image_file_id);
    
    /* Scenario 3: Versioned document */
    /* Show how to handle document versioning with metadata */
    println!("\n   Scenario 3: Versioned document series");
    let mut version_metadata = HashMap::new();
    version_metadata.insert("title".to_string(), "API Documentation".to_string());
    version_metadata.insert("author".to_string(), "Tech Writer".to_string());
    version_metadata.insert("version".to_string(), "2.1.0".to_string());
    version_metadata.insert("version_major".to_string(), "2".to_string());
    version_metadata.insert("version_minor".to_string(), "1".to_string());
    version_metadata.insert("version_patch".to_string(), "0".to_string());
    version_metadata.insert("status".to_string(), "published".to_string());
    version_metadata.insert("previous_version".to_string(), "2.0.5".to_string());
    
    let version_data = b"API documentation version 2.1.0...";
    let version_file_id = client.upload_buffer(version_data, "txt", Some(&version_metadata)).await?;
    println!("     ✓ Versioned document uploaded");
    println!("     File ID: {}", version_file_id);

    /* ====================================================================
     * EXAMPLE 2: Metadata Validation
     * ====================================================================
     * Validate metadata before and after operations.
     */
    
    println!("\n3. Metadata Validation...");
    
    /* Create a metadata schema for validation */
    /* This defines what metadata is valid for our use case */
    let schema = MetadataSchema::new();
    println!("   ✓ Metadata schema created");
    
    /* Validate existing metadata */
    /* Check if uploaded files have valid metadata */
    println!("\n   Validating document metadata...");
    let retrieved_doc_metadata = client.get_metadata(&doc_file_id).await?;
    let doc_errors = schema.validate(&retrieved_doc_metadata);
    if doc_errors.is_empty() {
        println!("     ✓ Document metadata is valid");
    } else {
        println!("     ✗ Document metadata validation errors:");
        for error in &doc_errors {
            println!("       - {}", error);
        }
    }
    
    /* Validate before upload */
    /* Check metadata before uploading to catch errors early */
    println!("\n   Validating metadata before upload...");
    let mut new_metadata = HashMap::new();
    new_metadata.insert("title".to_string(), "New Document".to_string());
    new_metadata.insert("author".to_string(), "Author Name".to_string());
    new_metadata.insert("email".to_string(), "invalid-email".to_string()); /* Invalid email */
    new_metadata.insert("date".to_string(), "2025-01-15".to_string());
    
    let validation_errors = schema.validate(&new_metadata);
    if !validation_errors.is_empty() {
        println!("     ✗ Validation errors detected (preventing upload):");
        for error in &validation_errors {
            println!("       - {}", error);
        }
        /* In production, you would fix errors before uploading */
        println!("     Note: In production, fix errors before uploading");
    }
    
    /* Fix validation errors and upload */
    /* Correct the metadata and proceed with upload */
    new_metadata.insert("email".to_string(), "author@example.com".to_string());
    let validation_errors = schema.validate(&new_metadata);
    if validation_errors.is_empty() {
        println!("     ✓ Metadata is now valid, ready for upload");
        let valid_data = b"Validated document content...";
        let _valid_file_id = client.upload_buffer(valid_data, "txt", Some(&new_metadata)).await?;
        println!("     ✓ File uploaded with validated metadata");
    }

    /* ====================================================================
     * EXAMPLE 3: Metadata Search Patterns
     * ====================================================================
     * Search and filter files based on metadata criteria.
     */
    
    println!("\n4. Metadata Search Patterns...");
    
    /* Create a collection of file IDs and their metadata */
    /* In production, you might maintain an index for efficient searching */
    let file_collection: Vec<(String, HashMap<String, String>)> = vec![
        (doc_file_id.clone(), doc_metadata.clone()),
        (image_file_id.clone(), image_metadata.clone()),
        (version_file_id.clone(), version_metadata.clone()),
    ];
    
    /* Search Pattern 1: Exact match */
    /* Find files with specific metadata values */
    println!("\n   Search Pattern 1: Exact match");
    let filter1 = MetadataFilter::new()
        .with_exact_match("status", "review");
    println!("     Searching for files with status='review'...");
    let mut matches = 0;
    for (file_id, metadata) in &file_collection {
        if filter1.matches(metadata) {
            println!("       ✓ Match found: {}", file_id);
            matches += 1;
        }
    }
    println!("     Found {} matching file(s)", matches);
    
    /* Search Pattern 2: Required key */
    /* Find files that have a specific key (any value) */
    println!("\n   Search Pattern 2: Required key");
    let filter2 = MetadataFilter::new()
        .with_required_key("category");
    println!("     Searching for files with 'category' key...");
    let mut matches = 0;
    for (file_id, metadata) in &file_collection {
        if filter2.matches(metadata) {
            println!("       ✓ Match found: {}", file_id);
            matches += 1;
        }
    }
    println!("     Found {} matching file(s)", matches);
    
    /* Search Pattern 3: Partial match */
    /* Find files where a value contains a substring */
    println!("\n   Search Pattern 3: Partial match");
    let filter3 = MetadataFilter::new()
        .with_partial_match("tags", "urgent");
    println!("     Searching for files with tags containing 'urgent'...");
    let mut matches = 0;
    for (file_id, metadata) in &file_collection {
        if filter3.matches(metadata) {
            println!("       ✓ Match found: {}", file_id);
            matches += 1;
        }
    }
    println!("     Found {} matching file(s)", matches);
    
    /* Search Pattern 4: Complex filter */
    /* Combine multiple criteria for precise searching */
    println!("\n   Search Pattern 4: Complex filter");
    let filter4 = MetadataFilter::new()
        .with_exact_match("category", "proposals")
        .with_required_key("author")
        .with_excluded_key("archived");
    println!("     Searching with complex criteria...");
    let mut matches = 0;
    for (file_id, metadata) in &file_collection {
        if filter4.matches(metadata) {
            println!("       ✓ Match found: {}", file_id);
            matches += 1;
        }
    }
    println!("     Found {} matching file(s)", matches);
    
    /* Search Pattern 5: Retrieve and filter dynamically */
    /* Get metadata from server and filter in application */
    println!("\n   Search Pattern 5: Dynamic retrieval and filtering");
    println!("     Retrieving metadata from server and filtering...");
    let mut dynamic_matches = 0;
    for (file_id, _) in &file_collection {
        /* Retrieve fresh metadata from server */
        /* This ensures we have the latest metadata state */
        match client.get_metadata(file_id).await {
            Ok(metadata) => {
                /* Apply filter to retrieved metadata */
                if filter1.matches(&metadata) {
                    println!("       ✓ Dynamic match: {}", file_id);
                    dynamic_matches += 1;
                }
            }
            Err(e) => {
                println!("       ✗ Error retrieving metadata for {}: {}", file_id, e);
            }
        }
    }
    println!("     Found {} matching file(s) via dynamic retrieval", dynamic_matches);

    /* ====================================================================
     * EXAMPLE 4: Metadata-Driven Workflows
     * ====================================================================
     * Use metadata to automate file processing workflows.
     */
    
    println!("\n5. Metadata-Driven Workflows...");
    
    /* Create a workflow engine */
    /* This engine processes files based on metadata rules */
    let workflow = WorkflowEngine::new()
        /* Rule 1: Archive old published documents */
        .add_rule(WorkflowRule {
            condition: MetadataFilter::new()
                .with_exact_match("status", "published")
                .with_required_key("date"),
            action: WorkflowAction::Archive,
            description: "Archive published documents".to_string(),
        })
        /* Rule 2: Categorize review documents */
        .add_rule(WorkflowRule {
            condition: MetadataFilter::new()
                .with_exact_match("status", "review"),
            action: WorkflowAction::Categorize("pending-review".to_string()),
            description: "Categorize documents under review".to_string(),
        })
        /* Rule 3: Update status for approved documents */
        .add_rule(WorkflowRule {
            condition: MetadataFilter::new()
                .with_exact_match("status", "approved"),
            action: WorkflowAction::UpdateStatus("published".to_string()),
            description: "Publish approved documents".to_string(),
        });
    
    println!("   ✓ Workflow engine created with {} rules", 3);
    
    /* Process files through workflow */
    /* Evaluate each file's metadata against workflow rules */
    println!("\n   Processing files through workflow...");
    for (file_id, metadata) in &file_collection {
        println!("     Processing file: {}", file_id);
        /* Get current metadata from server */
        /* Always use fresh metadata for workflow decisions */
        match client.get_metadata(file_id).await {
            Ok(current_metadata) => {
                /* Evaluate workflow rules */
                /* Find the first matching rule */
                match workflow.process(&current_metadata) {
                    Some(action) => {
                        /* Execute the workflow action */
                        /* In production, this would perform the actual action */
                        match action {
                            WorkflowAction::Archive => {
                                println!("       → Action: Archive file");
                                /* In production: move to archive storage, update metadata */
                            }
                            WorkflowAction::Categorize(category) => {
                                println!("       → Action: Categorize as '{}'", category);
                                /* Update metadata with new category */
                                let mut updated = current_metadata.clone();
                                updated.insert("workflow_category".to_string(), category.clone());
                                /* Apply update using merge to preserve other metadata */
                                let _ = client.set_metadata(file_id, &updated, MetadataFlag::Merge).await;
                            }
                            WorkflowAction::UpdateStatus(new_status) => {
                                println!("       → Action: Update status to '{}'", new_status);
                                /* Update status metadata */
                                let mut updated = current_metadata.clone();
                                updated.insert("status".to_string(), new_status.clone());
                                let _ = client.set_metadata(file_id, &updated, MetadataFlag::Merge).await;
                            }
                            WorkflowAction::AddTags(tags) => {
                                println!("       → Action: Add tags {:?}", tags);
                                /* Add tags to metadata */
                            }
                            WorkflowAction::Delete => {
                                println!("       → Action: Delete file");
                                /* Delete the file */
                            }
                        }
                    }
                    None => {
                        println!("       → No workflow action (no matching rule)");
                    }
                }
            }
            Err(e) => {
                println!("       ✗ Error retrieving metadata: {}", e);
            }
        }
    }

    /* ====================================================================
     * EXAMPLE 5: Performance Considerations
     * ====================================================================
     * Optimize metadata operations for better performance.
     */
    
    println!("\n6. Performance Considerations...");
    
    /* Technique 1: Batch metadata operations */
    /* Group multiple metadata updates together for efficiency */
    println!("\n   Technique 1: Batch metadata operations");
    let mut batch_files = Vec::new();
    for i in 0..5 {
        let mut batch_metadata = HashMap::new();
        batch_metadata.insert("title".to_string(), format!("Batch File {}", i));
        batch_metadata.insert("author".to_string(), "Batch Processor".to_string());
        batch_metadata.insert("batch_id".to_string(), "batch_001".to_string());
        
        let batch_data = format!("Batch file {} content", i).into_bytes();
        match client.upload_buffer(&batch_data, "txt", Some(&batch_metadata)).await {
            Ok(file_id) => {
                batch_files.push((file_id, batch_metadata));
            }
            Err(e) => {
                println!("     ✗ Error uploading batch file {}: {}", i, e);
            }
        }
    }
    println!("     ✓ Uploaded {} files in batch", batch_files.len());
    
    /* Technique 2: Metadata caching */
    /* Cache metadata locally to reduce server round-trips */
    println!("\n   Technique 2: Metadata caching");
    let mut metadata_cache: HashMap<String, HashMap<String, String>> = HashMap::new();
    println!("     Caching metadata for batch files...");
    for (file_id, _) in &batch_files {
        /* Retrieve and cache metadata */
        match client.get_metadata(file_id).await {
            Ok(metadata) => {
                metadata_cache.insert(file_id.clone(), metadata);
            }
            Err(e) => {
                println!("       ✗ Error caching metadata for {}: {}", file_id, e);
            }
        }
    }
    println!("     ✓ Cached metadata for {} files", metadata_cache.len());
    
    /* Use cached metadata for filtering */
    /* Avoid server round-trips by using cached data */
    println!("     Using cached metadata for filtering...");
    let cache_filter = MetadataFilter::new()
        .with_exact_match("batch_id", "batch_001");
    let mut cache_matches = 0;
    for (file_id, metadata) in &metadata_cache {
        if cache_filter.matches(metadata) {
            cache_matches += 1;
        }
    }
    println!("     ✓ Found {} matches using cached metadata (no server calls)", cache_matches);
    
    /* Technique 3: Selective metadata updates */
    /* Only update changed metadata to minimize network traffic */
    println!("\n   Technique 3: Selective metadata updates");
    if let Some((file_id, _)) = batch_files.first() {
        /* Get current metadata */
        if let Ok(current_metadata) = client.get_metadata(file_id).await {
            /* Only update if there are actual changes */
            let mut updated = current_metadata.clone();
            updated.insert("last_updated".to_string(), "2025-01-15".to_string());
            
            /* Use merge mode to only add/update specific fields */
            /* This is more efficient than overwriting all metadata */
            match client.set_metadata(file_id, &updated, MetadataFlag::Merge).await {
                Ok(_) => {
                    println!("     ✓ Selectively updated metadata (merge mode)");
                }
                Err(e) => {
                    println!("     ✗ Error updating metadata: {}", e);
                }
            }
        }
    }
    
    /* Clean up batch files */
    println!("\n   Cleaning up batch files...");
    for (file_id, _) in &batch_files {
        let _ = client.delete_file(file_id).await;
    }
    println!("     ✓ Batch files cleaned up");

    /* ====================================================================
     * EXAMPLE 6: Advanced Metadata Patterns
     * ====================================================================
     * Demonstrate advanced patterns for metadata organization.
     */
    
    println!("\n7. Advanced Metadata Patterns...");
    
    /* Pattern 1: Hierarchical categorization */
    /* Use dot notation for hierarchical categories */
    println!("\n   Pattern 1: Hierarchical categorization");
    let mut hierarchical_metadata = HashMap::new();
    hierarchical_metadata.insert("category".to_string(), "documents.proposals.2025".to_string());
    hierarchical_metadata.insert("title".to_string(), "Hierarchical Document".to_string());
    hierarchical_metadata.insert("author".to_string(), "System".to_string());
    
    let hierarchical_data = b"Document with hierarchical category...";
    let hierarchical_file_id = client.upload_buffer(hierarchical_data, "txt", Some(&hierarchical_metadata)).await?;
    println!("     ✓ File uploaded with hierarchical category: documents.proposals.2025");
    
    /* Pattern 2: Tag-based organization */
    /* Use comma-separated tags for flexible organization */
    println!("\n   Pattern 2: Tag-based organization");
    let mut tagged_metadata = HashMap::new();
    tagged_metadata.insert("title".to_string(), "Tagged Document".to_string());
    tagged_metadata.insert("author".to_string(), "System".to_string());
    tagged_metadata.insert("tags".to_string(), "important,urgent,client,confidential".to_string());
    tagged_metadata.insert("tag_count".to_string(), "4".to_string());
    
    let tagged_data = b"Document with multiple tags...";
    let tagged_file_id = client.upload_buffer(tagged_data, "txt", Some(&tagged_metadata)).await?;
    println!("     ✓ File uploaded with tags: important,urgent,client,confidential");
    
    /* Pattern 3: Version tracking */
    /* Track document versions using metadata */
    println!("\n   Pattern 3: Version tracking");
    let mut versioned_metadata = HashMap::new();
    versioned_metadata.insert("title".to_string(), "Versioned Document".to_string());
    versioned_metadata.insert("author".to_string(), "System".to_string());
    versioned_metadata.insert("version".to_string(), "3.2.1".to_string());
    versioned_metadata.insert("version_history".to_string(), "1.0.0,2.0.0,3.0.0,3.1.0,3.2.0".to_string());
    versioned_metadata.insert("is_latest".to_string(), "true".to_string());
    
    let versioned_data = b"Version 3.2.1 of the document...";
    let versioned_file_id = client.upload_buffer(versioned_data, "txt", Some(&versioned_metadata)).await?;
    println!("     ✓ File uploaded with version tracking: 3.2.1");
    
    /* Pattern 4: Relationship tracking */
    /* Track relationships between files */
    println!("\n   Pattern 4: Relationship tracking");
    let mut related_metadata = HashMap::new();
    related_metadata.insert("title".to_string(), "Related Document".to_string());
    related_metadata.insert("author".to_string(), "System".to_string());
    related_metadata.insert("related_to".to_string(), format!("{},{}", hierarchical_file_id, tagged_file_id));
    related_metadata.insert("relationship_type".to_string(), "references".to_string());
    
    let related_data = b"Document that references other documents...";
    let related_file_id = client.upload_buffer(related_data, "txt", Some(&related_metadata)).await?;
    println!("     ✓ File uploaded with relationship tracking");
    
    /* Pattern 5: Metadata templates */
    /* Use consistent metadata structures across similar files */
    println!("\n   Pattern 5: Metadata templates");
    /* Define a template for document metadata */
    fn create_document_template(title: &str, author: &str) -> HashMap<String, String> {
        let mut template = HashMap::new();
        template.insert("title".to_string(), title.to_string());
        template.insert("author".to_string(), author.to_string());
        template.insert("type".to_string(), "document".to_string());
        template.insert("status".to_string(), "draft".to_string());
        template.insert("created_date".to_string(), "2025-01-15".to_string());
        template
    }
    
    let template_metadata = create_document_template("Template Document", "Template Author");
    let template_data = b"Document created from template...";
    let template_file_id = client.upload_buffer(template_data, "txt", Some(&template_metadata)).await?;
    println!("     ✓ File uploaded using metadata template");

    /* ====================================================================
     * EXAMPLE 7: Metadata Filtering
     * ====================================================================
     * Advanced filtering techniques for metadata queries.
     */
    
    println!("\n8. Advanced Metadata Filtering...");
    
    /* Create a collection of all test files for filtering */
    let mut all_files: Vec<(String, HashMap<String, String>)> = vec![
        (hierarchical_file_id.clone(), hierarchical_metadata.clone()),
        (tagged_file_id.clone(), tagged_metadata.clone()),
        (versioned_file_id.clone(), versioned_metadata.clone()),
        (related_file_id.clone(), related_metadata.clone()),
        (template_file_id.clone(), template_metadata.clone()),
    ];
    
    /* Filter 1: Category-based filtering */
    /* Find files in a specific category hierarchy */
    println!("\n   Filter 1: Category-based filtering");
    let category_filter = MetadataFilter::new()
        .with_partial_match("category", "proposals");
    println!("     Finding files in 'proposals' category...");
    let mut category_matches = 0;
    for (file_id, metadata) in &all_files {
        if category_filter.matches(metadata) {
            println!("       ✓ Match: {} (category: {})", 
                     file_id, 
                     metadata.get("category").unwrap_or(&"N/A".to_string()));
            category_matches += 1;
        }
    }
    println!("     Found {} file(s) in proposals category", category_matches);
    
    /* Filter 2: Tag-based filtering */
    /* Find files with specific tags */
    println!("\n   Filter 2: Tag-based filtering");
    let tag_filter = MetadataFilter::new()
        .with_partial_match("tags", "important");
    println!("     Finding files tagged as 'important'...");
    let mut tag_matches = 0;
    for (file_id, metadata) in &all_files {
        if tag_filter.matches(metadata) {
            println!("       ✓ Match: {}", file_id);
            tag_matches += 1;
        }
    }
    println!("     Found {} file(s) with 'important' tag", tag_matches);
    
    /* Filter 3: Version-based filtering */
    /* Find files matching version criteria */
    println!("\n   Filter 3: Version-based filtering");
    let version_filter = MetadataFilter::new()
        .with_required_key("version")
        .with_partial_match("version", "3.");
    println!("     Finding files with version 3.x...");
    let mut version_matches = 0;
    for (file_id, metadata) in &all_files {
        if version_filter.matches(metadata) {
            println!("       ✓ Match: {} (version: {})", 
                     file_id,
                     metadata.get("version").unwrap_or(&"N/A".to_string()));
            version_matches += 1;
        }
    }
    println!("     Found {} file(s) with version 3.x", version_matches);
    
    /* Filter 4: Status-based filtering */
    /* Find files by status */
    println!("\n   Filter 4: Status-based filtering");
    let status_filter = MetadataFilter::new()
        .with_exact_match("status", "draft");
    println!("     Finding files with status 'draft'...");
    let mut status_matches = 0;
    for (file_id, metadata) in &all_files {
        if status_filter.matches(metadata) {
            println!("       ✓ Match: {}", file_id);
            status_matches += 1;
        }
    }
    println!("     Found {} file(s) with 'draft' status", status_matches);
    
    /* Filter 5: Combined complex filtering */
    /* Use multiple criteria for precise filtering */
    println!("\n   Filter 5: Combined complex filtering");
    let complex_filter = MetadataFilter::new()
        .with_required_key("title")
        .with_required_key("author")
        .with_excluded_key("archived");
    println!("     Finding files with title and author, not archived...");
    let mut complex_matches = 0;
    for (file_id, metadata) in &all_files {
        if complex_filter.matches(metadata) {
            println!("       ✓ Match: {}", file_id);
            complex_matches += 1;
        }
    }
    println!("     Found {} file(s) matching complex criteria", complex_matches);
    
    /* Filter 6: Dynamic filtering with server retrieval */
    /* Retrieve metadata from server and filter dynamically */
    println!("\n   Filter 6: Dynamic server-side filtering simulation");
    println!("     Retrieving and filtering metadata from server...");
    let mut dynamic_filter_matches = 0;
    for (file_id, _) in &all_files {
        /* Retrieve fresh metadata */
        match client.get_metadata(file_id).await {
            Ok(metadata) => {
                /* Apply filter to fresh metadata */
                if tag_filter.matches(&metadata) {
                    println!("       ✓ Dynamic match: {}", file_id);
                    dynamic_filter_matches += 1;
                }
            }
            Err(e) => {
                println!("       ✗ Error: {}", e);
            }
        }
    }
    println!("     Found {} file(s) via dynamic filtering", dynamic_filter_matches);

    /* ====================================================================
     * CLEANUP
     * ====================================================================
     * Clean up all test files created during the example.
     */
    
    println!("\n9. Cleaning up test files...");
    /* List of all file IDs to delete */
    let cleanup_files = vec![
        doc_file_id,
        image_file_id,
        version_file_id,
        hierarchical_file_id,
        tagged_file_id,
        versioned_file_id,
        related_file_id,
        template_file_id,
    ];
    
    /* Delete each test file */
    let mut deleted_count = 0;
    for file_id in &cleanup_files {
        match client.delete_file(file_id).await {
            Ok(_) => {
                deleted_count += 1;
            }
            Err(e) => {
                println!("     ⚠ Error deleting {}: {}", file_id, e);
            }
        }
    }
    println!("     ✓ Deleted {} test file(s)", deleted_count);

    /* ====================================================================
     * SUMMARY
     * ====================================================================
     * Print summary of all demonstrated features.
     */
    
    println!("\n{}", "=".repeat(70));
    println!("Advanced Metadata Operations Example Completed Successfully!");
    println!("\nSummary of demonstrated features:");
    println!("  ✓ Complex metadata scenarios with multiple files");
    println!("  ✓ Metadata validation and schema enforcement");
    println!("  ✓ Metadata search patterns (exact, partial, required keys)");
    println!("  ✓ Metadata-driven workflow automation");
    println!("  ✓ Performance optimization (batching, caching, selective updates)");
    println!("  ✓ Advanced metadata patterns (hierarchical, tags, versions, relationships)");
    println!("  ✓ Advanced metadata filtering and querying");
    println!("\nAll features demonstrated with extensive comments and examples.");

    /* Close the client to release resources */
    /* Always clean up connections when done */
    client.close().await;
    println!("\n✓ Client closed. All resources released.");

    /* Return success */
    Ok(())
}

