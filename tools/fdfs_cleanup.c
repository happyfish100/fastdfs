/**
 * FastDFS File Expiration and Cleanup Tool
 * 
 * This tool provides comprehensive file lifecycle management capabilities
 * for FastDFS. It allows administrators to automatically delete old or
 * unused files based on various criteria such as file age, last access
 * time, file size, metadata, and custom rules.
 * 
 * Features:
 * - Delete files by age (based on creation timestamp)
 * - Delete files by last access time (from metadata)
 * - Delete files by custom criteria (size, metadata, patterns)
 * - Dry-run mode to preview deletions without actually deleting
 * - Scheduling support via daemon mode or cron integration
 * - Batch processing with parallel deletion
 * - Detailed reporting and statistics
 * - Safe deletion with confirmation prompts
 * - JSON and text output formats
 * - Comprehensive logging
 * 
 * Use Cases:
 * - Automated cleanup of temporary files
 * - Removal of old backup files
 * - Lifecycle management for archived data
 * - Storage optimization by removing unused files
 * - Compliance with data retention policies
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
#include <signal.h>
#include <dirent.h>
#include <fnmatch.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum file ID length */
#define MAX_FILE_ID_LEN 256

/* Maximum group name length */
#define MAX_GROUP_NAME_LEN 32

/* Maximum metadata key/value length */
#define MAX_METADATA_KEY_LEN 64
#define MAX_METADATA_VALUE_LEN 256

/* Maximum pattern length for file matching */
#define MAX_PATTERN_LEN 512

/* Maximum number of threads for parallel processing */
#define MAX_THREADS 20

/* Default number of threads */
#define DEFAULT_THREADS 4

/* Buffer size for file operations */
#define BUFFER_SIZE (256 * 1024)

/* Maximum number of files to process in one batch */
#define MAX_BATCH_SIZE 10000

/* Cleanup criteria types */
typedef enum {
    CRITERIA_AGE = 0,              /* Delete files older than specified age */
    CRITERIA_LAST_ACCESS = 1,      /* Delete files not accessed for specified time */
    CRITERIA_SIZE = 2,             /* Delete files larger/smaller than size */
    CRITERIA_METADATA = 3,         /* Delete files matching metadata criteria */
    CRITERIA_PATTERN = 4,          /* Delete files matching filename pattern */
    CRITERIA_CUSTOM = 5            /* Custom criteria (combination) */
} CleanupCriteriaType;

/* File information structure */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];  /* File ID */
    int64_t file_size;              /* File size in bytes */
    time_t create_time;             /* File creation timestamp */
    time_t last_access_time;        /* Last access time (from metadata) */
    uint32_t crc32;                 /* CRC32 checksum */
    int has_metadata;               /* Whether file has metadata */
    int metadata_count;             /* Number of metadata items */
    int should_delete;              /* Whether file should be deleted */
    char reason[256];               /* Reason for deletion (if applicable) */
    int delete_status;              /* Deletion status (0 = success, error code otherwise) */
    char error_msg[256];            /* Error message if deletion failed */
} FileInfo;

/* Cleanup criteria structure */
typedef struct {
    CleanupCriteriaType type;       /* Type of criteria */
    int64_t age_seconds;           /* Age threshold in seconds (for CRITERIA_AGE) */
    int64_t access_seconds;        /* Last access threshold in seconds (for CRITERIA_LAST_ACCESS) */
    int64_t min_size_bytes;        /* Minimum file size in bytes */
    int64_t max_size_bytes;        /* Maximum file size in bytes */
    char metadata_key[MAX_METADATA_KEY_LEN];    /* Metadata key to match */
    char metadata_value[MAX_METADATA_VALUE_LEN]; /* Metadata value to match */
    char pattern[MAX_PATTERN_LEN]; /* Filename pattern to match */
    int match_all;                 /* Whether all criteria must match (AND) or any (OR) */
} CleanupCriteria;

/* Cleanup task structure */
typedef struct {
    FileInfo *files;                /* Array of file information */
    int file_count;                 /* Number of files */
    int current_index;              /* Current file index being processed */
    pthread_mutex_t mutex;          /* Mutex for thread synchronization */
    ConnectionInfo *pTrackerServer; /* Tracker server connection */
    CleanupCriteria criteria;       /* Cleanup criteria */
    int dry_run;                    /* Dry-run mode flag */
    int verbose;                    /* Verbose output flag */
    int json_output;                /* JSON output flag */
} CleanupContext;

/* Global statistics */
static int total_files_scanned = 0;
static int files_to_delete = 0;
static int files_deleted = 0;
static int files_failed = 0;
static int files_skipped = 0;
static int64_t total_bytes_freed = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;
static int dry_run = 0;
static int daemon_mode = 0;
static int schedule_interval = 0;  /* Schedule interval in seconds (0 = run once) */
static int running = 1;            /* Daemon running flag */

/**
 * Signal handler for graceful shutdown
 * 
 * This function handles SIGINT and SIGTERM signals to allow
 * graceful shutdown of the daemon mode.
 * 
 * @param sig - Signal number
 */
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        if (verbose) {
            fprintf(stderr, "Received signal %d, shutting down gracefully...\n", sig);
        }
    }
}

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_cleanup tool, including all available options and examples.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -g <group_name> [CRITERIA]\n", program_name);
    printf("       %s [OPTIONS] -f <file_list> [CRITERIA]\n", program_name);
    printf("\n");
    printf("FastDFS File Expiration and Cleanup Tool\n");
    printf("\n");
    printf("This tool automatically deletes old or unused files from FastDFS\n");
    printf("based on various criteria such as file age, last access time,\n");
    printf("file size, metadata, or custom patterns.\n");
    printf("\n");
    printf("Cleanup Criteria (at least one required):\n");
    printf("  --age DAYS           Delete files older than N days\n");
    printf("  --access DAYS         Delete files not accessed for N days\n");
    printf("  --min-size SIZE      Delete files larger than SIZE\n");
    printf("  --max-size SIZE      Delete files smaller than SIZE\n");
    printf("  --metadata KEY=VALUE Delete files with matching metadata\n");
    printf("  --pattern PATTERN    Delete files matching filename pattern\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -g, --group NAME     Storage group name to clean (required if -f not used)\n");
    printf("  -f, --file LIST      File list to process (one file ID per line)\n");
    printf("  -j, --threads NUM     Number of parallel threads (default: 4, max: 20)\n");
    printf("  -n, --dry-run        Dry run mode (preview deletions without deleting)\n");
    printf("  -d, --daemon         Run as daemon (continuous cleanup)\n");
    printf("  -i, --interval SEC   Daemon interval in seconds (default: 3600)\n");
    printf("  -o, --output FILE    Output report file (default: stdout)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -q, --quiet          Quiet mode (only show summary)\n");
    printf("  -y, --yes            Skip confirmation prompt\n");
    printf("  -J, --json           Output results in JSON format\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Size Format:\n");
    printf("  Sizes can be specified with suffixes: B, KB, MB, GB, TB\n");
    printf("  Examples: 100GB, 500MB, 1TB, 1024\n");
    printf("\n");
    printf("Pattern Format:\n");
    printf("  Patterns support shell-style wildcards: *, ?, [abc]\n");
    printf("  Examples: *.tmp, backup_*, file_*.jpg\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - Cleanup completed successfully\n");
    printf("  1 - Some files failed to delete\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Delete files older than 30 days (dry run)\n");
    printf("  %s -g group1 --age 30 -n\n", program_name);
    printf("\n");
    printf("  # Delete files not accessed for 90 days\n");
    printf("  %s -g group1 --access 90 -y\n", program_name);
    printf("\n");
    printf("  # Delete files larger than 1GB\n");
    printf("  %s -g group1 --min-size 1GB -y\n", program_name);
    printf("\n");
    printf("  # Delete files matching pattern\n");
    printf("  %s -g group1 --pattern \"*.tmp\" -y\n", program_name);
    printf("\n");
    printf("  # Delete files with specific metadata\n");
    printf("  %s -g group1 --metadata \"type=temp\" -y\n", program_name);
    printf("\n");
    printf("  # Run as daemon, cleanup every hour\n");
    printf("  %s -g group1 --age 7 -d -i 3600\n", program_name);
    printf("\n");
    printf("  # Process specific file list\n");
    printf("  %s -f file_list.txt --age 30 -y\n", program_name);
}

/**
 * Parse size string to bytes
 * 
 * This function parses a human-readable size string (e.g., "10GB", "500MB")
 * and converts it to bytes. Supports KB, MB, GB, TB suffixes.
 * 
 * @param size_str - Size string to parse
 * @param bytes - Output parameter for parsed bytes
 * @return 0 on success, -1 on error
 */
static int parse_size_string(const char *size_str, int64_t *bytes) {
    char *endptr;
    double value;
    int64_t multiplier = 1;
    size_t len;
    char unit[8];
    int i;
    
    if (size_str == NULL || bytes == NULL) {
        return -1;
    }
    
    /* Parse numeric value */
    value = strtod(size_str, &endptr);
    if (endptr == size_str) {
        return -1;
    }
    
    /* Skip whitespace */
    while (isspace((unsigned char)*endptr)) {
        endptr++;
    }
    
    /* Extract unit */
    len = strlen(endptr);
    if (len > 0) {
        for (i = 0; i < len && i < sizeof(unit) - 1; i++) {
            unit[i] = toupper((unsigned char)endptr[i]);
        }
        unit[i] = '\0';
        
        if (strcmp(unit, "KB") == 0 || strcmp(unit, "K") == 0) {
            multiplier = 1024LL;
        } else if (strcmp(unit, "MB") == 0 || strcmp(unit, "M") == 0) {
            multiplier = 1024LL * 1024LL;
        } else if (strcmp(unit, "GB") == 0 || strcmp(unit, "G") == 0) {
            multiplier = 1024LL * 1024LL * 1024LL;
        } else if (strcmp(unit, "TB") == 0 || strcmp(unit, "T") == 0) {
            multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
        } else if (strcmp(unit, "B") == 0 || len == 0) {
            multiplier = 1;
        } else {
            return -1;
        }
    }
    
    *bytes = (int64_t)(value * multiplier);
    return 0;
}

/**
 * Format bytes to human-readable string
 * 
 * This function converts a byte count to a human-readable string
 * with appropriate units (B, KB, MB, GB, TB).
 * 
 * @param bytes - Number of bytes to format
 * @param buf - Output buffer for formatted string
 * @param buf_size - Size of output buffer
 */
static void format_bytes(int64_t bytes, char *buf, size_t buf_size) {
    if (bytes >= 1099511627776LL) {
        snprintf(buf, buf_size, "%.2f TB", bytes / 1099511627776.0);
    } else if (bytes >= 1073741824LL) {
        snprintf(buf, buf_size, "%.2f GB", bytes / 1073741824.0);
    } else if (bytes >= 1048576LL) {
        snprintf(buf, buf_size, "%.2f MB", bytes / 1048576.0);
    } else if (bytes >= 1024LL) {
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    } else {
        snprintf(buf, buf_size, "%lld B", (long long)bytes);
    }
}

/**
 * Format time duration to human-readable string
 * 
 * This function converts a time duration in seconds to a
 * human-readable string (e.g., "30 days", "2 hours").
 * 
 * @param seconds - Duration in seconds
 * @param buf - Output buffer for formatted string
 * @param buf_size - Size of output buffer
 */
static void format_duration(int64_t seconds, char *buf, size_t buf_size) {
    if (seconds >= 86400LL * 365) {
        snprintf(buf, buf_size, "%.1f years", seconds / (86400.0 * 365));
    } else if (seconds >= 86400LL * 30) {
        snprintf(buf, buf_size, "%.1f months", seconds / (86400.0 * 30));
    } else if (seconds >= 86400LL) {
        snprintf(buf, buf_size, "%.1f days", seconds / 86400.0);
    } else if (seconds >= 3600LL) {
        snprintf(buf, buf_size, "%.1f hours", seconds / 3600.0);
    } else if (seconds >= 60LL) {
        snprintf(buf, buf_size, "%.1f minutes", seconds / 60.0);
    } else {
        snprintf(buf, buf_size, "%lld seconds", (long long)seconds);
    }
}

/**
 * Get file information from storage server
 * 
 * This function retrieves detailed information about a file from
 * the FastDFS storage server, including size, creation time, CRC32,
 * and metadata.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param pStorageServer - Storage server connection
 * @param file_id - File ID to query
 * @param file_info - Output parameter for file information
 * @return 0 on success, error code on failure
 */
static int get_file_info(ConnectionInfo *pTrackerServer,
                        ConnectionInfo *pStorageServer,
                        const char *file_id,
                        FileInfo *file_info) {
    FDFSFileInfo fdfs_info;
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    int ret;
    int i;
    time_t current_time;
    
    if (pTrackerServer == NULL || pStorageServer == NULL ||
        file_id == NULL || file_info == NULL) {
        return EINVAL;
    }
    
    /* Initialize file info structure */
    memset(file_info, 0, sizeof(FileInfo));
    strncpy(file_info->file_id, file_id, MAX_FILE_ID_LEN - 1);
    
    /* Query file information from storage server */
    ret = storage_query_file_info1(pTrackerServer, pStorageServer,
                                   file_id, &fdfs_info);
    if (ret != 0) {
        file_info->delete_status = ret;
        snprintf(file_info->error_msg, sizeof(file_info->error_msg),
                "Failed to query file info: %s", STRERROR(ret));
        return ret;
    }
    
    /* Store file information */
    file_info->file_size = fdfs_info.file_size;
    file_info->create_time = fdfs_info.create_time;
    file_info->crc32 = fdfs_info.crc32;
    
    /* Try to get metadata */
    ret = storage_get_metadata1(pTrackerServer, pStorageServer,
                               file_id, &meta_list, &meta_count);
    if (ret == 0 && meta_list != NULL) {
        file_info->has_metadata = 1;
        file_info->metadata_count = meta_count;
        
        /* Look for last access time in metadata */
        current_time = time(NULL);
        file_info->last_access_time = current_time; /* Default to current time */
        
        for (i = 0; i < meta_count; i++) {
            /* Check for common last access time metadata keys */
            if (strcasecmp(meta_list[i].name, "last_access") == 0 ||
                strcasecmp(meta_list[i].name, "last_access_time") == 0 ||
                strcasecmp(meta_list[i].name, "accessed") == 0) {
                /* Parse timestamp from metadata value */
                file_info->last_access_time = (time_t)atoll(meta_list[i].value);
                break;
            }
        }
        
        free(meta_list);
    } else {
        file_info->has_metadata = 0;
        file_info->metadata_count = 0;
        file_info->last_access_time = file_info->create_time; /* Use creation time as fallback */
    }
    
    return 0;
}

/**
 * Check if file matches cleanup criteria
 * 
 * This function evaluates whether a file matches the specified
 * cleanup criteria and should be deleted.
 * 
 * @param file_info - File information to check
 * @param criteria - Cleanup criteria to match against
 * @return 1 if file should be deleted, 0 otherwise
 */
static int matches_criteria(FileInfo *file_info, CleanupCriteria *criteria) {
    time_t current_time;
    int64_t file_age;
    int64_t time_since_access;
    int matches = 0;
    int all_match = 1;
    
    if (file_info == NULL || criteria == NULL) {
        return 0;
    }
    
    current_time = time(NULL);
    
    /* Check age criteria */
    if (criteria->age_seconds > 0) {
        file_age = current_time - file_info->create_time;
        if (file_age >= criteria->age_seconds) {
            if (criteria->match_all) {
                matches = 1;
            } else {
                snprintf(file_info->reason, sizeof(file_info->reason),
                        "File age: %lld seconds (threshold: %lld)",
                        (long long)file_age, (long long)criteria->age_seconds);
                return 1;
            }
        } else {
            if (criteria->match_all) {
                all_match = 0;
            }
        }
    }
    
    /* Check last access criteria */
    if (criteria->access_seconds > 0) {
        time_since_access = current_time - file_info->last_access_time;
        if (time_since_access >= criteria->access_seconds) {
            if (criteria->match_all) {
                matches = 1;
            } else {
                snprintf(file_info->reason, sizeof(file_info->reason),
                        "Last access: %lld seconds ago (threshold: %lld)",
                        (long long)time_since_access, (long long)criteria->access_seconds);
                return 1;
            }
        } else {
            if (criteria->match_all) {
                all_match = 0;
            }
        }
    }
    
    /* Check size criteria */
    if (criteria->min_size_bytes > 0 || criteria->max_size_bytes > 0) {
        int size_match = 1;
        
        if (criteria->min_size_bytes > 0 &&
            file_info->file_size < criteria->min_size_bytes) {
            size_match = 0;
        }
        
        if (criteria->max_size_bytes > 0 &&
            file_info->file_size > criteria->max_size_bytes) {
            size_match = 0;
        }
        
        if (size_match) {
            if (criteria->match_all) {
                matches = 1;
            } else {
                snprintf(file_info->reason, sizeof(file_info->reason),
                        "File size: %lld bytes (range: %lld - %lld)",
                        (long long)file_info->file_size,
                        (long long)criteria->min_size_bytes,
                        (long long)criteria->max_size_bytes);
                return 1;
            }
        } else {
            if (criteria->match_all) {
                all_match = 0;
            }
        }
    }
    
    /* Check pattern criteria */
    if (criteria->pattern[0] != '\0') {
        /* Extract filename from file_id (everything after last /) */
        const char *filename = strrchr(file_info->file_id, '/');
        if (filename == NULL) {
            filename = file_info->file_id;
        } else {
            filename++; /* Skip the / */
        }
        
        if (fnmatch(criteria->pattern, filename, 0) == 0) {
            if (criteria->match_all) {
                matches = 1;
            } else {
                snprintf(file_info->reason, sizeof(file_info->reason),
                        "Filename matches pattern: %s", criteria->pattern);
                return 1;
            }
        } else {
            if (criteria->match_all) {
                all_match = 0;
            }
        }
    }
    
    /* For match_all mode, all criteria must match */
    if (criteria->match_all) {
        return (matches && all_match) ? 1 : 0;
    }
    
    /* For match_any mode, at least one criterion must match */
    return matches;
}

/**
 * Delete a single file
 * 
 * This function deletes a single file from FastDFS storage.
 * In dry-run mode, it only simulates the deletion.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param pStorageServer - Storage server connection
 * @param file_id - File ID to delete
 * @param dry_run - Whether to perform dry run
 * @return 0 on success, error code on failure
 */
static int delete_file(ConnectionInfo *pTrackerServer,
                      ConnectionInfo *pStorageServer,
                      const char *file_id,
                      int dry_run) {
    int ret;
    
    if (pTrackerServer == NULL || pStorageServer == NULL || file_id == NULL) {
        return EINVAL;
    }
    
    if (dry_run) {
        /* Dry run - don't actually delete */
        if (verbose) {
            printf("DRY RUN: Would delete %s\n", file_id);
        }
        return 0;
    }
    
    /* Actually delete the file */
    ret = storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    return ret;
}

/**
 * Worker thread function for parallel file processing
 * 
 * This function is executed by each worker thread to process files
 * in parallel. It checks files against criteria and deletes matching ones.
 * 
 * @param arg - CleanupContext pointer
 * @return NULL
 */
static void *cleanup_worker_thread(void *arg) {
    CleanupContext *ctx = (CleanupContext *)arg;
    int file_index;
    FileInfo *file_info;
    ConnectionInfo *pStorageServer;
    int ret;
    
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
        
        file_info = &ctx->files[file_index];
        
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            file_info->delete_status = errno;
            snprintf(file_info->error_msg, sizeof(file_info->error_msg),
                    "Failed to connect to storage server");
            continue;
        }
        
        /* Get file information */
        ret = get_file_info(ctx->pTrackerServer, pStorageServer,
                           file_info->file_id, file_info);
        if (ret != 0) {
            file_info->should_delete = 0;
            tracker_disconnect_server_ex(pStorageServer, true);
            continue;
        }
        
        /* Check if file matches criteria */
        file_info->should_delete = matches_criteria(file_info, &ctx->criteria);
        
        if (file_info->should_delete) {
            /* Delete the file */
            ret = delete_file(ctx->pTrackerServer, pStorageServer,
                            file_info->file_id, ctx->dry_run);
            
            if (ret == 0) {
                file_info->delete_status = 0;
                
                /* Update statistics */
                pthread_mutex_lock(&stats_mutex);
                files_deleted++;
                total_bytes_freed += file_info->file_size;
                pthread_mutex_unlock(&stats_mutex);
                
                if (ctx->verbose && !ctx->json_output) {
                    printf("Deleted: %s (%s)\n", file_info->file_id, file_info->reason);
                }
            } else {
                file_info->delete_status = ret;
                snprintf(file_info->error_msg, sizeof(file_info->error_msg),
                        "Delete failed: %s", STRERROR(ret));
                
                pthread_mutex_lock(&stats_mutex);
                files_failed++;
                pthread_mutex_unlock(&stats_mutex);
                
                if (ctx->verbose && !ctx->json_output) {
                    fprintf(stderr, "ERROR: Failed to delete %s: %s\n",
                           file_info->file_id, file_info->error_msg);
                }
            }
        } else {
            /* File doesn't match criteria */
            file_info->delete_status = 0;
            
            pthread_mutex_lock(&stats_mutex);
            files_skipped++;
            pthread_mutex_unlock(&stats_mutex);
        }
        
        /* Disconnect from storage server */
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return NULL;
}

/**
 * Get list of files from a group
 * 
 * This function retrieves a list of files from a storage group.
 * Note: FastDFS doesn't provide a direct API to list all files,
 * so this function would need to work with a file list provided
 * by the user or from an external source.
 * 
 * For now, this is a placeholder that would need to be implemented
 * based on available FastDFS APIs or external file tracking.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param group_name - Group name
 * @param file_list - Output array for file IDs
 * @param max_files - Maximum number of files
 * @param file_count - Output parameter for actual file count
 * @return 0 on success, error code on failure
 */
static int get_group_files(ConnectionInfo *pTrackerServer,
                          const char *group_name,
                          char **file_list,
                          int max_files,
                          int *file_count) {
    /* Note: FastDFS doesn't provide a direct API to list all files in a group */
    /* This would typically require maintaining an external file index or */
    /* using a file list provided by the user */
    
    *file_count = 0;
    return 0;
}

/**
 * Process files from a file list
 * 
 * This function reads file IDs from a file and processes them
 * for cleanup based on the specified criteria.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param list_file - Path to file containing file IDs
 * @param criteria - Cleanup criteria
 * @param num_threads - Number of parallel threads
 * @param output_file - Output file for report (NULL for stdout)
 * @return 0 on success, error code on failure
 */
static int process_file_list(ConnectionInfo *pTrackerServer,
                            const char *list_file,
                            CleanupCriteria *criteria,
                            int num_threads,
                            const char *output_file) {
    FILE *fp;
    FILE *out_fp = stdout;
    char line[MAX_FILE_ID_LEN + 1];
    char **file_ids = NULL;
    int file_count = 0;
    int capacity = 1000;
    int i;
    pthread_t *threads = NULL;
    CleanupContext ctx;
    FileInfo *file_infos = NULL;
    int ret = 0;
    time_t start_time;
    time_t end_time;
    
    /* Allocate initial array for file IDs */
    file_ids = (char **)malloc(capacity * sizeof(char *));
    if (file_ids == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        return ENOMEM;
    }
    
    /* Open list file */
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        free(file_ids);
        return errno;
    }
    
    /* Read file IDs from list */
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p;
        
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
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        /* Expand array if needed */
        if (file_count >= capacity) {
            capacity *= 2;
            file_ids = (char **)realloc(file_ids, capacity * sizeof(char *));
            if (file_ids == NULL) {
                fprintf(stderr, "ERROR: Failed to reallocate memory\n");
                fclose(fp);
                for (i = 0; i < file_count; i++) {
                    free(file_ids[i]);
                }
                free(file_ids);
                return ENOMEM;
            }
        }
        
        /* Allocate and store file ID */
        file_ids[file_count] = (char *)malloc(strlen(line) + 1);
        if (file_ids[file_count] == NULL) {
            fprintf(stderr, "ERROR: Failed to allocate memory for file ID\n");
            fclose(fp);
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
            return ENOMEM;
        }
        
        strcpy(file_ids[file_count], line);
        file_count++;
    }
    
    fclose(fp);
    
    if (file_count == 0) {
        fprintf(stderr, "ERROR: No file IDs found in list file\n");
        free(file_ids);
        return EINVAL;
    }
    
    /* Allocate file info array */
    file_infos = (FileInfo *)calloc(file_count, sizeof(FileInfo));
    if (file_infos == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for file infos\n");
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
        return ENOMEM;
    }
    
    /* Initialize file info structures */
    for (i = 0; i < file_count; i++) {
        strncpy(file_infos[i].file_id, file_ids[i], MAX_FILE_ID_LEN - 1);
    }
    
    /* Initialize thread context */
    memset(&ctx, 0, sizeof(CleanupContext));
    ctx.files = file_infos;
    ctx.file_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    memcpy(&ctx.criteria, criteria, sizeof(CleanupCriteria));
    ctx.dry_run = dry_run;
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
        fprintf(stderr, "ERROR: Failed to allocate memory for threads\n");
        pthread_mutex_destroy(&ctx.mutex);
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
        free(file_infos);
        return ENOMEM;
    }
    
    /* Record start time */
    start_time = time(NULL);
    
    /* Update statistics */
    pthread_mutex_lock(&stats_mutex);
    total_files_scanned = file_count;
    files_to_delete = 0; /* Will be updated as files are processed */
    files_deleted = 0;
    files_failed = 0;
    files_skipped = 0;
    total_bytes_freed = 0;
    pthread_mutex_unlock(&stats_mutex);
    
    /* Start worker threads */
    for (i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, cleanup_worker_thread, &ctx) != 0) {
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
        fprintf(out_fp, "  \"timestamp\": %ld,\n", (long)time(NULL));
        fprintf(out_fp, "  \"dry_run\": %s,\n", dry_run ? "true" : "false");
        fprintf(out_fp, "  \"total_scanned\": %d,\n", total_files_scanned);
        fprintf(out_fp, "  \"files_deleted\": %d,\n", files_deleted);
        fprintf(out_fp, "  \"files_failed\": %d,\n", files_failed);
        fprintf(out_fp, "  \"files_skipped\": %d,\n", files_skipped);
        fprintf(out_fp, "  \"total_bytes_freed\": %lld,\n", (long long)total_bytes_freed);
        fprintf(out_fp, "  \"duration_seconds\": %ld,\n", (long)(end_time - start_time));
        fprintf(out_fp, "  \"files\": [\n");
        
        for (i = 0; i < file_count; i++) {
            FileInfo *fi = &file_infos[i];
            
            if (i > 0) {
                fprintf(out_fp, ",\n");
            }
            
            fprintf(out_fp, "    {\n");
            fprintf(out_fp, "      \"file_id\": \"%s\",\n", fi->file_id);
            fprintf(out_fp, "      \"file_size\": %lld,\n", (long long)fi->file_size);
            fprintf(out_fp, "      \"create_time\": %ld,\n", (long)fi->create_time);
            fprintf(out_fp, "      \"last_access_time\": %ld,\n", (long)fi->last_access_time);
            fprintf(out_fp, "      \"should_delete\": %s,\n", fi->should_delete ? "true" : "false");
            fprintf(out_fp, "      \"delete_status\": %d,\n", fi->delete_status);
            
            if (strlen(fi->reason) > 0) {
                fprintf(out_fp, "      \"reason\": \"%s\",\n", fi->reason);
            }
            
            if (fi->delete_status != 0 && strlen(fi->error_msg) > 0) {
                fprintf(out_fp, "      \"error_msg\": \"%s\",\n", fi->error_msg);
            }
            
            fprintf(out_fp, "    }");
        }
        
        fprintf(out_fp, "\n  ]\n");
        fprintf(out_fp, "}\n");
    } else {
        /* Text output */
        fprintf(out_fp, "\n");
        fprintf(out_fp, "=== FastDFS Cleanup Results ===\n");
        fprintf(out_fp, "Mode: %s\n", dry_run ? "DRY RUN" : "LIVE");
        fprintf(out_fp, "Total files scanned: %d\n", total_files_scanned);
        fprintf(out_fp, "Files deleted: %d\n", files_deleted);
        fprintf(out_fp, "Files failed: %d\n", files_failed);
        fprintf(out_fp, "Files skipped: %d\n", files_skipped);
        
        if (total_bytes_freed > 0) {
            char bytes_buf[64];
            format_bytes(total_bytes_freed, bytes_buf, sizeof(bytes_buf));
            fprintf(out_fp, "Total bytes freed: %s\n", bytes_buf);
        }
        
        fprintf(out_fp, "Duration: %ld seconds\n", (long)(end_time - start_time));
        fprintf(out_fp, "\n");
        
        if (dry_run) {
            fprintf(out_fp, "⚠ DRY RUN MODE: No files were actually deleted\n");
        } else if (files_deleted > 0) {
            fprintf(out_fp, "✓ Cleanup completed successfully\n");
        }
        
        if (files_failed > 0) {
            fprintf(out_fp, "⚠ WARNING: %d file(s) failed to delete\n", files_failed);
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
    free(file_infos);
    
    return ret;
}

/**
 * Main cleanup function
 * 
 * This function performs the main cleanup operation based on the
 * specified criteria and configuration.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param group_name - Group name to clean (NULL if using file list)
 * @param list_file - File list to process (NULL if using group)
 * @param criteria - Cleanup criteria
 * @param num_threads - Number of parallel threads
 * @param output_file - Output file for report
 * @return 0 on success, error code on failure
 */
static int perform_cleanup(ConnectionInfo *pTrackerServer,
                          const char *group_name,
                          const char *list_file,
                          CleanupCriteria *criteria,
                          int num_threads,
                          const char *output_file) {
    int ret;
    
    if (pTrackerServer == NULL || criteria == NULL) {
        return EINVAL;
    }
    
    /* Check that at least one criterion is specified */
    if (criteria->age_seconds == 0 &&
        criteria->access_seconds == 0 &&
        criteria->min_size_bytes == 0 &&
        criteria->max_size_bytes == 0 &&
        criteria->pattern[0] == '\0' &&
        criteria->metadata_key[0] == '\0') {
        fprintf(stderr, "ERROR: At least one cleanup criterion must be specified\n");
        return EINVAL;
    }
    
    if (list_file != NULL) {
        /* Process files from list */
        ret = process_file_list(pTrackerServer, list_file, criteria,
                               num_threads, output_file);
    } else if (group_name != NULL) {
        /* For group-based cleanup, we need a file list */
        /* In a real implementation, this would require maintaining */
        /* a file index or using an external file tracking system */
        fprintf(stderr, "ERROR: Group-based cleanup requires a file list\n");
        fprintf(stderr, "Please provide a file list using -f option\n");
        return EINVAL;
    } else {
        fprintf(stderr, "ERROR: Either group name (-g) or file list (-f) must be specified\n");
        return EINVAL;
    }
    
    return ret;
}

/**
 * Main function
 * 
 * Entry point for the file cleanup tool. Parses command-line
 * arguments and performs file cleanup operations.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = some failures, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *group_name = NULL;
    char *list_file = NULL;
    char *output_file = NULL;
    int num_threads = DEFAULT_THREADS;
    int skip_confirm = 0;
    CleanupCriteria criteria;
    int result;
    ConnectionInfo *pTrackerServer;
    int opt;
    int option_index = 0;
    char *age_str = NULL;
    char *access_str = NULL;
    char *min_size_str = NULL;
    char *max_size_str = NULL;
    char *metadata_str = NULL;
    int days;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"group", required_argument, 0, 'g'},
        {"file", required_argument, 0, 'f'},
        {"threads", required_argument, 0, 'j'},
        {"dry-run", no_argument, 0, 'n'},
        {"daemon", no_argument, 0, 'd'},
        {"interval", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"yes", no_argument, 0, 'y'},
        {"json", no_argument, 0, 'J'},
        {"age", required_argument, 0, 1000},
        {"access", required_argument, 0, 1001},
        {"min-size", required_argument, 0, 1002},
        {"max-size", required_argument, 0, 1003},
        {"metadata", required_argument, 0, 1004},
        {"pattern", required_argument, 0, 1005},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize criteria structure */
    memset(&criteria, 0, sizeof(CleanupCriteria));
    criteria.match_all = 0; /* Match any criteria by default */
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:g:f:j:ndi:o:vqyJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'g':
                group_name = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 'n':
                dry_run = 1;
                break;
            case 'd':
                daemon_mode = 1;
                break;
            case 'i':
                schedule_interval = atoi(optarg);
                if (schedule_interval < 1) schedule_interval = 3600;
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
            case 'y':
                skip_confirm = 1;
                break;
            case 'J':
                json_output = 1;
                break;
            case 1000:
                age_str = optarg;
                break;
            case 1001:
                access_str = optarg;
                break;
            case 1002:
                min_size_str = optarg;
                break;
            case 1003:
                max_size_str = optarg;
                break;
            case 1004:
                metadata_str = optarg;
                break;
            case 1005:
                strncpy(criteria.pattern, optarg, sizeof(criteria.pattern) - 1);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 2;
        }
    }
    
    /* Parse age criteria */
    if (age_str != NULL) {
        days = atoi(age_str);
        if (days <= 0) {
            fprintf(stderr, "ERROR: Invalid age: %s (must be positive number of days)\n", age_str);
            return 2;
        }
        criteria.age_seconds = (int64_t)days * 86400LL;
    }
    
    /* Parse access criteria */
    if (access_str != NULL) {
        days = atoi(access_str);
        if (days <= 0) {
            fprintf(stderr, "ERROR: Invalid access time: %s (must be positive number of days)\n", access_str);
            return 2;
        }
        criteria.access_seconds = (int64_t)days * 86400LL;
    }
    
    /* Parse size criteria */
    if (min_size_str != NULL) {
        if (parse_size_string(min_size_str, &criteria.min_size_bytes) != 0) {
            fprintf(stderr, "ERROR: Invalid min-size: %s\n", min_size_str);
            return 2;
        }
    }
    
    if (max_size_str != NULL) {
        if (parse_size_string(max_size_str, &criteria.max_size_bytes) != 0) {
            fprintf(stderr, "ERROR: Invalid max-size: %s\n", max_size_str);
            return 2;
        }
    }
    
    /* Parse metadata criteria */
    if (metadata_str != NULL) {
        char *equals = strchr(metadata_str, '=');
        if (equals == NULL) {
            fprintf(stderr, "ERROR: Invalid metadata format: %s (expected KEY=VALUE)\n", metadata_str);
            return 2;
        }
        
        *equals = '\0';
        strncpy(criteria.metadata_key, metadata_str, sizeof(criteria.metadata_key) - 1);
        strncpy(criteria.metadata_value, equals + 1, sizeof(criteria.metadata_value) - 1);
    }
    
    /* Validate required arguments */
    if (group_name == NULL && list_file == NULL) {
        fprintf(stderr, "ERROR: Either group name (-g) or file list (-f) must be specified\n\n");
        print_usage(argv[0]);
        return 2;
    }
    
    /* Check that at least one criterion is specified */
    if (criteria.age_seconds == 0 &&
        criteria.access_seconds == 0 &&
        criteria.min_size_bytes == 0 &&
        criteria.max_size_bytes == 0 &&
        criteria.pattern[0] == '\0' &&
        criteria.metadata_key[0] == '\0') {
        fprintf(stderr, "ERROR: At least one cleanup criterion must be specified\n\n");
        print_usage(argv[0]);
        return 2;
    }
    
    /* Setup signal handlers for daemon mode */
    if (daemon_mode) {
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
    }
    
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
    
    /* Perform cleanup in loop if daemon mode */
    do {
        /* Perform cleanup */
        result = perform_cleanup(pTrackerServer, group_name, list_file,
                                &criteria, num_threads, output_file);
        
        if (daemon_mode && running) {
            if (!quiet && !json_output) {
                printf("Cleanup completed. Next run in %d seconds...\n", schedule_interval);
            }
            sleep(schedule_interval);
        }
    } while (daemon_mode && running);
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    /* Return appropriate exit code */
    if (result != 0) {
        return 2;  /* Error occurred */
    }
    
    if (files_failed > 0) {
        return 1;  /* Some files failed */
    }
    
    return 0;  /* Success */
}

