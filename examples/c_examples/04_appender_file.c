/**
 * FastDFS Appender File Example
 * 
 * This example demonstrates how to work with appender files in FastDFS.
 * Appender files allow you to append data to existing files, which is useful
 * for log files, incremental backups, or any scenario where you need to
 * continuously add data to a file without re-uploading the entire content.
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./04_appender_file <config_file> <initial_file> <append_file>
 * 
 * EXAMPLE:
 *   ./04_appender_file client.conf /path/to/log_part1.txt /path/to/log_part2.txt
 * 
 * EXPECTED OUTPUT:
 *   Initial upload successful!
 *   File ID: group1/M00/00/00/wKgBcGXxxx.txt
 *   Initial size: 1024 bytes
 *   
 *   Appending data...
 *   Append successful!
 *   New file size: 2048 bytes
 *   
 *   Modified appender file!
 *   Final size: 2560 bytes
 * 
 * COMMON PITFALLS:
 *   1. Appending to non-appender file - Must upload as appender initially
 *   2. File size limits - Check max_appender_file_size in storage config
 *   3. Concurrent appends - FastDFS handles locking, but be aware of race conditions
 *   4. Cannot truncate - Appender files can only grow, not shrink
 *   5. Modify vs Append - Use modify for random access, append for sequential
 *   6. Storage server must support appender - Check storage server version
 * 
 * KEY CONCEPTS:
 *   - Appender files are special files that support append operations
 *   - They have a different file ID format (contains 'A' flag)
 *   - You can append data multiple times without re-uploading
 *   - Useful for logs, incremental data, streaming scenarios
 *   - Can also modify existing content at specific offsets
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fdfs_client.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "storage_client.h"
#include "fastcommon/logger.h"

/**
 * Print usage information
 */
void print_usage(const char *program_name)
{
    printf("FastDFS Appender File Example\n\n");
    printf("Usage: %s <config_file> <initial_file> <append_file>\n\n", program_name);
    printf("Arguments:\n");
    printf("  config_file   Path to FastDFS client configuration file\n");
    printf("  initial_file  Path to the initial file to upload as appender\n");
    printf("  append_file   Path to the file whose content will be appended\n\n");
    printf("Example:\n");
    printf("  %s client.conf log_part1.txt log_part2.txt\n\n", program_name);
    printf("What this does:\n");
    printf("  1. Uploads initial_file as an appender file\n");
    printf("  2. Appends the content of append_file to it\n");
    printf("  3. Demonstrates modify operation on the appender file\n");
}

/**
 * Validate that the file exists and is readable
 */
int validate_file(const char *filepath)
{
    struct stat stat_buf;
    
    if (stat(filepath, &stat_buf) != 0) {
        fprintf(stderr, "ERROR: Cannot access file '%s': %s\n", 
                filepath, strerror(errno));
        return errno;
    }
    
    if (!S_ISREG(stat_buf.st_mode)) {
        fprintf(stderr, "ERROR: '%s' is not a regular file\n", filepath);
        return EINVAL;
    }
    
    return 0;
}

/**
 * Read file content into buffer
 */
int read_file_content(const char *filepath, char **buffer, int64_t *file_size)
{
    FILE *fp;
    struct stat stat_buf;
    
    if (stat(filepath, &stat_buf) != 0) {
        return errno;
    }
    
    *file_size = stat_buf.st_size;
    *buffer = (char *)malloc(*file_size);
    if (*buffer == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for file content\n");
        return ENOMEM;
    }
    
    fp = fopen(filepath, "rb");
    if (fp == NULL) {
        free(*buffer);
        *buffer = NULL;
        return errno;
    }
    
    if (fread(*buffer, 1, *file_size, fp) != *file_size) {
        fclose(fp);
        free(*buffer);
        *buffer = NULL;
        return EIO;
    }
    
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    char *conf_filename;
    char *initial_filename;
    char *append_filename;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    ConnectionInfo storageServer;
    int result;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
    char appender_file_id[128];
    const char *file_ext_name;
    int store_path_index;
    FDFSFileInfo file_info;
    char *append_buffer = NULL;
    int64_t append_size = 0;
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    initial_filename = argv[2];
    append_filename = argv[3];
    
    /* Validate input files */
    if ((result = validate_file(initial_filename)) != 0) {
        return result;
    }
    if ((result = validate_file(append_filename)) != 0) {
        return result;
    }
    
    printf("=== FastDFS Appender File Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("Initial file: %s\n", initial_filename);
    printf("Append file: %s\n\n", append_filename);
    
    /* ========================================
     * STEP 2: Initialize FastDFS client
     * ======================================== */
    log_init();
    
    printf("Initializing FastDFS client...\n");
    if ((result = fdfs_client_init(conf_filename)) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
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
        return result;
    }
    printf("✓ Connected to tracker server: %s:%d\n\n", 
           pTrackerServer->ip_addr, pTrackerServer->port);
    
    /* ========================================
     * STEP 4: Query storage server
     * ======================================== */
    printf("Querying storage server for upload...\n");
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
        return result;
    }
    printf("✓ Connected to storage server\n\n");
    
    /* ========================================
     * STEP 6: Upload initial file as APPENDER
     * ======================================== */
    /* IMPORTANT: Use storage_upload_appender_by_filename() instead of
     * storage_upload_by_filename() to create an appender file.
     * This marks the file as appendable in FastDFS.
     */
    file_ext_name = fdfs_get_file_ext_name(initial_filename);
    
    printf("=== PHASE 1: Upload Initial Appender File ===\n");
    printf("Uploading '%s' as appender file...\n", initial_filename);
    
    result = storage_upload_appender_by_filename(pTrackerServer,
                                                  pStorageServer,
                                                  store_path_index,
                                                  initial_filename,
                                                  file_ext_name,
                                                  NULL,  /* No metadata */
                                                  0,     /* Metadata count */
                                                  group_name,
                                                  remote_filename);
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to upload appender file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - Storage server doesn't support appender files\n");
        fprintf(stderr, "  - Insufficient disk space\n");
        fprintf(stderr, "  - File too large for appender (check max_appender_file_size)\n");
        goto cleanup;
    }
    
    /* Construct file ID */
    snprintf(appender_file_id, sizeof(appender_file_id), 
             "%s/%s", group_name, remote_filename);
    
    printf("✓ Initial upload successful!\n");
    printf("  File ID: %s\n", appender_file_id);
    
    /* Get initial file info */
    result = fdfs_get_file_info(group_name, remote_filename, &file_info);
    if (result == 0) {
        printf("  Initial size: %lld bytes\n", (long long)file_info.file_size);
        printf("  CRC32: %u\n", file_info.crc32);
    }
    printf("\n");
    
    /* ========================================
     * STEP 7: Append data to the appender file
     * ======================================== */
    printf("=== PHASE 2: Append Data to File ===\n");
    printf("Reading append file content...\n");
    
    /* Read the content to append */
    result = read_file_content(append_filename, &append_buffer, &append_size);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to read append file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        goto cleanup;
    }
    printf("✓ Read %lld bytes from append file\n", (long long)append_size);
    
    printf("Appending data to appender file...\n");
    
    /* Append data to the existing appender file
     * This operation adds data to the end of the file without re-uploading
     * the entire content. Very efficient for incremental updates.
     * 
     * Parameters:
     *   - pTrackerServer: tracker connection
     *   - pStorageServer: storage connection (can be NULL)
     *   - append_buffer: data to append
     *   - append_size: size of data to append
     *   - group_name: group name of the appender file
     *   - remote_filename: filename of the appender file
     */
    result = storage_append_by_filebuff(pTrackerServer,
                                        pStorageServer,
                                        append_buffer,
                                        append_size,
                                        group_name,
                                        remote_filename);
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to append data\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - File is not an appender file\n");
        fprintf(stderr, "  - File size would exceed max_appender_file_size\n");
        fprintf(stderr, "  - Storage server connection lost\n");
        fprintf(stderr, "  - Concurrent modification conflict\n");
        goto cleanup;
    }
    
    printf("✓ Append successful!\n");
    
    /* Get updated file info */
    result = fdfs_get_file_info(group_name, remote_filename, &file_info);
    if (result == 0) {
        printf("  New file size: %lld bytes\n", (long long)file_info.file_size);
        printf("  New CRC32: %u\n", file_info.crc32);
    }
    printf("\n");
    
    /* ========================================
     * STEP 8: Modify appender file content
     * ======================================== */
    printf("=== PHASE 3: Modify Appender File ===\n");
    printf("Demonstrating modify operation...\n");
    
    /* You can also modify content at specific offsets in an appender file
     * This is useful for updating headers, correcting data, etc.
     * 
     * IMPORTANT: Modify doesn't change file size, it overwrites existing data
     */
    const char *modify_data = "MODIFIED";
    int64_t modify_offset = 0;  /* Modify at beginning of file */
    
    result = storage_modify_by_filebuff(pTrackerServer,
                                        pStorageServer,
                                        modify_data,
                                        modify_offset,
                                        strlen(modify_data),
                                        group_name,
                                        remote_filename);
    
    if (result != 0) {
        fprintf(stderr, "WARNING: Failed to modify appender file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        /* Non-fatal, continue */
    } else {
        printf("✓ Modified first %zu bytes of appender file\n", strlen(modify_data));
        
        /* Get final file info */
        result = fdfs_get_file_info(group_name, remote_filename, &file_info);
        if (result == 0) {
            printf("  Final size: %lld bytes (unchanged by modify)\n", 
                   (long long)file_info.file_size);
            printf("  Final CRC32: %u\n", file_info.crc32);
        }
    }
    printf("\n");
    
    /* ========================================
     * STEP 9: Display summary
     * ======================================== */
    printf("=== Summary ===\n");
    printf("Appender file operations completed successfully!\n");
    printf("File ID: %s\n", appender_file_id);
    printf("\nOperations performed:\n");
    printf("  1. ✓ Uploaded initial file as appender\n");
    printf("  2. ✓ Appended additional data\n");
    printf("  3. ✓ Modified content at specific offset\n");
    printf("\nUse cases for appender files:\n");
    printf("  - Log file aggregation\n");
    printf("  - Incremental backups\n");
    printf("  - Streaming data collection\n");
    printf("  - Multi-part uploads\n");
    printf("  - Real-time data appending\n");
    
    result = 0;

cleanup:
    /* ========================================
     * STEP 10: Cleanup
     * ======================================== */
    if (append_buffer != NULL) {
        free(append_buffer);
    }
    
    if (pStorageServer != NULL) {
        tracker_close_connection_ex(pStorageServer, result != 0);
    }
    if (pTrackerServer != NULL) {
        tracker_close_connection_ex(pTrackerServer, result != 0);
    }
    fdfs_client_destroy();
    
    printf("\n=== Cleanup Complete ===\n");
    
    return result;
}
