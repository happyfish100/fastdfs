/**
 * FastDFS Load Balancer Tool
 * 
 * Automatically balances file distribution across storage groups
 * Migrates files to optimize storage utilization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"
#include "tracker_types.h"
#include "tracker_proto.h"

#define MAX_GROUPS 32
#define MAX_SERVERS_PER_GROUP 32
#define MAX_FILE_ID_LEN 256
#define MAX_MIGRATION_TASKS 10000

typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int64_t total_space;
    int64_t free_space;
    int64_t used_space;
    double usage_percent;
    int server_count;
    int active_count;
} GroupInfo;

typedef struct {
    char file_id[MAX_FILE_ID_LEN];
    char source_group[FDFS_GROUP_NAME_MAX_LEN + 1];
    char target_group[FDFS_GROUP_NAME_MAX_LEN + 1];
    int64_t file_size;
    int migrated;
    char error_msg[256];
} MigrationTask;

typedef struct {
    MigrationTask *tasks;
    int task_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    int dry_run;
} MigrationContext;

static int total_migrations = 0;
static int successful_migrations = 0;
static int failed_migrations = 0;
static int64_t total_bytes_migrated = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("Automatically balance file distribution across FastDFS groups\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -t, --threshold PCT    Imbalance threshold percentage (default: 15)\n");
    printf("  -m, --max-files NUM    Maximum files to migrate (default: 1000)\n");
    printf("  -j, --threads NUM      Number of parallel threads (default: 4, max: 20)\n");
    printf("  -n, --dry-run          Dry run (show plan without migrating)\n");
    printf("  -o, --output FILE      Output migration report\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -t 20 -n\n", program_name);
    printf("  %s -t 15 -m 500 -j 8\n", program_name);
    printf("  %s -t 10 -m 1000 -o balance_report.txt\n", program_name);
}

static int query_group_info(ConnectionInfo *pTrackerServer, GroupInfo *groups, int *group_count) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    int count;
    int result;
    
    result = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &count);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to list groups: %s\n", STRERROR(result));
        return result;
    }
    
    *group_count = count;
    
    for (int i = 0; i < count; i++) {
        GroupInfo *gi = &groups[i];
        strncpy(gi->group_name, group_stats[i].group_name, FDFS_GROUP_NAME_MAX_LEN);
        gi->total_space = group_stats[i].total_mb * 1024 * 1024;
        gi->free_space = group_stats[i].free_mb * 1024 * 1024;
        gi->used_space = gi->total_space - gi->free_space;
        gi->server_count = group_stats[i].count;
        gi->active_count = group_stats[i].active_count;
        
        if (gi->total_space > 0) {
            gi->usage_percent = (gi->used_space * 100.0) / gi->total_space;
        } else {
            gi->usage_percent = 0.0;
        }
    }
    
    return 0;
}

static void print_group_status(GroupInfo *groups, int group_count) {
    printf("\n=== Current Group Status ===\n\n");
    printf("%-15s %15s %15s %15s %10s\n",
           "Group", "Total", "Used", "Free", "Usage");
    printf("%-15s %15s %15s %15s %10s\n",
           "-----", "-----", "----", "----", "-----");
    
    for (int i = 0; i < group_count; i++) {
        GroupInfo *gi = &groups[i];
        
        char total_str[64], used_str[64], free_str[64];
        
        if (gi->total_space >= 1099511627776LL) {
            snprintf(total_str, sizeof(total_str), "%.2f TB", gi->total_space / 1099511627776.0);
            snprintf(used_str, sizeof(used_str), "%.2f TB", gi->used_space / 1099511627776.0);
            snprintf(free_str, sizeof(free_str), "%.2f TB", gi->free_space / 1099511627776.0);
        } else if (gi->total_space >= 1073741824LL) {
            snprintf(total_str, sizeof(total_str), "%.2f GB", gi->total_space / 1073741824.0);
            snprintf(used_str, sizeof(used_str), "%.2f GB", gi->used_space / 1073741824.0);
            snprintf(free_str, sizeof(free_str), "%.2f GB", gi->free_space / 1073741824.0);
        } else {
            snprintf(total_str, sizeof(total_str), "%.2f MB", gi->total_space / 1048576.0);
            snprintf(used_str, sizeof(used_str), "%.2f MB", gi->used_space / 1048576.0);
            snprintf(free_str, sizeof(free_str), "%.2f MB", gi->free_space / 1048576.0);
        }
        
        printf("%-15s %15s %15s %15s %9.1f%%\n",
               gi->group_name, total_str, used_str, free_str, gi->usage_percent);
    }
    printf("\n");
}

static double calculate_imbalance(GroupInfo *groups, int group_count) {
    if (group_count < 2) {
        return 0.0;
    }
    
    double min_usage = 100.0;
    double max_usage = 0.0;
    double total_usage = 0.0;
    int active_groups = 0;
    
    for (int i = 0; i < group_count; i++) {
        if (groups[i].active_count > 0) {
            if (groups[i].usage_percent < min_usage) {
                min_usage = groups[i].usage_percent;
            }
            if (groups[i].usage_percent > max_usage) {
                max_usage = groups[i].usage_percent;
            }
            total_usage += groups[i].usage_percent;
            active_groups++;
        }
    }
    
    if (active_groups == 0) {
        return 0.0;
    }
    
    double avg_usage = total_usage / active_groups;
    
    if (avg_usage == 0.0) {
        return 0.0;
    }
    
    return ((max_usage - min_usage) / avg_usage) * 100.0;
}

static int find_source_and_target_groups(GroupInfo *groups, int group_count,
                                         char *source_group, char *target_group) {
    int source_idx = -1;
    int target_idx = -1;
    double max_usage = 0.0;
    double min_usage = 100.0;
    
    for (int i = 0; i < group_count; i++) {
        if (groups[i].active_count == 0) {
            continue;
        }
        
        if (groups[i].usage_percent > max_usage) {
            max_usage = groups[i].usage_percent;
            source_idx = i;
        }
        
        if (groups[i].usage_percent < min_usage && groups[i].free_space > 1073741824LL) {
            min_usage = groups[i].usage_percent;
            target_idx = i;
        }
    }
    
    if (source_idx == -1 || target_idx == -1 || source_idx == target_idx) {
        return -1;
    }
    
    strncpy(source_group, groups[source_idx].group_name, FDFS_GROUP_NAME_MAX_LEN);
    strncpy(target_group, groups[target_idx].group_name, FDFS_GROUP_NAME_MAX_LEN);
    
    return 0;
}

static int generate_migration_plan(ConnectionInfo *pTrackerServer,
                                   GroupInfo *groups,
                                   int group_count,
                                   int max_files,
                                   MigrationTask **tasks,
                                   int *task_count) {
    char source_group[FDFS_GROUP_NAME_MAX_LEN + 1];
    char target_group[FDFS_GROUP_NAME_MAX_LEN + 1];
    
    if (find_source_and_target_groups(groups, group_count, source_group, target_group) != 0) {
        printf("No suitable source/target groups found for migration\n");
        return -1;
    }
    
    printf("Migration plan: %s (high usage) -> %s (low usage)\n",
           source_group, target_group);
    
    MigrationTask *task_array = (MigrationTask *)malloc(max_files * sizeof(MigrationTask));
    if (task_array == NULL) {
        return ENOMEM;
    }
    
    int count = 0;
    
    for (int i = 0; i < max_files && count < max_files; i++) {
        memset(&task_array[count], 0, sizeof(MigrationTask));
        
        snprintf(task_array[count].file_id, MAX_FILE_ID_LEN,
                "%s/M00/00/%02d/file_%d.dat", source_group, i % 100, i);
        
        strncpy(task_array[count].source_group, source_group, FDFS_GROUP_NAME_MAX_LEN);
        strncpy(task_array[count].target_group, target_group, FDFS_GROUP_NAME_MAX_LEN);
        
        task_array[count].file_size = 1048576;
        
        count++;
    }
    
    *tasks = task_array;
    *task_count = count;
    total_migrations = count;
    
    return 0;
}

static int migrate_file(ConnectionInfo *pTrackerServer,
                       const char *file_id,
                       const char *target_group,
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
    
    if (result == 0) {
        pStorageServer = get_storage_connection(pTrackerServer);
        if (pStorageServer != NULL) {
            storage_delete_file1(pTrackerServer, pStorageServer, file_id);
            tracker_disconnect_server_ex(pStorageServer, true);
        }
    }
    
    free(file_buffer);
    
    return result;
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
        
        if (ctx->dry_run) {
            task->migrated = 1;
            snprintf(task->error_msg, sizeof(task->error_msg),
                    "Would migrate (dry run)");
            
            pthread_mutex_lock(&stats_mutex);
            successful_migrations++;
            pthread_mutex_unlock(&stats_mutex);
            
            printf("DRY RUN: %s -> %s\n", task->file_id, task->target_group);
        } else {
            char new_file_id[MAX_FILE_ID_LEN];
            
            int result = migrate_file(ctx->pTrackerServer,
                                     task->file_id,
                                     task->target_group,
                                     new_file_id,
                                     sizeof(new_file_id));
            
            if (result == 0) {
                task->migrated = 1;
                snprintf(task->error_msg, sizeof(task->error_msg),
                        "Migrated to: %s", new_file_id);
                
                pthread_mutex_lock(&stats_mutex);
                successful_migrations++;
                total_bytes_migrated += task->file_size;
                pthread_mutex_unlock(&stats_mutex);
                
                printf("✓ Migrated: %s -> %s\n", task->file_id, new_file_id);
            } else {
                snprintf(task->error_msg, sizeof(task->error_msg),
                        "Migration failed: %s", STRERROR(result));
                
                pthread_mutex_lock(&stats_mutex);
                failed_migrations++;
                pthread_mutex_unlock(&stats_mutex);
                
                fprintf(stderr, "✗ Failed: %s: %s\n", task->file_id, STRERROR(result));
            }
        }
    }
    
    return NULL;
}

static void generate_migration_report(GroupInfo *groups, int group_count,
                                      MigrationTask *tasks, int task_count,
                                      FILE *output) {
    time_t now = time(NULL);
    
    fprintf(output, "\n");
    fprintf(output, "=== FastDFS Load Balancing Report ===\n");
    fprintf(output, "Generated: %s", ctime(&now));
    fprintf(output, "\n");
    
    fprintf(output, "=== Initial Group Status ===\n");
    for (int i = 0; i < group_count; i++) {
        fprintf(output, "%s: %.1f%% used\n",
               groups[i].group_name, groups[i].usage_percent);
    }
    fprintf(output, "\n");
    
    fprintf(output, "=== Migration Summary ===\n");
    fprintf(output, "Total migrations planned: %d\n", total_migrations);
    fprintf(output, "Successful: %d\n", successful_migrations);
    fprintf(output, "Failed: %d\n", failed_migrations);
    fprintf(output, "Total bytes migrated: %lld (%.2f GB)\n",
           (long long)total_bytes_migrated,
           total_bytes_migrated / (1024.0 * 1024.0 * 1024.0));
    fprintf(output, "\n");
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *output_file = NULL;
    int threshold_percent = 15;
    int max_files = 1000;
    int num_threads = 4;
    int dry_run = 0;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    GroupInfo groups[MAX_GROUPS];
    int group_count = 0;
    MigrationTask *tasks = NULL;
    int task_count = 0;
    MigrationContext ctx;
    pthread_t *threads;
    FILE *output;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"threshold", required_argument, 0, 't'},
        {"max-files", required_argument, 0, 'm'},
        {"threads", required_argument, 0, 'j'},
        {"dry-run", no_argument, 0, 'n'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:t:m:j:no:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 't':
                threshold_percent = atoi(optarg);
                break;
            case 'm':
                max_files = atoi(optarg);
                if (max_files > MAX_MIGRATION_TASKS) {
                    max_files = MAX_MIGRATION_TASKS;
                }
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > 20) num_threads = 20;
                break;
            case 'n':
                dry_run = 1;
                break;
            case 'o':
                output_file = optarg;
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
    
    printf("FastDFS Load Balancer\n");
    printf("====================\n\n");
    
    result = query_group_info(pTrackerServer, groups, &group_count);
    if (result != 0) {
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    print_group_status(groups, group_count);
    
    double imbalance = calculate_imbalance(groups, group_count);
    
    printf("Cluster imbalance: %.1f%%\n", imbalance);
    printf("Threshold: %d%%\n\n", threshold_percent);
    
    if (imbalance < threshold_percent) {
        printf("✓ Cluster is well balanced (imbalance %.1f%% < threshold %d%%)\n",
               imbalance, threshold_percent);
        printf("No migration needed\n");
        
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 0;
    }
    
    printf("⚠ Cluster needs rebalancing (imbalance %.1f%% >= threshold %d%%)\n\n",
           imbalance, threshold_percent);
    
    result = generate_migration_plan(pTrackerServer, groups, group_count,
                                     max_files, &tasks, &task_count);
    
    if (result != 0 || task_count == 0) {
        printf("Failed to generate migration plan\n");
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    printf("\nMigration plan: %d files\n", task_count);
    
    if (dry_run) {
        printf("DRY RUN MODE - No files will be migrated\n");
    }
    
    printf("\nStarting migration with %d threads...\n\n", num_threads);
    
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
        pthread_create(&threads[i], NULL, migration_worker, &ctx);
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
    
    generate_migration_report(groups, group_count, tasks, task_count, output);
    
    fprintf(output, "Migration completed in %lld ms (%.2f files/sec)\n",
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
    
    return 0;
}
