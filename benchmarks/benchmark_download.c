/**
 * FastDFS Download Performance Benchmark
 * 
 * Measures download throughput, IOPS, and latency percentiles
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include "fdfs_client.h"
#include "logger.h"

#define MAX_THREADS 1024
#define MAX_FILES 100000
#define MAX_FILENAME_LEN 256

typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[MAX_FILENAME_LEN];
    int file_size;
} file_info_t;

typedef struct {
    int thread_id;
    int iterations;
    char *tracker_server;
    file_info_t *files;
    int file_count;
    
    // Results
    int successful_downloads;
    int failed_downloads;
    double *latencies;
    int latency_count;
    double total_bytes;
    double total_time;
} thread_context_t;

typedef struct {
    double mean;
    double median;
    double p50;
    double p75;
    double p90;
    double p95;
    double p99;
    double p999;
    double min;
    double max;
    double stddev;
} latency_stats_t;

// Global configuration
static int g_thread_count = 1;
static int g_iterations = 100;
static char g_tracker_server[256] = "127.0.0.1:22122";
static char g_file_list[256] = "";
static int g_warmup_enabled = 1;
static int g_warmup_duration = 10;
static int g_verbose = 0;
static int g_prepare_files = 1;
static int g_prepare_count = 100;
static int g_prepare_size = 1048576;

// Global file list
static file_info_t *g_files = NULL;
static int g_file_count = 0;

// Global statistics
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_total_downloads = 0;
static int g_successful_downloads = 0;
static int g_failed_downloads = 0;
static double g_total_bytes = 0;
static double *g_all_latencies = NULL;
static int g_latency_count = 0;

/**
 * Get current time in microseconds
 */
static double get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/**
 * Generate random data
 */
static char* generate_random_data(int size) {
    char *data = (char*)malloc(size);
    if (data == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < size; i++) {
        data[i] = (char)(rand() % 256);
    }
    
    return data;
}

/**
 * Prepare test files by uploading them
 */
static int prepare_test_files() {
    printf("Preparing %d test files...\n", g_prepare_count);
    
    if (fdfs_client_init(g_tracker_server) != 0) {
        fprintf(stderr, "Failed to initialize FDFS client\n");
        return -1;
    }
    
    g_files = (file_info_t*)malloc(g_prepare_count * sizeof(file_info_t));
    if (g_files == NULL) {
        fprintf(stderr, "Failed to allocate file info array\n");
        fdfs_client_destroy();
        return -1;
    }
    
    char *file_data = generate_random_data(g_prepare_size);
    if (file_data == NULL) {
        fprintf(stderr, "Failed to generate file data\n");
        free(g_files);
        fdfs_client_destroy();
        return -1;
    }
    
    g_file_count = 0;
    for (int i = 0; i < g_prepare_count; i++) {
        int result = fdfs_upload_by_buffer(file_data, g_prepare_size, NULL,
                                          g_files[g_file_count].group_name,
                                          g_files[g_file_count].remote_filename);
        
        if (result == 0) {
            g_files[g_file_count].file_size = g_prepare_size;
            g_file_count++;
            
            if ((i + 1) % 10 == 0) {
                printf("  Uploaded %d/%d files\r", i + 1, g_prepare_count);
                fflush(stdout);
            }
        } else {
            fprintf(stderr, "\nFailed to upload file %d: %s\n", i, strerror(errno));
        }
    }
    
    printf("\nPrepared %d test files successfully.\n\n", g_file_count);
    
    free(file_data);
    fdfs_client_destroy();
    
    return g_file_count > 0 ? 0 : -1;
}

/**
 * Load file list from file
 */
static int load_file_list(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file list: %s\n", filename);
        return -1;
    }
    
    // Count lines
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        count++;
    }
    
    if (count == 0) {
        fprintf(stderr, "File list is empty\n");
        fclose(fp);
        return -1;
    }
    
    // Allocate array
    g_files = (file_info_t*)malloc(count * sizeof(file_info_t));
    if (g_files == NULL) {
        fprintf(stderr, "Failed to allocate file info array\n");
        fclose(fp);
        return -1;
    }
    
    // Read file info
    rewind(fp);
    g_file_count = 0;
    while (fgets(line, sizeof(line), fp) != NULL && g_file_count < count) {
        // Parse: group_name/remote_filename size
        char *token = strtok(line, " \t\n");
        if (token == NULL) continue;
        
        // Split group and filename
        char *slash = strchr(token, '/');
        if (slash == NULL) continue;
        
        int group_len = slash - token;
        strncpy(g_files[g_file_count].group_name, token, group_len);
        g_files[g_file_count].group_name[group_len] = '\0';
        
        strcpy(g_files[g_file_count].remote_filename, slash + 1);
        
        // Get file size
        token = strtok(NULL, " \t\n");
        g_files[g_file_count].file_size = token ? atoi(token) : 0;
        
        g_file_count++;
    }
    
    fclose(fp);
    printf("Loaded %d files from %s\n\n", g_file_count, filename);
    
    return 0;
}

/**
 * Compare function for qsort
 */
static int compare_double(const void *a, const void *b) {
    double diff = *(double*)a - *(double*)b;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

/**
 * Calculate latency statistics
 */
static void calculate_latency_stats(double *latencies, int count, latency_stats_t *stats) {
    if (count == 0) {
        memset(stats, 0, sizeof(latency_stats_t));
        return;
    }
    
    qsort(latencies, count, sizeof(double), compare_double);
    
    stats->min = latencies[0];
    stats->max = latencies[count - 1];
    stats->p50 = stats->median = latencies[(int)(count * 0.50)];
    stats->p75 = latencies[(int)(count * 0.75)];
    stats->p90 = latencies[(int)(count * 0.90)];
    stats->p95 = latencies[(int)(count * 0.95)];
    stats->p99 = latencies[(int)(count * 0.99)];
    stats->p999 = latencies[(int)(count * 0.999)];
    
    double sum = 0;
    for (int i = 0; i < count; i++) {
        sum += latencies[i];
    }
    stats->mean = sum / count;
    
    double variance = 0;
    for (int i = 0; i < count; i++) {
        double diff = latencies[i] - stats->mean;
        variance += diff * diff;
    }
    stats->stddev = sqrt(variance / count);
}

/**
 * Download thread function
 */
static void* download_thread(void *arg) {
    thread_context_t *ctx = (thread_context_t*)arg;
    char *buffer = NULL;
    int64_t file_size = 0;
    int result;
    
    // Initialize client
    if (fdfs_client_init(ctx->tracker_server) != 0) {
        fprintf(stderr, "Thread %d: Failed to initialize FDFS client\n", ctx->thread_id);
        return NULL;
    }
    
    // Allocate latency array
    ctx->latencies = (double*)malloc(ctx->iterations * sizeof(double));
    if (ctx->latencies == NULL) {
        fprintf(stderr, "Thread %d: Failed to allocate latency array\n", ctx->thread_id);
        fdfs_client_destroy();
        return NULL;
    }
    
    ctx->latency_count = 0;
    ctx->successful_downloads = 0;
    ctx->failed_downloads = 0;
    ctx->total_bytes = 0;
    
    double thread_start = get_time_us();
    
    // Download files
    for (int i = 0; i < ctx->iterations; i++) {
        // Select random file
        int file_idx = rand() % ctx->file_count;
        file_info_t *file = &ctx->files[file_idx];
        
        double start_time = get_time_us();
        
        // Download file
        result = fdfs_download_file_to_buffer(file->group_name, file->remote_filename,
                                             &buffer, &file_size);
        
        double end_time = get_time_us();
        double latency_ms = (end_time - start_time) / 1000.0;
        
        if (result == 0 && buffer != NULL) {
            ctx->successful_downloads++;
            ctx->total_bytes += file_size;
            ctx->latencies[ctx->latency_count++] = latency_ms;
            
            free(buffer);
            buffer = NULL;
            
            if (g_verbose && (i % 100 == 0)) {
                printf("Thread %d: Downloaded %d/%d files (%.2f ms)\n", 
                       ctx->thread_id, i + 1, ctx->iterations, latency_ms);
            }
        } else {
            ctx->failed_downloads++;
            if (g_verbose) {
                fprintf(stderr, "Thread %d: Download failed: %s\n", 
                        ctx->thread_id, strerror(errno));
            }
        }
    }
    
    double thread_end = get_time_us();
    ctx->total_time = (thread_end - thread_start) / 1000000.0;
    
    // Cleanup
    fdfs_client_destroy();
    
    // Update global statistics
    pthread_mutex_lock(&g_stats_mutex);
    g_successful_downloads += ctx->successful_downloads;
    g_failed_downloads += ctx->failed_downloads;
    g_total_bytes += ctx->total_bytes;
    pthread_mutex_unlock(&g_stats_mutex);
    
    return NULL;
}

/**
 * Print results in JSON format
 */
static void print_results(double total_time, latency_stats_t *stats) {
    double throughput_mbps = (g_total_bytes / (1024 * 1024)) / total_time;
    double iops = g_successful_downloads / total_time;
    double success_rate = (double)g_successful_downloads / g_total_downloads * 100.0;
    
    printf("{\n");
    printf("  \"benchmark\": \"download\",\n");
    printf("  \"timestamp\": \"%ld\",\n", time(NULL));
    printf("  \"configuration\": {\n");
    printf("    \"threads\": %d,\n", g_thread_count);
    printf("    \"iterations\": %d,\n", g_iterations);
    printf("    \"file_count\": %d,\n", g_file_count);
    printf("    \"tracker_server\": \"%s\"\n", g_tracker_server);
    printf("  },\n");
    printf("  \"metrics\": {\n");
    printf("    \"throughput_mbps\": %.2f,\n", throughput_mbps);
    printf("    \"iops\": %.2f,\n", iops);
    printf("    \"latency_ms\": {\n");
    printf("      \"mean\": %.2f,\n", stats->mean);
    printf("      \"median\": %.2f,\n", stats->median);
    printf("      \"p50\": %.2f,\n", stats->p50);
    printf("      \"p75\": %.2f,\n", stats->p75);
    printf("      \"p90\": %.2f,\n", stats->p90);
    printf("      \"p95\": %.2f,\n", stats->p95);
    printf("      \"p99\": %.2f,\n", stats->p99);
    printf("      \"p999\": %.2f,\n", stats->p999);
    printf("      \"min\": %.2f,\n", stats->min);
    printf("      \"max\": %.2f,\n", stats->max);
    printf("      \"stddev\": %.2f\n", stats->stddev);
    printf("    },\n");
    printf("    \"operations\": {\n");
    printf("      \"total\": %d,\n", g_total_downloads);
    printf("      \"successful\": %d,\n", g_successful_downloads);
    printf("      \"failed\": %d,\n", g_failed_downloads);
    printf("      \"success_rate\": %.2f\n", success_rate);
    printf("    },\n");
    printf("    \"duration_seconds\": %.2f,\n", total_time);
    printf("    \"total_mb\": %.2f\n", g_total_bytes / (1024 * 1024));
    printf("  }\n");
    printf("}\n");
}

/**
 * Print usage information
 */
static void print_usage(const char *program) {
    printf("Usage: %s [OPTIONS]\n", program);
    printf("\nOptions:\n");
    printf("  -t, --threads NUM      Number of concurrent threads (default: 1)\n");
    printf("  -i, --iterations NUM   Number of download iterations (default: 100)\n");
    printf("  -T, --tracker SERVER   Tracker server (default: 127.0.0.1:22122)\n");
    printf("  -f, --file-list FILE   File containing list of files to download\n");
    printf("  -p, --prepare NUM      Prepare NUM test files (default: 100)\n");
    printf("  -s, --size BYTES       Size of prepared files (default: 1048576)\n");
    printf("  -w, --warmup SECONDS   Warmup duration (default: 10, 0 to disable)\n");
    printf("  -v, --verbose          Enable verbose output\n");
    printf("  -h, --help             Show this help message\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    pthread_t threads[MAX_THREADS];
    thread_context_t contexts[MAX_THREADS];
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"threads", required_argument, 0, 't'},
        {"iterations", required_argument, 0, 'i'},
        {"tracker", required_argument, 0, 'T'},
        {"file-list", required_argument, 0, 'f'},
        {"prepare", required_argument, 0, 'p'},
        {"size", required_argument, 0, 's'},
        {"warmup", required_argument, 0, 'w'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "t:i:T:f:p:s:w:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 't':
                g_thread_count = atoi(optarg);
                break;
            case 'i':
                g_iterations = atoi(optarg);
                break;
            case 'T':
                strncpy(g_tracker_server, optarg, sizeof(g_tracker_server) - 1);
                break;
            case 'f':
                strncpy(g_file_list, optarg, sizeof(g_file_list) - 1);
                g_prepare_files = 0;
                break;
            case 'p':
                g_prepare_count = atoi(optarg);
                break;
            case 's':
                g_prepare_size = atoi(optarg);
                break;
            case 'w':
                g_warmup_duration = atoi(optarg);
                break;
            case 'v':
                g_verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("FastDFS Download Performance Benchmark\n");
    printf("=======================================\n");
    printf("Threads: %d\n", g_thread_count);
    printf("Iterations per thread: %d\n", g_iterations / g_thread_count);
    printf("Total downloads: %d\n", g_iterations);
    printf("Tracker: %s\n\n", g_tracker_server);
    
    srand(time(NULL));
    
    // Prepare or load files
    if (g_prepare_files) {
        if (prepare_test_files() != 0) {
            return 1;
        }
    } else {
        if (load_file_list(g_file_list) != 0) {
            return 1;
        }
    }
    
    if (g_file_count == 0) {
        fprintf(stderr, "Error: No files available for download\n");
        return 1;
    }
    
    // Allocate global latency array
    g_all_latencies = (double*)malloc(g_iterations * sizeof(double));
    if (g_all_latencies == NULL) {
        fprintf(stderr, "Error: Failed to allocate latency array\n");
        free(g_files);
        return 1;
    }
    
    // Initialize thread contexts
    int iterations_per_thread = g_iterations / g_thread_count;
    int remaining_iterations = g_iterations % g_thread_count;
    
    for (int i = 0; i < g_thread_count; i++) {
        contexts[i].thread_id = i;
        contexts[i].iterations = iterations_per_thread + (i < remaining_iterations ? 1 : 0);
        contexts[i].tracker_server = g_tracker_server;
        contexts[i].files = g_files;
        contexts[i].file_count = g_file_count;
    }
    
    // Start benchmark
    printf("Starting benchmark...\n");
    double start_time = get_time_us();
    
    // Create threads
    for (int i = 0; i < g_thread_count; i++) {
        if (pthread_create(&threads[i], NULL, download_thread, &contexts[i]) != 0) {
            fprintf(stderr, "Error: Failed to create thread %d\n", i);
            free(g_files);
            free(g_all_latencies);
            return 1;
        }
    }
    
    // Wait for threads
    for (int i = 0; i < g_thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double end_time = get_time_us();
    double total_time = (end_time - start_time) / 1000000.0;
    
    // Collect latencies
    g_latency_count = 0;
    for (int i = 0; i < g_thread_count; i++) {
        for (int j = 0; j < contexts[i].latency_count; j++) {
            g_all_latencies[g_latency_count++] = contexts[i].latencies[j];
        }
        free(contexts[i].latencies);
    }
    
    // Calculate statistics
    latency_stats_t stats;
    calculate_latency_stats(g_all_latencies, g_latency_count, &stats);
    
    // Print results
    printf("\nBenchmark complete!\n\n");
    print_results(total_time, &stats);
    
    // Cleanup
    free(g_files);
    free(g_all_latencies);
    
    return 0;
}
