/**
 * FastDFS Basic Upload Example
 * 
 * This example demonstrates how to upload a file to FastDFS storage server.
 * It covers the essential steps: initialization, connection, upload, and cleanup.
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./01_basic_upload <config_file> <local_file_path>
 * 
 * EXAMPLE:
 *   ./01_basic_upload client.conf /path/to/image.jpg
 * 
 * EXPECTED OUTPUT:
 *   Upload successful!
 *   Group name: group1
 *   Remote filename: M00/00/00/wKgBcGXxxx.jpg
 *   File ID: group1/M00/00/00/wKgBcGXxxx.jpg
 *   File size: 12345 bytes
 * 
 * COMMON PITFALLS:
 *   1. Tracker server not running - Check tracker_server in config
 *   2. Storage server not available - Verify storage server is running
 *   3. File permissions - Ensure read access to local file
 *   4. Network timeout - Adjust network_timeout in config if needed
 *   5. Invalid config path - Use absolute path or ensure relative path is correct
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
    printf("FastDFS Basic Upload Example\n\n");
    printf("Usage: %s <config_file> <local_file_path>\n\n", program_name);
    printf("Arguments:\n");
    printf("  config_file      Path to FastDFS client configuration file\n");
    printf("  local_file_path  Path to the local file to upload\n\n");
    printf("Example:\n");
    printf("  %s client.conf /path/to/image.jpg\n\n", program_name);
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
    
    if (stat_buf.st_size == 0) {
        fprintf(stderr, "WARNING: File '%s' is empty (0 bytes)\n", filepath);
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    char *conf_filename;
    char *local_filename;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    ConnectionInfo storageServer;
    int result;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
    const char *file_ext_name;
    int store_path_index;
    char file_id[128];
    FDFSFileInfo file_info;
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    local_filename = argv[2];
    
    /* Validate that the file exists and is readable */
    if ((result = validate_file(local_filename)) != 0) {
        return result;
    }
    
    printf("=== FastDFS Basic Upload Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("Local file: %s\n\n", local_filename);
    
    /* ========================================
     * STEP 2: Initialize logging system
     * ======================================== */
    log_init();
    /* Uncomment to enable debug logging:
     * g_log_context.log_level = LOG_DEBUG;
     */
    
    /* ========================================
     * STEP 3: Initialize FastDFS client
     * ======================================== */
    printf("Initializing FastDFS client...\n");
    if ((result = fdfs_client_init(conf_filename)) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - Config file not found or invalid\n");
        fprintf(stderr, "  - Invalid configuration parameters\n");
        fprintf(stderr, "  - Missing required settings in config\n");
        return result;
    }
    printf("✓ Client initialized successfully\n\n");
    
    /* ========================================
     * STEP 4: Connect to tracker server
     * ======================================== */
    printf("Connecting to tracker server...\n");
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - Tracker server is not running\n");
        fprintf(stderr, "  - Incorrect tracker_server address in config\n");
        fprintf(stderr, "  - Network connectivity issues\n");
        fprintf(stderr, "  - Firewall blocking connection\n");
        fdfs_client_destroy();
        return result;
    }
    printf("✓ Connected to tracker server: %s:%d\n\n", 
           pTrackerServer->ip_addr, pTrackerServer->port);
    
    /* ========================================
     * STEP 5: Query storage server for upload
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
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - No storage servers available\n");
        fprintf(stderr, "  - Storage servers are full\n");
        fprintf(stderr, "  - Storage servers not registered with tracker\n");
        tracker_close_connection_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    printf("✓ Storage server assigned:\n");
    printf("  Group: %s\n", group_name);
    printf("  IP: %s\n", storageServer.ip_addr);
    printf("  Port: %d\n", storageServer.port);
    printf("  Store path index: %d\n\n", store_path_index);
    
    /* ========================================
     * STEP 6: Connect to storage server
     * ======================================== */
    printf("Connecting to storage server...\n");
    pStorageServer = tracker_make_connection(&storageServer, &result);
    if (pStorageServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to storage server\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - Storage server is not running\n");
        fprintf(stderr, "  - Network connectivity issues\n");
        fprintf(stderr, "  - Storage server overloaded\n");
        tracker_close_connection_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    printf("✓ Connected to storage server\n\n");
    
    /* ========================================
     * STEP 7: Extract file extension
     * ======================================== */
    /* Extract file extension (without dot) from filename
     * For example: "image.jpg" -> "jpg"
     * This is used by FastDFS to determine file type
     */
    file_ext_name = fdfs_get_file_ext_name(local_filename);
    if (file_ext_name != NULL) {
        printf("File extension: %s\n", file_ext_name);
    } else {
        printf("No file extension detected\n");
    }
    
    /* ========================================
     * STEP 8: Upload the file
     * ======================================== */
    printf("\nUploading file...\n");
    
    /* Upload file to storage server
     * Parameters:
     *   - pTrackerServer: tracker connection
     *   - pStorageServer: storage connection (can be NULL to auto-connect)
     *   - store_path_index: which path to store on storage server
     *   - local_filename: local file path
     *   - file_ext_name: file extension (without dot)
     *   - NULL, 0: metadata list and count (none in this example)
     *   - group_name: input/output - group name
     *   - remote_filename: output - generated filename on server
     */
    result = storage_upload_by_filename(pTrackerServer,
                                        pStorageServer,
                                        store_path_index,
                                        local_filename,
                                        file_ext_name,
                                        NULL,  /* No metadata */
                                        0,     /* Metadata count */
                                        group_name,
                                        remote_filename);
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to upload file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - Insufficient disk space on storage server\n");
        fprintf(stderr, "  - File too large (check max_file_size)\n");
        fprintf(stderr, "  - Permission issues on storage server\n");
        fprintf(stderr, "  - Network timeout during transfer\n");
        tracker_close_connection_ex(pStorageServer, true);
        tracker_close_connection_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    printf("✓ Upload successful!\n\n");
    
    /* ========================================
     * STEP 9: Display upload results
     * ======================================== */
    /* Construct the file ID (group_name + filename) */
    snprintf(file_id, sizeof(file_id), "%s/%s", group_name, remote_filename);
    
    printf("=== Upload Results ===\n");
    printf("Group name: %s\n", group_name);
    printf("Remote filename: %s\n", remote_filename);
    printf("File ID: %s\n", file_id);
    
    /* ========================================
     * STEP 10: Retrieve and display file info
     * ======================================== */
    /* Get detailed file information from storage server */
    result = fdfs_get_file_info(group_name, remote_filename, &file_info);
    if (result == 0) {
        printf("\n=== File Information ===\n");
        printf("File size: %lld bytes\n", (long long)file_info.file_size);
        printf("CRC32: %u\n", file_info.crc32);
        printf("Source IP: %s\n", file_info.source_ip_addr);
        printf("Created: %s", ctime(&file_info.create_timestamp));
    } else {
        fprintf(stderr, "\nWARNING: Could not retrieve file info (error %d)\n", 
                result);
    }
    
    /* ========================================
     * STEP 11: Cleanup and close connections
     * ======================================== */
    printf("\n=== Cleanup ===\n");
    
    /* Close storage server connection
     * Second parameter: true = force close, false = return to pool
     */
    tracker_close_connection_ex(pStorageServer, result != 0);
    printf("✓ Storage connection closed\n");
    
    /* Close tracker server connection */
    tracker_close_connection_ex(pTrackerServer, result != 0);
    printf("✓ Tracker connection closed\n");
    
    /* Cleanup FastDFS client resources */
    fdfs_client_destroy();
    printf("✓ Client destroyed\n");
    
    printf("\n=== Upload Complete ===\n");
    printf("You can now download this file using the file ID:\n");
    printf("  %s\n", file_id);
    
    return 0;
}
