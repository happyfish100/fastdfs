/**
 * FastDFS Large Files Performance Benchmark
 * 
 * Tests performance with large files (>100MB)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include "fdfs_client.h"

#define MAX_THREADS 64
#define MIN_FILE_SIZE 104857600  // 100MB
#define MAX_FILE_SIZE 1073741824 // 1GB

typedef struct {
    int thread_id;
    int file_count;
    int min_size;
    int max_size;
    char *tracker_server;
    
    // Results
    int successful;
    int failed;
    double total_bytes;
    double total_time;
    double *latencies;
    int latency_count;
} thread_context_t;

// Global configuration
static int g_thread_count = 5;
static int g_file_count = 100;
static int g_min_size = MIN_FILE_SIZE;
static int g_max_size = MAX_FILE_SIZE;
static char g_tracker_server[256] = "127.0.0.1:22122";
static int g_verbose = 1;

// Global statistics
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_total_successful = 0;
static int g_total_failed = 0;
static double g_total_bytes = 0;

/**
 * Get current time in microseconds
 */
static double get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/**
 * Generate random data in chunks to avoid memory issues
 */
static char* generate_large_file_data(int size) {
    char *data = (char*)malloc(size);
    if (data == NULL) return NULL;
    
    // Fill in chunks to avoid stack overflow
    int chunk_size = 1024 * 1024; // 1MB chunks
    for (int offset = 0; offset < size; offset += chunk_size) {
        int current_chunk = (offset + chunk_size > size) ? (size - offset) : chunk_size;
        for (int i = 0; i < current_chunk; i++) {
            data[offset + i] = (char)(rand() % 256);
        }
    }
    
    return data;
}

/**
 * Compare function for qsort
 */
static int compare_double(const void *a, const void *b) {
    double diff = *(double*)a - *(double*)b;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

/**
 * Upload thread
 */
static void* upload_thread(void *arg) {
    thread_context_t *ctx = (thread_context_t*)arg;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
    
    if (fdfs_client_init(ctx->tracker_server) != 0) {
        fprintf(stderr, "Thread %d: Failed to initialize client\n", ctx->thread_id);
        return NULL;
    }
    
    ctx->latencies = (double*)malloc(ctx->file_count * sizeof(double));
    if (ctx->latencies == NULL) {
        fdfs_client_destroy();
        return NULL;
    }
    
    ctx->successful = 0;
    ctx->failed = 0;
    ctx->total_bytes = 0;
    ctx->latency_count = 0;
    
    double thread_start = get_time_us();
    
    for (int i = 0; i < ctx->file_count; i++) {
        // Random file size
        int file_size = ctx->min_size + (rand() % (ctx->max_size - ctx->min_size + 1));
        
        printf("Thread %d: Generating file %d/%d (%.2f MB)...\n", 
               ctx->thread_id, i + 1, ctx->file_count, file_size / (1024.0 * 1024.0));
        
        char *data = generate_large_file_data(file_size);
        if (data == NULL) {
            fprintf(stderr, "Thread %d: Failed to allocate %d bytes\n", 
                    ctx->thread_id, file_size);
            ctx->failed++;
            continue;
        }
        
        printf("Thread %d: Uploading file %d/%d...\n", 
               ctx->thread_id, i + 1, ctx->file_count);
        
        double start = get_time_us();
        int result = fdfs_upload_by_buffer(data, file_size, NULL, 
                                          group_name, remote_filename);
        double end = get_time_us();
        double latency_sec = (end - start) / 1000000.0;
        
        if (result == 0) {
            ctx->successful++;
            ctx->total_bytes += file_size;
            ctx->latencies[ctx->latency_count++] = latency_sec;
            
            double throughput = (file_size / (1024.0 * 1024.0)) / latency_sec;
            printf("Thread %d: File %d uploaded successfully (%.2f MB/s)\n", 
                   ctx->thread_id, i + 1, throughput);
        } else {
            ctx->failed++;
            fprintf(stderr, "Thread %d: Upload failed for file %d\n", 
                    ctx->thread_id, i + 1);
        }
        
        free(data);
    }
    
    double thread_end = get_time_us();
    ctx->total_time = (thread_end - thread_start) / 1000000.0;
    
    fdfs_client_destroy();
    
    pthread_mutex_lock(&g_stats_mutex);
    g_total_successful += ctx->successful;
    g_total_failed += ctx->failed;
    g_total_bytes += ctx->total_bytes;
    pthread_mutex_unlock(&g_stats_mutex);
    
    return NULL;
}

/**
 * Print results
 */
static void print_results(double total_time, double *all_latencies, int latency_count) {
    if (latency_count > 0) {
        qsort(all_latencies, latency_count, sizeof(double), compare_double);
    }
    
    double mean = 0;
    for (int i = 0; i < latency_count; i++) {
        mean += all_latencies[i];
    }
    if (latency_count > 0) mean /= latency_count;
    
    double throughput_mbps = (g_total_bytes / (1024 * 1024)) / total_time;
    double avg_file_size_mb = (g_total_bytes / g_total_successful) / (1024 * 1024);
    
    printf("{\n");
    printf("  \"benchmark\": \"large_files\",\n");
    printf("  \"timestamp\": \"%ld\",\n", time(NULL));
    printf("  \"configuration\": {\n");
    printf("    \"threads\": %d,\n", g_thread_count);
    printf("    \"file_count\": %d,\n", g_file_count);
    printf("    \"min_size_mb\": %.2f,\n", g_min_size / (1024.0 * 1024.0));
    printf("    \"max_size_mb\": %.2f\n", g_max_size / (1024.0 * 1024.0));
    printf("  },\n");
    printf("  \"metrics\": {\n");
    printf("    \"throughput_mbps\": %.2f,\n", throughput_mbps);
    printf("    \"latency_seconds\": {\n");
    printf("      \"mean\": %.2f,\n", mean);
    if (latency_count > 0) {
        printf("      \"p50\": %.2f,\n", all_latencies[(int)(latency_count * 0.50)]);
        printf("      \"p95\": %.2f,\n", all_latencies[(int)(latency_count * 0.95)]);
        printf("      \"p99\": %.2f,\n", all_latencies[(int)(latency_count * 0.99)]);
        printf("      \"min\": %.2f,\n", all_latencies[0]);
        printf("      \"max\": %.2f\n", all_latencies[latency_count - 1]);
    } else {
        printf("      \"p50\": 0,\n");
        printf("      \"p95\": 0,\n");
        printf("      \"p99\": 0,\n");
        printf("      \"min\": 0,\n");
        printf("      \"max\": 0\n");
    }
    printf("    },\n");
    printf("    \"operations\": {\n");
    printf("      \"successful\": %d,\n", g_total_successful);
    printf("      \"failed\": %d,\n", g_total_failed);
    printf("      \"success_rate\": %.2f\n", 
           (g_total_successful + g_total_failed > 0) ? 
           (double)g_total_successful / (g_total_successful + g_total_failed) * 100 : 0);
    printf("    },\n");
    printf("    \"duration_seconds\": %.2f,\n", total_time);
    printf("    \"total_gb\": %.2f,\n", g_total_bytes / (1024 * 1024 * 1024));
    printf("    \"avg_file_size_mb\": %.2f\n", avg_file_size_mb);
    printf("  }\n");
    printf("}\n");
}

/**
 * Print usage
 */
static void print_usage(const char *program) {
    printf("Usage: %s [OPTIONS]\n", program);
    printf("\nOptions:\n");
    printf("  -t, --threads NUM      Number of threads (default: 5)\n");
    printf("  -c, --count NUM        Number of files (default: 100)\n");
    printf("  -m, --min-size BYTES   Minimum file size (default: 104857600)\n");
    printf("  -M, --max-size BYTES   Maximum file size (default: 1073741824)\n");
    printf("  -T, --tracker SERVER   Tracker server (default: 127.0.0.1:22122)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show help\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    pthread_t threads[MAX_THREADS];
    thread_context_t contexts[MAX_THREADS];
    
    static struct option long_options[] = {
        {"threads", required_argument, 0, 't'},
        {"count", required_argument, 0, 'c'},
        {"min-size", required_argument, 0, 'm'},
        {"max-size", required_argument, 0, 'M'},
        {"tracker", required_argument, 0, 'T'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "t:c:m:M:T:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 't': g_thread_count = atoi(optarg); break;
            case 'c': g_file_count = atoi(optarg); break;
            case 'm': g_min_size = atoi(optarg); break;
            case 'M': g_max_size = atoi(optarg); break;
            case 'T': strncpy(g_tracker_server, optarg, sizeof(g_tracker_server) - 1); break;
            case 'v': g_verbose = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }
    
    printf("FastDFS Large Files Benchmark\n");
    printf("==============================\n");
    printf("Threads: %d\n", g_thread_count);
    printf("Files: %d\n", g_file_count);
    printf("Size range: %.2f - %.2f MB\n", 
           g_min_size / (1024.0 * 1024.0), g_max_size / (1024.0 * 1024.0));
    printf("Tracker: %s\n\n", g_tracker_server);
    
    srand(time(NULL));
    
    int files_per_thread = g_file_count / g_thread_count;
    int remaining = g_file_count % g_thread_count;
    
    for (int i = 0; i < g_thread_count; i++) {
        contexts[i].thread_id = i;
        contexts[i].file_count = files_per_thread + (i < remaining ? 1 : 0);
        contexts[i].min_size = g_min_size;
        contexts[i].max_size = g_max_size;
        contexts[i].tracker_server = g_tracker_server;
    }
    
    printf("Starting benchmark...\n");
    double start_time = get_time_us();
    
    for (int i = 0; i < g_thread_count; i++) {
        pthread_create(&threads[i], NULL, upload_thread, &contexts[i]);
    }
    
    for (int i = 0; i < g_thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double end_time = get_time_us();
    double total_time = (end_time - start_time) / 1000000.0;
    
    // Collect all latencies
    double *all_latencies = (double*)malloc(g_file_count * sizeof(double));
    int latency_count = 0;
    
    for (int i = 0; i < g_thread_count; i++) {
        for (int j = 0; j < contexts[i].latency_count; j++) {
            all_latencies[latency_count++] = contexts[i].latencies[j];
        }
        free(contexts[i].latencies);
    }
    
    printf("\nBenchmark complete!\n\n");
    print_results(total_time, all_latencies, latency_count);
    
    free(all_latencies);
    return 0;
}
