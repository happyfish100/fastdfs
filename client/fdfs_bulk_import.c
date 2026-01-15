/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <getopt.h>
#include "fdfs_client.h"
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/sched_thread.h"
#include "../storage/storage_bulk_import.h"

#define DEFAULT_THREADS 4
#define MAX_THREADS 32
#define OUTPUT_BUFFER_SIZE 1024

typedef struct {
    char config_file[MAX_PATH_SIZE];
    char source_path[MAX_PATH_SIZE];
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char output_file[MAX_PATH_SIZE];
    int store_path_index;
    int import_mode;
    int thread_count;
    bool recursive;
    bool dry_run;
    bool calculate_crc32;
    bool verbose;
} BulkImportOptions;

static void usage(const char *program_name)
{
    printf("FastDFS Bulk Import Tool v1.0\n");
    printf("Usage: %s [OPTIONS] <source_path>\n\n", program_name);
    printf("Options:\n");
    printf("  -c, --config <file>       FastDFS client config file (required)\n");
    printf("  -g, --group <name>        Target storage group name (required)\n");
    printf("  -p, --path-index <num>    Storage path index (default: 0)\n");
    printf("  -m, --mode <copy|move>    Import mode (default: copy)\n");
    printf("  -t, --threads <num>       Number of worker threads (default: %d, max: %d)\n",
        DEFAULT_THREADS, MAX_THREADS);
    printf("  -r, --recursive           Recursively import directories\n");
    printf("  -o, --output <file>       Output mapping file (source -> file_id)\n");
    printf("  -n, --dry-run             Validate only, don't import\n");
    printf("  -C, --no-crc32            Skip CRC32 calculation (faster but less safe)\n");
    printf("  -v, --verbose             Verbose output\n");
    printf("  -h, --help                Show this help message\n\n");
    printf("Examples:\n");
    printf("  # Import single file\n");
    printf("  %s -c /etc/fdfs/client.conf -g group1 /data/file.jpg\n\n", program_name);
    printf("  # Import directory recursively with 8 threads\n");
    printf("  %s -c /etc/fdfs/client.conf -g group1 -r -t 8 /data/images/\n\n", program_name);
    printf("  # Move files instead of copy\n");
    printf("  %s -c /etc/fdfs/client.conf -g group1 -m move /data/old/\n\n", program_name);
    printf("  # Dry-run to validate before actual import\n");
    printf("  %s -c /etc/fdfs/client.conf -g group1 -n /data/test/\n\n", program_name);
}

static int parse_import_mode(const char *mode_str)
{
    if (strcmp(mode_str, "copy") == 0) {
        return BULK_IMPORT_MODE_COPY;
    } else if (strcmp(mode_str, "move") == 0) {
        return BULK_IMPORT_MODE_MOVE;
    } else {
        return -1;
    }
}

static int parse_options(int argc, char *argv[], BulkImportOptions *options)
{
    int c;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"config",      required_argument, 0, 'c'},
        {"group",       required_argument, 0, 'g'},
        {"path-index",  required_argument, 0, 'p'},
        {"mode",        required_argument, 0, 'm'},
        {"threads",     required_argument, 0, 't'},
        {"recursive",   no_argument,       0, 'r'},
        {"output",      required_argument, 0, 'o'},
        {"dry-run",     no_argument,       0, 'n'},
        {"no-crc32",    no_argument,       0, 'C'},
        {"verbose",     no_argument,       0, 'v'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    memset(options, 0, sizeof(BulkImportOptions));
    options->store_path_index = 0;
    options->import_mode = BULK_IMPORT_MODE_COPY;
    options->thread_count = DEFAULT_THREADS;
    options->calculate_crc32 = true;

    while ((c = getopt_long(argc, argv, "c:g:p:m:t:ro:nCvh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                snprintf(options->config_file, sizeof(options->config_file), "%s", optarg);
                break;
            case 'g':
                snprintf(options->group_name, sizeof(options->group_name), "%s", optarg);
                break;
            case 'p':
                options->store_path_index = atoi(optarg);
                break;
            case 'm':
                options->import_mode = parse_import_mode(optarg);
                if (options->import_mode < 0) {
                    fprintf(stderr, "Invalid import mode: %s (use 'copy' or 'move')\n", optarg);
                    return EINVAL;
                }
                break;
            case 't':
                options->thread_count = atoi(optarg);
                if (options->thread_count < 1 || options->thread_count > MAX_THREADS) {
                    fprintf(stderr, "Thread count must be between 1 and %d\n", MAX_THREADS);
                    return EINVAL;
                }
                break;
            case 'r':
                options->recursive = true;
                break;
            case 'o':
                snprintf(options->output_file, sizeof(options->output_file), "%s", optarg);
                break;
            case 'n':
                options->dry_run = true;
                break;
            case 'C':
                options->calculate_crc32 = false;
                break;
            case 'v':
                options->verbose = true;
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
            default:
                return EINVAL;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Source path is required\n\n");
        usage(argv[0]);
        return EINVAL;
    }

    snprintf(options->source_path, sizeof(options->source_path), "%s", argv[optind]);

    if (options->config_file[0] == '\0') {
        fprintf(stderr, "Error: Config file is required (-c option)\n\n");
        usage(argv[0]);
        return EINVAL;
    }

    if (options->group_name[0] == '\0') {
        fprintf(stderr, "Error: Group name is required (-g option)\n\n");
        usage(argv[0]);
        return EINVAL;
    }

    return 0;
}

static int write_output_mapping(FILE *fp, const BulkImportFileInfo *file_info)
{
    if (fp == NULL || file_info == NULL) {
        return EINVAL;
    }

    fprintf(fp, "%s\t%s\t%"PRId64"\t%u\t%s\n",
        file_info->source_path,
        file_info->file_id,
        file_info->file_size,
        file_info->crc32,
        file_info->status == BULK_IMPORT_STATUS_SUCCESS ? "SUCCESS" :
        file_info->status == BULK_IMPORT_STATUS_FAILED ? "FAILED" : "SKIPPED");
    
    fflush(fp);
    return 0;
}

static int import_single_file(BulkImportContext *context,
    const BulkImportOptions *options, const char *file_path, FILE *output_fp)
{
    BulkImportFileInfo file_info;
    int result;

    if (options->verbose) {
        printf("Processing: %s\n", file_path);
    }

    result = storage_calculate_file_metadata(file_path, &file_info, options->calculate_crc32);
    if (result != 0) {
        fprintf(stderr, "Error calculating metadata for %s: %s\n",
            file_path, file_info.error_message);
        __sync_add_and_fetch(&context->failed_files, 1);
        return result;
    }

    result = storage_generate_file_id(&file_info, options->group_name, options->store_path_index);
    if (result != 0) {
        fprintf(stderr, "Error generating file ID for %s: %s\n",
            file_path, file_info.error_message);
        __sync_add_and_fetch(&context->failed_files, 1);
        return result;
    }

    result = storage_register_bulk_file(context, &file_info);
    if (result != 0) {
        fprintf(stderr, "Error importing %s: %s\n",
            file_path, file_info.error_message);
        __sync_add_and_fetch(&context->failed_files, 1);
    } else {
        if (options->verbose) {
            printf("  -> %s (%"PRId64" bytes)\n", file_info.file_id, file_info.file_size);
        }
    }

    if (output_fp != NULL) {
        write_output_mapping(output_fp, &file_info);
    }

    __sync_add_and_fetch(&context->processed_files, 1);

    return result;
}

static int import_directory_recursive(BulkImportContext *context,
    const BulkImportOptions *options, const char *dir_path, FILE *output_fp)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH_SIZE];
    int result = 0;
    int file_result;

    dir = opendir(dir_path);
    if (dir == NULL) {
        fprintf(stderr, "Error opening directory %s: %s\n", dir_path, STRERROR(errno));
        return errno != 0 ? errno : EIO;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (stat(full_path, &st) != 0) {
            fprintf(stderr, "Error stat %s: %s\n", full_path, STRERROR(errno));
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            __sync_add_and_fetch(&context->total_files, 1);
            file_result = import_single_file(context, options, full_path, output_fp);
            if (file_result != 0 && result == 0) {
                result = file_result;
            }
        } else if (S_ISDIR(st.st_mode) && options->recursive) {
            file_result = import_directory_recursive(context, options, full_path, output_fp);
            if (file_result != 0 && result == 0) {
                result = file_result;
            }
        }
    }

    closedir(dir);
    return result;
}

static void print_summary(const BulkImportContext *context, const BulkImportOptions *options)
{
    time_t duration = context->end_time - context->start_time;
    double speed_mbps = 0.0;

    if (duration > 0) {
        speed_mbps = (double)context->total_bytes / (1024.0 * 1024.0) / duration;
    }

    printf("\n");
    printf("=== Import Summary ===\n");
    printf("Mode:            %s\n", options->dry_run ? "DRY-RUN" : 
        (options->import_mode == BULK_IMPORT_MODE_COPY ? "COPY" : "MOVE"));
    printf("Total files:     %"PRId64"\n", context->total_files);
    printf("Processed:       %"PRId64"\n", context->processed_files);
    printf("Success:         %"PRId64"\n", context->success_files);
    printf("Failed:          %"PRId64"\n", context->failed_files);
    printf("Skipped:         %"PRId64"\n", context->skipped_files);
    printf("Total bytes:     %"PRId64" (%.2f GB)\n",
        context->total_bytes, (double)context->total_bytes / (1024.0 * 1024.0 * 1024.0));
    printf("Duration:        %"PRId64" seconds\n", (int64_t)duration);
    printf("Speed:           %.2f MB/s\n", speed_mbps);
    printf("======================\n");
}

int main(int argc, char *argv[])
{
    BulkImportOptions options;
    BulkImportContext context;
    struct stat st;
    FILE *output_fp = NULL;
    int result;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    result = parse_options(argc, argv, &options);
    if (result != 0) {
        return result;
    }

    log_init();
    if (options.verbose) {
        g_log_context.log_level = LOG_DEBUG;
    } else {
        g_log_context.log_level = LOG_INFO;
    }

    printf("FastDFS Bulk Import Tool\n");
    printf("Config:          %s\n", options.config_file);
    printf("Group:           %s\n", options.group_name);
    printf("Source:          %s\n", options.source_path);
    printf("Mode:            %s\n", options.dry_run ? "DRY-RUN" :
        (options.import_mode == BULK_IMPORT_MODE_COPY ? "COPY" : "MOVE"));
    printf("Threads:         %d\n", options.thread_count);
    printf("CRC32:           %s\n", options.calculate_crc32 ? "enabled" : "disabled");
    printf("\n");

    result = storage_bulk_import_init();
    if (result != 0) {
        fprintf(stderr, "Failed to initialize bulk import module\n");
        return result;
    }

    memset(&context, 0, sizeof(context));
    snprintf(context.group_name, sizeof(context.group_name), "%s", options.group_name);
    context.store_path_index = options.store_path_index;
    context.import_mode = options.import_mode;
    context.calculate_crc32 = options.calculate_crc32;
    context.validate_only = options.dry_run;
    context.start_time = time(NULL);

    if (options.output_file[0] != '\0') {
        output_fp = fopen(options.output_file, "w");
        if (output_fp == NULL) {
            fprintf(stderr, "Error opening output file %s: %s\n",
                options.output_file, STRERROR(errno));
            storage_bulk_import_destroy();
            return errno != 0 ? errno : EIO;
        }
        fprintf(output_fp, "# Source\tFileID\tSize\tCRC32\tStatus\n");
    }

    if (stat(options.source_path, &st) != 0) {
        fprintf(stderr, "Error: Source path not found: %s\n", options.source_path);
        if (output_fp != NULL) {
            fclose(output_fp);
        }
        storage_bulk_import_destroy();
        return errno != 0 ? errno : ENOENT;
    }

    if (S_ISREG(st.st_mode)) {
        context.total_files = 1;
        result = import_single_file(&context, &options, options.source_path, output_fp);
    } else if (S_ISDIR(st.st_mode)) {
        result = import_directory_recursive(&context, &options, options.source_path, output_fp);
    } else {
        fprintf(stderr, "Error: Source path is not a file or directory\n");
        result = EINVAL;
    }

    context.end_time = time(NULL);

    if (output_fp != NULL) {
        fclose(output_fp);
        printf("Output mapping written to: %s\n", options.output_file);
    }

    print_summary(&context, &options);

    storage_bulk_import_destroy();

    return result == 0 ? 0 : 1;
}
