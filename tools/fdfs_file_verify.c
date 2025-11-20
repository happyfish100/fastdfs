/**
 * FastDFS File Integrity Verification Tool
 * 
 * Verifies file integrity by comparing CRC32 checksums
 * Useful for detecting corrupted files or verifying backups
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"
#include "fastcommon/hash.h"

#define MAX_FILE_ID_LEN 256
#define BUFFER_SIZE (256 * 1024)

typedef struct {
    char file_id[MAX_FILE_ID_LEN];
    int64_t file_size;
    uint32_t expected_crc32;
    uint32_t actual_crc32;
    int status;
} VerifyResult;

static int verbose = 0;
static int quiet = 0;
static int total_files = 0;
static int verified_files = 0;
static int corrupted_files = 0;
static int missing_files = 0;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <file_id> [file_id...]\n", program_name);
    printf("       %s [OPTIONS] -f <file_list>\n", program_name);
    printf("\n");
    printf("Verify file integrity in FastDFS by checking CRC32 checksums\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST      Read file IDs from file (one per line)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -q, --quiet          Quiet mode (only show errors)\n");
    printf("  -j, --json           Output results in JSON format\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s group1/M00/00/00/file.jpg\n", program_name);
    printf("  %s -f file_list.txt\n", program_name);
    printf("  %s -v group1/M00/00/00/file1.jpg group1/M00/00/00/file2.jpg\n", program_name);
}

static uint32_t calculate_file_crc32(const char *filename) {
    FILE *fp;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    uint32_t crc32 = 0;
    
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return 0;
    }
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        crc32 = CRC32_ex(buffer, bytes_read, crc32);
    }
    
    fclose(fp);
    return crc32;
}

static int verify_single_file(ConnectionInfo *pTrackerServer,
                              ConnectionInfo *pStorageServer,
                              const char *file_id,
                              VerifyResult *result) {
    FDFSFileInfo file_info;
    char local_file[256];
    int64_t file_size;
    int ret;
    
    memset(result, 0, sizeof(VerifyResult));
    strncpy(result->file_id, file_id, MAX_FILE_ID_LEN - 1);
    
    total_files++;
    
    ret = storage_query_file_info1(pTrackerServer, pStorageServer,
                                   file_id, &file_info);
    if (ret != 0) {
        if (verbose || !quiet) {
            fprintf(stderr, "ERROR: Failed to query file info for %s: %s\n",
                   file_id, STRERROR(ret));
        }
        result->status = -1;
        missing_files++;
        return ret;
    }
    
    result->file_size = file_info.file_size;
    result->expected_crc32 = file_info.crc32;
    
    snprintf(local_file, sizeof(local_file), "/tmp/fdfs_verify_%d.tmp", getpid());
    
    ret = storage_download_file_to_file1(pTrackerServer, pStorageServer,
                                        file_id, local_file, &file_size);
    if (ret != 0) {
        if (verbose || !quiet) {
            fprintf(stderr, "ERROR: Failed to download %s: %s\n",
                   file_id, STRERROR(ret));
        }
        result->status = -2;
        missing_files++;
        return ret;
    }
    
    result->actual_crc32 = calculate_file_crc32(local_file);
    unlink(local_file);
    
    if (result->actual_crc32 != result->expected_crc32) {
        if (!quiet) {
            fprintf(stderr, "CORRUPTED: %s (expected CRC32: 0x%08X, actual: 0x%08X)\n",
                   file_id, result->expected_crc32, result->actual_crc32);
        }
        result->status = 1;
        corrupted_files++;
        return 1;
    }
    
    if (verbose) {
        printf("OK: %s (CRC32: 0x%08X, size: %lld bytes)\n",
               file_id, result->expected_crc32, (long long)result->file_size);
    }
    
    result->status = 0;
    verified_files++;
    return 0;
}

static int verify_from_list(ConnectionInfo *pTrackerServer,
                           ConnectionInfo *pStorageServer,
                           const char *list_file,
                           int json_output) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    VerifyResult result;
    int first = 1;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    if (json_output) {
        printf("{\n");
        printf("  \"results\": [\n");
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
        
        verify_single_file(pTrackerServer, pStorageServer, line, &result);
        
        if (json_output) {
            if (!first) {
                printf(",\n");
            }
            printf("    {\n");
            printf("      \"file_id\": \"%s\",\n", result.file_id);
            printf("      \"size\": %lld,\n", (long long)result.file_size);
            printf("      \"expected_crc32\": \"0x%08X\",\n", result.expected_crc32);
            printf("      \"actual_crc32\": \"0x%08X\",\n", result.actual_crc32);
            printf("      \"status\": %d\n", result.status);
            printf("    }");
            first = 0;
        }
    }
    
    if (json_output) {
        printf("\n  ],\n");
        printf("  \"summary\": {\n");
        printf("    \"total\": %d,\n", total_files);
        printf("    \"verified\": %d,\n", verified_files);
        printf("    \"corrupted\": %d,\n", corrupted_files);
        printf("    \"missing\": %d\n", missing_files);
        printf("  }\n");
        printf("}\n");
    }
    
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    int json_output = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;
    VerifyResult verify_result;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'j'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:f:vqjh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'j':
                json_output = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (list_file == NULL && optind >= argc) {
        fprintf(stderr, "ERROR: No file IDs specified\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = LOG_ERR;
    
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
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to storage server\n");
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    if (list_file != NULL) {
        result = verify_from_list(pTrackerServer, pStorageServer, list_file, json_output);
    } else {
        if (json_output) {
            printf("{\n");
            printf("  \"results\": [\n");
        }
        
        for (int i = optind; i < argc; i++) {
            verify_single_file(pTrackerServer, pStorageServer, argv[i], &verify_result);
            
            if (json_output) {
                if (i > optind) {
                    printf(",\n");
                }
                printf("    {\n");
                printf("      \"file_id\": \"%s\",\n", verify_result.file_id);
                printf("      \"size\": %lld,\n", (long long)verify_result.file_size);
                printf("      \"expected_crc32\": \"0x%08X\",\n", verify_result.expected_crc32);
                printf("      \"actual_crc32\": \"0x%08X\",\n", verify_result.actual_crc32);
                printf("      \"status\": %d\n", verify_result.status);
                printf("    }");
            }
        }
        
        if (json_output) {
            printf("\n  ],\n");
            printf("  \"summary\": {\n");
            printf("    \"total\": %d,\n", total_files);
            printf("    \"verified\": %d,\n", verified_files);
            printf("    \"corrupted\": %d,\n", corrupted_files);
            printf("    \"missing\": %d\n", missing_files);
            printf("  }\n");
            printf("}\n");
        }
    }
    
    if (!quiet && !json_output) {
        printf("\n=== Verification Summary ===\n");
        printf("Total files: %d\n", total_files);
        printf("Verified: %d\n", verified_files);
        printf("Corrupted: %d\n", corrupted_files);
        printf("Missing: %d\n", missing_files);
        
        if (corrupted_files > 0 || missing_files > 0) {
            printf("\n⚠ WARNING: Found %d corrupted or missing files!\n",
                   corrupted_files + missing_files);
        } else {
            printf("\n✓ All files verified successfully\n");
        }
    }
    
    tracker_disconnect_server_ex(pStorageServer, true);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return (corrupted_files > 0 || missing_files > 0) ? 1 : 0;
}
