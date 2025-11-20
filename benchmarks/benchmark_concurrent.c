/**
 * FastDFS Concurrent Operations Benchmark
 * 
 * Simulates multiple concurrent users performing mixed operations
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
#include <signal.h>
#include "fdfs_client.h"

#define MAX_USERS 1024
#define MAX_FILENAME_LEN 256
#define MAX_UPLOADED_FILES 10000

typedef enum {
    OP_UPLOAD,
    OP_DOWNLOAD,
    OP_DELETE
} operation_type_t;

typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[MAX_FILENAME_LEN];
} uploaded_file_t;

typedef struct {
    int user_id;
    int duration_seconds;
    char *tracker_server;
    int upload_ratio;
    int download_ratio;
    int delete_ratio;
    int think_time_ms;
    int file_size;
    
    // Results
    int upload_count;
    int download_count;
    int delete_count;
    int upload_success;
    int download_success;
    int delete_success;
    double total_bytes_uploaded;
    double total_bytes_downloaded;
} user_context_t;

// Global configuration
static int g_user_count = 10;
static int g_duration = 60;
static char g_tracker_server[256] = "127.0.0.1:22122";
static int g_upload_ratio = 50;
static int g_download_ratio = 45;
static int g_delete_ratio = 5;
static int g_think_time = 100;
static int g_file_size = 1048576;
static int g_verbose = 0;
static volatile int g_running = 1;

// Global file pool
static uploaded_file_t g_uploaded_files[MAX_UPLOADED_FILES];
static int g_uploaded_count = 0;
static pthread_mutex_t g_file_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global statistics
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_total_uploads = 0;
static int g_total_downloads = 0;
static int g_total_deletes = 0;
static int g_successful_uploads = 0;
static int g_successful_downloads = 0;
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
    if (data == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < size; i++) {
        data[i] = (char)(rand() % 256);
    }
    
    return data;
}

/**
 * Add file to global pool
 */
static void add_to_file_pool(const char *group_name, const char *remote_filename) {
    pthread_mutex_lock(&g_file_pool_mutex);
    
    if (g_uploaded_count < MAX_UPLOADED_FILES) {
        strncpy(g_uploaded_files[g_uploaded_count].group_name, group_name, 
                FDFS_GROUP_NAME_MAX_LEN);
        strncpy(g_uploaded_files[g_uploaded_count].remote_filename, remote_filename, 
                MAX_FILENAME_LEN - 1);
        g_uploaded_count++;
    }
    
    pthread_mutex_unlock(&g_file_pool_mutex);
}

/**
 * Get random file from pool
 */
static int get_random_file(char *group_name, char *remote_filename) {
    pthread_mutex_lock(&g_file_pool_mutex);
    
    if (g_uploaded_count == 0) {
        pthread_mutex_unlock(&g_file_pool_mutex);
        return -1;
    }
    
    int idx = rand() % g_uploaded_count;
    strncpy(group_name, g_uploaded_files[idx].group_name, FDFS_GROUP_NAME_MAX_LEN);
    strncpy(remote_filename, g_uploaded_files[idx].remote_filename, MAX_FILENAME_LEN - 1);
    
    pthread_mutex_unlock(&g_file_pool_mutex);
    return 0;
}

/**
 * Remove file from pool
 */
static void remove_from_file_pool(const char *group_name, const char *remote_filename) {
    pthread_mutex_lock(&g_file_pool_mutex);
    
    for (int i = 0; i < g_uploaded_count; i++) {
        if (strcmp(g_uploaded_files[i].group_name, group_name) == 0 &&
            strcmp(g_uploaded_files[i].remote_filename, remote_filename) == 0) {
            // Move last element to this position
            if (i < g_uploaded_count - 1) {
                g_uploaded_files[i] = g_uploaded_files[g_uploaded_count - 1];
            }
            g_uploaded_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_file_pool_mutex);
}

/**
 * Select operation based on ratios
 */
static operation_type_t select_operation(int upload_ratio, int download_ratio, int delete_ratio) {
    int total = upload_ratio + download_ratio + delete_ratio;
    int r = rand() % total;
    
    if (r < upload_ratio) {
        return OP_UPLOAD;
    } else if (r < upload_ratio + download_ratio) {
        return OP_DOWNLOAD;
    } else {
        return OP_DELETE;
    }
}

/**
 * Perform upload operation
 */
static int perform_upload(user_context_t *ctx, char *file_data) {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[MAX_FILENAME_LEN];
    
    int result = fdfs_upload_by_buffer(file_data, ctx->file_size, NULL,
                                      group_name, remote_filename);
    
    if (result == 0) {
        ctx->upload_success++;
        ctx->total_bytes_uploaded += ctx->file_size;
        add_to_file_pool(group_name, remote_filename);
        return 0;
    }
    
    return -1;
}

/**
 * Perform download operation
 */
static int perform_download(user_context_t *ctx) {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[MAX_FILENAME_LEN];
    char *buffer = NULL;
    int64_t file_size = 0;
    
    if (get_random_file(group_name, remote_filename) != 0) {
        return -1;
    }
    
    int result = fdfs_download_file_to_buffer(group_name, remote_filename,
                                             &buffer, &file_size);
    
    if (result == 0 && buffer != NULL) {
        ctx->download_success++;
        ctx->total_bytes_downloaded += file_size;
        free(buffer);
        return 0;
    }
    
    return -1;
}

/**
 * Perform delete operation
 */
static int perform_delete(user_context_t *ctx) {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char remote_filename[MAX_FILENAME_LEN];
    
    if (get_random_file(group_name, remote_filename) != 0) {
        return -1;
    }
    
    int result = fdfs_delete_file(group_name, remote_filename);
    
    if (result == 0) {
        ctx->delete_success++;
        remove_from_file_pool(group_name, remote_filename);
        return 0;
    }
    
    return -1;
}

/**
 * User simulation thread
 */
static void* user_thread(void *arg) {
    user_context_t *ctx = (user_context_t*)arg;
    char *file_data = NULL;
    
    // Initialize client
    if (fdfs_client_init(ctx->tracker_server) != 0) {
        fprintf(stderr, "User %d: Failed to initialize FDFS client\n", ctx->user_id);
        return NULL;
    }
    
    // Generate file data once
    file_data = generate_random_data(ctx->file_size);
    if (file_data == NULL) {
        fprintf(stderr, "User %d: Failed to generate file data\n", ctx->user_id);
        fdfs_client_destroy();
        return NULL;
    }
    
    // Initialize counters
    ctx->upload_count = 0;
    ctx->download_count = 0;
    ctx->delete_count = 0;
    ctx->upload_success = 0;
    ctx->download_success = 0;
    ctx->delete_success = 0;
    ctx->total_bytes_uploaded = 0;
    ctx->total_bytes_downloaded = 0;
    
    time_t start_time = time(NULL);
    
    // Run until duration expires
    while (g_running && (time(NULL) - start_time) < ctx->duration_seconds) {
        operation_type_t op = select_operation(ctx->upload_ratio, 
                                              ctx->download_ratio, 
                                              ctx->delete_ratio);
        
        switch (op) {
            case OP_UPLOAD:
                ctx->upload_count++;
                perform_upload(ctx, file_data);
                break;
                
            case OP_DOWNLOAD:
                ctx->download_count++;
                perform_download(ctx);
                break;
                
            case OP_DELETE:
                ctx->delete_count++;
                perform_delete(ctx);
                break;
        }
        
        // Think time
        if (ctx->think_time_ms > 0) {
            usleep(ctx->think_time_ms * 1000);
        }
    }
    
    // Cleanup
    free(file_data);
    fdfs_client_destroy();
    
    // Update global statistics
    pthread_mutex_lock(&g_stats_mutex);
    g_total_uploads += ctx->upload_count;
    g_total_downloads += ctx->download_count;
    g_total_deletes += ctx->delete_count;
    g_successful_uploads += ctx->upload_success;
    g_successful_downloads += ctx->download_success;
    g_successful_deletes += ctx->delete_success;
    pthread_mutex_unlock(&g_stats_mutex);
    
    return NULL;
}

/**
 * Signal handler
 */
static void signal_handler(int signum) {
    g_running = 0;
}

/**
 * Print results
 */
static void print_results(double duration) {
    double upload_throughput = (g_successful_uploads > 0) ? 
        g_successful_uploads / duration : 0;
    double download_throughput = (g_successful_downloads > 0) ? 
        g_successful_downloads / duration : 0;
    double total_ops = g_total_uploads + g_total_downloads + g_total_deletes;
    double ops_per_sec = total_ops / duration;
    
    printf("{\n");
    printf("  \"benchmark\": \"concurrent\",\n");
    printf("  \"timestamp\": \"%ld\",\n", time(NULL));
    printf("  \"configuration\": {\n");
    printf("    \"users\": %d,\n", g_user_count);
    printf("    \"duration\": %d,\n", g_duration);
    printf("    \"operation_mix\": \"%d:%d:%d\",\n", 
           g_upload_ratio, g_download_ratio, g_delete_ratio);
    printf("    \"think_time_ms\": %d,\n", g_think_time);
    printf("    \"file_size\": %d\n", g_file_size);
    printf("  },\n");
    printf("  \"metrics\": {\n");
    printf("    \"operations\": {\n");
    printf("      \"total\": %.0f,\n", total_ops);
    printf("      \"per_second\": %.2f,\n", ops_per_sec);
    printf("      \"uploads\": {\n");
    printf("        \"total\": %d,\n", g_total_uploads);
    printf("        \"successful\": %d,\n", g_successful_uploads);
    printf("        \"success_rate\": %.2f,\n", 
               (g_total_uploads > 0) ? (double)g_successful_uploads / g_total_uploads * 100 : 0);
    printf("        \"per_second\": %.2f\n", upload_throughput);
    printf("      },\n");
    printf("      \"downloads\": {\n");
    printf("        \"total\": %d,\n", g_total_downloads);
    printf("        \"successful\": %d,\n", g_successful_downloads);
    printf("        \"success_rate\": %.2f,\n", 
               (g_total_downloads > 0) ? (double)g_successful_downloads / g_total_downloads * 100 : 0);
    printf("        \"per_second\": %.2f\n", download_throughput);
    printf("      },\n");
    printf("      \"deletes\": {\n");
    printf("        \"total\": %d,\n", g_total_deletes);
    printf("        \"successful\": %d,\n", g_successful_deletes);
    printf("        \"success_rate\": %.2f\n", 
               (g_total_deletes > 0) ? (double)g_successful_deletes / g_total_deletes * 100 : 0);
    printf("      }\n");
    printf("    },\n");
    printf("    \"duration_seconds\": %.2f,\n", duration);
    printf("    \"files_in_pool\": %d\n", g_uploaded_count);
    printf("  }\n");
    printf("}\n");
}

/**
 * Print usage
 */
static void print_usage(const char *program) {
    printf("Usage: %s [OPTIONS]\n", program);
    printf("\nOptions:\n");
    printf("  -u, --users NUM        Number of concurrent users (default: 10)\n");
    printf("  -d, --duration SEC     Test duration in seconds (default: 60)\n");
    printf("  -m, --mix RATIO        Operation mix upload:download:delete (default: 50:45:5)\n");
    printf("  -t, --think-time MS    Think time between operations (default: 100)\n");
    printf("  -s, --size BYTES       File size in bytes (default: 1048576)\n");
    printf("  -T, --tracker SERVER   Tracker server (default: 127.0.0.1:22122)\n");
    printf("  -v, --verbose          Enable verbose output\n");
    printf("  -h, --help             Show this help message\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    pthread_t threads[MAX_USERS];
    user_context_t contexts[MAX_USERS];
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"users", required_argument, 0, 'u'},
        {"duration", required_argument, 0, 'd'},
        {"mix", required_argument, 0, 'm'},
        {"think-time", required_argument, 0, 't'},
        {"size", required_argument, 0, 's'},
        {"tracker", required_argument, 0, 'T'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "u:d:m:t:s:T:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'u':
                g_user_count = atoi(optarg);
                break;
            case 'd':
                g_duration = atoi(optarg);
                break;
            case 'm':
                sscanf(optarg, "%d:%d:%d", &g_upload_ratio, &g_download_ratio, &g_delete_ratio);
                break;
            case 't':
                g_think_time = atoi(optarg);
                break;
            case 's':
                g_file_size = atoi(optarg);
                break;
            case 'T':
                strncpy(g_tracker_server, optarg, sizeof(g_tracker_server) - 1);
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
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("FastDFS Concurrent Operations Benchmark\n");
    printf("========================================\n");
    printf("Concurrent users: %d\n", g_user_count);
    printf("Duration: %d seconds\n", g_duration);
    printf("Operation mix: %d:%d:%d (upload:download:delete)\n", 
           g_upload_ratio, g_download_ratio, g_delete_ratio);
    printf("Think time: %d ms\n", g_think_time);
    printf("File size: %d bytes\n", g_file_size);
    printf("Tracker: %s\n\n", g_tracker_server);
    
    srand(time(NULL));
    
    // Initialize contexts
    for (int i = 0; i < g_user_count; i++) {
        contexts[i].user_id = i;
        contexts[i].duration_seconds = g_duration;
        contexts[i].tracker_server = g_tracker_server;
        contexts[i].upload_ratio = g_upload_ratio;
        contexts[i].download_ratio = g_download_ratio;
        contexts[i].delete_ratio = g_delete_ratio;
        contexts[i].think_time_ms = g_think_time;
        contexts[i].file_size = g_file_size;
    }
    
    // Start benchmark
    printf("Starting benchmark...\n");
    double start_time = get_time_us();
    
    // Create threads
    for (int i = 0; i < g_user_count; i++) {
        if (pthread_create(&threads[i], NULL, user_thread, &contexts[i]) != 0) {
            fprintf(stderr, "Error: Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    // Wait for threads
    for (int i = 0; i < g_user_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double end_time = get_time_us();
    double total_time = (end_time - start_time) / 1000000.0;
    
    // Print results
    printf("\nBenchmark complete!\n\n");
    print_results(total_time);
    
    return 0;
}
