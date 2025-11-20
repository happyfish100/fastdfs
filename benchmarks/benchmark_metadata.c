/**
 * FastDFS Metadata Operations Benchmark
 * 
 * Tests performance of metadata query, update, and delete operations
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

#define MAX_THREADS 1024
#define MAX_FILES 10000
#define MAX_METADATA_COUNT 10

typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[256];
} file_info_t;

typedef struct {
    int thread_id;
    int operation_count;
    char *tracker_server;
    file_info_t *files;
    int file_count;
    int query_ratio;
    int update_ratio;
    int delete_ratio;
    
    // Results
    int query_count;
    int update_count;
    int delete_count;
    int query_success;
    int update_success;
    int delete_success;
    double *query_latencies;
    double *update_latencies;
    double *delete_latencies;
    int query_latency_count;
    int update_latency_count;
    int delete_latency_count;
} thread_context_t;

// Global configuration
static int g_thread_count = 10;
static int g_operation_count = 10000;
static char g_tracker_server[256] = "127.0.0.1:22122";
static int g_query_ratio = 70;
static int g_update_ratio = 20;
static int g_delete_ratio = 10;
static int g_prepare_files = 100;
static int g_verbose = 0;

// Global file list
static file_info_t *g_files = NULL;
static int g_file_count = 0;

// Global statistics
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_total_queries = 0;
static int g_total_updates = 0;
static int g_total_deletes = 0;
static int g_successful_queries = 0;
static int g_successful_updates = 0;
static int g_successful_deletes = 0;

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
    if (data == NULL) return NULL;
    
    for (int i = 0; i < size; i++) {
        data[i] = (char)(rand() % 256);
    }
    return data;
}

/**
 * Prepare test files
 */
static int prepare_test_files() {
    printf("Preparing %d test files with metadata...\n", g_prepare_files);
    
    if (fdfs_client_init(g_tracker_server) != 0) {
        fprintf(stderr, "Failed to initialize FDFS client\n");
        return -1;
    }
    
    g_files = (file_info_t*)malloc(g_prepare_files * sizeof(file_info_t));
    if (g_files == NULL) {
        fdfs_client_destroy();
        return -1;
    }
    
    char *file_data = generate_random_data(1024);
    if (file_data == NULL) {
        free(g_files);
        fdfs_client_destroy();
        return -1;
    }
    
    g_file_count = 0;
    for (int i = 0; i < g_prepare_files; i++) {
        int result = fdfs_upload_by_buffer(file_data, 1024, NULL,
                                          g_files[g_file_count].group_name,
                                          g_files[g_file_count].remote_filename);
        
        if (result == 0) {
            // Set initial metadata
            FDFSMetaData meta_list[3];
            snprintf(meta_list[0].name, sizeof(meta_list[0].name), "author");
            snprintf(meta_list[0].value, sizeof(meta_list[0].value), "benchmark");
            snprintf(meta_list[1].name, sizeof(meta_list[1].name), "version");
            snprintf(meta_list[1].value, sizeof(meta_list[1].value), "1.0");
            snprintf(meta_list[2].name, sizeof(meta_list[2].name), "timestamp");
            snprintf(meta_list[2].value, sizeof(meta_list[2].value), "%ld", time(NULL));
            
            fdfs_set_metadata(g_files[g_file_count].group_name,
                            g_files[g_file_count].remote_filename,
                            meta_list, 3, FDFS_METADATA_OVERWRITE);
            
            g_file_count++;
            
            if ((i + 1) % 10 == 0) {
                printf("  Prepared %d/%d files\r", i + 1, g_prepare_files);
                fflush(stdout);
            }
        }
    }
    
    printf("\nPrepared %d test files successfully.\n\n", g_file_count);
    
    free(file_data);
    fdfs_client_destroy();
    
    return g_file_count > 0 ? 0 : -1;
}

/**
 * Compare function for qsort
 */
static int compare_double(const void *a, const void *b) {
    double diff = *(double*)a - *(double*)b;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

/**
 * Perform metadata query
 */
static int perform_query(file_info_t *file, double *latency) {
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    
    double start = get_time_us();
    int result = fdfs_get_metadata(file->group_name, file->remote_filename,
                                   &meta_list, &meta_count);
    double end = get_time_us();
    *latency = (end - start) / 1000.0;
    
    if (result == 0 && meta_list != NULL) {
        free(meta_list);
        return 0;
    }
    
    return -1;
}

/**
 * Perform metadata update
 */
static int perform_update(file_info_t *file, double *latency) {
    FDFSMetaData meta_list[2];
    snprintf(meta_list[0].name, sizeof(meta_list[0].name), "updated");
    snprintf(meta_list[0].value, sizeof(meta_list[0].value), "%ld", time(NULL));
    snprintf(meta_list[1].name, sizeof(meta_list[1].name), "counter");
    snprintf(meta_list[1].value, sizeof(meta_list[1].value), "%d", rand() % 1000);
    
    double start = get_time_us();
    int result = fdfs_set_metadata(file->group_name, file->remote_filename,
                                   meta_list, 2, FDFS_METADATA_MERGE);
    double end = get_time_us();
    *latency = (end - start) / 1000.0;
    
    return result;
}

/**
 * Perform metadata delete
 */
static int perform_delete(file_info_t *file, double *latency) {
    FDFSMetaData meta_list[1];
    snprintf(meta_list[0].name, sizeof(meta_list[0].name), "counter");
    meta_list[0].value[0] = '\0';
    
    double start = get_time_us();
    int result = fdfs_set_metadata(file->group_name, file->remote_filename,
                                   meta_list, 1, FDFS_METADATA_OVERWRITE);
    double end = get_time_us();
    *latency = (end - start) / 1000.0;
    
    return result;
}

/**
 * Metadata operations thread
 */
static void* metadata_thread(void *arg) {
    thread_context_t *ctx = (thread_context_t*)arg;
    
    if (fdfs_client_init(ctx->tracker_server) != 0) {
        fprintf(stderr, "Thread %d: Failed to initialize client\n", ctx->thread_id);
        return NULL;
    }
    
    // Allocate latency arrays
    ctx->query_latencies = (double*)malloc(ctx->operation_count * sizeof(double));
    ctx->update_latencies = (double*)malloc(ctx->operation_count * sizeof(double));
    ctx->delete_latencies = (double*)malloc(ctx->operation_count * sizeof(double));
    
    ctx->query_count = 0;
    ctx->update_count = 0;
    ctx->delete_count = 0;
    ctx->query_success = 0;
    ctx->update_success = 0;
    ctx->delete_success = 0;
    ctx->query_latency_count = 0;
    ctx->update_latency_count = 0;
    ctx->delete_latency_count = 0;
    
    int total_ratio = ctx->query_ratio + ctx->update_ratio + ctx->delete_ratio;
    
    for (int i = 0; i < ctx->operation_count; i++) {
        int file_idx = rand() % ctx->file_count;
        file_info_t *file = &ctx->files[file_idx];
        
        int op = rand() % total_ratio;
        double latency = 0;
        
        if (op < ctx->query_ratio) {
            // Query operation
            ctx->query_count++;
            if (perform_query(file, &latency) == 0) {
                ctx->query_success++;
                ctx->query_latencies[ctx->query_latency_count++] = latency;
            }
        } else if (op < ctx->query_ratio + ctx->update_ratio) {
            // Update operation
            ctx->update_count++;
            if (perform_update(file, &latency) == 0) {
                ctx->update_success++;
                ctx->update_latencies[ctx->update_latency_count++] = latency;
            }
        } else {
            // Delete operation
            ctx->delete_count++;
            if (perform_delete(file, &latency) == 0) {
                ctx->delete_success++;
                ctx->delete_latencies[ctx->delete_latency_count++] = latency;
            }
        }
        
        if (g_verbose && (i % 1000 == 0)) {
            printf("Thread %d: %d/%d operations\r", ctx->thread_id, i, ctx->operation_count);
            fflush(stdout);
        }
    }
    
    fdfs_client_destroy();
    
    // Update global statistics
    pthread_mutex_lock(&g_stats_mutex);
    g_total_queries += ctx->query_count;
    g_total_updates += ctx->update_count;
    g_total_deletes += ctx->delete_count;
    g_successful_queries += ctx->query_success;
    g_successful_updates += ctx->update_success;
    g_successful_deletes += ctx->delete_success;
    pthread_mutex_unlock(&g_stats_mutex);
    
    return NULL;
}

/**
 * Calculate and print statistics
 */
static void print_latency_stats(const char *op_name, double *latencies, int count) {
    if (count == 0) {
        printf("    \"%s\": { \"count\": 0 }", op_name);
        return;
    }
    
    qsort(latencies, count, sizeof(double), compare_double);
    
    double mean = 0;
    for (int i = 0; i < count; i++) {
        mean += latencies[i];
    }
    mean /= count;
    
    printf("    \"%s\": {\n", op_name);
    printf("      \"count\": %d,\n", count);
    printf("      \"mean_ms\": %.2f,\n", mean);
    printf("      \"p50_ms\": %.2f,\n", latencies[(int)(count * 0.50)]);
    printf("      \"p95_ms\": %.2f,\n", latencies[(int)(count * 0.95)]);
    printf("      \"p99_ms\": %.2f,\n", latencies[(int)(count * 0.99)]);
    printf("      \"min_ms\": %.2f,\n", latencies[0]);
    printf("      \"max_ms\": %.2f\n", latencies[count - 1]);
    printf("    }");
}

/**
 * Print results
 */
static void print_results(double total_time, thread_context_t *contexts) {
    // Collect all latencies
    int max_ops = g_operation_count;
    double *all_query_latencies = (double*)malloc(max_ops * sizeof(double));
    double *all_update_latencies = (double*)malloc(max_ops * sizeof(double));
    double *all_delete_latencies = (double*)malloc(max_ops * sizeof(double));
    
    int query_count = 0, update_count = 0, delete_count = 0;
    
    for (int i = 0; i < g_thread_count; i++) {
        for (int j = 0; j < contexts[i].query_latency_count; j++) {
            all_query_latencies[query_count++] = contexts[i].query_latencies[j];
        }
        for (int j = 0; j < contexts[i].update_latency_count; j++) {
            all_update_latencies[update_count++] = contexts[i].update_latencies[j];
        }
        for (int j = 0; j < contexts[i].delete_latency_count; j++) {
            all_delete_latencies[delete_count++] = contexts[i].delete_latencies[j];
        }
    }
    
    printf("{\n");
    printf("  \"benchmark\": \"metadata\",\n");
    printf("  \"timestamp\": \"%ld\",\n", time(NULL));
    printf("  \"configuration\": {\n");
    printf("    \"threads\": %d,\n", g_thread_count);
    printf("    \"operation_count\": %d,\n", g_operation_count);
    printf("    \"operation_mix\": \"%d:%d:%d\"\n", 
           g_query_ratio, g_update_ratio, g_delete_ratio);
    printf("  },\n");
    printf("  \"metrics\": {\n");
    printf("    \"total_operations\": %d,\n", 
           g_total_queries + g_total_updates + g_total_deletes);
    printf("    \"ops_per_second\": %.2f,\n", 
           (g_total_queries + g_total_updates + g_total_deletes) / total_time);
    printf("    \"duration_seconds\": %.2f,\n", total_time);
    printf("    \"operations\": {\n");
    printf("      \"query\": {\n");
    printf("        \"total\": %d,\n", g_total_queries);
    printf("        \"successful\": %d,\n", g_successful_queries);
    printf("        \"success_rate\": %.2f\n", 
           (g_total_queries > 0) ? (double)g_successful_queries / g_total_queries * 100 : 0);
    printf("      },\n");
    printf("      \"update\": {\n");
    printf("        \"total\": %d,\n", g_total_updates);
    printf("        \"successful\": %d,\n", g_successful_updates);
    printf("        \"success_rate\": %.2f\n", 
           (g_total_updates > 0) ? (double)g_successful_updates / g_total_updates * 100 : 0);
    printf("      },\n");
    printf("      \"delete\": {\n");
    printf("        \"total\": %d,\n", g_total_deletes);
    printf("        \"successful\": %d,\n", g_successful_deletes);
    printf("        \"success_rate\": %.2f\n", 
           (g_total_deletes > 0) ? (double)g_successful_deletes / g_total_deletes * 100 : 0);
    printf("      }\n");
    printf("    },\n");
    printf("    \"latency\": {\n");
    print_latency_stats("query", all_query_latencies, query_count);
    printf(",\n");
    print_latency_stats("update", all_update_latencies, update_count);
    printf(",\n");
    print_latency_stats("delete", all_delete_latencies, delete_count);
    printf("\n    }\n");
    printf("  }\n");
    printf("}\n");
    
    free(all_query_latencies);
    free(all_update_latencies);
    free(all_delete_latencies);
}

/**
 * Print usage
 */
static void print_usage(const char *program) {
    printf("Usage: %s [OPTIONS]\n", program);
    printf("\nOptions:\n");
    printf("  -t, --threads NUM      Number of threads (default: 10)\n");
    printf("  -o, --operations NUM   Number of operations (default: 10000)\n");
    printf("  -m, --mix RATIO        Operation mix query:update:delete (default: 70:20:10)\n");
    printf("  -p, --prepare NUM      Number of files to prepare (default: 100)\n");
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
        {"operations", required_argument, 0, 'o'},
        {"mix", required_argument, 0, 'm'},
        {"prepare", required_argument, 0, 'p'},
        {"tracker", required_argument, 0, 'T'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "t:o:m:p:T:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 't': g_thread_count = atoi(optarg); break;
            case 'o': g_operation_count = atoi(optarg); break;
            case 'm': sscanf(optarg, "%d:%d:%d", &g_query_ratio, &g_update_ratio, &g_delete_ratio); break;
            case 'p': g_prepare_files = atoi(optarg); break;
            case 'T': strncpy(g_tracker_server, optarg, sizeof(g_tracker_server) - 1); break;
            case 'v': g_verbose = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }
    
    printf("FastDFS Metadata Operations Benchmark\n");
    printf("======================================\n");
    printf("Threads: %d\n", g_thread_count);
    printf("Operations: %d\n", g_operation_count);
    printf("Operation mix: %d:%d:%d (query:update:delete)\n", 
           g_query_ratio, g_update_ratio, g_delete_ratio);
    printf("Tracker: %s\n\n", g_tracker_server);
    
    srand(time(NULL));
    
    // Prepare test files
    if (prepare_test_files() != 0) {
        return 1;
    }
    
    int ops_per_thread = g_operation_count / g_thread_count;
    int remaining = g_operation_count % g_thread_count;
    
    for (int i = 0; i < g_thread_count; i++) {
        contexts[i].thread_id = i;
        contexts[i].operation_count = ops_per_thread + (i < remaining ? 1 : 0);
        contexts[i].tracker_server = g_tracker_server;
        contexts[i].files = g_files;
        contexts[i].file_count = g_file_count;
        contexts[i].query_ratio = g_query_ratio;
        contexts[i].update_ratio = g_update_ratio;
        contexts[i].delete_ratio = g_delete_ratio;
    }
    
    printf("Starting benchmark...\n");
    double start_time = get_time_us();
    
    for (int i = 0; i < g_thread_count; i++) {
        pthread_create(&threads[i], NULL, metadata_thread, &contexts[i]);
    }
    
    for (int i = 0; i < g_thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double end_time = get_time_us();
    double total_time = (end_time - start_time) / 1000000.0;
    
    printf("\nBenchmark complete!\n\n");
    print_results(total_time, contexts);
    
    // Cleanup
    for (int i = 0; i < g_thread_count; i++) {
        free(contexts[i].query_latencies);
        free(contexts[i].update_latencies);
        free(contexts[i].delete_latencies);
    }
    free(g_files);
    
    return 0;
}
