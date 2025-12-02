/*! FastDFS Configuration Management Example
 *
 * This comprehensive example demonstrates advanced configuration management
 * for the FastDFS Rust client. It covers all configuration options, best
 * practices, environment-specific setups, and performance tuning.
 *
 * Configuration topics covered:
 * - Advanced configuration options and their effects
 * - Multiple tracker server setup for high availability
 * - Timeout tuning for different network conditions
 * - Connection pool tuning for optimal performance
 * - Environment-specific configurations (dev, staging, production)
 * - Configuration best practices and recommendations
 * - ClientConfig builder pattern usage
 *
 * Understanding proper configuration is crucial for:
 * - Achieving optimal performance
 * - Ensuring high availability
 * - Handling different network conditions
 * - Adapting to various deployment environments
 * - Preventing connection issues and timeouts
 *
 * Run this example with:
 * ```bash
 * cargo run --example configuration_example
 * ```
 */

/* Import FastDFS client components */
/* ClientConfig provides the builder pattern for configuration */
/* Client is the main client that uses the configuration */
use fastdfs::{Client, ClientConfig};
/* Standard library for error handling and environment variables */
use std::env;

/* ====================================================================
 * CONFIGURATION HELPER FUNCTIONS
 * ====================================================================
 * Utility functions for creating environment-specific configurations.
 */

/* Environment type for configuration selection */
/* Different environments have different requirements */
#[derive(Debug, Clone, Copy)]
enum Environment {
    /* Development environment - relaxed timeouts, smaller pools */
    Development,
    /* Staging environment - production-like but with more debugging */
    Staging,
    /* Production environment - optimized for performance and reliability */
    Production,
}

/* Get current environment from environment variable */
/* Allows configuration to adapt based on deployment environment */
fn get_environment() -> Environment {
    /* Check for environment variable */
    /* Common environment variables: ENV, ENVIRONMENT, APP_ENV */
    match env::var("ENV")
        .or_else(|_| env::var("ENVIRONMENT"))
        .or_else(|_| env::var("APP_ENV"))
    {
        Ok(env_str) => {
            /* Parse environment string (case-insensitive) */
            match env_str.to_lowercase().as_str() {
                "dev" | "development" => Environment::Development,
                "staging" | "stage" => Environment::Staging,
                "prod" | "production" => Environment::Production,
                _ => {
                    /* Default to development if unknown */
                    println!("   Unknown environment '{}', defaulting to Development", env_str);
                    Environment::Development
                }
            }
        }
        Err(_) => {
            /* No environment variable set, default to development */
            Environment::Development
        }
    }
}

/* Create configuration for development environment */
/* Development config prioritizes ease of debugging over performance */
fn create_development_config() -> ClientConfig {
    println!("   Creating Development configuration...");
    /* Development configuration characteristics: */
    /* - Smaller connection pools (less resource usage) */
    /* - Longer timeouts (more forgiving for debugging) */
    /* - Single tracker server (simpler setup) */
    /* - More retries (handles temporary issues during development) */
    
    ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        /* Smaller connection pool for development */
        /* Fewer connections mean less resource usage on dev machines */
        .with_max_conns(5)
        /* Longer connection timeout for debugging */
        /* Gives more time when stepping through code */
        .with_connect_timeout(10000) /* 10 seconds */
        /* Longer network timeout for development */
        /* Allows time to inspect network traffic */
        .with_network_timeout(60000) /* 60 seconds */
        /* Shorter idle timeout for development */
        /* Releases connections faster when not in use */
        .with_idle_timeout(30000) /* 30 seconds */
        /* More retries for development */
        /* Helps during development when services might be restarting */
        .with_retry_count(5)
}

/* Create configuration for staging environment */
/* Staging config balances production-like settings with debugging capability */
fn create_staging_config() -> ClientConfig {
    println!("   Creating Staging configuration...");
    /* Staging configuration characteristics: */
    /* - Medium connection pools (production-like but not maxed) */
    /* - Moderate timeouts (production-like with some buffer) */
    /* - Multiple tracker servers (test high availability) */
    /* - Standard retries (production-like behavior) */
    
    ClientConfig::new(vec![
        "192.168.1.100:22122".to_string(),
        "192.168.1.101:22122".to_string(),
    ])
        /* Medium connection pool for staging */
        /* Enough for testing but not maxed out */
        .with_max_conns(15)
        /* Moderate connection timeout */
        /* Production-like but with some buffer for testing */
        .with_connect_timeout(5000) /* 5 seconds */
        /* Moderate network timeout */
        /* Production-like network timeout */
        .with_network_timeout(30000) /* 30 seconds */
        /* Standard idle timeout */
        /* Production-like connection lifecycle */
        .with_idle_timeout(60000) /* 60 seconds */
        /* Standard retry count */
        /* Production-like retry behavior */
        .with_retry_count(3)
}

/* Create configuration for production environment */
/* Production config optimized for performance, reliability, and availability */
fn create_production_config() -> ClientConfig {
    println!("   Creating Production configuration...");
    /* Production configuration characteristics: */
    /* - Larger connection pools (handle high load) */
    /* - Optimized timeouts (balance responsiveness and reliability) */
    /* - Multiple tracker servers (high availability) */
    /* - Appropriate retries (handle transient failures) */
    
    ClientConfig::new(vec![
        "tracker1.example.com:22122".to_string(),
        "tracker2.example.com:22122".to_string(),
        "tracker3.example.com:22122".to_string(),
    ])
        /* Larger connection pool for production */
        /* More connections handle concurrent requests better */
        .with_max_conns(50)
        /* Optimized connection timeout */
        /* Fast enough for responsiveness, long enough for reliability */
        .with_connect_timeout(3000) /* 3 seconds */
        /* Optimized network timeout */
        /* Balance between responsiveness and handling slow networks */
        .with_network_timeout(20000) /* 20 seconds */
        /* Longer idle timeout for production */
        /* Keep connections alive longer to reduce connection overhead */
        .with_idle_timeout(120000) /* 120 seconds (2 minutes) */
        /* Appropriate retry count */
        /* Handle transient failures without excessive retries */
        .with_retry_count(3)
}

/* ====================================================================
 * CONFIGURATION PRESETS
 * ====================================================================
 * Pre-configured settings for common scenarios.
 */

/* Create high-performance configuration */
/* Optimized for maximum throughput and low latency */
fn create_high_performance_config() -> ClientConfig {
    println!("   Creating High-Performance configuration...");
    /* High-performance characteristics: */
    /* - Large connection pools (maximize parallelism) */
    /* - Aggressive timeouts (fail fast, retry quickly) */
    /* - Multiple trackers (load distribution) */
    
    ClientConfig::new(vec![
        "192.168.1.100:22122".to_string(),
        "192.168.1.101:22122".to_string(),
    ])
        /* Large connection pool for high concurrency */
        /* More connections = more parallel operations */
        .with_max_conns(100)
        /* Fast connection timeout */
        /* Fail fast if connection can't be established */
        .with_connect_timeout(2000) /* 2 seconds */
        /* Fast network timeout */
        /* Don't wait too long for slow operations */
        .with_network_timeout(10000) /* 10 seconds */
        /* Long idle timeout */
        /* Keep connections alive to avoid reconnection overhead */
        .with_idle_timeout(300000) /* 5 minutes */
        /* Fewer retries for high performance */
        /* Fail fast, let application handle retries if needed */
        .with_retry_count(2)
}

/* Create high-availability configuration */
/* Optimized for reliability and fault tolerance */
fn create_high_availability_config() -> ClientConfig {
    println!("   Creating High-Availability configuration...");
    /* High-availability characteristics: */
    /* - Multiple tracker servers (redundancy) */
    /* - Conservative timeouts (handle network issues) */
    /* - More retries (handle transient failures) */
    
    ClientConfig::new(vec![
        "tracker1.example.com:22122".to_string(),
        "tracker2.example.com:22122".to_string(),
        "tracker3.example.com:22122".to_string(),
        "tracker4.example.com:22122".to_string(),
    ])
        /* Moderate connection pool */
        /* Enough for load, not so many that failures cascade */
        .with_max_conns(30)
        /* Conservative connection timeout */
        /* Give time for connections to establish on slow networks */
        .with_connect_timeout(8000) /* 8 seconds */
        /* Conservative network timeout */
        /* Handle slow but working network connections */
        .with_network_timeout(45000) /* 45 seconds */
        /* Standard idle timeout */
        .with_idle_timeout(90000) /* 90 seconds */
        /* More retries for high availability */
        /* Retry more times to handle transient failures */
        .with_retry_count(5)
}

/* Create low-latency configuration */
/* Optimized for minimal response times */
fn create_low_latency_config() -> ClientConfig {
    println!("   Creating Low-Latency configuration...");
    /* Low-latency characteristics: */
    /* - Pre-warmed connection pools (connections ready) */
    /* - Very short timeouts (fail fast) */
    /* - Multiple trackers (choose fastest) */
    
    ClientConfig::new(vec![
        "192.168.1.100:22122".to_string(),
        "192.168.1.101:22122".to_string(),
    ])
        /* Moderate connection pool */
        /* Pre-warmed connections reduce latency */
        .with_max_conns(20)
        /* Very short connection timeout */
        /* Fail fast if connection is slow */
        .with_connect_timeout(1000) /* 1 second */
        /* Short network timeout */
        /* Don't wait for slow operations */
        .with_network_timeout(5000) /* 5 seconds */
        /* Long idle timeout */
        /* Keep connections alive to avoid connection overhead */
        .with_idle_timeout(180000) /* 3 minutes */
        /* Minimal retries */
        /* Fail fast, let application decide on retries */
        .with_retry_count(1)
}

/* Create resource-constrained configuration */
/* Optimized for environments with limited resources */
fn create_resource_constrained_config() -> ClientConfig {
    println!("   Creating Resource-Constrained configuration...");
    /* Resource-constrained characteristics: */
    /* - Small connection pools (minimal memory usage) */
    /* - Reasonable timeouts (don't hold resources too long) */
    /* - Single tracker (simpler, less overhead) */
    
    ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        /* Small connection pool */
        /* Minimize memory and connection overhead */
        .with_max_conns(3)
        /* Moderate connection timeout */
        .with_connect_timeout(5000) /* 5 seconds */
        /* Moderate network timeout */
        .with_network_timeout(30000) /* 30 seconds */
        /* Short idle timeout */
        /* Release connections quickly to free resources */
        .with_idle_timeout(20000) /* 20 seconds */
        /* Standard retries */
        .with_retry_count(3)
}

/* ====================================================================
 * MAIN EXAMPLE FUNCTION
 * ====================================================================
 * Demonstrates all configuration management techniques.
 */

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    /* Print header for better output readability */
    println!("FastDFS Rust Client - Configuration Management Example");
    println!("{}", "=".repeat(70));

    /* ====================================================================
     * EXAMPLE 1: Basic Configuration
     * ====================================================================
     * Start with the simplest configuration approach.
     */
    
    println!("\n1. Basic Configuration...");
    println!("\n   Example 1.1: Minimal configuration");
    /* Create a basic configuration with just tracker addresses */
    /* This uses all default values for other settings */
    let basic_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()]);
    println!("     Tracker addresses: {:?}", basic_config.tracker_addrs);
    println!("     Max connections: {} (default)", basic_config.max_conns);
    println!("     Connect timeout: {} ms (default)", basic_config.connect_timeout);
    println!("     Network timeout: {} ms (default)", basic_config.network_timeout);
    println!("     Idle timeout: {} ms (default)", basic_config.idle_timeout);
    println!("     Retry count: {} (default)", basic_config.retry_count);
    
    /* Try to create a client with basic config */
    /* This validates the configuration */
    match Client::new(basic_config) {
        Ok(_client) => {
            println!("     ✓ Basic configuration is valid");
            /* Note: We don't use this client, just validate the config */
        }
        Err(e) => {
            println!("     ✗ Configuration error: {}", e);
        }
    }

    /* ====================================================================
     * EXAMPLE 2: Builder Pattern Usage
     * ====================================================================
     * Demonstrate the fluent builder pattern for configuration.
     */
    
    println!("\n2. Builder Pattern Configuration...");
    println!("\n   Example 2.1: Step-by-step builder pattern");
    /* The builder pattern allows method chaining */
    /* Each method returns Self, allowing fluent configuration */
    let builder_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        /* Set maximum connections */
        /* More connections allow more concurrent operations */
        .with_max_conns(20)
        /* Set connection timeout */
        /* How long to wait when establishing a connection */
        .with_connect_timeout(5000)
        /* Set network timeout */
        /* How long to wait for network operations to complete */
        .with_network_timeout(30000)
        /* Set idle timeout */
        /* How long to keep idle connections before closing */
        .with_idle_timeout(60000)
        /* Set retry count */
        /* How many times to retry failed operations */
        .with_retry_count(3);
    
    println!("     ✓ Configuration built using builder pattern");
    println!("     Max connections: {}", builder_config.max_conns);
    println!("     Connect timeout: {} ms", builder_config.connect_timeout);
    println!("     Network timeout: {} ms", builder_config.network_timeout);
    println!("     Idle timeout: {} ms", builder_config.idle_timeout);
    println!("     Retry count: {}", builder_config.retry_count);
    
    println!("\n   Example 2.2: Compact builder pattern");
    /* Builder pattern can be used in a single expression */
    /* This is more concise but less readable for complex configs */
    let compact_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(15)
        .with_connect_timeout(3000)
        .with_network_timeout(20000)
        .with_idle_timeout(90000)
        .with_retry_count(2);
    println!("     ✓ Compact configuration created");
    println!("     Configuration: {:?}", compact_config);

    /* ====================================================================
     * EXAMPLE 3: Multiple Tracker Servers
     * ====================================================================
     * Configure multiple tracker servers for high availability.
     */
    
    println!("\n3. Multiple Tracker Server Configuration...");
    println!("\n   Example 3.1: Two tracker servers (primary + backup)");
    /* Configure with primary and backup tracker */
    /* If primary fails, client automatically uses backup */
    let two_tracker_config = ClientConfig::new(vec![
        "192.168.1.100:22122".to_string(), /* Primary tracker */
        "192.168.1.101:22122".to_string(), /* Backup tracker */
    ])
        .with_max_conns(20)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    println!("     Tracker servers: {:?}", two_tracker_config.tracker_addrs);
    println!("     ✓ Configuration supports automatic failover");
    
    println!("\n   Example 3.2: Three tracker servers (high availability)");
    /* Three trackers provide better redundancy */
    /* Client can distribute load and handle multiple failures */
    let three_tracker_config = ClientConfig::new(vec![
        "tracker1.example.com:22122".to_string(),
        "tracker2.example.com:22122".to_string(),
        "tracker3.example.com:22122".to_string(),
    ])
        .with_max_conns(30)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    println!("     Tracker servers: {:?}", three_tracker_config.tracker_addrs);
    println!("     ✓ High availability configuration with load distribution");
    
    println!("\n   Example 3.3: Four tracker servers (maximum redundancy)");
    /* Four trackers provide maximum redundancy */
    /* Can handle multiple tracker failures simultaneously */
    let four_tracker_config = ClientConfig::new(vec![
        "tracker1.example.com:22122".to_string(),
        "tracker2.example.com:22122".to_string(),
        "tracker3.example.com:22122".to_string(),
        "tracker4.example.com:22122".to_string(),
    ])
        .with_max_conns(40)
        .with_connect_timeout(5000)
        .with_network_timeout(30000);
    
    println!("     Tracker servers: {} servers configured", four_tracker_config.tracker_addrs.len());
    println!("     ✓ Maximum redundancy configuration");

    /* ====================================================================
     * EXAMPLE 4: Timeout Tuning
     * ====================================================================
     * Tune timeouts for different network conditions and use cases.
     */
    
    println!("\n4. Timeout Tuning...");
    
    println!("\n   Example 4.1: Fast network configuration");
    /* For fast, reliable networks (data center, local network) */
    /* Use shorter timeouts for better responsiveness */
    let fast_network_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_connect_timeout(2000) /* 2 seconds - fast connection */
        .with_network_timeout(10000) /* 10 seconds - fast operations */
        .with_idle_timeout(120000); /* 2 minutes - keep connections alive */
    
    println!("     Connect timeout: {} ms (fast network)", fast_network_config.connect_timeout);
    println!("     Network timeout: {} ms (fast network)", fast_network_config.network_timeout);
    println!("     Idle timeout: {} ms (keep connections)", fast_network_config.idle_timeout);
    println!("     ✓ Optimized for fast, reliable networks");
    
    println!("\n   Example 4.2: Slow network configuration");
    /* For slow or unreliable networks (WAN, mobile, satellite) */
    /* Use longer timeouts to handle network delays */
    let slow_network_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_connect_timeout(15000) /* 15 seconds - allow slow connection */
        .with_network_timeout(60000) /* 60 seconds - allow slow operations */
        .with_idle_timeout(300000); /* 5 minutes - keep connections longer */
    
    println!("     Connect timeout: {} ms (slow network)", slow_network_config.connect_timeout);
    println!("     Network timeout: {} ms (slow network)", slow_network_config.network_timeout);
    println!("     Idle timeout: {} ms (keep connections longer)", slow_network_config.idle_timeout);
    println!("     ✓ Optimized for slow or unreliable networks");
    
    println!("\n   Example 4.3: Balanced timeout configuration");
    /* Balanced timeouts work well for most scenarios */
    /* Good compromise between responsiveness and reliability */
    let balanced_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_connect_timeout(5000) /* 5 seconds - reasonable connection time */
        .with_network_timeout(30000) /* 30 seconds - reasonable operation time */
        .with_idle_timeout(60000); /* 60 seconds - standard idle time */
    
    println!("     Connect timeout: {} ms (balanced)", balanced_config.connect_timeout);
    println!("     Network timeout: {} ms (balanced)", balanced_config.network_timeout);
    println!("     Idle timeout: {} ms (balanced)", balanced_config.idle_timeout);
    println!("     ✓ Balanced configuration for general use");
    
    println!("\n   Example 4.4: Timeout recommendations by operation type");
    /* Different operations may need different timeout strategies */
    println!("     Upload operations:");
    println!("       - Large files: Longer network timeout (60-120s)");
    println!("       - Small files: Shorter network timeout (10-30s)");
    println!("     Download operations:");
    println!("       - Full downloads: Longer network timeout (30-60s)");
    println!("       - Partial downloads: Shorter network timeout (10-20s)");
    println!("     Metadata operations:");
    println!("       - Fast operations: Short network timeout (5-10s)");
    println!("     Connection establishment:");
    println!("       - Local network: Short connect timeout (1-3s)");
    println!("       - Remote network: Longer connect timeout (5-10s)");

    /* ====================================================================
     * EXAMPLE 5: Connection Pool Tuning
     * ====================================================================
     * Tune connection pools for optimal performance.
     */
    
    println!("\n5. Connection Pool Tuning...");
    
    println!("\n   Example 5.1: Small connection pool (low concurrency)");
    /* Small pools for applications with low concurrency */
    /* Uses less memory and resources */
    let small_pool_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(5) /* Small pool */
        .with_idle_timeout(30000); /* Short idle timeout */
    
    println!("     Max connections: {}", small_pool_config.max_conns);
    println!("     Use case: Low-traffic applications, single-threaded operations");
    println!("     Memory usage: Low");
    println!("     ✓ Suitable for low-concurrency scenarios");
    
    println!("\n   Example 5.2: Medium connection pool (moderate concurrency)");
    /* Medium pools for typical applications */
    /* Good balance of performance and resource usage */
    let medium_pool_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(20) /* Medium pool */
        .with_idle_timeout(60000); /* Standard idle timeout */
    
    println!("     Max connections: {}", medium_pool_config.max_conns);
    println!("     Use case: Typical web applications, moderate traffic");
    println!("     Memory usage: Moderate");
    println!("     ✓ Suitable for most applications");
    
    println!("\n   Example 5.3: Large connection pool (high concurrency)");
    /* Large pools for high-concurrency applications */
    /* Handles many simultaneous operations */
    let large_pool_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_max_conns(100) /* Large pool */
        .with_idle_timeout(120000); /* Longer idle timeout */
    
    println!("     Max connections: {}", large_pool_config.max_conns);
    println!("     Use case: High-traffic applications, many concurrent operations");
    println!("     Memory usage: High");
    println!("     ✓ Suitable for high-concurrency scenarios");
    
    println!("\n   Example 5.4: Connection pool sizing guidelines");
    /* Guidelines for choosing pool size */
    println!("     Pool size = Expected concurrent operations + buffer");
    println!("     Example calculations:");
    println!("       - 10 concurrent operations → pool size 15-20");
    println!("       - 50 concurrent operations → pool size 60-75");
    println!("       - 100 concurrent operations → pool size 120-150");
    println!("     Note: Each connection uses memory, don't over-allocate");
    println!("     Note: Too many connections can overwhelm the server");

    /* ====================================================================
     * EXAMPLE 6: Retry Configuration
     * ====================================================================
     * Configure retry behavior for handling failures.
     */
    
    println!("\n6. Retry Configuration...");
    
    println!("\n   Example 6.1: No retries (fail fast)");
    /* Fail immediately on errors */
    /* Let application handle retries if needed */
    let no_retry_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_retry_count(0); /* No automatic retries */
    
    println!("     Retry count: {}", no_retry_config.retry_count);
    println!("     Use case: Application handles retries, need immediate feedback");
    println!("     ✓ Fail-fast configuration");
    
    println!("\n   Example 6.2: Minimal retries (handle transient errors)");
    /* Retry once for transient errors */
    /* Good for most scenarios */
    let minimal_retry_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_retry_count(1); /* One retry */
    
    println!("     Retry count: {}", minimal_retry_config.retry_count);
    println!("     Use case: Handle brief network hiccups");
    println!("     ✓ Minimal retry configuration");
    
    println!("\n   Example 6.3: Standard retries (default)");
    /* Standard retry count */
    /* Good balance for most applications */
    let standard_retry_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_retry_count(3); /* Standard retries */
    
    println!("     Retry count: {}", standard_retry_config.retry_count);
    println!("     Use case: General purpose, handle transient failures");
    println!("     ✓ Standard retry configuration");
    
    println!("\n   Example 6.4: Aggressive retries (high availability)");
    /* Many retries for high availability scenarios */
    /* Handle multiple transient failures */
    let aggressive_retry_config = ClientConfig::new(vec!["192.168.1.100:22122".to_string()])
        .with_retry_count(5); /* Many retries */
    
    println!("     Retry count: {}", aggressive_retry_config.retry_count);
    println!("     Use case: High availability, unreliable networks");
    println!("     ✓ Aggressive retry configuration");
    
    println!("\n   Example 6.5: Retry strategy recommendations");
    /* Recommendations for different scenarios */
    println!("     Critical operations: 3-5 retries");
    println!("     Non-critical operations: 1-2 retries");
    println!("     Batch operations: 2-3 retries");
    println!("     Real-time operations: 0-1 retries");
    println!("     Note: More retries = longer wait time on failures");
    println!("     Note: Balance between reliability and responsiveness");

    /* ====================================================================
     * EXAMPLE 7: Environment-Specific Configurations
     * ====================================================================
     * Create configurations tailored to different environments.
     */
    
    println!("\n7. Environment-Specific Configurations...");
    
    /* Get current environment */
    /* This allows the application to adapt its configuration */
    let env = get_environment();
    println!("   Detected environment: {:?}", env);
    
    /* Create configuration based on environment */
    /* Each environment has different requirements */
    let env_config = match env {
        Environment::Development => create_development_config(),
        Environment::Staging => create_staging_config(),
        Environment::Production => create_production_config(),
    };
    
    println!("   Environment-specific configuration created:");
    println!("     Tracker servers: {}", env_config.tracker_addrs.len());
    println!("     Max connections: {}", env_config.max_conns);
    println!("     Connect timeout: {} ms", env_config.connect_timeout);
    println!("     Network timeout: {} ms", env_config.network_timeout);
    println!("     Idle timeout: {} ms", env_config.idle_timeout);
    println!("     Retry count: {}", env_config.retry_count);
    
    /* Demonstrate creating client with environment config */
    /* In production, you would use this configuration */
    match Client::new(env_config) {
        Ok(_client) => {
            println!("     ✓ Environment-specific configuration is valid");
        }
        Err(e) => {
            println!("     ✗ Configuration error: {}", e);
        }
    }

    /* ====================================================================
     * EXAMPLE 8: Configuration Presets
     * ====================================================================
     * Use pre-configured settings for common scenarios.
     */
    
    println!("\n8. Configuration Presets...");
    
    println!("\n   Preset 1: High-Performance Configuration");
    let perf_config = create_high_performance_config();
    println!("     Characteristics: Maximum throughput, low latency");
    println!("     Max connections: {}", perf_config.max_conns);
    println!("     Timeouts: Fast ({} ms connect, {} ms network)", 
             perf_config.connect_timeout, perf_config.network_timeout);
    println!("     ✓ Optimized for performance");
    
    println!("\n   Preset 2: High-Availability Configuration");
    let ha_config = create_high_availability_config();
    println!("     Characteristics: Maximum reliability, fault tolerance");
    println!("     Tracker servers: {}", ha_config.tracker_addrs.len());
    println!("     Retry count: {}", ha_config.retry_count);
    println!("     ✓ Optimized for availability");
    
    println!("\n   Preset 3: Low-Latency Configuration");
    let latency_config = create_low_latency_config();
    println!("     Characteristics: Minimal response times");
    println!("     Connect timeout: {} ms (very fast)", latency_config.connect_timeout);
    println!("     Network timeout: {} ms (very fast)", latency_config.network_timeout);
    println!("     ✓ Optimized for low latency");
    
    println!("\n   Preset 4: Resource-Constrained Configuration");
    let resource_config = create_resource_constrained_config();
    println!("     Characteristics: Minimal resource usage");
    println!("     Max connections: {} (small)", resource_config.max_conns);
    println!("     Idle timeout: {} ms (short)", resource_config.idle_timeout);
    println!("     ✓ Optimized for limited resources");

    /* ====================================================================
     * EXAMPLE 9: Configuration Best Practices
     * ====================================================================
     * Learn best practices for configuration management.
     */
    
    println!("\n9. Configuration Best Practices...");
    
    println!("\n   Best Practice 1: Use multiple tracker servers");
    println!("     ✓ Provides high availability and load distribution");
    println!("     ✗ Single tracker is a single point of failure");
    
    println!("\n   Best Practice 2: Tune timeouts based on network");
    println!("     ✓ Fast networks: Shorter timeouts (2-5s connect, 10-20s network)");
    println!("     ✓ Slow networks: Longer timeouts (5-15s connect, 30-60s network)");
    println!("     ✗ Too short: Premature failures");
    println!("     ✗ Too long: Slow failure detection");
    
    println!("\n   Best Practice 3: Size connection pools appropriately");
    println!("     ✓ Pool size = expected concurrent ops + 20-50% buffer");
    println!("     ✗ Too small: Connection starvation under load");
    println!("     ✗ Too large: Wasted memory and server resources");
    
    println!("\n   Best Practice 4: Use environment-specific configs");
    println!("     ✓ Different settings for dev/staging/production");
    println!("     ✓ Use environment variables to select configuration");
    println!("     ✗ Same config for all environments");
    
    println!("\n   Best Practice 5: Monitor and adjust timeouts");
    println!("     ✓ Monitor connection and operation times");
    println!("     ✓ Adjust timeouts based on actual performance");
    println!("     ✗ Set-and-forget configuration");
    
    println!("\n   Best Practice 6: Balance retries and timeouts");
    println!("     ✓ More retries with shorter timeouts");
    println!("     ✓ Fewer retries with longer timeouts");
    println!("     ✗ Many retries with long timeouts = very slow failures");
    
    println!("\n   Best Practice 7: Use builder pattern for clarity");
    println!("     ✓ Method chaining makes configuration readable");
    println!("     ✓ Easy to see what each setting does");
    println!("     ✗ Manual struct construction is less clear");
    
    println!("\n   Best Practice 8: Validate configuration early");
    println!("     ✓ Validate config at application startup");
    println!("     ✓ Fail fast if configuration is invalid");
    println!("     ✗ Discover configuration errors at runtime");

    /* ====================================================================
     * EXAMPLE 10: Complete Configuration Example
     * ====================================================================
     * Put it all together with a complete, production-ready configuration.
     */
    
    println!("\n10. Complete Production Configuration Example...");
    
    /* Create a complete, production-ready configuration */
    /* This demonstrates all best practices together */
    let production_ready_config = ClientConfig::new(vec![
        /* Primary tracker servers */
        "tracker1.production.com:22122".to_string(),
        "tracker2.production.com:22122".to_string(),
        "tracker3.production.com:22122".to_string(),
    ])
        /* Connection pool sized for expected load */
        /* Assume 50 concurrent operations, add 50% buffer = 75 */
        .with_max_conns(75)
        /* Connection timeout for production network */
        /* Fast enough for responsiveness, long enough for reliability */
        .with_connect_timeout(3000) /* 3 seconds */
        /* Network timeout for production operations */
        /* Balance between responsiveness and handling large files */
        .with_network_timeout(30000) /* 30 seconds */
        /* Idle timeout to keep connections alive */
        /* Reduces connection overhead while not holding resources too long */
        .with_idle_timeout(120000) /* 2 minutes */
        /* Retry count for handling transient failures */
        /* Enough retries for reliability, not too many for responsiveness */
        .with_retry_count(3);
    
    println!("   Production-ready configuration:");
    println!("     Tracker servers: {}", production_ready_config.tracker_addrs.len());
    println!("     Max connections: {}", production_ready_config.max_conns);
    println!("     Connect timeout: {} ms", production_ready_config.connect_timeout);
    println!("     Network timeout: {} ms", production_ready_config.network_timeout);
    println!("     Idle timeout: {} ms", production_ready_config.idle_timeout);
    println!("     Retry count: {}", production_ready_config.retry_count);
    
    /* Validate the production configuration */
    /* Always validate before using in production */
    match Client::new(production_ready_config) {
        Ok(_client) => {
            println!("     ✓ Production configuration is valid and ready to use");
            println!("     Note: In production, use this configuration to create your client");
        }
        Err(e) => {
            println!("     ✗ Configuration error: {}", e);
            println!("     Fix configuration before deploying to production");
        }
    }

    /* ====================================================================
     * SUMMARY
     * ====================================================================
     * Print summary of configuration management concepts.
     */
    
    println!("\n{}", "=".repeat(70));
    println!("Configuration Management Example Completed Successfully!");
    println!("\nSummary of demonstrated features:");
    println!("  ✓ Basic configuration with defaults");
    println!("  ✓ Builder pattern for fluent configuration");
    println!("  ✓ Multiple tracker servers for high availability");
    println!("  ✓ Timeout tuning for different network conditions");
    println!("  ✓ Connection pool tuning for optimal performance");
    println!("  ✓ Retry configuration for failure handling");
    println!("  ✓ Environment-specific configurations");
    println!("  ✓ Configuration presets for common scenarios");
    println!("  ✓ Configuration best practices and recommendations");
    println!("  ✓ Complete production-ready configuration example");
    println!("\nAll configuration concepts demonstrated with extensive comments.");

    /* Return success */
    Ok(())
}

