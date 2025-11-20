/**
 * FastDFS File Migration Tool
 * 
 * Migrates files between FastDFS groups or servers
 * Useful for load balancing, server maintenance, or data reorganization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"

#define MAX_FILE_ID_LEN 256
#define MAX_GROUP_NAME_LEN 32
#define BUFFER_SIZE (256 * 1024)
#define MAX_THREADS 10

typedef struct {
    char source_file_id[MAX_FILE_ID_LEN];
    char dest_file_id[MAX_FILE_ID_LEN];
    char target_group[MAX_GROUP_NAME_LEN];
    int64_t file_size;
    int status;
    char error_msg[256];
} MigrationTask;

typedef struct {
    MigrationTask *tasks;
    int task_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    int delete_source;
    int preserve_metadata;
} MigrationContext;

static int total_files = 0;
static int migrated_files = 0;
static int failed_files = 0;
static int64_t total_bytes = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -s <source_group> -t <target_group>\n", program_name);
    printf("       %s [OPTIONS] -f <file_list> -t <target_group>\n", program_name);
    printf("\n");
    printf("Migrate files between FastDFS groups\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -s, --source GROUP     Source group name\n");
    printf("  -t, --target GROUP     Target group name (required)\n");
    printf("  -f, --file LIST        File list to migrate (one file ID per line)\n");
    printf("  -d, --delete           Delete source files after successful migration\n");
    printf("  -m, --metadata         Preserve file metadata\n");
    printf("  -j, --threads NUM      Number of parallel threads (default: 1, max: 10)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -s group1 -t group2 -f files.txt\n", program_name);
    printf("  %s -t group2 -f files.txt -d -m\n", program_name);
    printf("  %s -s group1 -t group2 -f files.txt -j 4\n", program_name);
}

static int migrate_single_file(ConnectionInfo *pTrackerServer,
                               MigrationTask *task,
                               int delete_source,
                               int preserve_metadata) {
    char local_file[256];
    int64_t file_size;
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    int result;
    ConnectionInfo *pStorageServer;
    
    snprintf(local_file, sizeof(local_file), "/tmp/fdfs_migrate_%d_%ld.tmp",
             getpid(), (long)pthread_self());
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Failed to connect to storage server");
        return -1;
    }
    
    result = storage_download_file_to_file1(pTrackerServer, pStorageServer,
                                           task->source_file_id, local_file, &file_size);
    if (result != 0) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Failed to download: %s", STRERROR(result));
        tracker_disconnect_server_ex(pStorageServer, true);
        return result;
    }
    
    task->file_size = file_size;
    
    if (preserve_metadata) {
        result = storage_get_metadata1(pTrackerServer, pStorageServer,
                                      task->source_file_id, &meta_list, &meta_count);
        if (result != 0 && result != ENOENT) {
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Failed to get metadata: %s", STRERROR(result));
            unlink(local_file);
            tracker_disconnect_server_ex(pStorageServer, true);
            return result;
        }
    }
    
    if (strlen(task->target_group) > 0) {
        result = storage_upload_by_filename1_ex(pTrackerServer, pStorageServer,
                                               local_file, NULL,
                                               meta_list, meta_count,
                                               task->target_group,
                                               task->dest_file_id);
    } else {
        result = upload_file(pTrackerServer, pStorageServer, local_file,
                           task->dest_file_id, sizeof(task->dest_file_id));
    }
    
    if (result != 0) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Failed to upload to target: %s", STRERROR(result));
        unlink(local_file);
        if (meta_list != NULL) {
            free(meta_list);
        }
        tracker_disconnect_server_ex(pStorageServer, true);
        return result;
    }
    
    if (delete_source) {
        result = storage_delete_file1(pTrackerServer, pStorageServer,
                                     task->source_file_id);
        if (result != 0) {
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Warning: Failed to delete source file: %s", STRERROR(result));
        }
    }
    
    unlink(local_file);
    if (meta_list != NULL) {
        free(meta_list);
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    pthread_mutex_lock(&stats_mutex);
    migrated_files++;
    total_bytes += file_size;
    pthread_mutex_unlock(&stats_mutex);
    
    task->status = 0;
    return 0;
}

static void *migration_worker(void *arg) {
    MigrationContext *ctx = (MigrationContext *)arg;
    MigrationTask *task;
    int index;
    
    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->current_index >= ctx->task_count) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        task = &ctx->tasks[index];
        
        int result = migrate_single_file(ctx->pTrackerServer, task,
                                        ctx->delete_source, ctx->preserve_metadata);
        
        if (result != 0) {
            pthread_mutex_lock(&stats_mutex);
            failed_files++;
            pthread_mutex_unlock(&stats_mutex);
            
            fprintf(stderr, "ERROR: Migration failed for %s: %s\n",
                   task->source_file_id, task->error_msg);
        } else {
            printf("OK: %s -> %s (%lld bytes)\n",
                   task->source_file_id, task->dest_file_id,
                   (long long)task->file_size);
        }
    }
    
    return NULL;
}

static int load_file_list(const char *list_file, MigrationTask **tasks, int *count) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    int capacity = 1000;
    int task_count = 0;
    MigrationTask *task_array;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    task_array = (MigrationTask *)malloc(capacity * sizeof(MigrationTask));
    if (task_array == NULL) {
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
        
        if (task_count >= capacity) {
            capacity *= 2;
            task_array = (MigrationTask *)realloc(task_array,
                                                  capacity * sizeof(MigrationTask));
            if (task_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        memset(&task_array[task_count], 0, sizeof(MigrationTask));
        strncpy(task_array[task_count].source_file_id, line, MAX_FILE_ID_LEN - 1);
        task_count++;
    }
    
    fclose(fp);
    
    *tasks = task_array;
    *count = task_count;
    total_files = task_count;
    
    return 0;
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *source_group = NULL;
    char *target_group = NULL;
    char *list_file = NULL;
    int delete_source = 0;
    int preserve_metadata = 0;
    int num_threads = 1;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    MigrationTask *tasks = NULL;
    int task_count = 0;
    MigrationContext ctx;
    pthread_t *threads;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"source", required_argument, 0, 's'},
        {"target", required_argument, 0, 't'},
        {"file", required_argument, 0, 'f'},
        {"delete", no_argument, 0, 'd'},
        {"metadata", no_argument, 0, 'm'},
        {"threads", required_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:s:t:f:dmj:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 's':
                source_group = optarg;
                break;
            case 't':
                target_group = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 'd':
                delete_source = 1;
                break;
            case 'm':
                preserve_metadata = 1;
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
    
    if (target_group == NULL || list_file == NULL) {
        fprintf(stderr, "ERROR: Target group and file list are required\n\n");
        print_usage(argv[0]);
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
    
    result = load_file_list(list_file, &tasks, &task_count);
    if (result != 0) {
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    if (task_count == 0) {
        printf("No files to migrate\n");
        free(tasks);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 0;
    }
    
    for (int i = 0; i < task_count; i++) {
        strncpy(tasks[i].target_group, target_group, MAX_GROUP_NAME_LEN - 1);
    }
    
    printf("Starting migration of %d files to %s using %d threads...\n",
           task_count, target_group, num_threads);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.tasks = tasks;
    ctx.task_count = task_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.delete_source = delete_source;
    ctx.preserve_metadata = preserve_metadata;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, migration_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\n=== Migration Summary ===\n");
    printf("Total files: %d\n", total_files);
    printf("Migrated: %d\n", migrated_files);
    printf("Failed: %d\n", failed_files);
    printf("Total bytes: %lld (%.2f MB)\n",
           (long long)total_bytes, total_bytes / (1024.0 * 1024.0));
    
    if (failed_files > 0) {
        printf("\n⚠ WARNING: %d files failed to migrate!\n", failed_files);
    } else {
        printf("\n✓ All files migrated successfully\n");
    }
    
    free(tasks);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return failed_files > 0 ? 1 : 0;
}
