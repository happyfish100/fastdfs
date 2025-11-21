/**
 * FastDFS File Replication Tool
 * 
 * Manages file replication across storage groups
 * Ensures data redundancy and high availability
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"

#define MAX_FILE_ID_LEN 256
#define MAX_PATH_LEN 1024
#define MAX_THREADS 20
#define MAX_GROUPS 32

typedef struct {
    char file_id[MAX_FILE_ID_LEN];
    char source_group[FDFS_GROUP_NAME_MAX_LEN + 1];
    char target_groups[MAX_GROUPS][FDFS_GROUP_NAME_MAX_LEN + 1];
    int target_group_count;
    int64_t file_size;
    uint32_t crc32;
    int replicated_count;
    int failed_count;
    char error_msg[256];
} ReplicationTask;

typedef struct {
    ReplicationTask *tasks;
    int task_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    int verify_crc;
} ReplicationContext;

static int total_files = 0;
static int replicated_files = 0;
static int failed_replications = 0;
static int64_t total_bytes_replicated = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -f <file_list> -t <target_groups>\n", program_name);
    printf("\n");
    printf("Replicate FastDFS files across storage groups\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST        File list to replicate (one file ID per line)\n");
    printf("  -t, --targets GROUPS   Target groups (comma-separated)\n");
    printf("  -s, --source GROUP     Source group (optional, auto-detect if not specified)\n");
    printf("  -j, --threads NUM      Number of parallel threads (default: 4, max: 20)\n");
    printf("  -v, --verify           Verify CRC32 after replication\n");
    printf("  -o, --output FILE      Output replication report\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f files.txt -t group2,group3\n", program_name);
    printf("  %s -f files.txt -s group1 -t group2 -v -j 8\n", program_name);
    printf("  %s -f files.txt -t group2,group3 -o replication_report.txt\n", program_name);
}

static int parse_target_groups(const char *groups_str, char target_groups[][FDFS_GROUP_NAME_MAX_LEN + 1]) {
    char *groups_copy = strdup(groups_str);
    char *token;
    int count = 0;
    
    token = strtok(groups_copy, ",");
    while (token != NULL && count < MAX_GROUPS) {
        while (*token == ' ') token++;
        
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') end--;
        *(end + 1) = '\0';
        
        strncpy(target_groups[count], token, FDFS_GROUP_NAME_MAX_LEN);
        count++;
        
        token = strtok(NULL, ",");
    }
    
    free(groups_copy);
    return count;
}

static int extract_group_from_file_id(const char *file_id, char *group_name) {
    const char *slash = strchr(file_id, '/');
    if (slash == NULL) {
        return -1;
    }
    
    int group_len = slash - file_id;
    if (group_len > FDFS_GROUP_NAME_MAX_LEN) {
        return -1;
    }
    
    strncpy(group_name, file_id, group_len);
    group_name[group_len] = '\0';
    
    return 0;
}

static int replicate_file_to_group(ConnectionInfo *pTrackerServer,
                                   const char *file_id,
                                   const char *target_group,
                                   uint32_t expected_crc32,
                                   int verify_crc,
                                   char *new_file_id,
                                   size_t new_file_id_size) {
    ConnectionInfo *pStorageServer;
    char *file_buffer = NULL;
    int64_t file_size;
    int result;
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    result = storage_download_file_to_buff1(pTrackerServer, pStorageServer,
                                           file_id, &file_buffer, &file_size);
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    if (result != 0) {
        return result;
    }
    
    if (verify_crc && expected_crc32 != 0) {
        uint32_t actual_crc32 = CRC32(file_buffer, file_size);
        if (actual_crc32 != expected_crc32) {
            free(file_buffer);
            return EINVAL;
        }
    }
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        free(file_buffer);
        return -1;
    }
    
    result = storage_upload_by_filebuff1_ex(pTrackerServer, pStorageServer,
                                           file_buffer, file_size,
                                           NULL, NULL, 0,
                                           target_group,
                                           new_file_id, new_file_id_size);
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    free(file_buffer);
    
    return result;
}

static void *replication_worker(void *arg) {
    ReplicationContext *ctx = (ReplicationContext *)arg;
    ReplicationTask *task;
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
        
        for (int i = 0; i < task->target_group_count; i++) {
            char new_file_id[MAX_FILE_ID_LEN];
            
            int result = replicate_file_to_group(ctx->pTrackerServer,
                                                task->file_id,
                                                task->target_groups[i],
                                                task->crc32,
                                                ctx->verify_crc,
                                                new_file_id,
                                                sizeof(new_file_id));
            
            if (result == 0) {
                task->replicated_count++;
                
                pthread_mutex_lock(&stats_mutex);
                total_bytes_replicated += task->file_size;
                pthread_mutex_unlock(&stats_mutex);
                
                printf("✓ Replicated: %s -> %s (%s)\n",
                       task->file_id, task->target_groups[i], new_file_id);
            } else {
                task->failed_count++;
                
                snprintf(task->error_msg, sizeof(task->error_msg),
                        "Failed to replicate to %s: %s",
                        task->target_groups[i], STRERROR(result));
                
                fprintf(stderr, "✗ Failed: %s -> %s: %s\n",
                       task->file_id, task->target_groups[i], STRERROR(result));
            }
        }
        
        pthread_mutex_lock(&stats_mutex);
        if (task->replicated_count > 0) {
            replicated_files++;
        }
        if (task->failed_count > 0) {
            failed_replications++;
        }
        pthread_mutex_unlock(&stats_mutex);
    }
    
    return NULL;
}

static int load_file_list(const char *list_file,
                         const char *source_group,
                         char target_groups[][FDFS_GROUP_NAME_MAX_LEN + 1],
                         int target_group_count,
                         ReplicationTask **tasks,
                         int *task_count) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    int capacity = 1000;
    int count = 0;
    ReplicationTask *task_array;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    task_array = (ReplicationTask *)malloc(capacity * sizeof(ReplicationTask));
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
        
        if (count >= capacity) {
            capacity *= 2;
            task_array = (ReplicationTask *)realloc(task_array,
                                                   capacity * sizeof(ReplicationTask));
            if (task_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        memset(&task_array[count], 0, sizeof(ReplicationTask));
        strncpy(task_array[count].file_id, line, MAX_FILE_ID_LEN - 1);
        
        if (source_group != NULL) {
            strncpy(task_array[count].source_group, source_group,
                   FDFS_GROUP_NAME_MAX_LEN);
        } else {
            if (extract_group_from_file_id(line, task_array[count].source_group) != 0) {
                fprintf(stderr, "WARNING: Cannot extract group from file ID: %s\n", line);
                continue;
            }
        }
        
        for (int i = 0; i < target_group_count; i++) {
            strncpy(task_array[count].target_groups[i], target_groups[i],
                   FDFS_GROUP_NAME_MAX_LEN);
        }
        task_array[count].target_group_count = target_group_count;
        
        count++;
    }
    
    fclose(fp);
    
    *tasks = task_array;
    *task_count = count;
    total_files = count;
    
    return 0;
}

static int query_file_info(ConnectionInfo *pTrackerServer, ReplicationTask *task) {
    FDFSFileInfo file_info;
    int result;
    ConnectionInfo *pStorageServer;
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    result = storage_query_file_info1(pTrackerServer, pStorageServer,
                                     task->file_id, &file_info);
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    if (result == 0) {
        task->file_size = file_info.file_size;
        task->crc32 = file_info.crc32;
    }
    
    return result;
}

static void generate_replication_report(ReplicationTask *tasks, int count, FILE *output) {
    time_t now = time(NULL);
    
    fprintf(output, "\n");
    fprintf(output, "=== FastDFS File Replication Report ===\n");
    fprintf(output, "Generated: %s", ctime(&now));
    fprintf(output, "\n");
    
    fprintf(output, "=== Summary ===\n");
    fprintf(output, "Total files: %d\n", total_files);
    fprintf(output, "Successfully replicated: %d\n", replicated_files);
    fprintf(output, "Failed: %d\n", failed_replications);
    fprintf(output, "Total bytes replicated: %lld (%.2f GB)\n",
           (long long)total_bytes_replicated,
           total_bytes_replicated / (1024.0 * 1024.0 * 1024.0));
    fprintf(output, "\n");
    
    if (replicated_files > 0) {
        fprintf(output, "=== Successfully Replicated ===\n");
        for (int i = 0; i < count; i++) {
            if (tasks[i].replicated_count > 0) {
                fprintf(output, "%s -> ", tasks[i].file_id);
                for (int j = 0; j < tasks[i].target_group_count; j++) {
                    fprintf(output, "%s", tasks[i].target_groups[j]);
                    if (j < tasks[i].target_group_count - 1) {
                        fprintf(output, ", ");
                    }
                }
                fprintf(output, " (%d/%d successful)\n",
                       tasks[i].replicated_count, tasks[i].target_group_count);
            }
        }
        fprintf(output, "\n");
    }
    
    if (failed_replications > 0) {
        fprintf(output, "=== Failed Replications ===\n");
        for (int i = 0; i < count; i++) {
            if (tasks[i].failed_count > 0) {
                fprintf(output, "%s - %s\n", tasks[i].file_id, tasks[i].error_msg);
            }
        }
        fprintf(output, "\n");
    }
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    char *target_groups_str = NULL;
    char *source_group = NULL;
    char *output_file = NULL;
    int num_threads = 4;
    int verify_crc = 0;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    ReplicationTask *tasks = NULL;
    int task_count = 0;
    char target_groups[MAX_GROUPS][FDFS_GROUP_NAME_MAX_LEN + 1];
    int target_group_count = 0;
    ReplicationContext ctx;
    pthread_t *threads;
    FILE *output;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"targets", required_argument, 0, 't'},
        {"source", required_argument, 0, 's'},
        {"threads", required_argument, 0, 'j'},
        {"verify", no_argument, 0, 'v'},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:f:t:s:j:vo:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 't':
                target_groups_str = optarg;
                break;
            case 's':
                source_group = optarg;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 'v':
                verify_crc = 1;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (list_file == NULL || target_groups_str == NULL) {
        fprintf(stderr, "ERROR: File list and target groups required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    target_group_count = parse_target_groups(target_groups_str, target_groups);
    if (target_group_count == 0) {
        fprintf(stderr, "ERROR: No valid target groups specified\n");
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    result = load_file_list(list_file, source_group, target_groups,
                           target_group_count, &tasks, &task_count);
    if (result != 0) {
        return result;
    }
    
    if (task_count == 0) {
        printf("No files to replicate\n");
        free(tasks);
        return 0;
    }
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        free(tasks);
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        free(tasks);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    printf("Querying file information...\n");
    for (int i = 0; i < task_count; i++) {
        query_file_info(pTrackerServer, &tasks[i]);
        
        if ((i + 1) % 100 == 0) {
            printf("\rQueried %d/%d files...", i + 1, task_count);
            fflush(stdout);
        }
    }
    printf("\rQueried %d files\n", task_count);
    
    printf("\nReplicating %d files to %d target group(s) using %d threads...\n",
           task_count, target_group_count, num_threads);
    
    if (verify_crc) {
        printf("CRC32 verification enabled\n");
    }
    
    printf("\nTarget groups: ");
    for (int i = 0; i < target_group_count; i++) {
        printf("%s", target_groups[i]);
        if (i < target_group_count - 1) {
            printf(", ");
        }
    }
    printf("\n\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.tasks = tasks;
    ctx.task_count = task_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.verify_crc = verify_crc;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, replication_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000LL +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
    
    if (output_file != NULL) {
        output = fopen(output_file, "w");
        if (output == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            output = stdout;
        }
    } else {
        output = stdout;
    }
    
    generate_replication_report(tasks, task_count, output);
    
    fprintf(output, "Replication completed in %lld ms (%.2f files/sec)\n",
           elapsed_ms, task_count * 1000.0 / elapsed_ms);
    
    if (output != stdout) {
        fclose(output);
        printf("\nReport saved to: %s\n", output_file);
    }
    
    free(tasks);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return failed_replications > 0 ? 1 : 0;
}
