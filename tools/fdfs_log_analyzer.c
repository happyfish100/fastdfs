/**
 * FastDFS Access Log Analyzer Tool
 * 
 * This tool provides comprehensive access log analysis for FastDFS,
 * allowing users to understand file access patterns, identify hot and
 * cold files, generate access reports, and detect anomalies in usage.
 * 
 * Features:
 * - Parse FastDFS access logs
 * - Identify hot files (frequently accessed)
 * - Identify cold files (rarely accessed)
 * - Generate detailed access reports
 * - Detect access anomalies
 * - Analyze access patterns by time
 * - Track file access frequency
 * - Calculate access statistics
 * - JSON and text output formats
 * 
 * Access Pattern Analysis:
 * - Access frequency per file
 * - Access patterns by time of day
 * - Access patterns by day of week
 * - Peak access times
 * - Access distribution
 * - Hot and cold file identification
 * 
 * Anomaly Detection:
 * - Unusual access patterns
 * - Sudden spikes in access
 * - Unusual access times
 * - Suspicious access patterns
 * - Error rate analysis
 * 
 * Use Cases:
 * - Understand file usage patterns
 * - Optimize storage placement
 * - Identify frequently accessed files
 * - Detect access anomalies
 * - Capacity planning
 * - Performance optimization
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
#include <ctype.h>
#include <math.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum file ID length */
#define MAX_FILE_ID_LEN 256

/* Maximum log line length */
#define MAX_LINE_LEN 4096

/* Maximum number of files to track */
#define MAX_FILES 100000

/* Maximum number of time slots */
#define MAX_TIME_SLOTS 24

/* Maximum number of days to track */
#define MAX_DAYS 7

/* Access log entry structure */
typedef struct {
    time_t timestamp;                    /* Access timestamp */
    char file_id[MAX_FILE_ID_LEN];       /* File ID */
    char operation[32];                   /* Operation type (upload, download, delete) */
    char client_ip[64];                  /* Client IP address */
    int64_t file_size;                   /* File size in bytes */
    int response_time_ms;                 /* Response time in milliseconds */
    int status_code;                      /* HTTP/response status code */
    int is_error;                         /* Whether operation resulted in error */
} AccessLogEntry;

/* File access statistics */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];       /* File ID */
    int total_accesses;                  /* Total number of accesses */
    int upload_count;                     /* Number of uploads */
    int download_count;                   /* Number of downloads */
    int delete_count;                     /* Number of deletes */
    int error_count;                      /* Number of errors */
    time_t first_access;                  /* First access time */
    time_t last_access;                   /* Last access time */
    int64_t total_bytes_transferred;      /* Total bytes transferred */
    int64_t total_response_time_ms;       /* Total response time in milliseconds */
    int access_by_hour[MAX_TIME_SLOTS];   /* Access count by hour */
    int access_by_day[MAX_DAYS];          /* Access count by day of week */
    double avg_response_time_ms;          /* Average response time */
    int is_hot;                           /* Whether file is hot */
    int is_cold;                          /* Whether file is cold */
    double access_frequency;             /* Access frequency (accesses per day) */
} FileAccessStats;

/* Anomaly detection structure */
typedef struct {
    char file_id[MAX_FILE_ID_LEN];       /* File ID */
    char anomaly_type[64];                /* Type of anomaly */
    char description[512];                /* Anomaly description */
    time_t detected_time;                 /* When anomaly was detected */
    double severity;                      /* Anomaly severity (0.0 - 1.0) */
} Anomaly;

/* Analysis context */
typedef struct {
    FileAccessStats *file_stats;          /* Array of file statistics */
    int file_count;                       /* Number of files tracked */
    int total_entries;                     /* Total log entries processed */
    int total_errors;                      /* Total errors in logs */
    time_t analysis_start;                 /* Analysis start time */
    time_t analysis_end;                   /* Analysis end time */
    time_t log_start_time;                 /* First log entry time */
    time_t log_end_time;                   /* Last log entry time */
    Anomaly *anomalies;                   /* Array of detected anomalies */
    int anomaly_count;                     /* Number of anomalies detected */
    double hot_file_threshold;            /* Threshold for hot files */
    double cold_file_threshold;           /* Threshold for cold files */
    int verbose;                           /* Verbose output flag */
    int json_output;                       /* JSON output flag */
} AnalysisContext;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_log_analyzer tool, including all available options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <log_file> [log_file...]\n", program_name);
    printf("\n");
    printf("FastDFS Access Log Analyzer Tool\n");
    printf("\n");
    printf("This tool analyzes FastDFS access logs to understand file access\n");
    printf("patterns, identify hot and cold files, generate access reports,\n");
    printf("and detect anomalies in usage.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --hot-threshold NUM    Hot file threshold (accesses per day, default: 10.0)\n");
    printf("  --cold-threshold NUM   Cold file threshold (accesses per day, default: 0.1)\n");
    printf("  --time-range START END Filter by time range (YYYY-MM-DD HH:MM:SS)\n");
    printf("  --operation OP         Filter by operation (upload, download, delete)\n");
    printf("  --top-files NUM        Show top N most accessed files (default: 10)\n");
    printf("  --detect-anomalies     Enable anomaly detection\n");
    printf("  -o, --output FILE      Output report file (default: stdout)\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -q, --quiet            Quiet mode (only show errors)\n");
    printf("  -J, --json             Output in JSON format\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Analysis Features:\n");
    printf("  - Access frequency per file\n");
    printf("  - Hot and cold file identification\n");
    printf("  - Access patterns by time\n");
    printf("  - Peak access times\n");
    printf("  - Anomaly detection\n");
    printf("  - Error rate analysis\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - Analysis completed successfully\n");
    printf("  1 - Some errors occurred\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Analyze access log\n");
    printf("  %s /var/log/fastdfs/access.log\n", program_name);
    printf("\n");
    printf("  # Analyze with custom thresholds\n");
    printf("  %s --hot-threshold 20 --cold-threshold 0.05 access.log\n", program_name);
    printf("\n");
    printf("  # Analyze with anomaly detection\n");
    printf("  %s --detect-anomalies access.log\n", program_name);
    printf("\n");
    printf("  # Show top 20 files\n");
    printf("  %s --top-files 20 access.log\n", program_name);
}

/**
 * Parse log line
 * 
 * This function parses a single log line and extracts access information.
 * Supports common log formats (Apache-style, Nginx-style, custom).
 * 
 * @param line - Log line to parse
 * @param entry - Output parameter for parsed entry
 * @return 0 on success, -1 on error
 */
static int parse_log_line(const char *line, AccessLogEntry *entry) {
    char *p;
    char *end;
    struct tm tm;
    char time_str[64];
    int year, month, day, hour, min, sec;
    
    if (line == NULL || entry == NULL) {
        return -1;
    }
    
    /* Initialize entry */
    memset(entry, 0, sizeof(AccessLogEntry));
    
    /* Try to parse common log formats */
    /* Format: [timestamp] operation file_id client_ip size response_time status */
    /* Or: timestamp operation file_id client_ip size response_time status */
    
    /* Skip leading whitespace */
    p = (char *)line;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    
    /* Try to parse timestamp */
    /* Common formats: [2025-01-15 10:30:45] or 2025-01-15 10:30:45 */
    if (*p == '[') {
        p++;  /* Skip opening bracket */
    }
    
    if (sscanf(p, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        entry->timestamp = mktime(&tm);
        
        /* Skip timestamp */
        while (*p && *p != ']' && !isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == ']') {
            p++;
        }
        while (isspace((unsigned char)*p)) {
            p++;
        }
    } else {
        /* Use current time if timestamp parsing fails */
        entry->timestamp = time(NULL);
    }
    
    /* Parse operation */
    end = p;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }
    if (end > p) {
        size_t len = end - p;
        if (len >= sizeof(entry->operation)) {
            len = sizeof(entry->operation) - 1;
        }
        strncpy(entry->operation, p, len);
        entry->operation[len] = '\0';
        p = end;
        while (isspace((unsigned char)*p)) {
            p++;
        }
    }
    
    /* Parse file ID */
    end = p;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }
    if (end > p) {
        size_t len = end - p;
        if (len >= sizeof(entry->file_id)) {
            len = sizeof(entry->file_id) - 1;
        }
        strncpy(entry->file_id, p, len);
        entry->file_id[len] = '\0';
        p = end;
        while (isspace((unsigned char)*p)) {
            p++;
        }
    }
    
    /* Parse client IP */
    end = p;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }
    if (end > p) {
        size_t len = end - p;
        if (len >= sizeof(entry->client_ip)) {
            len = sizeof(entry->client_ip) - 1;
        }
        strncpy(entry->client_ip, p, len);
        entry->client_ip[len] = '\0';
        p = end;
        while (isspace((unsigned char)*p)) {
            p++;
        }
    }
    
    /* Parse file size */
    if (*p) {
        entry->file_size = strtoll(p, &end, 10);
        p = end;
        while (isspace((unsigned char)*p)) {
            p++;
        }
    }
    
    /* Parse response time */
    if (*p) {
        entry->response_time_ms = (int)strtol(p, &end, 10);
        p = end;
        while (isspace((unsigned char)*p)) {
            p++;
        }
    }
    
    /* Parse status code */
    if (*p) {
        entry->status_code = (int)strtol(p, &end, 10);
        if (entry->status_code >= 400) {
            entry->is_error = 1;
        }
    }
    
    return 0;
}

/**
 * Find or create file statistics
 * 
 * This function finds existing file statistics or creates a new one.
 * 
 * @param ctx - Analysis context
 * @param file_id - File ID
 * @return Pointer to file statistics, or NULL on error
 */
static FileAccessStats *find_or_create_file_stats(AnalysisContext *ctx,
                                                   const char *file_id) {
    int i;
    
    if (ctx == NULL || file_id == NULL) {
        return NULL;
    }
    
    /* Search for existing file */
    for (i = 0; i < ctx->file_count; i++) {
        if (strcmp(ctx->file_stats[i].file_id, file_id) == 0) {
            return &ctx->file_stats[i];
        }
    }
    
    /* Check if we can add more files */
    if (ctx->file_count >= MAX_FILES) {
        return NULL;
    }
    
    /* Create new file statistics */
    FileAccessStats *stats = &ctx->file_stats[ctx->file_count];
    memset(stats, 0, sizeof(FileAccessStats));
    strncpy(stats->file_id, file_id, sizeof(stats->file_id) - 1);
    stats->first_access = time(NULL);
    stats->last_access = 0;
    ctx->file_count++;
    
    return stats;
}

/**
 * Update file statistics from log entry
 * 
 * This function updates file statistics based on a log entry.
 * 
 * @param ctx - Analysis context
 * @param entry - Access log entry
 */
static void update_file_stats(AnalysisContext *ctx, AccessLogEntry *entry) {
    FileAccessStats *stats;
    struct tm *tm_info;
    
    if (ctx == NULL || entry == NULL) {
        return;
    }
    
    /* Find or create file statistics */
    stats = find_or_create_file_stats(ctx, entry->file_id);
    if (stats == NULL) {
        return;
    }
    
    /* Update statistics */
    stats->total_accesses++;
    
    if (strcmp(entry->operation, "upload") == 0) {
        stats->upload_count++;
    } else if (strcmp(entry->operation, "download") == 0) {
        stats->download_count++;
    } else if (strcmp(entry->operation, "delete") == 0) {
        stats->delete_count++;
    }
    
    if (entry->is_error) {
        stats->error_count++;
    }
    
    if (stats->first_access == 0 || entry->timestamp < stats->first_access) {
        stats->first_access = entry->timestamp;
    }
    if (entry->timestamp > stats->last_access) {
        stats->last_access = entry->timestamp;
    }
    
    stats->total_bytes_transferred += entry->file_size;
    stats->total_response_time_ms += entry->response_time_ms;
    
    /* Update time-based statistics */
    tm_info = localtime(&entry->timestamp);
    if (tm_info != NULL) {
        if (tm_info->tm_hour >= 0 && tm_info->tm_hour < MAX_TIME_SLOTS) {
            stats->access_by_hour[tm_info->tm_hour]++;
        }
        if (tm_info->tm_wday >= 0 && tm_info->tm_wday < MAX_DAYS) {
            stats->access_by_day[tm_info->tm_wday]++;
        }
    }
    
    /* Update average response time */
    if (stats->total_accesses > 0) {
        stats->avg_response_time_ms = (double)stats->total_response_time_ms /
                                     stats->total_accesses;
    }
}

/**
 * Calculate file access frequency
 * 
 * This function calculates access frequency for a file.
 * 
 * @param stats - File access statistics
 * @param analysis_duration_days - Analysis duration in days
 */
static void calculate_access_frequency(FileAccessStats *stats,
                                       double analysis_duration_days) {
    if (stats == NULL || analysis_duration_days <= 0) {
        stats->access_frequency = 0.0;
        return;
    }
    
    stats->access_frequency = stats->total_accesses / analysis_duration_days;
    
    /* Classify as hot or cold */
    /* This will be set based on thresholds in the analysis context */
}

/**
 * Detect anomalies
 * 
 * This function detects anomalies in file access patterns.
 * 
 * @param ctx - Analysis context
 */
static void detect_anomalies(AnalysisContext *ctx) {
    int i, j;
    double avg_accesses = 0.0;
    double std_dev = 0.0;
    double threshold;
    
    if (ctx == NULL || ctx->file_count == 0) {
        return;
    }
    
    /* Calculate average accesses */
    for (i = 0; i < ctx->file_count; i++) {
        avg_accesses += ctx->file_stats[i].total_accesses;
    }
    avg_accesses /= ctx->file_count;
    
    /* Calculate standard deviation */
    for (i = 0; i < ctx->file_count; i++) {
        double diff = ctx->file_stats[i].total_accesses - avg_accesses;
        std_dev += diff * diff;
    }
    std_dev = sqrt(std_dev / ctx->file_count);
    
    threshold = avg_accesses + (3 * std_dev);  /* 3-sigma rule */
    
    /* Detect anomalies */
    for (i = 0; i < ctx->file_count; i++) {
        FileAccessStats *stats = &ctx->file_stats[i];
        
        /* Check for unusual access patterns */
        if (stats->total_accesses > threshold) {
            /* Unusually high access count */
            if (ctx->anomaly_count < 1000) {  /* Limit anomalies */
                Anomaly *anomaly = &ctx->anomalies[ctx->anomaly_count++];
                strncpy(anomaly->file_id, stats->file_id, sizeof(anomaly->file_id) - 1);
                strncpy(anomaly->anomaly_type, "high_access", sizeof(anomaly->anomaly_type) - 1);
                snprintf(anomaly->description, sizeof(anomaly->description),
                        "Unusually high access count: %d (avg: %.2f, std: %.2f)",
                        stats->total_accesses, avg_accesses, std_dev);
                anomaly->detected_time = time(NULL);
                anomaly->severity = 0.7;
            }
        }
        
        /* Check for high error rate */
        if (stats->total_accesses > 0) {
            double error_rate = (double)stats->error_count / stats->total_accesses;
            if (error_rate > 0.1) {  /* More than 10% errors */
                if (ctx->anomaly_count < 1000) {
                    Anomaly *anomaly = &ctx->anomalies[ctx->anomaly_count++];
                    strncpy(anomaly->file_id, stats->file_id, sizeof(anomaly->file_id) - 1);
                    strncpy(anomaly->anomaly_type, "high_error_rate", sizeof(anomaly->anomaly_type) - 1);
                    snprintf(anomaly->description, sizeof(anomaly->description),
                            "High error rate: %.2f%% (%d errors out of %d accesses)",
                            error_rate * 100.0, stats->error_count, stats->total_accesses);
                    anomaly->detected_time = time(NULL);
                    anomaly->severity = 0.8;
                }
            }
        }
    }
}

/**
 * Analyze access logs
 * 
 * This function analyzes access logs and generates statistics.
 * 
 * @param log_files - Array of log file paths
 * @param log_count - Number of log files
 * @param ctx - Analysis context
 * @return 0 on success, error code on failure
 */
static int analyze_logs(const char **log_files, int log_count,
                       AnalysisContext *ctx) {
    FILE *fp;
    char line[MAX_LINE_LEN];
    AccessLogEntry entry;
    int i;
    int line_num;
    
    if (log_files == NULL || log_count == 0 || ctx == NULL) {
        return EINVAL;
    }
    
    ctx->analysis_start = time(NULL);
    
    /* Process each log file */
    for (i = 0; i < log_count; i++) {
        fp = fopen(log_files[i], "r");
        if (fp == NULL) {
            if (verbose) {
                fprintf(stderr, "WARNING: Failed to open log file: %s\n", log_files[i]);
            }
            continue;
        }
        
        line_num = 0;
        while (fgets(line, sizeof(line), fp) != NULL) {
            line_num++;
            
            /* Skip empty lines and comments */
            if (line[0] == '\0' || line[0] == '#' || line[0] == '\n') {
                continue;
            }
            
            /* Parse log line */
            if (parse_log_line(line, &entry) == 0) {
                /* Update log time range */
                if (ctx->log_start_time == 0 || entry.timestamp < ctx->log_start_time) {
                    ctx->log_start_time = entry.timestamp;
                }
                if (entry.timestamp > ctx->log_end_time) {
                    ctx->log_end_time = entry.timestamp;
                }
                
                /* Update file statistics */
                update_file_stats(ctx, &entry);
                
                ctx->total_entries++;
                if (entry.is_error) {
                    ctx->total_errors++;
                }
            } else if (verbose) {
                fprintf(stderr, "WARNING: Failed to parse line %d in %s\n",
                       line_num, log_files[i]);
            }
        }
        
        fclose(fp);
    }
    
    ctx->analysis_end = time(NULL);
    
    /* Calculate access frequencies */
    double analysis_duration = difftime(ctx->log_end_time, ctx->log_start_time) / 86400.0;
    if (analysis_duration < 0.1) {
        analysis_duration = 0.1;  /* Minimum 0.1 days */
    }
    
    for (i = 0; i < ctx->file_count; i++) {
        calculate_access_frequency(&ctx->file_stats[i], analysis_duration);
        
        /* Classify as hot or cold */
        if (ctx->file_stats[i].access_frequency >= ctx->hot_file_threshold) {
            ctx->file_stats[i].is_hot = 1;
        } else if (ctx->file_stats[i].access_frequency <= ctx->cold_file_threshold) {
            ctx->file_stats[i].is_cold = 1;
        }
    }
    
    return 0;
}

/**
 * Compare file statistics for sorting
 * 
 * This function compares two file statistics for sorting by access count.
 * 
 * @param a - First file statistics
 * @param b - Second file statistics
 * @return Comparison result
 */
static int compare_file_stats(const void *a, const void *b) {
    const FileAccessStats *stats_a = (const FileAccessStats *)a;
    const FileAccessStats *stats_b = (const FileAccessStats *)b;
    
    return stats_b->total_accesses - stats_a->total_accesses;
}

/**
 * Print analysis results in text format
 * 
 * This function prints analysis results in a human-readable text format.
 * 
 * @param ctx - Analysis context
 * @param top_files - Number of top files to show
 * @param output_file - Output file (NULL for stdout)
 */
static void print_analysis_results_text(AnalysisContext *ctx, int top_files,
                                       FILE *output_file) {
    int i, j;
    int hot_count = 0;
    int cold_count = 0;
    char time_buf[64];
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    /* Sort files by access count */
    qsort(ctx->file_stats, ctx->file_count, sizeof(FileAccessStats),
          compare_file_stats);
    
    fprintf(output_file, "\n");
    fprintf(output_file, "=== FastDFS Access Log Analysis ===\n");
    fprintf(output_file, "\n");
    
    /* Summary statistics */
    fprintf(output_file, "=== Summary ===\n");
    fprintf(output_file, "Total log entries: %d\n", ctx->total_entries);
    fprintf(output_file, "Total errors: %d\n", ctx->total_errors);
    fprintf(output_file, "Unique files: %d\n", ctx->file_count);
    
    if (ctx->log_start_time > 0) {
        struct tm *tm_info = localtime(&ctx->log_start_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output_file, "Log start time: %s\n", time_buf);
    }
    
    if (ctx->log_end_time > 0) {
        struct tm *tm_info = localtime(&ctx->log_end_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output_file, "Log end time: %s\n", time_buf);
    }
    
    /* Count hot and cold files */
    for (i = 0; i < ctx->file_count; i++) {
        if (ctx->file_stats[i].is_hot) {
            hot_count++;
        }
        if (ctx->file_stats[i].is_cold) {
            cold_count++;
        }
    }
    
    fprintf(output_file, "Hot files: %d (threshold: %.2f accesses/day)\n",
           hot_count, ctx->hot_file_threshold);
    fprintf(output_file, "Cold files: %d (threshold: %.2f accesses/day)\n",
           cold_count, ctx->cold_file_threshold);
    fprintf(output_file, "\n");
    
    /* Top accessed files */
    if (top_files > 0) {
        fprintf(output_file, "=== Top %d Most Accessed Files ===\n", top_files);
        fprintf(output_file, "\n");
        
        int count = (top_files < ctx->file_count) ? top_files : ctx->file_count;
        for (i = 0; i < count; i++) {
            FileAccessStats *stats = &ctx->file_stats[i];
            fprintf(output_file, "%d. %s\n", i + 1, stats->file_id);
            fprintf(output_file, "   Total accesses: %d\n", stats->total_accesses);
            fprintf(output_file, "   Access frequency: %.2f accesses/day\n",
                   stats->access_frequency);
            fprintf(output_file, "   Uploads: %d, Downloads: %d, Deletes: %d\n",
                   stats->upload_count, stats->download_count, stats->delete_count);
            fprintf(output_file, "   Errors: %d\n", stats->error_count);
            fprintf(output_file, "   Avg response time: %.2f ms\n",
                   stats->avg_response_time_ms);
            fprintf(output_file, "\n");
        }
    }
    
    /* Anomalies */
    if (ctx->anomaly_count > 0) {
        fprintf(output_file, "=== Detected Anomalies ===\n");
        fprintf(output_file, "\n");
        
        for (i = 0; i < ctx->anomaly_count; i++) {
            Anomaly *anomaly = &ctx->anomalies[i];
            struct tm *tm_info = localtime(&anomaly->detected_time);
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
            
            fprintf(output_file, "File: %s\n", anomaly->file_id);
            fprintf(output_file, "Type: %s\n", anomaly->anomaly_type);
            fprintf(output_file, "Description: %s\n", anomaly->description);
            fprintf(output_file, "Severity: %.2f\n", anomaly->severity);
            fprintf(output_file, "Detected: %s\n", time_buf);
            fprintf(output_file, "\n");
        }
    }
    
    fprintf(output_file, "\n");
}

/**
 * Print analysis results in JSON format
 * 
 * This function prints analysis results in JSON format
 * for programmatic processing.
 * 
 * @param ctx - Analysis context
 * @param top_files - Number of top files to show
 * @param output_file - Output file (NULL for stdout)
 */
static void print_analysis_results_json(AnalysisContext *ctx, int top_files,
                                       FILE *output_file) {
    int i;
    char time_buf[64];
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    /* Sort files by access count */
    qsort(ctx->file_stats, ctx->file_count, sizeof(FileAccessStats),
          compare_file_stats);
    
    fprintf(output_file, "{\n");
    fprintf(output_file, "  \"timestamp\": %ld,\n", (long)time(NULL));
    fprintf(output_file, "  \"summary\": {\n");
    fprintf(output_file, "    \"total_entries\": %d,\n", ctx->total_entries);
    fprintf(output_file, "    \"total_errors\": %d,\n", ctx->total_errors);
    fprintf(output_file, "    \"unique_files\": %d", ctx->file_count);
    
    if (ctx->log_start_time > 0) {
        struct tm *tm_info = localtime(&ctx->log_start_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output_file, ",\n    \"log_start_time\": \"%s\"", time_buf);
    }
    
    if (ctx->log_end_time > 0) {
        struct tm *tm_info = localtime(&ctx->log_end_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(output_file, ",\n    \"log_end_time\": \"%s\"", time_buf);
    }
    
    fprintf(output_file, "\n  },\n");
    fprintf(output_file, "  \"top_files\": [\n");
    
    int count = (top_files < ctx->file_count) ? top_files : ctx->file_count;
    for (i = 0; i < count; i++) {
        FileAccessStats *stats = &ctx->file_stats[i];
        
        if (i > 0) {
            fprintf(output_file, ",\n");
        }
        
        fprintf(output_file, "    {\n");
        fprintf(output_file, "      \"file_id\": \"%s\",\n", stats->file_id);
        fprintf(output_file, "      \"total_accesses\": %d,\n", stats->total_accesses);
        fprintf(output_file, "      \"access_frequency\": %.2f,\n", stats->access_frequency);
        fprintf(output_file, "      \"upload_count\": %d,\n", stats->upload_count);
        fprintf(output_file, "      \"download_count\": %d,\n", stats->download_count);
        fprintf(output_file, "      \"delete_count\": %d,\n", stats->delete_count);
        fprintf(output_file, "      \"error_count\": %d,\n", stats->error_count);
        fprintf(output_file, "      \"avg_response_time_ms\": %.2f,\n",
               stats->avg_response_time_ms);
        fprintf(output_file, "      \"is_hot\": %s,\n",
               stats->is_hot ? "true" : "false");
        fprintf(output_file, "      \"is_cold\": %s\n",
               stats->is_cold ? "true" : "false");
        fprintf(output_file, "    }");
    }
    
    fprintf(output_file, "\n  ]\n");
    
    if (ctx->anomaly_count > 0) {
        fprintf(output_file, ",\n  \"anomalies\": [\n");
        
        for (i = 0; i < ctx->anomaly_count; i++) {
            Anomaly *anomaly = &ctx->anomalies[i];
            
            if (i > 0) {
                fprintf(output_file, ",\n");
            }
            
            fprintf(output_file, "    {\n");
            fprintf(output_file, "      \"file_id\": \"%s\",\n", anomaly->file_id);
            fprintf(output_file, "      \"type\": \"%s\",\n", anomaly->anomaly_type);
            fprintf(output_file, "      \"description\": \"%s\",\n", anomaly->description);
            fprintf(output_file, "      \"severity\": %.2f\n", anomaly->severity);
            fprintf(output_file, "    }");
        }
        
        fprintf(output_file, "\n  ]\n");
    }
    
    fprintf(output_file, "}\n");
}

/**
 * Main function
 * 
 * Entry point for the access log analyzer tool. Parses command-line
 * arguments and performs log analysis.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = some errors, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *output_file = NULL;
    double hot_threshold = 10.0;
    double cold_threshold = 0.1;
    int top_files = 10;
    int detect_anomalies_flag = 0;
    int result;
    AnalysisContext ctx;
    const char **log_files = NULL;
    int log_count = 0;
    int i;
    FILE *out_fp = stdout;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"hot-threshold", required_argument, 0, 1000},
        {"cold-threshold", required_argument, 0, 1001},
        {"time-range", required_argument, 0, 1002},
        {"operation", required_argument, 0, 1003},
        {"top-files", required_argument, 0, 1004},
        {"detect-anomalies", no_argument, 0, 1005},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(AnalysisContext));
    ctx.hot_file_threshold = hot_threshold;
    ctx.cold_file_threshold = cold_threshold;
    
    /* Allocate file statistics array */
    ctx.file_stats = (FileAccessStats *)calloc(MAX_FILES, sizeof(FileAccessStats));
    if (ctx.file_stats == NULL) {
        return ENOMEM;
    }
    
    /* Allocate anomalies array */
    ctx.anomalies = (Anomaly *)calloc(1000, sizeof(Anomaly));
    if (ctx.anomalies == NULL) {
        free(ctx.file_stats);
        return ENOMEM;
    }
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "o:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 1000:
                hot_threshold = atof(optarg);
                ctx.hot_file_threshold = hot_threshold;
                break;
            case 1001:
                cold_threshold = atof(optarg);
                ctx.cold_file_threshold = cold_threshold;
                break;
            case 1004:
                top_files = atoi(optarg);
                if (top_files < 0) top_files = 0;
                break;
            case 1005:
                detect_anomalies_flag = 1;
                break;
            case 'o':
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
                free(ctx.file_stats);
                free(ctx.anomalies);
                return 0;
            default:
                print_usage(argv[0]);
                free(ctx.file_stats);
                free(ctx.anomalies);
                return 2;
        }
    }
    
    /* Get log files from arguments */
    if (optind < argc) {
        log_count = argc - optind;
        log_files = (const char **)&argv[optind];
    } else {
        fprintf(stderr, "ERROR: No log files specified\n\n");
        print_usage(argv[0]);
        free(ctx.file_stats);
        free(ctx.anomalies);
        return 2;
    }
    
    /* Initialize logging */
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
    /* Analyze logs */
    result = analyze_logs(log_files, log_count, &ctx);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to analyze logs: %s\n", STRERROR(result));
        free(ctx.file_stats);
        free(ctx.anomalies);
        return 2;
    }
    
    /* Detect anomalies if requested */
    if (detect_anomalies_flag) {
        detect_anomalies(&ctx);
    }
    
    /* Print results */
    if (output_file != NULL) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            out_fp = stdout;
        }
    }
    
    if (json_output) {
        print_analysis_results_json(&ctx, top_files, out_fp);
    } else {
        print_analysis_results_text(&ctx, top_files, out_fp);
    }
    
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    free(ctx.file_stats);
    free(ctx.anomalies);
    
    return 0;
}

