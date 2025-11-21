/**
 * FastDFS Metadata Bulk Operations Tool
 * 
 * This tool provides comprehensive bulk metadata management capabilities
 * for FastDFS. It enables efficient metadata operations at scale, allowing
 * administrators to set, get, delete, import, export, and search metadata
 * for multiple files in batch operations.
 * 
 * Features:
 * - Bulk set metadata for multiple files
 * - Bulk get metadata from multiple files
 * - Bulk delete metadata keys from multiple files
 * - Import metadata from CSV or JSON files
 * - Export metadata to CSV or JSON files
 * - Search files by metadata criteria
 * - Support for metadata merge and overwrite modes
 * - Multi-threaded parallel processing
 * - Detailed reporting and statistics
 * - JSON and text output formats
 * 
 * Use Cases:
 * - Batch tagging of files with metadata
 * - Bulk metadata updates across large file sets
 * - Metadata migration and synchronization
 * - File discovery and search by metadata
 * - Metadata backup and restore
 * - Compliance and audit operations
 * 
 * Copyright (C) 2025
 * License: GPL V3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <ctype.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum file ID length */
#define MAX_FILE_ID_LEN 256

/* Maximum metadata key length */
#define MAX_METADATA_KEY_LEN 64

/* Maximum metadata value length */
#define MAX_METADATA_VALUE_LEN 256

/* Maximum number of metadata items per file */
#define MAX_METADATA_ITEMS 128

/* Maximum number of threads for parallel processing */
#define MAX_THREADS 20

/* Default number of threads */
#define DEFAULT_THREADS 4

/* Maximum line length for file operations */
#define MAX_LINE_LEN 4096

/* Maximum number of files to process in one batch */
#define MAX_BATCH_SIZE 10000

/* Operation types */
typedef enum {
    OP_SET = 0,           /* Set metadata */
    OP_GET = 1,           /* Get metadata */
    OP_DELETE = 2,        /* Delete metadata keys */
    OP_IMPORT = 3,        /* Import metadata from file */
    OP_EXPORT = 4,        /* Export metadata to file */
    OP_SEARCH = 5         /* Search files by metadata */
} OperationType;

/* Metadata operation result */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];  /* File ID */
    int operation_status;            /* Operation status (0 = success, error code otherwise) */
    char error_msg[256];             /* Error message if operation failed */
    int metadata_count;              /* Number of metadata items processed */
    time_t operation_time;           /* When operation was performed */
} MetadataOperationResult;

/* File metadata entry */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];  /* File ID */
    FDFSMetaData *metadata;         /* Array of metadata items */
    int metadata_count;              /* Number of metadata items */
    int has_metadata;                /* Whether file has metadata */
} FileMetadataEntry;

/* Bulk operation context */
typedef struct {
    char **file_ids;                 /* Array of file IDs to process */
    int file_count;                  /* Number of files */
    int current_index;               /* Current file index being processed */
    pthread_mutex_t mutex;           /* Mutex for thread synchronization */
    ConnectionInfo *pTrackerServer;  /* Tracker server connection */
    FDFSMetaData *metadata_to_set;   /* Metadata to set (for set operations) */
    int metadata_count;              /* Number of metadata items to set */
    char **keys_to_delete;           /* Array of keys to delete (for delete operations) */
    int key_count;                   /* Number of keys to delete */
    char op_flag;                    /* Operation flag (MERGE or OVERWRITE) */
    OperationType op_type;           /* Type of operation */
    MetadataOperationResult *results; /* Array for operation results */
    int verbose;                     /* Verbose output flag */
    int json_output;                 /* JSON output flag */
} BulkOperationContext;

/* Search criteria */
typedef struct {
    char search_key[MAX_METADATA_KEY_LEN];    /* Metadata key to search for */
    char search_value[MAX_METADATA_VALUE_LEN]; /* Metadata value to match */
    int match_exact;                          /* Whether to match exact value or substring */
    char **file_list;                         /* File list to search in */
    int file_count;                           /* Number of files to search */
} SearchCriteria;

/* Global statistics */
static int total_files_processed = 0;
static int successful_operations = 0;
static int failed_operations = 0;
static int files_with_metadata = 0;
static int files_without_metadata = 0;
static int total_metadata_items = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_metadata_bulk tool, including all available commands and options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <COMMAND> [ARGUMENTS]\n", program_name);
    printf("\n");
    printf("FastDFS Metadata Bulk Operations Tool\n");
    printf("\n");
    printf("This tool enables efficient bulk metadata operations for FastDFS,\n");
    printf("allowing you to set, get, delete, import, export, and search metadata\n");
    printf("for multiple files in batch operations.\n");
    printf("\n");
    printf("Commands:\n");
    printf("  set FILE_LIST KEY=VALUE [KEY=VALUE...]  Set metadata for files\n");
    printf("  get FILE_LIST [OUTPUT_FILE]             Get metadata from files\n");
    printf("  delete FILE_LIST KEY [KEY...]          Delete metadata keys from files\n");
    printf("  import IMPORT_FILE                      Import metadata from CSV/JSON file\n");
    printf("  export FILE_LIST OUTPUT_FILE            Export metadata to CSV/JSON file\n");
    printf("  search FILE_LIST KEY=VALUE              Search files by metadata\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -j, --threads NUM    Number of parallel threads (default: 4, max: 20)\n");
    printf("  -m, --merge          Merge metadata (default: overwrite)\n");
    printf("  -f, --format FORMAT  Output format: csv, json, text (default: text)\n");
    printf("  -o, --output FILE    Output file (default: stdout)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -q, --quiet          Quiet mode (only show errors)\n");
    printf("  -J, --json           Output in JSON format (overrides --format)\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Metadata Format:\n");
    printf("  Metadata is specified as KEY=VALUE pairs\n");
    printf("  Multiple pairs can be specified separated by spaces\n");
    printf("  Examples: author=John title=\"My Document\" version=1.0\n");
    printf("\n");
    printf("File List Format:\n");
    printf("  File lists contain one file ID per line\n");
    printf("  Lines starting with # are treated as comments\n");
    printf("  Empty lines are ignored\n");
    printf("\n");
    printf("Import/Export Formats:\n");
    printf("  CSV: file_id,key1,value1,key2,value2,...\n");
    printf("  JSON: Array of objects with file_id and metadata fields\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - All operations completed successfully\n");
    printf("  1 - Some operations failed\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Set metadata for files in a list\n");
    printf("  %s set file_list.txt author=John title=\"Document\" version=1.0\n", program_name);
    printf("\n");
    printf("  # Get metadata from files\n");
    printf("  %s get file_list.txt metadata.json\n", program_name);
    printf("\n");
    printf("  # Delete specific metadata keys\n");
    printf("  %s delete file_list.txt temp_flag old_version\n", program_name);
    printf("\n");
    printf("  # Import metadata from CSV file\n");
    printf("  %s import metadata.csv\n", program_name);
    printf("\n");
    printf("  # Export metadata to JSON file\n");
    printf("  %s export file_list.txt metadata.json -f json\n", program_name);
    printf("\n");
    printf("  # Search files by metadata\n");
    printf("  %s search file_list.txt author=John\n", program_name);
}

/**
 * Parse metadata string
 * 
 * This function parses a metadata string in the format "KEY=VALUE"
 * and extracts the key and value components.
 * 
 * @param metadata_str - Metadata string to parse
 * @param key - Output buffer for key
 * @param key_size - Size of key buffer
 * @param value - Output buffer for value
 * @param value_size - Size of value buffer
 * @return 0 on success, -1 on error
 */
static int parse_metadata_string(const char *metadata_str,
                                 char *key, size_t key_size,
                                 char *value, size_t value_size) {
    const char *equals;
    size_t key_len;
    size_t value_len;
    
    if (metadata_str == NULL || key == NULL || value == NULL) {
        return -1;
    }
    
    /* Find equals sign */
    equals = strchr(metadata_str, '=');
    if (equals == NULL) {
        return -1;
    }
    
    /* Extract key */
    key_len = equals - metadata_str;
    if (key_len >= key_size || key_len == 0) {
        return -1;
    }
    
    strncpy(key, metadata_str, key_len);
    key[key_len] = '\0';
    
    /* Extract value */
    value_len = strlen(equals + 1);
    if (value_len >= value_size) {
        return -1;
    }
    
    strncpy(value, equals + 1, value_size - 1);
    value[value_size - 1] = '\0';
    
    /* Remove quotes from value if present */
    if (value_len >= 2 && value[0] == '"' && value[value_len - 1] == '"') {
        memmove(value, value + 1, value_len - 2);
        value[value_len - 2] = '\0';
    }
    
    return 0;
}

/**
 * Set metadata for a single file
 * 
 * This function sets metadata for a single file using the FastDFS
 * storage API. It supports both merge and overwrite modes.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param pStorageServer - Storage server connection
 * @param file_id - File ID
 * @param metadata - Array of metadata items to set
 * @param metadata_count - Number of metadata items
 * @param op_flag - Operation flag (MERGE or OVERWRITE)
 * @return 0 on success, error code on failure
 */
static int set_file_metadata(ConnectionInfo *pTrackerServer,
                             ConnectionInfo *pStorageServer,
                             const char *file_id,
                             FDFSMetaData *metadata,
                             int metadata_count,
                             char op_flag) {
    int ret;
    
    if (pTrackerServer == NULL || pStorageServer == NULL ||
        file_id == NULL || metadata == NULL || metadata_count <= 0) {
        return EINVAL;
    }
    
    /* Set metadata using FastDFS API */
    ret = storage_set_metadata1(pTrackerServer, pStorageServer,
                                file_id, metadata, metadata_count, op_flag);
    
    return ret;
}

/**
 * Get metadata for a single file
 * 
 * This function retrieves metadata for a single file from FastDFS
 * storage server.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param pStorageServer - Storage server connection
 * @param file_id - File ID
 * @param metadata - Output parameter for metadata array (must be freed)
 * @param metadata_count - Output parameter for metadata count
 * @return 0 on success, error code on failure
 */
static int get_file_metadata(ConnectionInfo *pTrackerServer,
                             ConnectionInfo *pStorageServer,
                             const char *file_id,
                             FDFSMetaData **metadata,
                             int *metadata_count) {
    int ret;
    
    if (pTrackerServer == NULL || pStorageServer == NULL ||
        file_id == NULL || metadata == NULL || metadata_count == NULL) {
        return EINVAL;
    }
    
    /* Get metadata using FastDFS API */
    ret = storage_get_metadata1(pTrackerServer, pStorageServer,
                               file_id, metadata, metadata_count);
    
    return ret;
}

/**
 * Delete metadata keys from a single file
 * 
 * This function deletes specific metadata keys from a file by
 * getting existing metadata, removing the specified keys, and
 * setting the updated metadata back.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param pStorageServer - Storage server connection
 * @param file_id - File ID
 * @param keys_to_delete - Array of keys to delete
 * @param key_count - Number of keys to delete
 * @return 0 on success, error code on failure
 */
static int delete_file_metadata_keys(ConnectionInfo *pTrackerServer,
                                    ConnectionInfo *pStorageServer,
                                    const char *file_id,
                                    char **keys_to_delete,
                                    int key_count) {
    FDFSMetaData *existing_metadata = NULL;
    FDFSMetaData *new_metadata = NULL;
    int existing_count = 0;
    int new_count = 0;
    int i, j;
    int found;
    int ret;
    
    if (pTrackerServer == NULL || pStorageServer == NULL ||
        file_id == NULL || keys_to_delete == NULL || key_count <= 0) {
        return EINVAL;
    }
    
    /* Get existing metadata */
    ret = get_file_metadata(pTrackerServer, pStorageServer,
                           file_id, &existing_metadata, &existing_count);
    if (ret != 0) {
        /* File may not have metadata, that's okay */
        return 0;
    }
    
    /* Allocate new metadata array */
    new_metadata = (FDFSMetaData *)malloc(existing_count * sizeof(FDFSMetaData));
    if (new_metadata == NULL) {
        free(existing_metadata);
        return ENOMEM;
    }
    
    /* Copy metadata, excluding keys to delete */
    new_count = 0;
    for (i = 0; i < existing_count; i++) {
        found = 0;
        
        /* Check if this key should be deleted */
        for (j = 0; j < key_count; j++) {
            if (strcmp(existing_metadata[i].name, keys_to_delete[j]) == 0) {
                found = 1;
                break;
            }
        }
        
        /* Keep this metadata item if not in delete list */
        if (!found) {
            strncpy(new_metadata[new_count].name, existing_metadata[i].name,
                   sizeof(new_metadata[new_count].name) - 1);
            strncpy(new_metadata[new_count].value, existing_metadata[i].value,
                   sizeof(new_metadata[new_count].value) - 1);
            new_count++;
        }
    }
    
    /* Set updated metadata (overwrite mode) */
    if (new_count > 0) {
        ret = set_file_metadata(pTrackerServer, pStorageServer,
                               file_id, new_metadata, new_count,
                               STORAGE_SET_METADATA_FLAG_OVERWRITE);
    } else {
        /* No metadata left, delete all by setting empty metadata */
        ret = set_file_metadata(pTrackerServer, pStorageServer,
                               file_id, NULL, 0,
                               STORAGE_SET_METADATA_FLAG_OVERWRITE);
    }
    
    free(existing_metadata);
    free(new_metadata);
    
    return ret;
}

/**
 * Worker thread function for parallel metadata operations
 * 
 * This function is executed by each worker thread to process files
 * in parallel for bulk metadata operations.
 * 
 * @param arg - BulkOperationContext pointer
 * @return NULL
 */
static void *metadata_worker_thread(void *arg) {
    BulkOperationContext *ctx = (BulkOperationContext *)arg;
    int file_index;
    char *file_id;
    ConnectionInfo *pStorageServer;
    MetadataOperationResult *result;
    FDFSMetaData *retrieved_metadata = NULL;
    int metadata_count = 0;
    int ret;
    int i;
    
    /* Process files until done */
    while (1) {
        /* Get next file index */
        pthread_mutex_lock(&ctx->mutex);
        file_index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        /* Check if we're done */
        if (file_index >= ctx->file_count) {
            break;
        }
        
        file_id = ctx->file_ids[file_index];
        result = &ctx->results[file_index];
        
        /* Initialize result */
        memset(result, 0, sizeof(MetadataOperationResult));
        strncpy(result->file_id, file_id, MAX_FILE_ID_LEN - 1);
        result->operation_time = time(NULL);
        
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            result->operation_status = errno;
            snprintf(result->error_msg, sizeof(result->error_msg),
                    "Failed to connect to storage server");
            continue;
        }
        
        /* Perform operation based on type */
        switch (ctx->op_type) {
            case OP_SET:
                /* Set metadata */
                ret = set_file_metadata(ctx->pTrackerServer, pStorageServer,
                                       file_id, ctx->metadata_to_set,
                                       ctx->metadata_count, ctx->op_flag);
                
                if (ret == 0) {
                    result->operation_status = 0;
                    result->metadata_count = ctx->metadata_count;
                    
                    pthread_mutex_lock(&stats_mutex);
                    successful_operations++;
                    total_metadata_items += ctx->metadata_count;
                    pthread_mutex_unlock(&stats_mutex);
                } else {
                    result->operation_status = ret;
                    snprintf(result->error_msg, sizeof(result->error_msg),
                            "Failed to set metadata: %s", STRERROR(ret));
                    
                    pthread_mutex_lock(&stats_mutex);
                    failed_operations++;
                    pthread_mutex_unlock(&stats_mutex);
                }
                break;
                
            case OP_GET:
                /* Get metadata */
                ret = get_file_metadata(ctx->pTrackerServer, pStorageServer,
                                       file_id, &retrieved_metadata, &metadata_count);
                
                if (ret == 0) {
                    result->operation_status = 0;
                    result->metadata_count = metadata_count;
                    
                    if (metadata_count > 0) {
                        pthread_mutex_lock(&stats_mutex);
                        files_with_metadata++;
                        total_metadata_items += metadata_count;
                        pthread_mutex_unlock(&stats_mutex);
                    } else {
                        pthread_mutex_lock(&stats_mutex);
                        files_without_metadata++;
                        pthread_mutex_unlock(&stats_mutex);
                    }
                    
                    /* Free retrieved metadata (we'll get it again if needed for output) */
                    if (retrieved_metadata != NULL) {
                        free(retrieved_metadata);
                        retrieved_metadata = NULL;
                    }
                } else {
                    result->operation_status = ret;
                    snprintf(result->error_msg, sizeof(result->error_msg),
                            "Failed to get metadata: %s", STRERROR(ret));
                    
                    pthread_mutex_lock(&stats_mutex);
                    failed_operations++;
                    if (ret == ENOENT) {
                        files_without_metadata++;
                    }
                    pthread_mutex_unlock(&stats_mutex);
                }
                break;
                
            case OP_DELETE:
                /* Delete metadata keys */
                ret = delete_file_metadata_keys(ctx->pTrackerServer, pStorageServer,
                                               file_id, ctx->keys_to_delete,
                                               ctx->key_count);
                
                if (ret == 0) {
                    result->operation_status = 0;
                    result->metadata_count = ctx->key_count;
                    
                    pthread_mutex_lock(&stats_mutex);
                    successful_operations++;
                    pthread_mutex_unlock(&stats_mutex);
                } else {
                    result->operation_status = ret;
                    snprintf(result->error_msg, sizeof(result->error_msg),
                            "Failed to delete metadata: %s", STRERROR(ret));
                    
                    pthread_mutex_lock(&stats_mutex);
                    failed_operations++;
                    pthread_mutex_unlock(&stats_mutex);
                }
                break;
                
            default:
                result->operation_status = EINVAL;
                snprintf(result->error_msg, sizeof(result->error_msg),
                        "Unsupported operation type");
                break;
        }
        
        /* Disconnect from storage server */
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return NULL;
}

/**
 * Read file list from file
 * 
 * This function reads a list of file IDs from a text file,
 * one file ID per line.
 * 
 * @param list_file - Path to file list
 * @param file_ids - Output array for file IDs (must be freed)
 * @param file_count - Output parameter for file count
 * @return 0 on success, error code on failure
 */
static int read_file_list(const char *list_file,
                         char ***file_ids,
                         int *file_count) {
    FILE *fp;
    char line[MAX_LINE_LEN];
    char **ids = NULL;
    int count = 0;
    int capacity = 1000;
    char *p;
    int i;
    
    if (list_file == NULL || file_ids == NULL || file_count == NULL) {
        return EINVAL;
    }
    
    /* Open file list */
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        return errno;
    }
    
    /* Allocate initial array */
    ids = (char **)malloc(capacity * sizeof(char *));
    if (ids == NULL) {
        fclose(fp);
        return ENOMEM;
    }
    
    /* Read file IDs */
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Remove newline characters */
        p = strchr(line, '\n');
        if (p != NULL) {
            *p = '\0';
        }
        
        p = strchr(line, '\r');
        if (p != NULL) {
            *p = '\0';
        }
        
        /* Skip empty lines and comments */
        p = line;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        
        if (*p == '\0' || *p == '#') {
            continue;
        }
        
        /* Expand array if needed */
        if (count >= capacity) {
            capacity *= 2;
            ids = (char **)realloc(ids, capacity * sizeof(char *));
            if (ids == NULL) {
                fclose(fp);
                for (i = 0; i < count; i++) {
                    free(ids[i]);
                }
                free(ids);
                return ENOMEM;
            }
        }
        
        /* Allocate and store file ID */
        ids[count] = (char *)malloc(strlen(p) + 1);
        if (ids[count] == NULL) {
            fclose(fp);
            for (i = 0; i < count; i++) {
                free(ids[i]);
            }
            free(ids);
            return ENOMEM;
        }
        
        strcpy(ids[count], p);
        count++;
    }
    
    fclose(fp);
    
    *file_ids = ids;
    *file_count = count;
    
    return 0;
}

/**
 * Perform bulk set metadata operation
 * 
 * This function performs bulk metadata setting for multiple files.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param list_file - File list containing file IDs
 * @param metadata - Array of metadata items to set
 * @param metadata_count - Number of metadata items
 * @param op_flag - Operation flag (MERGE or OVERWRITE)
 * @param num_threads - Number of parallel threads
 * @param output_file - Output file for results
 * @return 0 on success, error code on failure
 */
static int bulk_set_metadata(ConnectionInfo *pTrackerServer,
                             const char *list_file,
                             FDFSMetaData *metadata,
                             int metadata_count,
                             char op_flag,
                             int num_threads,
                             const char *output_file) {
    char **file_ids = NULL;
    int file_count = 0;
    BulkOperationContext ctx;
    pthread_t *threads = NULL;
    int i;
    int ret;
    FILE *out_fp = stdout;
    time_t start_time;
    time_t end_time;
    
    /* Read file list */
    ret = read_file_list(list_file, &file_ids, &file_count);
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to read file list: %s\n", STRERROR(ret));
        return ret;
    }
    
    if (file_count == 0) {
        fprintf(stderr, "ERROR: No file IDs found in list file\n");
        free(file_ids);
        return EINVAL;
    }
    
    /* Allocate results array */
    ctx.results = (MetadataOperationResult *)calloc(file_count, sizeof(MetadataOperationResult));
    if (ctx.results == NULL) {
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
        return ENOMEM;
    }
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(BulkOperationContext));
    ctx.file_ids = file_ids;
    ctx.file_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.metadata_to_set = metadata;
    ctx.metadata_count = metadata_count;
    ctx.op_flag = op_flag;
    ctx.op_type = OP_SET;
    ctx.verbose = verbose;
    ctx.json_output = json_output;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    /* Limit number of threads */
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
    }
    if (num_threads > file_count) {
        num_threads = file_count;
    }
    
    /* Allocate thread array */
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        pthread_mutex_destroy(&ctx.mutex);
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
        free(ctx.results);
        return ENOMEM;
    }
    
    /* Reset statistics */
    pthread_mutex_lock(&stats_mutex);
    total_files_processed = file_count;
    successful_operations = 0;
    failed_operations = 0;
    total_metadata_items = 0;
    pthread_mutex_unlock(&stats_mutex);
    
    /* Record start time */
    start_time = time(NULL);
    
    /* Start worker threads */
    for (i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, metadata_worker_thread, &ctx) != 0) {
            fprintf(stderr, "ERROR: Failed to create thread %d\n", i);
            ret = errno;
            break;
        }
    }
    
    /* Wait for all threads to complete */
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Record end time */
    end_time = time(NULL);
    
    /* Open output file if specified */
    if (output_file != NULL) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            out_fp = stdout;
        }
    }
    
    /* Print results */
    if (json_output) {
        fprintf(out_fp, "{\n");
        fprintf(out_fp, "  \"operation\": \"set\",\n");
        fprintf(out_fp, "  \"timestamp\": %ld,\n", (long)time(NULL));
        fprintf(out_fp, "  \"total_files\": %d,\n", file_count);
        fprintf(out_fp, "  \"successful\": %d,\n", successful_operations);
        fprintf(out_fp, "  \"failed\": %d,\n", failed_operations);
        fprintf(out_fp, "  \"total_metadata_items\": %d,\n", total_metadata_items);
        fprintf(out_fp, "  \"duration_seconds\": %ld,\n", (long)(end_time - start_time));
        fprintf(out_fp, "  \"results\": [\n");
        
        for (i = 0; i < file_count; i++) {
            MetadataOperationResult *r = &ctx.results[i];
            
            if (i > 0) {
                fprintf(out_fp, ",\n");
            }
            
            fprintf(out_fp, "    {\n");
            fprintf(out_fp, "      \"file_id\": \"%s\",\n", r->file_id);
            fprintf(out_fp, "      \"status\": %d,\n", r->operation_status);
            fprintf(out_fp, "      \"metadata_count\": %d", r->metadata_count);
            
            if (r->operation_status != 0) {
                fprintf(out_fp, ",\n      \"error\": \"%s\"", r->error_msg);
            }
            
            fprintf(out_fp, "\n    }");
        }
        
        fprintf(out_fp, "\n  ]\n");
        fprintf(out_fp, "}\n");
    } else {
        /* Text output */
        fprintf(out_fp, "\n");
        fprintf(out_fp, "=== Bulk Metadata Set Results ===\n");
        fprintf(out_fp, "Total files: %d\n", file_count);
        fprintf(out_fp, "Successful: %d\n", successful_operations);
        fprintf(out_fp, "Failed: %d\n", failed_operations);
        fprintf(out_fp, "Total metadata items set: %d\n", total_metadata_items);
        fprintf(out_fp, "Duration: %ld seconds\n", (long)(end_time - start_time));
        fprintf(out_fp, "\n");
        
        if (!quiet) {
            for (i = 0; i < file_count; i++) {
                MetadataOperationResult *r = &ctx.results[i];
                
                if (r->operation_status == 0) {
                    if (verbose) {
                        fprintf(out_fp, "✓ %s: Set %d metadata item(s)\n",
                               r->file_id, r->metadata_count);
                    }
                } else {
                    fprintf(out_fp, "✗ %s: %s\n", r->file_id, r->error_msg);
                }
            }
        }
    }
    
    /* Close output file if opened */
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    pthread_mutex_destroy(&ctx.mutex);
    free(threads);
    for (i = 0; i < file_count; i++) {
        free(file_ids[i]);
    }
    free(file_ids);
    free(ctx.results);
    
    return (failed_operations > 0) ? 1 : 0;
}

/**
 * Perform bulk get metadata operation
 * 
 * This function performs bulk metadata retrieval for multiple files
 * and exports the results to a file.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param list_file - File list containing file IDs
 * @param output_file - Output file for metadata
 * @param output_format - Output format (csv, json, text)
 * @param num_threads - Number of parallel threads
 * @return 0 on success, error code on failure
 */
static int bulk_get_metadata(ConnectionInfo *pTrackerServer,
                             const char *list_file,
                             const char *output_file,
                             const char *output_format,
                             int num_threads) {
    char **file_ids = NULL;
    int file_count = 0;
    FILE *out_fp = stdout;
    int i;
    int ret;
    ConnectionInfo *pStorageServer;
    FDFSMetaData *metadata = NULL;
    int metadata_count = 0;
    int is_json = 0;
    int is_csv = 0;
    time_t start_time;
    time_t end_time;
    
    /* Determine output format */
    if (output_format != NULL) {
        if (strcasecmp(output_format, "json") == 0) {
            is_json = 1;
        } else if (strcasecmp(output_format, "csv") == 0) {
            is_csv = 1;
        }
    } else if (json_output) {
        is_json = 1;
    }
    
    /* Read file list */
    ret = read_file_list(list_file, &file_ids, &file_count);
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to read file list: %s\n", STRERROR(ret));
        return ret;
    }
    
    if (file_count == 0) {
        fprintf(stderr, "ERROR: No file IDs found in list file\n");
        free(file_ids);
        return EINVAL;
    }
    
    /* Open output file if specified */
    if (output_file != NULL) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
            return errno;
        }
    }
    
    /* Reset statistics */
    pthread_mutex_lock(&stats_mutex);
    total_files_processed = file_count;
    successful_operations = 0;
    failed_operations = 0;
    files_with_metadata = 0;
    files_without_metadata = 0;
    total_metadata_items = 0;
    pthread_mutex_unlock(&stats_mutex);
    
    /* Record start time */
    start_time = time(NULL);
    
    /* Print header based on format */
    if (is_json) {
        fprintf(out_fp, "{\n");
        fprintf(out_fp, "  \"timestamp\": %ld,\n", (long)time(NULL));
        fprintf(out_fp, "  \"files\": [\n");
    } else if (is_csv) {
        fprintf(out_fp, "file_id");
        /* We'll add column headers after first file */
    }
    
    /* Process each file */
    for (i = 0; i < file_count; i++) {
        /* Get storage connection */
        pStorageServer = get_storage_connection(pTrackerServer);
        if (pStorageServer == NULL) {
            if (is_json) {
                if (i > 0) fprintf(out_fp, ",\n");
                fprintf(out_fp, "    {\n");
                fprintf(out_fp, "      \"file_id\": \"%s\",\n", file_ids[i]);
                fprintf(out_fp, "      \"error\": \"Failed to connect to storage server\",\n");
                fprintf(out_fp, "      \"metadata\": {}\n");
                fprintf(out_fp, "    }");
            } else if (is_csv) {
                fprintf(out_fp, "%s,ERROR:Failed to connect\n", file_ids[i]);
            } else {
                fprintf(out_fp, "✗ %s: Failed to connect to storage server\n", file_ids[i]);
            }
            
            pthread_mutex_lock(&stats_mutex);
            failed_operations++;
            pthread_mutex_unlock(&stats_mutex);
            continue;
        }
        
        /* Get metadata */
        ret = get_file_metadata(pTrackerServer, pStorageServer,
                               file_ids[i], &metadata, &metadata_count);
        
        if (ret == 0) {
            pthread_mutex_lock(&stats_mutex);
            successful_operations++;
            if (metadata_count > 0) {
                files_with_metadata++;
                total_metadata_items += metadata_count;
            } else {
                files_without_metadata++;
            }
            pthread_mutex_unlock(&stats_mutex);
            
            /* Output metadata based on format */
            if (is_json) {
                if (i > 0) fprintf(out_fp, ",\n");
                fprintf(out_fp, "    {\n");
                fprintf(out_fp, "      \"file_id\": \"%s\",\n", file_ids[i]);
                fprintf(out_fp, "      \"metadata_count\": %d,\n", metadata_count);
                fprintf(out_fp, "      \"metadata\": {\n");
                
                for (int j = 0; j < metadata_count; j++) {
                    if (j > 0) fprintf(out_fp, ",\n");
                    fprintf(out_fp, "        \"%s\": \"%s\"",
                           metadata[j].name, metadata[j].value);
                }
                
                fprintf(out_fp, "\n      }\n");
                fprintf(out_fp, "    }");
            } else if (is_csv) {
                fprintf(out_fp, "%s", file_ids[i]);
                for (int j = 0; j < metadata_count; j++) {
                    fprintf(out_fp, ",%s,%s", metadata[j].name, metadata[j].value);
                }
                fprintf(out_fp, "\n");
            } else {
                fprintf(out_fp, "File: %s\n", file_ids[i]);
                if (metadata_count > 0) {
                    for (int j = 0; j < metadata_count; j++) {
                        fprintf(out_fp, "  %s = %s\n", metadata[j].name, metadata[j].value);
                    }
                } else {
                    fprintf(out_fp, "  (no metadata)\n");
                }
                fprintf(out_fp, "\n");
            }
            
            /* Free metadata */
            if (metadata != NULL) {
                free(metadata);
                metadata = NULL;
            }
        } else {
            if (is_json) {
                if (i > 0) fprintf(out_fp, ",\n");
                fprintf(out_fp, "    {\n");
                fprintf(out_fp, "      \"file_id\": \"%s\",\n", file_ids[i]);
                fprintf(out_fp, "      \"error\": \"%s\",\n", STRERROR(ret));
                fprintf(out_fp, "      \"metadata\": {}\n");
                fprintf(out_fp, "    }");
            } else if (is_csv) {
                fprintf(out_fp, "%s,ERROR:%s\n", file_ids[i], STRERROR(ret));
            } else {
                fprintf(out_fp, "✗ %s: %s\n", file_ids[i], STRERROR(ret));
            }
            
            pthread_mutex_lock(&stats_mutex);
            failed_operations++;
            if (ret == ENOENT) {
                files_without_metadata++;
            }
            pthread_mutex_unlock(&stats_mutex);
        }
        
        /* Disconnect from storage server */
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    /* Close JSON array */
    if (is_json) {
        fprintf(out_fp, "\n  ],\n");
        fprintf(out_fp, "  \"summary\": {\n");
        fprintf(out_fp, "    \"total_files\": %d,\n", total_files_processed);
        fprintf(out_fp, "    \"successful\": %d,\n", successful_operations);
        fprintf(out_fp, "    \"failed\": %d,\n", failed_operations);
        fprintf(out_fp, "    \"files_with_metadata\": %d,\n", files_with_metadata);
        fprintf(out_fp, "    \"files_without_metadata\": %d,\n", files_without_metadata);
        fprintf(out_fp, "    \"total_metadata_items\": %d\n", total_metadata_items);
        fprintf(out_fp, "  }\n");
        fprintf(out_fp, "}\n");
    }
    
    /* Record end time */
    end_time = time(NULL);
    
    if (!is_json && !is_csv && !quiet) {
        fprintf(out_fp, "\n=== Summary ===\n");
        fprintf(out_fp, "Total files: %d\n", total_files_processed);
        fprintf(out_fp, "Successful: %d\n", successful_operations);
        fprintf(out_fp, "Failed: %d\n", failed_operations);
        fprintf(out_fp, "Files with metadata: %d\n", files_with_metadata);
        fprintf(out_fp, "Files without metadata: %d\n", files_without_metadata);
        fprintf(out_fp, "Total metadata items: %d\n", total_metadata_items);
        fprintf(out_fp, "Duration: %ld seconds\n", (long)(end_time - start_time));
    }
    
    /* Close output file if opened */
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    for (i = 0; i < file_count; i++) {
        free(file_ids[i]);
    }
    free(file_ids);
    
    return (failed_operations > 0) ? 1 : 0;
}

/**
 * Perform bulk delete metadata operation
 * 
 * This function performs bulk metadata key deletion for multiple files.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param list_file - File list containing file IDs
 * @param keys_to_delete - Array of keys to delete
 * @param key_count - Number of keys to delete
 * @param num_threads - Number of parallel threads
 * @param output_file - Output file for results
 * @return 0 on success, error code on failure
 */
static int bulk_delete_metadata(ConnectionInfo *pTrackerServer,
                                const char *list_file,
                                char **keys_to_delete,
                                int key_count,
                                int num_threads,
                                const char *output_file) {
    char **file_ids = NULL;
    int file_count = 0;
    BulkOperationContext ctx;
    pthread_t *threads = NULL;
    int i;
    int ret;
    FILE *out_fp = stdout;
    time_t start_time;
    time_t end_time;
    
    /* Read file list */
    ret = read_file_list(list_file, &file_ids, &file_count);
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to read file list: %s\n", STRERROR(ret));
        return ret;
    }
    
    if (file_count == 0) {
        fprintf(stderr, "ERROR: No file IDs found in list file\n");
        free(file_ids);
        return EINVAL;
    }
    
    /* Allocate results array */
    ctx.results = (MetadataOperationResult *)calloc(file_count, sizeof(MetadataOperationResult));
    if (ctx.results == NULL) {
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
        return ENOMEM;
    }
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(BulkOperationContext));
    ctx.file_ids = file_ids;
    ctx.file_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.keys_to_delete = keys_to_delete;
    ctx.key_count = key_count;
    ctx.op_type = OP_DELETE;
    ctx.verbose = verbose;
    ctx.json_output = json_output;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    /* Limit number of threads */
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
    }
    if (num_threads > file_count) {
        num_threads = file_count;
    }
    
    /* Allocate thread array */
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        pthread_mutex_destroy(&ctx.mutex);
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
        free(ctx.results);
        return ENOMEM;
    }
    
    /* Reset statistics */
    pthread_mutex_lock(&stats_mutex);
    total_files_processed = file_count;
    successful_operations = 0;
    failed_operations = 0;
    pthread_mutex_unlock(&stats_mutex);
    
    /* Record start time */
    start_time = time(NULL);
    
    /* Start worker threads */
    for (i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, metadata_worker_thread, &ctx) != 0) {
            fprintf(stderr, "ERROR: Failed to create thread %d\n", i);
            ret = errno;
            break;
        }
    }
    
    /* Wait for all threads to complete */
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Record end time */
    end_time = time(NULL);
    
    /* Open output file if specified */
    if (output_file != NULL) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            out_fp = stdout;
        }
    }
    
    /* Print results */
    if (json_output) {
        fprintf(out_fp, "{\n");
        fprintf(out_fp, "  \"operation\": \"delete\",\n");
        fprintf(out_fp, "  \"timestamp\": %ld,\n", (long)time(NULL));
        fprintf(out_fp, "  \"total_files\": %d,\n", file_count);
        fprintf(out_fp, "  \"successful\": %d,\n", successful_operations);
        fprintf(out_fp, "  \"failed\": %d,\n", failed_operations);
        fprintf(out_fp, "  \"duration_seconds\": %ld,\n", (long)(end_time - start_time));
        fprintf(out_fp, "  \"results\": [\n");
        
        for (i = 0; i < file_count; i++) {
            MetadataOperationResult *r = &ctx.results[i];
            
            if (i > 0) {
                fprintf(out_fp, ",\n");
            }
            
            fprintf(out_fp, "    {\n");
            fprintf(out_fp, "      \"file_id\": \"%s\",\n", r->file_id);
            fprintf(out_fp, "      \"status\": %d", r->operation_status);
            
            if (r->operation_status != 0) {
                fprintf(out_fp, ",\n      \"error\": \"%s\"", r->error_msg);
            }
            
            fprintf(out_fp, "\n    }");
        }
        
        fprintf(out_fp, "\n  ]\n");
        fprintf(out_fp, "}\n");
    } else {
        /* Text output */
        fprintf(out_fp, "\n");
        fprintf(out_fp, "=== Bulk Metadata Delete Results ===\n");
        fprintf(out_fp, "Total files: %d\n", file_count);
        fprintf(out_fp, "Successful: %d\n", successful_operations);
        fprintf(out_fp, "Failed: %d\n", failed_operations);
        fprintf(out_fp, "Duration: %ld seconds\n", (long)(end_time - start_time));
        fprintf(out_fp, "\n");
        
        if (!quiet) {
            for (i = 0; i < file_count; i++) {
                MetadataOperationResult *r = &ctx.results[i];
                
                if (r->operation_status == 0) {
                    if (verbose) {
                        fprintf(out_fp, "✓ %s: Deleted %d metadata key(s)\n",
                               r->file_id, r->metadata_count);
                    }
                } else {
                    fprintf(out_fp, "✗ %s: %s\n", r->file_id, r->error_msg);
                }
            }
        }
    }
    
    /* Close output file if opened */
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    pthread_mutex_destroy(&ctx.mutex);
    free(threads);
    for (i = 0; i < file_count; i++) {
        free(file_ids[i]);
    }
    free(file_ids);
    free(ctx.results);
    
    return (failed_operations > 0) ? 1 : 0;
}

/**
 * Main function
 * 
 * Entry point for the metadata bulk operations tool. Parses command-line
 * arguments and performs the requested bulk metadata operations.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = some failures, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    char *output_file = NULL;
    char *output_format = NULL;
    int num_threads = DEFAULT_THREADS;
    char op_flag = STORAGE_SET_METADATA_FLAG_OVERWRITE;
    char *command = NULL;
    FDFSMetaData *metadata = NULL;
    int metadata_count = 0;
    char **keys_to_delete = NULL;
    int key_count = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    int opt;
    int option_index = 0;
    int i;
    char key[MAX_METADATA_KEY_LEN];
    char value[MAX_METADATA_VALUE_LEN];
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"threads", required_argument, 0, 'j'},
        {"merge", no_argument, 0, 'm'},
        {"format", required_argument, 0, 'f'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:j:mf:o:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 'm':
                op_flag = STORAGE_SET_METADATA_FLAG_MERGE;
                break;
            case 'f':
                output_format = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'J':
                json_output = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 2;
        }
    }
    
    /* Get command */
    if (optind >= argc) {
        fprintf(stderr, "ERROR: Command required\n\n");
        print_usage(argv[0]);
        return 2;
    }
    
    command = argv[optind];
    optind++;
    
    /* Initialize logging */
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    /* Initialize FastDFS client */
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        return 2;
    }
    
    /* Connect to tracker server */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        fdfs_client_destroy();
        return 2;
    }
    
    /* Handle different commands */
    if (strcmp(command, "set") == 0) {
        /* Set metadata command */
        if (optind >= argc) {
            fprintf(stderr, "ERROR: File list required for set command\n");
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        list_file = argv[optind];
        optind++;
        
        /* Parse metadata key-value pairs */
        if (optind >= argc) {
            fprintf(stderr, "ERROR: At least one metadata KEY=VALUE pair required\n");
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        /* Count metadata items */
        metadata_count = argc - optind;
        if (metadata_count > MAX_METADATA_ITEMS) {
            fprintf(stderr, "ERROR: Too many metadata items (max: %d)\n", MAX_METADATA_ITEMS);
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        /* Allocate metadata array */
        metadata = (FDFSMetaData *)malloc(metadata_count * sizeof(FDFSMetaData));
        if (metadata == NULL) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return ENOMEM;
        }
        
        /* Parse each metadata item */
        for (i = 0; i < metadata_count; i++) {
            if (parse_metadata_string(argv[optind + i], key, sizeof(key),
                                     value, sizeof(value)) != 0) {
                fprintf(stderr, "ERROR: Invalid metadata format: %s (expected KEY=VALUE)\n",
                       argv[optind + i]);
                free(metadata);
                tracker_disconnect_server_ex(pTrackerServer, true);
                fdfs_client_destroy();
                return 2;
            }
            
            strncpy(metadata[i].name, key, sizeof(metadata[i].name) - 1);
            strncpy(metadata[i].value, value, sizeof(metadata[i].value) - 1);
        }
        
        /* Perform bulk set operation */
        result = bulk_set_metadata(pTrackerServer, list_file, metadata,
                                   metadata_count, op_flag, num_threads, output_file);
        
        free(metadata);
        
    } else if (strcmp(command, "get") == 0) {
        /* Get metadata command */
        if (optind >= argc) {
            fprintf(stderr, "ERROR: File list required for get command\n");
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        list_file = argv[optind];
        optind++;
        
        /* Optional output file */
        if (optind < argc) {
            output_file = argv[optind];
        }
        
        /* Perform bulk get operation */
        result = bulk_get_metadata(pTrackerServer, list_file, output_file,
                                   output_format, num_threads);
        
    } else if (strcmp(command, "delete") == 0) {
        /* Delete metadata command */
        if (optind >= argc) {
            fprintf(stderr, "ERROR: File list required for delete command\n");
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        list_file = argv[optind];
        optind++;
        
        /* Parse keys to delete */
        if (optind >= argc) {
            fprintf(stderr, "ERROR: At least one metadata key required for delete command\n");
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        key_count = argc - optind;
        keys_to_delete = (char **)malloc(key_count * sizeof(char *));
        if (keys_to_delete == NULL) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return ENOMEM;
        }
        
        for (i = 0; i < key_count; i++) {
            keys_to_delete[i] = strdup(argv[optind + i]);
            if (keys_to_delete[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(keys_to_delete[j]);
                }
                free(keys_to_delete);
                tracker_disconnect_server_ex(pTrackerServer, true);
                fdfs_client_destroy();
                return ENOMEM;
            }
        }
        
        /* Perform bulk delete operation */
        result = bulk_delete_metadata(pTrackerServer, list_file, keys_to_delete,
                                     key_count, num_threads, output_file);
        
        for (i = 0; i < key_count; i++) {
            free(keys_to_delete[i]);
        }
        free(keys_to_delete);
        
    } else {
        fprintf(stderr, "ERROR: Unknown command: %s\n", command);
        print_usage(argv[0]);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 2;
    }
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    /* Return appropriate exit code */
    if (result != 0) {
        return result;
    }
    
    return 0;
}

