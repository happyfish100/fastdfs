/**
 * FastDFS Storage Analyzer
 * 
 * Analyzes storage usage patterns and generates statistics
 * Helps with capacity planning and optimization
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
#define MAX_EXTENSION_LEN 32
#define MAX_EXTENSIONS 1000
#define SIZE_BUCKETS 10
#define MAX_THREADS 10

typedef struct {
    char extension[MAX_EXTENSION_LEN];
    int count;
    int64_t total_size;
} ExtensionStats;

typedef struct {
    int64_t min_size;
    int64_t max_size;
    int count;
    int64_t total_size;
    char label[64];
} SizeBucket;

typedef struct {
    ExtensionStats extensions[MAX_EXTENSIONS];
    int extension_count;
    SizeBucket size_buckets[SIZE_BUCKETS];
    int64_t total_files;
    int64_t total_size;
    int64_t min_file_size;
    int64_t max_file_size;
    time_t oldest_file;
    time_t newest_file;
    pthread_mutex_t mutex;
} AnalysisStats;

typedef struct {
    char *file_ids;
    int file_count;
    int current_index;
    pthread_mutex_t mutex;
    ConnectionInfo *pTrackerServer;
    AnalysisStats *stats;
    int verbose;
} AnalysisContext;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] -f <file_list>\n", program_name);
    printf("\n");
    printf("Analyze FastDFS storage usage patterns\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -f, --file LIST      File list to analyze (one file ID per line)\n");
    printf("  -o, --output FILE    Output report file (default: stdout)\n");
    printf("  -j, --threads NUM    Number of parallel threads (default: 4, max: 10)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -f all_files.txt\n", program_name);
    printf("  %s -f files.txt -o analysis.txt -j 8\n", program_name);
}

static void init_size_buckets(SizeBucket *buckets) {
    const int64_t KB = 1024;
    const int64_t MB = 1024 * KB;
    const int64_t GB = 1024 * MB;
    
    buckets[0].min_size = 0;
    buckets[0].max_size = 10 * KB;
    strcpy(buckets[0].label, "0-10 KB");
    
    buckets[1].min_size = 10 * KB;
    buckets[1].max_size = 100 * KB;
    strcpy(buckets[1].label, "10-100 KB");
    
    buckets[2].min_size = 100 * KB;
    buckets[2].max_size = MB;
    strcpy(buckets[2].label, "100 KB-1 MB");
    
    buckets[3].min_size = MB;
    buckets[3].max_size = 10 * MB;
    strcpy(buckets[3].label, "1-10 MB");
    
    buckets[4].min_size = 10 * MB;
    buckets[4].max_size = 100 * MB;
    strcpy(buckets[4].label, "10-100 MB");
    
    buckets[5].min_size = 100 * MB;
    buckets[5].max_size = GB;
    strcpy(buckets[5].label, "100 MB-1 GB");
    
    buckets[6].min_size = GB;
    buckets[6].max_size = 10 * GB;
    strcpy(buckets[6].label, "1-10 GB");
    
    buckets[7].min_size = 10 * GB;
    buckets[7].max_size = 100 * GB;
    strcpy(buckets[7].label, "10-100 GB");
    
    buckets[8].min_size = 100 * GB;
    buckets[8].max_size = 1024 * GB;
    strcpy(buckets[8].label, "100 GB-1 TB");
    
    buckets[9].min_size = 1024 * GB;
    buckets[9].max_size = LLONG_MAX;
    strcpy(buckets[9].label, "> 1 TB");
    
    for (int i = 0; i < SIZE_BUCKETS; i++) {
        buckets[i].count = 0;
        buckets[i].total_size = 0;
    }
}

static const char *get_file_extension(const char *file_id) {
    const char *dot = strrchr(file_id, '.');
    if (dot == NULL || dot == file_id) {
        return "no_ext";
    }
    return dot + 1;
}

static void update_extension_stats(AnalysisStats *stats, const char *extension,
                                   int64_t size) {
    int found = 0;
    
    for (int i = 0; i < stats->extension_count; i++) {
        if (strcmp(stats->extensions[i].extension, extension) == 0) {
            stats->extensions[i].count++;
            stats->extensions[i].total_size += size;
            found = 1;
            break;
        }
    }
    
    if (!found && stats->extension_count < MAX_EXTENSIONS) {
        strncpy(stats->extensions[stats->extension_count].extension,
               extension, MAX_EXTENSION_LEN - 1);
        stats->extensions[stats->extension_count].count = 1;
        stats->extensions[stats->extension_count].total_size = size;
        stats->extension_count++;
    }
}

static void update_size_bucket(AnalysisStats *stats, int64_t size) {
    for (int i = 0; i < SIZE_BUCKETS; i++) {
        if (size >= stats->size_buckets[i].min_size &&
            size < stats->size_buckets[i].max_size) {
            stats->size_buckets[i].count++;
            stats->size_buckets[i].total_size += size;
            break;
        }
    }
}

static int analyze_file(ConnectionInfo *pTrackerServer, const char *file_id,
                       AnalysisStats *stats, int verbose) {
    FDFSFileInfo file_info;
    int result;
    ConnectionInfo *pStorageServer;
    
    pStorageServer = get_storage_connection(pTrackerServer);
    if (pStorageServer == NULL) {
        if (verbose) {
            fprintf(stderr, "ERROR: Failed to connect to storage server for %s\n", file_id);
        }
        return -1;
    }
    
    result = storage_query_file_info1(pTrackerServer, pStorageServer, file_id, &file_info);
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    if (result != 0) {
        if (verbose) {
            fprintf(stderr, "ERROR: Failed to query %s: %s\n", file_id, STRERROR(result));
        }
        return result;
    }
    
    pthread_mutex_lock(&stats->mutex);
    
    stats->total_files++;
    stats->total_size += file_info.file_size;
    
    if (stats->total_files == 1 || file_info.file_size < stats->min_file_size) {
        stats->min_file_size = file_info.file_size;
    }
    
    if (stats->total_files == 1 || file_info.file_size > stats->max_file_size) {
        stats->max_file_size = file_info.file_size;
    }
    
    if (stats->total_files == 1 || file_info.create_timestamp < stats->oldest_file) {
        stats->oldest_file = file_info.create_timestamp;
    }
    
    if (stats->total_files == 1 || file_info.create_timestamp > stats->newest_file) {
        stats->newest_file = file_info.create_timestamp;
    }
    
    const char *extension = get_file_extension(file_id);
    update_extension_stats(stats, extension, file_info.file_size);
    update_size_bucket(stats, file_info.file_size);
    
    pthread_mutex_unlock(&stats->mutex);
    
    return 0;
}

static void *analysis_worker(void *arg) {
    AnalysisContext *ctx = (AnalysisContext *)arg;
    int index;
    char file_id[MAX_FILE_ID_LEN];
    int processed = 0;
    
    while (1) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->current_index >= ctx->file_count) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        index = ctx->current_index++;
        pthread_mutex_unlock(&ctx->mutex);
        
        strncpy(file_id, ctx->file_ids + index * MAX_FILE_ID_LEN, MAX_FILE_ID_LEN - 1);
        file_id[MAX_FILE_ID_LEN - 1] = '\0';
        
        analyze_file(ctx->pTrackerServer, file_id, ctx->stats, ctx->verbose);
        
        processed++;
        if (!ctx->verbose && processed % 100 == 0) {
            printf("\rAnalyzed: %lld files...", (long long)ctx->stats->total_files);
            fflush(stdout);
        }
    }
    
    return NULL;
}

static int compare_extensions(const void *a, const void *b) {
    const ExtensionStats *ext_a = (const ExtensionStats *)a;
    const ExtensionStats *ext_b = (const ExtensionStats *)b;
    
    if (ext_b->total_size > ext_a->total_size) return 1;
    if (ext_b->total_size < ext_a->total_size) return -1;
    return 0;
}

static void format_bytes(int64_t bytes, char *buf, size_t buf_size) {
    if (bytes >= 1099511627776LL) {
        snprintf(buf, buf_size, "%.2f TB", bytes / 1099511627776.0);
    } else if (bytes >= 1073741824LL) {
        snprintf(buf, buf_size, "%.2f GB", bytes / 1073741824.0);
    } else if (bytes >= 1048576LL) {
        snprintf(buf, buf_size, "%.2f MB", bytes / 1048576.0);
    } else if (bytes >= 1024LL) {
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    } else {
        snprintf(buf, buf_size, "%lld B", (long long)bytes);
    }
}

static void generate_analysis_report(AnalysisStats *stats, FILE *output) {
    char size_str[64];
    char time_str[64];
    struct tm *tm_info;
    
    fprintf(output, "\n");
    fprintf(output, "=== FastDFS Storage Analysis Report ===\n");
    fprintf(output, "\n");
    
    fprintf(output, "=== Overall Statistics ===\n");
    fprintf(output, "Total files: %lld\n", (long long)stats->total_files);
    
    format_bytes(stats->total_size, size_str, sizeof(size_str));
    fprintf(output, "Total size: %s (%lld bytes)\n", size_str, (long long)stats->total_size);
    
    if (stats->total_files > 0) {
        int64_t avg_size = stats->total_size / stats->total_files;
        format_bytes(avg_size, size_str, sizeof(size_str));
        fprintf(output, "Average file size: %s\n", size_str);
        
        format_bytes(stats->min_file_size, size_str, sizeof(size_str));
        fprintf(output, "Smallest file: %s\n", size_str);
        
        format_bytes(stats->max_file_size, size_str, sizeof(size_str));
        fprintf(output, "Largest file: %s\n", size_str);
        
        tm_info = localtime(&stats->oldest_file);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output, "Oldest file: %s\n", time_str);
        
        tm_info = localtime(&stats->newest_file);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output, "Newest file: %s\n", time_str);
    }
    
    fprintf(output, "\n=== File Size Distribution ===\n");
    for (int i = 0; i < SIZE_BUCKETS; i++) {
        if (stats->size_buckets[i].count > 0) {
            double percent = (stats->size_buckets[i].count * 100.0) / stats->total_files;
            format_bytes(stats->size_buckets[i].total_size, size_str, sizeof(size_str));
            fprintf(output, "%-15s: %6d files (%5.1f%%) - %s\n",
                   stats->size_buckets[i].label,
                   stats->size_buckets[i].count,
                   percent,
                   size_str);
        }
    }
    
    fprintf(output, "\n=== File Type Distribution (Top 20) ===\n");
    
    qsort(stats->extensions, stats->extension_count, sizeof(ExtensionStats),
          compare_extensions);
    
    int top_count = stats->extension_count < 20 ? stats->extension_count : 20;
    
    for (int i = 0; i < top_count; i++) {
        double percent = (stats->extensions[i].total_size * 100.0) / stats->total_size;
        format_bytes(stats->extensions[i].total_size, size_str, sizeof(size_str));
        fprintf(output, "%-10s: %6d files (%5.1f%%) - %s\n",
               stats->extensions[i].extension,
               stats->extensions[i].count,
               percent,
               size_str);
    }
    
    if (stats->extension_count > 20) {
        fprintf(output, "... and %d more extensions\n", stats->extension_count - 20);
    }
}

static int load_file_list(const char *list_file, char **file_ids, int *count) {
    FILE *fp;
    char line[MAX_FILE_ID_LEN];
    int capacity = 10000;
    int file_count = 0;
    char *id_array;
    
    fp = fopen(list_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Failed to open file list: %s\n", list_file);
        return errno;
    }
    
    id_array = (char *)malloc(capacity * MAX_FILE_ID_LEN);
    if (id_array == NULL) {
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
        
        if (file_count >= capacity) {
            capacity *= 2;
            id_array = (char *)realloc(id_array, capacity * MAX_FILE_ID_LEN);
            if (id_array == NULL) {
                fclose(fp);
                return ENOMEM;
            }
        }
        
        strncpy(id_array + file_count * MAX_FILE_ID_LEN, line, MAX_FILE_ID_LEN - 1);
        file_count++;
    }
    
    fclose(fp);
    
    *file_ids = id_array;
    *count = file_count;
    
    return 0;
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *list_file = NULL;
    char *output_file = NULL;
    int num_threads = 4;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    char *file_ids = NULL;
    int file_count = 0;
    AnalysisStats stats;
    AnalysisContext ctx;
    pthread_t *threads;
    FILE *output;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"file", required_argument, 0, 'f'},
        {"output", required_argument, 0, 'o'},
        {"threads", required_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:f:o:j:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'f':
                list_file = optarg;
                break;
            case 'o':
                output_file = optarg;
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
    
    if (list_file == NULL) {
        fprintf(stderr, "ERROR: File list required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    result = load_file_list(list_file, &file_ids, &file_count);
    if (result != 0) {
        return result;
    }
    
    if (file_count == 0) {
        printf("No files to analyze\n");
        free(file_ids);
        return 0;
    }
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        free(file_ids);
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        free(file_ids);
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    memset(&stats, 0, sizeof(stats));
    init_size_buckets(stats.size_buckets);
    pthread_mutex_init(&stats.mutex, NULL);
    
    printf("Analyzing %d files using %d threads...\n", file_count, num_threads);
    printf("\n");
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    memset(&ctx, 0, sizeof(ctx));
    ctx.file_ids = file_ids;
    ctx.file_count = file_count;
    ctx.current_index = 0;
    ctx.pTrackerServer = pTrackerServer;
    ctx.stats = &stats;
    ctx.verbose = verbose;
    pthread_mutex_init(&ctx.mutex, NULL);
    
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, analysis_worker, &ctx);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000LL +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
    
    if (!verbose) {
        printf("\n");
    }
    
    if (output_file != NULL) {
        output = fopen(output_file, "w");
        if (output == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            output = stdout;
        }
    } else {
        output = stdout;
    }
    
    generate_analysis_report(&stats, output);
    
    fprintf(output, "\nAnalysis completed in %lld ms (%.2f files/sec)\n",
           elapsed_ms, file_count * 1000.0 / elapsed_ms);
    
    if (output != stdout) {
        fclose(output);
        printf("\nReport saved to: %s\n", output_file);
    }
    
    free(file_ids);
    free(threads);
    pthread_mutex_destroy(&ctx.mutex);
    pthread_mutex_destroy(&stats.mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return 0;
}
