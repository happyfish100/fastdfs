/*! FastDFS Integration Patterns Example
 *
 * This comprehensive example demonstrates how to integrate the FastDFS client
 * into real-world Rust applications, including web frameworks, async runtimes,
 * dependency injection, configuration management, and logging.
 *
 * Key Integration Patterns Covered:
 * - Integration with web frameworks (axum, actix-web)
 * - Integration with async runtimes (tokio)
 * - Dependency injection patterns
 * - Configuration from environment variables
 * - Logging integration (tracing, log)
 * - Error handling in web contexts
 *
 * Run this example with:
 * ```bash
 * cargo run --example integration_example
 * ```
 *
 * Note: This example requires optional dependencies. To enable web framework examples,
 * you may need to add axum and actix-web to your Cargo.toml dev-dependencies.
 */

use fastdfs::{Client, ClientConfig, FastDFSError};
use std::sync::Arc;
use std::env;

// ============================================================================
// SECTION 1: Configuration from Environment Variables
// ============================================================================

/// Application configuration loaded from environment variables
/// This demonstrates how to configure FastDFS client from environment
#[derive(Debug, Clone)]
pub struct AppConfig {
    /// FastDFS tracker server addresses (comma-separated)
    pub tracker_addrs: Vec<String>,
    /// Maximum connections per server
    pub max_conns: usize,
    /// Connection timeout in milliseconds
    pub connect_timeout: u64,
    /// Network timeout in milliseconds
    pub network_timeout: u64,
    /// Application name for logging
    pub app_name: String,
    /// Log level (trace, debug, info, warn, error)
    pub log_level: String,
}

impl AppConfig {
    /// Load configuration from environment variables
    /// This pattern allows configuration without hardcoding values
    pub fn from_env() -> Result<Self, Box<dyn std::error::Error>> {
        let tracker_addrs = env::var("FASTDFS_TRACKER_ADDRS")
            .unwrap_or_else(|_| "192.168.1.100:22122".to_string())
            .split(',')
            .map(|s| s.trim().to_string())
            .collect();

        let max_conns = env::var("FASTDFS_MAX_CONNS")
            .unwrap_or_else(|_| "10".to_string())
            .parse()
            .unwrap_or(10);

        let connect_timeout = env::var("FASTDFS_CONNECT_TIMEOUT")
            .unwrap_or_else(|_| "5000".to_string())
            .parse()
            .unwrap_or(5000);

        let network_timeout = env::var("FASTDFS_NETWORK_TIMEOUT")
            .unwrap_or_else(|_| "30000".to_string())
            .parse()
            .unwrap_or(30000);

        let app_name = env::var("APP_NAME")
            .unwrap_or_else(|_| "fastdfs-integration-example".to_string());

        let log_level = env::var("LOG_LEVEL")
            .unwrap_or_else(|_| "info".to_string());

        Ok(AppConfig {
            tracker_addrs,
            max_conns,
            connect_timeout,
            network_timeout,
            app_name,
            log_level,
        })
    }

    /// Create FastDFS ClientConfig from application config
    pub fn to_client_config(&self) -> ClientConfig {
        ClientConfig::new(self.tracker_addrs.clone())
            .with_max_conns(self.max_conns)
            .with_connect_timeout(self.connect_timeout)
            .with_network_timeout(self.network_timeout)
    }
}

// ============================================================================
// SECTION 2: Dependency Injection Pattern
// ============================================================================

/// Application state that can be shared across handlers
/// This demonstrates dependency injection in web frameworks
#[derive(Clone)]
pub struct AppState {
    /// FastDFS client wrapped in Arc for shared ownership
    /// Arc allows the client to be shared across multiple async tasks
    pub fastdfs_client: Arc<Client>,
    /// Application configuration
    pub config: Arc<AppConfig>,
}

impl AppState {
    /// Create new application state with FastDFS client
    pub fn new(config: AppConfig) -> Result<Self, Box<dyn std::error::Error>> {
        let client_config = config.to_client_config();
        let client = Client::new(client_config)?;
        
        Ok(AppState {
            fastdfs_client: Arc::new(client),
            config: Arc::new(config),
        })
    }
}

/// Service layer that encapsulates FastDFS operations
/// This demonstrates service-oriented architecture with dependency injection
pub struct FileService {
    client: Arc<Client>,
}

impl FileService {
    /// Create a new file service with injected client
    pub fn new(client: Arc<Client>) -> Self {
        FileService { client }
    }

    /// Upload file with error handling suitable for web contexts
    pub async fn upload_file(
        &self,
        data: &[u8],
        extension: &str,
    ) -> Result<String, ServiceError> {
        self.client
            .upload_buffer(data, extension, None)
            .await
            .map_err(ServiceError::from)
    }

    /// Download file with error handling
    pub async fn download_file(&self, file_id: &str) -> Result<Vec<u8>, ServiceError> {
        let data = self.client
            .download_file(file_id)
            .await
            .map_err(ServiceError::from)?;
        Ok(data.to_vec())
    }

    /// Get file info with error handling
    pub async fn get_file_info(&self, file_id: &str) -> Result<fastdfs::FileInfo, ServiceError> {
        self.client
            .get_file_info(file_id)
            .await
            .map_err(ServiceError::from)
    }

    /// Delete file with error handling
    pub async fn delete_file(&self, file_id: &str) -> Result<(), ServiceError> {
        self.client
            .delete_file(file_id)
            .await
            .map_err(ServiceError::from)
    }
}

// ============================================================================
// SECTION 3: Error Handling for Web Contexts
// ============================================================================

/// Service-level error type that can be converted to HTTP responses
/// This demonstrates error handling patterns for web applications
#[derive(Debug)]
pub enum ServiceError {
    /// FastDFS-specific errors
    FastDFS(FastDFSError),
    /// File not found errors
    NotFound(String),
    /// Validation errors
    Validation(String),
    /// Internal server errors
    Internal(String),
}

impl From<FastDFSError> for ServiceError {
    fn from(err: FastDFSError) -> Self {
        match err {
            FastDFSError::FileNotExist(_) => ServiceError::NotFound(
                "File not found".to_string()
            ),
            FastDFSError::ConnectionTimeout(_) | 
            FastDFSError::NetworkTimeout(_) => ServiceError::Internal(
                "Connection timeout".to_string()
            ),
            _ => ServiceError::FastDFS(err),
        }
    }
}

impl std::fmt::Display for ServiceError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ServiceError::FastDFS(e) => write!(f, "FastDFS error: {}", e),
            ServiceError::NotFound(msg) => write!(f, "Not found: {}", msg),
            ServiceError::Validation(msg) => write!(f, "Validation error: {}", msg),
            ServiceError::Internal(msg) => write!(f, "Internal error: {}", msg),
        }
    }
}

impl std::error::Error for ServiceError {}

// ============================================================================
// SECTION 4: Logging Integration
// ============================================================================

/// Initialize logging based on configuration
/// This demonstrates integration with logging frameworks
pub fn init_logging(config: &AppConfig) {
    // In a real application, you would use tracing or log crate
    // This is a simplified example
    println!("[LOG] Initializing logging with level: {}", config.log_level);
    println!("[LOG] Application: {}", config.app_name);
    
    // Example of structured logging
    log_info("application_started", &format!("App: {}", config.app_name));
    log_info("config_loaded", &format!("Trackers: {:?}", config.tracker_addrs));
}

/// Log info message (simplified - in real app use tracing/log)
fn log_info(event: &str, message: &str) {
    println!("[INFO] event={} message={}", event, message);
}

/// Log error message
fn log_error(event: &str, error: &dyn std::error::Error) {
    eprintln!("[ERROR] event={} error={}", event, error);
}

/// Log warning message
fn log_warn(event: &str, message: &str) {
    println!("[WARN] event={} message={}", event, message);
}

// ============================================================================
// SECTION 5: Async Runtime Integration
// ============================================================================

/// Demonstrate integration with Tokio async runtime
/// This shows how FastDFS client works seamlessly with async/await
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("FastDFS Rust Client - Integration Patterns Example");
    println!("{}", "=".repeat(70));
    println!();

    // ====================================================================
    // Example 1: Configuration from Environment Variables
    // ====================================================================
    println!("1. Configuration from Environment Variables");
    println!("{}", "-".repeat(70));
    
    let config = AppConfig::from_env()?;
    println!("   Loaded configuration:");
    println!("     Tracker addresses: {:?}", config.tracker_addrs);
    println!("     Max connections: {}", config.max_conns);
    println!("     Connect timeout: {}ms", config.connect_timeout);
    println!("     Network timeout: {}ms", config.network_timeout);
    println!("     App name: {}", config.app_name);
    println!("     Log level: {}", config.log_level);
    println!();

    // ====================================================================
    // Example 2: Logging Integration
    // ====================================================================
    println!("2. Logging Integration");
    println!("{}", "-".repeat(70));
    init_logging(&config);
    println!();

    // ====================================================================
    // Example 3: Dependency Injection Pattern
    // ====================================================================
    println!("3. Dependency Injection Pattern");
    println!("{}", "-".repeat(70));
    
    let app_state = AppState::new(config.clone())?;
    log_info("client_initialized", "FastDFS client created successfully");
    
    let file_service = FileService::new(app_state.fastdfs_client.clone());
    println!("   ✓ Created FileService with injected FastDFS client");
    println!("   ✓ Client is shared via Arc for thread-safe access");
    println!();

    // ====================================================================
    // Example 4: Async Runtime Integration
    // ====================================================================
    println!("4. Async Runtime Integration");
    println!("{}", "-".repeat(70));
    
    // Demonstrate concurrent operations using async/await
    println!("   Running concurrent file operations...");
    
    let upload_task = {
        let service = file_service.clone();
        tokio::spawn(async move {
            let data = b"Concurrent upload test";
            service.upload_file(data, "txt").await
        })
    };

    let info_task = {
        let client = app_state.fastdfs_client.clone();
        tokio::spawn(async move {
            // This would normally use an existing file_id
            // For demo purposes, we'll just show the pattern
            log_info("async_task", "Running async file info retrieval");
            Ok(())
        })
    };

    // Wait for both tasks to complete
    let upload_result = upload_task.await??;
    info_task.await??;
    
    println!("   ✓ Concurrent operations completed");
    println!("   ✓ Uploaded file: {}", upload_result);
    println!();

    // ====================================================================
    // Example 5: Error Handling in Service Layer
    // ====================================================================
    println!("5. Error Handling in Service Layer");
    println!("{}", "-".repeat(70));
    
    // Test error handling with non-existent file
    match file_service.get_file_info("group1/nonexistent_file.txt").await {
        Ok(_) => println!("   ⚠ Unexpected: File exists"),
        Err(ServiceError::NotFound(_)) => {
            println!("   ✓ Correctly handled file not found error");
        }
        Err(e) => {
            log_error("file_info_error", &e);
        }
    }
    println!();

    // ====================================================================
    // Example 6: Web Framework Integration Patterns
    // ====================================================================
    println!("6. Web Framework Integration Patterns");
    println!("{}", "-".repeat(70));
    println!("   The following sections show how to integrate with web frameworks.");
    println!("   Note: Actual web server code would require additional dependencies.");
    println!();

    // Demonstrate Axum integration pattern (conceptual)
    println!("   Axum Integration Pattern:");
    println!("   ```rust");
    println!("   use axum::{{extract::State, response::Json, routing::post, Router}};");
    println!("   ");
    println!("   async fn upload_handler(");
    println!("       State(state): State<AppState>,");
    println!("       body: Vec<u8>,");
    println!("   ) -> Result<Json<UploadResponse>, ServiceError> {{");
    println!("       let service = FileService::new(state.fastdfs_client.clone());");
    println!("       let file_id = service.upload_file(&body, \"bin\").await?;");
    println!("       Ok(Json(UploadResponse {{ file_id }}))");
    println!("   }}");
    println!("   ");
    println!("   let app = Router::new()");
    println!("       .route(\"/upload\", post(upload_handler))");
    println!("       .with_state(app_state);");
    println!("   ```");
    println!();

    // Demonstrate Actix-Web integration pattern (conceptual)
    println!("   Actix-Web Integration Pattern:");
    println!("   ```rust");
    println!("   use actix_web::{{web, App, HttpResponse, HttpServer, Result}};");
    println!("   ");
    println!("   async fn upload_handler(");
    println!("       state: web::Data<AppState>,");
    println!("       body: web::Bytes,");
    println!("   ) -> Result<HttpResponse> {{");
    println!("       let service = FileService::new(state.fastdfs_client.clone());");
    println!("       match service.upload_file(&body, \"bin\").await {{");
    println!("           Ok(file_id) => Ok(HttpResponse::Ok().json(UploadResponse {{ file_id }})),");
    println!("           Err(e) => Ok(HttpResponse::InternalServerError().json(e))),");
    println!("       }}");
    println!("   }}");
    println!("   ");
    println!("   HttpServer::new(move || {{");
    println!("       App::new()");
    println!("           .app_data(web::Data::new(app_state.clone()))");
    println!("           .route(\"/upload\", web::post().to(upload_handler))");
    println!("   }})");
    println!("   ```");
    println!();

    // ====================================================================
    // Example 7: Real Web Handler Implementation (Simplified)
    // ====================================================================
    println!("7. Simplified Web Handler Implementation");
    println!("{}", "-".repeat(70));
    
    // Simulate a web request handler
    async fn simulate_web_handler(
        service: FileService,
        request_body: &[u8],
    ) -> Result<String, ServiceError> {
        log_info("web_request", "Received upload request");
        
        // Validate request
        if request_body.is_empty() {
            return Err(ServiceError::Validation("Empty file not allowed".to_string()));
        }
        
        if request_body.len() > 10 * 1024 * 1024 {
            return Err(ServiceError::Validation("File too large".to_string()));
        }
        
        // Process upload
        let file_id = service.upload_file(request_body, "bin").await?;
        log_info("upload_success", &format!("File uploaded: {}", file_id));
        
        Ok(file_id)
    }
    
    let test_data = b"Test file for web handler simulation";
    match simulate_web_handler(file_service.clone(), test_data).await {
        Ok(file_id) => {
            println!("   ✓ Web handler processed request successfully");
            println!("   ✓ File ID: {}", file_id);
            
            // Cleanup
            let _ = file_service.delete_file(&file_id).await;
        }
        Err(e) => {
            log_error("web_handler_error", &e);
        }
    }
    println!();

    // ====================================================================
    // Example 8: Graceful Shutdown Pattern
    // ====================================================================
    println!("8. Graceful Shutdown Pattern");
    println!("{}", "-".repeat(70));
    
    // In a real web application, you would set up signal handlers
    // This demonstrates the pattern for closing the FastDFS client
    println!("   Setting up graceful shutdown...");
    
    // Simulate receiving shutdown signal
    tokio::spawn(async move {
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;
        log_info("shutdown_signal", "Received shutdown signal");
    });
    
    println!("   ✓ Shutdown handler registered");
    println!();

    // ====================================================================
    // Cleanup
    // ====================================================================
    println!("9. Cleanup");
    println!("{}", "-".repeat(70));
    
    // Close the client gracefully
    app_state.fastdfs_client.close().await;
    log_info("client_closed", "FastDFS client closed, resources released");
    
    println!();
    println!("{}", "=".repeat(70));
    println!("Integration patterns example completed successfully!");
    println!();
    println!("Summary of demonstrated patterns:");
    println!("  ✓ Configuration from environment variables");
    println!("  ✓ Dependency injection with Arc and service layer");
    println!("  ✓ Logging integration");
    println!("  ✓ Async runtime integration with Tokio");
    println!("  ✓ Error handling for web contexts");
    println!("  ✓ Web framework integration patterns (Axum, Actix-Web)");
    println!("  ✓ Graceful shutdown handling");
    
    Ok(())
}

// Helper implementation for FileService cloning
impl Clone for FileService {
    fn clone(&self) -> Self {
        FileService {
            client: Arc::clone(&self.client),
        }
    }
}

