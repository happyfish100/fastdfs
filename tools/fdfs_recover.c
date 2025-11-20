/**
 * FastDFS File Recovery Tool
 * 
 * Recovers deleted or lost files from storage servers
 * Scans storage directories and rebuilds file index
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"

#define MAX_FILE_ID_LEN 256
#define MAX_PATH_LEN 1024
#define MAX_THREADS 10

typedef struct {
    char file_path[MAX_PATH_LEN];
    char file_id[MAX_FILE_ID_LEN];
    int64_t file_size;
    time_t mtime;
    int recovered;
    char error_msg[256];
} RecoveryInfo;

typedef struct {
    RecoveryInfo *files;
    int file_count;
    int capacity;
    pthread_mutex_t mutex;
} RecoveryList;

typedef struct {
    RecoveryList *list;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    char target_group[FDFS_GROUP_NAME_MAX_LEN + 1];
    int dry_run;
} RecoveryContext;

static int total_scanned = 0;
static int total_recovered = 0;
static int total_failed = 0;
static int64_t total_bytes = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -d <storage_dir>\n", program_name);
    printf("\n");
    printf("Recover deleted or lost files from FastDFS storage\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -d, --dir PATH       Storage directory to scan\n");
    printf("  -g, --group NAME     Target group for recovery\n");
    printf("  -o, --output FILE    Output recovery report\n");
    printf("  -j, --threads NUM    Number of parallel threads (default: 4, max: 10)\n");
    printf("  -n, --dry-run        Dry run (don't actually recover)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -d /data/fastdfs/storage/data\n", program_name);
    printf("  %s -d /data/storage -g group1 -j 8\n", program_name);
    printf("  %s -d /data/storage -n -o recovery_plan.txt\n", program_name);
}

static int add_to_recovery_list(RecoveryList *list, const char *file_path,
                                int64_t file_size, time_t mtime) {
    pthread_mutex_lock(&list->mutex);
    
    if (list->file_count >= list->capacity) {
        list->capacity *= 2;
        list->files = (RecoveryInfo *)realloc(list->files,
                                             list->capacity * sizeof(RecoveryInfo));
        if (list->files == NULL) {
            pthread_mutex_unlock(&list->mutex);
            return -1;
        }
    }
    
    RecoveryInfo *info = &list->files[list->file_count];
    memset(info, 0, sizeof(RecoveryInfo));
    
    strncpy(info->file_path, file_path, MAX_PATH_LEN - 1);
    info->file_size = file_size;
    info->mtime = mtime;
    
    list->file_count++;
    
    pthread_mutex_unlock(&list->mutex);
    
    return 0;
}

static int scan_directory_recursive(const char *dir_path, RecoveryList *list) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH_LEN];
    int file_count = 0;
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        fprintf(stderr, "ERROR: Failed to open directory: %s\n", dir_path);
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (stat(full_path, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            file_count += scan_directory_recursive(full_path, list);
        } else if (S_ISREG(st.st_mode)) {
            add_to_recovery_list(list, full_path, st.st_size, st.st_mtime);
            file_count++;
            
            if (file_count % 1000 == 0) {
                printf("\rScanned %d files...", file_count);
                fflush(stdout);
            }
        }
    }
    
    closedir(dir);
    return file_count;
}

static int extract_file_id_from_path(const char *file_path, const char *storage_dir,
                                     char *file_id, size_t file_id_size) {
    const char *relative_path = file_path + strlen(storage_dir);
    
    while (*relative_path == '/') {
        relative_path++;
    }
    
    if (strncmp(relative_path, "data/", 5) == 0) {
        relative_path += 5;
    }
    
    strncpy(file_id, relative_path, file_id_size - 1);
    file_id[file_id_size - 1] = '\0';
    
    return 0;
}

static int recover_file(ConnectionInfo *pTrackerServer,
                       RecoveryInfo *info,
                       const char *target_group,
                       int dry_run) {
    ConnectionInfo *pStorageServer;
    char new_file_id[MAX_FILE_ID_LEN];
    int result;
    
    if (dry_run) {
        snprintf(info->error_msg, sizeof(info->error_msg),
                "Would recover: %lld bytes", (long long)info->file_size);
        info->recovered = 1;
        
        pthread_mutex_lock(&stats_mutex);
        total_recovered++;
        total_bytes += info->file_size;
        pthread_mutex_unlock(&stats_mutex);
        
        return 0;
    }
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        snprintf(info->error_msg, sizeof(info->error_msg),
                "Failed to connect to storage server");
        
        pthread_mutex_lock(&stats_mutex);
        total_failed++;
        pthread_mutex_unlock(&stats_mutex);
        
        return -1;
    }
    
    if (target_group != NULL && strlen(target_group) > 0) {
        result = storage_upload_by_filename1_ex(pTrackerServer, pStorageServer,
                                               info->file_path, NULL, NULL, 0,
                                               target_group, new_file_id);
    } else {
        result = upload_file(pTrackerServer, pStorageServer, info->file_path,
                           new_file_id, sizeof(new_file_id));
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    if (result == 0) {
        strncpy(info->file_id, new_file_id, MAX_FILE_ID_LEN - 1);
        snprintf(info->error_msg, sizeof(info->error_msg),
                "Recovered as: %s", new_file_id);
        info->recovered = 1;
        
        pthread_mutex_lock(&stats_mutex);
        total_recovered++;
        total_bytes += info->file_size;
        pthread_mutex_unlock(&stats_mutex);
    } else {
        snprintf(info->error_msg, sizeof(info->error_msg),
                "Recovery failed: %s", STRERROR(result));
        
        pthread_mutex_lock(&stats_mutex);
        total_failed++;
        pthread_mutex_unlock(&stats_mutex);
    }
    
    return result;
}

static void *recovery_worker(void *arg) {
    RecoveryContext *ctx = (RecoveryContext *)arg;
    RecoveryInfo *info;
    int index;
    
    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->current_index >= ctx->list->file_count) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        info = &ctx->list->files[index];
        
        recover_file(ctx->pTrackerServer, info, ctx->target_group, ctx->dry_run);
        
        if (info->recovered) {
            printf("RECOVERED: %s - %s\n", info->file_path, info->error_msg);
        } else {
            fprintf(stderr, "FAILED: %s - %s\n", info->file_path, info->error_msg);
        }
        
        pthread_mutex_lock(&stats_mutex);
        total_scanned++;
        pthread_mutex_unlock(&stats_mutex);
        
        if (total_scanned % 100 == 0) {
            printf("\rProcessed: %d/%d files...", total_scanned, ctx->list->file_count);
            fflush(stdout);
        }
    }
    
    return NULL;
}

static void generate_recovery_report(RecoveryList *list, FILE *output) {
    time_t now = time(NULL);
    
    fprintf(output, "\n");
    fprintf(output, "=== FastDFS File Recovery Report ===\n");
    fprintf(output, "Generated: %s", ctime(&now));
    fprintf(output, "\n");
    
    fprintf(output, "=== Summary ===\n");
    fprintf(output, "Total files scanned: %d\n", total_scanned);
    fprintf(output, "Successfully recovered: %d\n", total_recovered);
    fprintf(output, "Failed: %d\n", total_failed);
    fprintf(output, "Total size recovered: %lld bytes (%.2f GB)\n",
           (long long)total_bytes, total_bytes / (1024.0 * 1024.0 * 1024.0));
    fprintf(output, "\n");
    
    if (total_recovered > 0) {
        fprintf(output, "=== Recovered Files ===\n");
        for (int i = 0; i < list->file_count; i++) {
            if (list->files[i].recovered) {
                fprintf(output, "%s -> %s (%lld bytes)\n",
                       list->files[i].file_path,
                       list->files[i].file_id,
                       (long long)list->files[i].file_size);
            }
        }
        fprintf(output, "\n");
    }
    
    if (total_failed > 0) {
        fprintf(output, "=== Failed Recoveries ===\n");
        for (int i = 0; i < list->file_count; i++) {
            if (!list->files[i].recovered && strlen(list->files[i].error_msg) > 0) {
                fprintf(output, "%s - %s\n",
                       list->files[i].file_path,
                       list->files[i].error_msg);
            }
        }
        fprintf(output, "\n");
    }
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *storage_dir = NULL;
    char *target_group = NULL;
    char *output_file = NULL;
    int num_threads = 4;
    int dry_run = 0;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    RecoveryList list;
    RecoveryContext ctx;
    pthread_t *threads;
    FILE *output;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"dir", required_argument, 0, 'd'},
        {"group", required_argument, 0, 'g'},
        {"output", required_argument, 0, 'o'},
        {"threads", required_argument, 0, 'j'},
        {"dry-run", no_argument, 0, 'n'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:d:g:o:j:nvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'd':
                storage_dir = optarg;
                break;
            case 'g':
                target_group = optarg;
                break;
            case 'o':
                output_file = optarg;
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
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (storage_dir == NULL) {
        fprintf(stderr, "ERROR: Storage directory required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    memset(&list, 0, sizeof(list));
    list.capacity = 10000;
    list.files = (RecoveryInfo *)malloc(list.capacity * sizeof(RecoveryInfo));
    if (list.files == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        return ENOMEM;
    }
    pthread_mutex_init(&list.mutex, NULL);
    
    printf("Scanning storage directory: %s\n", storage_dir);
    printf("\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    int scanned = scan_directory_recursive(storage_dir, &list);
    
    printf("\rScanned %d files\n", scanned);
    
    if (list.file_count == 0) {
        printf("No files found to recover\n");
        free(list.files);
        pthread_mutex_destroy(&list.mutex);
        return 0;
    }
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        free(list.files);
        pthread_mutex_destroy(&list.mutex);
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        free(list.files);
        pthread_mutex_destroy(&list.mutex);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    printf("Recovering %d files using %d threads...\n", list.file_count, num_threads);
    if (target_group != NULL) {
        printf("Target group: %s\n", target_group);
    }
    if (dry_run) {
        printf("DRY RUN MODE - No files will be uploaded\n");
    }
    printf("\n");
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.list = &list;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    if (target_group != NULL) {
        strncpy(ctx.target_group, target_group, sizeof(ctx.target_group) - 1);
    }
    ctx.dry_run = dry_run;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, recovery_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000LL +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
    
    printf("\n");
    
    if (output_file != NULL) {
        output = fopen(output_file, "w");
        if (output == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            output = stdout;
        }
    } else {
        output = stdout;
    }
    
    generate_recovery_report(&list, output);
    
    fprintf(output, "Recovery completed in %lld ms (%.2f files/sec)\n",
           elapsed_ms, list.file_count * 1000.0 / elapsed_ms);
    
    if (output != stdout) {
        fclose(output);
        printf("\nReport saved to: %s\n", output_file);
    }
    
    free(list.files);
    free(threads);
    pthread_mutex_destroy(&list.mutex);
    pthread_mutex_destroy(&ctx.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return total_failed > 0 ? 1 : 0;
}
