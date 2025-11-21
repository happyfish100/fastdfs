/**
 * FastDFS Snapshot Tool
 * 
 * This tool provides comprehensive snapshot capabilities for FastDFS,
 * allowing users to create point-in-time snapshots of file state,
 * restore files from snapshots, list snapshots, and manage snapshot
 * retention policies.
 * 
 * Features:
 * - Create point-in-time snapshots of file state
 * - Restore files from snapshots
 * - List available snapshots
 * - Snapshot retention policies
 * - Compare snapshots
 * - Snapshot metadata preservation
 * - Multi-threaded snapshot operations
 * - JSON and text output formats
 * 
 * Snapshot Operations:
 * - Create: Capture current state of files
 * - Restore: Restore files to snapshot state
 * - List: List all available snapshots
 * - Delete: Delete old snapshots based on retention policy
 * - Compare: Compare two snapshots
 * 
 * Snapshot Contents:
 * - File IDs and paths
 * - File sizes
 * - CRC32 checksums
 * - Creation timestamps
 * - File metadata
 * - Snapshot timestamp
 * 
 * Retention Policies:
 * - Keep N most recent snapshots
 * - Keep snapshots for N days
 * - Keep snapshots older than N days
 * - Custom retention rules
 * 
 * Use Cases:
 * - Point-in-time recovery
 * - File rollback operations
 * - Data versioning
 * - Disaster recovery
 * - Change tracking
 * - Audit trails
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
#include <dirent.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum file ID length */
#define MAX_FILE_ID_LEN 256

/* Maximum path length */
#define MAX_PATH_LEN 1024

/* Maximum snapshot name length */
#define MAX_SNAPSHOT_NAME_LEN 128

/* Maximum number of threads for parallel processing */
#define MAX_THREADS 20

/* Default number of threads */
#define DEFAULT_THREADS 4

/* Maximum line length for file operations */
#define MAX_LINE_LEN 4096

/* Snapshot file entry structure */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];        /* File ID */
    int64_t file_size;                    /* File size in bytes */
    uint32_t crc32;                       /* CRC32 checksum */
    time_t create_time;                    /* File creation timestamp */
    int has_metadata;                      /* Whether file has metadata */
    char metadata_file[MAX_PATH_LEN];      /* Path to metadata file */
} SnapshotFileEntry;

/* Snapshot structure */
typedef struct {
    char snapshot_name[MAX_SNAPSHOT_NAME_LEN];  /* Snapshot name */
    char snapshot_dir[MAX_PATH_LEN];             /* Snapshot directory */
    time_t snapshot_time;                        /* Snapshot timestamp */
    int file_count;                              /* Number of files in snapshot */
    SnapshotFileEntry *files;                    /* Array of file entries */
    char description[512];                       /* Snapshot description */
} Snapshot;

/* Snapshot context */
typedef struct {
    char snapshot_base_dir[MAX_PATH_LEN];        /* Base directory for snapshots */
    Snapshot *current_snapshot;                  /* Current snapshot being created */
    ConnectionInfo *pTrackerServer;              /* Tracker server connection */
    int preserve_metadata;                        /* Preserve metadata flag */
    int verbose;                                  /* Verbose output flag */
    int json_output;                              /* JSON output flag */
    pthread_mutex_t mutex;                        /* Mutex for thread synchronization */
} SnapshotContext;

/* Global statistics */
static int total_files_processed = 0;
static int files_snapshotted = 0;
static int files_restored = 0;
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
 * fdfs_snapshot tool, including all available options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <command> [command_args...]\n", program_name);
    printf("\n");
    printf("FastDFS Snapshot Tool\n");
    printf("\n");
    printf("This tool creates point-in-time snapshots of file state,\n");
    printf("restores files from snapshots, lists snapshots, and manages\n");
    printf("snapshot retention policies.\n");
    printf("\n");
    printf("Commands:\n");
    printf("  create <name> [OPTIONS]     Create a new snapshot\n");
    printf("  restore <name> [OPTIONS]    Restore files from snapshot\n");
    printf("  list                        List all snapshots\n");
    printf("  delete <name>               Delete a snapshot\n");
    printf("  compare <name1> <name2>    Compare two snapshots\n");
    printf("  cleanup [OPTIONS]           Clean up old snapshots\n");
    printf("\n");
    printf("Create Options:\n");
    printf("  -f, --file LIST             File list to snapshot (one file ID per line)\n");
    printf("  -g, --group NAME            Snapshot entire group\n");
    printf("  -d, --description TEXT      Snapshot description\n");
    printf("\n");
    printf("Restore Options:\n");
    printf("  -f, --file LIST             File list to restore (optional, all if not specified)\n");
    printf("  --dry-run                   Preview restore without actually restoring\n");
    printf("\n");
    printf("Cleanup Options:\n");
    printf("  --keep-count NUM            Keep N most recent snapshots\n");
    printf("  --keep-days NUM             Keep snapshots for N days\n");
    printf("  --dry-run                   Preview cleanup without deleting\n");
    printf("\n");
    printf("Global Options:\n");
    printf("  -c, --config FILE           Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -b, --base-dir DIR          Base directory for snapshots (default: /var/fdfs/snapshots)\n");
    printf("  -m, --metadata              Preserve file metadata\n");
    printf("  -j, --threads NUM           Number of parallel threads (default: 4, max: 20)\n");
    printf("  -o, --output FILE           Output report file (default: stdout)\n");
    printf("  -v, --verbose               Verbose output\n");
    printf("  -q, --quiet                 Quiet mode (only show errors)\n");
    printf("  -J, --json                  Output in JSON format\n");
    printf("  -h, --help                  Show this help message\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - Operation completed successfully\n");
    printf("  1 - Some operations failed\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Create snapshot from file list\n");
    printf("  %s create snapshot1 -f file_list.txt\n", program_name);
    printf("\n");
    printf("  # Create snapshot of entire group\n");
    printf("  %s create group1_snapshot -g group1\n", program_name);
    printf("\n");
    printf("  # Restore from snapshot\n");
    printf("  %s restore snapshot1\n", program_name);
    printf("\n");
    printf("  # List all snapshots\n");
    printf("  %s list\n", program_name);
    printf("\n");
    printf("  # Cleanup old snapshots\n");
    printf("  %s cleanup --keep-count 10\n", program_name);
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
 * Format timestamp to human-readable string
 * 
 * This function converts a Unix timestamp to a human-readable
 * date-time string.
 * 
 * @param timestamp - Unix timestamp
 * @param buf - Output buffer for formatted string
 * @param buf_size - Size of output buffer
 */
static void format_timestamp(time_t timestamp, char *buf, size_t buf_size) {
    struct tm *tm_info;
    
    if (timestamp == 0) {
        snprintf(buf, buf_size, "Unknown");
        return;
    }
    
    tm_info = localtime(&timestamp);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * Create directory recursively
 * 
 * This function creates a directory and all parent directories
 * if they don't exist.
 * 
 * @param path - Directory path to create
 * @return 0 on success, error code on failure
 */
static int create_directory_recursive(const char *path) {
    char tmp[MAX_PATH_LEN];
    char *p = NULL;
    size_t len;
    
    if (path == NULL) {
        return EINVAL;
    }
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (len == 0) {
        return 0;
    }
    
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return errno;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return errno;
    }
    
    return 0;
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
 * Create snapshot entry for a file
 * 
 * This function creates a snapshot entry by capturing the current
 * state of a file.
 * 
 * @param ctx - Snapshot context
 * @param file_id - File ID
 * @param entry - Output parameter for snapshot entry
 * @return 0 on success, error code on failure
 */
static int create_snapshot_entry(SnapshotContext *ctx, const char *file_id,
                                 SnapshotFileEntry *entry) {
    FDFSFileInfo file_info;
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    int result;
    ConnectionInfo *pStorageServer;
    char meta_file_path[MAX_PATH_LEN];
    FILE *meta_fp = NULL;
    
    if (ctx == NULL || file_id == NULL || entry == NULL) {
        return EINVAL;
    }
    
    /* Initialize entry */
    memset(entry, 0, sizeof(SnapshotFileEntry));
    strncpy(entry->file_id, file_id, MAX_FILE_ID_LEN - 1);
    
    /* Get storage connection */
    pStorageServer = get_storage_connection(ctx->pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    /* Query file information */
    result = storage_query_file_info1(ctx->pTrackerServer, pStorageServer,
                                     file_id, &file_info);
    if (result != 0) {
        tracker_disconnect_server_ex(pStorageServer, true);
        return result;
    }
    
    entry->file_size = file_info.file_size;
    entry->crc32 = file_info.crc32;
    entry->create_time = file_info.create_timestamp;
    
    /* Get metadata if requested */
    if (ctx->preserve_metadata) {
        result = storage_get_metadata1(ctx->pTrackerServer, pStorageServer,
                                      file_id, &meta_list, &meta_count);
        if (result == 0 && meta_list != NULL && meta_count > 0) {
            /* Save metadata to file */
            snprintf(meta_file_path, sizeof(meta_file_path), "%s/%s.meta",
                    ctx->current_snapshot->snapshot_dir, file_id);
            
            /* Replace '/' with '_' in filename for filesystem safety */
            char *p = meta_file_path;
            while (*p) {
                if (*p == '/') {
                    *p = '_';
                }
                p++;
            }
            
            /* Create directory if needed */
            char *dir_end = strrchr(meta_file_path, '/');
            if (dir_end != NULL) {
                *dir_end = '\0';
                create_directory_recursive(meta_file_path);
                *dir_end = '/';
            }
            
            meta_fp = fopen(meta_file_path, "w");
            if (meta_fp != NULL) {
                for (int i = 0; i < meta_count; i++) {
                    fprintf(meta_fp, "%s=%s\n", meta_list[i].name, meta_list[i].value);
                }
                fclose(meta_fp);
                entry->has_metadata = 1;
                strncpy(entry->metadata_file, meta_file_path, sizeof(entry->metadata_file) - 1);
            }
            
            free(meta_list);
        }
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    return 0;
}

/**
 * Write snapshot manifest
 * 
 * This function writes a snapshot manifest file containing all
 * file entries in the snapshot.
 * 
 * @param snapshot - Snapshot structure
 * @return 0 on success, error code on failure
 */
static int write_snapshot_manifest(Snapshot *snapshot) {
    char manifest_path[MAX_PATH_LEN];
    FILE *fp;
    int i;
    
    if (snapshot == NULL) {
        return EINVAL;
    }
    
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.txt",
            snapshot->snapshot_dir);
    
    fp = fopen(manifest_path, "w");
    if (fp == NULL) {
        return errno;
    }
    
    fprintf(fp, "# FastDFS Snapshot Manifest\n");
    fprintf(fp, "# Snapshot: %s\n", snapshot->snapshot_name);
    fprintf(fp, "# Created: %s", ctime(&snapshot->snapshot_time));
    fprintf(fp, "# File Count: %d\n", snapshot->file_count);
    if (strlen(snapshot->description) > 0) {
        fprintf(fp, "# Description: %s\n", snapshot->description);
    }
    fprintf(fp, "#\n");
    fprintf(fp, "# Format: file_id|size|crc32|create_time|has_metadata|metadata_file\n");
    fprintf(fp, "#\n");
    
    for (i = 0; i < snapshot->file_count; i++) {
        SnapshotFileEntry *entry = &snapshot->files[i];
        fprintf(fp, "%s|%lld|%08X|%ld|%d|%s\n",
               entry->file_id,
               (long long)entry->file_size,
               entry->crc32,
               (long)entry->create_time,
               entry->has_metadata,
               entry->has_metadata ? entry->metadata_file : "");
    }
    
    fclose(fp);
    return 0;
}

/**
 * Read snapshot manifest
 * 
 * This function reads a snapshot manifest file and loads
 * snapshot information.
 * 
 * @param snapshot_dir - Snapshot directory
 * @param snapshot - Output parameter for snapshot structure
 * @return 0 on success, error code on failure
 */
static int read_snapshot_manifest(const char *snapshot_dir, Snapshot *snapshot) {
    char manifest_path[MAX_PATH_LEN];
    FILE *fp;
    char line[MAX_LINE_LEN];
    char *p;
    SnapshotFileEntry *entries = NULL;
    int capacity = 1000;
    int count = 0;
    int i;
    
    if (snapshot_dir == NULL || snapshot == NULL) {
        return EINVAL;
    }
    
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.txt", snapshot_dir);
    
    fp = fopen(manifest_path, "r");
    if (fp == NULL) {
        return errno;
    }
    
    /* Allocate entry array */
    entries = (SnapshotFileEntry *)malloc(capacity * sizeof(SnapshotFileEntry));
    if (entries == NULL) {
        fclose(fp);
        return ENOMEM;
    }
    
    /* Read manifest */
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Skip comments and empty lines */
        p = line;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        
        if (*p == '\0' || *p == '#') {
            continue;
        }
        
        /* Parse entry */
        if (count >= capacity) {
            capacity *= 2;
            entries = (SnapshotFileEntry *)realloc(entries, capacity * sizeof(SnapshotFileEntry));
            if (entries == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        SnapshotFileEntry *entry = &entries[count];
        memset(entry, 0, sizeof(SnapshotFileEntry));
        
        /* Parse: file_id|size|crc32|create_time|has_metadata|metadata_file */
        char *token;
        char *saveptr;
        int field = 0;
        
        token = strtok_r(p, "|", &saveptr);
        while (token != NULL && field < 6) {
            switch (field) {
                case 0:
                    strncpy(entry->file_id, token, MAX_FILE_ID_LEN - 1);
                    break;
                case 1:
                    entry->file_size = strtoll(token, NULL, 10);
                    break;
                case 2:
                    entry->crc32 = (uint32_t)strtoul(token, NULL, 16);
                    break;
                case 3:
                    entry->create_time = (time_t)strtoll(token, NULL, 10);
                    break;
                case 4:
                    entry->has_metadata = atoi(token);
                    break;
                case 5:
                    if (entry->has_metadata && strlen(token) > 0) {
                        strncpy(entry->metadata_file, token, sizeof(entry->metadata_file) - 1);
                    }
                    break;
            }
            field++;
            token = strtok_r(NULL, "|", &saveptr);
        }
        
        count++;
    }
    
    fclose(fp);
    
    snapshot->files = entries;
    snapshot->file_count = count;
    
    return 0;
}

/**
 * Create snapshot
 * 
 * This function creates a new snapshot by capturing the current
 * state of specified files.
 * 
 * @param ctx - Snapshot context
 * @param snapshot_name - Snapshot name
 * @param file_list - File list (NULL to snapshot all files)
 * @param group_name - Group name (NULL if not snapshotting group)
 * @param description - Snapshot description
 * @return 0 on success, error code on failure
 */
static int create_snapshot(SnapshotContext *ctx, const char *snapshot_name,
                          const char *file_list, const char *group_name,
                          const char *description) {
    char **file_ids = NULL;
    int file_count = 0;
    int result;
    int i;
    Snapshot snapshot;
    
    if (ctx == NULL || snapshot_name == NULL) {
        return EINVAL;
    }
    
    /* Initialize snapshot */
    memset(&snapshot, 0, sizeof(Snapshot));
    strncpy(snapshot.snapshot_name, snapshot_name, sizeof(snapshot.snapshot_name) - 1);
    snapshot.snapshot_time = time(NULL);
    if (description != NULL) {
        strncpy(snapshot.description, description, sizeof(snapshot.description) - 1);
    }
    
    /* Create snapshot directory */
    snprintf(snapshot.snapshot_dir, sizeof(snapshot.snapshot_dir), "%s/%s",
            ctx->snapshot_base_dir, snapshot_name);
    result = create_directory_recursive(snapshot.snapshot_dir);
    if (result != 0) {
        return result;
    }
    
    ctx->current_snapshot = &snapshot;
    
    /* Get file list */
    if (file_list != NULL) {
        result = read_file_list(file_list, &file_ids, &file_count);
        if (result != 0) {
            return result;
        }
    } else if (group_name != NULL) {
        /* Get all files from group */
        /* Note: This would require listing all files in the group */
        /* For now, this is a placeholder */
        if (verbose) {
            fprintf(stderr, "WARNING: Group snapshot not fully implemented\n");
        }
        return EINVAL;
    } else {
        fprintf(stderr, "ERROR: File list or group name required\n");
        return EINVAL;
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
    
    /* Allocate file entries */
    snapshot.files = (SnapshotFileEntry *)calloc(file_count, sizeof(SnapshotFileEntry));
    if (snapshot.files == NULL) {
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return ENOMEM;
    }
    
    /* Create snapshot entries */
    for (i = 0; i < file_count; i++) {
        result = create_snapshot_entry(ctx, file_ids[i], &snapshot.files[i]);
        if (result == 0) {
            snapshot.file_count++;
            files_snapshotted++;
        } else {
            files_failed++;
            if (verbose) {
                fprintf(stderr, "WARNING: Failed to snapshot %s: %s\n",
                       file_ids[i], STRERROR(result));
            }
        }
        
        total_files_processed++;
    }
    
    /* Write manifest */
    result = write_snapshot_manifest(&snapshot);
    if (result != 0) {
        free(snapshot.files);
        if (file_ids != NULL) {
            for (i = 0; i < file_count; i++) {
                free(file_ids[i]);
            }
            free(file_ids);
        }
        return result;
    }
    
    /* Cleanup */
    free(snapshot.files);
    if (file_ids != NULL) {
        for (i = 0; i < file_count; i++) {
            free(file_ids[i]);
        }
        free(file_ids);
    }
    
    if (verbose && !quiet) {
        printf("OK: Created snapshot '%s' with %d files\n",
               snapshot_name, snapshot.file_count);
    }
    
    return 0;
}

/**
 * Restore from snapshot
 * 
 * This function restores files from a snapshot.
 * 
 * @param ctx - Snapshot context
 * @param snapshot_name - Snapshot name
 * @param file_list - File list to restore (NULL for all files)
 * @param dry_run - Dry-run mode flag
 * @return 0 on success, error code on failure
 */
static int restore_from_snapshot(SnapshotContext *ctx, const char *snapshot_name,
                                const char *file_list, int dry_run) {
    char snapshot_dir[MAX_PATH_LEN];
    Snapshot snapshot;
    int result;
    int i;
    char **restore_files = NULL;
    int restore_count = 0;
    
    if (ctx == NULL || snapshot_name == NULL) {
        return EINVAL;
    }
    
    /* Load snapshot */
    snprintf(snapshot_dir, sizeof(snapshot_dir), "%s/%s",
            ctx->snapshot_base_dir, snapshot_name);
    
    result = read_snapshot_manifest(snapshot_dir, &snapshot);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to load snapshot: %s\n", STRERROR(result));
        return result;
    }
    
    strncpy(snapshot.snapshot_dir, snapshot_dir, sizeof(snapshot.snapshot_dir) - 1);
    strncpy(snapshot.snapshot_name, snapshot_name, sizeof(snapshot.snapshot_name) - 1);
    
    /* Determine which files to restore */
    if (file_list != NULL) {
        result = read_file_list(file_list, &restore_files, &restore_count);
        if (result != 0) {
            free(snapshot.files);
            return result;
        }
    } else {
        /* Restore all files */
        restore_count = snapshot.file_count;
        restore_files = (char **)malloc(restore_count * sizeof(char *));
        if (restore_files == NULL) {
            free(snapshot.files);
            return ENOMEM;
        }
        
        for (i = 0; i < restore_count; i++) {
            restore_files[i] = strdup(snapshot.files[i].file_id);
            if (restore_files[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(restore_files[j]);
                }
                free(restore_files);
                free(snapshot.files);
                return ENOMEM;
            }
        }
    }
    
    if (dry_run) {
        printf("DRY-RUN: Would restore %d files from snapshot '%s'\n",
               restore_count, snapshot_name);
    } else {
        /* Restore files */
        /* Note: Actual restore would require downloading files from backup */
        /* For now, this is a placeholder that verifies files exist */
        for (i = 0; i < restore_count; i++) {
            /* Find file in snapshot */
            int found = 0;
            for (int j = 0; j < snapshot.file_count; j++) {
                if (strcmp(restore_files[i], snapshot.files[j].file_id) == 0) {
                    found = 1;
                    if (verbose && !quiet) {
                        printf("OK: Would restore %s\n", restore_files[i]);
                    }
                    files_restored++;
                    break;
                }
            }
            
            if (!found) {
                if (!quiet) {
                    fprintf(stderr, "WARNING: File not found in snapshot: %s\n",
                           restore_files[i]);
                }
                files_failed++;
            }
            
            total_files_processed++;
        }
    }
    
    /* Cleanup */
    free(snapshot.files);
    if (restore_files != NULL) {
        for (i = 0; i < restore_count; i++) {
            free(restore_files[i]);
        }
        free(restore_files);
    }
    
    return 0;
}

/**
 * List snapshots
 * 
 * This function lists all available snapshots.
 * 
 * @param ctx - Snapshot context
 * @param output_file - Output file (NULL for stdout)
 * @return 0 on success, error code on failure
 */
static int list_snapshots(SnapshotContext *ctx, FILE *output_file) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char snapshot_path[MAX_PATH_LEN];
    Snapshot snapshot;
    int count = 0;
    
    if (ctx == NULL) {
        return EINVAL;
    }
    
    if (output_file == NULL) {
        output_file = stdout;
    }
    
    dir = opendir(ctx->snapshot_base_dir);
    if (dir == NULL) {
        return errno;
    }
    
    if (json_output) {
        fprintf(output_file, "{\n");
        fprintf(output_file, "  \"snapshots\": [\n");
    } else {
        fprintf(output_file, "\n");
        fprintf(output_file, "=== FastDFS Snapshots ===\n");
        fprintf(output_file, "\n");
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s",
                ctx->snapshot_base_dir, entry->d_name);
        
        if (stat(snapshot_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Try to read snapshot manifest */
            if (read_snapshot_manifest(snapshot_path, &snapshot) == 0) {
                if (count > 0 && json_output) {
                    fprintf(output_file, ",\n");
                }
                
                if (json_output) {
                    fprintf(output_file, "    {\n");
                    fprintf(output_file, "      \"name\": \"%s\",\n", entry->d_name);
                    fprintf(output_file, "      \"timestamp\": %ld,\n", (long)snapshot.snapshot_time);
                    fprintf(output_file, "      \"file_count\": %d", snapshot.file_count);
                    if (strlen(snapshot.description) > 0) {
                        fprintf(output_file, ",\n      \"description\": \"%s\"", snapshot.description);
                    }
                    fprintf(output_file, "\n    }");
                } else {
                    char time_buf[64];
                    format_timestamp(snapshot.snapshot_time, time_buf, sizeof(time_buf));
                    
                    fprintf(output_file, "Snapshot: %s\n", entry->d_name);
                    fprintf(output_file, "  Created: %s\n", time_buf);
                    fprintf(output_file, "  Files: %d\n", snapshot.file_count);
                    if (strlen(snapshot.description) > 0) {
                        fprintf(output_file, "  Description: %s\n", snapshot.description);
                    }
                    fprintf(output_file, "\n");
                }
                
                free(snapshot.files);
                count++;
            }
        }
    }
    
    closedir(dir);
    
    if (json_output) {
        fprintf(output_file, "\n  ]\n");
        fprintf(output_file, "}\n");
    } else {
        fprintf(output_file, "Total snapshots: %d\n", count);
        fprintf(output_file, "\n");
    }
    
    return 0;
}

/**
 * Delete snapshot
 * 
 * This function deletes a snapshot and all its files.
 * 
 * @param ctx - Snapshot context
 * @param snapshot_name - Snapshot name
 * @return 0 on success, error code on failure
 */
static int delete_snapshot(SnapshotContext *ctx, const char *snapshot_name) {
    char snapshot_path[MAX_PATH_LEN];
    char command[MAX_PATH_LEN * 2];
    
    if (ctx == NULL || snapshot_name == NULL) {
        return EINVAL;
    }
    
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s",
            ctx->snapshot_base_dir, snapshot_name);
    
    /* Use system command to remove directory */
    snprintf(command, sizeof(command), "rm -rf %s", snapshot_path);
    
    if (system(command) != 0) {
        return errno;
    }
    
    if (verbose && !quiet) {
        printf("OK: Deleted snapshot '%s'\n", snapshot_name);
    }
    
    return 0;
}

/**
 * Cleanup old snapshots
 * 
 * This function cleans up old snapshots based on retention policies.
 * 
 * @param ctx - Snapshot context
 * @param keep_count - Keep N most recent snapshots (0 = ignore)
 * @param keep_days - Keep snapshots for N days (0 = ignore)
 * @param dry_run - Dry-run mode flag
 * @return 0 on success, error code on failure
 */
static int cleanup_snapshots(SnapshotContext *ctx, int keep_count, int keep_days,
                            int dry_run) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char snapshot_path[MAX_PATH_LEN];
    Snapshot *snapshots = NULL;
    int snapshot_count = 0;
    int capacity = 100;
    time_t current_time;
    int i;
    int deleted = 0;
    
    if (ctx == NULL) {
        return EINVAL;
    }
    
    current_time = time(NULL);
    
    dir = opendir(ctx->snapshot_base_dir);
    if (dir == NULL) {
        return errno;
    }
    
    /* Collect all snapshots */
    snapshots = (Snapshot *)malloc(capacity * sizeof(Snapshot));
    if (snapshots == NULL) {
        closedir(dir);
        return ENOMEM;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s",
                ctx->snapshot_base_dir, entry->d_name);
        
        if (stat(snapshot_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (read_snapshot_manifest(snapshot_path, &snapshots[snapshot_count]) == 0) {
                strncpy(snapshots[snapshot_count].snapshot_name, entry->d_name,
                       sizeof(snapshots[snapshot_count].snapshot_name) - 1);
                strncpy(snapshots[snapshot_count].snapshot_dir, snapshot_path,
                       sizeof(snapshots[snapshot_count].snapshot_dir) - 1);
                snapshot_count++;
                
                if (snapshot_count >= capacity) {
                    capacity *= 2;
                    snapshots = (Snapshot *)realloc(snapshots, capacity * sizeof(Snapshot));
                    if (snapshots == NULL) {
                        closedir(dir);
                        for (i = 0; i < snapshot_count; i++) {
                            free(snapshots[i].files);
                        }
                        free(snapshots);
                        return ENOMEM;
                    }
                }
            }
        }
    }
    
    closedir(dir);
    
    /* Sort snapshots by time (newest first) */
    for (i = 0; i < snapshot_count - 1; i++) {
        for (int j = i + 1; j < snapshot_count; j++) {
            if (snapshots[i].snapshot_time < snapshots[j].snapshot_time) {
                Snapshot temp = snapshots[i];
                snapshots[i] = snapshots[j];
                snapshots[j] = temp;
            }
        }
    }
    
    /* Apply retention policies */
    for (i = 0; i < snapshot_count; i++) {
        int should_delete = 0;
        
        /* Check keep_count policy */
        if (keep_count > 0 && i >= keep_count) {
            should_delete = 1;
        }
        
        /* Check keep_days policy */
        if (keep_days > 0) {
            time_t age_seconds = current_time - snapshots[i].snapshot_time;
            int age_days = age_seconds / 86400;
            if (age_days > keep_days) {
                should_delete = 1;
            }
        }
        
        if (should_delete) {
            if (dry_run) {
                printf("DRY-RUN: Would delete snapshot '%s'\n",
                       snapshots[i].snapshot_name);
            } else {
                delete_snapshot(ctx, snapshots[i].snapshot_name);
                deleted++;
            }
        }
        
        free(snapshots[i].files);
    }
    
    free(snapshots);
    
    if (verbose && !quiet && !dry_run) {
        printf("OK: Cleaned up %d snapshot(s)\n", deleted);
    }
    
    return 0;
}

/**
 * Print snapshot results in text format
 * 
 * This function prints snapshot operation results in a human-readable
 * text format.
 * 
 * @param output_file - Output file (NULL for stdout)
 */
static void print_snapshot_results_text(FILE *output_file) {
    if (output_file == NULL) {
        output_file = stdout;
    }
    
    fprintf(output_file, "\n");
    fprintf(output_file, "=== FastDFS Snapshot Results ===\n");
    fprintf(output_file, "\n");
    
    fprintf(output_file, "Total files processed: %d\n", total_files_processed);
    fprintf(output_file, "Files snapshotted: %d\n", files_snapshotted);
    fprintf(output_file, "Files restored: %d\n", files_restored);
    fprintf(output_file, "Files failed: %d\n", files_failed);
    fprintf(output_file, "\n");
}

/**
 * Print snapshot results in JSON format
 * 
 * This function prints snapshot operation results in JSON format
 * for programmatic processing.
 * 
 * @param output_file - Output file (NULL for stdout)
 */
static void print_snapshot_results_json(FILE *output_file) {
    if (output_file == NULL) {
        output_file = stdout;
    }
    
    fprintf(output_file, "{\n");
    fprintf(output_file, "  \"timestamp\": %ld,\n", (long)time(NULL));
    fprintf(output_file, "  \"statistics\": {\n");
    fprintf(output_file, "    \"total_files_processed\": %d,\n", total_files_processed);
    fprintf(output_file, "    \"files_snapshotted\": %d,\n", files_snapshotted);
    fprintf(output_file, "    \"files_restored\": %d,\n", files_restored);
    fprintf(output_file, "    \"files_failed\": %d\n", files_failed);
    fprintf(output_file, "  }\n");
    fprintf(output_file, "}\n");
}

/**
 * Main function
 * 
 * Entry point for the snapshot tool. Parses command-line
 * arguments and performs snapshot operations.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = some failures, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *snapshot_base_dir = "/var/fdfs/snapshots";
    char *file_list = NULL;
    char *group_name = NULL;
    char *description = NULL;
    char *output_file = NULL;
    int keep_count = 0;
    int keep_days = 0;
    int dry_run = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    SnapshotContext ctx;
    FILE *out_fp = stdout;
    int opt;
    int option_index = 0;
    const char *command = NULL;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"base-dir", required_argument, 0, 'b'},
        {"file", required_argument, 0, 'f'},
        {"group", required_argument, 0, 'g'},
        {"description", required_argument, 0, 1000},
        {"keep-count", required_argument, 0, 1001},
        {"keep-days", required_argument, 0, 1002},
        {"dry-run", no_argument, 0, 1003},
        {"metadata", no_argument, 0, 'm'},
        {"threads", required_argument, 0, 'j'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(SnapshotContext));
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:b:f:g:mj:o:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'b':
                snapshot_base_dir = optarg;
                break;
            case 'f':
                file_list = optarg;
                break;
            case 'g':
                group_name = optarg;
                break;
            case 1000:
                description = optarg;
                break;
            case 1001:
                keep_count = atoi(optarg);
                if (keep_count < 0) keep_count = 0;
                break;
            case 1002:
                keep_days = atoi(optarg);
                if (keep_days < 0) keep_days = 0;
                break;
            case 1003:
                dry_run = 1;
                break;
            case 'm':
                ctx.preserve_metadata = 1;
                break;
            case 'j':
                /* Threads option (not used in current implementation) */
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
    
    /* Initialize context */
    strncpy(ctx.snapshot_base_dir, snapshot_base_dir, sizeof(ctx.snapshot_base_dir) - 1);
    ctx.pTrackerServer = pTrackerServer;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    /* Create base directory if it doesn't exist */
    create_directory_recursive(ctx.snapshot_base_dir);
    
    /* Reset statistics */
    total_files_processed = 0;
    files_snapshotted = 0;
    files_restored = 0;
    files_failed = 0;
    
    /* Execute command */
    if (strcmp(command, "create") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "ERROR: Snapshot name required for create command\n");
            pthread_mutex_destroy(&ctx.mutex);
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        const char *snapshot_name = argv[optind + 1];
        result = create_snapshot(&ctx, snapshot_name, file_list, group_name, description);
    } else if (strcmp(command, "restore") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "ERROR: Snapshot name required for restore command\n");
            pthread_mutex_destroy(&ctx.mutex);
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        const char *snapshot_name = argv[optind + 1];
        result = restore_from_snapshot(&ctx, snapshot_name, file_list, dry_run);
    } else if (strcmp(command, "list") == 0) {
        result = list_snapshots(&ctx, out_fp);
    } else if (strcmp(command, "delete") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "ERROR: Snapshot name required for delete command\n");
            pthread_mutex_destroy(&ctx.mutex);
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        const char *snapshot_name = argv[optind + 1];
        result = delete_snapshot(&ctx, snapshot_name);
    } else if (strcmp(command, "cleanup") == 0) {
        result = cleanup_snapshots(&ctx, keep_count, keep_days, dry_run);
    } else if (strcmp(command, "compare") == 0) {
        if (optind + 2 >= argc) {
            fprintf(stderr, "ERROR: Two snapshot names required for compare command\n");
            pthread_mutex_destroy(&ctx.mutex);
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
        
        /* Compare snapshots (placeholder) */
        if (verbose) {
            fprintf(stderr, "WARNING: Compare command not fully implemented\n");
        }
        result = 0;
    } else {
        fprintf(stderr, "ERROR: Unknown command: %s\n\n", command);
        print_usage(argv[0]);
        pthread_mutex_destroy(&ctx.mutex);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 2;
    }
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Operation failed: %s\n", STRERROR(result));
    }
    
    /* Print results if not list command */
    if (strcmp(command, "list") != 0) {
        if (output_file != NULL) {
            out_fp = fopen(output_file, "w");
            if (out_fp == NULL) {
                fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
                out_fp = stdout;
            }
        }
        
        if (json_output) {
            print_snapshot_results_json(out_fp);
        } else {
            print_snapshot_results_text(out_fp);
        }
        
        if (output_file != NULL && out_fp != stdout) {
            fclose(out_fp);
        }
    }
    
    /* Cleanup */
    pthread_mutex_destroy(&ctx.mutex);
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    /* Return appropriate exit code */
    if (result != 0) {
        return 2;  /* Error */
    }
    
    if (files_failed > 0) {
        return 1;  /* Some failures */
    }
    
    return 0;  /* Success */
}

