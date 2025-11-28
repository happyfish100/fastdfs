/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_capacity_planner.c
* Capacity planning tool for FastDFS
* Helps plan storage capacity and predict growth
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <time.h>
#include <math.h>

#define MAX_PATH_LENGTH 256
#define MAX_STORE_PATHS 10
#define MAX_LINE_LENGTH 1024
#define GB_BYTES (1024ULL * 1024 * 1024)
#define TB_BYTES (1024ULL * GB_BYTES)
#define MB_BYTES (1024ULL * 1024)

typedef struct {
    char path[MAX_PATH_LENGTH];
    unsigned long long total_bytes;
    unsigned long long used_bytes;
    unsigned long long free_bytes;
    unsigned long long available_bytes;
    double usage_percent;
    unsigned long long file_count;
    unsigned long long dir_count;
} StoragePathInfo;

typedef struct {
    StoragePathInfo paths[MAX_STORE_PATHS];
    int path_count;
    unsigned long long total_capacity;
    unsigned long long total_used;
    unsigned long long total_free;
    double overall_usage;
} ClusterCapacity;

typedef struct {
    double daily_upload_gb;
    double daily_delete_gb;
    double net_growth_gb;
    int days_until_full;
    double recommended_capacity_gb;
} GrowthPrediction;

typedef struct {
    unsigned long long avg_file_size;
    unsigned long long total_files;
    unsigned long long small_files;   /* < 64KB */
    unsigned long long medium_files;  /* 64KB - 1MB */
    unsigned long long large_files;   /* > 1MB */
} FileDistribution;

/* Function prototypes */
static void print_usage(const char *program);
static int load_storage_paths(const char *config_file, char paths[][MAX_PATH_LENGTH], int *count);
static int get_path_capacity(const char *path, StoragePathInfo *info);
static int count_files_in_path(const char *path, unsigned long long *file_count, unsigned long long *dir_count);
static void analyze_cluster_capacity(ClusterCapacity *cluster);
static void predict_growth(ClusterCapacity *cluster, double daily_upload_gb, double daily_delete_gb, GrowthPrediction *prediction);
static void print_capacity_report(ClusterCapacity *cluster);
static void print_growth_prediction(GrowthPrediction *prediction, ClusterCapacity *cluster);
static void print_recommendations(ClusterCapacity *cluster, GrowthPrediction *prediction);
static const char *format_bytes(unsigned long long bytes, char *buffer, size_t size);
static const char *format_number(unsigned long long num, char *buffer, size_t size);
static void calculate_optimal_config(ClusterCapacity *cluster, double target_usage);

static void print_usage(const char *program)
{
    printf("FastDFS Capacity Planner v1.0\n");
    printf("Plan storage capacity and predict growth\n\n");
    printf("Usage: %s [options]\n\n", program);
    printf("Options:\n");
    printf("  -c <file>      Storage config file (storage.conf)\n");
    printf("  -p <path>      Add storage path manually (can be used multiple times)\n");
    printf("  -u <GB/day>    Expected daily upload volume in GB (default: 10)\n");
    printf("  -d <GB/day>    Expected daily delete volume in GB (default: 1)\n");
    printf("  -t <percent>   Target usage percentage (default: 80)\n");
    printf("  -r             Show detailed recommendations\n");
    printf("  -v             Verbose output\n");
    printf("  -h             Show this help\n\n");
    printf("Examples:\n");
    printf("  %s -c /etc/fdfs/storage.conf\n", program);
    printf("  %s -p /data/fastdfs -u 50 -d 5\n", program);
    printf("  %s -c /etc/fdfs/storage.conf -t 70 -r\n", program);
}

static const char *format_bytes(unsigned long long bytes, char *buffer, size_t size)
{
    if (bytes >= TB_BYTES) {
        snprintf(buffer, size, "%.2f TB", (double)bytes / TB_BYTES);
    } else if (bytes >= GB_BYTES) {
        snprintf(buffer, size, "%.2f GB", (double)bytes / GB_BYTES);
    } else if (bytes >= MB_BYTES) {
        snprintf(buffer, size, "%.2f MB", (double)bytes / MB_BYTES);
    } else if (bytes >= 1024) {
        snprintf(buffer, size, "%.2f KB", (double)bytes / 1024);
    } else {
        snprintf(buffer, size, "%llu B", bytes);
    }
    return buffer;
}

static const char *format_number(unsigned long long num, char *buffer, size_t size)
{
    if (num >= 1000000000ULL) {
        snprintf(buffer, size, "%.2fB", (double)num / 1000000000);
    } else if (num >= 1000000ULL) {
        snprintf(buffer, size, "%.2fM", (double)num / 1000000);
    } else if (num >= 1000ULL) {
        snprintf(buffer, size, "%.2fK", (double)num / 1000);
    } else {
        snprintf(buffer, size, "%llu", num);
    }
    return buffer;
}

static int load_storage_paths(const char *config_file, char paths[][MAX_PATH_LENGTH], int *count)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *key, *value, *eq_pos;
    int store_path_count = 1;
    int i;
    
    *count = 0;
    
    fp = fopen(config_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "Cannot open config file: %s\n", config_file);
        return -1;
    }
    
    /* First pass: get store_path_count */
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n') continue;
        
        eq_pos = strchr(trimmed, '=');
        if (eq_pos == NULL) continue;
        
        *eq_pos = '\0';
        key = trimmed;
        value = eq_pos + 1;
        
        /* Trim */
        while (*key && (*key == ' ' || *key == '\t')) key++;
        char *end = key + strlen(key) - 1;
        while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';
        
        while (*value && (*value == ' ' || *value == '\t')) value++;
        end = value + strlen(value) - 1;
        while (end > value && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
        
        if (strcmp(key, "store_path_count") == 0) {
            store_path_count = atoi(value);
            if (store_path_count > MAX_STORE_PATHS) {
                store_path_count = MAX_STORE_PATHS;
            }
            break;
        }
    }
    
    /* Second pass: get store paths */
    rewind(fp);
    while (fgets(line, sizeof(line), fp) != NULL && *count < store_path_count) {
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n') continue;
        
        eq_pos = strchr(trimmed, '=');
        if (eq_pos == NULL) continue;
        
        *eq_pos = '\0';
        key = trimmed;
        value = eq_pos + 1;
        
        /* Trim */
        while (*key && (*key == ' ' || *key == '\t')) key++;
        char *end = key + strlen(key) - 1;
        while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';
        
        while (*value && (*value == ' ' || *value == '\t')) value++;
        end = value + strlen(value) - 1;
        while (end > value && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
        
        /* Check for store_path0, store_path1, etc. */
        if (strncmp(key, "store_path", 10) == 0) {
            for (i = 0; i < store_path_count; i++) {
                char expected_key[32];
                snprintf(expected_key, sizeof(expected_key), "store_path%d", i);
                if (strcmp(key, expected_key) == 0) {
                    strncpy(paths[*count], value, MAX_PATH_LENGTH - 1);
                    (*count)++;
                    break;
                }
            }
        }
    }
    
    fclose(fp);
    return *count > 0 ? 0 : -1;
}

static int get_path_capacity(const char *path, StoragePathInfo *info)
{
    struct statvfs stat;
    
    memset(info, 0, sizeof(StoragePathInfo));
    strncpy(info->path, path, MAX_PATH_LENGTH - 1);
    
    if (statvfs(path, &stat) != 0) {
        fprintf(stderr, "Cannot get filesystem info for %s: %s\n", path, strerror(errno));
        return -1;
    }
    
    info->total_bytes = (unsigned long long)stat.f_blocks * stat.f_frsize;
    info->free_bytes = (unsigned long long)stat.f_bfree * stat.f_frsize;
    info->available_bytes = (unsigned long long)stat.f_bavail * stat.f_frsize;
    info->used_bytes = info->total_bytes - info->free_bytes;
    
    if (info->total_bytes > 0) {
        info->usage_percent = (double)info->used_bytes / info->total_bytes * 100.0;
    }
    
    return 0;
}

static int count_files_recursive(const char *path, unsigned long long *file_count, unsigned long long *dir_count, int depth)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH_LENGTH * 2];
    
    if (depth > 5) return 0;  /* Limit recursion depth */
    
    dir = opendir(path);
    if (dir == NULL) return -1;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (lstat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                (*dir_count)++;
                count_files_recursive(full_path, file_count, dir_count, depth + 1);
            } else if (S_ISREG(st.st_mode)) {
                (*file_count)++;
            }
        }
    }
    
    closedir(dir);
    return 0;
}

static int count_files_in_path(const char *path, unsigned long long *file_count, unsigned long long *dir_count)
{
    char data_path[MAX_PATH_LENGTH];
    
    *file_count = 0;
    *dir_count = 0;
    
    /* FastDFS stores files in data subdirectory */
    snprintf(data_path, sizeof(data_path), "%s/data", path);
    
    if (access(data_path, R_OK) == 0) {
        count_files_recursive(data_path, file_count, dir_count, 0);
    } else {
        count_files_recursive(path, file_count, dir_count, 0);
    }
    
    return 0;
}

static void analyze_cluster_capacity(ClusterCapacity *cluster)
{
    int i;
    
    cluster->total_capacity = 0;
    cluster->total_used = 0;
    cluster->total_free = 0;
    
    for (i = 0; i < cluster->path_count; i++) {
        cluster->total_capacity += cluster->paths[i].total_bytes;
        cluster->total_used += cluster->paths[i].used_bytes;
        cluster->total_free += cluster->paths[i].available_bytes;
    }
    
    if (cluster->total_capacity > 0) {
        cluster->overall_usage = (double)cluster->total_used / cluster->total_capacity * 100.0;
    }
}

static void predict_growth(ClusterCapacity *cluster, double daily_upload_gb, double daily_delete_gb, GrowthPrediction *prediction)
{
    double free_gb;
    
    prediction->daily_upload_gb = daily_upload_gb;
    prediction->daily_delete_gb = daily_delete_gb;
    prediction->net_growth_gb = daily_upload_gb - daily_delete_gb;
    
    free_gb = (double)cluster->total_free / GB_BYTES;
    
    if (prediction->net_growth_gb > 0) {
        prediction->days_until_full = (int)(free_gb / prediction->net_growth_gb);
    } else {
        prediction->days_until_full = -1;  /* Not growing */
    }
    
    /* Recommend capacity for 1 year at current growth rate */
    prediction->recommended_capacity_gb = (double)cluster->total_used / GB_BYTES + 
                                          (prediction->net_growth_gb * 365);
}

static void print_capacity_report(ClusterCapacity *cluster)
{
    int i;
    char buf1[64], buf2[64], buf3[64], buf4[64];
    const char *color;
    
    printf("\n");
    printf("================================================================================\n");
    printf("                        FastDFS Capacity Report\n");
    printf("================================================================================\n\n");
    
    printf("Storage Paths:\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("%-40s %12s %12s %12s %8s\n", "Path", "Total", "Used", "Free", "Usage");
    printf("--------------------------------------------------------------------------------\n");
    
    for (i = 0; i < cluster->path_count; i++) {
        StoragePathInfo *p = &cluster->paths[i];
        
        if (p->usage_percent >= 90) {
            color = "\033[31m";  /* Red */
        } else if (p->usage_percent >= 80) {
            color = "\033[33m";  /* Yellow */
        } else {
            color = "\033[32m";  /* Green */
        }
        
        printf("%-40s %12s %12s %12s %s%7.1f%%\033[0m\n",
            p->path,
            format_bytes(p->total_bytes, buf1, sizeof(buf1)),
            format_bytes(p->used_bytes, buf2, sizeof(buf2)),
            format_bytes(p->available_bytes, buf3, sizeof(buf3)),
            color,
            p->usage_percent);
        
        if (p->file_count > 0) {
            printf("  Files: %s, Directories: %s\n",
                format_number(p->file_count, buf1, sizeof(buf1)),
                format_number(p->dir_count, buf2, sizeof(buf2)));
        }
    }
    
    printf("--------------------------------------------------------------------------------\n");
    
    /* Overall summary */
    if (cluster->overall_usage >= 90) {
        color = "\033[31m";
    } else if (cluster->overall_usage >= 80) {
        color = "\033[33m";
    } else {
        color = "\033[32m";
    }
    
    printf("%-40s %12s %12s %12s %s%7.1f%%\033[0m\n",
        "TOTAL",
        format_bytes(cluster->total_capacity, buf1, sizeof(buf1)),
        format_bytes(cluster->total_used, buf2, sizeof(buf2)),
        format_bytes(cluster->total_free, buf3, sizeof(buf3)),
        color,
        cluster->overall_usage);
    printf("================================================================================\n");
}

static void print_growth_prediction(GrowthPrediction *prediction, ClusterCapacity *cluster)
{
    char buf[64];
    const char *color;
    
    printf("\n");
    printf("================================================================================\n");
    printf("                        Growth Prediction\n");
    printf("================================================================================\n\n");
    
    printf("Daily Upload:     %.2f GB/day\n", prediction->daily_upload_gb);
    printf("Daily Delete:     %.2f GB/day\n", prediction->daily_delete_gb);
    printf("Net Growth:       %.2f GB/day (%.2f GB/month, %.2f TB/year)\n",
        prediction->net_growth_gb,
        prediction->net_growth_gb * 30,
        prediction->net_growth_gb * 365 / 1024);
    
    printf("\n");
    
    if (prediction->days_until_full > 0) {
        if (prediction->days_until_full < 30) {
            color = "\033[31m";  /* Red - critical */
        } else if (prediction->days_until_full < 90) {
            color = "\033[33m";  /* Yellow - warning */
        } else {
            color = "\033[32m";  /* Green - OK */
        }
        
        printf("Time Until Full:  %s%d days (%.1f months)\033[0m\n",
            color,
            prediction->days_until_full,
            prediction->days_until_full / 30.0);
        
        if (prediction->days_until_full < 30) {
            printf("\n\033[31m*** CRITICAL: Storage will be full in less than 30 days! ***\033[0m\n");
        } else if (prediction->days_until_full < 90) {
            printf("\n\033[33m*** WARNING: Storage will be full in less than 90 days! ***\033[0m\n");
        }
    } else {
        printf("Time Until Full:  \033[32mN/A (not growing or shrinking)\033[0m\n");
    }
    
    printf("\nCapacity Planning:\n");
    printf("  Current Used:      %s\n", format_bytes(cluster->total_used, buf, sizeof(buf)));
    printf("  Recommended (1yr): %.2f TB\n", prediction->recommended_capacity_gb / 1024);
    
    printf("================================================================================\n");
}

static void calculate_optimal_config(ClusterCapacity *cluster, double target_usage)
{
    char buf[64];
    double current_usage = cluster->overall_usage;
    unsigned long long optimal_capacity;
    unsigned long long additional_needed;
    
    printf("\n");
    printf("================================================================================\n");
    printf("                        Optimal Configuration\n");
    printf("================================================================================\n\n");
    
    printf("Target Usage:     %.0f%%\n", target_usage);
    printf("Current Usage:    %.1f%%\n", current_usage);
    
    if (current_usage > target_usage) {
        /* Need more capacity */
        optimal_capacity = (unsigned long long)((double)cluster->total_used / (target_usage / 100.0));
        additional_needed = optimal_capacity - cluster->total_capacity;
        
        printf("\n\033[33mAction Required: Add more storage capacity\033[0m\n");
        printf("  Current Capacity:   %s\n", format_bytes(cluster->total_capacity, buf, sizeof(buf)));
        printf("  Optimal Capacity:   %s\n", format_bytes(optimal_capacity, buf, sizeof(buf)));
        printf("  Additional Needed:  %s\n", format_bytes(additional_needed, buf, sizeof(buf)));
        
        /* Suggest number of disks */
        unsigned long long disk_sizes[] = {500ULL * GB_BYTES, 1ULL * TB_BYTES, 2ULL * TB_BYTES, 4ULL * TB_BYTES, 8ULL * TB_BYTES};
        const char *disk_names[] = {"500GB", "1TB", "2TB", "4TB", "8TB"};
        int i;
        
        printf("\n  Disk Options:\n");
        for (i = 0; i < 5; i++) {
            int num_disks = (int)ceil((double)additional_needed / disk_sizes[i]);
            if (num_disks > 0 && num_disks <= 100) {
                printf("    - %d x %s disks\n", num_disks, disk_names[i]);
            }
        }
    } else {
        printf("\n\033[32mCapacity is within target range.\033[0m\n");
        
        /* Calculate headroom */
        unsigned long long headroom = cluster->total_free - 
            (unsigned long long)(cluster->total_capacity * (1.0 - target_usage / 100.0));
        printf("  Available Headroom: %s\n", format_bytes(headroom, buf, sizeof(buf)));
    }
    
    printf("================================================================================\n");
}

static void print_recommendations(ClusterCapacity *cluster, GrowthPrediction *prediction)
{
    printf("\n");
    printf("================================================================================\n");
    printf("                        Recommendations\n");
    printf("================================================================================\n\n");
    
    int rec_num = 1;
    
    /* Usage-based recommendations */
    if (cluster->overall_usage >= 90) {
        printf("%d. \033[31m[CRITICAL]\033[0m Storage usage is above 90%%!\n", rec_num++);
        printf("   - Add storage capacity immediately\n");
        printf("   - Consider enabling file deduplication\n");
        printf("   - Review and delete unnecessary files\n\n");
    } else if (cluster->overall_usage >= 80) {
        printf("%d. \033[33m[WARNING]\033[0m Storage usage is above 80%%\n", rec_num++);
        printf("   - Plan for capacity expansion\n");
        printf("   - Monitor growth rate closely\n\n");
    }
    
    /* Growth-based recommendations */
    if (prediction->days_until_full > 0 && prediction->days_until_full < 90) {
        printf("%d. \033[33m[WARNING]\033[0m Storage will be full in %d days\n", rec_num++, prediction->days_until_full);
        printf("   - Order additional storage now\n");
        printf("   - Consider archiving old data\n\n");
    }
    
    /* Path balance recommendations */
    if (cluster->path_count > 1) {
        double max_usage = 0, min_usage = 100;
        int i;
        for (i = 0; i < cluster->path_count; i++) {
            if (cluster->paths[i].usage_percent > max_usage) {
                max_usage = cluster->paths[i].usage_percent;
            }
            if (cluster->paths[i].usage_percent < min_usage) {
                min_usage = cluster->paths[i].usage_percent;
            }
        }
        
        if (max_usage - min_usage > 20) {
            printf("%d. \033[33m[INFO]\033[0m Storage paths are unbalanced (%.1f%% difference)\n", 
                rec_num++, max_usage - min_usage);
            printf("   - Consider running fdfs_rebalance tool\n");
            printf("   - Check file distribution settings\n\n");
        }
    }
    
    /* Performance recommendations */
    if (cluster->total_capacity > 10ULL * TB_BYTES) {
        printf("%d. \033[32m[TIP]\033[0m Large cluster detected\n", rec_num++);
        printf("   - Ensure disk_rw_separated = true\n");
        printf("   - Increase work_threads based on CPU cores\n");
        printf("   - Consider SSD for metadata storage\n\n");
    }
    
    /* General best practices */
    printf("%d. \033[32m[BEST PRACTICE]\033[0m General recommendations:\n", rec_num++);
    printf("   - Keep storage usage below 80%% for optimal performance\n");
    printf("   - Monitor disk I/O and network throughput\n");
    printf("   - Regular backup of tracker data\n");
    printf("   - Use connection pooling for clients\n");
    
    printf("\n================================================================================\n");
}

int main(int argc, char *argv[])
{
    int opt;
    const char *config_file = NULL;
    char manual_paths[MAX_STORE_PATHS][MAX_PATH_LENGTH];
    int manual_path_count = 0;
    double daily_upload_gb = 10.0;
    double daily_delete_gb = 1.0;
    double target_usage = 80.0;
    int show_recommendations = 0;
    int verbose = 0;
    ClusterCapacity cluster;
    GrowthPrediction prediction;
    int i;
    
    memset(&cluster, 0, sizeof(cluster));
    memset(&prediction, 0, sizeof(prediction));
    
    while ((opt = getopt(argc, argv, "c:p:u:d:t:rvh")) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'p':
                if (manual_path_count < MAX_STORE_PATHS) {
                    strncpy(manual_paths[manual_path_count], optarg, MAX_PATH_LENGTH - 1);
                    manual_path_count++;
                }
                break;
            case 'u':
                daily_upload_gb = atof(optarg);
                break;
            case 'd':
                daily_delete_gb = atof(optarg);
                break;
            case 't':
                target_usage = atof(optarg);
                if (target_usage < 50) target_usage = 50;
                if (target_usage > 95) target_usage = 95;
                break;
            case 'r':
                show_recommendations = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }
    
    /* Load paths from config file */
    if (config_file != NULL) {
        char config_paths[MAX_STORE_PATHS][MAX_PATH_LENGTH];
        int config_path_count = 0;
        
        if (load_storage_paths(config_file, config_paths, &config_path_count) == 0) {
            for (i = 0; i < config_path_count && cluster.path_count < MAX_STORE_PATHS; i++) {
                strncpy(cluster.paths[cluster.path_count].path, config_paths[i], MAX_PATH_LENGTH - 1);
                cluster.path_count++;
            }
            if (verbose) {
                printf("Loaded %d paths from %s\n", config_path_count, config_file);
            }
        }
    }
    
    /* Add manual paths */
    for (i = 0; i < manual_path_count && cluster.path_count < MAX_STORE_PATHS; i++) {
        strncpy(cluster.paths[cluster.path_count].path, manual_paths[i], MAX_PATH_LENGTH - 1);
        cluster.path_count++;
    }
    
    if (cluster.path_count == 0) {
        fprintf(stderr, "No storage paths specified.\n");
        fprintf(stderr, "Use -c <config_file> or -p <path> to specify storage paths.\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Get capacity info for each path */
    printf("FastDFS Capacity Planner\n");
    printf("Analyzing %d storage path(s)...\n", cluster.path_count);
    
    for (i = 0; i < cluster.path_count; i++) {
        if (get_path_capacity(cluster.paths[i].path, &cluster.paths[i]) != 0) {
            fprintf(stderr, "Warning: Could not analyze path: %s\n", cluster.paths[i].path);
        }
        
        if (verbose) {
            printf("  Counting files in %s...\n", cluster.paths[i].path);
        }
        count_files_in_path(cluster.paths[i].path, 
            &cluster.paths[i].file_count, 
            &cluster.paths[i].dir_count);
    }
    
    /* Analyze cluster */
    analyze_cluster_capacity(&cluster);
    
    /* Predict growth */
    predict_growth(&cluster, daily_upload_gb, daily_delete_gb, &prediction);
    
    /* Print reports */
    print_capacity_report(&cluster);
    print_growth_prediction(&prediction, &cluster);
    calculate_optimal_config(&cluster, target_usage);
    
    if (show_recommendations) {
        print_recommendations(&cluster, &prediction);
    }
    
    return 0;
}
