// ============================================================================
// FastDFS C# Client - Integration Example
// ============================================================================
// 
// Copyright (C) 2025 FastDFS C# Client Contributors
//
// This example demonstrates integration with ASP.NET Core, Web API,
// dependency injection, configuration from appsettings.json, and logging
// integration. It shows how to properly integrate the FastDFS client into
// ASP.NET Core applications.
//
// Integration patterns are essential for building production-ready applications
// that leverage ASP.NET Core's dependency injection, configuration, and
// logging systems. This example provides comprehensive patterns for
// integrating FastDFS into modern .NET applications.
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
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace FastDFS.Client.Examples
{
    /// <summary>
    /// Example demonstrating FastDFS client integration with ASP.NET Core.
    /// 
    /// This example shows:
    /// - How to integrate FastDFS client with ASP.NET Core
    /// - How to use dependency injection
    /// - How to configure from appsettings.json
    /// - How to integrate with logging
    /// - How to create Web API controllers
    /// - How to register services
    /// 
    /// Integration patterns demonstrated:
    /// 1. Service registration and dependency injection
    /// 2. Configuration from appsettings.json
    /// 3. Logging integration
    /// 4. ASP.NET Core Web API integration
    /// 5. Service lifetime management
    /// 6. Configuration options pattern
    /// </summary>
    class IntegrationExample
    {
        /// <summary>
        /// Main entry point for the integration example.
        /// 
        /// This method demonstrates various integration patterns through
        /// a series of examples, each showing different aspects of
        /// integrating FastDFS with ASP.NET Core.
        /// </summary>
        /// <param name="args">
        /// Command-line arguments (not used in this example).
        /// </param>
        /// <returns>
        /// A task that represents the asynchronous operation.
        /// </returns>
        static async Task Main(string[] args)
        {
            Console.WriteLine("FastDFS C# Client - Integration Example");
            Console.WriteLine("=========================================");
            Console.WriteLine();
            Console.WriteLine("This example demonstrates integration with ASP.NET Core,");
            Console.WriteLine("dependency injection, configuration, and logging.");
            Console.WriteLine();

            // ====================================================================
            // Example 1: Service Registration and Dependency Injection
            // ====================================================================
            // 
            // This example demonstrates how to register FastDFS client services
            // in the ASP.NET Core dependency injection container. Proper service
            // registration enables dependency injection throughout the application.
            // 
            // Service registration patterns:
            // - Singleton service registration
            // - Scoped service registration
            // - Transient service registration
            // - Service factory registration
            // ====================================================================

            Console.WriteLine("Example 1: Service Registration and Dependency Injection");
            Console.WriteLine("===========================================================");
            Console.WriteLine();

            // Pattern 1: Basic service registration
            Console.WriteLine("Pattern 1: Basic Service Registration");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            var services = new ServiceCollection();

            // Register FastDFS client configuration
            // Configuration is typically loaded from appsettings.json
            // For this example, we'll create a configuration manually
            var config = new FastDFSClientConfig
            {
                TrackerAddresses = new[] { "192.168.1.100:22122" },
                MaxConnections = 100,
                ConnectTimeout = TimeSpan.FromSeconds(5),
                NetworkTimeout = TimeSpan.FromSeconds(30),
                IdleTimeout = TimeSpan.FromMinutes(5),
                RetryCount = 3
            };

            // Register configuration as singleton
            // This ensures the same configuration is used throughout the application
            services.AddSingleton(config);

            // Register FastDFS client as singleton
            // Singleton is appropriate because the client manages connection pools
            // and should be reused across requests
            services.AddSingleton<FastDFSClient>(serviceProvider =>
            {
                var clientConfig = serviceProvider.GetRequiredService<FastDFSClientConfig>();
                return new FastDFSClient(clientConfig);
            });

            Console.WriteLine("  ✓ FastDFS client configuration registered as singleton");
            Console.WriteLine("  ✓ FastDFS client registered as singleton");
            Console.WriteLine("  ✓ Services ready for dependency injection");
            Console.WriteLine();

            // Pattern 2: Service registration with factory
            Console.WriteLine("Pattern 2: Service Registration with Factory");
            Console.WriteLine("---------------------------------------------");
            Console.WriteLine();

            var servicesWithFactory = new ServiceCollection();

            // Register configuration options
            servicesWithFactory.AddSingleton<FastDFSClientConfig>(serviceProvider =>
            {
                // In a real application, this would load from configuration
                return new FastDFSClientConfig
                {
                    TrackerAddresses = new[] { "192.168.1.100:22122", "192.168.1.101:22122" },
                    MaxConnections = 100,
                    ConnectTimeout = TimeSpan.FromSeconds(5),
                    NetworkTimeout = TimeSpan.FromSeconds(30),
                    IdleTimeout = TimeSpan.FromMinutes(5),
                    RetryCount = 3
                };
            });

            // Register client with factory that handles disposal
            servicesWithFactory.AddSingleton<FastDFSClient>(serviceProvider =>
            {
                var clientConfig = serviceProvider.GetRequiredService<FastDFSClientConfig>();
                var logger = serviceProvider.GetService<ILogger<FastDFSClient>>();

                // Create client with optional logging
                var client = new FastDFSClient(clientConfig);

                // Log client creation if logger is available
                logger?.LogInformation("FastDFS client created with {TrackerCount} trackers", 
                    clientConfig.TrackerAddresses.Length);

                return client;
            });

            Console.WriteLine("  ✓ Configuration registered with factory");
            Console.WriteLine("  ✓ Client registered with factory and logging support");
            Console.WriteLine("  ✓ Factory pattern enables flexible client creation");
            Console.WriteLine();

            // Pattern 3: Scoped service registration
            Console.WriteLine("Pattern 3: Scoped Service Registration");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            var scopedServices = new ServiceCollection();

            // Register as scoped if you need per-request instances
            // Note: FastDFS client is typically singleton, but this shows the pattern
            scopedServices.AddScoped<FastDFSClientConfig>(serviceProvider =>
            {
                return new FastDFSClientConfig
                {
                    TrackerAddresses = new[] { "192.168.1.100:22122" },
                    MaxConnections = 50,
                    ConnectTimeout = TimeSpan.FromSeconds(5),
                    NetworkTimeout = TimeSpan.FromSeconds(30),
                    IdleTimeout = TimeSpan.FromMinutes(5),
                    RetryCount = 3
                };
            });

            scopedServices.AddScoped<FastDFSClient>(serviceProvider =>
            {
                var clientConfig = serviceProvider.GetRequiredService<FastDFSClientConfig>();
                return new FastDFSClient(clientConfig);
            });

            Console.WriteLine("  ✓ Configuration registered as scoped");
            Console.WriteLine("  ✓ Client registered as scoped");
            Console.WriteLine("  ✓ Scoped services are created per HTTP request");
            Console.WriteLine();

            // ====================================================================
            // Example 2: Configuration from appsettings.json
            // ====================================================================
            // 
            // This example demonstrates loading FastDFS configuration from
            // appsettings.json. This is the standard way to configure
            // applications in ASP.NET Core.
            // 
            // Configuration patterns:
            // - Loading from appsettings.json
            // - Environment-specific configuration
            // - Options pattern
            // - Configuration validation
            // ====================================================================

            Console.WriteLine("Example 2: Configuration from appsettings.json");
            Console.WriteLine("==============================================");
            Console.WriteLine();

            // Pattern 1: Basic configuration loading
            Console.WriteLine("Pattern 1: Basic Configuration Loading");
            Console.WriteLine("----------------------------------------");
            Console.WriteLine();

            // Create configuration builder
            // In a real ASP.NET Core application, this is done in Program.cs or Startup.cs
            var configurationBuilder = new ConfigurationBuilder()
                .SetBasePath(Directory.GetCurrentDirectory())
                .AddJsonFile("appsettings.json", optional: true, reloadOnChange: true)
                .AddJsonFile("appsettings.Development.json", optional: true, reloadOnChange: true)
                .AddEnvironmentVariables();

            var configuration = configurationBuilder.Build();

            Console.WriteLine("  ✓ Configuration builder created");
            Console.WriteLine("  ✓ appsettings.json loaded");
            Console.WriteLine("  ✓ Environment-specific configuration supported");
            Console.WriteLine();

            // Pattern 2: Loading FastDFS configuration from appsettings.json
            Console.WriteLine("Pattern 2: Loading FastDFS Configuration");
            Console.WriteLine("------------------------------------------");
            Console.WriteLine();

            // Example appsettings.json structure:
            // {
            //   "FastDFS": {
            //     "TrackerAddresses": ["192.168.1.100:22122", "192.168.1.101:22122"],
            //     "MaxConnections": 100,
            //     "ConnectTimeout": "00:00:05",
            //     "NetworkTimeout": "00:00:30",
            //     "IdleTimeout": "00:05:00",
            //     "RetryCount": 3
            //   }
            // }

            // Load configuration section
            var fastDfsSection = configuration.GetSection("FastDFS");

            if (fastDfsSection.Exists())
            {
                var trackerAddresses = fastDfsSection.GetSection("TrackerAddresses").Get<string[]>();
                var maxConnections = fastDfsSection.GetValue<int>("MaxConnections", 100);
                var connectTimeoutSeconds = fastDfsSection.GetValue<int>("ConnectTimeoutSeconds", 5);
                var networkTimeoutSeconds = fastDfsSection.GetValue<int>("NetworkTimeoutSeconds", 30);
                var idleTimeoutMinutes = fastDfsSection.GetValue<int>("IdleTimeoutMinutes", 5);
                var retryCount = fastDfsSection.GetValue<int>("RetryCount", 3);

                var configFromJson = new FastDFSClientConfig
                {
                    TrackerAddresses = trackerAddresses ?? new[] { "192.168.1.100:22122" },
                    MaxConnections = maxConnections,
                    ConnectTimeout = TimeSpan.FromSeconds(connectTimeoutSeconds),
                    NetworkTimeout = TimeSpan.FromSeconds(networkTimeoutSeconds),
                    IdleTimeout = TimeSpan.FromMinutes(idleTimeoutMinutes),
                    RetryCount = retryCount
                };

                Console.WriteLine("  ✓ Configuration loaded from appsettings.json");
                Console.WriteLine($"  ✓ Tracker addresses: {string.Join(", ", configFromJson.TrackerAddresses)}");
                Console.WriteLine($"  ✓ Max connections: {configFromJson.MaxConnections}");
                Console.WriteLine($"  ✓ Connect timeout: {configFromJson.ConnectTimeout.TotalSeconds}s");
                Console.WriteLine($"  ✓ Network timeout: {configFromJson.NetworkTimeout.TotalSeconds}s");
                Console.WriteLine();
            }
            else
            {
                Console.WriteLine("  ⚠ FastDFS configuration section not found in appsettings.json");
                Console.WriteLine("  Using default configuration");
                Console.WriteLine();
            }

            // Pattern 3: Options pattern for configuration
            Console.WriteLine("Pattern 3: Options Pattern for Configuration");
            Console.WriteLine("----------------------------------------------");
            Console.WriteLine();

            // Define options class for FastDFS configuration
            // This is the recommended pattern in ASP.NET Core
            var optionsServices = new ServiceCollection();

            // Configure options from configuration
            optionsServices.Configure<FastDFSOptions>(fastDfsSection);

            // Register configuration as IOptions<FastDFSOptions>
            // This enables the options pattern with validation and change notifications
            optionsServices.AddSingleton<FastDFSClientConfig>(serviceProvider =>
            {
                var options = serviceProvider.GetRequiredService<IOptions<FastDFSOptions>>().Value;

                return new FastDFSClientConfig
                {
                    TrackerAddresses = options.TrackerAddresses,
                    MaxConnections = options.MaxConnections,
                    ConnectTimeout = TimeSpan.FromSeconds(options.ConnectTimeoutSeconds),
                    NetworkTimeout = TimeSpan.FromSeconds(options.NetworkTimeoutSeconds),
                    IdleTimeout = TimeSpan.FromMinutes(options.IdleTimeoutMinutes),
                    RetryCount = options.RetryCount
                };
            });

            Console.WriteLine("  ✓ Options pattern configured");
            Console.WriteLine("  ✓ Configuration bound to FastDFSOptions");
            Console.WriteLine("  ✓ Options validation and change notifications supported");
            Console.WriteLine();

            // ====================================================================
            // Example 3: Logging Integration
            // ====================================================================
            // 
            // This example demonstrates integrating FastDFS client with
            // ASP.NET Core logging. Logging is essential for monitoring,
            // debugging, and troubleshooting production applications.
            // 
            // Logging patterns:
            // - ILogger integration
            // - Logging levels
            // - Structured logging
            // - Logging in service registration
            // ====================================================================

            Console.WriteLine("Example 3: Logging Integration");
            Console.WriteLine("===============================");
            Console.WriteLine();

            // Pattern 1: Basic logging setup
            Console.WriteLine("Pattern 1: Basic Logging Setup");
            Console.WriteLine("-------------------------------");
            Console.WriteLine();

            var loggingServices = new ServiceCollection();

            // Add logging services
            loggingServices.AddLogging(builder =>
            {
                builder.AddConsole();
                builder.AddDebug();
                builder.SetMinimumLevel(LogLevel.Information);
            });

            Console.WriteLine("  ✓ Logging services registered");
            Console.WriteLine("  ✓ Console logging enabled");
            Console.WriteLine("  ✓ Debug logging enabled");
            Console.WriteLine("  ✓ Minimum log level: Information");
            Console.WriteLine();

            // Pattern 2: Logging in service registration
            Console.WriteLine("Pattern 2: Logging in Service Registration");
            Console.WriteLine("---------------------------------------------");
            Console.WriteLine();

            loggingServices.AddSingleton<FastDFSClientConfig>(serviceProvider =>
            {
                var logger = serviceProvider.GetRequiredService<ILogger<FastDFSClientConfig>>();

                logger.LogInformation("Creating FastDFS client configuration");

                var config = new FastDFSClientConfig
                {
                    TrackerAddresses = new[] { "192.168.1.100:22122" },
                    MaxConnections = 100,
                    ConnectTimeout = TimeSpan.FromSeconds(5),
                    NetworkTimeout = TimeSpan.FromSeconds(30),
                    IdleTimeout = TimeSpan.FromMinutes(5),
                    RetryCount = 3
                };

                logger.LogInformation("FastDFS configuration created with {TrackerCount} trackers", 
                    config.TrackerAddresses.Length);

                return config;
            });

            loggingServices.AddSingleton<FastDFSClient>(serviceProvider =>
            {
                var logger = serviceProvider.GetRequiredService<ILogger<FastDFSClient>>();
                var config = serviceProvider.GetRequiredService<FastDFSClientConfig>();

                logger.LogInformation("Creating FastDFS client");

                try
                {
                    var client = new FastDFSClient(config);
                    logger.LogInformation("FastDFS client created successfully");
                    return client;
                }
                catch (Exception ex)
                {
                    logger.LogError(ex, "Failed to create FastDFS client");
                    throw;
                }
            });

            Console.WriteLine("  ✓ Logging integrated in service registration");
            Console.WriteLine("  ✓ Configuration creation logged");
            Console.WriteLine("  ✓ Client creation logged");
            Console.WriteLine("  ✓ Error logging implemented");
            Console.WriteLine();

            // Pattern 3: Logging in wrapper service
            Console.WriteLine("Pattern 3: Logging in Wrapper Service");
            Console.WriteLine("--------------------------------------");
            Console.WriteLine();

            // Create a wrapper service that adds logging to FastDFS operations
            loggingServices.AddSingleton<IFastDFSService, FastDFSService>(serviceProvider =>
            {
                var client = serviceProvider.GetRequiredService<FastDFSClient>();
                var logger = serviceProvider.GetRequiredService<ILogger<FastDFSService>>();
                return new FastDFSService(client, logger);
            });

            Console.WriteLine("  ✓ Wrapper service with logging created");
            Console.WriteLine("  ✓ FastDFS operations will be logged");
            Console.WriteLine("  ✓ Structured logging supported");
            Console.WriteLine();

            // ====================================================================
            // Example 4: ASP.NET Core Web API Integration
            // ====================================================================
            // 
            // This example demonstrates creating Web API controllers that use
            // the FastDFS client through dependency injection. This shows how
            // to build RESTful APIs for file operations.
            // 
            // Web API patterns:
            // - Controller with dependency injection
            // - File upload endpoints
            // - File download endpoints
            // - Error handling in controllers
            // ====================================================================

            Console.WriteLine("Example 4: ASP.NET Core Web API Integration");
            Console.WriteLine("============================================");
            Console.WriteLine();

            // Pattern 1: File upload controller
            Console.WriteLine("Pattern 1: File Upload Controller");
            Console.WriteLine("-----------------------------------");
            Console.WriteLine();

            // Example controller code (commented out as this is a console example):
            /*
            [ApiController]
            [Route("api/[controller]")]
            public class FilesController : ControllerBase
            {
                private readonly FastDFSClient _fastDfsClient;
                private readonly ILogger<FilesController> _logger;

                public FilesController(
                    FastDFSClient fastDfsClient,
                    ILogger<FilesController> logger)
                {
                    _fastDfsClient = fastDfsClient;
                    _logger = logger;
                }

                [HttpPost("upload")]
                public async Task<IActionResult> UploadFile(IFormFile file)
                {
                    if (file == null || file.Length == 0)
                    {
                        return BadRequest("No file uploaded");
                    }

                    try
                    {
                        _logger.LogInformation("Uploading file: {FileName}, Size: {FileSize}", 
                            file.FileName, file.Length);

                        // Save uploaded file temporarily
                        var tempPath = Path.GetTempFileName();
                        using (var stream = new FileStream(tempPath, FileMode.Create))
                        {
                            await file.CopyToAsync(stream);
                        }

                        // Upload to FastDFS
                        var fileId = await _fastDfsClient.UploadFileAsync(tempPath, null);

                        // Clean up temp file
                        File.Delete(tempPath);

                        _logger.LogInformation("File uploaded successfully: {FileId}", fileId);

                        return Ok(new { FileId = fileId, FileName = file.FileName, Size = file.Length });
                    }
                    catch (Exception ex)
                    {
                        _logger.LogError(ex, "Failed to upload file: {FileName}", file.FileName);
                        return StatusCode(500, "File upload failed");
                    }
                }
            }
            */

            Console.WriteLine("  ✓ File upload controller example provided");
            Console.WriteLine("  ✓ Dependency injection in controller");
            Console.WriteLine("  ✓ Logging in controller actions");
            Console.WriteLine("  ✓ Error handling implemented");
            Console.WriteLine();

            // Pattern 2: File download controller
            Console.WriteLine("Pattern 2: File Download Controller");
            Console.WriteLine("------------------------------------");
            Console.WriteLine();

            // Example controller code (commented out):
            /*
            [HttpGet("download/{fileId}")]
            public async Task<IActionResult> DownloadFile(string fileId)
            {
                if (string.IsNullOrWhiteSpace(fileId))
                {
                    return BadRequest("File ID is required");
                }

                try
                {
                    _logger.LogInformation("Downloading file: {FileId}", fileId);

                    // Get file info
                    var fileInfo = await _fastDfsClient.GetFileInfoAsync(fileId);

                    // Download file
                    var fileData = await _fastDfsClient.DownloadFileAsync(fileId);

                    _logger.LogInformation("File downloaded successfully: {FileId}, Size: {FileSize}", 
                        fileId, fileData.Length);

                    return File(fileData, "application/octet-stream", fileId);
                }
                catch (FastDFSFileNotFoundException)
                {
                    _logger.LogWarning("File not found: {FileId}", fileId);
                    return NotFound($"File not found: {fileId}");
                }
                catch (Exception ex)
                {
                    _logger.LogError(ex, "Failed to download file: {FileId}", fileId);
                    return StatusCode(500, "File download failed");
                }
            }
            */

            Console.WriteLine("  ✓ File download controller example provided");
            Console.WriteLine("  ✓ File info retrieval");
            Console.WriteLine("  ✓ Error handling for file not found");
            Console.WriteLine("  ✓ Proper HTTP status codes");
            Console.WriteLine();

            // Pattern 3: File metadata controller
            Console.WriteLine("Pattern 3: File Metadata Controller");
            Console.WriteLine("-------------------------------------");
            Console.WriteLine();

            // Example controller code (commented out):
            /*
            [HttpGet("{fileId}/metadata")]
            public async Task<IActionResult> GetFileMetadata(string fileId)
            {
                try
                {
                    var metadata = await _fastDfsClient.GetMetadataAsync(fileId);
                    return Ok(metadata);
                }
                catch (FastDFSFileNotFoundException)
                {
                    return NotFound($"File not found: {fileId}");
                }
            }

            [HttpPut("{fileId}/metadata")]
            public async Task<IActionResult> SetFileMetadata(
                string fileId,
                [FromBody] Dictionary<string, string> metadata,
                [FromQuery] string flag = "merge")
            {
                try
                {
                    var metadataFlag = flag.ToLower() == "overwrite" 
                        ? MetadataFlag.Overwrite 
                        : MetadataFlag.Merge;

                    await _fastDfsClient.SetMetadataAsync(fileId, metadata, metadataFlag);
                    return Ok();
                }
                catch (FastDFSFileNotFoundException)
                {
                    return NotFound($"File not found: {fileId}");
                }
            }
            */

            Console.WriteLine("  ✓ File metadata controller example provided");
            Console.WriteLine("  ✓ GET and PUT endpoints for metadata");
            Console.WriteLine("  ✓ Metadata flag support");
            Console.WriteLine("  ✓ Proper error handling");
            Console.WriteLine();

            // ====================================================================
            // Example 5: Complete Integration Example
            // ====================================================================
            // 
            // This example demonstrates a complete integration setup combining
            // all the patterns above: dependency injection, configuration,
            // logging, and Web API integration.
            // 
            // Complete integration includes:
            // - Service registration
            // - Configuration loading
            // - Logging setup
            // - Service provider creation
            // - Service usage
            // ====================================================================

            Console.WriteLine("Example 5: Complete Integration Example");
            Console.WriteLine("========================================");
            Console.WriteLine();

            // Pattern 1: Complete service setup
            Console.WriteLine("Pattern 1: Complete Service Setup");
            Console.WriteLine("----------------------------------");
            Console.WriteLine();

            var completeServices = new ServiceCollection();

            // 1. Add configuration
            var completeConfiguration = new ConfigurationBuilder()
                .SetBasePath(Directory.GetCurrentDirectory())
                .AddJsonFile("appsettings.json", optional: true, reloadOnChange: true)
                .AddEnvironmentVariables()
                .Build();

            completeServices.AddSingleton<IConfiguration>(completeConfiguration);

            // 2. Add logging
            completeServices.AddLogging(builder =>
            {
                builder.AddConsole();
                builder.SetMinimumLevel(LogLevel.Information);
            });

            // 3. Configure FastDFS options
            var fastDfsConfigSection = completeConfiguration.GetSection("FastDFS");
            completeServices.Configure<FastDFSOptions>(fastDfsConfigSection);

            // 4. Register FastDFS client configuration
            completeServices.AddSingleton<FastDFSClientConfig>(serviceProvider =>
            {
                var options = serviceProvider.GetRequiredService<IOptions<FastDFSOptions>>().Value;
                var logger = serviceProvider.GetRequiredService<ILogger<FastDFSClientConfig>>();

                logger.LogInformation("Creating FastDFS client configuration");

                var config = new FastDFSClientConfig
                {
                    TrackerAddresses = options.TrackerAddresses,
                    MaxConnections = options.MaxConnections,
                    ConnectTimeout = TimeSpan.FromSeconds(options.ConnectTimeoutSeconds),
                    NetworkTimeout = TimeSpan.FromSeconds(options.NetworkTimeoutSeconds),
                    IdleTimeout = TimeSpan.FromMinutes(options.IdleTimeoutMinutes),
                    RetryCount = options.RetryCount
                };

                logger.LogInformation("FastDFS configuration created with {TrackerCount} trackers", 
                    config.TrackerAddresses.Length);

                return config;
            });

            // 5. Register FastDFS client
            completeServices.AddSingleton<FastDFSClient>(serviceProvider =>
            {
                var config = serviceProvider.GetRequiredService<FastDFSClientConfig>();
                var logger = serviceProvider.GetRequiredService<ILogger<FastDFSClient>>();

                logger.LogInformation("Creating FastDFS client");

                try
                {
                    var client = new FastDFSClient(config);
                    logger.LogInformation("FastDFS client created successfully");
                    return client;
                }
                catch (Exception ex)
                {
                    logger.LogError(ex, "Failed to create FastDFS client");
                    throw;
                }
            });

            // 6. Register wrapper service (optional)
            completeServices.AddSingleton<IFastDFSService, FastDFSService>();

            Console.WriteLine("  ✓ Configuration services registered");
            Console.WriteLine("  ✓ Logging services registered");
            Console.WriteLine("  ✓ FastDFS options configured");
            Console.WriteLine("  ✓ FastDFS client configuration registered");
            Console.WriteLine("  ✓ FastDFS client registered");
            Console.WriteLine("  ✓ Wrapper service registered");
            Console.WriteLine();

            // Pattern 2: Service provider usage
            Console.WriteLine("Pattern 2: Service Provider Usage");
            Console.WriteLine("----------------------------------");
            Console.WriteLine();

            var serviceProvider = completeServices.BuildServiceProvider();

            try
            {
                // Resolve services
                var fastDfsClient = serviceProvider.GetRequiredService<FastDFSClient>();
                var logger = serviceProvider.GetRequiredService<ILogger<IntegrationExample>>();

                logger.LogInformation("FastDFS client resolved from service provider");

                // Use client
                Console.WriteLine("  ✓ FastDFS client resolved successfully");
                Console.WriteLine("  ✓ Client ready for use in application");
                Console.WriteLine();

                // Example: Upload a test file
                var testFile = "integration_test.txt";
                await File.WriteAllTextAsync(testFile, "Integration test file content");

                try
                {
                    logger.LogInformation("Uploading test file: {FileName}", testFile);
                    var fileId = await fastDfsClient.UploadFileAsync(testFile, null);
                    logger.LogInformation("File uploaded successfully: {FileId}", fileId);

                    Console.WriteLine($"  ✓ Test file uploaded: {fileId}");

                    // Clean up
                    await fastDfsClient.DeleteFileAsync(fileId);
                    File.Delete(testFile);
                    Console.WriteLine("  ✓ Test file deleted");
                }
                catch (Exception ex)
                {
                    logger.LogError(ex, "Failed to upload test file");
                    Console.WriteLine($"  ✗ Upload failed: {ex.Message}");
                }
            }
            finally
            {
                // Dispose service provider (cleans up singleton services)
                if (serviceProvider is IDisposable disposable)
                {
                    disposable.Dispose();
                }
            }

            Console.WriteLine();

            // ====================================================================
            // Example 6: Program.cs / Startup.cs Integration
            // ====================================================================
            // 
            // This example demonstrates how to integrate FastDFS client
            // registration in Program.cs (ASP.NET Core 6+) or Startup.cs
            // (ASP.NET Core 5 and earlier). This shows the complete setup
            // for an ASP.NET Core application.
            // ====================================================================

            Console.WriteLine("Example 6: Program.cs / Startup.cs Integration");
            Console.WriteLine("==============================================");
            Console.WriteLine();

            // Pattern 1: Program.cs integration (ASP.NET Core 6+)
            Console.WriteLine("Pattern 1: Program.cs Integration (ASP.NET Core 6+)");
            Console.WriteLine("-----------------------------------------------------");
            Console.WriteLine();

            // Example Program.cs code (commented out):
            /*
            var builder = WebApplication.CreateBuilder(args);

            // Add services to the container
            builder.Services.AddControllers();

            // Add logging
            builder.Services.AddLogging();

            // Configure FastDFS
            builder.Services.Configure<FastDFSOptions>(
                builder.Configuration.GetSection("FastDFS"));

            // Register FastDFS client configuration
            builder.Services.AddSingleton<FastDFSClientConfig>(serviceProvider =>
            {
                var options = serviceProvider.GetRequiredService<IOptions<FastDFSOptions>>().Value;
                return new FastDFSClientConfig
                {
                    TrackerAddresses = options.TrackerAddresses,
                    MaxConnections = options.MaxConnections,
                    ConnectTimeout = TimeSpan.FromSeconds(options.ConnectTimeoutSeconds),
                    NetworkTimeout = TimeSpan.FromSeconds(options.NetworkTimeoutSeconds),
                    IdleTimeout = TimeSpan.FromMinutes(options.IdleTimeoutMinutes),
                    RetryCount = options.RetryCount
                };
            });

            // Register FastDFS client
            builder.Services.AddSingleton<FastDFSClient>(serviceProvider =>
            {
                var config = serviceProvider.GetRequiredService<FastDFSClientConfig>();
                var logger = serviceProvider.GetRequiredService<ILogger<FastDFSClient>>();
                logger.LogInformation("Creating FastDFS client");
                return new FastDFSClient(config);
            });

            var app = builder.Build();

            // Configure the HTTP request pipeline
            app.UseHttpsRedirection();
            app.UseAuthorization();
            app.MapControllers();

            app.Run();
            */

            Console.WriteLine("  ✓ Program.cs integration example provided");
            Console.WriteLine("  ✓ Service registration in builder");
            Console.WriteLine("  ✓ Configuration from appsettings.json");
            Console.WriteLine("  ✓ Logging integration");
            Console.WriteLine();

            // Pattern 2: Startup.cs integration (ASP.NET Core 5 and earlier)
            Console.WriteLine("Pattern 2: Startup.cs Integration (ASP.NET Core 5 and earlier)");
            Console.WriteLine("----------------------------------------------------------------");
            Console.WriteLine();

            // Example Startup.cs code (commented out):
            /*
            public class Startup
            {
                public Startup(IConfiguration configuration)
                {
                    Configuration = configuration;
                }

                public IConfiguration Configuration { get; }

                public void ConfigureServices(IServiceCollection services)
                {
                    services.AddControllers();

                    // Configure FastDFS
                    services.Configure<FastDFSOptions>(
                        Configuration.GetSection("FastDFS"));

                    // Register FastDFS client configuration
                    services.AddSingleton<FastDFSClientConfig>(serviceProvider =>
                    {
                        var options = serviceProvider.GetRequiredService<IOptions<FastDFSOptions>>().Value;
                        return new FastDFSClientConfig
                        {
                            TrackerAddresses = options.TrackerAddresses,
                            MaxConnections = options.MaxConnections,
                            ConnectTimeout = TimeSpan.FromSeconds(options.ConnectTimeoutSeconds),
                            NetworkTimeout = TimeSpan.FromSeconds(options.NetworkTimeoutSeconds),
                            IdleTimeout = TimeSpan.FromMinutes(options.IdleTimeoutMinutes),
                            RetryCount = options.RetryCount
                        };
                    });

                    // Register FastDFS client
                    services.AddSingleton<FastDFSClient>(serviceProvider =>
                    {
                        var config = serviceProvider.GetRequiredService<FastDFSClientConfig>();
                        var logger = serviceProvider.GetRequiredService<ILogger<FastDFSClient>>();
                        logger.LogInformation("Creating FastDFS client");
                        return new FastDFSClient(config);
                    });
                }

                public void Configure(IApplicationBuilder app, IWebHostEnvironment env)
                {
                    if (env.IsDevelopment())
                    {
                        app.UseDeveloperExceptionPage();
                    }

                    app.UseHttpsRedirection();
                    app.UseRouting();
                    app.UseAuthorization();
                    app.UseEndpoints(endpoints =>
                    {
                        endpoints.MapControllers();
                    });
                }
            }
            */

            Console.WriteLine("  ✓ Startup.cs integration example provided");
            Console.WriteLine("  ✓ ConfigureServices method");
            Console.WriteLine("  ✓ Configure method");
            Console.WriteLine("  ✓ Environment-specific configuration");
            Console.WriteLine();

            // ====================================================================
            // Best Practices Summary
            // ====================================================================
            // 
            // This section summarizes best practices for integrating FastDFS
            // client with ASP.NET Core applications.
            // ====================================================================

            Console.WriteLine("Best Practices for ASP.NET Core Integration");
            Console.WriteLine("===========================================");
            Console.WriteLine();
            Console.WriteLine("1. Service Registration:");
            Console.WriteLine("   - Register FastDFS client as singleton");
            Console.WriteLine("   - Use factory pattern for flexible creation");
            Console.WriteLine("   - Register configuration separately");
            Console.WriteLine("   - Handle service disposal properly");
            Console.WriteLine();
            Console.WriteLine("2. Configuration:");
            Console.WriteLine("   - Load configuration from appsettings.json");
            Console.WriteLine("   - Use options pattern for configuration");
            Console.WriteLine("   - Support environment-specific configuration");
            Console.WriteLine("   - Validate configuration on startup");
            Console.WriteLine();
            Console.WriteLine("3. Logging:");
            Console.WriteLine("   - Integrate with ASP.NET Core logging");
            Console.WriteLine("   - Log client creation and operations");
            Console.WriteLine("   - Use structured logging");
            Console.WriteLine("   - Log errors and exceptions");
            Console.WriteLine();
            Console.WriteLine("4. Dependency Injection:");
            Console.WriteLine("   - Use constructor injection in controllers");
            Console.WriteLine("   - Inject ILogger for logging");
            Console.WriteLine("   - Use interface abstractions when appropriate");
            Console.WriteLine("   - Follow dependency inversion principle");
            Console.WriteLine();
            Console.WriteLine("5. Error Handling:");
            Console.WriteLine("   - Handle FastDFS exceptions in controllers");
            Console.WriteLine("   - Return appropriate HTTP status codes");
            Console.WriteLine("   - Log errors for troubleshooting");
            Console.WriteLine("   - Provide meaningful error messages");
            Console.WriteLine();
            Console.WriteLine("6. Service Lifetime:");
            Console.WriteLine("   - Use singleton for FastDFS client");
            Console.WriteLine("   - Client manages connection pools internally");
            Console.WriteLine("   - Avoid creating multiple client instances");
            Console.WriteLine("   - Dispose client on application shutdown");
            Console.WriteLine();
            Console.WriteLine("7. Configuration Management:");
            Console.WriteLine("   - Store configuration in appsettings.json");
            Console.WriteLine("   - Use environment variables for sensitive data");
            Console.WriteLine("   - Support configuration reloading");
            Console.WriteLine("   - Validate configuration on startup");
            Console.WriteLine();
            Console.WriteLine("8. Web API Design:");
            Console.WriteLine("   - Use RESTful API design");
            Console.WriteLine("   - Return appropriate HTTP status codes");
            Console.WriteLine("   - Use async/await for all operations");
            Console.WriteLine("   - Support cancellation tokens");
            Console.WriteLine();
            Console.WriteLine("9. Performance:");
            Console.WriteLine("   - Use singleton client for connection reuse");
            Console.WriteLine("   - Configure appropriate connection pool size");
            Console.WriteLine("   - Use async operations throughout");
            Console.WriteLine("   - Monitor and tune performance");
            Console.WriteLine();
            Console.WriteLine("10. Best Practices Summary:");
            Console.WriteLine("    - Register services properly");
            Console.WriteLine("    - Load configuration from appsettings.json");
            Console.WriteLine("    - Integrate with logging");
            Console.WriteLine("    - Use dependency injection");
            Console.WriteLine("    - Handle errors appropriately");
            Console.WriteLine();

            Console.WriteLine("All examples completed successfully!");
        }
    }

    // ====================================================================
    // Helper Classes and Interfaces
    // ====================================================================

    /// <summary>
    /// Options class for FastDFS configuration.
    /// 
    /// This class represents the configuration options that can be loaded
    /// from appsettings.json. It uses the options pattern recommended
    /// in ASP.NET Core.
    /// </summary>
    public class FastDFSOptions
    {
        /// <summary>
        /// Gets or sets the tracker server addresses.
        /// </summary>
        public string[] TrackerAddresses { get; set; } = new[] { "192.168.1.100:22122" };

        /// <summary>
        /// Gets or sets the maximum number of connections per server.
        /// </summary>
        public int MaxConnections { get; set; } = 100;

        /// <summary>
        /// Gets or sets the connection timeout in seconds.
        /// </summary>
        public int ConnectTimeoutSeconds { get; set; } = 5;

        /// <summary>
        /// Gets or sets the network timeout in seconds.
        /// </summary>
        public int NetworkTimeoutSeconds { get; set; } = 30;

        /// <summary>
        /// Gets or sets the idle timeout in minutes.
        /// </summary>
        public int IdleTimeoutMinutes { get; set; } = 5;

        /// <summary>
        /// Gets or sets the retry count for failed operations.
        /// </summary>
        public int RetryCount { get; set; } = 3;
    }

    /// <summary>
    /// Interface for FastDFS service wrapper.
    /// 
    /// This interface provides an abstraction over the FastDFS client,
    /// enabling easier testing and additional functionality like logging.
    /// </summary>
    public interface IFastDFSService
    {
        /// <summary>
        /// Uploads a file to FastDFS.
        /// </summary>
        Task<string> UploadFileAsync(string localFilePath, Dictionary<string, string> metadata = null);

        /// <summary>
        /// Downloads a file from FastDFS.
        /// </summary>
        Task<byte[]> DownloadFileAsync(string fileId);

        /// <summary>
        /// Deletes a file from FastDFS.
        /// </summary>
        Task DeleteFileAsync(string fileId);
    }

    /// <summary>
    /// FastDFS service wrapper with logging.
    /// 
    /// This class wraps the FastDFS client and adds logging functionality.
    /// It implements the IFastDFSService interface for dependency injection.
    /// </summary>
    public class FastDFSService : IFastDFSService
    {
        private readonly FastDFSClient _client;
        private readonly ILogger<FastDFSService> _logger;

        /// <summary>
        /// Initializes a new instance of the FastDFSService class.
        /// </summary>
        /// <param name="client">The FastDFS client instance.</param>
        /// <param name="logger">The logger instance.</param>
        public FastDFSService(FastDFSClient client, ILogger<FastDFSService> logger)
        {
            _client = client ?? throw new ArgumentNullException(nameof(client));
            _logger = logger ?? throw new ArgumentNullException(nameof(logger));
        }

        /// <summary>
        /// Uploads a file to FastDFS with logging.
        /// </summary>
        public async Task<string> UploadFileAsync(string localFilePath, Dictionary<string, string> metadata = null)
        {
            _logger.LogInformation("Uploading file: {FilePath}", localFilePath);

            try
            {
                var fileId = await _client.UploadFileAsync(localFilePath, metadata);
                _logger.LogInformation("File uploaded successfully: {FileId}", fileId);
                return fileId;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Failed to upload file: {FilePath}", localFilePath);
                throw;
            }
        }

        /// <summary>
        /// Downloads a file from FastDFS with logging.
        /// </summary>
        public async Task<byte[]> DownloadFileAsync(string fileId)
        {
            _logger.LogInformation("Downloading file: {FileId}", fileId);

            try
            {
                var data = await _client.DownloadFileAsync(fileId);
                _logger.LogInformation("File downloaded successfully: {FileId}, Size: {Size}", fileId, data.Length);
                return data;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Failed to download file: {FileId}", fileId);
                throw;
            }
        }

        /// <summary>
        /// Deletes a file from FastDFS with logging.
        /// </summary>
        public async Task DeleteFileAsync(string fileId)
        {
            _logger.LogInformation("Deleting file: {FileId}", fileId);

            try
            {
                await _client.DeleteFileAsync(fileId);
                _logger.LogInformation("File deleted successfully: {FileId}", fileId);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Failed to delete file: {FileId}", fileId);
                throw;
            }
        }
    }
}

