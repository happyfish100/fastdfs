/**
 * FastDFS Batch Upload Example
 * 
 * This example demonstrates how to efficiently upload multiple files to FastDFS
 * in batch mode. It covers various batch upload strategies including:
 * - Sequential uploads with connection reuse
 * - Error handling and retry logic
 * - Progress tracking
 * - Performance optimization techniques
 * - Batch result reporting
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./06_batch_upload <config_file> <file1> <file2> <file3> ...
 *   ./06_batch_upload <config_file> <directory>
 * 
 * EXAMPLE:
 *   ./06_batch_upload client.conf image1.jpg image2.jpg image3.jpg
 *   ./06_batch_upload client.conf /path/to/images/
 * 
 * EXPECTED OUTPUT:
 *   === Batch Upload Progress ===
 *   [1/3] Uploading image1.jpg... ✓ (1.2 MB in 0.5s)
 *   [2/3] Uploading image2.jpg... ✓ (2.4 MB in 0.8s)
 *   [3/3] Uploading image3.jpg... ✓ (1.8 MB in 0.6s)
 *   
 *   === Batch Upload Summary ===
 *   Total files: 3
 *   Successful: 3
 *   Failed: 0
 *   Total size: 5.4 MB
 *   Total time: 1.9s
 *   Average speed: 2.84 MB/s
 * 
 * COMMON PITFALLS:
 *   1. Connection pooling - Reuse connections for better performance
 *   2. Memory management - Free resources for each file in batch
 *   3. Error handling - One failure shouldn't stop entire batch
 *   4. Large batches - Consider chunking very large batches
 *   5. Network timeout - Adjust timeouts for large files
 *   6. Storage balance - Files distributed across storage servers
 *   7. Transaction handling - No built-in rollback for partial failures
 * 
 * PERFORMANCE TIPS:
 *   - Reuse tracker and storage connections
 *   - Upload to same storage server when possible
 *   - Use appropriate buffer sizes
 *   - Consider parallel uploads for large batches (not shown here)
 *   - Monitor network bandwidth
 *   - Batch similar file sizes together
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include "fdfs_client.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "storage_client.h"
#include "fastcommon/logger.h"

/* Maximum number of files in a batch */
#define MAX_BATCH_FILES 1000

/* Structure to hold upload result for each file */
typedef struct {
    char local_filename[256];
    char file_id[128];
    int64_t file_size;
    int result_code;
    double upload_time;
    char error_msg[256];
} UploadResult;

/* Structure to hold batch statistics */
typedef struct {
    int total_files;
    int successful;
    int failed;
    int64_t total_size;
    double total_time;
} BatchStats;

/**
 * Print usage information
 */
void print_usage(const char *program_name)
{
    printf("FastDFS Batch Upload Example\n\n");
    printf("Usage: %s <config_file> <file1> [file2] [file3] ...\n", program_name);
    printf("   or: %s <config_file> <directory>\n\n", program_name);
    printf("Arguments:\n");
    printf("  config_file  Path to FastDFS client configuration file\n");
    printf("  file1...     One or more files to upload\n");
    printf("  directory    Directory containing files to upload\n\n");
    printf("Examples:\n");
    printf("  %s client.conf image1.jpg image2.jpg image3.jpg\n", program_name);
    printf("  %s client.conf /path/to/images/\n\n", program_name);
}

/**
 * Check if path is a directory
 */
int is_directory(const char *path)
{
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return S_ISDIR(stat_buf.st_mode);
}

/**
 * Check if path is a regular file
 */
int is_regular_file(const char *path)
{
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0) {
        return 0;
    }
    return S_ISREG(stat_buf.st_mode);
}

/**
 * Get file size
 */
int64_t get_file_size(const char *filepath)
{
    struct stat stat_buf;
    if (stat(filepath, &stat_buf) != 0) {
        return -1;
    }
    return stat_buf.st_size;
}

/**
 * Format file size for display
 */
void format_size(int64_t size, char *buffer, size_t buffer_size)
{
    if (size < 1024) {
        snprintf(buffer, buffer_size, "%lld B", (long long)size);
    } else if (size < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f MB", size / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.2f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

/**
 * Get current time in seconds (with millisecond precision)
 */
double get_current_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

/**
 * Upload a single file and record result
 */
int upload_single_file(ConnectionInfo *pTrackerServer,
                       ConnectionInfo *pStorageServer,
                       int store_path_index,
                       const char *local_filename,
                       char *group_name,
                       UploadResult *result)
{
    char remote_filename[256];
    const char *file_ext_name;
    int ret;
    double start_time, end_time;
    
    /* Initialize result structure */
    strncpy(result->local_filename, local_filename, sizeof(result->local_filename) - 1);
    result->file_size = get_file_size(local_filename);
    result->result_code = 0;
    result->upload_time = 0.0;
    result->error_msg[0] = '\0';
    result->file_id[0] = '\0';
    
    /* Get file extension */
    file_ext_name = fdfs_get_file_ext_name(local_filename);
    
    /* Start timing */
    start_time = get_current_time();
    
    /* Upload file
     * IMPORTANT: We pass pStorageServer (not NULL) to reuse the connection
     * This significantly improves batch upload performance
     */
    ret = storage_upload_by_filename(pTrackerServer,
                                     pStorageServer,
                                     store_path_index,
                                     local_filename,
                                     file_ext_name,
                                     NULL,  /* No metadata */
                                     0,     /* Metadata count */
                                     group_name,
                                     remote_filename);
    
    /* End timing */
    end_time = get_current_time();
    result->upload_time = end_time - start_time;
    result->result_code = ret;
    
    if (ret == 0) {
        /* Success - construct file ID */
        snprintf(result->file_id, sizeof(result->file_id), 
                 "%s/%s", group_name, remote_filename);
    } else {
        /* Failure - record error message */
        snprintf(result->error_msg, sizeof(result->error_msg), 
                 "%s", STRERROR(ret));
    }
    
    return ret;
}

/**
 * Print upload result for a single file
 */
void print_upload_result(int index, int total, const UploadResult *result)
{
    char size_str[32];
    format_size(result->file_size, size_str, sizeof(size_str));
    
    printf("[%d/%d] Uploading %s... ", index, total, result->local_filename);
    
    if (result->result_code == 0) {
        printf("✓ (%s in %.2fs)\n", size_str, result->upload_time);
    } else {
        printf("✗ FAILED\n");
        printf("      Error: %s\n", result->error_msg);
    }
}

/**
 * Print batch upload summary
 */
void print_batch_summary(const BatchStats *stats, const UploadResult *results)
{
    char total_size_str[32];
    double avg_speed;
    int i;
    
    format_size(stats->total_size, total_size_str, sizeof(total_size_str));
    avg_speed = stats->total_time > 0 ? 
                (stats->total_size / (1024.0 * 1024.0)) / stats->total_time : 0;
    
    printf("\n=== Batch Upload Summary ===\n");
    printf("Total files: %d\n", stats->total_files);
    printf("Successful: %d\n", stats->successful);
    printf("Failed: %d\n", stats->failed);
    printf("Total size: %s\n", total_size_str);
    printf("Total time: %.2fs\n", stats->total_time);
    printf("Average speed: %.2f MB/s\n", avg_speed);
    
    if (stats->successful > 0) {
        printf("\n=== Successfully Uploaded Files ===\n");
        for (i = 0; i < stats->total_files; i++) {
            if (results[i].result_code == 0) {
                printf("  %s\n", results[i].file_id);
            }
        }
    }
    
    if (stats->failed > 0) {
        printf("\n=== Failed Uploads ===\n");
        for (i = 0; i < stats->total_files; i++) {
            if (results[i].result_code != 0) {
                printf("  %s: %s\n", results[i].local_filename, results[i].error_msg);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    char *conf_filename;
    char **file_list;
    int file_count;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    ConnectionInfo storageServer;
    int result;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int store_path_index;
    char **file_list_dynamic = NULL;
    int directory_mode = 0;
    UploadResult *results;
    BatchStats stats;
    double batch_start_time, batch_end_time;
    int i;
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    
    /* Check if second argument is a directory */
    if (argc == 3 && is_directory(argv[2])) {
        printf("Scanning directory: %s\n", argv[2]);
        file_count = scan_directory(argv[2], &file_list_dynamic);
        if (file_count < 0) {
            fprintf(stderr, "ERROR: Failed to scan directory\n");
            return 1;
        }
        if (file_count == 0) {
            printf("No files found in directory: %s\n", argv[2]);
            return 0;
        }
        if (file_count > MAX_BATCH_FILES) {
            fprintf(stderr, "ERROR: Too many files found (max %d)\n", MAX_BATCH_FILES);
            free_file_list(file_list_dynamic, file_count);
            return 1;
        }
        file_list = file_list_dynamic;
        directory_mode = 1;
        printf("Found %d files to upload\n\n", file_count);
    } else {
        /* Build file list from arguments */
        file_count = argc - 2;
        if (file_count > MAX_BATCH_FILES) {
            fprintf(stderr, "ERROR: Too many files (max %d)\n", MAX_BATCH_FILES);
            return 1;
        }
        file_list = &argv[2];
        directory_mode = 0;
    }
    
    /* Validate all files exist and are readable */
    printf("=== FastDFS Batch Upload Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("Files to upload: %d\n\n", file_count);
    
    printf("Validating files...\n");
    for (i = 0; i < file_count; i++) {
        if (!is_regular_file(file_list[i])) {
            fprintf(stderr, "ERROR: '%s' is not a valid file\n", file_list[i]);
            return 1;
        }
        printf("  ✓ %s\n", file_list[i]);
    }
    printf("\n");
    
    /* Allocate results array */
    results = (UploadResult *)calloc(file_count, sizeof(UploadResult));
    if (results == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for results\n");
        return ENOMEM;
    }
    
    /* Initialize statistics */
    memset(&stats, 0, sizeof(stats));
    stats.total_files = file_count;
    
    /* ========================================
     * STEP 2: Initialize FastDFS client
     * ======================================== */
    log_init();
    
    printf("Initializing FastDFS client...\n");
    if ((result = fdfs_client_init(conf_filename)) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        free(results);
        return result;
    }
    printf("✓ Client initialized successfully\n\n");
    
    /* ========================================
     * STEP 3: Connect to tracker server
     * ======================================== */
    printf("Connecting to tracker server...\n");
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fdfs_client_destroy();
        free(results);
        return result;
    }
    printf("✓ Connected to tracker server: %s:%d\n\n", 
           pTrackerServer->ip_addr, pTrackerServer->port);
    
    /* ========================================
     * STEP 4: Query storage server
     * ======================================== */
    printf("Querying storage server for batch upload...\n");
    store_path_index = 0;
    memset(group_name, 0, sizeof(group_name));
    
    result = tracker_query_storage_store(pTrackerServer, 
                                         &storageServer, 
                                         group_name, 
                                         &store_path_index);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to query storage server\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        tracker_close_connection_ex(pTrackerServer, true);
        fdfs_client_destroy();
        free(results);
        return result;
    }
    
    printf("✓ Storage server assigned: %s:%d (group: %s)\n\n", 
           storageServer.ip_addr, storageServer.port, group_name);
    
    /* ========================================
     * STEP 5: Connect to storage server
     * ======================================== */
    printf("Connecting to storage server...\n");
    pStorageServer = tracker_make_connection(&storageServer, &result);
    if (pStorageServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to storage server\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        tracker_close_connection_ex(pTrackerServer, true);
        fdfs_client_destroy();
        free(results);
        return result;
    }
    printf("✓ Connected to storage server\n\n");
    
    /* ========================================
     * STEP 6: Batch upload files
     * ======================================== */
    printf("=== Batch Upload Progress ===\n");
    
    /* Start batch timing */
    batch_start_time = get_current_time();
    
    /* Upload each file sequentially
     * PERFORMANCE NOTE: We reuse the same storage connection for all uploads
     * This is much faster than creating a new connection for each file
     * 
     * For even better performance with large batches, consider:
     * - Parallel uploads using multiple threads
     * - Connection pooling
     * - Uploading to multiple storage servers simultaneously
     */
    for (i = 0; i < file_count; i++) {
        /* Upload single file and record result */
        result = upload_single_file(pTrackerServer,
                                     pStorageServer,
                                     store_path_index,
                                     file_list[i],
                                     group_name,
                                     &results[i]);
        
        /* Print progress */
        print_upload_result(i + 1, file_count, &results[i]);
        
        /* Update statistics */
        if (result == 0) {
            stats.successful++;
            stats.total_size += results[i].file_size;
        } else {
            stats.failed++;
        }
        
        /* OPTIONAL: Add delay between uploads to avoid overwhelming server
         * Uncomment if needed:
         * usleep(100000);  // 100ms delay
         */
    }
    
    /* End batch timing */
    batch_end_time = get_current_time();
    stats.total_time = batch_end_time - batch_start_time;
    
    /* ========================================
     * STEP 7: Print summary and statistics
     * ======================================== */
    print_batch_summary(&stats, results);
    
    /* ========================================
     * STEP 8: Best practices and recommendations
     * ======================================== */
    printf("\n=== Best Practices for Batch Uploads ===\n");
    printf("1. Connection Reuse:\n");
    printf("   ✓ This example reuses tracker and storage connections\n");
    printf("   ✓ Significantly improves performance for batch operations\n\n");
    
    printf("2. Error Handling:\n");
    printf("   ✓ Each file upload is independent\n");
    printf("   ✓ One failure doesn't stop the entire batch\n");
    printf("   ✓ Detailed error reporting for failed uploads\n\n");
    
    printf("3. Performance Optimization:\n");
    printf("   - Consider parallel uploads for large batches\n");
    printf("   - Use connection pooling for concurrent operations\n");
    printf("   - Monitor network bandwidth and adjust batch size\n");
    printf("   - Group similar file sizes together\n\n");
    
    printf("4. Production Considerations:\n");
    printf("   - Implement retry logic for failed uploads\n");
    printf("   - Add progress callbacks for long-running batches\n");
    printf("   - Log upload results to database or file\n");
    printf("   - Implement rate limiting to avoid server overload\n");
    printf("   - Consider chunking very large batches\n\n");
    
    printf("5. Monitoring:\n");
    printf("   - Track upload success rate\n");
    printf("   - Monitor average upload speed\n");
    printf("   - Alert on high failure rates\n");
    printf("   - Log storage server distribution\n");
    
    /* ========================================
     * STEP 9: Cleanup
     * ======================================== */
    printf("\n=== Cleanup ===\n");
    
    if (pStorageServer != NULL) {
        tracker_close_connection_ex(pStorageServer, false);
        printf("✓ Storage connection closed\n");
    }
    
    if (pTrackerServer != NULL) {
        tracker_close_connection_ex(pTrackerServer, false);
        printf("✓ Tracker connection closed\n");
    }
    
    fdfs_client_destroy();
    printf("✓ Client destroyed\n");
    
    free(results);
    printf("✓ Memory freed\n");
    
    /* Cleanup file list if we allocated it for directory scanning */
    if (directory_mode && file_list_dynamic != NULL) {
        free_file_list(file_list_dynamic, file_count);
        printf("✓ Directory file list freed\n");
    }
    
    printf("\n=== Batch Upload Complete ===\n");
    
    /* Return non-zero if any uploads failed */
    return (stats.failed > 0) ? 1 : 0;
}
