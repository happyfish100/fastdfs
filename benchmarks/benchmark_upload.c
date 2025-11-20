/**
 * FastDFS Upload Performance Benchmark
 * 
 * Measures upload throughput, IOPS, and latency percentiles
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
#define MAX_LATENCIES 1000000
#define DEFAULT_FILE_SIZE 1048576  // 1MB

typedef struct {
    int thread_id;
    int files_to_upload;
    int file_size;
    char *tracker_server;
    
    // Results
    int successful_uploads;
    int failed_uploads;
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
static int g_file_count = 100;
static int g_file_size = DEFAULT_FILE_SIZE;
static char g_tracker_server[256] = "127.0.0.1:22122";
static char g_config_file[256] = "config/benchmark.conf";
static int g_warmup_enabled = 1;
static int g_warmup_duration = 10;
static int g_verbose = 0;

// Global statistics
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_total_uploads = 0;
static int g_successful_uploads = 0;
static int g_failed_uploads = 0;
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
 * Generate random data for file upload
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
 * Compare function for qsort (for percentile calculation)
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
    
    // Sort latencies
    qsort(latencies, count, sizeof(double), compare_double);
    
    // Calculate percentiles
    stats->min = latencies[0];
    stats->max = latencies[count - 1];
    stats->p50 = stats->median = latencies[(int)(count * 0.50)];
    stats->p75 = latencies[(int)(count * 0.75)];
    stats->p90 = latencies[(int)(count * 0.90)];
    stats->p95 = latencies[(int)(count * 0.95)];
    stats->p99 = latencies[(int)(count * 0.99)];
    stats->p999 = latencies[(int)(count * 0.999)];
    
    // Calculate mean
    double sum = 0;
    for (int i = 0; i < count; i++) {
        sum += latencies[i];
    }
    stats->mean = sum / count;
    
    // Calculate standard deviation
    double variance = 0;
    for (int i = 0; i < count; i++) {
        double diff = latencies[i] - stats->mean;
        variance += diff * diff;
    }
    stats->stddev = sqrt(variance / count);
}

/**
 * Upload thread function
 */
static void* upload_thread(void *arg) {
    thread_context_t *ctx = (thread_context_t*)arg;
    FDFSClient client;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
    char *file_data = NULL;
    int result;
    
    // Initialize client
    if (fdfs_client_init(ctx->tracker_server) != 0) {
        fprintf(stderr, "Thread %d: Failed to initialize FDFS client\n", ctx->thread_id);
        return NULL;
    }
    
    // Allocate latency array
    ctx->latencies = (double*)malloc(ctx->files_to_upload * sizeof(double));
    if (ctx->latencies == NULL) {
        fprintf(stderr, "Thread %d: Failed to allocate latency array\n", ctx->thread_id);
        fdfs_client_destroy();
        return NULL;
    }
    
    ctx->latency_count = 0;
    ctx->successful_uploads = 0;
    ctx->failed_uploads = 0;
    ctx->total_bytes = 0;
    
    // Generate file data once
    file_data = generate_random_data(ctx->file_size);
    if (file_data == NULL) {
        fprintf(stderr, "Thread %d: Failed to generate file data\n", ctx->thread_id);
        free(ctx->latencies);
        fdfs_client_destroy();
        return NULL;
    }
    
    double thread_start = get_time_us();
    
    // Upload files
    for (int i = 0; i < ctx->files_to_upload; i++) {
        double start_time = get_time_us();
        
        // Upload file
        result = fdfs_upload_by_buffer(file_data, ctx->file_size, 
                                       NULL, group_name, remote_filename);
        
        double end_time = get_time_us();
        double latency_ms = (end_time - start_time) / 1000.0;
        
        if (result == 0) {
            ctx->successful_uploads++;
            ctx->total_bytes += ctx->file_size;
            ctx->latencies[ctx->latency_count++] = latency_ms;
            
            if (g_verbose && (i % 100 == 0)) {
                printf("Thread %d: Uploaded %d/%d files (%.2f ms)\n", 
                       ctx->thread_id, i + 1, ctx->files_to_upload, latency_ms);
            }
        } else {
            ctx->failed_uploads++;
            if (g_verbose) {
                fprintf(stderr, "Thread %d: Upload failed: %s\n", 
                        ctx->thread_id, strerror(errno));
            }
        }
    }
    
    double thread_end = get_time_us();
    ctx->total_time = (thread_end - thread_start) / 1000000.0;
    
    // Cleanup
    free(file_data);
    fdfs_client_destroy();
    
    // Update global statistics
    pthread_mutex_lock(&g_stats_mutex);
    g_successful_uploads += ctx->successful_uploads;
    g_failed_uploads += ctx->failed_uploads;
    g_total_bytes += ctx->total_bytes;
    pthread_mutex_unlock(&g_stats_mutex);
    
    return NULL;
}

/**
 * Run warmup phase
 */
static void run_warmup() {
    if (!g_warmup_enabled || g_warmup_duration <= 0) {
        return;
    }
    
    printf("Running warmup phase for %d seconds...\n", g_warmup_duration);
    
    thread_context_t warmup_ctx;
    warmup_ctx.thread_id = 0;
    warmup_ctx.files_to_upload = 10;
    warmup_ctx.file_size = g_file_size;
    warmup_ctx.tracker_server = g_tracker_server;
    
    pthread_t warmup_thread;
    pthread_create(&warmup_thread, NULL, upload_thread, &warmup_ctx);
    
    sleep(g_warmup_duration);
    
    pthread_join(warmup_thread, NULL);
    
    if (warmup_ctx.latencies) {
        free(warmup_ctx.latencies);
    }
    
    printf("Warmup complete.\n\n");
}

/**
 * Print results in JSON format
 */
static void print_results(double total_time, latency_stats_t *stats) {
    double throughput_mbps = (g_total_bytes / (1024 * 1024)) / total_time;
    double iops = g_successful_uploads / total_time;
    double success_rate = (double)g_successful_uploads / g_total_uploads * 100.0;
    
    printf("{\n");
    printf("  \"benchmark\": \"upload\",\n");
    printf("  \"timestamp\": \"%ld\",\n", time(NULL));
    printf("  \"configuration\": {\n");
    printf("    \"threads\": %d,\n", g_thread_count);
    printf("    \"file_count\": %d,\n", g_file_count);
    printf("    \"file_size\": %d,\n", g_file_size);
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
    printf("      \"total\": %d,\n", g_total_uploads);
    printf("      \"successful\": %d,\n", g_successful_uploads);
    printf("      \"failed\": %d,\n", g_failed_uploads);
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
    printf("  -f, --files NUM        Number of files to upload (default: 100)\n");
    printf("  -s, --size BYTES       File size in bytes (default: 1048576)\n");
    printf("  -c, --config FILE      Configuration file (default: config/benchmark.conf)\n");
    printf("  -T, --tracker SERVER   Tracker server (default: 127.0.0.1:22122)\n");
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
        {"files", required_argument, 0, 'f'},
        {"size", required_argument, 0, 's'},
        {"config", required_argument, 0, 'c'},
        {"tracker", required_argument, 0, 'T'},
        {"warmup", required_argument, 0, 'w'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "t:f:s:c:T:w:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 't':
                g_thread_count = atoi(optarg);
                break;
            case 'f':
                g_file_count = atoi(optarg);
                break;
            case 's':
                g_file_size = atoi(optarg);
                break;
            case 'c':
                strncpy(g_config_file, optarg, sizeof(g_config_file) - 1);
                break;
            case 'T':
                strncpy(g_tracker_server, optarg, sizeof(g_tracker_server) - 1);
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
    
    // Validate parameters
    if (g_thread_count < 1 || g_thread_count > MAX_THREADS) {
        fprintf(stderr, "Error: Thread count must be between 1 and %d\n", MAX_THREADS);
        return 1;
    }
    
    if (g_file_count < 1) {
        fprintf(stderr, "Error: File count must be at least 1\n");
        return 1;
    }
    
    if (g_file_size < 1) {
        fprintf(stderr, "Error: File size must be at least 1 byte\n");
        return 1;
    }
    
    printf("FastDFS Upload Performance Benchmark\n");
    printf("=====================================\n");
    printf("Threads: %d\n", g_thread_count);
    printf("Files per thread: %d\n", g_file_count / g_thread_count);
    printf("File size: %d bytes (%.2f MB)\n", g_file_size, g_file_size / (1024.0 * 1024.0));
    printf("Total files: %d\n", g_file_count);
    printf("Tracker: %s\n\n", g_tracker_server);
    
    // Initialize random seed
    srand(time(NULL));
    
    // Run warmup
    run_warmup();
    
    // Allocate global latency array
    g_all_latencies = (double*)malloc(g_file_count * sizeof(double));
    if (g_all_latencies == NULL) {
        fprintf(stderr, "Error: Failed to allocate latency array\n");
        return 1;
    }
    
    // Initialize thread contexts
    int files_per_thread = g_file_count / g_thread_count;
    int remaining_files = g_file_count % g_thread_count;
    
    for (int i = 0; i < g_thread_count; i++) {
        contexts[i].thread_id = i;
        contexts[i].files_to_upload = files_per_thread + (i < remaining_files ? 1 : 0);
        contexts[i].file_size = g_file_size;
        contexts[i].tracker_server = g_tracker_server;
    }
    
    // Start benchmark
    printf("Starting benchmark...\n");
    double start_time = get_time_us();
    
    // Create threads
    for (int i = 0; i < g_thread_count; i++) {
        if (pthread_create(&threads[i], NULL, upload_thread, &contexts[i]) != 0) {
            fprintf(stderr, "Error: Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    // Wait for threads to complete
    for (int i = 0; i < g_thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double end_time = get_time_us();
    double total_time = (end_time - start_time) / 1000000.0;
    
    // Collect all latencies
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
    free(g_all_latencies);
    
    return 0;
}
