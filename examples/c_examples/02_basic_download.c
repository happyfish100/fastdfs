/**
 * FastDFS Basic Download Example
 * 
 * This example demonstrates how to download a file from FastDFS storage server.
 * It shows three download methods: to buffer, to file, and using callback.
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./02_basic_download <config_file> <file_id> [output_file]
 * 
 * EXAMPLES:
 *   # Download to buffer (auto-named output file)
 *   ./02_basic_download client.conf group1/M00/00/00/wKgBcGXxxx.jpg
 * 
 *   # Download to specific file
 *   ./02_basic_download client.conf group1/M00/00/00/wKgBcGXxxx.jpg output.jpg
 * 
 * EXPECTED OUTPUT:
 *   Download successful!
 *   File size: 12345 bytes
 *   Saved to: output.jpg
 * 
 * COMMON PITFALLS:
 *   1. Invalid file ID format - Must be "group_name/path/filename"
 *   2. File not found - Verify file exists on storage server
 *   3. Permission denied - Check write permissions for output directory
 *   4. Network timeout - Increase network_timeout for large files
 *   5. Disk space - Ensure sufficient space for downloaded file
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
 * Callback function for downloading file data
 * This is called multiple times as data chunks are received
 */
int download_callback(void *arg, const int64_t file_size, 
                     const char *data, const int current_size)
{
    FILE *fp = (FILE *)arg;
    
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Invalid file pointer in callback\n");
        return EINVAL;
    }
    
    /* Write received data chunk to file */
    if (fwrite(data, current_size, 1, fp) != 1) {
        int err = errno != 0 ? errno : EIO;
        fprintf(stderr, "ERROR: Failed to write data: %s\n", strerror(err));
        return err;
    }
    
    /* Print progress for large files */
    static int64_t total_received = 0;
    total_received += current_size;
    if (file_size > 1024 * 1024) {  /* Show progress for files > 1MB */
        printf("\rProgress: %lld / %lld bytes (%.1f%%)", 
               (long long)total_received, 
               (long long)file_size,
               (total_received * 100.0) / file_size);
        fflush(stdout);
    }
    
    return 0;
}

/**
 * Print usage information
 */
void print_usage(const char *program_name)
{
    printf("FastDFS Basic Download Example\n\n");
    printf("Usage: %s <config_file> <file_id> [output_file]\n\n", program_name);
    printf("Arguments:\n");
    printf("  config_file   Path to FastDFS client configuration file\n");
    printf("  file_id       FastDFS file ID (format: group_name/path/filename)\n");
    printf("  output_file   Optional: Local file path to save (default: auto-named)\n\n");
    printf("Examples:\n");
    printf("  %s client.conf group1/M00/00/00/wKgBcGXxxx.jpg\n", program_name);
    printf("  %s client.conf group1/M00/00/00/wKgBcGXxxx.jpg output.jpg\n\n", 
           program_name);
}

/**
 * Parse file ID into group name and filename
 * File ID format: "group_name/path/filename"
 */
int parse_file_id(const char *file_id, char *group_name, char *filename)
{
    const char *pSeperator;
    int group_len;
    
    /* Find the separator between group name and filename */
    pSeperator = strchr(file_id, FDFS_FILE_ID_SEPERATOR);
    if (pSeperator == NULL) {
        fprintf(stderr, "ERROR: Invalid file ID format\n");
        fprintf(stderr, "Expected format: group_name/path/filename\n");
        fprintf(stderr, "Example: group1/M00/00/00/wKgBcGXxxx.jpg\n");
        return EINVAL;
    }
    
    /* Extract group name */
    group_len = pSeperator - file_id;
    if (group_len >= FDFS_GROUP_NAME_MAX_LEN) {
        fprintf(stderr, "ERROR: Group name too long (max %d characters)\n", 
                FDFS_GROUP_NAME_MAX_LEN);
        return EINVAL;
    }
    
    memcpy(group_name, file_id, group_len);
    group_name[group_len] = '\0';
    
    /* Extract filename (skip the separator) */
    strcpy(filename, pSeperator + 1);
    
    return 0;
}

/**
 * Write buffer to file
 */
int write_to_file(const char *filename, const char *buff, const int64_t file_size)
{
    FILE *fp;
    int result = 0;
    
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        result = errno != 0 ? errno : EPERM;
        fprintf(stderr, "ERROR: Cannot create file '%s': %s\n", 
                filename, strerror(result));
        return result;
    }
    
    if (fwrite(buff, file_size, 1, fp) != 1) {
        result = errno != 0 ? errno : EIO;
        fprintf(stderr, "ERROR: Failed to write to file: %s\n", strerror(result));
    }
    
    fclose(fp);
    return result;
}

int main(int argc, char *argv[])
{
    char *conf_filename;
    char *file_id;
    char *output_filename = NULL;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    ConnectionInfo storageServer;
    int result;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
    int64_t file_size = 0;
    int download_method = 1;  /* 1=to_file, 2=to_buffer, 3=callback */
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    file_id = argv[2];
    
    if (argc >= 4) {
        output_filename = argv[3];
        download_method = 1;  /* Download to specified file */
    } else {
        download_method = 2;  /* Download to buffer, then save */
    }
    
    printf("=== FastDFS Basic Download Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("File ID: %s\n", file_id);
    if (output_filename) {
        printf("Output file: %s\n", output_filename);
    }
    printf("\n");
    
    /* ========================================
     * STEP 2: Parse file ID
     * ======================================== */
    printf("Parsing file ID...\n");
    if ((result = parse_file_id(file_id, group_name, remote_filename)) != 0) {
        return result;
    }
    printf("✓ Group name: %s\n", group_name);
    printf("✓ Remote filename: %s\n\n", remote_filename);
    
    /* ========================================
     * STEP 3: Initialize logging and client
     * ======================================== */
    log_init();
    /* Uncomment for debug logging:
     * g_log_context.log_level = LOG_DEBUG;
     */
    
    printf("Initializing FastDFS client...\n");
    if ((result = fdfs_client_init(conf_filename)) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
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
        fdfs_client_destroy();
        return result;
    }
    printf("✓ Connected to tracker: %s:%d\n\n", 
           pTrackerServer->ip_addr, pTrackerServer->port);
    
    /* ========================================
     * STEP 5: Query storage server for download
     * ======================================== */
    printf("Querying storage server for download...\n");
    
    /* Query which storage server has this file
     * tracker_query_storage_fetch returns a storage server that has the file
     */
    result = tracker_query_storage_fetch(pTrackerServer, 
                                         &storageServer, 
                                         group_name, 
                                         remote_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to query storage server\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - File does not exist\n");
        fprintf(stderr, "  - Invalid group name or filename\n");
        fprintf(stderr, "  - Storage server offline\n");
        tracker_close_connection_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    printf("✓ Storage server located:\n");
    printf("  IP: %s\n", storageServer.ip_addr);
    printf("  Port: %d\n\n", storageServer.port);
    
    /* ========================================
     * STEP 6: Connect to storage server
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
     * STEP 7: Download the file
     * ======================================== */
    printf("Downloading file...\n");
    
    if (download_method == 1 && output_filename != NULL) {
        /* METHOD 1: Download directly to file
         * This is the most efficient method for large files
         * as it doesn't load the entire file into memory
         */
        printf("Using method: Direct to file\n");
        
        result = storage_download_file_to_file(pTrackerServer,
                                               pStorageServer,
                                               group_name,
                                               remote_filename,
                                               output_filename,
                                               &file_size);
        
    } else if (download_method == 2) {
        /* METHOD 2: Download to buffer, then write to file
         * Good for small files or when you need to process the data
         * before saving
         */
        char *file_buff = NULL;
        const char *filename_only;
        
        printf("Using method: Download to buffer\n");
        
        result = storage_download_file_to_buff(pTrackerServer,
                                               pStorageServer,
                                               group_name,
                                               remote_filename,
                                               &file_buff,
                                               &file_size);
        
        if (result == 0) {
            /* Extract filename from remote path if no output specified */
            filename_only = strrchr(remote_filename, '/');
            if (filename_only != NULL) {
                filename_only++;  /* Skip the '/' */
            } else {
                filename_only = remote_filename;
            }
            
            /* Write buffer to file */
            result = write_to_file(filename_only, file_buff, file_size);
            if (result == 0) {
                output_filename = (char *)filename_only;
            }
            
            /* Free the downloaded buffer */
            free(file_buff);
        }
        
    } else {
        /* METHOD 3: Download using callback
         * Useful for processing data as it arrives (streaming)
         * or for very large files with progress tracking
         */
        FILE *fp;
        
        printf("Using method: Callback (streaming)\n");
        
        /* Generate output filename if not specified */
        if (output_filename == NULL) {
            const char *filename_only = strrchr(remote_filename, '/');
            output_filename = (char *)(filename_only ? filename_only + 1 : remote_filename);
        }
        
        fp = fopen(output_filename, "wb");
        if (fp == NULL) {
            result = errno != 0 ? errno : EPERM;
            fprintf(stderr, "ERROR: Cannot create file '%s': %s\n", 
                    output_filename, strerror(result));
        } else {
            /* Download with callback
             * Parameters:
             *   - file_offset: 0 (start from beginning)
             *   - download_bytes: 0 (download entire file)
             */
            result = storage_download_file_ex(pTrackerServer,
                                             pStorageServer,
                                             group_name,
                                             remote_filename,
                                             0,  /* file_offset */
                                             0,  /* download_bytes (0=all) */
                                             download_callback,
                                             fp,
                                             &file_size);
            fclose(fp);
            
            if (file_size > 1024 * 1024) {
                printf("\n");  /* New line after progress bar */
            }
        }
    }
    
    /* ========================================
     * STEP 8: Check download result
     * ======================================== */
    if (result != 0) {
        fprintf(stderr, "\nERROR: Failed to download file\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "  - File was deleted from storage\n");
        fprintf(stderr, "  - Network timeout (try increasing network_timeout)\n");
        fprintf(stderr, "  - Insufficient disk space\n");
        fprintf(stderr, "  - Permission denied on output directory\n");
        
        tracker_close_connection_ex(pStorageServer, true);
        tracker_close_connection_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    printf("✓ Download successful!\n\n");
    
    /* ========================================
     * STEP 9: Display download results
     * ======================================== */
    printf("=== Download Results ===\n");
    printf("File size: %lld bytes", (long long)file_size);
    
    /* Display human-readable file size */
    if (file_size >= 1024 * 1024 * 1024) {
        printf(" (%.2f GB)\n", file_size / (1024.0 * 1024.0 * 1024.0));
    } else if (file_size >= 1024 * 1024) {
        printf(" (%.2f MB)\n", file_size / (1024.0 * 1024.0));
    } else if (file_size >= 1024) {
        printf(" (%.2f KB)\n", file_size / 1024.0);
    } else {
        printf("\n");
    }
    
    if (output_filename) {
        printf("Saved to: %s\n", output_filename);
        
        /* Verify the downloaded file */
        struct stat stat_buf;
        if (stat(output_filename, &stat_buf) == 0) {
            if (stat_buf.st_size == file_size) {
                printf("✓ File size verified\n");
            } else {
                fprintf(stderr, "WARNING: File size mismatch!\n");
                fprintf(stderr, "  Expected: %lld bytes\n", (long long)file_size);
                fprintf(stderr, "  Actual: %lld bytes\n", (long long)stat_buf.st_size);
            }
        }
    }
    
    /* ========================================
     * STEP 10: Cleanup
     * ======================================== */
    printf("\n=== Cleanup ===\n");
    tracker_close_connection_ex(pStorageServer, false);
    printf("✓ Storage connection closed\n");
    
    tracker_close_connection_ex(pTrackerServer, false);
    printf("✓ Tracker connection closed\n");
    
    fdfs_client_destroy();
    printf("✓ Client destroyed\n");
    
    printf("\n=== Download Complete ===\n");
    
    return 0;
}
