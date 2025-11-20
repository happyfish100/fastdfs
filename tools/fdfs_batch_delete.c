/**
 * FastDFS Batch Delete Tool
 * 
 * Efficiently delete multiple files in batch mode
 * Supports parallel deletion and detailed reporting
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"

#define MAX_FILE_ID_LEN 256
#define MAX_THREADS 20

typedef struct {
    char file_id[MAX_FILE_ID_LEN];
    int status;
    char error_msg[256];
    struct timespec start_time;
    struct timespec end_time;
} DeleteTask;

typedef struct {
    DeleteTask *tasks;
    int task_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    int dry_run;
} DeleteContext;

static int total_files = 0;
static int deleted_files = 0;
static int failed_files = 0;
static int skipped_files = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -f <file_list>\n", program_name);
    printf("       %s [OPTIONS] <file_id> [file_id...]\n", program_name);
    printf("\n");
    printf("Batch delete files from FastDFS\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST      File list to delete (one file ID per line)\n");
    printf("  -j, --threads NUM    Number of parallel threads (default: 1, max: 20)\n");
    printf("  -n, --dry-run        Dry run mode (don't actually delete)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -y, --yes            Skip confirmation prompt\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f files_to_delete.txt\n", program_name);
    printf("  %s -f files.txt -j 10 -y\n", program_name);
    printf("  %s -n -f files.txt\n", program_name);
    printf("  %s group1/M00/00/00/file1.jpg group1/M00/00/00/file2.jpg\n", program_name);
}

static long long timespec_diff_ms(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000LL +
           (end->tv_nsec - start->tv_nsec) / 1000000LL;
}

static int delete_single_file(ConnectionInfo *pTrackerServer,
                              DeleteTask *task,
                              int dry_run) {
    int result;
    ConnectionInfo *pStorageServer;
    int exists;
    
    clock_gettime(CLOCK_MONOTONIC, &task->start_time);
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Failed to connect to storage server");
        task->status = -1;
        return -1;
    }
    
    result = storage_file_exist1(pTrackerServer, pStorageServer,
                                task->file_id, &exists);
    if (result != 0) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Failed to check file existence: %s", STRERROR(result));
        task->status = -2;
        tracker_disconnect_server_ex(pStorageServer, true);
        return result;
    }
    
    if (!exists) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "File does not exist");
        task->status = -3;
        tracker_disconnect_server_ex(pStorageServer, true);
        
        pthread_mutex_lock(&stats_mutex);
        skipped_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        return -3;
    }
    
    if (dry_run) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Dry run - would delete");
        task->status = 0;
        tracker_disconnect_server_ex(pStorageServer, true);
        
        pthread_mutex_lock(&stats_mutex);
        deleted_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        clock_gettime(CLOCK_MONOTONIC, &task->end_time);
        return 0;
    }
    
    result = storage_delete_file1(pTrackerServer, pStorageServer, task->file_id);
    
    clock_gettime(CLOCK_MONOTONIC, &task->end_time);
    
    if (result != 0) {
        snprintf(task->error_msg, sizeof(task->error_msg),
                "Delete failed: %s", STRERROR(result));
        task->status = result;
        tracker_disconnect_server_ex(pStorageServer, true);
        
        pthread_mutex_lock(&stats_mutex);
        failed_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        return result;
    }
    
    task->status = 0;
    tracker_disconnect_server_ex(pStorageServer, true);
    
    pthread_mutex_lock(&stats_mutex);
    deleted_files++;
    pthread_mutex_unlock(&stats_mutex);
    
    return 0;
}

static void *delete_worker(void *arg) {
    DeleteContext *ctx = (DeleteContext *)arg;
    DeleteTask *task;
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
        
        int result = delete_single_file(ctx->pTrackerServer, task, ctx->dry_run);
        
        long long elapsed_ms = timespec_diff_ms(&task->start_time, &task->end_time);
        
        if (result != 0) {
            if (task->status == -3) {
                printf("SKIP: %s (file not found)\n", task->file_id);
            } else {
                fprintf(stderr, "ERROR: %s - %s\n", task->file_id, task->error_msg);
            }
        } else {
            if (ctx->dry_run) {
                printf("DRY-RUN: %s (would delete in %lld ms)\n",
                       task->file_id, elapsed_ms);
            } else {
                printf("OK: %s (deleted in %lld ms)\n", task->file_id, elapsed_ms);
            }
        }
    }
    
    return NULL;
}

static int load_file_list(const char *list_file, DeleteTask **tasks, int *count) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    int capacity = 1000;
    int task_count = 0;
    DeleteTask *task_array;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    task_array = (DeleteTask *)malloc(capacity * sizeof(DeleteTask));
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
            task_array = (DeleteTask *)realloc(task_array,
                                              capacity * sizeof(DeleteTask));
            if (task_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        memset(&task_array[task_count], 0, sizeof(DeleteTask));
        strncpy(task_array[task_count].file_id, line, MAX_FILE_ID_LEN - 1);
        task_count++;
    }
    
    fclose(fp);
    
    *tasks = task_array;
    *count = task_count;
    total_files = task_count;
    
    return 0;
}

static int confirm_deletion(int file_count, int dry_run) {
    char response[10];
    
    if (dry_run) {
        printf("\n⚠ DRY RUN MODE - No files will actually be deleted\n");
        return 1;
    }
    
    printf("\n⚠ WARNING: You are about to delete %d files!\n", file_count);
    printf("This operation cannot be undone.\n");
    printf("Are you sure you want to continue? (yes/no): ");
    
    if (fgets(response, sizeof(response), stdin) == NULL) {
        return 0;
    }
    
    if (strcmp(response, "yes\n") == 0 || strcmp(response, "y\n") == 0) {
        return 1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    int num_threads = 1;
    int dry_run = 0;
    int verbose = 0;
    int skip_confirm = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    DeleteTask *tasks = NULL;
    int task_count = 0;
    DeleteContext ctx;
    pthread_t *threads;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"threads", required_argument, 0, 'j'},
        {"dry-run", no_argument, 0, 'n'},
        {"verbose", no_argument, 0, 'v'},
        {"yes", no_argument, 0, 'y'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:f:j:nvyh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
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
            case 'v':
                verbose = 1;
                break;
            case 'y':
                skip_confirm = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
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
        result = load_file_list(list_file, &tasks, &task_count);
        if (result != 0) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return result;
        }
    } else if (optind < argc) {
        task_count = argc - optind;
        tasks = (DeleteTask *)malloc(task_count * sizeof(DeleteTask));
        for (int i = 0; i < task_count; i++) {
            memset(&tasks[i], 0, sizeof(DeleteTask));
            strncpy(tasks[i].file_id, argv[optind + i], MAX_FILE_ID_LEN - 1);
        }
        total_files = task_count;
    } else {
        fprintf(stderr, "ERROR: No files specified\n\n");
        print_usage(argv[0]);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 1;
    }
    
    if (task_count == 0) {
        printf("No files to delete\n");
        free(tasks);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 0;
    }
    
    if (!skip_confirm && !confirm_deletion(task_count, dry_run)) {
        printf("Operation cancelled\n");
        free(tasks);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 0;
    }
    
    printf("\nStarting %sdeletion of %d files using %d threads...\n",
           dry_run ? "dry-run " : "", task_count, num_threads);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.tasks = tasks;
    ctx.task_count = task_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.dry_run = dry_run;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, delete_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long total_time_ms = timespec_diff_ms(&start_time, &end_time);
    
    printf("\n=== Deletion Summary ===\n");
    printf("Total files: %d\n", total_files);
    printf("Deleted: %d\n", deleted_files);
    printf("Failed: %d\n", failed_files);
    printf("Skipped (not found): %d\n", skipped_files);
    printf("Total time: %lld ms (%.2f files/sec)\n",
           total_time_ms,
           total_files * 1000.0 / total_time_ms);
    
    if (failed_files > 0) {
        printf("\n⚠ WARNING: %d files failed to delete!\n", failed_files);
    } else if (!dry_run) {
        printf("\n✓ All files deleted successfully\n");
    }
    
    free(tasks);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return failed_files > 0 ? 1 : 0;
}
