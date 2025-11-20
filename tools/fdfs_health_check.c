/**
 * FastDFS Health Check Tool
 * 
 * Performs comprehensive health checks on FastDFS cluster
 * Tests connectivity, performance, and identifies potential issues
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "logger.h"

#define TEST_FILE_SIZE 1024
#define MAX_GROUPS 64
#define MAX_SERVERS 128

typedef enum {
    CHECK_PASS = 0,
    CHECK_WARN = 1,
    CHECK_FAIL = 2
} CheckStatus;

typedef struct {
    char name[128];
    CheckStatus status;
    char message[512];
    long duration_ms;
} HealthCheckResult;

static int verbose = 0;
static int json_output = 0;
static int total_checks = 0;
static int passed_checks = 0;
static int warned_checks = 0;
static int failed_checks = 0;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("Perform comprehensive health checks on FastDFS cluster\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -q, --quick          Quick check (skip performance tests)\n");
    printf("  -j, --json           Output in JSON format\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - All checks passed\n");
    printf("  1 - Some checks failed\n");
    printf("  2 - Critical failure\n");
}

static long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void record_check(HealthCheckResult *result, const char *name,
                        CheckStatus status, const char *message, long duration_ms) {
    strncpy(result->name, name, sizeof(result->name) - 1);
    result->status = status;
    strncpy(result->message, message, sizeof(result->message) - 1);
    result->duration_ms = duration_ms;
    
    total_checks++;
    if (status == CHECK_PASS) {
        passed_checks++;
    } else if (status == CHECK_WARN) {
        warned_checks++;
    } else {
        failed_checks++;
    }
}

static void check_tracker_connection(ConnectionInfo *pTrackerServer,
                                    HealthCheckResult *result) {
    long start_time = get_time_ms();
    
    if (pTrackerServer == NULL || pTrackerServer->sock < 0) {
        record_check(result, "Tracker Connection", CHECK_FAIL,
                    "Failed to connect to tracker server", 0);
        return;
    }
    
    long duration = get_time_ms() - start_time;
    
    if (duration > 1000) {
        record_check(result, "Tracker Connection", CHECK_WARN,
                    "Tracker connection slow (>1s)", duration);
    } else {
        record_check(result, "Tracker Connection", CHECK_PASS,
                    "Tracker server connected successfully", duration);
    }
}

static void check_storage_servers(ConnectionInfo *pTrackerServer,
                                 HealthCheckResult *results, int *result_count) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    int group_count;
    int result;
    long start_time;
    
    start_time = get_time_ms();
    result = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &group_count);
    
    if (result != 0) {
        record_check(&results[*result_count], "List Groups", CHECK_FAIL,
                    "Failed to list storage groups", get_time_ms() - start_time);
        (*result_count)++;
        return;
    }
    
    record_check(&results[*result_count], "List Groups", CHECK_PASS,
                "Successfully listed storage groups", get_time_ms() - start_time);
    (*result_count)++;
    
    if (group_count == 0) {
        record_check(&results[*result_count], "Group Count", CHECK_FAIL,
                    "No storage groups found", 0);
        (*result_count)++;
        return;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Found %d storage group(s)", group_count);
    record_check(&results[*result_count], "Group Count", CHECK_PASS, msg, 0);
    (*result_count)++;
    
    int total_servers = 0;
    int active_servers = 0;
    int offline_servers = 0;
    
    for (int i = 0; i < group_count; i++) {
        FDFSStorageStat storage_stats[MAX_SERVERS];
        int storage_count;
        
        start_time = get_time_ms();
        result = tracker_list_servers(pTrackerServer, group_stats[i].group_name,
                                     NULL, storage_stats, MAX_SERVERS, &storage_count);
        
        if (result != 0) {
            snprintf(msg, sizeof(msg), "Failed to list servers for group %s",
                    group_stats[i].group_name);
            record_check(&results[*result_count], "List Servers", CHECK_WARN,
                        msg, get_time_ms() - start_time);
            (*result_count)++;
            continue;
        }
        
        total_servers += storage_count;
        
        for (int j = 0; j < storage_count; j++) {
            if (storage_stats[j].status == FDFS_STORAGE_STATUS_ACTIVE) {
                active_servers++;
            } else {
                offline_servers++;
            }
        }
    }
    
    snprintf(msg, sizeof(msg), "Total: %d, Active: %d, Offline: %d",
            total_servers, active_servers, offline_servers);
    
    if (offline_servers > 0) {
        record_check(&results[*result_count], "Server Status", CHECK_WARN,
                    msg, 0);
    } else {
        record_check(&results[*result_count], "Server Status", CHECK_PASS,
                    msg, 0);
    }
    (*result_count)++;
}

static void check_storage_space(ConnectionInfo *pTrackerServer,
                               HealthCheckResult *results, int *result_count) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    int group_count;
    int result;
    
    result = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &group_count);
    if (result != 0) {
        return;
    }
    
    for (int i = 0; i < group_count; i++) {
        int64_t total_mb = group_stats[i].total_mb;
        int64_t free_mb = group_stats[i].free_mb;
        
        if (total_mb == 0) {
            continue;
        }
        
        double usage_percent = ((total_mb - free_mb) * 100.0) / total_mb;
        char msg[256];
        
        snprintf(msg, sizeof(msg), "Group %s: %.1f%% used (%lld MB free)",
                group_stats[i].group_name, usage_percent, (long long)free_mb);
        
        CheckStatus status;
        if (usage_percent >= 95.0) {
            status = CHECK_FAIL;
        } else if (usage_percent >= 85.0) {
            status = CHECK_WARN;
        } else {
            status = CHECK_PASS;
        }
        
        record_check(&results[*result_count], "Storage Space", status, msg, 0);
        (*result_count)++;
    }
}

static void check_upload_performance(ConnectionInfo *pTrackerServer,
                                    ConnectionInfo *pStorageServer,
                                    HealthCheckResult *result) {
    char test_file[256];
    char file_id[128];
    FILE *fp;
    long start_time, duration;
    int ret;
    
    snprintf(test_file, sizeof(test_file), "/tmp/fdfs_health_test_%d.dat", getpid());
    
    fp = fopen(test_file, "wb");
    if (fp == NULL) {
        record_check(result, "Upload Test", CHECK_FAIL,
                    "Failed to create test file", 0);
        return;
    }
    
    for (int i = 0; i < TEST_FILE_SIZE; i++) {
        fputc('A' + (i % 26), fp);
    }
    fclose(fp);
    
    start_time = get_time_ms();
    ret = upload_file(pTrackerServer, pStorageServer, test_file,
                     file_id, sizeof(file_id));
    duration = get_time_ms() - start_time;
    
    unlink(test_file);
    
    if (ret != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Upload failed: %s", STRERROR(ret));
        record_check(result, "Upload Test", CHECK_FAIL, msg, duration);
        return;
    }
    
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Upload successful (%ld ms, %.2f KB/s)",
            duration, (TEST_FILE_SIZE / 1024.0) / (duration / 1000.0));
    
    CheckStatus status;
    if (duration > 5000) {
        status = CHECK_WARN;
    } else {
        status = CHECK_PASS;
    }
    
    record_check(result, "Upload Test", status, msg, duration);
}

static void check_download_performance(ConnectionInfo *pTrackerServer,
                                      ConnectionInfo *pStorageServer,
                                      HealthCheckResult *result) {
    char test_file[256];
    char download_file[256];
    char file_id[128];
    FILE *fp;
    long start_time, upload_time, download_time;
    int64_t file_size;
    int ret;
    
    snprintf(test_file, sizeof(test_file), "/tmp/fdfs_health_upload_%d.dat", getpid());
    snprintf(download_file, sizeof(download_file), "/tmp/fdfs_health_download_%d.dat", getpid());
    
    fp = fopen(test_file, "wb");
    if (fp == NULL) {
        record_check(result, "Download Test", CHECK_FAIL,
                    "Failed to create test file", 0);
        return;
    }
    
    for (int i = 0; i < TEST_FILE_SIZE; i++) {
        fputc('B' + (i % 26), fp);
    }
    fclose(fp);
    
    ret = upload_file(pTrackerServer, pStorageServer, test_file,
                     file_id, sizeof(file_id));
    unlink(test_file);
    
    if (ret != 0) {
        record_check(result, "Download Test", CHECK_FAIL,
                    "Failed to upload test file", 0);
        return;
    }
    
    start_time = get_time_ms();
    ret = storage_download_file_to_file1(pTrackerServer, pStorageServer,
                                        file_id, download_file, &file_size);
    download_time = get_time_ms() - start_time;
    
    unlink(download_file);
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    if (ret != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Download failed: %s", STRERROR(ret));
        record_check(result, "Download Test", CHECK_FAIL, msg, download_time);
        return;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Download successful (%ld ms, %.2f KB/s)",
            download_time, (file_size / 1024.0) / (download_time / 1000.0));
    
    CheckStatus status;
    if (download_time > 5000) {
        status = CHECK_WARN;
    } else {
        status = CHECK_PASS;
    }
    
    record_check(result, "Download Test", status, msg, download_time);
}

static void check_metadata_operations(ConnectionInfo *pTrackerServer,
                                     ConnectionInfo *pStorageServer,
                                     HealthCheckResult *result) {
    char test_file[256];
    char file_id[128];
    FILE *fp;
    FDFSMetaData meta_list[2];
    FDFSMetaData *retrieved_meta = NULL;
    int meta_count;
    long start_time, duration;
    int ret;
    
    snprintf(test_file, sizeof(test_file), "/tmp/fdfs_health_meta_%d.dat", getpid());
    
    fp = fopen(test_file, "wb");
    if (fp == NULL) {
        record_check(result, "Metadata Test", CHECK_FAIL,
                    "Failed to create test file", 0);
        return;
    }
    fwrite("test", 1, 4, fp);
    fclose(fp);
    
    ret = upload_file(pTrackerServer, pStorageServer, test_file,
                     file_id, sizeof(file_id));
    unlink(test_file);
    
    if (ret != 0) {
        record_check(result, "Metadata Test", CHECK_FAIL,
                    "Failed to upload test file", 0);
        return;
    }
    
    strcpy(meta_list[0].name, "test_key");
    strcpy(meta_list[0].value, "test_value");
    strcpy(meta_list[1].name, "health_check");
    strcpy(meta_list[1].value, "true");
    
    start_time = get_time_ms();
    ret = storage_set_metadata1(pTrackerServer, pStorageServer, file_id,
                               meta_list, 2, STORAGE_SET_METADATA_FLAG_OVERWRITE);
    
    if (ret == 0) {
        ret = storage_get_metadata1(pTrackerServer, pStorageServer, file_id,
                                   &retrieved_meta, &meta_count);
    }
    
    duration = get_time_ms() - start_time;
    
    storage_delete_file1(pTrackerServer, pStorageServer, file_id);
    
    if (retrieved_meta != NULL) {
        free(retrieved_meta);
    }
    
    if (ret != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Metadata operation failed: %s", STRERROR(ret));
        record_check(result, "Metadata Test", CHECK_FAIL, msg, duration);
        return;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Metadata operations successful (%ld ms)", duration);
    record_check(result, "Metadata Test", CHECK_PASS, msg, duration);
}

static void print_results_text(HealthCheckResult *results, int count) {
    printf("\n");
    printf("=== FastDFS Health Check Results ===\n");
    printf("\n");
    
    for (int i = 0; i < count; i++) {
        const char *status_str;
        const char *status_symbol;
        
        switch (results[i].status) {
            case CHECK_PASS:
                status_str = "PASS";
                status_symbol = "✓";
                break;
            case CHECK_WARN:
                status_str = "WARN";
                status_symbol = "⚠";
                break;
            case CHECK_FAIL:
                status_str = "FAIL";
                status_symbol = "✗";
                break;
            default:
                status_str = "UNKNOWN";
                status_symbol = "?";
        }
        
        printf("[%s] %s %s\n", status_symbol, results[i].name, status_str);
        printf("    %s\n", results[i].message);
        if (results[i].duration_ms > 0) {
            printf("    Duration: %ld ms\n", results[i].duration_ms);
        }
        printf("\n");
    }
    
    printf("=== Summary ===\n");
    printf("Total checks: %d\n", total_checks);
    printf("Passed: %d\n", passed_checks);
    printf("Warnings: %d\n", warned_checks);
    printf("Failed: %d\n", failed_checks);
    printf("\n");
    
    if (failed_checks > 0) {
        printf("⚠ Health check FAILED - %d critical issues found\n", failed_checks);
    } else if (warned_checks > 0) {
        printf("⚠ Health check passed with %d warnings\n", warned_checks);
    } else {
        printf("✓ All health checks PASSED\n");
    }
}

static void print_results_json(HealthCheckResult *results, int count) {
    printf("{\n");
    printf("  \"timestamp\": %ld,\n", (long)time(NULL));
    printf("  \"total_checks\": %d,\n", total_checks);
    printf("  \"passed\": %d,\n", passed_checks);
    printf("  \"warnings\": %d,\n", warned_checks);
    printf("  \"failed\": %d,\n", failed_checks);
    printf("  \"checks\": [\n");
    
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            printf(",\n");
        }
        
        printf("    {\n");
        printf("      \"name\": \"%s\",\n", results[i].name);
        printf("      \"status\": \"%s\",\n",
               results[i].status == CHECK_PASS ? "pass" :
               results[i].status == CHECK_WARN ? "warn" : "fail");
        printf("      \"message\": \"%s\",\n", results[i].message);
        printf("      \"duration_ms\": %ld\n", results[i].duration_ms);
        printf("    }");
    }
    
    printf("\n  ]\n");
    printf("}\n");
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    int quick_check = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    HealthCheckResult results[64];
    int result_count = 0;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"quick", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:qjvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'q':
                quick_check = 1;
                break;
            case 'j':
                json_output = 1;
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
        return 2;
    }
    
    pTrackerServer = tracker_get_connection();
    check_tracker_connection(pTrackerServer, &results[result_count++]);
    
    if (pTrackerServer == NULL) {
        if (json_output) {
            print_results_json(results, result_count);
        } else {
            print_results_text(results, result_count);
        }
        fdfs_client_destroy();
        return 2;
    }
    
    check_storage_servers(pTrackerServer, results, &result_count);
    check_storage_space(pTrackerServer, results, &result_count);
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer != NULL) {
        if (!quick_check) {
            check_upload_performance(pTrackerServer, pStorageServer, &results[result_count++]);
            check_download_performance(pTrackerServer, pStorageServer, &results[result_count++]);
            check_metadata_operations(pTrackerServer, pStorageServer, &results[result_count++]);
        }
        tracker_disconnect_server_ex(pStorageServer, true);
    } else {
        record_check(&results[result_count++], "Storage Connection", CHECK_FAIL,
                    "Failed to connect to storage server", 0);
    }
    
    if (json_output) {
        print_results_json(results, result_count);
    } else {
        print_results_text(results, result_count);
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    if (failed_checks > 0) {
        return 1;
    }
    
    return 0;
}
