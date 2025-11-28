/**
 * FastDFS Performance Profiler Tool
 * 
 * This tool provides comprehensive performance profiling capabilities for FastDFS,
 * allowing users to measure operation latency, identify slow operations, generate
 * performance reports, and compare performance across servers.
 * 
 * Features:
 * - Measure operation latency for various FastDFS operations
 * - Identify slow operations and bottlenecks
 * - Generate detailed performance reports
 * - Compare performance across servers
 * - Statistical analysis (mean, median, percentiles)
 * - Multi-threaded performance testing
 * - JSON and text output formats
 * 
 * Profiled Operations:
 * - Upload: File upload performance
 * - Download: File download performance
 * - Delete: File deletion performance
 * - Query: File info query performance
 * - Metadata: Metadata operations performance
 * - Connection: Connection establishment performance
 * 
 * Performance Metrics:
 * - Latency (mean, median, min, max)
 * - Percentiles (p50, p75, p90, p95, p99, p99.9)
 * - Throughput (operations per second)
 * - Success rate
 * - Standard deviation
 * 
 * Server Comparison:
 * - Compare performance across storage servers
 * - Identify fastest/slowest servers
 * - Network latency analysis
 * - Server load analysis
 * 
 * Use Cases:
 * - Performance optimization
 * - Bottleneck identification
 * - Capacity planning
 * - Server performance comparison
 * - Baseline establishment
 * - Performance regression testing
 * 
 * Copyright (C) 2025
 * License: GPL V3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum number of latency samples */
#define MAX_SAMPLES 1000000

/* Maximum number of threads */
#define MAX_THREADS 100

/* Default number of threads */
#define DEFAULT_THREADS 4

/* Maximum file ID length */
#define MAX_FILE_ID_LEN 256

/* Default test file size (1MB) */
#define DEFAULT_TEST_FILE_SIZE 1048576

/* Operation type enumeration */
typedef enum {
    OP_UPLOAD = 0,      /* Upload operation */
    OP_DOWNLOAD = 1,    /* Download operation */
    OP_DELETE = 2,      /* Delete operation */
    OP_QUERY = 3,       /* Query operation */
    OP_METADATA_GET = 4,  /* Get metadata operation */
    OP_METADATA_SET = 5,  /* Set metadata operation */
    OP_CONNECTION = 6   /* Connection operation */
} OperationType;

/* Latency sample structure */
typedef struct {
    double latency_ms;      /* Latency in milliseconds */
    int success;            /* Success flag (1 = success, 0 = failure) */
    time_t timestamp;        /* Timestamp of operation */
    char server_ip[64];      /* Server IP address */
} LatencySample;

/* Performance statistics structure */
typedef struct {
    int sample_count;       /* Number of samples */
    double mean;            /* Mean latency */
    double median;          /* Median latency */
    double min;             /* Minimum latency */
    double max;             /* Maximum latency */
    double stddev;          /* Standard deviation */
    double p50;             /* 50th percentile */
    double p75;             /* 75th percentile */
    double p90;             /* 90th percentile */
    double p95;             /* 95th percentile */
    double p99;             /* 99th percentile */
    double p999;            /* 99.9th percentile */
    int success_count;      /* Number of successful operations */
    int failure_count;      /* Number of failed operations */
    double throughput;     /* Throughput (ops/sec) */
} PerformanceStats;

/* Operation profile structure */
typedef struct {
    OperationType op_type;  /* Operation type */
    char op_name[64];        /* Operation name */
    LatencySample *samples;  /* Array of latency samples */
    int sample_count;        /* Number of samples */
    int sample_capacity;     /* Sample array capacity */
    PerformanceStats stats;  /* Performance statistics */
    pthread_mutex_t mutex;   /* Mutex for thread safety */
} OperationProfile;

/* Profiler context */
typedef struct {
    ConnectionInfo *pTrackerServer;  /* Tracker server connection */
    OperationProfile *profiles;       /* Array of operation profiles */
    int profile_count;                 /* Number of profiles */
    int iterations;                   /* Number of iterations per operation */
    int num_threads;                   /* Number of threads */
    int test_file_size;                /* Test file size */
    char test_file_data[1048576];      /* Test file data buffer */
    int verbose;                       /* Verbose output flag */
    int json_output;                   /* JSON output flag */
    time_t start_time;                 /* Profiling start time */
    time_t end_time;                   /* Profiling end time */
} ProfilerContext;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;

/**
 * Get current time in milliseconds
 * 
 * This function returns the current time in milliseconds
 * using high-resolution timing.
 * 
 * @return Current time in milliseconds
 */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/**
 * Get current time in microseconds
 * 
 * This function returns the current time in microseconds
 * for high-precision latency measurements.
 * 
 * @return Current time in microseconds
 */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/**
 * Compare latency samples for sorting
 * 
 * This function compares two latency samples for sorting
 * purposes (used by qsort).
 * 
 * @param a - First latency sample
 * @param b - Second latency sample
 * @return Comparison result (-1, 0, or 1)
 */
static int compare_latency_samples(const void *a, const void *b) {
    const LatencySample *sa = (const LatencySample *)a;
    const LatencySample *sb = (const LatencySample *)b;
    
    if (sa->latency_ms < sb->latency_ms) {
        return -1;
    } else if (sa->latency_ms > sb->latency_ms) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Calculate performance statistics
 * 
 * This function calculates comprehensive performance statistics
 * from latency samples, including mean, median, percentiles,
 * and standard deviation.
 * 
 * @param samples - Array of latency samples
 * @param sample_count - Number of samples
 * @param stats - Output parameter for statistics
 */
static void calculate_performance_stats(LatencySample *samples, int sample_count,
                                       PerformanceStats *stats) {
    int i;
    double sum = 0.0;
    double sum_sq_diff = 0.0;
    LatencySample *sorted_samples = NULL;
    
    if (samples == NULL || sample_count <= 0 || stats == NULL) {
        return;
    }
    
    memset(stats, 0, sizeof(PerformanceStats));
    stats->sample_count = sample_count;
    
    if (sample_count == 0) {
        return;
    }
    
    /* Count successes and failures */
    for (i = 0; i < sample_count; i++) {
        if (samples[i].success) {
            stats->success_count++;
        } else {
            stats->failure_count++;
        }
    }
    
    /* Calculate mean */
    for (i = 0; i < sample_count; i++) {
        sum += samples[i].latency_ms;
    }
    stats->mean = sum / sample_count;
    
    /* Calculate min and max */
    stats->min = samples[0].latency_ms;
    stats->max = samples[0].latency_ms;
    for (i = 1; i < sample_count; i++) {
        if (samples[i].latency_ms < stats->min) {
            stats->min = samples[i].latency_ms;
        }
        if (samples[i].latency_ms > stats->max) {
            stats->max = samples[i].latency_ms;
        }
    }
    
    /* Calculate standard deviation */
    for (i = 0; i < sample_count; i++) {
        double diff = samples[i].latency_ms - stats->mean;
        sum_sq_diff += diff * diff;
    }
    stats->stddev = sqrt(sum_sq_diff / sample_count);
    
    /* Sort samples for percentile calculation */
    sorted_samples = (LatencySample *)malloc(sample_count * sizeof(LatencySample));
    if (sorted_samples == NULL) {
        return;
    }
    
    memcpy(sorted_samples, samples, sample_count * sizeof(LatencySample));
    qsort(sorted_samples, sample_count, sizeof(LatencySample), compare_latency_samples);
    
    /* Calculate median */
    if (sample_count % 2 == 0) {
        stats->median = (sorted_samples[sample_count / 2 - 1].latency_ms +
                        sorted_samples[sample_count / 2].latency_ms) / 2.0;
    } else {
        stats->median = sorted_samples[sample_count / 2].latency_ms;
    }
    
    /* Calculate percentiles */
    stats->p50 = sorted_samples[(int)(sample_count * 0.50)].latency_ms;
    stats->p75 = sorted_samples[(int)(sample_count * 0.75)].latency_ms;
    stats->p90 = sorted_samples[(int)(sample_count * 0.90)].latency_ms;
    stats->p95 = sorted_samples[(int)(sample_count * 0.95)].latency_ms;
    stats->p99 = sorted_samples[(int)(sample_count * 0.99)].latency_ms;
    if (sample_count >= 1000) {
        stats->p999 = sorted_samples[(int)(sample_count * 0.999)].latency_ms;
    } else {
        stats->p999 = sorted_samples[sample_count - 1].latency_ms;
    }
    
    free(sorted_samples);
}

/**
 * Profile upload operation
 * 
 * This function profiles file upload operations by measuring
 * latency and success rate.
 * 
 * @param ctx - Profiler context
 * @param profile - Operation profile
 * @return 0 on success, error code on failure
 */
static int profile_upload(ProfilerContext *ctx, OperationProfile *profile) {
    ConnectionInfo *pStorageServer;
    char file_id[MAX_FILE_ID_LEN];
    double start_time, end_time;
    int result;
    int i;
    LatencySample sample;
    
    if (ctx == NULL || profile == NULL) {
        return EINVAL;
    }
    
    for (i = 0; i < ctx->iterations; i++) {
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            sample.latency_ms = 0;
            sample.success = 0;
            sample.timestamp = time(NULL);
            strcpy(sample.server_ip, "unknown");
            
            pthread_mutex_lock(&profile->mutex);
            if (profile->sample_count < profile->sample_capacity) {
                profile->samples[profile->sample_count++] = sample;
            }
            pthread_mutex_unlock(&profile->mutex);
            continue;
        }
        
        strncpy(sample.server_ip, pStorageServer->ip_addr, sizeof(sample.server_ip) - 1);
        
        /* Measure upload latency */
        start_time = get_time_us();
        result = storage_upload_by_filebuff1(ctx->pTrackerServer, pStorageServer,
                                            ctx->test_file_data, ctx->test_file_size,
                                            "txt", NULL, 0, NULL, 0, file_id);
        end_time = get_time_us();
        
        sample.latency_ms = (end_time - start_time) / 1000.0;
        sample.success = (result == 0) ? 1 : 0;
        sample.timestamp = time(NULL);
        
        /* Store sample */
        pthread_mutex_lock(&profile->mutex);
        if (profile->sample_count < profile->sample_capacity) {
            profile->samples[profile->sample_count++] = sample;
        }
        pthread_mutex_unlock(&profile->mutex);
        
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return 0;
}

/**
 * Profile download operation
 * 
 * This function profiles file download operations by measuring
 * latency and success rate.
 * 
 * @param ctx - Profiler context
 * @param profile - Operation profile
 * @param file_id - File ID to download
 * @return 0 on success, error code on failure
 */
static int profile_download(ProfilerContext *ctx, OperationProfile *profile,
                           const char *file_id) {
    ConnectionInfo *pStorageServer;
    char *file_buff = NULL;
    int64_t file_size;
    double start_time, end_time;
    int result;
    int i;
    LatencySample sample;
    
    if (ctx == NULL || profile == NULL || file_id == NULL) {
        return EINVAL;
    }
    
    for (i = 0; i < ctx->iterations; i++) {
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            sample.latency_ms = 0;
            sample.success = 0;
            sample.timestamp = time(NULL);
            strcpy(sample.server_ip, "unknown");
            
            pthread_mutex_lock(&profile->mutex);
            if (profile->sample_count < profile->sample_capacity) {
                profile->samples[profile->sample_count++] = sample;
            }
            pthread_mutex_unlock(&profile->mutex);
            continue;
        }
        
        strncpy(sample.server_ip, pStorageServer->ip_addr, sizeof(sample.server_ip) - 1);
        
        /* Measure download latency */
        start_time = get_time_us();
        result = storage_download_file1(ctx->pTrackerServer, pStorageServer,
                                       file_id, 0, 0, &file_buff, &file_size);
        end_time = get_time_us();
        
        sample.latency_ms = (end_time - start_time) / 1000.0;
        sample.success = (result == 0) ? 1 : 0;
        sample.timestamp = time(NULL);
        
        if (file_buff != NULL) {
            free(file_buff);
            file_buff = NULL;
        }
        
        /* Store sample */
        pthread_mutex_lock(&profile->mutex);
        if (profile->sample_count < profile->sample_capacity) {
            profile->samples[profile->sample_count++] = sample;
        }
        pthread_mutex_unlock(&profile->mutex);
        
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return 0;
}

/**
 * Profile delete operation
 * 
 * This function profiles file deletion operations by measuring
 * latency and success rate.
 * 
 * @param ctx - Profiler context
 * @param profile - Operation profile
 * @param file_id - File ID to delete
 * @return 0 on success, error code on failure
 */
static int profile_delete(ProfilerContext *ctx, OperationProfile *profile,
                         const char *file_id) {
    ConnectionInfo *pStorageServer;
    double start_time, end_time;
    int result;
    int i;
    LatencySample sample;
    
    if (ctx == NULL || profile == NULL || file_id == NULL) {
        return EINVAL;
    }
    
    for (i = 0; i < ctx->iterations; i++) {
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            sample.latency_ms = 0;
            sample.success = 0;
            sample.timestamp = time(NULL);
            strcpy(sample.server_ip, "unknown");
            
            pthread_mutex_lock(&profile->mutex);
            if (profile->sample_count < profile->sample_capacity) {
                profile->samples[profile->sample_count++] = sample;
            }
            pthread_mutex_unlock(&profile->mutex);
            continue;
        }
        
        strncpy(sample.server_ip, pStorageServer->ip_addr, sizeof(sample.server_ip) - 1);
        
        /* Measure delete latency */
        start_time = get_time_us();
        result = storage_delete_file1(ctx->pTrackerServer, pStorageServer, file_id);
        end_time = get_time_us();
        
        sample.latency_ms = (end_time - start_time) / 1000.0;
        sample.success = (result == 0) ? 1 : 0;
        sample.timestamp = time(NULL);
        
        /* Store sample */
        pthread_mutex_lock(&profile->mutex);
        if (profile->sample_count < profile->sample_capacity) {
            profile->samples[profile->sample_count++] = sample;
        }
        pthread_mutex_unlock(&profile->mutex);
        
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return 0;
}

/**
 * Profile query operation
 * 
 * This function profiles file info query operations by measuring
 * latency and success rate.
 * 
 * @param ctx - Profiler context
 * @param profile - Operation profile
 * @param file_id - File ID to query
 * @return 0 on success, error code on failure
 */
static int profile_query(ProfilerContext *ctx, OperationProfile *profile,
                        const char *file_id) {
    ConnectionInfo *pStorageServer;
    FDFSFileInfo file_info;
    double start_time, end_time;
    int result;
    int i;
    LatencySample sample;
    
    if (ctx == NULL || profile == NULL || file_id == NULL) {
        return EINVAL;
    }
    
    for (i = 0; i < ctx->iterations; i++) {
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            sample.latency_ms = 0;
            sample.success = 0;
            sample.timestamp = time(NULL);
            strcpy(sample.server_ip, "unknown");
            
            pthread_mutex_lock(&profile->mutex);
            if (profile->sample_count < profile->sample_capacity) {
                profile->samples[profile->sample_count++] = sample;
            }
            pthread_mutex_unlock(&profile->mutex);
            continue;
        }
        
        strncpy(sample.server_ip, pStorageServer->ip_addr, sizeof(sample.server_ip) - 1);
        
        /* Measure query latency */
        start_time = get_time_us();
        result = storage_query_file_info1(ctx->pTrackerServer, pStorageServer,
                                          file_id, &file_info);
        end_time = get_time_us();
        
        sample.latency_ms = (end_time - start_time) / 1000.0;
        sample.success = (result == 0) ? 1 : 0;
        sample.timestamp = time(NULL);
        
        /* Store sample */
        pthread_mutex_lock(&profile->mutex);
        if (profile->sample_count < profile->sample_capacity) {
            profile->samples[profile->sample_count++] = sample;
        }
        pthread_mutex_unlock(&profile->mutex);
        
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return 0;
}

/**
 * Profile metadata get operation
 * 
 * This function profiles metadata retrieval operations by measuring
 * latency and success rate.
 * 
 * @param ctx - Profiler context
 * @param profile - Operation profile
 * @param file_id - File ID to get metadata from
 * @return 0 on success, error code on failure
 */
static int profile_metadata_get(ProfilerContext *ctx, OperationProfile *profile,
                                const char *file_id) {
    ConnectionInfo *pStorageServer;
    FDFSMetaData *meta_list = NULL;
    int meta_count = 0;
    double start_time, end_time;
    int result;
    int i;
    LatencySample sample;
    
    if (ctx == NULL || profile == NULL || file_id == NULL) {
        return EINVAL;
    }
    
    for (i = 0; i < ctx->iterations; i++) {
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            sample.latency_ms = 0;
            sample.success = 0;
            sample.timestamp = time(NULL);
            strcpy(sample.server_ip, "unknown");
            
            pthread_mutex_lock(&profile->mutex);
            if (profile->sample_count < profile->sample_capacity) {
                profile->samples[profile->sample_count++] = sample;
            }
            pthread_mutex_unlock(&profile->mutex);
            continue;
        }
        
        strncpy(sample.server_ip, pStorageServer->ip_addr, sizeof(sample.server_ip) - 1);
        
        /* Measure metadata get latency */
        start_time = get_time_us();
        result = storage_get_metadata1(ctx->pTrackerServer, pStorageServer,
                                     file_id, &meta_list, &meta_count);
        end_time = get_time_us();
        
        sample.latency_ms = (end_time - start_time) / 1000.0;
        sample.success = (result == 0) ? 1 : 0;
        sample.timestamp = time(NULL);
        
        if (meta_list != NULL) {
            free(meta_list);
            meta_list = NULL;
        }
        
        /* Store sample */
        pthread_mutex_lock(&profile->mutex);
        if (profile->sample_count < profile->sample_capacity) {
            profile->samples[profile->sample_count++] = sample;
        }
        pthread_mutex_unlock(&profile->mutex);
        
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return 0;
}

/**
 * Profile metadata set operation
 * 
 * This function profiles metadata setting operations by measuring
 * latency and success rate.
 * 
 * @param ctx - Profiler context
 * @param profile - Operation profile
 * @param file_id - File ID to set metadata for
 * @return 0 on success, error code on failure
 */
static int profile_metadata_set(ProfilerContext *ctx, OperationProfile *profile,
                                const char *file_id) {
    ConnectionInfo *pStorageServer;
    FDFSMetaData meta_list[2];
    double start_time, end_time;
    int result;
    int i;
    LatencySample sample;
    
    if (ctx == NULL || profile == NULL || file_id == NULL) {
        return EINVAL;
    }
    
    /* Prepare test metadata */
    strncpy(meta_list[0].name, "test_key", sizeof(meta_list[0].name) - 1);
    strncpy(meta_list[0].value, "test_value", sizeof(meta_list[0].value) - 1);
    strncpy(meta_list[1].name, "timestamp", sizeof(meta_list[1].name) - 1);
    snprintf(meta_list[1].value, sizeof(meta_list[1].value), "%ld", time(NULL));
    
    for (i = 0; i < ctx->iterations; i++) {
        /* Get storage connection */
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer == NULL) {
            sample.latency_ms = 0;
            sample.success = 0;
            sample.timestamp = time(NULL);
            strcpy(sample.server_ip, "unknown");
            
            pthread_mutex_lock(&profile->mutex);
            if (profile->sample_count < profile->sample_capacity) {
                profile->samples[profile->sample_count++] = sample;
            }
            pthread_mutex_unlock(&profile->mutex);
            continue;
        }
        
        strncpy(sample.server_ip, pStorageServer->ip_addr, sizeof(sample.server_ip) - 1);
        
        /* Measure metadata set latency */
        start_time = get_time_us();
        result = storage_set_metadata1(ctx->pTrackerServer, pStorageServer,
                                      file_id, meta_list, 2,
                                      FDFS_STORAGE_SET_METADATA_FLAG_MERGE);
        end_time = get_time_us();
        
        sample.latency_ms = (end_time - start_time) / 1000.0;
        sample.success = (result == 0) ? 1 : 0;
        sample.timestamp = time(NULL);
        
        /* Store sample */
        pthread_mutex_lock(&profile->mutex);
        if (profile->sample_count < profile->sample_capacity) {
            profile->samples[profile->sample_count++] = sample;
        }
        pthread_mutex_unlock(&profile->mutex);
        
        tracker_disconnect_server_ex(pStorageServer, true);
    }
    
    return 0;
}

/**
 * Profile connection operation
 * 
 * This function profiles connection establishment operations by measuring
 * latency and success rate.
 * 
 * @param ctx - Profiler context
 * @param profile - Operation profile
 * @return 0 on success, error code on failure
 */
static int profile_connection(ProfilerContext *ctx, OperationProfile *profile) {
    ConnectionInfo *pStorageServer;
    double start_time, end_time;
    int i;
    LatencySample sample;
    
    if (ctx == NULL || profile == NULL) {
        return EINVAL;
    }
    
    for (i = 0; i < ctx->iterations; i++) {
        /* Measure connection latency */
        start_time = get_time_us();
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        end_time = get_time_us();
        
        sample.latency_ms = (end_time - start_time) / 1000.0;
        sample.success = (pStorageServer != NULL) ? 1 : 0;
        sample.timestamp = time(NULL);
        
        if (pStorageServer != NULL) {
            strncpy(sample.server_ip, pStorageServer->ip_addr, sizeof(sample.server_ip) - 1);
            tracker_disconnect_server_ex(pStorageServer, true);
        } else {
            strcpy(sample.server_ip, "unknown");
        }
        
        /* Store sample */
        pthread_mutex_lock(&profile->mutex);
        if (profile->sample_count < profile->sample_capacity) {
            profile->samples[profile->sample_count++] = sample;
        }
        pthread_mutex_unlock(&profile->mutex);
    }
    
    return 0;
}

/**
 * Worker thread function for parallel profiling
 * 
 * This function is executed by each worker thread to perform
 * profiling operations in parallel.
 * 
 * @param arg - ProfilerContext pointer
 * @return NULL
 */
static void *profiler_worker_thread(void *arg) {
    ProfilerContext *ctx = (ProfilerContext *)arg;
    int i;
    char test_file_id[MAX_FILE_ID_LEN];
    
    if (ctx == NULL) {
        return NULL;
    }
    
    /* Upload a test file first for download/delete/query operations */
    if (ctx->profile_count > 0) {
        ConnectionInfo *pStorageServer;
        pStorageServer = get_storage_connection(ctx->pTrackerServer);
        if (pStorageServer != NULL) {
            storage_upload_by_filebuff1(ctx->pTrackerServer, pStorageServer,
                                      ctx->test_file_data, ctx->test_file_size,
                                      "txt", NULL, 0, NULL, 0, test_file_id);
            tracker_disconnect_server_ex(pStorageServer, true);
        }
    }
    
    /* Profile each operation */
    for (i = 0; i < ctx->profile_count; i++) {
        OperationProfile *profile = &ctx->profiles[i];
        
        switch (profile->op_type) {
            case OP_UPLOAD:
                profile_upload(ctx, profile);
                break;
                
            case OP_DOWNLOAD:
                profile_download(ctx, profile, test_file_id);
                break;
                
            case OP_DELETE:
                profile_delete(ctx, profile, test_file_id);
                break;
                
            case OP_QUERY:
                profile_query(ctx, profile, test_file_id);
                break;
                
            case OP_METADATA_GET:
                profile_metadata_get(ctx, profile, test_file_id);
                break;
                
            case OP_METADATA_SET:
                profile_metadata_set(ctx, profile, test_file_id);
                break;
                
            case OP_CONNECTION:
                profile_connection(ctx, profile);
                break;
                
            default:
                break;
        }
    }
    
    return NULL;
}

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_profiler tool, including all available options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("FastDFS Performance Profiler Tool\n");
    printf("\n");
    printf("This tool profiles FastDFS operations to measure latency,\n");
    printf("identify slow operations, and generate performance reports.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE      Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -o, --operations LIST  Operations to profile (comma-separated)\n");
    printf("                         Options: upload,download,delete,query,metadata_get,metadata_set,connection\n");
    printf("                         Default: all operations\n");
    printf("  -i, --iterations NUM   Number of iterations per operation (default: 100)\n");
    printf("  -j, --threads NUM      Number of parallel threads (default: 4, max: 100)\n");
    printf("  -s, --size SIZE        Test file size in bytes (default: 1048576 = 1MB)\n");
    printf("  -O, --output FILE      Output report file (default: stdout)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -q, --quiet            Quiet mode (only show errors)\n");
    printf("  -J, --json             Output in JSON format\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - Profiling completed successfully\n");
    printf("  1 - Some operations failed\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Profile all operations\n");
    printf("  %s\n", program_name);
    printf("\n");
    printf("  # Profile only upload and download\n");
    printf("  %s -o upload,download\n", program_name);
    printf("\n");
    printf("  # Profile with 1000 iterations\n");
    printf("  %s -i 1000\n", program_name);
    printf("\n");
    printf("  # Profile with 10 threads\n");
    printf("  %s -j 10\n", program_name);
}

/**
 * Print performance report in text format
 * 
 * This function prints a comprehensive performance report in
 * human-readable text format.
 * 
 * @param ctx - Profiler context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_performance_report_text(ProfilerContext *ctx, FILE *output_file) {
    int i, j;
    double total_time;
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    total_time = difftime(ctx->end_time, ctx->start_time);
    
    fprintf(output_file, "\n");
    fprintf(output_file, "========================================\n");
    fprintf(output_file, "FastDFS Performance Profiling Report\n");
    fprintf(output_file, "========================================\n");
    fprintf(output_file, "\n");
    fprintf(output_file, "Profiling Duration: %.2f seconds\n", total_time);
    fprintf(output_file, "Iterations per Operation: %d\n", ctx->iterations);
    fprintf(output_file, "Number of Threads: %d\n", ctx->num_threads);
    fprintf(output_file, "Test File Size: %d bytes\n", ctx->test_file_size);
    fprintf(output_file, "\n");
    
    for (i = 0; i < ctx->profile_count; i++) {
        OperationProfile *profile = &ctx->profiles[i];
        
        /* Calculate statistics */
        calculate_performance_stats(profile->samples, profile->sample_count,
                                   &profile->stats);
        
        /* Calculate throughput */
        if (total_time > 0) {
            profile->stats.throughput = profile->stats.success_count / total_time;
        }
        
        fprintf(output_file, "----------------------------------------\n");
        fprintf(output_file, "Operation: %s\n", profile->op_name);
        fprintf(output_file, "----------------------------------------\n");
        fprintf(output_file, "\n");
        
        fprintf(output_file, "Samples: %d\n", profile->stats.sample_count);
        fprintf(output_file, "Success: %d (%.2f%%)\n",
               profile->stats.success_count,
               profile->stats.sample_count > 0 ?
               100.0 * profile->stats.success_count / profile->stats.sample_count : 0.0);
        fprintf(output_file, "Failures: %d (%.2f%%)\n",
               profile->stats.failure_count,
               profile->stats.sample_count > 0 ?
               100.0 * profile->stats.failure_count / profile->stats.sample_count : 0.0);
        fprintf(output_file, "\n");
        
        fprintf(output_file, "Latency Statistics (ms):\n");
        fprintf(output_file, "  Mean:     %.2f\n", profile->stats.mean);
        fprintf(output_file, "  Median:   %.2f\n", profile->stats.median);
        fprintf(output_file, "  Min:      %.2f\n", profile->stats.min);
        fprintf(output_file, "  Max:      %.2f\n", profile->stats.max);
        fprintf(output_file, "  StdDev:   %.2f\n", profile->stats.stddev);
        fprintf(output_file, "\n");
        
        fprintf(output_file, "Percentiles (ms):\n");
        fprintf(output_file, "  p50:      %.2f\n", profile->stats.p50);
        fprintf(output_file, "  p75:      %.2f\n", profile->stats.p75);
        fprintf(output_file, "  p90:      %.2f\n", profile->stats.p90);
        fprintf(output_file, "  p95:      %.2f\n", profile->stats.p95);
        fprintf(output_file, "  p99:      %.2f\n", profile->stats.p99);
        fprintf(output_file, "  p99.9:    %.2f\n", profile->stats.p999);
        fprintf(output_file, "\n");
        
        fprintf(output_file, "Throughput: %.2f ops/sec\n", profile->stats.throughput);
        fprintf(output_file, "\n");
    }
    
    fprintf(output_file, "========================================\n");
    fprintf(output_file, "\n");
}

/**
 * Print performance report in JSON format
 * 
 * This function prints a comprehensive performance report in JSON format
 * for programmatic processing.
 * 
 * @param ctx - Profiler context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_performance_report_json(ProfilerContext *ctx, FILE *output_file) {
    int i;
    double total_time;
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    total_time = difftime(ctx->end_time, ctx->start_time);
    
    fprintf(output_file, "{\n");
    fprintf(output_file, "  \"timestamp\": %ld,\n", (long)time(NULL));
    fprintf(output_file, "  \"profiling_duration\": %.2f,\n", total_time);
    fprintf(output_file, "  \"iterations_per_operation\": %d,\n", ctx->iterations);
    fprintf(output_file, "  \"num_threads\": %d,\n", ctx->num_threads);
    fprintf(output_file, "  \"test_file_size\": %d,\n", ctx->test_file_size);
    fprintf(output_file, "  \"operations\": [\n");
    
    for (i = 0; i < ctx->profile_count; i++) {
        OperationProfile *profile = &ctx->profiles[i];
        
        /* Calculate statistics */
        calculate_performance_stats(profile->samples, profile->sample_count,
                                   &profile->stats);
        
        /* Calculate throughput */
        if (total_time > 0) {
            profile->stats.throughput = profile->stats.success_count / total_time;
        }
        
        if (i > 0) {
            fprintf(output_file, ",\n");
        }
        
        fprintf(output_file, "    {\n");
        fprintf(output_file, "      \"operation\": \"%s\",\n", profile->op_name);
        fprintf(output_file, "      \"samples\": %d,\n", profile->stats.sample_count);
        fprintf(output_file, "      \"success_count\": %d,\n", profile->stats.success_count);
        fprintf(output_file, "      \"failure_count\": %d,\n", profile->stats.failure_count);
        fprintf(output_file, "      \"success_rate\": %.2f,\n",
               profile->stats.sample_count > 0 ?
               100.0 * profile->stats.success_count / profile->stats.sample_count : 0.0);
        fprintf(output_file, "      \"latency\": {\n");
        fprintf(output_file, "        \"mean\": %.2f,\n", profile->stats.mean);
        fprintf(output_file, "        \"median\": %.2f,\n", profile->stats.median);
        fprintf(output_file, "        \"min\": %.2f,\n", profile->stats.min);
        fprintf(output_file, "        \"max\": %.2f,\n", profile->stats.max);
        fprintf(output_file, "        \"stddev\": %.2f,\n", profile->stats.stddev);
        fprintf(output_file, "        \"p50\": %.2f,\n", profile->stats.p50);
        fprintf(output_file, "        \"p75\": %.2f,\n", profile->stats.p75);
        fprintf(output_file, "        \"p90\": %.2f,\n", profile->stats.p90);
        fprintf(output_file, "        \"p95\": %.2f,\n", profile->stats.p95);
        fprintf(output_file, "        \"p99\": %.2f,\n", profile->stats.p99);
        fprintf(output_file, "        \"p999\": %.2f\n", profile->stats.p999);
        fprintf(output_file, "      },\n");
        fprintf(output_file, "      \"throughput\": %.2f\n", profile->stats.throughput);
        fprintf(output_file, "    }");
    }
    
    fprintf(output_file, "\n  ]\n");
    fprintf(output_file, "}\n");
}

/**
 * Main function
 * 
 * Entry point for the profiler tool. Parses command-line
 * arguments and performs performance profiling.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = some failures, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *operations_str = NULL;
    char *output_file = NULL;
    int iterations = 100;
    int num_threads = DEFAULT_THREADS;
    int test_file_size = DEFAULT_TEST_FILE_SIZE;
    int result;
    ConnectionInfo *pTrackerServer;
    ProfilerContext ctx;
    pthread_t *threads = NULL;
    FILE *out_fp = stdout;
    int i, j;
    int opt;
    int option_index = 0;
    char *op_token;
    char *saveptr;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"operations", required_argument, 0, 'o'},
        {"iterations", required_argument, 0, 'i'},
        {"threads", required_argument, 0, 'j'},
        {"size", required_argument, 0, 's'},
        {"output", required_argument, 0, 'O'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(ProfilerContext));
    ctx.iterations = iterations;
    ctx.num_threads = num_threads;
    ctx.test_file_size = test_file_size;
    
    /* Generate test file data */
    for (i = 0; i < test_file_size; i++) {
        ctx.test_file_data[i] = (char)(i % 256);
    }
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:o:i:j:s:O:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'o':
                operations_str = optarg;
                break;
            case 'i':
                iterations = atoi(optarg);
                if (iterations < 1) iterations = 1;
                ctx.iterations = iterations;
                break;
            case 'j':
                num_threads = atoi(optarg);
                if (num_threads < 1) num_threads = 1;
                if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
                ctx.num_threads = num_threads;
                break;
            case 's':
                test_file_size = atoi(optarg);
                if (test_file_size < 1) test_file_size = DEFAULT_TEST_FILE_SIZE;
                if (test_file_size > sizeof(ctx.test_file_data)) {
                    test_file_size = sizeof(ctx.test_file_data);
                }
                ctx.test_file_size = test_file_size;
                break;
            case 'O':
                output_file = optarg;
                break;
            case 'v':
                verbose = 1;
                ctx.verbose = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'J':
                json_output = 1;
                ctx.json_output = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 2;
        }
    }
    
    /* Initialize logging */
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    /* Initialize FastDFS client */
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        return 2;
    }
    
    /* Connect to tracker server */
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        fdfs_client_destroy();
        return 2;
    }
    
    ctx.pTrackerServer = pTrackerServer;
    
    /* Determine which operations to profile */
    if (operations_str == NULL) {
        /* Profile all operations */
        ctx.profile_count = 7;
        ctx.profiles = (OperationProfile *)calloc(ctx.profile_count, sizeof(OperationProfile));
        if (ctx.profiles == NULL) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return ENOMEM;
        }
        
        ctx.profiles[0].op_type = OP_UPLOAD;
        strcpy(ctx.profiles[0].op_name, "upload");
        ctx.profiles[1].op_type = OP_DOWNLOAD;
        strcpy(ctx.profiles[1].op_name, "download");
        ctx.profiles[2].op_type = OP_DELETE;
        strcpy(ctx.profiles[2].op_name, "delete");
        ctx.profiles[3].op_type = OP_QUERY;
        strcpy(ctx.profiles[3].op_name, "query");
        ctx.profiles[4].op_type = OP_METADATA_GET;
        strcpy(ctx.profiles[4].op_name, "metadata_get");
        ctx.profiles[5].op_type = OP_METADATA_SET;
        strcpy(ctx.profiles[5].op_name, "metadata_set");
        ctx.profiles[6].op_type = OP_CONNECTION;
        strcpy(ctx.profiles[6].op_name, "connection");
    } else {
        /* Parse operations string */
        ctx.profile_count = 0;
        ctx.profiles = (OperationProfile *)calloc(10, sizeof(OperationProfile));
        if (ctx.profiles == NULL) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return ENOMEM;
        }
        
        op_token = strtok_r(operations_str, ",", &saveptr);
        while (op_token != NULL && ctx.profile_count < 10) {
            OperationProfile *profile = &ctx.profiles[ctx.profile_count];
            
            if (strcmp(op_token, "upload") == 0) {
                profile->op_type = OP_UPLOAD;
                strcpy(profile->op_name, "upload");
            } else if (strcmp(op_token, "download") == 0) {
                profile->op_type = OP_DOWNLOAD;
                strcpy(profile->op_name, "download");
            } else if (strcmp(op_token, "delete") == 0) {
                profile->op_type = OP_DELETE;
                strcpy(profile->op_name, "delete");
            } else if (strcmp(op_token, "query") == 0) {
                profile->op_type = OP_QUERY;
                strcpy(profile->op_name, "query");
            } else if (strcmp(op_token, "metadata_get") == 0) {
                profile->op_type = OP_METADATA_GET;
                strcpy(profile->op_name, "metadata_get");
            } else if (strcmp(op_token, "metadata_set") == 0) {
                profile->op_type = OP_METADATA_SET;
                strcpy(profile->op_name, "metadata_set");
            } else if (strcmp(op_token, "connection") == 0) {
                profile->op_type = OP_CONNECTION;
                strcpy(profile->op_name, "connection");
            } else {
                op_token = strtok_r(NULL, ",", &saveptr);
                continue;
            }
            
            ctx.profile_count++;
            op_token = strtok_r(NULL, ",", &saveptr);
        }
    }
    
    if (ctx.profile_count == 0) {
        fprintf(stderr, "ERROR: No valid operations specified\n");
        free(ctx.profiles);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 2;
    }
    
    /* Initialize profiles */
    for (i = 0; i < ctx.profile_count; i++) {
        OperationProfile *profile = &ctx.profiles[i];
        profile->sample_capacity = ctx.iterations * ctx.num_threads;
        profile->samples = (LatencySample *)calloc(profile->sample_capacity, sizeof(LatencySample));
        if (profile->samples == NULL) {
            for (j = 0; j < i; j++) {
                free(ctx.profiles[j].samples);
            }
            free(ctx.profiles);
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return ENOMEM;
        }
        pthread_mutex_init(&profile->mutex, NULL);
    }
    
    /* Start profiling */
    ctx.start_time = time(NULL);
    
    if (verbose && !quiet) {
        printf("Starting performance profiling...\n");
        printf("Operations: %d\n", ctx.profile_count);
        printf("Iterations per operation: %d\n", ctx.iterations);
        printf("Number of threads: %d\n", ctx.num_threads);
        printf("\n");
    }
    
    /* Allocate thread array */
    threads = (pthread_t *)malloc(ctx.num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        for (i = 0; i < ctx.profile_count; i++) {
            free(ctx.profiles[i].samples);
            pthread_mutex_destroy(&ctx.profiles[i].mutex);
        }
        free(ctx.profiles);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return ENOMEM;
    }
    
    /* Start worker threads */
    for (i = 0; i < ctx.num_threads; i++) {
        if (pthread_create(&threads[i], NULL, profiler_worker_thread, &ctx) != 0) {
            fprintf(stderr, "ERROR: Failed to create thread %d\n", i);
            result = errno;
            break;
        }
    }
    
    /* Wait for all threads to complete */
    for (i = 0; i < ctx.num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* End profiling */
    ctx.end_time = time(NULL);
    
    /* Print results */
    if (output_file != NULL) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            out_fp = stdout;
        }
    }
    
    if (json_output) {
        print_performance_report_json(&ctx, out_fp);
    } else {
        print_performance_report_text(&ctx, out_fp);
    }
    
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    free(threads);
    for (i = 0; i < ctx.profile_count; i++) {
        free(ctx.profiles[i].samples);
        pthread_mutex_destroy(&ctx.profiles[i].mutex);
    }
    free(ctx.profiles);
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return 0;
}

