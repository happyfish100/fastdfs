/**
 * FastDFS Metadata Operations Example
 * 
 * This example demonstrates how to set and retrieve metadata for files
 * stored in FastDFS. Metadata is stored as key-value pairs and can be
 * used to store file attributes like dimensions, author, tags, etc.
 * 
 * Copyright (C) 2024
 * License: GPL v3
 * 
 * USAGE:
 *   ./03_metadata_operations <config_file> <operation> <file_id> [key=value ...]
 * 
 * OPERATIONS:
 *   set       - Set metadata (overwrites existing)
 *   merge     - Merge metadata (updates existing, adds new)
 *   get       - Get all metadata
 * 
 * EXAMPLES:
 *   # Set metadata (overwrite mode)
 *   ./03_metadata_operations client.conf set group1/M00/00/00/xxx.jpg \
 *       width=1920 height=1080 author=John
 * 
 *   # Merge metadata (update/add mode)
 *   ./03_metadata_operations client.conf merge group1/M00/00/00/xxx.jpg \
 *       tags=landscape camera=Canon
 * 
 *   # Get all metadata
 *   ./03_metadata_operations client.conf get group1/M00/00/00/xxx.jpg
 * 
 * EXPECTED OUTPUT:
 *   Metadata operation successful!
 *   Key: width, Value: 1920
 *   Key: height, Value: 1080
 *   Key: author, Value: John
 * 
 * COMMON PITFALLS:
 *   1. Metadata key/value length limits - Keys and values have max lengths
 *   2. Special characters - Avoid using '=' in keys or values
 *   3. Overwrite vs Merge - 'set' deletes old metadata, 'merge' preserves it
 *   4. File not found - Verify file exists before setting metadata
 *   5. Empty metadata - Getting metadata on file with none returns empty list
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fdfs_client.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "storage_client.h"
#include "fastcommon/logger.h"

/* Maximum metadata items to handle */
#define MAX_METADATA_COUNT 64

/**
 * Print usage information
 */
void print_usage(const char *program_name)
{
    printf("FastDFS Metadata Operations Example\n\n");
    printf("Usage: %s <config_file> <operation> <file_id> [key=value ...]\n\n", 
           program_name);
    printf("Operations:\n");
    printf("  set    - Set metadata (overwrites all existing metadata)\n");
    printf("  merge  - Merge metadata (updates existing, adds new)\n");
    printf("  get    - Get all metadata for the file\n\n");
    printf("Arguments:\n");
    printf("  config_file   Path to FastDFS client configuration file\n");
    printf("  operation     One of: set, merge, get\n");
    printf("  file_id       FastDFS file ID (format: group_name/path/filename)\n");
    printf("  key=value     Metadata pairs (for set/merge operations)\n\n");
    printf("Examples:\n");
    printf("  # Set metadata (overwrite)\n");
    printf("  %s client.conf set group1/M00/00/00/xxx.jpg width=1920 height=1080\n\n", 
           program_name);
    printf("  # Merge metadata (update/add)\n");
    printf("  %s client.conf merge group1/M00/00/00/xxx.jpg author=John\n\n", 
           program_name);
    printf("  # Get metadata\n");
    printf("  %s client.conf get group1/M00/00/00/xxx.jpg\n\n", 
           program_name);
    printf("Notes:\n");
    printf("  - 'set' operation deletes all existing metadata\n");
    printf("  - 'merge' operation preserves existing metadata\n");
    printf("  - Metadata keys and values have length limits\n");
    printf("  - Use quotes for values with spaces: author=\"John Doe\"\n");
}

/**
 * Parse file ID into group name and filename
 */
int parse_file_id(const char *file_id, char *group_name, char *filename)
{
    const char *pSeperator;
    int group_len;
    
    pSeperator = strchr(file_id, FDFS_FILE_ID_SEPERATOR);
    if (pSeperator == NULL) {
        fprintf(stderr, "ERROR: Invalid file ID format\n");
        fprintf(stderr, "Expected format: group_name/path/filename\n");
        return EINVAL;
    }
    
    group_len = pSeperator - file_id;
    if (group_len >= FDFS_GROUP_NAME_MAX_LEN) {
        fprintf(stderr, "ERROR: Group name too long\n");
        return EINVAL;
    }
    
    memcpy(group_name, file_id, group_len);
    group_name[group_len] = '\0';
    strcpy(filename, pSeperator + 1);
    
    return 0;
}

/**
 * Parse metadata from command line arguments
 * Format: key=value
 */
int parse_metadata(int argc, char *argv[], int start_index, 
                   FDFSMetaData *meta_list, int *meta_count)
{
    int i;
    char *pEqual;
    int key_len, value_len;
    
    *meta_count = 0;
    
    for (i = start_index; i < argc && *meta_count < MAX_METADATA_COUNT; i++) {
        /* Find the '=' separator */
        pEqual = strchr(argv[i], '=');
        if (pEqual == NULL) {
            fprintf(stderr, "ERROR: Invalid metadata format: '%s'\n", argv[i]);
            fprintf(stderr, "Expected format: key=value\n");
            return EINVAL;
        }
        
        key_len = pEqual - argv[i];
        value_len = strlen(pEqual + 1);
        
        /* Validate key length */
        if (key_len == 0) {
            fprintf(stderr, "ERROR: Empty metadata key in '%s'\n", argv[i]);
            return EINVAL;
        }
        if (key_len >= FDFS_MAX_META_NAME_LEN) {
            fprintf(stderr, "ERROR: Metadata key too long (max %d): '%.*s'\n",
                    FDFS_MAX_META_NAME_LEN - 1, key_len, argv[i]);
            return EINVAL;
        }
        
        /* Validate value length */
        if (value_len >= FDFS_MAX_META_VALUE_LEN) {
            fprintf(stderr, "ERROR: Metadata value too long (max %d): '%s'\n",
                    FDFS_MAX_META_VALUE_LEN - 1, pEqual + 1);
            return EINVAL;
        }
        
        /* Copy key and value to metadata structure */
        memcpy(meta_list[*meta_count].name, argv[i], key_len);
        meta_list[*meta_count].name[key_len] = '\0';
        strcpy(meta_list[*meta_count].value, pEqual + 1);
        
        (*meta_count)++;
    }
    
    if (*meta_count == 0) {
        fprintf(stderr, "ERROR: No metadata provided\n");
        fprintf(stderr, "Please provide at least one key=value pair\n");
        return EINVAL;
    }
    
    if (i < argc) {
        fprintf(stderr, "WARNING: Maximum metadata count (%d) reached\n", 
                MAX_METADATA_COUNT);
        fprintf(stderr, "Ignoring remaining %d items\n", argc - i);
    }
    
    return 0;
}

/**
 * Display metadata list
 */
void display_metadata(const FDFSMetaData *meta_list, int meta_count)
{
    int i;
    
    if (meta_count == 0) {
        printf("No metadata found\n");
        return;
    }
    
    printf("=== Metadata (%d items) ===\n", meta_count);
    for (i = 0; i < meta_count; i++) {
        printf("  [%2d] %-20s = %s\n", 
               i + 1, 
               meta_list[i].name, 
               meta_list[i].value);
    }
}

int main(int argc, char *argv[])
{
    char *conf_filename;
    char *operation;
    char *file_id;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    ConnectionInfo storageServer;
    int result;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
    FDFSMetaData meta_list[MAX_METADATA_COUNT];
    int meta_count = 0;
    FDFSMetaData *pMetaList = NULL;
    char op_flag;
    
    /* ========================================
     * STEP 1: Parse and validate arguments
     * ======================================== */
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    conf_filename = argv[1];
    operation = argv[2];
    file_id = argv[3];
    
    /* Validate operation */
    if (strcmp(operation, "set") == 0) {
        op_flag = STORAGE_SET_METADATA_FLAG_OVERWRITE;
        if (argc < 5) {
            fprintf(stderr, "ERROR: 'set' operation requires metadata\n");
            print_usage(argv[0]);
            return 1;
        }
    } else if (strcmp(operation, "merge") == 0) {
        op_flag = STORAGE_SET_METADATA_FLAG_MERGE;
        if (argc < 5) {
            fprintf(stderr, "ERROR: 'merge' operation requires metadata\n");
            print_usage(argv[0]);
            return 1;
        }
    } else if (strcmp(operation, "get") == 0) {
        /* Get operation doesn't need metadata arguments */
    } else {
        fprintf(stderr, "ERROR: Invalid operation '%s'\n", operation);
        fprintf(stderr, "Valid operations: set, merge, get\n");
        return 1;
    }
    
    printf("=== FastDFS Metadata Operations Example ===\n");
    printf("Config file: %s\n", conf_filename);
    printf("Operation: %s\n", operation);
    printf("File ID: %s\n\n", file_id);
    
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
     * STEP 3: Parse metadata (for set/merge)
     * ======================================== */
    if (strcmp(operation, "set") == 0 || strcmp(operation, "merge") == 0) {
        printf("Parsing metadata...\n");
        if ((result = parse_metadata(argc, argv, 4, meta_list, &meta_count)) != 0) {
            return result;
        }
        printf("✓ Parsed %d metadata items:\n", meta_count);
        display_metadata(meta_list, meta_count);
        printf("\n");
    }
    
    /* ========================================
     * STEP 4: Initialize client
     * ======================================== */
    log_init();
    
    printf("Initializing FastDFS client...\n");
    if ((result = fdfs_client_init(conf_filename)) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize client\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        return result;
    }
    printf("✓ Client initialized\n\n");
    
    /* ========================================
     * STEP 5: Connect to tracker
     * ======================================== */
    printf("Connecting to tracker server...\n");
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        result = errno != 0 ? errno : ECONNREFUSED;
        fprintf(stderr, "ERROR: Failed to connect to tracker\n");
        fprintf(stderr, "Error code: %d, Error info: %s\n", 
                result, STRERROR(result));
        fdfs_client_destroy();
        return result;
    }
    printf("✓ Connected to tracker: %s:%d\n\n", 
           pTrackerServer->ip_addr, pTrackerServer->port);
    
    /* ========================================
     * STEP 6: Query storage server
     * ======================================== */
    printf("Querying storage server...\n");
    
    /* For metadata operations, we need to query the storage server
     * that can update the file (not just read it)
     */
    if (strcmp(operation, "get") == 0) {
        /* For read operations, query fetch server */
        result = tracker_query_storage_fetch(pTrackerServer, 
                                             &storageServer, 
                                             group_name, 
                                             remote_filename);
    } else {
        /* For write operations (set/merge), query update server */
        result = tracker_query_storage_update(pTrackerServer, 
                                              &storageServer, 
                                              group_name, 
                                              remote_filename);
    }
    
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
    
    printf("✓ Storage server located: %s:%d\n\n", 
           storageServer.ip_addr, storageServer.port);
    
    /* ========================================
     * STEP 7: Connect to storage server
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
     * STEP 8: Perform metadata operation
     * ======================================== */
    if (strcmp(operation, "get") == 0) {
        /* ===== GET METADATA ===== */
        printf("Retrieving metadata...\n");
        
        result = storage_get_metadata(pTrackerServer,
                                      pStorageServer,
                                      group_name,
                                      remote_filename,
                                      &pMetaList,
                                      &meta_count);
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to get metadata\n");
            fprintf(stderr, "Error code: %d, Error info: %s\n", 
                    result, STRERROR(result));
        } else {
            printf("✓ Metadata retrieved successfully\n\n");
            display_metadata(pMetaList, meta_count);
            
            /* Free the metadata list allocated by storage_get_metadata */
            if (pMetaList != NULL) {
                free(pMetaList);
            }
        }
        
    } else {
        /* ===== SET or MERGE METADATA ===== */
        if (strcmp(operation, "set") == 0) {
            printf("Setting metadata (overwrite mode)...\n");
            printf("WARNING: This will delete all existing metadata\n\n");
        } else {
            printf("Merging metadata (update/add mode)...\n");
            printf("Existing metadata will be preserved\n\n");
        }
        
        result = storage_set_metadata(pTrackerServer,
                                      pStorageServer,
                                      group_name,
                                      remote_filename,
                                      meta_list,
                                      meta_count,
                                      op_flag);
        
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to set metadata\n");
            fprintf(stderr, "Error code: %d, Error info: %s\n", 
                    result, STRERROR(result));
            fprintf(stderr, "\nPossible causes:\n");
            fprintf(stderr, "  - File does not exist\n");
            fprintf(stderr, "  - Metadata too large\n");
            fprintf(stderr, "  - Storage server error\n");
        } else {
            printf("✓ Metadata %s successful!\n\n", 
                   strcmp(operation, "set") == 0 ? "set" : "merged");
            
            /* Verify by retrieving the metadata */
            printf("Verifying metadata...\n");
            result = storage_get_metadata(pTrackerServer,
                                         pStorageServer,
                                         group_name,
                                         remote_filename,
                                         &pMetaList,
                                         &meta_count);
            
            if (result == 0) {
                printf("✓ Verification successful\n\n");
                display_metadata(pMetaList, meta_count);
                if (pMetaList != NULL) {
                    free(pMetaList);
                }
            }
        }
    }
    
    /* ========================================
     * STEP 9: Cleanup
     * ======================================== */
    printf("\n=== Cleanup ===\n");
    tracker_close_connection_ex(pStorageServer, result != 0);
    printf("✓ Storage connection closed\n");
    
    tracker_close_connection_ex(pTrackerServer, result != 0);
    printf("✓ Tracker connection closed\n");
    
    fdfs_client_destroy();
    printf("✓ Client destroyed\n");
    
    if (result == 0) {
        printf("\n=== Operation Complete ===\n");
        
        /* Print helpful tips based on operation */
        if (strcmp(operation, "set") == 0) {
            printf("\nTip: Use 'merge' operation to add metadata without\n");
            printf("     deleting existing metadata.\n");
        } else if (strcmp(operation, "merge") == 0) {
            printf("\nTip: Use 'get' operation to view all metadata.\n");
        } else {
            printf("\nTip: Use 'set' or 'merge' to modify metadata.\n");
        }
    }
    
    return result;
}
