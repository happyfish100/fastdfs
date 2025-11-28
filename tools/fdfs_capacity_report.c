/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_capacity_report.c
* Capacity reporting tool for FastDFS
* Generates detailed capacity reports in various formats
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
#include <getopt.h>

#define MAX_PATH_LENGTH 256
#define MAX_STORE_PATHS 10
#define MAX_GROUPS 32
#define MAX_LINE_LENGTH 1024
#define GB_BYTES (1024ULL * 1024 * 1024)
#define TB_BYTES (1024ULL * GB_BYTES)
#define MB_BYTES (1024ULL * 1024)

/* Report formats */
#define FORMAT_TEXT 0
#define FORMAT_JSON 1
#define FORMAT_HTML 2
#define FORMAT_CSV 3
#define FORMAT_MARKDOWN 4

/* Alert levels */
#define LEVEL_OK 0
#define LEVEL_WARNING 1
#define LEVEL_CRITICAL 2

typedef struct {
    char path[MAX_PATH_LENGTH];
    unsigned long long total_bytes;
    unsigned long long used_bytes;
    unsigned long long free_bytes;
    double usage_percent;
    unsigned long long file_count;
} StoragePathInfo;

typedef struct {
    char group_name[64];
    StoragePathInfo paths[MAX_STORE_PATHS];
    int path_count;
    unsigned long long total_capacity;
    unsigned long long total_used;
    unsigned long long total_free;
    double usage_percent;
} GroupInfo;

typedef struct {
    GroupInfo groups[MAX_GROUPS];
    int group_count;
    unsigned long long total_capacity;
    unsigned long long total_used;
    unsigned long long total_free;
    double usage_percent;
    time_t report_time;
} ClusterReport;

typedef struct {
    int format;
    int verbose;
    double warning_threshold;
    double critical_threshold;
    char output_file[MAX_PATH_LENGTH];
    char config_file[MAX_PATH_LENGTH];
    int show_paths;
    int show_predictions;
} ReportOptions;

/* Function prototypes */
static void print_usage(const char *program);
static int get_path_info(const char *path, StoragePathInfo *info);
static void format_bytes(unsigned long long bytes, char *buffer, size_t size);
static int get_alert_level(double usage, double warning, double critical);
static const char *get_level_name(int level);
static const char *get_level_color(int level);
static void print_report_text(ClusterReport *report, ReportOptions *options);
static void print_report_json(ClusterReport *report, ReportOptions *options);
static void print_report_html(ClusterReport *report, ReportOptions *options);
static void print_report_csv(ClusterReport *report, ReportOptions *options);
static void print_report_markdown(ClusterReport *report, ReportOptions *options);
static int load_cluster_config(ClusterReport *report, const char *config_file);
static unsigned long long count_files(const char *path);

static void print_usage(const char *program)
{
    printf("FastDFS Capacity Report Generator v1.0\n");
    printf("Generates detailed capacity reports for FastDFS clusters\n\n");
    printf("Usage: %s [options] [config_file]\n", program);
    printf("Options:\n");
    printf("  -f, --format <fmt>      Output format: text, json, html, csv, markdown\n");
    printf("  -o, --output <file>     Output file (default: stdout)\n");
    printf("  -w, --warning <pct>     Warning threshold percentage (default: 80)\n");
    printf("  -c, --critical <pct>    Critical threshold percentage (default: 90)\n");
    printf("  -p, --paths             Show individual path details\n");
    printf("  -P, --predictions       Show capacity predictions\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -h, --help              Show this help\n\n");
    printf("Examples:\n");
    printf("  %s -f html -o report.html cluster.conf\n", program);
    printf("  %s -f json -p -P cluster.conf\n", program);
}

static void format_bytes(unsigned long long bytes, char *buffer, size_t size)
{
    if (bytes >= TB_BYTES) {
        snprintf(buffer, size, "%.2f TB", (double)bytes / TB_BYTES);
    } else if (bytes >= GB_BYTES) {
        snprintf(buffer, size, "%.2f GB", (double)bytes / GB_BYTES);
    } else if (bytes >= MB_BYTES) {
        snprintf(buffer, size, "%.2f MB", (double)bytes / MB_BYTES);
    } else {
        snprintf(buffer, size, "%llu bytes", bytes);
    }
}

static int get_path_info(const char *path, StoragePathInfo *info)
{
    struct statvfs stat;
    
    memset(info, 0, sizeof(StoragePathInfo));
    strncpy(info->path, path, MAX_PATH_LENGTH - 1);
    
    if (statvfs(path, &stat) != 0) {
        return -1;
    }
    
    info->total_bytes = (unsigned long long)stat.f_blocks * stat.f_frsize;
    info->free_bytes = (unsigned long long)stat.f_bfree * stat.f_frsize;
    info->used_bytes = info->total_bytes - info->free_bytes;
    
    if (info->total_bytes > 0) {
        info->usage_percent = (info->used_bytes * 100.0) / info->total_bytes;
    }
    
    info->file_count = count_files(path);
    
    return 0;
}

static unsigned long long count_files(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH_LENGTH];
    unsigned long long count = 0;
    
    dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (lstat(full_path, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                count++;
            } else if (S_ISDIR(st.st_mode)) {
                count += count_files(full_path);
            }
        }
    }
    
    closedir(dir);
    return count;
}

static int get_alert_level(double usage, double warning, double critical)
{
    if (usage >= critical) return LEVEL_CRITICAL;
    if (usage >= warning) return LEVEL_WARNING;
    return LEVEL_OK;
}

static const char *get_level_name(int level)
{
    switch (level) {
        case LEVEL_OK: return "OK";
        case LEVEL_WARNING: return "WARNING";
        case LEVEL_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

static const char *get_level_color(int level)
{
    switch (level) {
        case LEVEL_OK: return "\033[32m";
        case LEVEL_WARNING: return "\033[33m";
        case LEVEL_CRITICAL: return "\033[31m";
        default: return "\033[0m";
    }
}

static int load_cluster_config(ClusterReport *report, const char *config_file)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char group_name[64], path[MAX_PATH_LENGTH];
    GroupInfo *current_group = NULL;
    
    memset(report, 0, sizeof(ClusterReport));
    report->report_time = time(NULL);
    
    fp = fopen(config_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open config file '%s': %s\n",
                config_file, strerror(errno));
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        
        /* Parse group:path format */
        if (sscanf(p, "%63[^:]:%255s", group_name, path) == 2) {
            /* Find or create group */
            int i;
            current_group = NULL;
            
            for (i = 0; i < report->group_count; i++) {
                if (strcmp(report->groups[i].group_name, group_name) == 0) {
                    current_group = &report->groups[i];
                    break;
                }
            }
            
            if (current_group == NULL && report->group_count < MAX_GROUPS) {
                current_group = &report->groups[report->group_count++];
                strncpy(current_group->group_name, group_name, 63);
            }
            
            /* Add path to group */
            if (current_group != NULL && current_group->path_count < MAX_STORE_PATHS) {
                StoragePathInfo *path_info = &current_group->paths[current_group->path_count];
                if (get_path_info(path, path_info) == 0) {
                    current_group->path_count++;
                    current_group->total_capacity += path_info->total_bytes;
                    current_group->total_used += path_info->used_bytes;
                    current_group->total_free += path_info->free_bytes;
                }
            }
        }
    }
    
    fclose(fp);
    
    /* Calculate group and cluster totals */
    int i;
    for (i = 0; i < report->group_count; i++) {
        GroupInfo *group = &report->groups[i];
        if (group->total_capacity > 0) {
            group->usage_percent = (group->total_used * 100.0) / group->total_capacity;
        }
        report->total_capacity += group->total_capacity;
        report->total_used += group->total_used;
        report->total_free += group->total_free;
    }
    
    if (report->total_capacity > 0) {
        report->usage_percent = (report->total_used * 100.0) / report->total_capacity;
    }
    
    return 0;
}

static void print_report_text(ClusterReport *report, ReportOptions *options)
{
    int i, j;
    char time_str[64];
    char total_str[32], used_str[32], free_str[32];
    int level;
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
             localtime(&report->report_time));
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║           FastDFS Capacity Report - %s           ║\n", time_str);
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Cluster summary */
    format_bytes(report->total_capacity, total_str, sizeof(total_str));
    format_bytes(report->total_used, used_str, sizeof(used_str));
    format_bytes(report->total_free, free_str, sizeof(free_str));
    level = get_alert_level(report->usage_percent, options->warning_threshold,
                            options->critical_threshold);
    
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ CLUSTER SUMMARY                                                 │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│ Total Capacity: %-15s                                   │\n", total_str);
    printf("│ Used Space:     %-15s                                   │\n", used_str);
    printf("│ Free Space:     %-15s                                   │\n", free_str);
    printf("│ Usage:          %s%.1f%% (%s)\033[0m                                    │\n",
           get_level_color(level), report->usage_percent, get_level_name(level));
    printf("│ Groups:         %d                                               │\n",
           report->group_count);
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    /* Group details */
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ GROUP DETAILS                                                   │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│ %-15s %-12s %-12s %-12s %-8s %-8s │\n",
           "Group", "Total", "Used", "Free", "Usage%", "Status");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    
    for (i = 0; i < report->group_count; i++) {
        GroupInfo *group = &report->groups[i];
        
        format_bytes(group->total_capacity, total_str, sizeof(total_str));
        format_bytes(group->total_used, used_str, sizeof(used_str));
        format_bytes(group->total_free, free_str, sizeof(free_str));
        level = get_alert_level(group->usage_percent, options->warning_threshold,
                                options->critical_threshold);
        
        printf("│ %-15s %-12s %-12s %-12s %s%6.1f%%\033[0m %-8s │\n",
               group->group_name, total_str, used_str, free_str,
               get_level_color(level), group->usage_percent, get_level_name(level));
        
        /* Show paths if requested */
        if (options->show_paths) {
            for (j = 0; j < group->path_count; j++) {
                StoragePathInfo *path = &group->paths[j];
                
                format_bytes(path->total_bytes, total_str, sizeof(total_str));
                format_bytes(path->used_bytes, used_str, sizeof(used_str));
                format_bytes(path->free_bytes, free_str, sizeof(free_str));
                level = get_alert_level(path->usage_percent, options->warning_threshold,
                                        options->critical_threshold);
                
                printf("│   └─ %-40s                      │\n", path->path);
                printf("│      %-12s %-12s %-12s %s%6.1f%%\033[0m         │\n",
                       total_str, used_str, free_str,
                       get_level_color(level), path->usage_percent);
            }
        }
    }
    
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
}

static void print_report_json(ClusterReport *report, ReportOptions *options)
{
    int i, j;
    FILE *out = stdout;
    
    if (options->output_file[0]) {
        out = fopen(options->output_file, "w");
        if (out == NULL) {
            fprintf(stderr, "Error: Cannot open output file\n");
            return;
        }
    }
    
    fprintf(out, "{\n");
    fprintf(out, "  \"report_time\": %ld,\n", report->report_time);
    fprintf(out, "  \"cluster\": {\n");
    fprintf(out, "    \"total_capacity\": %llu,\n", report->total_capacity);
    fprintf(out, "    \"total_used\": %llu,\n", report->total_used);
    fprintf(out, "    \"total_free\": %llu,\n", report->total_free);
    fprintf(out, "    \"usage_percent\": %.2f,\n", report->usage_percent);
    fprintf(out, "    \"group_count\": %d\n", report->group_count);
    fprintf(out, "  },\n");
    fprintf(out, "  \"groups\": [\n");
    
    for (i = 0; i < report->group_count; i++) {
        GroupInfo *group = &report->groups[i];
        
        fprintf(out, "    {\n");
        fprintf(out, "      \"name\": \"%s\",\n", group->group_name);
        fprintf(out, "      \"total_capacity\": %llu,\n", group->total_capacity);
        fprintf(out, "      \"total_used\": %llu,\n", group->total_used);
        fprintf(out, "      \"total_free\": %llu,\n", group->total_free);
        fprintf(out, "      \"usage_percent\": %.2f,\n", group->usage_percent);
        fprintf(out, "      \"path_count\": %d", group->path_count);
        
        if (options->show_paths) {
            fprintf(out, ",\n      \"paths\": [\n");
            for (j = 0; j < group->path_count; j++) {
                StoragePathInfo *path = &group->paths[j];
                fprintf(out, "        {\n");
                fprintf(out, "          \"path\": \"%s\",\n", path->path);
                fprintf(out, "          \"total_bytes\": %llu,\n", path->total_bytes);
                fprintf(out, "          \"used_bytes\": %llu,\n", path->used_bytes);
                fprintf(out, "          \"free_bytes\": %llu,\n", path->free_bytes);
                fprintf(out, "          \"usage_percent\": %.2f,\n", path->usage_percent);
                fprintf(out, "          \"file_count\": %llu\n", path->file_count);
                fprintf(out, "        }%s\n", (j < group->path_count - 1) ? "," : "");
            }
            fprintf(out, "      ]\n");
        } else {
            fprintf(out, "\n");
        }
        
        fprintf(out, "    }%s\n", (i < report->group_count - 1) ? "," : "");
    }
    
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    
    if (options->output_file[0] && out != stdout) {
        fclose(out);
        printf("Report written to %s\n", options->output_file);
    }
}

static void print_report_html(ClusterReport *report, ReportOptions *options)
{
    int i, j;
    FILE *out = stdout;
    char time_str[64];
    char size_str[32];
    int level;
    
    if (options->output_file[0]) {
        out = fopen(options->output_file, "w");
        if (out == NULL) {
            fprintf(stderr, "Error: Cannot open output file\n");
            return;
        }
    }
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
             localtime(&report->report_time));
    
    fprintf(out, "<!DOCTYPE html>\n<html>\n<head>\n");
    fprintf(out, "<title>FastDFS Capacity Report</title>\n");
    fprintf(out, "<style>\n");
    fprintf(out, "body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }\n");
    fprintf(out, "h1 { color: #333; }\n");
    fprintf(out, ".container { max-width: 1200px; margin: 0 auto; }\n");
    fprintf(out, ".card { background: white; border-radius: 8px; padding: 20px; margin: 20px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n");
    fprintf(out, "table { border-collapse: collapse; width: 100%%; }\n");
    fprintf(out, "th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }\n");
    fprintf(out, "th { background-color: #4CAF50; color: white; }\n");
    fprintf(out, ".ok { color: #4CAF50; font-weight: bold; }\n");
    fprintf(out, ".warning { color: #FF9800; font-weight: bold; }\n");
    fprintf(out, ".critical { color: #f44336; font-weight: bold; }\n");
    fprintf(out, ".progress { background: #e0e0e0; border-radius: 4px; height: 20px; }\n");
    fprintf(out, ".progress-bar { height: 100%%; border-radius: 4px; }\n");
    fprintf(out, ".summary { display: grid; grid-template-columns: repeat(4, 1fr); gap: 20px; }\n");
    fprintf(out, ".summary-item { text-align: center; padding: 20px; background: #f9f9f9; border-radius: 8px; }\n");
    fprintf(out, ".summary-value { font-size: 24px; font-weight: bold; color: #333; }\n");
    fprintf(out, ".summary-label { color: #666; margin-top: 5px; }\n");
    fprintf(out, "</style>\n</head>\n<body>\n");
    
    fprintf(out, "<div class=\"container\">\n");
    fprintf(out, "<h1>FastDFS Capacity Report</h1>\n");
    fprintf(out, "<p>Generated: %s</p>\n", time_str);
    
    /* Summary cards */
    fprintf(out, "<div class=\"card\">\n");
    fprintf(out, "<h2>Cluster Summary</h2>\n");
    fprintf(out, "<div class=\"summary\">\n");
    
    format_bytes(report->total_capacity, size_str, sizeof(size_str));
    fprintf(out, "<div class=\"summary-item\"><div class=\"summary-value\">%s</div><div class=\"summary-label\">Total Capacity</div></div>\n", size_str);
    
    format_bytes(report->total_used, size_str, sizeof(size_str));
    fprintf(out, "<div class=\"summary-item\"><div class=\"summary-value\">%s</div><div class=\"summary-label\">Used Space</div></div>\n", size_str);
    
    format_bytes(report->total_free, size_str, sizeof(size_str));
    fprintf(out, "<div class=\"summary-item\"><div class=\"summary-value\">%s</div><div class=\"summary-label\">Free Space</div></div>\n", size_str);
    
    level = get_alert_level(report->usage_percent, options->warning_threshold, options->critical_threshold);
    fprintf(out, "<div class=\"summary-item\"><div class=\"summary-value %s\">%.1f%%</div><div class=\"summary-label\">Usage</div></div>\n",
            level == LEVEL_OK ? "ok" : (level == LEVEL_WARNING ? "warning" : "critical"),
            report->usage_percent);
    
    fprintf(out, "</div>\n</div>\n");
    
    /* Group table */
    fprintf(out, "<div class=\"card\">\n");
    fprintf(out, "<h2>Storage Groups</h2>\n");
    fprintf(out, "<table>\n");
    fprintf(out, "<tr><th>Group</th><th>Total</th><th>Used</th><th>Free</th><th>Usage</th><th>Status</th></tr>\n");
    
    for (i = 0; i < report->group_count; i++) {
        GroupInfo *group = &report->groups[i];
        char total_str[32], used_str[32], free_str[32];
        
        format_bytes(group->total_capacity, total_str, sizeof(total_str));
        format_bytes(group->total_used, used_str, sizeof(used_str));
        format_bytes(group->total_free, free_str, sizeof(free_str));
        level = get_alert_level(group->usage_percent, options->warning_threshold, options->critical_threshold);
        
        fprintf(out, "<tr>\n");
        fprintf(out, "<td>%s</td>\n", group->group_name);
        fprintf(out, "<td>%s</td>\n", total_str);
        fprintf(out, "<td>%s</td>\n", used_str);
        fprintf(out, "<td>%s</td>\n", free_str);
        fprintf(out, "<td>\n");
        fprintf(out, "<div class=\"progress\"><div class=\"progress-bar\" style=\"width: %.1f%%; background: %s;\"></div></div>\n",
                group->usage_percent,
                level == LEVEL_OK ? "#4CAF50" : (level == LEVEL_WARNING ? "#FF9800" : "#f44336"));
        fprintf(out, "%.1f%%\n", group->usage_percent);
        fprintf(out, "</td>\n");
        fprintf(out, "<td class=\"%s\">%s</td>\n",
                level == LEVEL_OK ? "ok" : (level == LEVEL_WARNING ? "warning" : "critical"),
                get_level_name(level));
        fprintf(out, "</tr>\n");
    }
    
    fprintf(out, "</table>\n</div>\n");
    fprintf(out, "</div>\n</body>\n</html>\n");
    
    if (options->output_file[0] && out != stdout) {
        fclose(out);
        printf("Report written to %s\n", options->output_file);
    }
}

static void print_report_csv(ClusterReport *report, ReportOptions *options)
{
    int i, j;
    FILE *out = stdout;
    
    if (options->output_file[0]) {
        out = fopen(options->output_file, "w");
        if (out == NULL) {
            fprintf(stderr, "Error: Cannot open output file\n");
            return;
        }
    }
    
    /* Header */
    fprintf(out, "timestamp,group,path,total_bytes,used_bytes,free_bytes,usage_percent,file_count\n");
    
    for (i = 0; i < report->group_count; i++) {
        GroupInfo *group = &report->groups[i];
        
        for (j = 0; j < group->path_count; j++) {
            StoragePathInfo *path = &group->paths[j];
            
            fprintf(out, "%ld,%s,%s,%llu,%llu,%llu,%.2f,%llu\n",
                    report->report_time,
                    group->group_name,
                    path->path,
                    path->total_bytes,
                    path->used_bytes,
                    path->free_bytes,
                    path->usage_percent,
                    path->file_count);
        }
    }
    
    if (options->output_file[0] && out != stdout) {
        fclose(out);
        printf("Report written to %s\n", options->output_file);
    }
}

static void print_report_markdown(ClusterReport *report, ReportOptions *options)
{
    int i;
    FILE *out = stdout;
    char time_str[64];
    char total_str[32], used_str[32], free_str[32];
    int level;
    
    if (options->output_file[0]) {
        out = fopen(options->output_file, "w");
        if (out == NULL) {
            fprintf(stderr, "Error: Cannot open output file\n");
            return;
        }
    }
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
             localtime(&report->report_time));
    
    fprintf(out, "# FastDFS Capacity Report\n\n");
    fprintf(out, "**Generated:** %s\n\n", time_str);
    
    /* Cluster summary */
    fprintf(out, "## Cluster Summary\n\n");
    
    format_bytes(report->total_capacity, total_str, sizeof(total_str));
    format_bytes(report->total_used, used_str, sizeof(used_str));
    format_bytes(report->total_free, free_str, sizeof(free_str));
    level = get_alert_level(report->usage_percent, options->warning_threshold,
                            options->critical_threshold);
    
    fprintf(out, "| Metric | Value |\n");
    fprintf(out, "|--------|-------|\n");
    fprintf(out, "| Total Capacity | %s |\n", total_str);
    fprintf(out, "| Used Space | %s |\n", used_str);
    fprintf(out, "| Free Space | %s |\n", free_str);
    fprintf(out, "| Usage | %.1f%% (%s) |\n", report->usage_percent, get_level_name(level));
    fprintf(out, "| Groups | %d |\n\n", report->group_count);
    
    /* Group details */
    fprintf(out, "## Storage Groups\n\n");
    fprintf(out, "| Group | Total | Used | Free | Usage | Status |\n");
    fprintf(out, "|-------|-------|------|------|-------|--------|\n");
    
    for (i = 0; i < report->group_count; i++) {
        GroupInfo *group = &report->groups[i];
        
        format_bytes(group->total_capacity, total_str, sizeof(total_str));
        format_bytes(group->total_used, used_str, sizeof(used_str));
        format_bytes(group->total_free, free_str, sizeof(free_str));
        level = get_alert_level(group->usage_percent, options->warning_threshold,
                                options->critical_threshold);
        
        fprintf(out, "| %s | %s | %s | %s | %.1f%% | %s |\n",
                group->group_name, total_str, used_str, free_str,
                group->usage_percent, get_level_name(level));
    }
    
    fprintf(out, "\n---\n*Generated by FastDFS Capacity Report Tool*\n");
    
    if (options->output_file[0] && out != stdout) {
        fclose(out);
        printf("Report written to %s\n", options->output_file);
    }
}

int main(int argc, char *argv[])
{
    ClusterReport report;
    ReportOptions options;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"format", required_argument, 0, 'f'},
        {"output", required_argument, 0, 'o'},
        {"warning", required_argument, 0, 'w'},
        {"critical", required_argument, 0, 'c'},
        {"paths", no_argument, 0, 'p'},
        {"predictions", no_argument, 0, 'P'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize options */
    memset(&options, 0, sizeof(options));
    options.format = FORMAT_TEXT;
    options.warning_threshold = 80.0;
    options.critical_threshold = 90.0;
    
    /* Parse command line options */
    while ((opt = getopt_long(argc, argv, "f:o:w:c:pPvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'f':
                if (strcmp(optarg, "json") == 0) {
                    options.format = FORMAT_JSON;
                } else if (strcmp(optarg, "html") == 0) {
                    options.format = FORMAT_HTML;
                } else if (strcmp(optarg, "csv") == 0) {
                    options.format = FORMAT_CSV;
                } else if (strcmp(optarg, "markdown") == 0 || strcmp(optarg, "md") == 0) {
                    options.format = FORMAT_MARKDOWN;
                }
                break;
            case 'o':
                strncpy(options.output_file, optarg, MAX_PATH_LENGTH - 1);
                break;
            case 'w':
                options.warning_threshold = atof(optarg);
                break;
            case 'c':
                options.critical_threshold = atof(optarg);
                break;
            case 'p':
                options.show_paths = 1;
                break;
            case 'P':
                options.show_predictions = 1;
                break;
            case 'v':
                options.verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Check for config file */
    if (optind >= argc) {
        fprintf(stderr, "Error: Config file required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    strncpy(options.config_file, argv[optind], MAX_PATH_LENGTH - 1);
    
    /* Load cluster configuration */
    if (load_cluster_config(&report, options.config_file) != 0) {
        return 1;
    }
    
    /* Print report */
    switch (options.format) {
        case FORMAT_JSON:
            print_report_json(&report, &options);
            break;
        case FORMAT_HTML:
            print_report_html(&report, &options);
            break;
        case FORMAT_CSV:
            print_report_csv(&report, &options);
            break;
        case FORMAT_MARKDOWN:
            print_report_markdown(&report, &options);
            break;
        default:
            print_report_text(&report, &options);
            break;
    }
    
    return 0;
}
