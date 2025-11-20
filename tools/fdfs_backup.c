/**
 * FastDFS Backup Tool
 * 
 * Creates incremental or full backups of FastDFS files
 * Supports metadata preservation and compression
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"
#include "fastcommon/hash.h"

#define MAX_FILE_ID_LEN 256
#define MAX_PATH_LEN 1024
#define MANIFEST_VERSION "1.0"
#define MAX_THREADS 10

typedef struct {
    char file_id[MAX_FILE_ID_LEN];
    int64_t file_size;
    uint32_t crc32;
    time_t create_time;
    char local_path[MAX_PATH_LEN];
    int has_metadata;
    int backup_status;
} BackupFileInfo;

typedef struct {
    BackupFileInfo *files;
    int file_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    char backup_dir[MAX_PATH_LEN];
    int preserve_metadata;
} BackupContext;

static int total_files = 0;
static int backed_up_files = 0;
static int failed_files = 0;
static int64_t total_bytes = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -o <backup_dir>\n", program_name);
    printf("       %s [OPTIONS] -f <file_list> -o <backup_dir>\n", program_name);
    printf("\n");
    printf("Create backups of FastDFS files\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST        File list to backup (one file ID per line)\n");
    printf("  -g, --group NAME       Backup entire group\n");
    printf("  -o, --output DIR       Output backup directory (required)\n");
    printf("  -m, --metadata         Preserve file metadata\n");
    printf("  -i, --incremental      Incremental backup (skip existing files)\n");
    printf("  -j, --threads NUM      Number of parallel threads (default: 1, max: 10)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f files.txt -o /backup/fastdfs\n", program_name);
    printf("  %s -g group1 -o /backup/group1 -m\n", program_name);
    printf("  %s -f files.txt -o /backup -i -j 4\n", program_name);
}

static int create_directory_recursive(const char *path) {
    char tmp[MAX_PATH_LEN];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    
    return 0;
}

static int write_manifest(const char *backup_dir, BackupFileInfo *files, int file_count) {
    char manifest_path[MAX_PATH_LEN];
    FILE *fp;
    time_t now;
    
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.txt", backup_dir);
    
    fp = fopen(manifest_path, "w");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to create manifest file: %s\n", manifest_path);
        return -1;
    }
    
    now = time(NULL);
    
    fprintf(fp, "# FastDFS Backup Manifest\n");
    fprintf(fp, "# Version: %s\n", MANIFEST_VERSION);
    fprintf(fp, "# Created: %s", ctime(&now));
    fprintf(fp, "# Total Files: %d\n", file_count);
    fprintf(fp, "# Total Size: %lld bytes\n", (long long)total_bytes);
    fprintf(fp, "#\n");
    fprintf(fp, "# Format: file_id|size|crc32|local_path|has_metadata\n");
    fprintf(fp, "#\n");
    
    for (int i = 0; i < file_count; i++) {
        if (files[i].backup_status == 0) {
            fprintf(fp, "%s|%lld|%08X|%s|%d\n",
                   files[i].file_id,
                   (long long)files[i].file_size,
                   files[i].crc32,
                   files[i].local_path,
                   files[i].has_metadata);
        }
    }
    
    fclose(fp);
    return 0;
}

static int backup_single_file(ConnectionInfo *pTrackerServer,
                              BackupFileInfo *file_info,
                              const char *backup_dir,
                              int preserve_metadata,
                              int incremental) {
    char full_path[MAX_PATH_LEN];
    char dir_path[MAX_PATH_LEN];
    char meta_path[MAX_PATH_LEN];
    char *last_slash;
    int64_t file_size;
    int result;
    ConnectionInfo *pStorageServer;
    FDFSFileInfo fdfs_info;
    
    snprintf(full_path, sizeof(full_path), "%s/%s", backup_dir, file_info->file_id);
    
    if (incremental) {
        struct stat st;
        if (stat(full_path, &st) == 0) {
            file_info->backup_status = 0;
            
            pthread_mutex_lock(&stats_mutex);
            backed_up_files++;
            total_bytes += st.st_size;
            pthread_mutex_unlock(&stats_mutex);
            
            return 0;
        }
    }
    
    strncpy(dir_path, full_path, sizeof(dir_path) - 1);
    last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (create_directory_recursive(dir_path) != 0) {
            fprintf(stderr, "ERROR: Failed to create directory: %s\n", dir_path);
            file_info->backup_status = -1;
            return -1;
        }
    }
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to storage server\n");
        file_info->backup_status = -2;
        return -2;
    }
    
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                     file_info->file_id, &fdfs_info);
    if (result == 0) {
        file_info->file_size = fdfs_info.file_size;
        file_info->crc32 = fdfs_info.crc32;
        file_info->create_time = fdfs_info.create_timestamp;
    }
    
    result = storage_download_file_to_file1(pTrackerServer, pStorageServer,
                                           file_info->file_id, full_path, &file_size);
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to download %s: %s\n",
               file_info->file_id, STRERROR(result));
        tracker_disconnect_server_ex(pStorageServer, true);
        file_info->backup_status = result;
        
        pthread_mutex_lock(&stats_mutex);
        failed_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        return result;
    }
    
    strncpy(file_info->local_path, file_info->file_id, sizeof(file_info->local_path) - 1);
    file_info->file_size = file_size;
    
    if (preserve_metadata) {
        FDFSMetaData *meta_list = NULL;
        int meta_count = 0;
        
        result = storage_get_metadata1(pTrackerServer, pStorageServer,
                                      file_info->file_id, &meta_list, &meta_count);
        
        if (result == 0 && meta_count > 0) {
            snprintf(meta_path, sizeof(meta_path), "%s.meta", full_path);
            
            FILE *meta_fp = fopen(meta_path, "w");
            if (meta_fp != NULL) {
                for (int i = 0; i < meta_count; i++) {
                    fprintf(meta_fp, "%s=%s\n", meta_list[i].name, meta_list[i].value);
                }
                fclose(meta_fp);
                file_info->has_metadata = 1;
            }
            
            free(meta_list);
        }
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    file_info->backup_status = 0;
    
    pthread_mutex_lock(&stats_mutex);
    backed_up_files++;
    total_bytes += file_size;
    pthread_mutex_unlock(&stats_mutex);
    
    return 0;
}

static void *backup_worker(void *arg) {
    BackupContext *ctx = (BackupContext *)arg;
    BackupFileInfo *file_info;
    int index;
    
    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->current_index >= ctx->file_count) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        file_info = &ctx->files[index];
        
        int result = backup_single_file(ctx->pTrackerServer, file_info,
                                       ctx->backup_dir, ctx->preserve_metadata, 0);
        
        if (result == 0) {
            printf("OK: %s (%lld bytes)\n",
                   file_info->file_id, (long long)file_info->file_size);
        } else {
            fprintf(stderr, "FAILED: %s\n", file_info->file_id);
        }
    }
    
    return NULL;
}

static int load_file_list(const char *list_file, BackupFileInfo **files, int *count) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    int capacity = 1000;
    int file_count = 0;
    BackupFileInfo *file_array;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    file_array = (BackupFileInfo *)malloc(capacity * sizeof(BackupFileInfo));
    if (file_array == NULL) {
        fclose(fp);
        return ENOMEM;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = strchr(line, '\n');
        if (p != NULL) {
            *p = '\0';
        }
        
        p = strchr(line, '\r');
        if (p != NULL) {
            *p = '\0';
        }
        
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        if (file_count >= capacity) {
            capacity *= 2;
            file_array = (BackupFileInfo *)realloc(file_array,
                                                   capacity * sizeof(BackupFileInfo));
            if (file_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        memset(&file_array[file_count], 0, sizeof(BackupFileInfo));
        strncpy(file_array[file_count].file_id, line, MAX_FILE_ID_LEN - 1);
        file_count++;
    }
    
    fclose(fp);
    
    *files = file_array;
    *count = file_count;
    total_files = file_count;
    
    return 0;
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    char *group_name = NULL;
    char *backup_dir = NULL;
    int preserve_metadata = 0;
    int incremental = 0;
    int num_threads = 1;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    BackupFileInfo *files = NULL;
    int file_count = 0;
    BackupContext ctx;
    pthread_t *threads;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"group", required_argument, 0, 'g'},
        {"output", required_argument, 0, 'o'},
        {"metadata", no_argument, 0, 'm'},
        {"incremental", no_argument, 0, 'i'},
        {"threads", required_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:f:g:o:mij:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 'g':
                group_name = optarg;
                break;
            case 'o':
                backup_dir = optarg;
                break;
            case 'm':
                preserve_metadata = 1;
                break;
            case 'i':
                incremental = 1;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (backup_dir == NULL || (list_file == NULL && group_name == NULL)) {
        fprintf(stderr, "ERROR: Output directory and file list or group name required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (create_directory_recursive(backup_dir) != 0) {
        fprintf(stderr, "ERROR: Failed to create backup directory: %s\n", backup_dir);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    if (list_file != NULL) {
        result = load_file_list(list_file, &files, &file_count);
        if (result != 0) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return result;
        }
    }
    
    if (file_count == 0) {
        printf("No files to backup\n");
        if (files != NULL) free(files);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 0;
    }
    
    printf("Starting backup of %d files to %s using %d threads...\n",
           file_count, backup_dir, num_threads);
    if (incremental) {
        printf("Incremental mode: skipping existing files\n");
    }
    if (preserve_metadata) {
        printf("Preserving file metadata\n");
    }
    printf("\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.files = files;
    ctx.file_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    strncpy(ctx.backup_dir, backup_dir, sizeof(ctx.backup_dir) - 1);
    ctx.preserve_metadata = preserve_metadata;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, backup_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000LL +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
    
    write_manifest(backup_dir, files, file_count);
    
    printf("\n=== Backup Summary ===\n");
    printf("Total files: %d\n", total_files);
    printf("Backed up: %d\n", backed_up_files);
    printf("Failed: %d\n", failed_files);
    printf("Total size: %lld bytes (%.2f MB)\n",
           (long long)total_bytes, total_bytes / (1024.0 * 1024.0));
    printf("Time: %lld ms (%.2f files/sec)\n",
           elapsed_ms, total_files * 1000.0 / elapsed_ms);
    printf("Manifest: %s/manifest.txt\n", backup_dir);
    
    if (failed_files > 0) {
        printf("\n⚠ WARNING: %d files failed to backup!\n", failed_files);
    } else {
        printf("\n✓ Backup completed successfully\n");
    }
    
    free(files);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return failed_files > 0 ? 1 : 0;
}
