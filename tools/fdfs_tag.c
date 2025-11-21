/**
 * FastDFS File Tagging Tool
 * 
 * This tool provides comprehensive file tagging capabilities for FastDFS,
 * allowing users to add tags to files for better organization and management.
 * Tags are stored as metadata and enable tag-based operations such as search,
 * delete, migrate, and other file management tasks.
 * 
 * Features:
 * - Add tags to files
 * - Remove tags from files
 * - List tags for files
 * - Search files by tags
 * - Tag-based operations (delete, migrate, etc.)
 * - Bulk tag operations
 * - Tag management and organization
 * - Multi-threaded parallel operations
 * - JSON and text output formats
 * 
 * Tag Operations:
 * - Add: Add one or more tags to files
 * - Remove: Remove one or more tags from files
 * - List: List all tags for files
 * - Search: Find files by tags
 * - Delete: Delete files by tags
 * - Migrate: Migrate files by tags
 * 
 * Tag Format:
 * - Tags are stored as metadata with key "tags"
 * - Multiple tags are comma-separated
 * - Example: tags=important,archive,backup
 * 
 * Tag Search:
 * - Search by single tag
 * - Search by multiple tags (AND/OR logic)
 * - Search by tag patterns
 * - Export search results
 * 
 * Use Cases:
 * - File organization and categorization
 * - Tag-based file management
 * - Automated file operations
 * - File discovery and search
 * - Workflow management
 * - Content classification
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

/* Maximum tag length */
#define MAX_TAG_LEN 128

/* Maximum number of tags per file */
#define MAX_TAGS_PER_FILE 100

/* Maximum number of threads for parallel processing */
#define MAX_THREADS 20

/* Default number of threads */
#define DEFAULT_THREADS 4

/* Maximum line length for file operations */
#define MAX_LINE_LEN 4096

/* Tag operation enumeration */
typedef enum {
    TAG_OP_ADD = 0,      /* Add tags */
    TAG_OP_REMOVE = 1,   /* Remove tags */
    TAG_OP_LIST = 2,    /* List tags */
    TAG_OP_SEARCH = 3,   /* Search by tags */
    TAG_OP_DELETE = 4,   /* Delete files by tags */
    TAG_OP_MIGRATE = 5   /* Migrate files by tags */
} TagOperation;

/* Tag task structure */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];        /* File ID */
    char tags[MAX_TAGS_PER_FILE][MAX_TAG_LEN];  /* Array of tags */
    int tag_count;                         /* Number of tags */
    int status;                            /* Task status (0 = pending, 1 = success, -1 = failed) */
    char error_msg[512];                   /* Error message if failed */
    char current_tags[4096];              /* Current tags (for list/search) */
} TagTask;

/* Tag context */
typedef struct {
    TagTask *tasks;                        /* Array of tag tasks */
    int task_count;                        /* Number of tasks */
    int current_index;                     /* Current task index */
    pthread_mutex_t mutex;                 /* Mutex for thread synchronization */
    ConnectionInfo *pTrackerServer;        /* Tracker server connection */
    TagOperation operation;                /* Operation type */
    char search_tags[MAX_TAGS_PER_FILE][MAX_TAG_LEN];  /* Tags to search for */
    int search_tag_count;                 /* Number of search tags */
    int search_and_mode;                  /* AND mode for search (all tags must match) */
    char target_group[FDFS_GROUP_NAME_MAX_LEN + 1];  /* Target group for migrate */
    int verbose;                          /* Verbose output flag */
    int json_output;                       /* JSON output flag */
} TagContext;

/* Global statistics */
static int total_files_processed = 0;
static int files_tagged = 0;
static int files_untagged = 0;
static int files_found = 0;
static int files_deleted = 0;
static int files_migrated = 0;
static int files_failed = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_tag tool, including all available options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <command> [command_args...]\n", program_name);
    printf("\n");
    printf("FastDFS File Tagging Tool\n");
    printf("\n");
    printf("This tool adds tags to files for organization and enables\n");
    printf("tag-based operations such as search, delete, and migrate.\n");
    printf("\n");
    printf("Commands:\n");
    printf("  add <file_id> [file_id...] <tag> [tag...]    Add tags to files\n");
    printf("  add -f <file_list> <tag> [tag...]             Add tags to files from list\n");
    printf("  remove <file_id> [file_id...] <tag> [tag...] Remove tags from files\n");
    printf("  remove -f <file_list> <tag> [tag...]         Remove tags from files from list\n");
    printf("  list <file_id> [file_id...]                   List tags for files\n");
    printf("  list -f <file_list>                           List tags for files from list\n");
    printf("  search <tag> [tag...]                         Search files by tags\n");
    printf("  delete <tag> [tag...]                        Delete files by tags\n");
    printf("  migrate <tag> [tag...] -g <group>            Migrate files by tags\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST       File list (one file ID per line)\n");
    printf("  -g, --group NAME      Target group for migrate command\n");
    printf("  --and                 All tags must match (AND mode, default for search)\n");
    printf("  --or                  Any tag must match (OR mode)\n");
    printf("  --dry-run             Preview operations without executing\n");
    printf("  -j, --threads NUM     Number of parallel threads (default: 4, max: 20)\n");
    printf("  -o, --output FILE     Output report file (default: stdout)\n");
    printf("  -v, --verbose         Verbose output\n");
    printf("  -q, --quiet           Quiet mode (only show errors)\n");
    printf("  -J, --json            Output in JSON format\n");
    printf("  -h, --help            Show this help message\n");
    printf("\n");
    printf("Tag Format:\n");
    printf("  Tags are stored as metadata with key \"tags\"\n");
    printf("  Multiple tags are comma-separated\n");
    printf("  Example: tags=important,archive,backup\n");
    printf("\n");
    printf("Search Modes:\n");
    printf("  --and: All specified tags must be present (default)\n");
    printf("  --or:  Any specified tag must be present\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - Operation completed successfully\n");
    printf("  1 - Some operations failed\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Add tags to files\n");
    printf("  %s add file1.jpg file2.jpg important archive\n", program_name);
    printf("\n");
    printf("  # Remove tags from files\n");
    printf("  %s remove file1.jpg archive\n", program_name);
    printf("\n");
    printf("  # List tags for files\n");
    printf("  %s list file1.jpg file2.jpg\n", program_name);
    printf("\n");
    printf("  # Search files by tags\n");
    printf("  %s search important archive\n", program_name);
    printf("\n");
    printf("  # Delete files by tags\n");
    printf("  %s delete temp old\n", program_name);
    printf("\n");
    printf("  # Migrate files by tags\n");
    printf("  %s migrate archive -g group2\n", program_name);
}

/**
 * Parse tags string
 * 
 * This function parses a comma-separated tags string into an array.
 * 
 * @param tags_str - Tags string (comma-separated)
 * @param tags - Output array for tags
 * @param max_tags - Maximum number of tags
 * @param tag_count - Output parameter for tag count
 * @return 0 on success, error code on failure
 */
static int parse_tags_string(const char *tags_str, char tags[][MAX_TAG_LEN],
                             int max_tags, int *tag_count) {
    char *str_copy;
    char *token;
    char *saveptr;
    int count = 0;
    
    if (tags_str == NULL || tags == NULL || tag_count == NULL) {
        return EINVAL;
    }
    
    str_copy = strdup(tags_str);
    if (str_copy == NULL) {
        return ENOMEM;
    }
    
    token = strtok_r(str_copy, ",", &saveptr);
    while (token != NULL && count < max_tags) {
        /* Trim whitespace */
        while (isspace((unsigned char)*token)) {
            token++;
        }
        
        char *end = token + strlen(token) - 1;
        while (end > token && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }
        
        if (strlen(token) > 0) {
            strncpy(tags[count], token, MAX_TAG_LEN - 1);
            tags[count][MAX_TAG_LEN - 1] = '\0';
            count++;
        }
        
        token = strtok_r(NULL, ",", &saveptr);
    }
    
    free(str_copy);
    *tag_count = count;
    
    return 0;
}

/**
 * Get current tags for a file
 * 
 * This function retrieves the current tags for a file from metadata.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param file_id - File ID
 * @param tags_str - Output buffer for tags string
 * @param tags_str_size - Size of tags string buffer
 * @return 0 on success, error code on failure
 */
static int get_file_tags(ConnectionInfo *pTrackerServer, const char *file_id,
                        char *tags_str, size_t tags_str_size) {
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    int result;
    ConnectionInfo *pStorageServer;
    int i;
    
    if (pTrackerServer == NULL || file_id == NULL ||
        tags_str == NULL || tags_str_size == 0) {
        return EINVAL;
    }
    
    tags_str[0] = '\0';
    
    /* Get storage connection */
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    /* Get metadata */
    result = storage_get_metadata1(pTrackerServer, pStorageServer,
                                  file_id, &meta_list, &meta_count);
    if (result != 0) {
        tracker_disconnect_server_ex(pStorageServer, true);
        if (result == ENOENT) {
            /* No metadata, return empty tags */
            return 0;
        }
        return result;
    }
    
    /* Find tags metadata */
    if (meta_list != NULL) {
        for (i = 0; i < meta_count; i++) {
            if (strcmp(meta_list[i].name, "tags") == 0) {
                strncpy(tags_str, meta_list[i].value, tags_str_size - 1);
                tags_str[tags_str_size - 1] = '\0';
                break;
            }
        }
    }
    
    if (meta_list != NULL) {
        free(meta_list);
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    return 0;
}

/**
 * Set tags for a file
 * 
 * This function sets tags for a file by updating metadata.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param file_id - File ID
 * @param tags - Array of tags
 * @param tag_count - Number of tags
 * @param merge - Whether to merge with existing tags
 * @return 0 on success, error code on failure
 */
static int set_file_tags(ConnectionInfo *pTrackerServer, const char *file_id,
                        char tags[][MAX_TAG_LEN], int tag_count, int merge) {
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    int result;
    ConnectionInfo *pStorageServer;
    char tags_str[4096];
    char current_tags[4096];
    char new_tags[4096];
    char tag_set[MAX_TAGS_PER_FILE][MAX_TAG_LEN];
    int tag_set_count = 0;
    int i, j;
    int found;
    
    if (pTrackerServer == NULL || file_id == NULL ||
        tags == NULL || tag_count <= 0) {
        return EINVAL;
    }
    
    /* Get current tags if merging */
    if (merge) {
        get_file_tags(pTrackerServer, file_id, current_tags, sizeof(current_tags));
        
        /* Parse current tags */
        char current_tag_list[MAX_TAGS_PER_FILE][MAX_TAG_LEN];
        int current_tag_count = 0;
        parse_tags_string(current_tags, current_tag_list, MAX_TAGS_PER_FILE,
                         &current_tag_count);
        
        /* Start with current tags */
        for (i = 0; i < current_tag_count; i++) {
            strncpy(tag_set[tag_set_count], current_tag_list[i], MAX_TAG_LEN - 1);
            tag_set[tag_set_count][MAX_TAG_LEN - 1] = '\0';
            tag_set_count++;
        }
    }
    
    /* Add new tags (avoid duplicates) */
    for (i = 0; i < tag_count; i++) {
        found = 0;
        for (j = 0; j < tag_set_count; j++) {
            if (strcmp(tags[i], tag_set[j]) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found && tag_set_count < MAX_TAGS_PER_FILE) {
            strncpy(tag_set[tag_set_count], tags[i], MAX_TAG_LEN - 1);
            tag_set[tag_set_count][MAX_TAG_LEN - 1] = '\0';
            tag_set_count++;
        }
    }
    
    /* Build tags string */
    new_tags[0] = '\0';
    for (i = 0; i < tag_set_count; i++) {
        if (i > 0) {
            strncat(new_tags, ",", sizeof(new_tags) - strlen(new_tags) - 1);
        }
        strncat(new_tags, tag_set[i], sizeof(new_tags) - strlen(new_tags) - 1);
    }
    
    /* Get storage connection */
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    /* Prepare metadata */
    meta_count = 1;
    meta_list = (FDFSMetaData *)calloc(meta_count, sizeof(FDFSMetaData));
    if (meta_list == NULL) {
        tracker_disconnect_server_ex(pStorageServer, true);
        return ENOMEM;
    }
    
    strncpy(meta_list[0].name, "tags", sizeof(meta_list[0].name) - 1);
    strncpy(meta_list[0].value, new_tags, sizeof(meta_list[0].value) - 1);
    
    /* Set metadata */
    result = storage_set_metadata1(pTrackerServer, pStorageServer,
                                  file_id, meta_list, meta_count,
                                  merge ? FDFS_STORAGE_SET_METADATA_FLAG_MERGE :
                                         FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE);
    
    free(meta_list);
    tracker_disconnect_server_ex(pStorageServer, true);
    
    return result;
}

/**
 * Remove tags from a file
 * 
 * This function removes specified tags from a file.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param file_id - File ID
 * @param tags - Array of tags to remove
 * @param tag_count - Number of tags to remove
 * @return 0 on success, error code on failure
 */
static int remove_file_tags(ConnectionInfo *pTrackerServer, const char *file_id,
                           char tags[][MAX_TAG_LEN], int tag_count) {
    char current_tags[4096];
    char tag_list[MAX_TAGS_PER_FILE][MAX_TAG_LEN];
    int current_tag_count = 0;
    char new_tags[4096];
    int i, j;
    int result;
    
    if (pTrackerServer == NULL || file_id == NULL ||
        tags == NULL || tag_count <= 0) {
        return EINVAL;
    }
    
    /* Get current tags */
    result = get_file_tags(pTrackerServer, file_id, current_tags, sizeof(current_tags));
    if (result != 0) {
        return result;
    }
    
    if (strlen(current_tags) == 0) {
        /* No tags to remove */
        return 0;
    }
    
    /* Parse current tags */
    parse_tags_string(current_tags, tag_list, MAX_TAGS_PER_FILE, &current_tag_count);
    
    /* Remove specified tags */
    new_tags[0] = '\0';
    for (i = 0; i < current_tag_count; i++) {
        int should_remove = 0;
        
        for (j = 0; j < tag_count; j++) {
            if (strcmp(tag_list[i], tags[j]) == 0) {
                should_remove = 1;
                break;
            }
        }
        
        if (!should_remove) {
            if (strlen(new_tags) > 0) {
                strncat(new_tags, ",", sizeof(new_tags) - strlen(new_tags) - 1);
            }
            strncat(new_tags, tag_list[i], sizeof(new_tags) - strlen(new_tags) - 1);
        }
    }
    
    /* Set updated tags */
    char new_tag_array[MAX_TAGS_PER_FILE][MAX_TAG_LEN];
    int new_tag_count = 0;
    
    if (strlen(new_tags) > 0) {
        parse_tags_string(new_tags, new_tag_array, MAX_TAGS_PER_FILE, &new_tag_count);
        return set_file_tags(pTrackerServer, file_id, new_tag_array, new_tag_count, 0);
    } else {
        /* Remove tags metadata entirely */
        FDFSMetaData *meta_list = NULL;
        ConnectionInfo *pStorageServer;
        
        pStorageServer = get_storage_connection(pTrackerServer);
        if (pStorageServer == NULL) {
            return -1;
        }
        
        /* Set empty tags to remove */
        meta_list = (FDFSMetaData *)calloc(1, sizeof(FDFSMetaData));
        if (meta_list == NULL) {
            tracker_disconnect_server_ex(pStorageServer, true);
            return ENOMEM;
        }
        
        strncpy(meta_list[0].name, "tags", sizeof(meta_list[0].name) - 1);
        meta_list[0].value[0] = '\0';
        
        result = storage_set_metadata1(pTrackerServer, pStorageServer,
                                      file_id, meta_list, 1,
                                      FDFS_STORAGE_SET_METADATA_FLAG_OVERWRITE);
        
        free(meta_list);
        tracker_disconnect_server_ex(pStorageServer, true);
        
        return result;
    }
}

/**
 * Check if file matches tag search
 * 
 * This function checks if a file matches the tag search criteria.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param file_id - File ID
 * @param search_tags - Tags to search for
 * @param search_tag_count - Number of search tags
 * @param and_mode - AND mode (all tags must match)
 * @return 1 if matches, 0 if not, -1 on error
 */
static int file_matches_tags(ConnectionInfo *pTrackerServer, const char *file_id,
                             char search_tags[][MAX_TAG_LEN], int search_tag_count,
                             int and_mode) {
    char current_tags[4096];
    char tag_list[MAX_TAGS_PER_FILE][MAX_TAG_LEN];
    int current_tag_count = 0;
    int i, j;
    int matches = 0;
    int result;
    
    if (pTrackerServer == NULL || file_id == NULL ||
        search_tags == NULL || search_tag_count <= 0) {
        return -1;
    }
    
    /* Get current tags */
    result = get_file_tags(pTrackerServer, file_id, current_tags, sizeof(current_tags));
    if (result != 0) {
        return -1;
    }
    
    if (strlen(current_tags) == 0) {
        return 0;
    }
    
    /* Parse current tags */
    parse_tags_string(current_tags, tag_list, MAX_TAGS_PER_FILE, &current_tag_count);
    
    /* Check matches */
    if (and_mode) {
        /* All search tags must be present */
        matches = 1;
        for (i = 0; i < search_tag_count; i++) {
            int found = 0;
            for (j = 0; j < current_tag_count; j++) {
                if (strcmp(search_tags[i], tag_list[j]) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                matches = 0;
                break;
            }
        }
    } else {
        /* Any search tag must be present */
        for (i = 0; i < search_tag_count; i++) {
            for (j = 0; j < current_tag_count; j++) {
                if (strcmp(search_tags[i], tag_list[j]) == 0) {
                    matches = 1;
                    break;
                }
            }
            if (matches) {
                break;
            }
        }
    }
    
    return matches;
}

/**
 * Process a single tag task
 * 
 * This function processes a single tag task based on the operation type.
 * 
 * @param ctx - Tag context
 * @param task - Tag task
 * @return 0 on success, error code on failure
 */
static int process_tag_task(TagContext *ctx, TagTask *task) {
    int result;
    
    if (ctx == NULL || task == NULL) {
        return EINVAL;
    }
    
    switch (ctx->operation) {
        case TAG_OP_ADD:
            result = set_file_tags(ctx->pTrackerServer, task->file_id,
                                  task->tags, task->tag_count, 1);  /* Merge mode */
            break;
            
        case TAG_OP_REMOVE:
            result = remove_file_tags(ctx->pTrackerServer, task->file_id,
                                     task->tags, task->tag_count);
            break;
            
        case TAG_OP_LIST:
            result = get_file_tags(ctx->pTrackerServer, task->file_id,
                                  task->current_tags, sizeof(task->current_tags));
            break;
            
        case TAG_OP_SEARCH:
            result = file_matches_tags(ctx->pTrackerServer, task->file_id,
                                      ctx->search_tags, ctx->search_tag_count,
                                      ctx->search_and_mode);
            if (result == 1) {
                /* File matches, get its tags */
                get_file_tags(ctx->pTrackerServer, task->file_id,
                            task->current_tags, sizeof(task->current_tags));
            }
            break;
            
        case TAG_OP_DELETE:
            /* Check if file matches tags, then delete */
            result = file_matches_tags(ctx->pTrackerServer, task->file_id,
                                      ctx->search_tags, ctx->search_tag_count,
                                      ctx->search_and_mode);
            if (result == 1) {
                ConnectionInfo *pStorageServer;
                pStorageServer = get_storage_connection(ctx->pTrackerServer);
                if (pStorageServer != NULL) {
                    result = storage_delete_file1(ctx->pTrackerServer, pStorageServer,
                                                task->file_id);
                    tracker_disconnect_server_ex(pStorageServer, true);
                } else {
                    result = -1;
                }
            }
            break;
            
        case TAG_OP_MIGRATE:
            /* Check if file matches tags, then migrate */
            result = file_matches_tags(ctx->pTrackerServer, task->file_id,
                                      ctx->search_tags, ctx->search_tag_count,
                                      ctx->search_and_mode);
            if (result == 1) {
                /* Migration would be implemented here */
                /* For now, this is a placeholder */
                if (verbose) {
                    fprintf(stderr, "WARNING: Migrate operation not fully implemented\n");
                }
                result = 0;
            }
            break;
            
        default:
            result = EINVAL;
            break;
    }
    
    if (result == 0) {
        task->status = 1;
    } else {
        task->status = -1;
        snprintf(task->error_msg, sizeof(task->error_msg), "%s", STRERROR(result));
    }
    
    return result;
}

/**
 * Worker thread function for parallel tag operations
 * 
 * This function is executed by each worker thread to process tag tasks
 * in parallel for better performance.
 * 
 * @param arg - TagContext pointer
 * @return NULL
 */
static void *tag_worker_thread(void *arg) {
    TagContext *ctx = (TagContext *)arg;
    int task_index;
    TagTask *task;
    int ret;
    
    /* Process tasks until done */
    while (1) {
        /* Get next task index */
        pthread_mutex_lock(&ctx->mutex);
        task_index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        /* Check if we're done */
        if (task_index >= ctx->task_count) {
            break;
        }
        
        task = &ctx->tasks[task_index];
        
        /* Process tag task */
        ret = process_tag_task(ctx, task);
        
        if (ret == 0 && task->status == 1) {
            pthread_mutex_lock(&stats_mutex);
            switch (ctx->operation) {
                case TAG_OP_ADD:
                    files_tagged++;
                    break;
                case TAG_OP_REMOVE:
                    files_untagged++;
                    break;
                case TAG_OP_SEARCH:
                    files_found++;
                    break;
                case TAG_OP_DELETE:
                    files_deleted++;
                    break;
                case TAG_OP_MIGRATE:
                    files_migrated++;
                    break;
                default:
                    break;
            }
            pthread_mutex_unlock(&stats_mutex);
            
            if (verbose && !quiet) {
                switch (ctx->operation) {
                    case TAG_OP_ADD:
                        printf("OK: Added tags to %s\n", task->file_id);
                        break;
                    case TAG_OP_REMOVE:
                        printf("OK: Removed tags from %s\n", task->file_id);
                        break;
                    case TAG_OP_LIST:
                        printf("OK: %s - Tags: %s\n", task->file_id,
                               strlen(task->current_tags) > 0 ? task->current_tags : "(none)");
                        break;
                    case TAG_OP_SEARCH:
                        printf("OK: Found %s (tags: %s)\n", task->file_id,
                               strlen(task->current_tags) > 0 ? task->current_tags : "(none)");
                        break;
                    case TAG_OP_DELETE:
                        printf("OK: Deleted %s\n", task->file_id);
                        break;
                    default:
                        break;
                }
            }
        } else {
            task->status = -1;
            pthread_mutex_lock(&stats_mutex);
            files_failed++;
            pthread_mutex_unlock(&stats_mutex);
            
            if (!quiet) {
                fprintf(stderr, "ERROR: Failed to process %s: %s\n",
                       task->file_id, task->error_msg);
            }
        }
        
        pthread_mutex_lock(&stats_mutex);
        total_files_processed++;
        pthread_mutex_unlock(&stats_mutex);
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
 * Search files by tags
 * 
 * This function searches for files by tags. Since FastDFS doesn't provide
 * a direct way to list all files, this function requires a file list
 * to search through.
 * 
 * @param ctx - Tag context
 * @param file_list - File list to search (required)
 * @return 0 on success, error code on failure
 */
static int search_files_by_tags(TagContext *ctx, const char *file_list) {
    char **file_ids = NULL;
    int file_count = 0;
    int result;
    int i;
    
    if (ctx == NULL || file_list == NULL) {
        return EINVAL;
    }
    
    /* Read file list */
    result = read_file_list(file_list, &file_ids, &file_count);
    if (result != 0) {
        return result;
    }
    
    if (file_count == 0) {
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return EINVAL;
    }
    
    /* Allocate tasks */
    ctx->tasks = (TagTask *)calloc(file_count, sizeof(TagTask));
    if (ctx->tasks == NULL) {
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return ENOMEM;
    }
    
    ctx->task_count = file_count;
    ctx->current_index = 0;
    
    /* Initialize tasks */
    for (i = 0; i < file_count; i++) {
        strncpy(ctx->tasks[i].file_id, file_ids[i], MAX_FILE_ID_LEN - 1);
        ctx->tasks[i].status = 0;
    }
    
    /* Process tasks in parallel would be done by worker threads */
    /* For now, process sequentially */
    for (i = 0; i < file_count; i++) {
        process_tag_task(ctx, &ctx->tasks[i]);
    }
    
    /* Cleanup */
    if (file_ids != NULL) {
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
    }
    
    return 0;
}

/**
 * Print tag results in text format
 * 
 * This function prints tag operation results in a human-readable
 * text format.
 * 
 * @param ctx - Tag context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_tag_results_text(TagContext *ctx, FILE *output_file) {
    int i;
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    fprintf(output_file, "\n");
    fprintf(output_file, "=== FastDFS Tag Operation Results ===\n");
    fprintf(output_file, "\n");
    
    /* Statistics */
    fprintf(output_file, "=== Statistics ===\n");
    fprintf(output_file, "Total files processed: %d\n", total_files_processed);
    
    switch (ctx->operation) {
        case TAG_OP_ADD:
            fprintf(output_file, "Files tagged: %d\n", files_tagged);
            break;
        case TAG_OP_REMOVE:
            fprintf(output_file, "Files untagged: %d\n", files_untagged);
            break;
        case TAG_OP_SEARCH:
            fprintf(output_file, "Files found: %d\n", files_found);
            break;
        case TAG_OP_DELETE:
            fprintf(output_file, "Files deleted: %d\n", files_deleted);
            break;
        case TAG_OP_MIGRATE:
            fprintf(output_file, "Files migrated: %d\n", files_migrated);
            break;
        default:
            break;
    }
    
    fprintf(output_file, "Files failed: %d\n", files_failed);
    fprintf(output_file, "\n");
    
    /* List results for list/search operations */
    if (ctx->operation == TAG_OP_LIST || ctx->operation == TAG_OP_SEARCH) {
        fprintf(output_file, "=== Results ===\n");
        fprintf(output_file, "\n");
        
        for (i = 0; i < ctx->task_count; i++) {
            TagTask *task = &ctx->tasks[i];
            
            if (task->status == 1) {
                if (ctx->operation == TAG_OP_LIST) {
                    fprintf(output_file, "%s: %s\n", task->file_id,
                           strlen(task->current_tags) > 0 ? task->current_tags : "(no tags)");
                } else if (ctx->operation == TAG_OP_SEARCH) {
                    fprintf(output_file, "%s\n", task->file_id);
                }
            }
        }
        
        fprintf(output_file, "\n");
    }
}

/**
 * Print tag results in JSON format
 * 
 * This function prints tag operation results in JSON format
 * for programmatic processing.
 * 
 * @param ctx - Tag context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_tag_results_json(TagContext *ctx, FILE *output_file) {
    int i;
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    fprintf(output_file, "{\n");
    fprintf(output_file, "  \"timestamp\": %ld,\n", (long)time(NULL));
    fprintf(output_file, "  \"operation\": \"%s\",\n",
           ctx->operation == TAG_OP_ADD ? "add" :
           ctx->operation == TAG_OP_REMOVE ? "remove" :
           ctx->operation == TAG_OP_LIST ? "list" :
           ctx->operation == TAG_OP_SEARCH ? "search" :
           ctx->operation == TAG_OP_DELETE ? "delete" : "migrate");
    fprintf(output_file, "  \"statistics\": {\n");
    fprintf(output_file, "    \"total_files_processed\": %d,\n", total_files_processed);
    
    switch (ctx->operation) {
        case TAG_OP_ADD:
            fprintf(output_file, "    \"files_tagged\": %d,\n", files_tagged);
            break;
        case TAG_OP_REMOVE:
            fprintf(output_file, "    \"files_untagged\": %d,\n", files_untagged);
            break;
        case TAG_OP_SEARCH:
            fprintf(output_file, "    \"files_found\": %d,\n", files_found);
            break;
        case TAG_OP_DELETE:
            fprintf(output_file, "    \"files_deleted\": %d,\n", files_deleted);
            break;
        case TAG_OP_MIGRATE:
            fprintf(output_file, "    \"files_migrated\": %d,\n", files_migrated);
            break;
        default:
            break;
    }
    
    fprintf(output_file, "    \"files_failed\": %d\n", files_failed);
    fprintf(output_file, "  }");
    
    /* Include results for list/search operations */
    if (ctx->operation == TAG_OP_LIST || ctx->operation == TAG_OP_SEARCH) {
        fprintf(output_file, ",\n  \"results\": [\n");
        
        int first = 1;
        for (i = 0; i < ctx->task_count; i++) {
            TagTask *task = &ctx->tasks[i];
            
            if (task->status == 1) {
                if (!first) {
                    fprintf(output_file, ",\n");
                }
                first = 0;
                
                fprintf(output_file, "    {\n");
                fprintf(output_file, "      \"file_id\": \"%s\"", task->file_id);
                
                if (strlen(task->current_tags) > 0) {
                    fprintf(output_file, ",\n      \"tags\": \"%s\"", task->current_tags);
                }
                
                fprintf(output_file, "\n    }");
            }
        }
        
        fprintf(output_file, "\n  ]");
    }
    
    fprintf(output_file, "\n}\n");
}

/**
 * Main function
 * 
 * Entry point for the tagging tool. Parses command-line
 * arguments and performs tag operations.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = some failures, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *file_list = NULL;
    char *target_group = NULL;
    char *output_file = NULL;
    int num_threads = DEFAULT_THREADS;
    int dry_run = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    TagContext ctx;
    pthread_t *threads = NULL;
    char **file_ids = NULL;
    int file_count = 0;
    char tags[MAX_TAGS_PER_FILE][MAX_TAG_LEN];
    int tag_count = 0;
    int i;
    FILE *out_fp = stdout;
    int opt;
    int option_index = 0;
    const char *command = NULL;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"group", required_argument, 0, 'g'},
        {"and", no_argument, 0, 1000},
        {"or", no_argument, 0, 1001},
        {"dry-run", no_argument, 0, 1002},
        {"threads", required_argument, 0, 'j'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(TagContext));
    ctx.search_and_mode = 1;  /* Default to AND mode */
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:f:g:j:o:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                file_list = optarg;
                break;
            case 'g':
                target_group = optarg;
                break;
            case 1000:
                ctx.search_and_mode = 1;
                break;
            case 1001:
                ctx.search_and_mode = 0;
                break;
            case 1002:
                dry_run = 1;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'v':
                verbose = 1;
                ctx.verbose = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'J':
                json_output = 1;
                ctx.json_output = 1;
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
    if (optind < argc) {
        command = argv[optind];
    } else {
        fprintf(stderr, "ERROR: Command required\n\n");
        print_usage(argv[0]);
        return 2;
    }
    
    /* Determine operation type */
    if (strcmp(command, "add") == 0) {
        ctx.operation = TAG_OP_ADD;
    } else if (strcmp(command, "remove") == 0) {
        ctx.operation = TAG_OP_REMOVE;
    } else if (strcmp(command, "list") == 0) {
        ctx.operation = TAG_OP_LIST;
    } else if (strcmp(command, "search") == 0) {
        ctx.operation = TAG_OP_SEARCH;
    } else if (strcmp(command, "delete") == 0) {
        ctx.operation = TAG_OP_DELETE;
    } else if (strcmp(command, "migrate") == 0) {
        ctx.operation = TAG_OP_MIGRATE;
    } else {
        fprintf(stderr, "ERROR: Unknown command: %s\n\n", command);
        print_usage(argv[0]);
        return 2;
    }
    
    /* Get tags from command-line arguments */
    if (ctx.operation == TAG_OP_ADD || ctx.operation == TAG_OP_REMOVE ||
        ctx.operation == TAG_OP_SEARCH || ctx.operation == TAG_OP_DELETE ||
        ctx.operation == TAG_OP_MIGRATE) {
        int tag_start = optind + 1;
        
        /* Skip file IDs if provided as arguments */
        if (file_list == NULL && tag_start < argc) {
            /* Check if next argument looks like a file ID or tag */
            /* For simplicity, assume all remaining args are tags */
            while (tag_start < argc) {
                if (tag_count < MAX_TAGS_PER_FILE) {
                    strncpy(tags[tag_count], argv[tag_start], MAX_TAG_LEN - 1);
                    tags[tag_count][MAX_TAG_LEN - 1] = '\0';
                    tag_count++;
                }
                tag_start++;
            }
        } else if (tag_start < argc) {
            /* Tags come after file list option */
            while (tag_start < argc) {
                if (tag_count < MAX_TAGS_PER_FILE) {
                    strncpy(tags[tag_count], argv[tag_start], MAX_TAG_LEN - 1);
                    tags[tag_count][MAX_TAG_LEN - 1] = '\0';
                    tag_count++;
                }
                tag_start++;
            }
        }
        
        if (tag_count == 0 && (ctx.operation == TAG_OP_ADD || ctx.operation == TAG_OP_REMOVE)) {
            fprintf(stderr, "ERROR: Tags required for %s command\n", command);
            return 2;
        }
        
        if (tag_count == 0 && (ctx.operation == TAG_OP_SEARCH || ctx.operation == TAG_OP_DELETE ||
                               ctx.operation == TAG_OP_MIGRATE)) {
            fprintf(stderr, "ERROR: Search tags required for %s command\n", command);
            return 2;
        }
    }
    
    /* Get file IDs */
    if (file_list != NULL) {
        result = read_file_list(file_list, &file_ids, &file_count);
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to read file list: %s\n", STRERROR(result));
            return 2;
        }
    } else if (ctx.operation != TAG_OP_SEARCH && ctx.operation != TAG_OP_DELETE &&
               ctx.operation != TAG_OP_MIGRATE && optind + 1 < argc) {
        /* Get file IDs from command-line arguments */
        int file_start = optind + 1;
        int file_end = file_start;
        
        /* Count file IDs (stop at first tag-like argument) */
        while (file_end < argc) {
            /* Simple heuristic: if it contains '=' or starts with '--', it's not a file ID */
            if (strchr(argv[file_end], '=') != NULL || strncmp(argv[file_end], "--", 2) == 0) {
                break;
            }
            file_end++;
        }
        
        file_count = file_end - file_start;
        if (file_count > 0) {
            file_ids = (char **)malloc(file_count * sizeof(char *));
            if (file_ids == NULL) {
                return ENOMEM;
            }
            
            for (i = 0; i < file_count; i++) {
                file_ids[i] = strdup(argv[file_start + i]);
                if (file_ids[i] == NULL) {
                    for (int j = 0; j < i; j++) {
                        free(file_ids[j]);
                    }
                    free(file_ids);
                    return ENOMEM;
                }
            }
        }
    }
    
    if ((ctx.operation == TAG_OP_ADD || ctx.operation == TAG_OP_REMOVE ||
         ctx.operation == TAG_OP_LIST) && file_count == 0) {
        fprintf(stderr, "ERROR: File IDs or file list required for %s command\n", command);
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return 2;
    }
    
    if ((ctx.operation == TAG_OP_SEARCH || ctx.operation == TAG_OP_DELETE ||
         ctx.operation == TAG_OP_MIGRATE) && file_list == NULL) {
        fprintf(stderr, "ERROR: File list required for %s command\n", command);
        return 2;
    }
    
    /* Initialize logging */
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    /* Initialize FastDFS client */
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return 2;
    }
    
    /* Connect to tracker server */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        fdfs_client_destroy();
        return 2;
    }
    
    ctx.pTrackerServer = pTrackerServer;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    /* Set search tags for search/delete/migrate operations */
    if (ctx.operation == TAG_OP_SEARCH || ctx.operation == TAG_OP_DELETE ||
        ctx.operation == TAG_OP_MIGRATE) {
        for (i = 0; i < tag_count && i < MAX_TAGS_PER_FILE; i++) {
            strncpy(ctx.search_tags[i], tags[i], MAX_TAG_LEN - 1);
        }
        ctx.search_tag_count = tag_count;
        
        if (target_group != NULL) {
            strncpy(ctx.target_group, target_group, sizeof(ctx.target_group) - 1);
        }
    }
    
    /* Reset statistics */
    total_files_processed = 0;
    files_tagged = 0;
    files_untagged = 0;
    files_found = 0;
    files_deleted = 0;
    files_migrated = 0;
    files_failed = 0;
    
    /* Handle search/delete/migrate operations */
    if (ctx.operation == TAG_OP_SEARCH || ctx.operation == TAG_OP_DELETE ||
        ctx.operation == TAG_OP_MIGRATE) {
        result = search_files_by_tags(&ctx, file_list);
        if (result != 0) {
            fprintf(stderr, "ERROR: Search operation failed: %s\n", STRERROR(result));
            pthread_mutex_destroy(&ctx.mutex);
            if (file_ids != NULL) {
                for (i = 0; i < file_count; i++) {
                    free(file_ids[i]);
                }
                free(file_ids);
            }
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
    } else {
        /* Allocate tasks */
        ctx.tasks = (TagTask *)calloc(file_count, sizeof(TagTask));
        if (ctx.tasks == NULL) {
            pthread_mutex_destroy(&ctx.mutex);
            if (file_ids != NULL) {
                for (i = 0; i < file_count; i++) {
                    free(file_ids[i]);
                }
                free(file_ids);
            }
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return ENOMEM;
        }
        
        ctx.task_count = file_count;
        ctx.current_index = 0;
        
        /* Initialize tasks */
        for (i = 0; i < file_count; i++) {
            TagTask *task = &ctx.tasks[i];
            strncpy(task->file_id, file_ids[i], MAX_FILE_ID_LEN - 1);
            
            if (ctx.operation == TAG_OP_ADD || ctx.operation == TAG_OP_REMOVE) {
                for (int j = 0; j < tag_count && j < MAX_TAGS_PER_FILE; j++) {
                    strncpy(task->tags[j], tags[j], MAX_TAG_LEN - 1);
                }
                task->tag_count = tag_count;
            }
            
            task->status = 0;
        }
        
        /* Limit number of threads */
        if (num_threads > MAX_THREADS) {
            num_threads = MAX_THREADS;
        }
        if (num_threads > file_count) {
            num_threads = file_count;
        }
        
        if (num_threads < 1) {
            num_threads = 1;
        }
        
        /* Allocate thread array */
        threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
        if (threads == NULL) {
            pthread_mutex_destroy(&ctx.mutex);
            free(ctx.tasks);
            if (file_ids != NULL) {
                for (i = 0; i < file_count; i++) {
                    free(file_ids[i]);
                }
                free(file_ids);
            }
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return ENOMEM;
        }
        
        /* Start worker threads */
        for (i = 0; i < num_threads; i++) {
            if (pthread_create(&threads[i], NULL, tag_worker_thread, &ctx) != 0) {
                fprintf(stderr, "ERROR: Failed to create thread %d\n", i);
                result = errno;
                break;
            }
        }
        
        /* Wait for all threads to complete */
        for (i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        
        free(threads);
    }
    
    /* Print results */
    if (output_file != NULL) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            out_fp = stdout;
        }
    }
    
    if (json_output) {
        print_tag_results_json(&ctx, out_fp);
    } else {
        print_tag_results_text(&ctx, out_fp);
    }
    
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    pthread_mutex_destroy(&ctx.mutex);
    if (ctx.tasks != NULL) {
        free(ctx.tasks);
    }
    if (file_ids != NULL) {
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
    }
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    /* Return appropriate exit code */
    if (files_failed > 0) {
        return 1;  /* Some failures */
    }
    
    return 0;  /* Success */
}

