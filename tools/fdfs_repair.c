/**
 * FastDFS File Repair Tool
 * 
 * Repairs corrupted or missing files by re-uploading from backup
 * Verifies file integrity and fixes metadata issues
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

#define MAX_FILE_ID_LEN 256
#define MAX_PATH_LEN 1024
#define MAX_THREADS 10

typedef enum {
    REPAIR_OK = 0,
    REPAIR_MISSING = 1,
    REPAIR_CORRUPTED = 2,
    REPAIR_METADATA_MISSING = 3,
    REPAIR_FAILED = 4
} RepairStatus;

typedef struct {
    char file_id[MAX_FILE_ID_LEN];
    char backup_path[MAX_PATH_LEN];
    RepairStatus status;
    char error_msg[256];
    int64_t file_size;
    uint32_t expected_crc32;
    uint32_t actual_crc32;
} RepairInfo;

typedef struct {
    RepairInfo *repairs;
    int repair_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    char backup_dir[MAX_PATH_LEN];
    int verify_only;
    int fix_metadata;
} RepairContext;

static int total_files = 0;
static int ok_files = 0;
static int missing_files = 0;
static int corrupted_files = 0;
static int repaired_files = 0;
static int failed_repairs = 0;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -f <file_list> -b <backup_dir>\n", program_name);
    printf("\n");
    printf("Repair corrupted or missing FastDFS files\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST      File list to check/repair (one file ID per line)\n");
    printf("  -b, --backup DIR     Backup directory for repair source\n");
    printf("  -v, --verify-only    Verify only, don't repair\n");
    printf("  -m, --fix-metadata   Fix metadata issues\n");
    printf("  -j, --threads NUM    Number of parallel threads (default: 4, max: 10)\n");
    printf("  -o, --output FILE    Output repair report\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f files.txt -b /backup -v\n", program_name);
    printf("  %s -f files.txt -b /backup -m -j 8\n", program_name);
    printf("  %s -f files.txt -b /backup -o repair_report.txt\n", program_name);
}

static int verify_file_integrity(ConnectionInfo *pTrackerServer,
                                 const char *file_id,
                                 uint32_t *actual_crc32,
                                 int64_t *file_size) {
    FDFSFileInfo file_info;
    int result;
    ConnectionInfo *pStorageServer;
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    result = storage_query_file_info1(pTrackerServer, pStorageServer, file_id, &file_info);
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    if (result != 0) {
        return result;
    }
    
    *actual_crc32 = file_info.crc32;
    *file_size = file_info.file_size;
    
    return 0;
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

static int repair_file(ConnectionInfo *pTrackerServer,
                      RepairInfo *info,
                      const char *backup_dir,
                      int verify_only) {
    char backup_path[MAX_PATH_LEN];
    struct stat st;
    int result;
    ConnectionInfo *pStorageServer;
    uint32_t actual_crc32;
    int64_t file_size;
    
    result = verify_file_integrity(pTrackerServer, info->file_id,
                                   &actual_crc32, &file_size);
    
    if (result == ENOENT || result == 2) {
        info->status = REPAIR_MISSING;
        snprintf(info->error_msg, sizeof(info->error_msg),
                "File not found in FastDFS");
        
        pthread_mutex_lock(&stats_mutex);
        missing_files++;
        pthread_mutex_unlock(&stats_mutex);
    } else if (result != 0) {
        info->status = REPAIR_FAILED;
        snprintf(info->error_msg, sizeof(info->error_msg),
                "Failed to query file: %s", STRERROR(result));
        
        pthread_mutex_lock(&stats_mutex);
        failed_repairs++;
        pthread_mutex_unlock(&stats_mutex);
        
        return result;
    } else {
        info->actual_crc32 = actual_crc32;
        info->file_size = file_size;
    }
    
    snprintf(backup_path, sizeof(backup_path), "%s/%s", backup_dir, info->file_id);
    
    if (stat(backup_path, &st) != 0) {
        if (info->status == REPAIR_MISSING) {
            snprintf(info->error_msg, sizeof(info->error_msg),
                    "File missing and no backup available");
        } else {
            info->status = REPAIR_OK;
            snprintf(info->error_msg, sizeof(info->error_msg), "OK");
            
            pthread_mutex_lock(&stats_mutex);
            ok_files++;
            pthread_mutex_unlock(&stats_mutex);
        }
        return 0;
    }
    
    strncpy(info->backup_path, backup_path, sizeof(info->backup_path) - 1);
    
    uint32_t backup_crc32 = calculate_file_crc32(backup_path);
    info->expected_crc32 = backup_crc32;
    
    if (info->status == REPAIR_OK && actual_crc32 != backup_crc32) {
        info->status = REPAIR_CORRUPTED;
        snprintf(info->error_msg, sizeof(info->error_msg),
                "CRC32 mismatch: expected %08X, actual %08X",
                backup_crc32, actual_crc32);
        
        pthread_mutex_lock(&stats_mutex);
        corrupted_files++;
        pthread_mutex_unlock(&stats_mutex);
    } else if (info->status == REPAIR_OK) {
        snprintf(info->error_msg, sizeof(info->error_msg), "OK");
        
        pthread_mutex_lock(&stats_mutex);
        ok_files++;
        pthread_mutex_unlock(&stats_mutex);
        
        return 0;
    }
    
    if (verify_only) {
        return 0;
    }
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        info->status = REPAIR_FAILED;
        snprintf(info->error_msg, sizeof(info->error_msg),
                "Failed to connect to storage server");
        
        pthread_mutex_lock(&stats_mutex);
        failed_repairs++;
        pthread_mutex_unlock(&stats_mutex);
        
        return -1;
    }
    
    if (info->status == REPAIR_MISSING || info->status == REPAIR_CORRUPTED) {
        char new_file_id[MAX_FILE_ID_LEN];
        
        result = upload_file(pTrackerServer, pStorageServer, backup_path,
                           new_file_id, sizeof(new_file_id));
        
        if (result == 0) {
            if (strcmp(new_file_id, info->file_id) != 0) {
                snprintf(info->error_msg, sizeof(info->error_msg),
                        "Repaired but file ID changed: %s -> %s",
                        info->file_id, new_file_id);
            } else {
                snprintf(info->error_msg, sizeof(info->error_msg),
                        "Successfully repaired");
            }
            
            pthread_mutex_lock(&stats_mutex);
            repaired_files++;
            pthread_mutex_unlock(&stats_mutex);
        } else {
            info->status = REPAIR_FAILED;
            snprintf(info->error_msg, sizeof(info->error_msg),
                    "Repair failed: %s", STRERROR(result));
            
            pthread_mutex_lock(&stats_mutex);
            failed_repairs++;
            pthread_mutex_unlock(&stats_mutex);
        }
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    return 0;
}

static void *repair_worker(void *arg) {
    RepairContext *ctx = (RepairContext *)arg;
    RepairInfo *info;
    int index;
    
    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->current_index >= ctx->repair_count) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        info = &ctx->repairs[index];
        
        repair_file(ctx->pTrackerServer, info, ctx->backup_dir, ctx->verify_only);
        
        const char *status_str;
        switch (info->status) {
            case REPAIR_OK:
                status_str = "OK";
                break;
            case REPAIR_MISSING:
                status_str = "MISSING";
                break;
            case REPAIR_CORRUPTED:
                status_str = "CORRUPTED";
                break;
            case REPAIR_METADATA_MISSING:
                status_str = "METADATA_MISSING";
                break;
            case REPAIR_FAILED:
                status_str = "FAILED";
                break;
            default:
                status_str = "UNKNOWN";
        }
        
        printf("%s: %s - %s\n", status_str, info->file_id, info->error_msg);
    }
    
    return NULL;
}

static int load_file_list(const char *list_file, RepairInfo **repairs, int *count) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    int capacity = 1000;
    int repair_count = 0;
    RepairInfo *repair_array;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    repair_array = (RepairInfo *)malloc(capacity * sizeof(RepairInfo));
    if (repair_array == NULL) {
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
        
        if (repair_count >= capacity) {
            capacity *= 2;
            repair_array = (RepairInfo *)realloc(repair_array,
                                                capacity * sizeof(RepairInfo));
            if (repair_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        memset(&repair_array[repair_count], 0, sizeof(RepairInfo));
        strncpy(repair_array[repair_count].file_id, line, MAX_FILE_ID_LEN - 1);
        repair_count++;
    }
    
    fclose(fp);
    
    *repairs = repair_array;
    *count = repair_count;
    total_files = repair_count;
    
    return 0;
}

static void generate_repair_report(RepairInfo *repairs, int count, FILE *output) {
    time_t now = time(NULL);
    
    fprintf(output, "\n");
    fprintf(output, "=== FastDFS File Repair Report ===\n");
    fprintf(output, "Generated: %s", ctime(&now));
    fprintf(output, "\n");
    
    fprintf(output, "=== Summary ===\n");
    fprintf(output, "Total files checked: %d\n", total_files);
    fprintf(output, "OK: %d\n", ok_files);
    fprintf(output, "Missing: %d\n", missing_files);
    fprintf(output, "Corrupted: %d\n", corrupted_files);
    fprintf(output, "Repaired: %d\n", repaired_files);
    fprintf(output, "Failed: %d\n", failed_repairs);
    fprintf(output, "\n");
    
    if (missing_files > 0) {
        fprintf(output, "=== Missing Files ===\n");
        for (int i = 0; i < count; i++) {
            if (repairs[i].status == REPAIR_MISSING) {
                fprintf(output, "%s - %s\n", repairs[i].file_id, repairs[i].error_msg);
            }
        }
        fprintf(output, "\n");
    }
    
    if (corrupted_files > 0) {
        fprintf(output, "=== Corrupted Files ===\n");
        for (int i = 0; i < count; i++) {
            if (repairs[i].status == REPAIR_CORRUPTED) {
                fprintf(output, "%s - %s\n", repairs[i].file_id, repairs[i].error_msg);
            }
        }
        fprintf(output, "\n");
    }
    
    if (failed_repairs > 0) {
        fprintf(output, "=== Failed Repairs ===\n");
        for (int i = 0; i < count; i++) {
            if (repairs[i].status == REPAIR_FAILED) {
                fprintf(output, "%s - %s\n", repairs[i].file_id, repairs[i].error_msg);
            }
        }
        fprintf(output, "\n");
    }
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    char *backup_dir = NULL;
    char *output_file = NULL;
    int verify_only = 0;
    int fix_metadata = 0;
    int num_threads = 4;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    RepairInfo *repairs = NULL;
    int repair_count = 0;
    RepairContext ctx;
    pthread_t *threads;
    FILE *output;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"backup", required_argument, 0, 'b'},
        {"verify-only", no_argument, 0, 'v'},
        {"fix-metadata", no_argument, 0, 'm'},
        {"threads", required_argument, 0, 'j'},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:f:b:vmj:o:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 'b':
                backup_dir = optarg;
                break;
            case 'v':
                verify_only = 1;
                break;
            case 'm':
                fix_metadata = 1;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
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
    
    if (list_file == NULL || backup_dir == NULL) {
        fprintf(stderr, "ERROR: File list and backup directory required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    result = load_file_list(list_file, &repairs, &repair_count);
    if (result != 0) {
        return result;
    }
    
    if (repair_count == 0) {
        printf("No files to check\n");
        free(repairs);
        return 0;
    }
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        free(repairs);
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        free(repairs);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    printf("Checking %d files using %d threads...\n", repair_count, num_threads);
    if (verify_only) {
        printf("Verify-only mode: No repairs will be performed\n");
    }
    printf("\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.repairs = repairs;
    ctx.repair_count = repair_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    strncpy(ctx.backup_dir, backup_dir, sizeof(ctx.backup_dir) - 1);
    ctx.verify_only = verify_only;
    ctx.fix_metadata = fix_metadata;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, repair_worker, &ctx);
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
    
    generate_repair_report(repairs, repair_count, output);
    
    fprintf(output, "Check completed in %lld ms (%.2f files/sec)\n",
           elapsed_ms, repair_count * 1000.0 / elapsed_ms);
    
    if (output != stdout) {
        fclose(output);
        printf("\nReport saved to: %s\n", output_file);
    }
    
    free(repairs);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    int exit_code = 0;
    if (failed_repairs > 0 || (missing_files > 0 && !verify_only) ||
        (corrupted_files > 0 && !verify_only)) {
        exit_code = 1;
    }
    
    return exit_code;
}
