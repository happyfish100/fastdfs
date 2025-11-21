/**
 * FastDFS Performance Benchmark Tool
 * 
 * Comprehensive performance testing for FastDFS operations
 * Measures throughput, latency, and concurrency performance
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"

#define MAX_FILE_SIZE (100 * 1024 * 1024)
#define MAX_THREADS 100
#define MAX_FILE_IDS 10000

typedef enum {
    BENCH_UPLOAD = 1,
    BENCH_DOWNLOAD = 2,
    BENCH_DELETE = 3,
    BENCH_METADATA = 4,
    BENCH_MIXED = 5
} BenchmarkType;

typedef struct {
    long long total_ops;
    long long successful_ops;
    long long failed_ops;
    long long total_bytes;
    long long min_latency_us;
    long long max_latency_us;
    long long total_latency_us;
    pthread_mutex_t mutex;
} BenchmarkStats;

typedef struct {
    int thread_id;
    ConnectionInfo *pTrackerServer;
    BenchmarkType bench_type;
    int file_size;
    int operations_per_thread;
    BenchmarkStats *stats;
    char **file_ids;
    int *file_id_count;
    pthread_mutex_t *file_id_mutex;
    int running;
} ThreadContext;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("FastDFS performance benchmark tool\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -t, --type TYPE        Benchmark type:\n");
    printf("                         upload, download, delete, metadata, mixed\n");
    printf("  -s, --size SIZE        File size in bytes (default: 10240)\n");
    printf("  -n, --operations NUM   Total operations (default: 1000)\n");
    printf("  -j, --threads NUM      Number of threads (default: 10, max: 100)\n");
    printf("  -d, --duration SEC     Run for specified duration (overrides -n)\n");
    printf("  -w, --warmup SEC       Warmup duration in seconds (default: 5)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -t upload -s 10240 -n 10000 -j 20\n", program_name);
    printf("  %s -t download -d 60 -j 50\n", program_name);
    printf("  %s -t mixed -n 5000 -j 10\n", program_name);
}

static long long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

static void update_stats(BenchmarkStats *stats, int success, long long latency_us, long long bytes) {
    pthread_mutex_lock(&stats->mutex);
    
    stats->total_ops++;
    if (success) {
        stats->successful_ops++;
        stats->total_bytes += bytes;
    } else {
        stats->failed_ops++;
    }
    
    stats->total_latency_us += latency_us;
    
    if (stats->total_ops == 1 || latency_us < stats->min_latency_us) {
        stats->min_latency_us = latency_us;
    }
    
    if (stats->total_ops == 1 || latency_us > stats->max_latency_us) {
        stats->max_latency_us = latency_us;
    }
    
    pthread_mutex_unlock(&stats->mutex);
}

static int benchmark_upload(ThreadContext *ctx) {
    char *file_buffer;
    char file_id[128];
    long long start_time, end_time;
    int result;
    ConnectionInfo *pStorageServer;
    
    file_buffer = (char *)malloc(ctx->file_size);
    if (file_buffer == NULL) {
        return -1;
    }
    
    for (int i = 0; i < ctx->file_size; i++) {
        file_buffer[i] = 'A' + (i % 26);
    }
    
    pStorageServer = get_storage_connection(ctx->pTrackerServer);
    if (pStorageServer == NULL) {
        free(file_buffer);
        return -1;
    }
    
    start_time = get_time_us();
    
    result = storage_upload_by_filebuff1(ctx->pTrackerServer, pStorageServer,
                                        file_buffer, ctx->file_size,
                                        NULL, NULL, 0, NULL,
                                        file_id, sizeof(file_id));
    
    end_time = get_time_us();
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    update_stats(ctx->stats, result == 0, end_time - start_time, ctx->file_size);
    
    if (result == 0 && ctx->file_ids != NULL) {
        pthread_mutex_lock(ctx->file_id_mutex);
        if (*ctx->file_id_count < MAX_FILE_IDS) {
            ctx->file_ids[*ctx->file_id_count] = strdup(file_id);
            (*ctx->file_id_count)++;
        }
        pthread_mutex_unlock(ctx->file_id_mutex);
    }
    
    free(file_buffer);
    return result;
}

static int benchmark_download(ThreadContext *ctx) {
    char *file_buffer = NULL;
    int64_t file_size;
    long long start_time, end_time;
    int result;
    ConnectionInfo *pStorageServer;
    char *file_id;
    
    pthread_mutex_lock(ctx->file_id_mutex);
    if (*ctx->file_id_count == 0) {
        pthread_mutex_unlock(ctx->file_id_mutex);
        return -1;
    }
    file_id = ctx->file_ids[rand() % *ctx->file_id_count];
    pthread_mutex_unlock(ctx->file_id_mutex);
    
    pStorageServer = get_storage_connection(ctx->pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    start_time = get_time_us();
    
    result = storage_download_file_to_buff1(ctx->pTrackerServer, pStorageServer,
                                           file_id, &file_buffer, &file_size);
    
    end_time = get_time_us();
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    update_stats(ctx->stats, result == 0, end_time - start_time, file_size);
    
    if (file_buffer != NULL) {
        free(file_buffer);
    }
    
    return result;
}

static int benchmark_delete(ThreadContext *ctx) {
    long long start_time, end_time;
    int result;
    ConnectionInfo *pStorageServer;
    char *file_id;
    
    pthread_mutex_lock(ctx->file_id_mutex);
    if (*ctx->file_id_count == 0) {
        pthread_mutex_unlock(ctx->file_id_mutex);
        return -1;
    }
    int index = rand() % *ctx->file_id_count;
    file_id = ctx->file_ids[index];
    
    ctx->file_ids[index] = ctx->file_ids[*ctx->file_id_count - 1];
    (*ctx->file_id_count)--;
    pthread_mutex_unlock(ctx->file_id_mutex);
    
    pStorageServer = get_storage_connection(ctx->pTrackerServer);
    if (pStorageServer == NULL) {
        free(file_id);
        return -1;
    }
    
    start_time = get_time_us();
    
    result = storage_delete_file1(ctx->pTrackerServer, pStorageServer, file_id);
    
    end_time = get_time_us();
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    update_stats(ctx->stats, result == 0, end_time - start_time, 0);
    
    free(file_id);
    return result;
}

static int benchmark_metadata(ThreadContext *ctx) {
    FDFSMetaData meta_list[3];
    long long start_time, end_time;
    int result;
    ConnectionInfo *pStorageServer;
    char *file_id;
    
    pthread_mutex_lock(ctx->file_id_mutex);
    if (*ctx->file_id_count == 0) {
        pthread_mutex_unlock(ctx->file_id_mutex);
        return -1;
    }
    file_id = ctx->file_ids[rand() % *ctx->file_id_count];
    pthread_mutex_unlock(ctx->file_id_mutex);
    
    strcpy(meta_list[0].name, "benchmark");
    strcpy(meta_list[0].value, "test");
    strcpy(meta_list[1].name, "thread");
    snprintf(meta_list[1].value, sizeof(meta_list[1].value), "%d", ctx->thread_id);
    strcpy(meta_list[2].name, "timestamp");
    snprintf(meta_list[2].value, sizeof(meta_list[2].value), "%lld", get_time_us());
    
    pStorageServer = get_storage_connection(ctx->pTrackerServer);
    if (pStorageServer == NULL) {
        return -1;
    }
    
    start_time = get_time_us();
    
    result = storage_set_metadata1(ctx->pTrackerServer, pStorageServer, file_id,
                                  meta_list, 3, STORAGE_SET_METADATA_FLAG_OVERWRITE);
    
    end_time = get_time_us();
    
    tracker_disconnect_server_ex(pStorageServer, true);
    
    update_stats(ctx->stats, result == 0, end_time - start_time, 0);
    
    return result;
}

static void *benchmark_worker(void *arg) {
    ThreadContext *ctx = (ThreadContext *)arg;
    int ops_done = 0;
    
    while (ctx->running && (ctx->operations_per_thread == 0 || ops_done < ctx->operations_per_thread)) {
        int result = -1;
        
        switch (ctx->bench_type) {
            case BENCH_UPLOAD:
                result = benchmark_upload(ctx);
                break;
            case BENCH_DOWNLOAD:
                result = benchmark_download(ctx);
                break;
            case BENCH_DELETE:
                result = benchmark_delete(ctx);
                break;
            case BENCH_METADATA:
                result = benchmark_metadata(ctx);
                break;
            case BENCH_MIXED:
                switch (rand() % 4) {
                    case 0: result = benchmark_upload(ctx); break;
                    case 1: result = benchmark_download(ctx); break;
                    case 2: result = benchmark_metadata(ctx); break;
                    case 3: result = benchmark_delete(ctx); break;
                }
                break;
        }
        
        ops_done++;
        
        if (ops_done % 100 == 0) {
            usleep(1000);
        }
    }
    
    return NULL;
}

static void print_results(BenchmarkStats *stats, const char *bench_name,
                         long long duration_ms, int num_threads) {
    double duration_sec = duration_ms / 1000.0;
    double ops_per_sec = stats->successful_ops / duration_sec;
    double avg_latency_ms = stats->total_latency_us / (stats->total_ops * 1000.0);
    double throughput_mbps = (stats->total_bytes / (1024.0 * 1024.0)) / duration_sec;
    
    printf("\n");
    printf("=== %s Benchmark Results ===\n", bench_name);
    printf("\n");
    printf("Configuration:\n");
    printf("  Threads: %d\n", num_threads);
    printf("  Duration: %.2f seconds\n", duration_sec);
    printf("\n");
    printf("Operations:\n");
    printf("  Total: %lld\n", stats->total_ops);
    printf("  Successful: %lld\n", stats->successful_ops);
    printf("  Failed: %lld\n", stats->failed_ops);
    printf("  Success rate: %.2f%%\n",
           (stats->successful_ops * 100.0) / stats->total_ops);
    printf("\n");
    printf("Performance:\n");
    printf("  Operations/sec: %.2f\n", ops_per_sec);
    printf("  Avg latency: %.2f ms\n", avg_latency_ms);
    printf("  Min latency: %.2f ms\n", stats->min_latency_us / 1000.0);
    printf("  Max latency: %.2f ms\n", stats->max_latency_us / 1000.0);
    
    if (stats->total_bytes > 0) {
        printf("  Total data: %.2f MB\n", stats->total_bytes / (1024.0 * 1024.0));
        printf("  Throughput: %.2f MB/s\n", throughput_mbps);
    }
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *bench_type_str = "upload";
    BenchmarkType bench_type = BENCH_UPLOAD;
    int file_size = 10240;
    int total_operations = 1000;
    int num_threads = 10;
    int duration_sec = 0;
    int warmup_sec = 5;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    BenchmarkStats stats;
    ThreadContext *contexts;
    pthread_t *threads;
    char **file_ids;
    int file_id_count = 0;
    pthread_mutex_t file_id_mutex = PTHREAD_MUTEX_INITIALIZER;
    struct timespec start_time, end_time;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"type", required_argument, 0, 't'},
        {"size", required_argument, 0, 's'},
        {"operations", required_argument, 0, 'n'},
        {"threads", required_argument, 0, 'j'},
        {"duration", required_argument, 0, 'd'},
        {"warmup", required_argument, 0, 'w'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:t:s:n:j:d:w:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 't':
                bench_type_str = optarg;
                if (strcmp(optarg, "upload") == 0) {
                    bench_type = BENCH_UPLOAD;
                } else if (strcmp(optarg, "download") == 0) {
                    bench_type = BENCH_DOWNLOAD;
                } else if (strcmp(optarg, "delete") == 0) {
                    bench_type = BENCH_DELETE;
                } else if (strcmp(optarg, "metadata") == 0) {
                    bench_type = BENCH_METADATA;
                } else if (strcmp(optarg, "mixed") == 0) {
                    bench_type = BENCH_MIXED;
                } else {
                    fprintf(stderr, "ERROR: Invalid benchmark type: %s\n", optarg);
                    return 1;
                }
                break;
            case 's':
                file_size = atoi(optarg);
                if (file_size < 1 || file_size > MAX_FILE_SIZE) {
                    fprintf(stderr, "ERROR: Invalid file size\n");
                    return 1;
                }
                break;
            case 'n':
                total_operations = atoi(optarg);
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                break;
            case 'd':
                duration_sec = atoi(optarg);
                break;
            case 'w':
                warmup_sec = atoi(optarg);
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
    
    file_ids = (char **)malloc(MAX_FILE_IDS * sizeof(char *));
    if (file_ids == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return ENOMEM;
    }
    
    memset(&stats, 0, sizeof(stats));
    pthread_mutex_init(&stats.mutex, NULL);
    
    printf("FastDFS Performance Benchmark\n");
    printf("=============================\n");
    printf("Benchmark type: %s\n", bench_type_str);
    printf("File size: %d bytes\n", file_size);
    printf("Threads: %d\n", num_threads);
    if (duration_sec > 0) {
        printf("Duration: %d seconds\n", duration_sec);
    } else {
        printf("Total operations: %d\n", total_operations);
    }
    printf("Warmup: %d seconds\n", warmup_sec);
    printf("\n");
    
    if (bench_type == BENCH_DOWNLOAD || bench_type == BENCH_DELETE ||
        bench_type == BENCH_METADATA || bench_type == BENCH_MIXED) {
        printf("Preparing test files...\n");
        int prep_files = num_threads * 10;
        for (int i = 0; i < prep_files && file_id_count < MAX_FILE_IDS; i++) {
            ThreadContext prep_ctx;
            prep_ctx.pTrackerServer = pTrackerServer;
            prep_ctx.file_size = file_size;
            prep_ctx.stats = &stats;
            prep_ctx.file_ids = file_ids;
            prep_ctx.file_id_count = &file_id_count;
            prep_ctx.file_id_mutex = &file_id_mutex;
            
            benchmark_upload(&prep_ctx);
            
            if ((i + 1) % 10 == 0) {
                printf("\rPrepared %d files...", i + 1);
                fflush(stdout);
            }
        }
        printf("\rPrepared %d files\n", file_id_count);
        
        memset(&stats, 0, sizeof(stats));
    }
    
    if (warmup_sec > 0) {
        printf("\nWarming up for %d seconds...\n", warmup_sec);
        sleep(warmup_sec);
    }
    
    printf("\nStarting benchmark...\n\n");
    
    contexts = (ThreadContext *)malloc(num_threads * sizeof(ThreadContext));
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    int ops_per_thread = duration_sec > 0 ? 0 : (total_operations / num_threads);
    
    for (int i = 0; i < num_threads; i++) {
        contexts[i].thread_id = i;
        contexts[i].pTrackerServer = pTrackerServer;
        contexts[i].bench_type = bench_type;
        contexts[i].file_size = file_size;
        contexts[i].operations_per_thread = ops_per_thread;
        contexts[i].stats = &stats;
        contexts[i].file_ids = file_ids;
        contexts[i].file_id_count = &file_id_count;
        contexts[i].file_id_mutex = &file_id_mutex;
        contexts[i].running = 1;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, benchmark_worker, &contexts[i]);
    }
    
    if (duration_sec > 0) {
        sleep(duration_sec);
        for (int i = 0; i < num_threads; i++) {
            contexts[i].running = 0;
        }
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000LL +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000LL;
    
    print_results(&stats, bench_type_str, elapsed_ms, num_threads);
    
    for (int i = 0; i < file_id_count; i++) {
        free(file_ids[i]);
    }
    free(file_ids);
    free(contexts);
    free(threads);
    pthread_mutex_destroy(&stats.mutex);
    pthread_mutex_destroy(&file_id_mutex);
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return 0;
}
