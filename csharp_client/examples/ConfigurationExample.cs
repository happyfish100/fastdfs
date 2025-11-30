// ============================================================================
// FastDFS C# Client - Configuration Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates advanced configuration options in FastDFS, including
// multiple tracker servers, timeout tuning, connection pool tuning, and
// environment-specific configurations. It shows how to configure the FastDFS
// client for different scenarios, workloads, and environments.
//
// Proper configuration is essential for optimal FastDFS client performance and
// reliability. This example provides comprehensive patterns and best practices
// for configuring the client to match your specific requirements and environment.
//
// ============================================================================

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using FastDFS.Client;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating advanced configuration options in FastDFS.
    /// 
    /// This example shows:
    /// - Advanced configuration options and patterns
    /// - Multiple tracker server configuration
    /// - Timeout tuning for different scenarios
    /// - Connection pool tuning and optimization
    /// - Environment-specific configurations (development, staging, production)
    /// - Best practices for configuration management
    /// 
    /// Configuration patterns demonstrated:
    /// 1. Basic configuration with defaults
    /// 2. Multiple tracker server setup
    /// 3. Timeout tuning for different scenarios
    /// 4. Connection pool size optimization
    /// 5. Environment-specific configurations
    /// 6. Configuration validation
    /// 7. Dynamic configuration adjustment
    /// </summary>
    class ConfigurationExample
    {
        /// <summary>
        /// Main entry point for the configuration example.
        /// 
        /// This method demonstrates various configuration patterns through
        /// a series of examples, each showing different aspects of FastDFS
        /// client configuration.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Configuration Example");
            Console.WriteLine("===========================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates advanced configuration options,");
            Console.WriteLine("including multiple trackers, timeout tuning, and pool optimization.");
            Console.WriteLine();

            // ====================================================================
            // Example 1: Basic Configuration with Defaults
            // ====================================================================
            // 
            // This example demonstrates creating a basic configuration with
            // default values. Default configuration is suitable for most
            // standard use cases and provides a good starting point.
            // 
            // Default configuration values:
            // - MaxConnections: 10
            // - ConnectTimeout: 5 seconds
            // - NetworkTimeout: 30 seconds
            // - IdleTimeout: 5 minutes
            // - RetryCount: 3
            // ====================================================================

            Console.WriteLine("Example 1: Basic Configuration with Defaults");
            Console.WriteLine("===============================================");
            Console.WriteLine();

            // Create basic configuration with minimal settings
            // Only tracker addresses are required; other settings use defaults
            Console.WriteLine("Creating basic configuration with default values...");
            Console.WriteLine();

            var basicConfig = new FastDFSClientConfig
            {
                // Only required setting: tracker server addresses
                // All other settings will use default values
                TrackerAddresses = new[] { "192.168.1.100:22122" }
            };

            Console.WriteLine("Basic configuration created:");
            Console.WriteLine($"  TrackerAddresses: {string.Join(", ", basicConfig.TrackerAddresses)}");
            Console.WriteLine($"  MaxConnections: {basicConfig.MaxConnections} (default)");
            Console.WriteLine($"  ConnectTimeout: {basicConfig.ConnectTimeout.TotalSeconds} seconds (default)");
            Console.WriteLine($"  NetworkTimeout: {basicConfig.NetworkTimeout.TotalSeconds} seconds (default)");
            Console.WriteLine($"  IdleTimeout: {basicConfig.IdleTimeout.TotalMinutes} minutes (default)");
            Console.WriteLine($"  RetryCount: {basicConfig.RetryCount} (default)");
            Console.WriteLine($"  EnablePool: {basicConfig.EnablePool} (default)");
            Console.WriteLine();

            // Test basic configuration
            Console.WriteLine("Testing basic configuration...");
            try
            {
                using (var client = new FastDFSClient(basicConfig))
                {
                    Console.WriteLine("  Client created successfully with basic configuration");
                    Console.WriteLine("  Configuration is valid and ready to use");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  Configuration test failed: {ex.Message}");
            }

            Console.WriteLine();

            // ====================================================================
            // Example 2: Multiple Tracker Servers
            // ====================================================================
            // 
            // This example demonstrates configuring multiple tracker servers
            // for redundancy and load balancing. Multiple trackers provide
            // high availability and better fault tolerance.
            // 
            // Benefits of multiple trackers:
            // - High availability and redundancy
            // - Load balancing across trackers
            // - Automatic failover
            // - Better fault tolerance
            // ====================================================================

            Console.WriteLine("Example 2: Multiple Tracker Servers");
            Console.WriteLine("====================================");
            Console.WriteLine();

            // Configuration with multiple tracker servers
            // Multiple trackers provide redundancy and load balancing
            Console.WriteLine("Creating configuration with multiple tracker servers...");
            Console.WriteLine();

            var multiTrackerConfig = new FastDFSClientConfig
            {
                // Multiple tracker servers for redundancy
                // The client will use these trackers for load balancing and failover
                TrackerAddresses = new[]
                {
                    "192.168.1.100:22122",  // Primary tracker server
                    "192.168.1.101:22122",  // Secondary tracker server
                    "192.168.1.102:22122"   // Tertiary tracker server
                },

                // Standard connection pool settings
                MaxConnections = 100,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            Console.WriteLine("Multiple tracker configuration:");
            Console.WriteLine($"  Tracker servers: {multiTrackerConfig.TrackerAddresses.Length}");
            foreach (var tracker in multiTrackerConfig.TrackerAddresses)
            {
                Console.WriteLine($"    - {tracker}");
            }
            Console.WriteLine();
            Console.WriteLine("Benefits of multiple trackers:");
            Console.WriteLine("  ✓ High availability - if one tracker fails, others are available");
            Console.WriteLine("  ✓ Load balancing - requests distributed across trackers");
            Console.WriteLine("  ✓ Automatic failover - client switches to available trackers");
            Console.WriteLine("  ✓ Better fault tolerance - system continues operating");
            Console.WriteLine();

            // Test multiple tracker configuration
            Console.WriteLine("Testing multiple tracker configuration...");
            try
            {
                using (var client = new FastDFSClient(multiTrackerConfig))
                {
                    Console.WriteLine("  Client created successfully with multiple trackers");
                    Console.WriteLine("  Configuration supports redundancy and load balancing");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  Configuration test failed: {ex.Message}");
            }

            Console.WriteLine();

            // ====================================================================
            // Example 3: Timeout Tuning
            // ====================================================================
            // 
            // This example demonstrates tuning timeout values for different
            // scenarios. Proper timeout configuration is crucial for balancing
            // responsiveness and reliability in various network conditions and
            // workload types.
            // 
            // Timeout tuning scenarios:
            // - Fast network environments
            // - Slow network environments
            // - Large file operations
            // - High-latency networks
            // ====================================================================

            Console.WriteLine("Example 3: Timeout Tuning");
            Console.WriteLine("==========================");
            Console.WriteLine();

            // Scenario 1: Fast Network Environment
            // Shorter timeouts for fast networks enable faster failure detection
            Console.WriteLine("Scenario 1: Fast Network Environment");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            var fastNetworkConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,

                // Shorter timeouts for fast networks
                // Faster failure detection in reliable networks
                ConnectTimeout = TimeSpan.FromSeconds(2),   // Shorter connection timeout
                NetworkTimeout = TimeSpan.FromSeconds(10), // Shorter network timeout
                IdleTimeout = TimeSpan.FromMinutes(3),     // Shorter idle timeout

                RetryCount = 2  // Fewer retries needed in fast networks
            };

            Console.WriteLine("Fast network configuration:");
            Console.WriteLine($"  ConnectTimeout: {fastNetworkConfig.ConnectTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  NetworkTimeout: {fastNetworkConfig.NetworkTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  IdleTimeout: {fastNetworkConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine($"  RetryCount: {fastNetworkConfig.RetryCount}");
            Console.WriteLine();
            Console.WriteLine("Use case: Fast, reliable local network");
            Console.WriteLine("Benefits: Faster failure detection, better responsiveness");
            Console.WriteLine();

            // Scenario 2: Slow Network Environment
            // Longer timeouts for slow networks accommodate network delays
            Console.WriteLine("Scenario 2: Slow Network Environment");
            Console.WriteLine("-------------------------------------");
            Console.WriteLine();

            var slowNetworkConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,

                // Longer timeouts for slow networks
                // Accommodate network delays and latency
                ConnectTimeout = TimeSpan.FromSeconds(10),  // Longer connection timeout
                NetworkTimeout = TimeSpan.FromSeconds(120), // Longer network timeout
                IdleTimeout = TimeSpan.FromMinutes(10),    // Longer idle timeout

                RetryCount = 5  // More retries for unreliable networks
            };

            Console.WriteLine("Slow network configuration:");
            Console.WriteLine($"  ConnectTimeout: {slowNetworkConfig.ConnectTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  NetworkTimeout: {slowNetworkConfig.NetworkTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  IdleTimeout: {slowNetworkConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine($"  RetryCount: {slowNetworkConfig.RetryCount}");
            Console.WriteLine();
            Console.WriteLine("Use case: Slow, unreliable, or high-latency networks");
            Console.WriteLine("Benefits: Accommodates network delays, reduces false failures");
            Console.WriteLine();

            // Scenario 3: Large File Operations
            // Longer network timeout for large file transfers
            Console.WriteLine("Scenario 3: Large File Operations");
            Console.WriteLine("----------------------------------");
            Console.WriteLine();

            var largeFileConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 150,  // More connections for concurrent large file ops

                // Standard connection timeout
                ConnectTimeout = TimeSpan.FromSeconds(5),

                // Longer network timeout for large file transfers
                // Large files need more time to transfer
                NetworkTimeout = TimeSpan.FromSeconds(300), // 5 minutes for large files

                // Longer idle timeout to maintain connections
                IdleTimeout = TimeSpan.FromMinutes(15),

                RetryCount = 3
            };

            Console.WriteLine("Large file configuration:");
            Console.WriteLine($"  MaxConnections: {largeFileConfig.MaxConnections}");
            Console.WriteLine($"  ConnectTimeout: {largeFileConfig.ConnectTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  NetworkTimeout: {largeFileConfig.NetworkTimeout.TotalSeconds} seconds ({largeFileConfig.NetworkTimeout.TotalMinutes} minutes)");
            Console.WriteLine($"  IdleTimeout: {largeFileConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine();
            Console.WriteLine("Use case: Large file uploads/downloads (GB+ files)");
            Console.WriteLine("Benefits: Accommodates long transfer times, maintains connections");
            Console.WriteLine();

            // Scenario 4: High-Latency Network
            // Extended timeouts for high-latency networks (e.g., WAN, cloud)
            Console.WriteLine("Scenario 4: High-Latency Network");
            Console.WriteLine("--------------------------------");
            Console.WriteLine();

            var highLatencyConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,

                // Extended timeouts for high latency
                // Accommodate round-trip delays
                ConnectTimeout = TimeSpan.FromSeconds(15),  // Extended connection timeout
                NetworkTimeout = TimeSpan.FromSeconds(180), // Extended network timeout
                IdleTimeout = TimeSpan.FromMinutes(10),

                RetryCount = 4  // More retries for high-latency networks
            };

            Console.WriteLine("High-latency network configuration:");
            Console.WriteLine($"  ConnectTimeout: {highLatencyConfig.ConnectTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  NetworkTimeout: {highLatencyConfig.NetworkTimeout.TotalSeconds} seconds ({highLatencyConfig.NetworkTimeout.TotalMinutes} minutes)");
            Console.WriteLine($"  IdleTimeout: {highLatencyConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine($"  RetryCount: {highLatencyConfig.RetryCount}");
            Console.WriteLine();
            Console.WriteLine("Use case: High-latency networks (WAN, cloud, inter-region)");
            Console.WriteLine("Benefits: Accommodates latency, reduces timeout errors");
            Console.WriteLine();

            // ====================================================================
            // Example 4: Connection Pool Tuning
            // ====================================================================
            // 
            // This example demonstrates tuning connection pool settings for
            // different workloads. Connection pool tuning is essential for
            // optimizing performance and resource usage.
            // 
            // Connection pool tuning factors:
            // - Concurrent operation requirements
            // - Server capacity
            // - Resource constraints
            // - Workload characteristics
            // ====================================================================

            Console.WriteLine("Example 4: Connection Pool Tuning");
            Console.WriteLine("===================================");
            Console.WriteLine();

            // Scenario 1: Low Concurrency Workload
            // Small connection pool for low-concurrency scenarios
            Console.WriteLine("Scenario 1: Low Concurrency Workload");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            var lowConcurrencyConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },

                // Small connection pool for low concurrency
                // Fewer connections reduce resource usage
                MaxConnections = 10,

                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Shorter idle timeout for low-concurrency scenarios
                // Connections closed sooner to free resources
                IdleTimeout = TimeSpan.FromMinutes(3),

                RetryCount = 3
            };

            Console.WriteLine("Low concurrency configuration:");
            Console.WriteLine($"  MaxConnections: {lowConcurrencyConfig.MaxConnections}");
            Console.WriteLine($"  IdleTimeout: {lowConcurrencyConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine();
            Console.WriteLine("Use case: Low-concurrency applications (few simultaneous operations)");
            Console.WriteLine("Benefits: Lower resource usage, sufficient for low load");
            Console.WriteLine();

            // Scenario 2: Medium Concurrency Workload
            // Medium connection pool for moderate concurrency
            Console.WriteLine("Scenario 2: Medium Concurrency Workload");
            Console.WriteLine("----------------------------------------");
            Console.WriteLine();

            var mediumConcurrencyConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },

                // Medium connection pool for moderate concurrency
                // Balances performance and resource usage
                MaxConnections = 50,

                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Medium idle timeout
                // Maintains connections for better reuse
                IdleTimeout = TimeSpan.FromMinutes(5),

                RetryCount = 3
            };

            Console.WriteLine("Medium concurrency configuration:");
            Console.WriteLine($"  MaxConnections: {mediumConcurrencyConfig.MaxConnections}");
            Console.WriteLine($"  IdleTimeout: {mediumConcurrencyConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine();
            Console.WriteLine("Use case: Medium-concurrency applications (moderate simultaneous operations)");
            Console.WriteLine("Benefits: Balanced performance and resource usage");
            Console.WriteLine();

            // Scenario 3: High Concurrency Workload
            // Large connection pool for high concurrency
            Console.WriteLine("Scenario 3: High Concurrency Workload");
            Console.WriteLine("---------------------------------------");
            Console.WriteLine();

            var highConcurrencyConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },

                // Large connection pool for high concurrency
                // More connections allow higher throughput
                MaxConnections = 200,

                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(60),  // Longer for concurrent ops

                // Longer idle timeout for high-concurrency scenarios
                // Maintains connections for rapid reuse
                IdleTimeout = TimeSpan.FromMinutes(10),

                RetryCount = 3
            };

            Console.WriteLine("High concurrency configuration:");
            Console.WriteLine($"  MaxConnections: {highConcurrencyConfig.MaxConnections}");
            Console.WriteLine($"  NetworkTimeout: {highConcurrencyConfig.NetworkTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  IdleTimeout: {highConcurrencyConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine();
            Console.WriteLine("Use case: High-concurrency applications (many simultaneous operations)");
            Console.WriteLine("Benefits: Higher throughput, better concurrent operation support");
            Console.WriteLine();

            // Scenario 4: Resource-Constrained Environment
            // Minimal connection pool for resource-constrained environments
            Console.WriteLine("Scenario 4: Resource-Constrained Environment");
            Console.WriteLine("----------------------------------------------");
            Console.WriteLine();

            var resourceConstrainedConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },

                // Minimal connection pool for resource constraints
                // Reduces memory and connection usage
                MaxConnections = 5,

                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),

                // Short idle timeout to free resources quickly
                IdleTimeout = TimeSpan.FromMinutes(2),

                RetryCount = 2  // Fewer retries to reduce resource usage
            };

            Console.WriteLine("Resource-constrained configuration:");
            Console.WriteLine($"  MaxConnections: {resourceConstrainedConfig.MaxConnections}");
            Console.WriteLine($"  IdleTimeout: {resourceConstrainedConfig.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine($"  RetryCount: {resourceConstrainedConfig.RetryCount}");
            Console.WriteLine();
            Console.WriteLine("Use case: Resource-constrained environments (limited memory/connections)");
            Console.WriteLine("Benefits: Minimal resource usage, suitable for constrained systems");
            Console.WriteLine();

            // ====================================================================
            // Example 5: Environment-Specific Configurations
            // ====================================================================
            // 
            // This example demonstrates creating environment-specific
            // configurations for development, staging, and production
            // environments. Different environments often require different
            // configuration settings.
            // 
            // Environment-specific considerations:
            // - Development: Relaxed settings, debugging-friendly
            // - Staging: Production-like settings for testing
            // - Production: Optimized settings for performance and reliability
            // ====================================================================

            Console.WriteLine("Example 5: Environment-Specific Configurations");
            Console.WriteLine("=================================================");
            Console.WriteLine();

            // Development Environment Configuration
            // Relaxed settings suitable for development and debugging
            Console.WriteLine("Development Environment Configuration");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            var devConfig = CreateDevelopmentConfig();
            DisplayConfiguration("Development", devConfig);
            Console.WriteLine();
            Console.WriteLine("Development environment characteristics:");
            Console.WriteLine("  - Relaxed timeout settings for debugging");
            Console.WriteLine("  - Smaller connection pool (sufficient for dev)");
            Console.WriteLine("  - Single tracker server (typical in dev)");
            Console.WriteLine("  - Lower retry count (faster failure detection)");
            Console.WriteLine();

            // Staging Environment Configuration
            // Production-like settings for testing and validation
            Console.WriteLine("Staging Environment Configuration");
            Console.WriteLine("-----------------------------------");
            Console.WriteLine();

            var stagingConfig = CreateStagingConfig();
            DisplayConfiguration("Staging", stagingConfig);
            Console.WriteLine();
            Console.WriteLine("Staging environment characteristics:");
            Console.WriteLine("  - Production-like timeout settings");
            Console.WriteLine("  - Medium connection pool (test production load)");
            Console.WriteLine("  - Multiple tracker servers (test redundancy)");
            Console.WriteLine("  - Standard retry count");
            Console.WriteLine();

            // Production Environment Configuration
            // Optimized settings for performance and reliability
            Console.WriteLine("Production Environment Configuration");
            Console.WriteLine("-------------------------------------");
            Console.WriteLine();

            var productionConfig = CreateProductionConfig();
            DisplayConfiguration("Production", productionConfig);
            Console.WriteLine();
            Console.WriteLine("Production environment characteristics:");
            Console.WriteLine("  - Optimized timeout settings");
            Console.WriteLine("  - Large connection pool (high throughput)");
            Console.WriteLine("  - Multiple tracker servers (high availability)");
            Console.WriteLine("  - Appropriate retry count (reliability)");
            Console.WriteLine();

            // Test Environment Configuration
            // Settings optimized for automated testing
            Console.WriteLine("Test Environment Configuration");
            Console.WriteLine("-------------------------------");
            Console.WriteLine();

            var testConfig = CreateTestConfig();
            DisplayConfiguration("Test", testConfig);
            Console.WriteLine();
            Console.WriteLine("Test environment characteristics:");
            Console.WriteLine("  - Fast timeout settings (quick test execution)");
            Console.WriteLine("  - Small connection pool (sufficient for tests)");
            Console.WriteLine("  - Single tracker server (typical in test)");
            Console.WriteLine("  - Minimal retry count (faster test failures)");
            Console.WriteLine();

            // ====================================================================
            // Example 6: Configuration Validation
            // ====================================================================
            // 
            // This example demonstrates validating configurations before use.
            // Configuration validation helps catch configuration errors early
            // and ensures the client is properly configured.
            // 
            // Validation aspects:
            // - Required fields validation
            // - Value range validation
            // - Format validation
            // - Consistency validation
            // ====================================================================

            Console.WriteLine("Example 6: Configuration Validation");
            Console.WriteLine("=====================================");
            Console.WriteLine();

            // Valid configuration
            Console.WriteLine("Testing valid configuration...");
            Console.WriteLine();

            var validConfig = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            try
            {
                validConfig.Validate();
                Console.WriteLine("  ✓ Configuration is valid");
                Console.WriteLine("  ✓ All required fields are set");
                Console.WriteLine("  ✓ All values are within acceptable ranges");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  ✗ Validation failed: {ex.Message}");
            }

            Console.WriteLine();

            // Invalid configuration examples
            Console.WriteLine("Testing invalid configurations...");
            Console.WriteLine();

            // Missing tracker addresses
            Console.WriteLine("Test 1: Missing tracker addresses");
            try
            {
                var invalidConfig1 = new FastDFSClientConfig
                {
                    // TrackerAddresses not set - will fail validation
                    MaxConnections = 100
                };

                invalidConfig1.Validate();
                Console.WriteLine("  ✗ Validation should have failed");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  ✓ Validation correctly failed: {ex.Message}");
            }

            Console.WriteLine();

            // Invalid MaxConnections
            Console.WriteLine("Test 2: Invalid MaxConnections (zero)");
            try
            {
                var invalidConfig2 = new FastDFSClientConfig
                {
                    TrackerAddresses = new[] { "192.168.1.100:22122" },
                    MaxConnections = 0  // Invalid: must be > 0
                };

                invalidConfig2.Validate();
                Console.WriteLine("  ✗ Validation should have failed");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  ✓ Validation correctly failed: {ex.Message}");
            }

            Console.WriteLine();

            // Invalid timeout
            Console.WriteLine("Test 3: Invalid timeout (zero)");
            try
            {
                var invalidConfig3 = new FastDFSClientConfig
                {
                    TrackerAddresses = new[] { "192.168.1.100:22122" },
                    ConnectTimeout = TimeSpan.Zero  // Invalid: must be > 0
                };

                invalidConfig3.Validate();
                Console.WriteLine("  ✗ Validation should have failed");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  ✓ Validation correctly failed: {ex.Message}");
            }

            Console.WriteLine();

            // ====================================================================
            // Example 7: Dynamic Configuration Adjustment
            // ====================================================================
            // 
            // This example demonstrates creating configurations dynamically based
            // on runtime conditions, environment variables, or configuration files.
            // Dynamic configuration enables flexible deployment and environment
            // adaptation.
            // 
            // Dynamic configuration patterns:
            // - Environment variable-based configuration
            // - Configuration file-based configuration
            // - Runtime condition-based configuration
            // - Adaptive configuration
            // ====================================================================

            Console.WriteLine("Example 7: Dynamic Configuration Adjustment");
            Console.WriteLine("=============================================");
            Console.WriteLine();

            // Pattern 1: Environment variable-based configuration
            Console.WriteLine("Pattern 1: Environment Variable-Based Configuration");
            Console.WriteLine("----------------------------------------------------");
            Console.WriteLine();

            var envBasedConfig = CreateConfigFromEnvironment();
            DisplayConfiguration("Environment-Based", envBasedConfig);
            Console.WriteLine();

            // Pattern 2: Configuration file-based configuration
            Console.WriteLine("Pattern 2: Configuration File-Based Configuration");
            Console.WriteLine("---------------------------------------------------");
            Console.WriteLine();

            var fileBasedConfig = CreateConfigFromFile();
            DisplayConfiguration("File-Based", fileBasedConfig);
            Console.WriteLine();

            // Pattern 3: Runtime condition-based configuration
            Console.WriteLine("Pattern 3: Runtime Condition-Based Configuration");
            Console.WriteLine("--------------------------------------------------");
            Console.WriteLine();

            // Determine configuration based on runtime conditions
            var isProduction = Environment.GetEnvironmentVariable("ENVIRONMENT") == "production";
            var isHighLoad = GetCurrentLoadLevel() > 0.8;

            var adaptiveConfig = new FastDFSClientConfig
            {
                TrackerAddresses = isProduction
                    ? new[] { "prod-tracker-1:22122", "prod-tracker-2:22122" }
                    : new[] { "dev-tracker:22122" },

                MaxConnections = isHighLoad ? 200 : 50,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = isHighLoad ? TimeSpan.FromSeconds(60) : TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = isProduction ? 3 : 2
            };

            Console.WriteLine("Adaptive configuration:");
            Console.WriteLine($"  Environment: {(isProduction ? "Production" : "Development")}");
            Console.WriteLine($"  Load level: {(isHighLoad ? "High" : "Normal")}");
            DisplayConfiguration("Adaptive", adaptiveConfig);
            Console.WriteLine();

            // ====================================================================
            // Best Practices Summary
            // ====================================================================
            // 
            // This section summarizes best practices for FastDFS client
            // configuration, based on the examples above.
            // ====================================================================

            Console.WriteLine("Best Practices for FastDFS Client Configuration");
            Console.WriteLine("==================================================");
            Console.WriteLine();
            Console.WriteLine("1. Multiple Tracker Servers:");
            Console.WriteLine("   - Use multiple trackers for high availability");
            Console.WriteLine("   - Distribute trackers across different servers");
            Console.WriteLine("   - Test failover behavior");
            Console.WriteLine("   - Monitor tracker health");
            Console.WriteLine();
            Console.WriteLine("2. Timeout Tuning:");
            Console.WriteLine("   - Match timeouts to network characteristics");
            Console.WriteLine("   - Use shorter timeouts for fast networks");
            Console.WriteLine("   - Use longer timeouts for slow/high-latency networks");
            Console.WriteLine("   - Increase network timeout for large file operations");
            Console.WriteLine("   - Test timeout values in your environment");
            Console.WriteLine();
            Console.WriteLine("3. Connection Pool Tuning:");
            Console.WriteLine("   - Match pool size to concurrent operation needs");
            Console.WriteLine("   - Start with conservative values and tune based on metrics");
            Console.WriteLine("   - Consider server capacity when setting pool size");
            Console.WriteLine("   - Monitor connection pool usage");
            Console.WriteLine("   - Adjust based on actual workload patterns");
            Console.WriteLine();
            Console.WriteLine("4. Environment-Specific Configuration:");
            Console.WriteLine("   - Use different configs for dev, staging, and production");
            Console.WriteLine("   - Store configs in environment-specific files");
            Console.WriteLine("   - Use environment variables for sensitive settings");
            Console.WriteLine("   - Document configuration differences");
            Console.WriteLine("   - Test configurations in each environment");
            Console.WriteLine();
            Console.WriteLine("5. Configuration Validation:");
            Console.WriteLine("   - Validate configurations before creating clients");
            Console.WriteLine("   - Check required fields are set");
            Console.WriteLine("   - Verify value ranges are appropriate");
            Console.WriteLine("   - Test invalid configurations to ensure proper error handling");
            Console.WriteLine();
            Console.WriteLine("6. Dynamic Configuration:");
            Console.WriteLine("   - Support environment variable-based configuration");
            Console.WriteLine("   - Load configuration from files when appropriate");
            Console.WriteLine("   - Adapt configuration based on runtime conditions");
            Console.WriteLine("   - Provide configuration defaults");
            Console.WriteLine();
            Console.WriteLine("7. Performance Optimization:");
            Console.WriteLine("   - Tune timeouts for your network conditions");
            Console.WriteLine("   - Optimize connection pool size for your workload");
            Console.WriteLine("   - Monitor and adjust based on metrics");
            Console.WriteLine("   - Test different configurations to find optimal values");
            Console.WriteLine();
            Console.WriteLine("8. Reliability Configuration:");
            Console.WriteLine("   - Use multiple trackers for redundancy");
            Console.WriteLine("   - Set appropriate retry counts");
            Console.WriteLine("   - Configure timeouts to handle network variability");
            Console.WriteLine("   - Test failover scenarios");
            Console.WriteLine();
            Console.WriteLine("9. Resource Management:");
            Console.WriteLine("   - Balance connection pool size with resource constraints");
            Console.WriteLine("   - Configure idle timeout appropriately");
            Console.WriteLine("   - Monitor resource usage");
            Console.WriteLine("   - Adjust for resource-constrained environments");
            Console.WriteLine();
            Console.WriteLine("10. Best Practices Summary:");
            Console.WriteLine("    - Use multiple trackers for high availability");
            Console.WriteLine("    - Tune timeouts for your network conditions");
            Console.WriteLine("    - Optimize connection pool for your workload");
            Console.WriteLine("    - Use environment-specific configurations");
            Console.WriteLine("    - Validate configurations before use");
            Console.WriteLine();

            Console.WriteLine("All examples completed successfully!");
        }

        // ====================================================================
        // Helper Methods for Configuration Creation
        // ====================================================================

        /// <summary>
        /// Creates a development environment configuration.
        /// 
        /// Development configurations typically have relaxed settings suitable
        /// for debugging and development workflows.
        /// </summary>
        /// <returns>
        /// A FastDFSClientConfig configured for development environment.
        /// </returns>
        static FastDFSClientConfig CreateDevelopmentConfig()
        {
            return new FastDFSClientConfig
            {
                // Single tracker server is typical in development
                TrackerAddresses = new[] { "localhost:22122" },

                // Smaller connection pool sufficient for development
                MaxConnections = 20,

                // Relaxed timeouts for debugging
                ConnectTimeout = TimeSpan.FromSeconds(10),
                NetworkTimeout = TimeSpan.FromSeconds(60),

                // Shorter idle timeout to free resources
                IdleTimeout = TimeSpan.FromMinutes(3),

                // Lower retry count for faster failure detection in dev
                RetryCount = 2
            };
        }

        /// <summary>
        /// Creates a staging environment configuration.
        /// 
        /// Staging configurations should mirror production settings to
        /// validate production-like behavior.
        /// </summary>
        /// <returns>
        /// A FastDFSClientConfig configured for staging environment.
        /// </returns>
        static FastDFSClientConfig CreateStagingConfig()
        {
            return new FastDFSClientConfig
            {
                // Multiple trackers for testing redundancy
                TrackerAddresses = new[]
                {
                    "staging-tracker-1:22122",
                    "staging-tracker-2:22122"
                },

                // Medium connection pool for staging testing
                MaxConnections = 75,

                // Production-like timeouts
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(45),

                // Standard idle timeout
                IdleTimeout = TimeSpan.FromMinutes(5),

                // Standard retry count
                RetryCount = 3
            };
        }

        /// <summary>
        /// Creates a production environment configuration.
        /// 
        /// Production configurations should be optimized for performance,
        /// reliability, and high availability.
        /// </summary>
        /// <returns>
        /// A FastDFSClientConfig configured for production environment.
        /// </returns>
        static FastDFSClientConfig CreateProductionConfig()
        {
            return new FastDFSClientConfig
            {
                // Multiple trackers for high availability
                TrackerAddresses = new[]
                {
                    "prod-tracker-1:22122",
                    "prod-tracker-2:22122",
                    "prod-tracker-3:22122"
                },

                // Large connection pool for high throughput
                MaxConnections = 200,

                // Optimized timeouts for production
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(60),

                // Longer idle timeout for better connection reuse
                IdleTimeout = TimeSpan.FromMinutes(10),

                // Appropriate retry count for reliability
                RetryCount = 3
            };
        }

        /// <summary>
        /// Creates a test environment configuration.
        /// 
        /// Test configurations should be optimized for fast test execution
        /// while maintaining sufficient functionality for testing.
        /// </summary>
        /// <returns>
        /// A FastDFSClientConfig configured for test environment.
        /// </returns>
        static FastDFSClientConfig CreateTestConfig()
        {
            return new FastDFSClientConfig
            {
                // Single tracker server typical in test environments
                TrackerAddresses = new[] { "test-tracker:22122" },

                // Small connection pool sufficient for tests
                MaxConnections = 10,

                // Fast timeouts for quick test execution
                ConnectTimeout = TimeSpan.FromSeconds(2),
                NetworkTimeout = TimeSpan.FromSeconds(10),

                // Short idle timeout
                IdleTimeout = TimeSpan.FromMinutes(1),

                // Minimal retry count for faster test failures
                RetryCount = 1
            };
        }

        /// <summary>
        /// Creates a configuration from environment variables.
        /// 
        /// This method demonstrates loading configuration from environment
        /// variables, which is useful for containerized deployments and
        /// cloud environments.
        /// </summary>
        /// <returns>
        /// A FastDFSClientConfig created from environment variables.
        /// </returns>
        static FastDFSClientConfig CreateConfigFromEnvironment()
        {
            // Read tracker addresses from environment variable
            // Format: "host1:port1,host2:port2,host3:port3"
            var trackerEnv = Environment.GetEnvironmentVariable("FASTDFS_TRACKERS");
            var trackers = !string.IsNullOrEmpty(trackerEnv)
                ? trackerEnv.Split(',').Select(t => t.Trim()).ToArray()
                : new[] { "192.168.1.100:22122" };  // Default

            // Read other settings from environment variables
            var maxConnectionsEnv = Environment.GetEnvironmentVariable("FASTDFS_MAX_CONNECTIONS");
            var maxConnections = !string.IsNullOrEmpty(maxConnectionsEnv) && int.TryParse(maxConnectionsEnv, out int mc)
                ? mc
                : 100;  // Default

            var connectTimeoutEnv = Environment.GetEnvironmentVariable("FASTDFS_CONNECT_TIMEOUT");
            var connectTimeout = !string.IsNullOrEmpty(connectTimeoutEnv) && int.TryParse(connectTimeoutEnv, out int ct)
                ? TimeSpan.FromSeconds(ct)
                : TimeSpan.FromSeconds(5);  // Default

            var networkTimeoutEnv = Environment.GetEnvironmentVariable("FASTDFS_NETWORK_TIMEOUT");
            var networkTimeout = !string.IsNullOrEmpty(networkTimeoutEnv) && int.TryParse(networkTimeoutEnv, out int nt)
                ? TimeSpan.FromSeconds(nt)
                : TimeSpan.FromSeconds(30);  // Default

            return new FastDFSClientConfig
            {
                TrackerAddresses = trackers,
                MaxConnections = maxConnections,
                ConnectTimeout = connectTimeout,
                NetworkTimeout = networkTimeout,
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };
        }

        /// <summary>
        /// Creates a configuration from a configuration file.
        /// 
        /// This method demonstrates loading configuration from a file,
        /// which is useful for application configuration management.
        /// </summary>
        /// <returns>
        /// A FastDFSClientConfig created from configuration file.
        /// </returns>
        static FastDFSClientConfig CreateConfigFromFile()
        {
            // In a real scenario, you would read from a configuration file
            // (e.g., appsettings.json, config.xml, etc.)
            // For this example, we'll use a simple approach

            var configFile = "fastdfs_config.txt";
            if (File.Exists(configFile))
            {
                // Read configuration from file
                // Format: key=value (one per line)
                var configLines = File.ReadAllLines(configFile);
                var configDict = new Dictionary<string, string>();

                foreach (var line in configLines)
                {
                    if (string.IsNullOrWhiteSpace(line) || line.StartsWith("#"))
                        continue;

                    var parts = line.Split('=', 2);
                    if (parts.Length == 2)
                    {
                        configDict[parts[0].Trim()] = parts[1].Trim();
                    }
                }

                // Build configuration from file values
                var trackers = configDict.ContainsKey("trackers")
                    ? configDict["trackers"].Split(',').Select(t => t.Trim()).ToArray()
                    : new[] { "192.168.1.100:22122" };

                var maxConnections = configDict.ContainsKey("max_connections") && int.TryParse(configDict["max_connections"], out int mc)
                    ? mc
                    : 100;

                var connectTimeout = configDict.ContainsKey("connect_timeout") && int.TryParse(configDict["connect_timeout"], out int ct)
                    ? TimeSpan.FromSeconds(ct)
                    : TimeSpan.FromSeconds(5);

                var networkTimeout = configDict.ContainsKey("network_timeout") && int.TryParse(configDict["network_timeout"], out int nt)
                    ? TimeSpan.FromSeconds(nt)
                    : TimeSpan.FromSeconds(30);

                return new FastDFSClientConfig
                {
                    TrackerAddresses = trackers,
                    MaxConnections = maxConnections,
                    ConnectTimeout = connectTimeout,
                    NetworkTimeout = networkTimeout,
                    IdleTimeout = TimeSpan.FromMinutes(5),
                    RetryCount = 3
                };
            }

            // Return default configuration if file doesn't exist
            return new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };
        }

        /// <summary>
        /// Gets the current system load level (simulated).
        /// 
        /// In a real scenario, this would query actual system metrics.
        /// </summary>
        /// <returns>
        /// A value between 0.0 and 1.0 representing the current load level.
        /// </returns>
        static double GetCurrentLoadLevel()
        {
            // Simulate load level detection
            // In real scenario, this would query CPU, memory, or connection metrics
            return 0.5;  // 50% load (simulated)
        }

        /// <summary>
        /// Displays configuration details in a formatted manner.
        /// 
        /// This helper method provides a consistent way to display
        /// configuration information across examples.
        /// </summary>
        /// <param name="name">
        /// The name of the configuration (e.g., "Development", "Production").
        /// </param>
        /// <param name="config">
        /// The FastDFSClientConfig to display.
        /// </param>
        static void DisplayConfiguration(string name, FastDFSClientConfig config)
        {
            Console.WriteLine($"{name} configuration:");
            Console.WriteLine($"  TrackerAddresses: {string.Join(", ", config.TrackerAddresses)}");
            Console.WriteLine($"  MaxConnections: {config.MaxConnections}");
            Console.WriteLine($"  ConnectTimeout: {config.ConnectTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  NetworkTimeout: {config.NetworkTimeout.TotalSeconds} seconds");
            Console.WriteLine($"  IdleTimeout: {config.IdleTimeout.TotalMinutes} minutes");
            Console.WriteLine($"  RetryCount: {config.RetryCount}");
            Console.WriteLine($"  EnablePool: {config.EnablePool}");
        }
    }
}

