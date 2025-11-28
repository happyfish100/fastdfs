/**
 * FastDFS Error Handling Example
 * 
 * This example demonstrates comprehensive error handling techniques when working
 * with FastDFS. It covers common error scenarios, proper error checking, recovery
 * strategies, and best practices for robust application development.
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./08_error_handling <config_file> <test_scenario>
 * 
 * EXAMPLE:
 *   ./08_error_handling client.conf all
 *   ./08_error_handling client.conf connection
 *   ./08_error_handling client.conf upload
 * 
 * TEST SCENARIOS:
 *   all          - Run all error handling tests
 *   connection   - Test connection error handling
 *   upload       - Test upload error handling
 *   download     - Test download error handling
 *   metadata     - Test metadata error handling
 *   timeout      - Test timeout error handling
 * 
 * EXPECTED OUTPUT:
 *   Testing various error scenarios with proper handling
 *   Each test shows: error detection, diagnosis, and recovery
 * 
 * COMMON ERROR CATEGORIES:
 *   1. Connection Errors (ECONNREFUSED, ETIMEDOUT, ENETUNREACH)
 *   2. File Errors (ENOENT, EACCES, EINVAL)
 *   3. Protocol Errors (Invalid response, version mismatch)
 *   4. Resource Errors (ENOMEM, ENOSPC, EMFILE)
 *   5. Configuration Errors (Invalid settings, missing parameters)
 * 
 * ERROR HANDLING BEST PRACTICES:
 *   - Always check return values from FastDFS functions
 *   - Use STRERROR() macro for human-readable error messages
 *   - Implement proper cleanup in error paths
 *   - Log errors with sufficient context for debugging
 *   - Implement retry logic for transient errors
 *   - Validate inputs before making FastDFS calls
 *   - Handle partial failures in batch operations
 *   - Close connections properly even on errors
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fdfs_client.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "tracker_client.h"
#include "storage_client.h"
#include "fastcommon/logger.h"

/**
 * Error category enumeration for classification
 */
typedef enum {
    ERROR_CATEGORY_CONNECTION = 1,
    ERROR_CATEGORY_FILE = 2,
    ERROR_CATEGORY_PROTOCOL = 3,
    ERROR_CATEGORY_RESOURCE = 4,
    ERROR_CATEGORY_CONFIG = 5,
    ERROR_CATEGORY_UNKNOWN = 99
} ErrorCategory;

/**
 * Error recovery action enumeration
 */
typedef enum {
    RECOVERY_RETRY = 1,        /* Retry the operation */
    RECOVERY_SKIP = 2,         /* Skip and continue */
    RECOVERY_ABORT = 3,        /* Abort operation */
    RECOVERY_FALLBACK = 4      /* Use fallback mechanism */
} RecoveryAction;

/**
 * Print usage information
 */
void print_usage(const char *program_name)
{
    printf("FastDFS Error Handling Example\n\n");
    printf("Usage: %s <config_file> <test_scenario>\n\n", program_name);
    printf("Arguments:\n");
    printf("  config_file     Path to FastDFS client configuration file\n");
    printf("  test_scenario   Error scenario to test\n\n");
    printf("Available scenarios:\n");
    printf("  all          - Run all error handling tests\n");
    printf("  connection   - Test connection error handling\n");
    printf("  upload       - Test upload error handling\n");
    printf("  download     - Test download error handling\n");
    printf("  metadata     - Test metadata error handling\n");
    printf("  timeout      - Test timeout error handling\n\n");
    printf("Example:\n");
    printf("  %s client.conf all\n\n", program_name);
}

/**
 * Categorize error code into error categories
 * 
 * @param error_code The errno or FastDFS error code
 * @return ErrorCategory enum value
 */
ErrorCategory categorize_error(int error_code)
{
    /* Connection-related errors */
    if (error_code == ECONNREFUSED || error_code == ETIMEDOUT ||
        error_code == ENETUNREACH || error_code == EHOSTUNREACH ||
        error_code == ECONNRESET || error_code == EPIPE) {
        return ERROR_CATEGORY_CONNECTION;
    }
    
    /* File-related errors */
    if (error_code == ENOENT || error_code == EACCES ||
        error_code == EISDIR || error_code == ENOTDIR ||
        error_code == EROFS) {
        return ERROR_CATEGORY_FILE;
    }
    
    /* Resource-related errors */
    if (error_code == ENOMEM || error_code == ENOSPC ||
        error_code == EMFILE || error_code == ENFILE) {
        return ERROR_CATEGORY_RESOURCE;
    }
    
    /* Configuration/validation errors */
    if (error_code == EINVAL || error_code == ERANGE) {
        return ERROR_CATEGORY_CONFIG;
    }
    
    /* Protocol errors (FastDFS specific) */
    if (error_code >= 200) {  /* FastDFS error codes typically >= 200 */
        return ERROR_CATEGORY_PROTOCOL;
    }
    
    return ERROR_CATEGORY_UNKNOWN;
}

/**
 * Get human-readable error category name
 */
const char* get_error_category_name(ErrorCategory category)
{
    switch (category) {
        case ERROR_CATEGORY_CONNECTION: return "Connection Error";
        case ERROR_CATEGORY_FILE: return "File Error";
        case ERROR_CATEGORY_PROTOCOL: return "Protocol Error";
        case ERROR_CATEGORY_RESOURCE: return "Resource Error";
        case ERROR_CATEGORY_CONFIG: return "Configuration Error";
        default: return "Unknown Error";
    }
}

/**
 * Determine if an error is transient and should be retried
 * 
 * @param error_code The error code to check
 * @return 1 if error is transient, 0 otherwise
 */
int is_transient_error(int error_code)
{
    /* Transient errors that may succeed on retry */
    return (error_code == ETIMEDOUT ||
            error_code == EAGAIN ||
            error_code == EWOULDBLOCK ||
            error_code == EINTR ||
            error_code == ECONNRESET);
}

/**
 * Suggest recovery action based on error code
 */
RecoveryAction suggest_recovery_action(int error_code)
{
    ErrorCategory category = categorize_error(error_code);
    
    /* Transient errors should be retried */
    if (is_transient_error(error_code)) {
        return RECOVERY_RETRY;
    }
    
    /* Connection errors might benefit from retry */
    if (category == ERROR_CATEGORY_CONNECTION) {
        return RECOVERY_RETRY;
    }
    
    /* Resource errors should abort */
    if (category == ERROR_CATEGORY_RESOURCE) {
        return RECOVERY_ABORT;
    }
    
    /* File errors might be skippable in batch operations */
    if (category == ERROR_CATEGORY_FILE) {
        return RECOVERY_SKIP;
    }
    
    /* Config errors should abort */
    if (category == ERROR_CATEGORY_CONFIG) {
        return RECOVERY_ABORT;
    }
    
    /* Default: abort on unknown errors */
    return RECOVERY_ABORT;
}

/**
 * Print detailed error information with context
 * 
 * @param operation Description of the operation that failed
 * @param error_code The error code
 * @param context Additional context information
 */
void print_error_details(const char *operation, int error_code, const char *context)
{
    ErrorCategory category = categorize_error(error_code);
    RecoveryAction recovery = suggest_recovery_action(error_code);
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ ERROR DETECTED                                             ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Operation: %-47s ║\n", operation);
    printf("║ Error Code: %-46d ║\n", error_code);
    printf("║ Error Message: %-43s ║\n", STRERROR(error_code));
    printf("║ Category: %-48s ║\n", get_error_category_name(category));
    printf("║ Transient: %-47s ║\n", is_transient_error(error_code) ? "Yes" : "No");
    if (context && strlen(context) > 0) {
        printf("║ Context: %-49s ║\n", context);
    }
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* Print suggested recovery action */
    printf("Suggested Action: ");
    switch (recovery) {
        case RECOVERY_RETRY:
            printf("RETRY (Error may be transient)\n");
            break;
        case RECOVERY_SKIP:
            printf("SKIP (Continue with next operation)\n");
            break;
        case RECOVERY_ABORT:
            printf("ABORT (Fatal error, cannot continue)\n");
            break;
        case RECOVERY_FALLBACK:
            printf("FALLBACK (Use alternative approach)\n");
            break;
    }
    printf("\n");
}

/**
 * Retry wrapper for operations with exponential backoff
 * 
 * @param operation_func Function pointer to operation
 * @param context Context to pass to operation
 * @param max_retries Maximum number of retry attempts
 * @return 0 on success, error code on failure
 */
typedef int (*operation_func_t)(void *context);

int retry_operation(operation_func_t operation_func, void *context, 
                   int max_retries, const char *operation_name)
{
    int result;
    int attempt;
    int wait_ms = 100;  /* Start with 100ms */
    
    for (attempt = 0; attempt <= max_retries; attempt++) {
        if (attempt > 0) {
            printf("Retry attempt %d/%d for '%s' (waiting %dms)...\n", 
                   attempt, max_retries, operation_name, wait_ms);
            usleep(wait_ms * 1000);  /* Convert to microseconds */
            wait_ms *= 2;  /* Exponential backoff */
            if (wait_ms > 5000) wait_ms = 5000;  /* Cap at 5 seconds */
        }
        
        result = operation_func(context);
        
        if (result == 0) {
            if (attempt > 0) {
                printf("✓ Operation succeeded on retry attempt %d\n", attempt);
            }
            return 0;
        }
        
        /* Check if error is retryable */
        if (!is_transient_error(result)) {
            printf("✗ Non-transient error, aborting retries\n");
            return result;
        }
    }
    
    printf("✗ Operation failed after %d retries\n", max_retries);
    return result;
}

/**
 * Test connection error handling
 */
int test_connection_errors(const char *conf_filename)
{
    ConnectionInfo *pTrackerServer;
    int result;
    
    printf("\n=== Test 1: Connection Error Handling ===\n");
    printf("Testing connection to tracker server...\n");
    
    /* Attempt to get tracker connection */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        print_error_details("tracker_get_connection", result, 
                          "Failed to connect to tracker server");
        
        /* Demonstrate error handling */
        printf("Error Handling Steps:\n");
        printf("1. Check if tracker server is running\n");
        printf("2. Verify tracker_server setting in config file\n");
        printf("3. Check network connectivity\n");
        printf("4. Verify firewall rules\n");
        printf("5. Check if port is correct (default: 22122)\n");
        
        return result;
    }
    
    printf("✓ Successfully connected to tracker: %s:%d\n", 
           pTrackerServer->ip_addr, pTrackerServer->port);
    
    /* Test connection validity */
    FDFSGroupStat group_stats[FDFS_MAX_GROUPS];
    int group_count = 0;
    
    result = tracker_list_groups(pTrackerServer, group_stats, 
                                FDFS_MAX_GROUPS, &group_count);
    if (result != 0) {
        print_error_details("tracker_list_groups", result,
                          "Failed to list groups from tracker");
        tracker_close_connection_ex(pTrackerServer, true);
        return result;
    }
    
    printf("✓ Connection is valid, found %d group(s)\n", group_count);
    
    /* Properly close connection */
    tracker_close_connection_ex(pTrackerServer, false);
    
    return 0;
}

/**
 * Test file validation and upload error handling
 */
int test_upload_errors(const char *conf_filename)
{
    ConnectionInfo *pTrackerServer = NULL;
    ConnectionInfo *pStorageServer = NULL;
    ConnectionInfo storageServer;
    int result;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
    const char *test_files[] = {
        "/nonexistent/file.txt",      /* File doesn't exist */
        "/tmp",                        /* Directory, not a file */
        NULL
    };
    int i;
    
    printf("\n=== Test 2: Upload Error Handling ===\n");
    
    /* Get tracker connection */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        print_error_details("tracker_get_connection", result, "Upload test");
        return result;
    }
    
    /* Test uploading various problematic files */
    for (i = 0; test_files[i] != NULL; i++) {
        const char *filepath = test_files[i];
        struct stat stat_buf;
        
        printf("\nTesting upload of: %s\n", filepath);
        
        /* Validate file before attempting upload */
        if (stat(filepath, &stat_buf) != 0) {
            result = errno;
            print_error_details("File validation (stat)", result, filepath);
            printf("Prevention: Always validate file existence before upload\n");
            continue;
        }
        
        if (!S_ISREG(stat_buf.st_mode)) {
            result = EISDIR;
            print_error_details("File validation (type check)", result, filepath);
            printf("Prevention: Check S_ISREG() before upload\n");
            continue;
        }
        
        /* If we get here, file is valid - attempt upload */
        *group_name = '\0';
        result = storage_upload_by_filename1(pTrackerServer, NULL,
                                             filepath, NULL,
                                             NULL, 0,
                                             group_name, remote_filename);
        if (result != 0) {
            print_error_details("storage_upload_by_filename1", result, filepath);
        } else {
            printf("✓ Upload successful: %s/%s\n", group_name, remote_filename);
        }
    }
    
    /* Cleanup */
    tracker_close_connection_ex(pTrackerServer, false);
    
    return 0;
}

/**
 * Test download error handling
 */
int test_download_errors(const char *conf_filename)
{
    ConnectionInfo *pTrackerServer = NULL;
    ConnectionInfo *pStorageServer = NULL;
    ConnectionInfo storageServer;
    int result;
    char *file_buff = NULL;
    int64_t file_size = 0;
    const char *invalid_file_ids[] = {
        "group1/M00/00/00/invalid_file.jpg",  /* Non-existent file */
        "invalid_group/M00/00/00/file.jpg",   /* Invalid group */
        "group1/invalid/path/file.jpg",       /* Invalid path format */
        NULL
    };
    int i;
    
    printf("\n=== Test 3: Download Error Handling ===\n");
    
    /* Get tracker connection */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        print_error_details("tracker_get_connection", result, "Download test");
        return result;
    }
    
    /* Test downloading various invalid files */
    for (i = 0; invalid_file_ids[i] != NULL; i++) {
        const char *file_id = invalid_file_ids[i];
        
        printf("\nTesting download of: %s\n", file_id);
        
        /* Parse file_id to get group and filename */
        char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
        char *remote_filename;
        char file_id_copy[256];
        
        snprintf(file_id_copy, sizeof(file_id_copy), "%s", file_id);
        remote_filename = strchr(file_id_copy, '/');
        
        if (remote_filename == NULL) {
            result = EINVAL;
            print_error_details("File ID parsing", result, file_id);
            printf("Prevention: Validate file_id format before download\n");
            continue;
        }
        
        *remote_filename = '\0';
        remote_filename++;
        snprintf(group_name, sizeof(group_name), "%s", file_id_copy);
        
        /* Attempt download */
        result = storage_download_file_to_buff1(pTrackerServer, NULL,
                                               group_name, remote_filename,
                                               &file_buff, &file_size);
        if (result != 0) {
            print_error_details("storage_download_file_to_buff1", result, file_id);
            
            /* Provide specific guidance based on error */
            if (result == ENOENT) {
                printf("Guidance: File does not exist on storage server\n");
                printf("  - Verify file_id is correct\n");
                printf("  - Check if file was deleted\n");
                printf("  - Ensure file was uploaded successfully\n");
            }
        } else {
            printf("✓ Download successful: %lld bytes\n", (long long)file_size);
            if (file_buff != NULL) {
                free(file_buff);
                file_buff = NULL;
            }
        }
    }
    
    /* Cleanup */
    tracker_close_connection_ex(pTrackerServer, false);
    
    return 0;
}

/**
 * Test metadata operation error handling
 */
int test_metadata_errors(const char *conf_filename)
{
    ConnectionInfo *pTrackerServer = NULL;
    int result;
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    
    printf("\n=== Test 4: Metadata Error Handling ===\n");
    
    /* Get tracker connection */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        print_error_details("tracker_get_connection", result, "Metadata test");
        return result;
    }
    
    /* Test getting metadata from non-existent file */
    const char *invalid_file_id = "group1/M00/00/00/nonexistent.jpg";
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = "group1";
    const char *remote_filename = "M00/00/00/nonexistent.jpg";
    
    printf("Testing metadata retrieval from: %s\n", invalid_file_id);
    
    result = storage_get_metadata1(pTrackerServer, NULL,
                                   group_name, remote_filename,
                                   &meta_list, &meta_count);
    if (result != 0) {
        print_error_details("storage_get_metadata1", result, invalid_file_id);
        printf("Best Practice: Check file existence before metadata operations\n");
    } else {
        printf("✓ Metadata retrieved: %d items\n", meta_count);
        if (meta_list != NULL) {
            free(meta_list);
        }
    }
    
    /* Cleanup */
    tracker_close_connection_ex(pTrackerServer, false);
    
    return 0;
}

/**
 * Test timeout error handling
 */
int test_timeout_errors(const char *conf_filename)
{
    printf("\n=== Test 5: Timeout Error Handling ===\n");
    printf("Timeout errors typically occur when:\n");
    printf("  - Network is slow or congested\n");
    printf("  - Server is overloaded\n");
    printf("  - Large file transfers\n");
    printf("  - Firewall is blocking connections\n\n");
    
    printf("Configuration options for timeout handling:\n");
    printf("  network_timeout       - Socket operation timeout (default: 30s)\n");
    printf("  connect_timeout       - Connection establishment timeout\n");
    printf("  tracker_server_timeout - Tracker server response timeout\n\n");
    
    printf("Timeout error handling strategies:\n");
    printf("  1. Implement retry logic with exponential backoff\n");
    printf("  2. Increase timeout values in client.conf if needed\n");
    printf("  3. Use async operations for large files\n");
    printf("  4. Monitor network latency\n");
    printf("  5. Implement circuit breaker pattern for repeated failures\n");
    
    return 0;
}

/**
 * Demonstrate proper cleanup in error scenarios
 */
void demonstrate_cleanup_patterns(void)
{
    printf("\n=== Error Cleanup Patterns ===\n\n");
    
    printf("Pattern 1: Simple cleanup with goto\n");
    printf("```c\n");
    printf("int result;\n");
    printf("ConnectionInfo *pTracker = NULL;\n");
    printf("char *buffer = NULL;\n\n");
    printf("pTracker = tracker_get_connection();\n");
    printf("if (pTracker == NULL) {\n");
    printf("    result = errno;\n");
    printf("    goto cleanup;\n");
    printf("}\n\n");
    printf("buffer = malloc(size);\n");
    printf("if (buffer == NULL) {\n");
    printf("    result = ENOMEM;\n");
    printf("    goto cleanup;\n");
    printf("}\n\n");
    printf("cleanup:\n");
    printf("    if (buffer) free(buffer);\n");
    printf("    if (pTracker) tracker_close_connection_ex(pTracker, true);\n");
    printf("    return result;\n");
    printf("```\n\n");
    
    printf("Pattern 2: RAII-style cleanup (requires compiler support)\n");
    printf("Use __attribute__((cleanup)) for automatic cleanup\n\n");
    
    printf("Pattern 3: Error-specific cleanup\n");
    printf("```c\n");
    printf("if (result != 0) {\n");
    printf("    /* Force close on error */\n");
    printf("    tracker_close_connection_ex(pTracker, true);\n");
    printf("} else {\n");
    printf("    /* Return to pool on success */\n");
    printf("    tracker_close_connection_ex(pTracker, false);\n");
    printf("}\n");
    printf("```\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[])
{
    char *conf_filename;
    char *test_scenario;
    int result = 0;
    int run_all = 0;
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    test_scenario = argv[2];
    
    run_all = (strcmp(test_scenario, "all") == 0);
    
    printf("=== FastDFS Error Handling Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("Test scenario: %s\n", test_scenario);
    
    /* ========================================
     * STEP 2: Initialize logging and client
     * ======================================== */
    log_init();
    g_log_context.log_level = LOG_WARNING;  /* Reduce noise */
    
    printf("\nInitializing FastDFS client...\n");
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        print_error_details("fdfs_client_init", result, conf_filename);
        
        printf("\nTroubleshooting Steps:\n");
        printf("1. Verify config file exists and is readable\n");
        printf("2. Check config file syntax\n");
        printf("3. Ensure all required parameters are set:\n");
        printf("   - tracker_server\n");
        printf("   - base_path\n");
        printf("   - network_timeout\n");
        printf("4. Check file permissions\n");
        printf("5. Verify paths in config are valid\n");
        
        return result;
    }
    printf("✓ Client initialized successfully\n");
    
    /* ========================================
     * STEP 3: Run selected error tests
     * ======================================== */
    
    if (run_all || strcmp(test_scenario, "connection") == 0) {
        result = test_connection_errors(conf_filename);
        if (result != 0 && !run_all) {
            goto cleanup;
        }
    }
    
    if (run_all || strcmp(test_scenario, "upload") == 0) {
        result = test_upload_errors(conf_filename);
        if (result != 0 && !run_all) {
            goto cleanup;
        }
    }
    
    if (run_all || strcmp(test_scenario, "download") == 0) {
        result = test_download_errors(conf_filename);
        if (result != 0 && !run_all) {
            goto cleanup;
        }
    }
    
    if (run_all || strcmp(test_scenario, "metadata") == 0) {
        result = test_metadata_errors(conf_filename);
        if (result != 0 && !run_all) {
            goto cleanup;
        }
    }
    
    if (run_all || strcmp(test_scenario, "timeout") == 0) {
        result = test_timeout_errors(conf_filename);
    }
    
    /* ========================================
     * STEP 4: Display best practices
     * ======================================== */
    printf("\n=== Error Handling Best Practices Summary ===\n");
    printf("1. ✓ Always check return values\n");
    printf("2. ✓ Use STRERROR() for error messages\n");
    printf("3. ✓ Categorize errors for appropriate handling\n");
    printf("4. ✓ Implement retry logic for transient errors\n");
    printf("5. ✓ Validate inputs before FastDFS operations\n");
    printf("6. ✓ Proper cleanup in all error paths\n");
    printf("7. ✓ Log errors with sufficient context\n");
    printf("8. ✓ Use force=true when closing on errors\n");
    printf("9. ✓ Handle partial failures in batch operations\n");
    printf("10. ✓ Monitor and alert on error patterns\n");
    
    demonstrate_cleanup_patterns();
    
    printf("\n=== Common Error Codes Reference ===\n");
    printf("ECONNREFUSED (%d) - Connection refused by server\n", ECONNREFUSED);
    printf("ETIMEDOUT (%d)    - Operation timed out\n", ETIMEDOUT);
    printf("ENOENT (%d)       - File or directory not found\n", ENOENT);
    printf("EACCES (%d)       - Permission denied\n", EACCES);
    printf("ENOMEM (%d)       - Out of memory\n", ENOMEM);
    printf("EINVAL (%d)       - Invalid argument\n", EINVAL);
    printf("ENOSPC (%d)       - No space left on device\n", ENOSPC);

cleanup:
    /* ========================================
     * STEP 5: Cleanup
     * ======================================== */
    printf("\n=== Cleanup ===\n");
    tracker_close_all_connections();
    printf("✓ All connections closed\n");
    
    fdfs_client_destroy();
    printf("✓ Client destroyed\n");
    
    printf("\n=== Example Complete ===\n");
    if (result == 0) {
        printf("Status: All tests completed successfully\n");
    } else {
        printf("Status: Tests completed with errors (code: %d)\n", result);
    }
    
    return result;
}
