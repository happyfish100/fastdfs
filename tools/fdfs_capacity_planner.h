/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_capacity_planner.h
* Header file for FastDFS capacity planning utilities
*/

#ifndef FDFS_CAPACITY_PLANNER_H
#define FDFS_CAPACITY_PLANNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum limits */
#define CP_MAX_STORE_PATHS 10
#define CP_MAX_PATH_LENGTH 256
#define CP_MAX_GROUPS 32
#define CP_MAX_SERVERS 64
#define CP_MAX_HISTORY 365
#define CP_MAX_MESSAGE 512

/* Size constants */
#define CP_KB_BYTES (1024ULL)
#define CP_MB_BYTES (1024ULL * CP_KB_BYTES)
#define CP_GB_BYTES (1024ULL * CP_MB_BYTES)
#define CP_TB_BYTES (1024ULL * CP_GB_BYTES)
#define CP_PB_BYTES (1024ULL * CP_TB_BYTES)

/* Threshold defaults */
#define CP_DEFAULT_WARNING_PERCENT 80.0
#define CP_DEFAULT_CRITICAL_PERCENT 90.0
#define CP_DEFAULT_RESERVED_PERCENT 10.0

/* Prediction models */
#define CP_MODEL_LINEAR 1
#define CP_MODEL_EXPONENTIAL 2
#define CP_MODEL_POLYNOMIAL 3

/* Report formats */
#define CP_FORMAT_TEXT 0
#define CP_FORMAT_JSON 1
#define CP_FORMAT_HTML 2
#define CP_FORMAT_CSV 3

/* Alert levels */
#define CP_LEVEL_OK 0
#define CP_LEVEL_INFO 1
#define CP_LEVEL_WARNING 2
#define CP_LEVEL_CRITICAL 3

/**
 * Storage path information structure
 */
typedef struct cp_storage_path {
    char path[CP_MAX_PATH_LENGTH];
    unsigned long long total_bytes;
    unsigned long long used_bytes;
    unsigned long long free_bytes;
    unsigned long long available_bytes;
    double usage_percent;
    unsigned long long file_count;
    unsigned long long dir_count;
    time_t last_updated;
} CPStoragePath;

/**
 * Usage history sample structure
 */
typedef struct cp_usage_sample {
    time_t timestamp;
    unsigned long long used_bytes;
    unsigned long long total_bytes;
    double usage_percent;
    unsigned long long file_count;
} CPUsageSample;

/**
 * Growth statistics structure
 */
typedef struct cp_growth_stats {
    double daily_growth_bytes;
    double weekly_growth_bytes;
    double monthly_growth_bytes;
    double daily_growth_percent;
    double weekly_growth_percent;
    double monthly_growth_percent;
    double avg_file_size;
    unsigned long long files_per_day;
    int samples_count;
} CPGrowthStats;

/**
 * Capacity prediction structure
 */
typedef struct cp_prediction {
    time_t prediction_date;
    unsigned long long predicted_used;
    unsigned long long predicted_free;
    double predicted_usage_percent;
    double confidence;
    int days_until_warning;
    int days_until_critical;
    int days_until_full;
} CPPrediction;

/**
 * Storage group information structure
 */
typedef struct cp_group_info {
    char group_name[64];
    CPStoragePath paths[CP_MAX_STORE_PATHS];
    int path_count;
    unsigned long long total_capacity;
    unsigned long long total_used;
    unsigned long long total_free;
    double usage_percent;
    int server_count;
} CPGroupInfo;

/**
 * Cluster capacity structure
 */
typedef struct cp_cluster_capacity {
    CPGroupInfo groups[CP_MAX_GROUPS];
    int group_count;
    unsigned long long total_capacity;
    unsigned long long total_used;
    unsigned long long total_free;
    double usage_percent;
    int total_servers;
    time_t last_updated;
} CPClusterCapacity;

/**
 * Capacity report structure
 */
typedef struct cp_capacity_report {
    CPClusterCapacity cluster;
    CPGrowthStats growth;
    CPPrediction predictions[30];
    int prediction_count;
    int alert_level;
    char alert_message[CP_MAX_MESSAGE];
    time_t report_time;
} CPCapacityReport;

/**
 * Planning context structure
 */
typedef struct cp_planning_context {
    CPClusterCapacity *cluster;
    CPUsageSample history[CP_MAX_HISTORY];
    int history_count;
    double warning_threshold;
    double critical_threshold;
    double reserved_percent;
    int prediction_model;
    int verbose;
} CPPlanningContext;

/* ============================================================
 * Storage Path Functions
 * ============================================================ */

/**
 * Initialize storage path structure
 * @param path Pointer to storage path
 */
void cp_path_init(CPStoragePath *path);

/**
 * Get storage path information
 * @param path_str Path string
 * @param path Pointer to storage path structure
 * @return 0 on success, -1 on error
 */
int cp_path_get_info(const char *path_str, CPStoragePath *path);

/**
 * Count files in path
 * @param path Path to count
 * @param file_count Output file count
 * @param dir_count Output directory count
 * @return 0 on success, -1 on error
 */
int cp_path_count_files(const char *path, unsigned long long *file_count,
                        unsigned long long *dir_count);

/**
 * Get path usage percentage
 * @param path Pointer to storage path
 * @return Usage percentage
 */
double cp_path_get_usage(CPStoragePath *path);

/**
 * Check if path is healthy
 * @param path Pointer to storage path
 * @param warning_threshold Warning threshold percentage
 * @param critical_threshold Critical threshold percentage
 * @return Alert level
 */
int cp_path_check_health(CPStoragePath *path, double warning_threshold,
                         double critical_threshold);

/* ============================================================
 * Group Functions
 * ============================================================ */

/**
 * Initialize group info structure
 * @param group Pointer to group info
 */
void cp_group_init(CPGroupInfo *group);

/**
 * Add path to group
 * @param group Pointer to group info
 * @param path Pointer to storage path
 * @return 0 on success, -1 if full
 */
int cp_group_add_path(CPGroupInfo *group, CPStoragePath *path);

/**
 * Calculate group totals
 * @param group Pointer to group info
 */
void cp_group_calculate_totals(CPGroupInfo *group);

/**
 * Get group usage percentage
 * @param group Pointer to group info
 * @return Usage percentage
 */
double cp_group_get_usage(CPGroupInfo *group);

/* ============================================================
 * Cluster Functions
 * ============================================================ */

/**
 * Initialize cluster capacity structure
 * @param cluster Pointer to cluster capacity
 */
void cp_cluster_init(CPClusterCapacity *cluster);

/**
 * Add group to cluster
 * @param cluster Pointer to cluster capacity
 * @param group Pointer to group info
 * @return 0 on success, -1 if full
 */
int cp_cluster_add_group(CPClusterCapacity *cluster, CPGroupInfo *group);

/**
 * Calculate cluster totals
 * @param cluster Pointer to cluster capacity
 */
void cp_cluster_calculate_totals(CPClusterCapacity *cluster);

/**
 * Load cluster from config
 * @param cluster Pointer to cluster capacity
 * @param config_file Config file path
 * @return 0 on success, -1 on error
 */
int cp_cluster_load_config(CPClusterCapacity *cluster, const char *config_file);

/**
 * Refresh cluster information
 * @param cluster Pointer to cluster capacity
 * @return 0 on success, -1 on error
 */
int cp_cluster_refresh(CPClusterCapacity *cluster);

/* ============================================================
 * History Functions
 * ============================================================ */

/**
 * Add usage sample to history
 * @param ctx Pointer to planning context
 * @param sample Pointer to usage sample
 * @return 0 on success, -1 if full
 */
int cp_history_add_sample(CPPlanningContext *ctx, CPUsageSample *sample);

/**
 * Load history from file
 * @param ctx Pointer to planning context
 * @param filename History file path
 * @return Number of samples loaded, -1 on error
 */
int cp_history_load(CPPlanningContext *ctx, const char *filename);

/**
 * Save history to file
 * @param ctx Pointer to planning context
 * @param filename History file path
 * @return 0 on success, -1 on error
 */
int cp_history_save(CPPlanningContext *ctx, const char *filename);

/**
 * Clear history
 * @param ctx Pointer to planning context
 */
void cp_history_clear(CPPlanningContext *ctx);

/* ============================================================
 * Growth Analysis Functions
 * ============================================================ */

/**
 * Calculate growth statistics
 * @param ctx Pointer to planning context
 * @param stats Pointer to growth stats output
 * @return 0 on success, -1 on error
 */
int cp_calculate_growth(CPPlanningContext *ctx, CPGrowthStats *stats);

/**
 * Calculate daily growth rate
 * @param ctx Pointer to planning context
 * @return Daily growth in bytes
 */
double cp_get_daily_growth(CPPlanningContext *ctx);

/**
 * Calculate average file size
 * @param ctx Pointer to planning context
 * @return Average file size in bytes
 */
double cp_get_avg_file_size(CPPlanningContext *ctx);

/**
 * Calculate files per day
 * @param ctx Pointer to planning context
 * @return Files uploaded per day
 */
unsigned long long cp_get_files_per_day(CPPlanningContext *ctx);

/* ============================================================
 * Prediction Functions
 * ============================================================ */

/**
 * Predict capacity at future date
 * @param ctx Pointer to planning context
 * @param days_ahead Days in the future
 * @param prediction Pointer to prediction output
 * @return 0 on success, -1 on error
 */
int cp_predict_capacity(CPPlanningContext *ctx, int days_ahead,
                        CPPrediction *prediction);

/**
 * Predict days until threshold
 * @param ctx Pointer to planning context
 * @param threshold_percent Threshold percentage
 * @return Days until threshold, -1 if never
 */
int cp_predict_days_until(CPPlanningContext *ctx, double threshold_percent);

/**
 * Generate predictions for next N days
 * @param ctx Pointer to planning context
 * @param predictions Array of predictions
 * @param max_days Maximum days to predict
 * @return Number of predictions generated
 */
int cp_generate_predictions(CPPlanningContext *ctx, CPPrediction *predictions,
                            int max_days);

/**
 * Set prediction model
 * @param ctx Pointer to planning context
 * @param model Model type constant
 */
void cp_set_prediction_model(CPPlanningContext *ctx, int model);

/* ============================================================
 * Report Functions
 * ============================================================ */

/**
 * Generate capacity report
 * @param ctx Pointer to planning context
 * @param report Pointer to report output
 * @return 0 on success, -1 on error
 */
int cp_generate_report(CPPlanningContext *ctx, CPCapacityReport *report);

/**
 * Print report to stdout
 * @param report Pointer to report
 * @param format Output format
 * @param verbose Include detailed information
 */
void cp_print_report(CPCapacityReport *report, int format, int verbose);

/**
 * Export report to file
 * @param report Pointer to report
 * @param filename Output filename
 * @param format Output format
 * @return 0 on success, -1 on error
 */
int cp_export_report(CPCapacityReport *report, const char *filename, int format);

/**
 * Get report summary
 * @param report Pointer to report
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cp_get_report_summary(CPCapacityReport *report, char *buffer, size_t buffer_size);

/* ============================================================
 * Planning Context Functions
 * ============================================================ */

/**
 * Initialize planning context
 * @param ctx Pointer to planning context
 * @param cluster Pointer to cluster capacity
 */
void cp_context_init(CPPlanningContext *ctx, CPClusterCapacity *cluster);

/**
 * Set warning threshold
 * @param ctx Pointer to planning context
 * @param threshold Threshold percentage
 */
void cp_context_set_warning(CPPlanningContext *ctx, double threshold);

/**
 * Set critical threshold
 * @param ctx Pointer to planning context
 * @param threshold Threshold percentage
 */
void cp_context_set_critical(CPPlanningContext *ctx, double threshold);

/**
 * Set reserved percentage
 * @param ctx Pointer to planning context
 * @param percent Reserved percentage
 */
void cp_context_set_reserved(CPPlanningContext *ctx, double percent);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Format bytes for display
 * @param bytes Size in bytes
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cp_format_bytes(unsigned long long bytes, char *buffer, size_t buffer_size);

/**
 * Format percentage for display
 * @param percent Percentage value
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cp_format_percent(double percent, char *buffer, size_t buffer_size);

/**
 * Format time for display
 * @param timestamp Unix timestamp
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cp_format_time(time_t timestamp, char *buffer, size_t buffer_size);

/**
 * Format duration for display
 * @param days Number of days
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void cp_format_duration(int days, char *buffer, size_t buffer_size);

/**
 * Get alert level name
 * @param level Alert level
 * @return Level name string
 */
const char *cp_get_level_name(int level);

/**
 * Get alert level color (ANSI)
 * @param level Alert level
 * @return ANSI color code string
 */
const char *cp_get_level_color(int level);

/**
 * Parse size string (e.g., "1TB", "500GB")
 * @param str Size string
 * @return Size in bytes
 */
unsigned long long cp_parse_size(const char *str);

/**
 * Calculate linear regression
 * @param x Array of x values
 * @param y Array of y values
 * @param n Number of points
 * @param slope Output slope
 * @param intercept Output intercept
 * @return 0 on success, -1 on error
 */
int cp_linear_regression(double *x, double *y, int n, double *slope, double *intercept);

/**
 * Calculate standard deviation
 * @param values Array of values
 * @param n Number of values
 * @return Standard deviation
 */
double cp_std_deviation(double *values, int n);

#ifdef __cplusplus
}
#endif

#endif /* FDFS_CAPACITY_PLANNER_H */
