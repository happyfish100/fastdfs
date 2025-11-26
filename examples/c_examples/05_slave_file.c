/**
 * FastDFS Slave File Example
 * 
 * This example demonstrates how to work with slave files in FastDFS.
 * Slave files are associated files linked to a master file, commonly used
 * for storing different versions or variants of the same content, such as:
 * - Image thumbnails (small, medium, large)
 * - Video transcodes (different resolutions/formats)
 * - Document previews
 * - Processed versions of original files
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./05_slave_file <config_file> <master_file> <slave_file> <prefix_name>
 * 
 * EXAMPLE:
 *   ./05_slave_file client.conf original.jpg thumbnail.jpg _150x150
 *   ./05_slave_file client.conf video.mp4 video_720p.mp4 _720p
 * 
 * EXPECTED OUTPUT:
 *   Master file uploaded!
 *   File ID: group1/M00/00/00/wKgBcGXxxx.jpg
 *   
 *   Slave file uploaded!
 *   Slave File ID: group1/M00/00/00/wKgBcGXxxx_150x150.jpg
 *   
 *   Slave file downloaded successfully!
 *   Downloaded to: downloaded_slave_150x150.jpg
 * 
 * COMMON PITFALLS:
 *   1. Prefix naming - Must start with underscore or hyphen (e.g., _thumb, -small)
 *   2. Master file must exist - Upload master before slave
 *   3. Same storage server - Slave must be on same server as master
 *   4. Deleting master - Deleting master doesn't auto-delete slaves
 *   5. Prefix uniqueness - Different slaves need different prefixes
 *   6. File extension - Slave can have different extension than master
 * 
 * KEY CONCEPTS:
 *   - Slave files are stored on the same storage server as the master
 *   - Slave filename = master_filename + prefix + extension
 *   - Multiple slaves can be attached to one master
 *   - Slaves are independent files but logically linked
 *   - Useful for multi-resolution images, video transcodes, etc.
 *   - Slaves don't automatically inherit master's metadata
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
    printf("FastDFS Slave File Example\n\n");
    printf("Usage: %s <config_file> <master_file> <slave_file> <prefix_name>\n\n", 
           program_name);
    printf("Arguments:\n");
    printf("  config_file  Path to FastDFS client configuration file\n");
    printf("  master_file  Path to the master file to upload\n");
    printf("  slave_file   Path to the slave file to upload\n");
    printf("  prefix_name  Prefix for slave file (e.g., _150x150, _thumb, -small)\n\n");
    printf("Example:\n");
    printf("  %s client.conf photo.jpg thumbnail.jpg _150x150\n", program_name);
    printf("  %s client.conf video.mp4 video_hd.mp4 _720p\n\n", program_name);
    printf("What this does:\n");
    printf("  1. Uploads master file to FastDFS\n");
    printf("  2. Uploads slave file linked to master with prefix\n");
    printf("  3. Downloads the slave file to verify\n");
    printf("  4. Demonstrates querying slave file info\n");
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
 * Validate slave file prefix
 * Prefix should start with underscore or hyphen
 */
int validate_prefix(const char *prefix)
{
    if (prefix == NULL || strlen(prefix) == 0) {
        fprintf(stderr, "ERROR: Prefix cannot be empty\n");
        return EINVAL;
    }
    
    if (prefix[0] != '_' && prefix[0] != '-') {
        fprintf(stderr, "WARNING: Prefix '%s' doesn't start with _ or -\n", prefix);
        fprintf(stderr, "         This is recommended but not required\n");
    }
    
    if (strlen(prefix) > 64) {
        fprintf(stderr, "ERROR: Prefix too long (max 64 characters)\n");
        return EINVAL;
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    char *conf_filename;
    char *master_filename;
    char *slave_filename;
    char *prefix_name;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    ConnectionInfo storageServer;
    int result;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char master_remote_filename[256];
    char slave_remote_filename[256];
    char master_file_id[128];
    char slave_file_id[128];
    const char *master_ext;
    const char *slave_ext;
    int store_path_index;
    FDFSFileInfo file_info;
    char download_filename[256];
    int64_t file_size;
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc != 5) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    master_filename = argv[2];
    slave_filename = argv[3];
    prefix_name = argv[4];
    
    /* Validate input files */
    if ((result = validate_file(master_filename)) != 0) {
        return result;
    }
    if ((result = validate_file(slave_filename)) != 0) {
        return result;
    }
    if ((result = validate_prefix(prefix_name)) != 0) {
        return result;
    }
    
    printf("=== FastDFS Slave File Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("Master file: %s\n", master_filename);
    printf("Slave file: %s\n", slave_filename);
    printf("Prefix name: %s\n\n", prefix_name);
    
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
     * STEP 6: Upload MASTER file
     * ======================================== */
    printf("=== PHASE 1: Upload Master File ===\n");
    printf("Uploading master file '%s'...\n", master_filename);
    
    master_ext = fdfs_get_file_ext_name(master_filename);
    
    /* Upload master file using standard upload function */
    result = storage_upload_by_filename(pTrackerServer,
                                        pStorageServer,
                                        store_path_index,
                                        master_filename,
                                        master_ext,
                                        NULL,  /* No metadata */
                                        0,     /* Metadata count */
                                        group_name,
                                        master_remote_filename);
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to upload master file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        goto cleanup;
    }
    
    /* Construct master file ID */
    snprintf(master_file_id, sizeof(master_file_id), 
             "%s/%s", group_name, master_remote_filename);
    
    printf("✓ Master file uploaded successfully!\n");
    printf("  Master File ID: %s\n", master_file_id);
    
    /* Get master file info */
    result = fdfs_get_file_info(group_name, master_remote_filename, &file_info);
    if (result == 0) {
        printf("  Master file size: %lld bytes\n", (long long)file_info.file_size);
        printf("  CRC32: %u\n", file_info.crc32);
    }
    printf("\n");
    
    /* ========================================
     * STEP 7: Upload SLAVE file
     * ======================================== */
    printf("=== PHASE 2: Upload Slave File ===\n");
    printf("Uploading slave file '%s' with prefix '%s'...\n", 
           slave_filename, prefix_name);
    
    slave_ext = fdfs_get_file_ext_name(slave_filename);
    
    /* Upload slave file linked to master
     * IMPORTANT: Use storage_upload_slave_by_filename() to create a slave file
     * 
     * The slave file will be stored on the same storage server as the master
     * and its filename will be: master_filename + prefix + extension
     * 
     * Parameters:
     *   - pTrackerServer: tracker connection
     *   - pStorageServer: storage connection (can be NULL)
     *   - slave_filename: local path to slave file
     *   - master_remote_filename: the master file's remote filename
     *   - prefix_name: prefix to identify this slave (e.g., _150x150)
     *   - slave_ext: file extension for slave file
     *   - NULL, 0: metadata (optional)
     *   - group_name: group name (same as master)
     *   - slave_remote_filename: output - generated slave filename
     */
    result = storage_upload_slave_by_filename(pTrackerServer,
                                              pStorageServer,
                                              slave_filename,
                                              master_remote_filename,
                                              prefix_name,
                                              slave_ext,
                                              NULL,  /* No metadata */
                                              0,     /* Metadata count */
                                              group_name,
                                              slave_remote_filename);
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to upload slave file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - Master file doesn't exist\n");
        fprintf(stderr, "  - Invalid prefix format\n");
        fprintf(stderr, "  - Storage server connection lost\n");
        fprintf(stderr, "  - Insufficient disk space\n");
        goto cleanup;
    }
    
    /* Construct slave file ID */
    snprintf(slave_file_id, sizeof(slave_file_id), 
             "%s/%s", group_name, slave_remote_filename);
    
    printf("✓ Slave file uploaded successfully!\n");
    printf("  Slave File ID: %s\n", slave_file_id);
    
    /* Get slave file info */
    result = fdfs_get_file_info(group_name, slave_remote_filename, &file_info);
    if (result == 0) {
        printf("  Slave file size: %lld bytes\n", (long long)file_info.file_size);
        printf("  CRC32: %u\n", file_info.crc32);
        printf("  Source IP: %s\n", file_info.source_ip_addr);
    }
    printf("\n");
    
    /* ========================================
     * STEP 8: Demonstrate filename relationship
     * ======================================== */
    printf("=== PHASE 3: Filename Relationship ===\n");
    printf("Master filename: %s\n", master_remote_filename);
    printf("Slave filename:  %s\n", slave_remote_filename);
    printf("\nNotice how slave filename is constructed:\n");
    printf("  Base: %s\n", master_remote_filename);
    printf("  + Prefix: %s\n", prefix_name);
    printf("  + Extension: .%s\n", slave_ext ? slave_ext : "(none)");
    printf("\n");
    
    /* ========================================
     * STEP 9: Download slave file to verify
     * ======================================== */
    printf("=== PHASE 4: Download Slave File ===\n");
    
    /* Construct download filename */
    snprintf(download_filename, sizeof(download_filename), 
             "downloaded_slave%s.%s", prefix_name, slave_ext ? slave_ext : "dat");
    
    printf("Downloading slave file to '%s'...\n", download_filename);
    
    /* Download the slave file
     * Use the same download function as for regular files
     */
    result = storage_download_file_to_file(pTrackerServer,
                                           pStorageServer,
                                           group_name,
                                           slave_remote_filename,
                                           download_filename,
                                           &file_size);
    
    if (result != 0) {
        fprintf(stderr, "WARNING: Failed to download slave file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        /* Non-fatal, continue */
    } else {
        printf("✓ Slave file downloaded successfully!\n");
        printf("  Downloaded to: %s\n", download_filename);
        printf("  Downloaded size: %lld bytes\n", (long long)file_size);
    }
    printf("\n");
    
    /* ========================================
     * STEP 10: Display summary and use cases
     * ======================================== */
    printf("=== Summary ===\n");
    printf("Slave file operations completed successfully!\n\n");
    
    printf("Files created:\n");
    printf("  Master: %s\n", master_file_id);
    printf("  Slave:  %s\n", slave_file_id);
    
    printf("\nCommon use cases for slave files:\n");
    printf("  1. Image thumbnails:\n");
    printf("     - master: original.jpg\n");
    printf("     - slave: original_150x150.jpg (thumbnail)\n");
    printf("     - slave: original_800x600.jpg (medium)\n");
    printf("     - slave: original_1920x1080.jpg (large)\n\n");
    
    printf("  2. Video transcoding:\n");
    printf("     - master: video.mp4 (original 4K)\n");
    printf("     - slave: video_1080p.mp4\n");
    printf("     - slave: video_720p.mp4\n");
    printf("     - slave: video_480p.mp4\n\n");
    
    printf("  3. Document formats:\n");
    printf("     - master: document.pdf\n");
    printf("     - slave: document_preview.jpg\n");
    printf("     - slave: document_text.txt\n\n");
    
    printf("  4. Audio quality variants:\n");
    printf("     - master: song.flac (lossless)\n");
    printf("     - slave: song_320k.mp3\n");
    printf("     - slave: song_128k.mp3\n\n");
    
    printf("Best practices:\n");
    printf("  ✓ Use descriptive prefixes (e.g., _thumb, _720p, _preview)\n");
    printf("  ✓ Upload master first, then slaves\n");
    printf("  ✓ Keep prefix naming consistent across your application\n");
    printf("  ✓ Document your prefix conventions\n");
    printf("  ✓ Consider slave file lifecycle with master deletion\n");
    
    result = 0;

cleanup:
    /* ========================================
     * STEP 11: Cleanup
     * ======================================== */
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
