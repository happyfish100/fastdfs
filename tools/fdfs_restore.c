/**
 * FastDFS Restore Tool
 * 
 * Restores files from FastDFS backups
 * Supports metadata restoration and verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"
#include "fastcommon/hash.h"

#define MAX_FILE_ID_LEN 256
#define MAX_PATH_LEN 1024
#define MAX_LINE_LEN 2048
#define MAX_THREADS 10

typedef struct {
    char file_id[MAX_FILE_ID_LEN];
    char local_path[MAX_PATH_LEN];
    int64_t file_size;
    uint32_t expected_crc32;
    int has_metadata;
    char new_file_id[MAX_FILE_ID_LEN];
    int restore_status;
} RestoreFileInfo;

typedef struct {
    RestoreFileInfo *files;
    int file_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    char backup_dir[MAX_PATH_LEN];
    char target_group[FDFS_GROUP_NAME_MAX_LEN + 1];
    int verify_crc;
    int restore_metadata;
} RestoreContext;

static int total_files = 0;
static int restored_files = 0;
static int failed_files = 0;
static int64_t total_bytes = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -i <backup_dir>\n", program_name);
    printf("\n");
    printf("Restore files from FastDFS backup\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -i, --input DIR        Input backup directory (required)\n");
    printf("  -g, --group NAME       Target group (default: original group)\n");
    printf("  -m, --metadata         Restore file metadata\n");
    printf("  -v, --verify           Verify CRC32 after restore\n");
    printf("  -j, --threads NUM      Number of parallel threads (default: 1, max: 10)\n");
    printf("  -d, --dry-run          Dry run (don't actually restore)\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -i /backup/fastdfs\n", program_name);
    printf("  %s -i /backup -g group2 -m -v\n", program_name);
    printf("  %s -i /backup -d\n", program_name);
    printf("  %s -i /backup -j 4 -m\n", program_name);
}

static uint32_t calculate_file_crc32(const char *filename) {
    FILE *fp;
    unsigned char buffer[256 * 1024];
    size_t bytes_read;
    uint32_t crc32 = 0;
    
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return 0;
    }
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        crc32 = CRC32_ex(buffer, bytes_read, crc32);
    }
    
    fclose(fp);
    return crc32;
}

static int parse_manifest(const char *backup_dir, RestoreFileInfo **files, int *count) {
    char manifest_path[MAX_PATH_LEN];
    FILE *fp;
    char line[MAX_LINE_LEN];
    int capacity = 1000;
    int file_count = 0;
    RestoreFileInfo *file_array;
    
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.txt", backup_dir);
    
    fp = fopen(manifest_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open manifest file: %s\n", manifest_path);
        return errno;
    }
    
    file_array = (RestoreFileInfo *)malloc(capacity * sizeof(RestoreFileInfo));
    if (file_array == NULL) {
        fclose(fp);
        return ENOMEM;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '#' || strlen(line) < 10) {
            continue;
        }
        
        if (file_count >= capacity) {
            capacity *= 2;
            file_array = (RestoreFileInfo *)realloc(file_array,
                                                    capacity * sizeof(RestoreFileInfo));
            if (file_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        RestoreFileInfo *info = &file_array[file_count];
        memset(info, 0, sizeof(RestoreFileInfo));
        
        char *token = strtok(line, "|");
        if (token != NULL) {
            strncpy(info->file_id, token, MAX_FILE_ID_LEN - 1);
        }
        
        token = strtok(NULL, "|");
        if (token != NULL) {
            info->file_size = atoll(token);
        }
        
        token = strtok(NULL, "|");
        if (token != NULL) {
            sscanf(token, "%X", &info->expected_crc32);
        }
        
        token = strtok(NULL, "|");
        if (token != NULL) {
            strncpy(info->local_path, token, MAX_PATH_LEN - 1);
        }
        
        token = strtok(NULL, "|\n\r");
        if (token != NULL) {
            info->has_metadata = atoi(token);
        }
        
        file_count++;
    }
    
    fclose(fp);
    
    *files = file_array;
    *count = file_count;
    total_files = file_count;
    
    return 0;
}

static int restore_metadata(ConnectionInfo *pTrackerServer,
                           ConnectionInfo *pStorageServer,
                           const char *file_id,
                           const char *meta_file_path) {
    FILE *fp;
    char line[512];
    FDFSMetaData meta_list[64];
    int meta_count = 0;
    int result;
    
    fp = fopen(meta_file_path, "r");
    if (fp == NULL) {
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL && meta_count < 64) {
        char *equals = strchr(line, '=');
        if (equals == NULL) {
            continue;
        }
        
        *equals = '\0';
        char *value = equals + 1;
        
        char *newline = strchr(value, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }
        
        strncpy(meta_list[meta_count].name, line, FDFS_MAX_META_NAME_LEN - 1);
        strncpy(meta_list[meta_count].value, value, FDFS_MAX_META_VALUE_LEN - 1);
        meta_count++;
    }
    
    fclose(fp);
    
    if (meta_count > 0) {
        result = storage_set_metadata1(pTrackerServer, pStorageServer, file_id,
                                      meta_list, meta_count,
                                      STORAGE_SET_METADATA_FLAG_OVERWRITE);
        return result;
    }
    
    return 0;
}

static int restore_single_file(ConnectionInfo *pTrackerServer,
                               RestoreFileInfo *file_info,
                               const char *backup_dir,
                               const char *target_group,
                               int verify_crc,
                               int restore_metadata_flag,
                               int dry_run) {
    char full_path[MAX_PATH_LEN];
    char meta_path[MAX_PATH_LEN];
    struct stat st;
    int result;
    ConnectionInfo *pStorageServer;
    
    snprintf(full_path, sizeof(full_path), "%s/%s", backup_dir, file_info->local_path);
    
    if (stat(full_path, &st) != 0) {
        fprintf(stderr, "ERROR: Backup file not found: %s\n", full_path);
        file_info->restore_status = -1;
        
        pthread_mutex_lock(&stats_mutex);
        failed_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        return -1;
    }
    
    if (verify_crc) {
        uint32_t actual_crc = calculate_file_crc32(full_path);
        if (actual_crc != file_info->expected_crc32) {
            fprintf(stderr, "ERROR: CRC32 mismatch for %s (expected: %08X, actual: %08X)\n",
                   file_info->file_id, file_info->expected_crc32, actual_crc);
            file_info->restore_status = -2;
            
            pthread_mutex_lock(&stats_mutex);
            failed_files++;
            pthread_mutex_unlock(&stats_mutex);
            
            return -2;
        }
    }
    
    if (dry_run) {
        strncpy(file_info->new_file_id, file_info->file_id, MAX_FILE_ID_LEN - 1);
        file_info->restore_status = 0;
        
        pthread_mutex_lock(&stats_mutex);
        restored_files++;
        total_bytes += st.st_size;
        pthread_mutex_unlock(&stats_mutex);
        
        return 0;
    }
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to storage server\n");
        file_info->restore_status = -3;
        
        pthread_mutex_lock(&stats_mutex);
        failed_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        return -3;
    }
    
    if (target_group != NULL && strlen(target_group) > 0) {
        result = storage_upload_by_filename1_ex(pTrackerServer, pStorageServer,
                                               full_path, NULL, NULL, 0,
                                               target_group, file_info->new_file_id);
    } else {
        result = upload_file(pTrackerServer, pStorageServer, full_path,
                           file_info->new_file_id, sizeof(file_info->new_file_id));
    }
    
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to upload %s: %s\n",
               file_info->file_id, STRERROR(result));
        tracker_disconnect_server_ex(pStorageServer, true);
        file_info->restore_status = result;
        
        pthread_mutex_lock(&stats_mutex);
        failed_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        return result;
    }
    
    if (restore_metadata_flag && file_info->has_metadata) {
        snprintf(meta_path, sizeof(meta_path), "%s.meta", full_path);
        
        if (access(meta_path, F_OK) == 0) {
            result = restore_metadata(pTrackerServer, pStorageServer,
                                    file_info->new_file_id, meta_path);
            if (result != 0) {
                fprintf(stderr, "WARNING: Failed to restore metadata for %s\n",
                       file_info->new_file_id);
            }
        }
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    file_info->restore_status = 0;
    
    pthread_mutex_lock(&stats_mutex);
    restored_files++;
    total_bytes += st.st_size;
    pthread_mutex_unlock(&stats_mutex);
    
    return 0;
}

static void *restore_worker(void *arg) {
    RestoreContext *ctx = (RestoreContext *)arg;
    RestoreFileInfo *file_info;
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
        
        int result = restore_single_file(ctx->pTrackerServer, file_info,
                                        ctx->backup_dir, ctx->target_group,
                                        ctx->verify_crc, ctx->restore_metadata, 0);
        
        if (result == 0) {
            printf("OK: %s -> %s (%lld bytes)\n",
                   file_info->file_id, file_info->new_file_id,
                   (long long)file_info->file_size);
        } else {
            fprintf(stderr, "FAILED: %s\n", file_info->file_id);
        }
    }
    
    return NULL;
}

static int write_restore_log(const char *backup_dir, RestoreFileInfo *files, int file_count) {
    char log_path[MAX_PATH_LEN];
    FILE *fp;
    time_t now;
    
    snprintf(log_path, sizeof(log_path), "%s/restore_log.txt", backup_dir);
    
    fp = fopen(log_path, "w");
    if (fp == NULL) {
        return -1;
    }
    
    now = time(NULL);
    
    fprintf(fp, "# FastDFS Restore Log\n");
    fprintf(fp, "# Restored: %s", ctime(&now));
    fprintf(fp, "# Total Files: %d\n", file_count);
    fprintf(fp, "#\n");
    fprintf(fp, "# Format: original_file_id|new_file_id|status\n");
    fprintf(fp, "#\n");
    
    for (int i = 0; i < file_count; i++) {
        fprintf(fp, "%s|%s|%d\n",
               files[i].file_id,
               files[i].new_file_id,
               files[i].restore_status);
    }
    
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *backup_dir = NULL;
    char *target_group = NULL;
    int verify_crc = 0;
    int restore_metadata_flag = 0;
    int num_threads = 1;
    int dry_run = 0;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    RestoreFileInfo *files = NULL;
    int file_count = 0;
    RestoreContext ctx;
    pthread_t *threads;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"input", required_argument, 0, 'i'},
        {"group", required_argument, 0, 'g'},
        {"metadata", no_argument, 0, 'm'},
        {"verify", no_argument, 0, 'v'},
        {"threads", required_argument, 0, 'j'},
        {"dry-run", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:i:g:mvj:dh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'i':
                backup_dir = optarg;
                break;
            case 'g':
                target_group = optarg;
                break;
            case 'm':
                restore_metadata_flag = 1;
                break;
            case 'v':
                verify_crc = 1;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 'd':
                dry_run = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (backup_dir == NULL) {
        fprintf(stderr, "ERROR: Backup directory required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    result = parse_manifest(backup_dir, &files, &file_count);
    if (result != 0) {
        return result;
    }
    
    if (file_count == 0) {
        printf("No files to restore\n");
        free(files);
        return 0;
    }
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        free(files);
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        free(files);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    printf("Starting restore of %d files from %s using %d threads...\n",
           file_count, backup_dir, num_threads);
    if (target_group != NULL) {
        printf("Target group: %s\n", target_group);
    }
    if (verify_crc) {
        printf("CRC32 verification enabled\n");
    }
    if (restore_metadata_flag) {
        printf("Metadata restoration enabled\n");
    }
    if (dry_run) {
        printf("DRY RUN MODE - No files will be uploaded\n");
    }
    printf("\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.files = files;
    ctx.file_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    strncpy(ctx.backup_dir, backup_dir, sizeof(ctx.backup_dir) - 1);
    if (target_group != NULL) {
        strncpy(ctx.target_group, target_group, sizeof(ctx.target_group) - 1);
    }
    ctx.verify_crc = verify_crc;
    ctx.restore_metadata = restore_metadata_flag;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, restore_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000LL +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
    
    if (!dry_run) {
        write_restore_log(backup_dir, files, file_count);
    }
    
    printf("\n=== Restore Summary ===\n");
    printf("Total files: %d\n", total_files);
    printf("Restored: %d\n", restored_files);
    printf("Failed: %d\n", failed_files);
    printf("Total size: %lld bytes (%.2f MB)\n",
           (long long)total_bytes, total_bytes / (1024.0 * 1024.0));
    printf("Time: %lld ms (%.2f files/sec)\n",
           elapsed_ms, total_files * 1000.0 / elapsed_ms);
    if (!dry_run) {
        printf("Restore log: %s/restore_log.txt\n", backup_dir);
    }
    
    if (failed_files > 0) {
        printf("\n⚠ WARNING: %d files failed to restore!\n", failed_files);
    } else {
        printf("\n✓ Restore completed successfully\n");
    }
    
    free(files);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return failed_files > 0 ? 1 : 0;
}
