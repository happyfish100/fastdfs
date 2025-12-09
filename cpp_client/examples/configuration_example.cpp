/**
 * Copyright (C) 2025 FastDFS C++ Client Contributors
 *
 * FastDFS Configuration Example
 *
 * This comprehensive example demonstrates comprehensive client configuration options,
 * including timeouts, connection pools, retry policies, loading from files and
 * environment variables, configuration validation, and best practices for production.
 *
 * Key Topics Covered:
 * - Demonstrates comprehensive client configuration options
 * - Shows how to configure timeouts, connection pools, and retry policies
 * - Includes examples of loading configuration from files and environment variables
 * - Demonstrates configuration validation
 * - Useful for production deployment and environment-specific configurations
 * - Shows best practices for configuration management
 *
 * Run this example with:
 *   ./configuration_example <tracker_address>
 *   Example: ./configuration_example 192.168.1.100:22122
 */

#include "fastdfs/client.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <cstdlib>
#include <iomanip>
#include <chrono>

// Helper function to parse configuration file (simple key-value format)
std::map<std::string, std::string> parse_config_file(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return config; // Return empty config if file doesn't exist
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Parse key=value pairs
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            config[key] = value;
        }
    }
    
    file.close();
    return config;
}

// Helper function to get environment variable with default
std::string get_env(const std::string& key, const std::string& default_value = "") {
    const char* value = std::getenv(key.c_str());
    return value ? std::string(value) : default_value;
}

// Helper function to parse timeout string (e.g., "5000ms", "5s", "30")
std::chrono::milliseconds parse_timeout(const std::string& timeout_str) {
    if (timeout_str.empty()) {
        return std::chrono::milliseconds(5000);
    }
    
    // Remove whitespace
    std::string str = timeout_str;
    str.erase(0, str.find_first_not_of(" \t"));
    str.erase(str.find_last_not_of(" \t") + 1);
    
    // Parse number
    int64_t value = 0;
    std::string unit = "ms";
    
    if (str.back() == 's' || str.back() == 'S') {
        unit = "s";
        value = std::stoll(str.substr(0, str.length() - 1));
    } else if (str.length() > 2 && str.substr(str.length() - 2) == "ms") {
        unit = "ms";
        value = std::stoll(str.substr(0, str.length() - 2));
    } else {
        value = std::stoll(str);
    }
    
    if (unit == "s") {
        return std::chrono::milliseconds(value * 1000);
    } else {
        return std::chrono::milliseconds(value);
    }
}

// Configuration validation function
bool validate_config(const fastdfs::ClientConfig& config, std::string& error_msg) {
    // Validate tracker addresses
    if (config.tracker_addrs.empty()) {
        error_msg = "Tracker addresses are required";
        return false;
    }
    
    for (const auto& addr : config.tracker_addrs) {
        if (addr.empty()) {
            error_msg = "Empty tracker address found";
            return false;
        }
        if (addr.find(':') == std::string::npos) {
            error_msg = "Invalid tracker address format (missing port): " + addr;
            return false;
        }
    }
    
    // Validate timeouts
    if (config.connect_timeout.count() <= 0) {
        error_msg = "Connect timeout must be positive";
        return false;
    }
    
    if (config.network_timeout.count() <= 0) {
        error_msg = "Network timeout must be positive";
        return false;
    }
    
    if (config.idle_timeout.count() <= 0) {
        error_msg = "Idle timeout must be positive";
        return false;
    }
    
    // Validate connection limits
    if (config.max_conns <= 0) {
        error_msg = "Max connections must be positive";
        return false;
    }
    
    if (config.max_conns > 1000) {
        error_msg = "Max connections is too high (max 1000)";
        return false;
    }
    
    // Validate retry count
    if (config.retry_count < 0) {
        error_msg = "Retry count cannot be negative";
        return false;
    }
    
    if (config.retry_count > 10) {
        error_msg = "Retry count is too high (max 10)";
        return false;
    }
    
    return true;
}

// Print configuration
void print_config(const fastdfs::ClientConfig& config, const std::string& title = "Configuration") {
    std::cout << "   " << title << ":" << std::endl;
    std::cout << "     Tracker Addresses: ";
    for (size_t i = 0; i < config.tracker_addrs.size(); ++i) {
        std::cout << config.tracker_addrs[i];
        if (i < config.tracker_addrs.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    std::cout << "     Max Connections: " << config.max_conns << std::endl;
    std::cout << "     Connect Timeout: " << config.connect_timeout.count() << " ms" << std::endl;
    std::cout << "     Network Timeout: " << config.network_timeout.count() << " ms" << std::endl;
    std::cout << "     Idle Timeout: " << config.idle_timeout.count() << " ms" << std::endl;
    std::cout << "     Connection Pool: " << (config.enable_pool ? "Enabled" : "Disabled") << std::endl;
    std::cout << "     Retry Count: " << config.retry_count << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tracker_address>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100:22122" << std::endl;
        return 1;
    }

    try {
        std::cout << "FastDFS C++ Client - Configuration Example" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 1: Basic Configuration
        // ====================================================================
        std::cout << "1. Basic Configuration" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates comprehensive client configuration options." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig basic_config;
        basic_config.tracker_addrs = {argv[1]};
        basic_config.max_conns = 10;
        basic_config.connect_timeout = std::chrono::milliseconds(5000);
        basic_config.network_timeout = std::chrono::milliseconds(30000);
        basic_config.idle_timeout = std::chrono::milliseconds(60000);
        basic_config.enable_pool = true;
        basic_config.retry_count = 3;

        print_config(basic_config, "Basic Configuration");
        std::cout << std::endl;

        // Validate configuration
        std::string error_msg;
        if (validate_config(basic_config, error_msg)) {
            std::cout << "   ✓ Configuration is valid" << std::endl;
        } else {
            std::cout << "   ✗ Configuration validation failed: " << error_msg << std::endl;
            return 1;
        }
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 2: Timeout Configuration
        // ====================================================================
        std::cout << "2. Timeout Configuration" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows how to configure timeouts, connection pools, and retry policies." << std::endl;
        std::cout << std::endl;

        // Fast timeout for quick operations
        fastdfs::ClientConfig fast_config;
        fast_config.tracker_addrs = {argv[1]};
        fast_config.max_conns = 5;
        fast_config.connect_timeout = std::chrono::milliseconds(2000);  // 2 seconds
        fast_config.network_timeout = std::chrono::milliseconds(10000); // 10 seconds
        fast_config.idle_timeout = std::chrono::milliseconds(30000);
        fast_config.enable_pool = true;
        fast_config.retry_count = 2;

        print_config(fast_config, "Fast Timeout Configuration");
        std::cout << "   → Use for: Quick operations, low-latency requirements" << std::endl;
        std::cout << std::endl;

        // Slow timeout for large file operations
        fastdfs::ClientConfig slow_config;
        slow_config.tracker_addrs = {argv[1]};
        slow_config.max_conns = 20;
        slow_config.connect_timeout = std::chrono::milliseconds(10000);  // 10 seconds
        slow_config.network_timeout = std::chrono::milliseconds(300000); // 5 minutes
        slow_config.idle_timeout = std::chrono::milliseconds(120000);
        slow_config.enable_pool = true;
        slow_config.retry_count = 5;

        print_config(slow_config, "Slow Timeout Configuration");
        std::cout << "   → Use for: Large file operations, slow networks" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 3: Connection Pool Configuration
        // ====================================================================
        std::cout << "3. Connection Pool Configuration" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates different connection pool configurations." << std::endl;
        std::cout << std::endl;

        // High concurrency configuration
        fastdfs::ClientConfig high_concurrency_config;
        high_concurrency_config.tracker_addrs = {argv[1]};
        high_concurrency_config.max_conns = 100;  // High connection limit
        high_concurrency_config.connect_timeout = std::chrono::milliseconds(5000);
        high_concurrency_config.network_timeout = std::chrono::milliseconds(30000);
        high_concurrency_config.idle_timeout = std::chrono::milliseconds(60000);
        high_concurrency_config.enable_pool = true;
        high_concurrency_config.retry_count = 3;

        print_config(high_concurrency_config, "High Concurrency Configuration");
        std::cout << "   → Use for: High-throughput applications, many concurrent operations" << std::endl;
        std::cout << std::endl;

        // Low resource configuration
        fastdfs::ClientConfig low_resource_config;
        low_resource_config.tracker_addrs = {argv[1]};
        low_resource_config.max_conns = 2;  // Low connection limit
        low_resource_config.connect_timeout = std::chrono::milliseconds(5000);
        low_resource_config.network_timeout = std::chrono::milliseconds(30000);
        low_resource_config.idle_timeout = std::chrono::milliseconds(30000);
        low_resource_config.enable_pool = true;
        low_resource_config.retry_count = 1;

        print_config(low_resource_config, "Low Resource Configuration");
        std::cout << "   → Use for: Resource-constrained environments" << std::endl;
        std::cout << std::endl;

        // Connection pool disabled
        fastdfs::ClientConfig no_pool_config;
        no_pool_config.tracker_addrs = {argv[1]};
        no_pool_config.max_conns = 1;
        no_pool_config.connect_timeout = std::chrono::milliseconds(5000);
        no_pool_config.network_timeout = std::chrono::milliseconds(30000);
        no_pool_config.idle_timeout = std::chrono::milliseconds(60000);
        no_pool_config.enable_pool = false;  // Disable connection pooling
        no_pool_config.retry_count = 3;

        print_config(no_pool_config, "No Connection Pool Configuration");
        std::cout << "   → Use for: Simple applications, single-threaded operations" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 4: Retry Policy Configuration
        // ====================================================================
        std::cout << "4. Retry Policy Configuration" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Shows different retry policies for different scenarios." << std::endl;
        std::cout << std::endl;

        // Aggressive retry (for unreliable networks)
        fastdfs::ClientConfig aggressive_retry_config;
        aggressive_retry_config.tracker_addrs = {argv[1]};
        aggressive_retry_config.max_conns = 10;
        aggressive_retry_config.connect_timeout = std::chrono::milliseconds(5000);
        aggressive_retry_config.network_timeout = std::chrono::milliseconds(30000);
        aggressive_retry_config.idle_timeout = std::chrono::milliseconds(60000);
        aggressive_retry_config.enable_pool = true;
        aggressive_retry_config.retry_count = 10;  // High retry count

        print_config(aggressive_retry_config, "Aggressive Retry Configuration");
        std::cout << "   → Use for: Unreliable networks, high availability requirements" << std::endl;
        std::cout << std::endl;

        // No retry (for fast failure)
        fastdfs::ClientConfig no_retry_config;
        no_retry_config.tracker_addrs = {argv[1]};
        no_retry_config.max_conns = 10;
        no_retry_config.connect_timeout = std::chrono::milliseconds(5000);
        no_retry_config.network_timeout = std::chrono::milliseconds(30000);
        no_retry_config.idle_timeout = std::chrono::milliseconds(60000);
        no_retry_config.enable_pool = true;
        no_retry_config.retry_count = 0;  // No retries

        print_config(no_retry_config, "No Retry Configuration");
        std::cout << "   → Use for: Fast failure scenarios, when retries are handled externally" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 5: Loading from Environment Variables
        // ====================================================================
        std::cout << "5. Loading Configuration from Environment Variables" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Includes examples of loading configuration from environment variables." << std::endl;
        std::cout << std::endl;

        fastdfs::ClientConfig env_config;
        
        // Load tracker address from environment
        std::string tracker_env = get_env("FASTDFS_TRACKER_ADDR");
        if (!tracker_env.empty()) {
            env_config.tracker_addrs = {tracker_env};
            std::cout << "   → Loaded tracker address from FASTDFS_TRACKER_ADDR: " << tracker_env << std::endl;
        } else {
            env_config.tracker_addrs = {argv[1]};  // Fallback to command line
            std::cout << "   → Using command line tracker address (FASTDFS_TRACKER_ADDR not set)" << std::endl;
        }
        
        // Load max connections from environment
        std::string max_conns_env = get_env("FASTDFS_MAX_CONNS");
        if (!max_conns_env.empty()) {
            env_config.max_conns = std::stoi(max_conns_env);
            std::cout << "   → Loaded max_conns from FASTDFS_MAX_CONNS: " << env_config.max_conns << std::endl;
        } else {
            env_config.max_conns = 10;
            std::cout << "   → Using default max_conns: 10" << std::endl;
        }
        
        // Load timeouts from environment
        std::string connect_timeout_env = get_env("FASTDFS_CONNECT_TIMEOUT");
        env_config.connect_timeout = parse_timeout(connect_timeout_env);
        if (!connect_timeout_env.empty()) {
            std::cout << "   → Loaded connect_timeout from FASTDFS_CONNECT_TIMEOUT: " 
                     << env_config.connect_timeout.count() << " ms" << std::endl;
        } else {
            env_config.connect_timeout = std::chrono::milliseconds(5000);
            std::cout << "   → Using default connect_timeout: 5000 ms" << std::endl;
        }
        
        std::string network_timeout_env = get_env("FASTDFS_NETWORK_TIMEOUT");
        env_config.network_timeout = parse_timeout(network_timeout_env);
        if (!network_timeout_env.empty()) {
            std::cout << "   → Loaded network_timeout from FASTDFS_NETWORK_TIMEOUT: " 
                     << env_config.network_timeout.count() << " ms" << std::endl;
        } else {
            env_config.network_timeout = std::chrono::milliseconds(30000);
            std::cout << "   → Using default network_timeout: 30000 ms" << std::endl;
        }
        
        // Load other settings
        std::string enable_pool_env = get_env("FASTDFS_ENABLE_POOL");
        if (!enable_pool_env.empty()) {
            env_config.enable_pool = (enable_pool_env == "true" || enable_pool_env == "1");
            std::cout << "   → Loaded enable_pool from FASTDFS_ENABLE_POOL: " 
                     << (env_config.enable_pool ? "true" : "false") << std::endl;
        } else {
            env_config.enable_pool = true;
        }
        
        std::string retry_count_env = get_env("FASTDFS_RETRY_COUNT");
        if (!retry_count_env.empty()) {
            env_config.retry_count = std::stoi(retry_count_env);
            std::cout << "   → Loaded retry_count from FASTDFS_RETRY_COUNT: " << env_config.retry_count << std::endl;
        } else {
            env_config.retry_count = 3;
        }
        
        env_config.idle_timeout = std::chrono::milliseconds(60000);
        
        std::cout << std::endl;
        print_config(env_config, "Environment-Based Configuration");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 6: Loading from Configuration File
        // ====================================================================
        std::cout << "6. Loading Configuration from File" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates loading configuration from files." << std::endl;
        std::cout << std::endl;

        // Create a sample configuration file
        const std::string config_file = "fastdfs_client.conf";
        std::ofstream config_out(config_file);
        config_out << "# FastDFS Client Configuration\n";
        config_out << "tracker_addr=" << argv[1] << "\n";
        config_out << "max_conns=20\n";
        config_out << "connect_timeout=5000ms\n";
        config_out << "network_timeout=60000ms\n";
        config_out << "idle_timeout=120000ms\n";
        config_out << "enable_pool=true\n";
        config_out << "retry_count=5\n";
        config_out.close();
        
        std::cout << "   Created sample configuration file: " << config_file << std::endl;
        std::cout << std::endl;

        // Load configuration from file
        std::map<std::string, std::string> file_config = parse_config_file(config_file);
        
        fastdfs::ClientConfig file_based_config;
        
        if (file_config.find("tracker_addr") != file_config.end()) {
            file_based_config.tracker_addrs = {file_config["tracker_addr"]};
            std::cout << "   → Loaded tracker_addr from file: " << file_config["tracker_addr"] << std::endl;
        } else {
            file_based_config.tracker_addrs = {argv[1]};
        }
        
        if (file_config.find("max_conns") != file_config.end()) {
            file_based_config.max_conns = std::stoi(file_config["max_conns"]);
            std::cout << "   → Loaded max_conns from file: " << file_based_config.max_conns << std::endl;
        } else {
            file_based_config.max_conns = 10;
        }
        
        if (file_config.find("connect_timeout") != file_config.end()) {
            file_based_config.connect_timeout = parse_timeout(file_config["connect_timeout"]);
            std::cout << "   → Loaded connect_timeout from file: " 
                     << file_based_config.connect_timeout.count() << " ms" << std::endl;
        } else {
            file_based_config.connect_timeout = std::chrono::milliseconds(5000);
        }
        
        if (file_config.find("network_timeout") != file_config.end()) {
            file_based_config.network_timeout = parse_timeout(file_config["network_timeout"]);
            std::cout << "   → Loaded network_timeout from file: " 
                     << file_based_config.network_timeout.count() << " ms" << std::endl;
        } else {
            file_based_config.network_timeout = std::chrono::milliseconds(30000);
        }
        
        if (file_config.find("idle_timeout") != file_config.end()) {
            file_based_config.idle_timeout = parse_timeout(file_config["idle_timeout"]);
            std::cout << "   → Loaded idle_timeout from file: " 
                     << file_based_config.idle_timeout.count() << " ms" << std::endl;
        } else {
            file_based_config.idle_timeout = std::chrono::milliseconds(60000);
        }
        
        if (file_config.find("enable_pool") != file_config.end()) {
            file_based_config.enable_pool = (file_config["enable_pool"] == "true" || 
                                            file_config["enable_pool"] == "1");
            std::cout << "   → Loaded enable_pool from file: " 
                     << (file_based_config.enable_pool ? "true" : "false") << std::endl;
        } else {
            file_based_config.enable_pool = true;
        }
        
        if (file_config.find("retry_count") != file_config.end()) {
            file_based_config.retry_count = std::stoi(file_config["retry_count"]);
            std::cout << "   → Loaded retry_count from file: " << file_based_config.retry_count << std::endl;
        } else {
            file_based_config.retry_count = 3;
        }
        
        std::cout << std::endl;
        print_config(file_based_config, "File-Based Configuration");
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 7: Configuration Validation
        // ====================================================================
        std::cout << "7. Configuration Validation" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Demonstrates configuration validation." << std::endl;
        std::cout << std::endl;

        // Test valid configuration
        fastdfs::ClientConfig valid_config;
        valid_config.tracker_addrs = {argv[1]};
        valid_config.max_conns = 10;
        valid_config.connect_timeout = std::chrono::milliseconds(5000);
        valid_config.network_timeout = std::chrono::milliseconds(30000);
        valid_config.idle_timeout = std::chrono::milliseconds(60000);
        valid_config.enable_pool = true;
        valid_config.retry_count = 3;

        std::string validation_error;
        if (validate_config(valid_config, validation_error)) {
            std::cout << "   ✓ Valid configuration passed validation" << std::endl;
        } else {
            std::cout << "   ✗ Validation failed: " << validation_error << std::endl;
        }
        std::cout << std::endl;

        // Test invalid configurations
        std::cout << "   Testing invalid configurations..." << std::endl;
        
        // Empty tracker addresses
        fastdfs::ClientConfig invalid_config1;
        invalid_config1.tracker_addrs = {};
        if (!validate_config(invalid_config1, validation_error)) {
            std::cout << "   ✓ Correctly detected empty tracker addresses: " << validation_error << std::endl;
        }
        
        // Invalid tracker address format
        fastdfs::ClientConfig invalid_config2;
        invalid_config2.tracker_addrs = {"invalid_address"};
        if (!validate_config(invalid_config2, validation_error)) {
            std::cout << "   ✓ Correctly detected invalid address format: " << validation_error << std::endl;
        }
        
        // Negative timeout
        fastdfs::ClientConfig invalid_config3;
        invalid_config3.tracker_addrs = {argv[1]};
        invalid_config3.connect_timeout = std::chrono::milliseconds(-1);
        if (!validate_config(invalid_config3, validation_error)) {
            std::cout << "   ✓ Correctly detected negative timeout: " << validation_error << std::endl;
        }
        
        // Invalid max connections
        fastdfs::ClientConfig invalid_config4;
        invalid_config4.tracker_addrs = {argv[1]};
        invalid_config4.max_conns = -1;
        if (!validate_config(invalid_config4, validation_error)) {
            std::cout << "   ✓ Correctly detected invalid max_conns: " << validation_error << std::endl;
        }
        
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 8: Environment-Specific Configurations
        // ====================================================================
        std::cout << "8. Environment-Specific Configurations" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Useful for production deployment and environment-specific configurations." << std::endl;
        std::cout << std::endl;

        // Development environment
        fastdfs::ClientConfig dev_config;
        dev_config.tracker_addrs = {argv[1]};
        dev_config.max_conns = 5;
        dev_config.connect_timeout = std::chrono::milliseconds(2000);
        dev_config.network_timeout = std::chrono::milliseconds(10000);
        dev_config.idle_timeout = std::chrono::milliseconds(30000);
        dev_config.enable_pool = true;
        dev_config.retry_count = 1;

        print_config(dev_config, "Development Environment");
        std::cout << "   → Characteristics: Fast timeouts, low connections, minimal retries" << std::endl;
        std::cout << std::endl;

        // Staging environment
        fastdfs::ClientConfig staging_config;
        staging_config.tracker_addrs = {argv[1]};
        staging_config.max_conns = 20;
        staging_config.connect_timeout = std::chrono::milliseconds(5000);
        staging_config.network_timeout = std::chrono::milliseconds(30000);
        staging_config.idle_timeout = std::chrono::milliseconds(60000);
        staging_config.enable_pool = true;
        staging_config.retry_count = 3;

        print_config(staging_config, "Staging Environment");
        std::cout << "   → Characteristics: Balanced settings, moderate timeouts" << std::endl;
        std::cout << std::endl;

        // Production environment
        fastdfs::ClientConfig prod_config;
        prod_config.tracker_addrs = {argv[1]};
        prod_config.max_conns = 50;
        prod_config.connect_timeout = std::chrono::milliseconds(10000);
        prod_config.network_timeout = std::chrono::milliseconds(60000);
        prod_config.idle_timeout = std::chrono::milliseconds(120000);
        prod_config.enable_pool = true;
        prod_config.retry_count = 5;

        print_config(prod_config, "Production Environment");
        std::cout << "   → Characteristics: High reliability, generous timeouts, more retries" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // EXAMPLE 9: Testing Configuration
        // ====================================================================
        std::cout << "9. Testing Configuration" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        std::cout << "   Testing a configuration by creating a client and performing an operation." << std::endl;
        std::cout << std::endl;

        // Use the basic configuration
        std::string validation_error2;
        if (!validate_config(basic_config, validation_error2)) {
            std::cout << "   ✗ Configuration validation failed: " << validation_error2 << std::endl;
            return 1;
        }

        std::cout << "   Creating client with validated configuration..." << std::endl;
        fastdfs::Client test_client(basic_config);
        std::cout << "   ✓ Client created successfully" << std::endl;
        std::cout << std::endl;

        // Test with a simple operation
        std::cout << "   Testing configuration with a simple upload operation..." << std::endl;
        std::string test_content = "Configuration test";
        std::vector<uint8_t> test_data(test_content.begin(), test_content.end());
        std::string test_file_id = test_client.upload_buffer(test_data, "txt", nullptr);
        std::cout << "   ✓ Upload successful: " << test_file_id << std::endl;
        
        // Clean up
        test_client.delete_file(test_file_id);
        std::cout << "   ✓ Test file deleted" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // CLEANUP
        // ====================================================================
        std::cout << "10. Cleaning up..." << std::endl;
        std::remove(config_file.c_str());
        std::cout << "   ✓ Configuration file cleaned up" << std::endl;
        std::cout << std::endl;

        // ====================================================================
        // SUMMARY
        // ====================================================================
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Summary of demonstrated features:" << std::endl;
        std::cout << "  ✓ Comprehensive client configuration options" << std::endl;
        std::cout << "  ✓ How to configure timeouts, connection pools, and retry policies" << std::endl;
        std::cout << "  ✓ Loading configuration from files and environment variables" << std::endl;
        std::cout << "  ✓ Configuration validation" << std::endl;
        std::cout << "  ✓ Production deployment and environment-specific configurations" << std::endl;
        std::cout << "  ✓ Best practices for configuration management" << std::endl;
        std::cout << std::endl;
        std::cout << "Best Practices:" << std::endl;
        std::cout << "  • Always validate configuration before creating client" << std::endl;
        std::cout << "  • Use environment variables for sensitive or environment-specific settings" << std::endl;
        std::cout << "  • Use configuration files for complex or multiple settings" << std::endl;
        std::cout << "  • Choose appropriate timeouts based on network conditions and file sizes" << std::endl;
        std::cout << "  • Configure connection pools based on expected concurrency" << std::endl;
        std::cout << "  • Set retry counts based on network reliability requirements" << std::endl;
        std::cout << "  • Use different configurations for dev, staging, and production" << std::endl;
        std::cout << "  • Test configurations before deploying to production" << std::endl;

        test_client.close();
        std::cout << std::endl << "✓ Client closed. All resources released." << std::endl;

    } catch (const fastdfs::InvalidArgumentException& e) {
        std::cerr << "Invalid configuration: " << e.what() << std::endl;
        return 1;
    } catch (const fastdfs::ConnectionException& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        std::cerr << "Please check that the tracker server is running and accessible." << std::endl;
        return 1;
    } catch (const fastdfs::TimeoutException& e) {
        std::cerr << "Timeout error: " << e.what() << std::endl;
        return 1;
    } catch (const fastdfs::FastDFSException& e) {
        std::cerr << "FastDFS error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

