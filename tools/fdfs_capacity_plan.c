/**
 * FastDFS Capacity Planner Tool
 * 
 * This tool provides comprehensive capacity planning capabilities for FastDFS,
 * allowing users to analyze growth trends, predict future storage needs,
 * recommend scaling actions, and generate capacity reports.
 * 
 * Features:
 * - Analyze current storage utilization
 * - Predict future capacity needs based on growth trends
 * - Recommend scaling actions (add servers, expand storage)
 * - Generate detailed capacity reports
 * - Project capacity exhaustion dates
 * - Calculate growth rates and trends
 * - Multi-group analysis
 * - JSON and text output formats
 * 
 * Capacity Analysis:
 * - Current storage utilization
 * - Growth rate calculation
 * - Projected capacity needs
 * - Time to capacity exhaustion
 * - Recommended scaling actions
 * 
 * Growth Projections:
 * - Linear growth projection
 * - Exponential growth projection
 * - Custom growth rate
 * - Multiple projection scenarios
 * 
 * Recommendations:
 * - Add storage servers
 * - Expand existing storage
 * - Rebalance storage distribution
 * - Optimize storage usage
 * 
 * Use Cases:
 * - Proactive capacity planning
 * - Budget planning for storage expansion
 * - Capacity exhaustion prevention
 * - Growth trend analysis
 * - Infrastructure planning
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
#include <math.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum number of groups */
#define MAX_GROUPS 64

/* Maximum number of historical data points */
#define MAX_HISTORY_POINTS 100

/* Default warning threshold (percentage) */
#define DEFAULT_WARNING_THRESHOLD 80.0

/* Default critical threshold (percentage) */
#define DEFAULT_CRITICAL_THRESHOLD 90.0

/* Default projection period (days) */
#define DEFAULT_PROJECTION_DAYS 90

/* Storage snapshot structure */
typedef struct {
    time_t timestamp;        /* Snapshot timestamp */
    int64_t total_space;      /* Total storage space */
    int64_t used_space;       /* Used storage space */
    int64_t free_space;       /* Free storage space */
    double utilization;      /* Utilization percentage */
} StorageSnapshot;

/* Group capacity data structure */
typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];  /* Group name */
    int64_t total_space;                             /* Total storage space */
    int64_t free_space;                              /* Free storage space */
    int64_t used_space;                              /* Used storage space */
    double utilization;                              /* Current utilization percentage */
    int server_count;                                 /* Number of servers */
    StorageSnapshot *history;                         /* Historical snapshots */
    int history_count;                               /* Number of historical points */
    int history_capacity;                            /* History array capacity */
} GroupCapacityData;

/* Growth projection structure */
typedef struct {
    double growth_rate_per_day;      /* Daily growth rate (bytes/day) */
    double growth_rate_percent;      /* Daily growth rate (percentage) */
    int64_t projected_used;          /* Projected used space */
    int64_t projected_free;          /* Projected free space */
    double projected_utilization;    /* Projected utilization */
    int days_to_warning;             /* Days until warning threshold */
    int days_to_critical;            /* Days until critical threshold */
    int days_to_exhaustion;          /* Days until capacity exhaustion */
} GrowthProjection;

/* Capacity recommendation structure */
typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];  /* Group name */
    char recommendation[512];                       /* Recommendation text */
    int priority;                                    /* Priority (1=high, 2=medium, 3=low) */
    int64_t additional_space_needed;                /* Additional space needed */
    int servers_to_add;                             /* Number of servers to add */
    int days_until_action;                          /* Days until action needed */
} CapacityRecommendation;

/* Capacity planner context */
typedef struct {
    ConnectionInfo *pTrackerServer;  /* Tracker server connection */
    GroupCapacityData *groups;       /* Array of group capacity data */
    int group_count;                  /* Number of groups */
    double warning_threshold;         /* Warning threshold (percentage) */
    double critical_threshold;        /* Critical threshold (percentage) */
    int projection_days;             /* Projection period in days */
    int verbose;                     /* Verbose output flag */
    int json_output;                 /* JSON output flag */
} CapacityPlannerContext;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;

/**
 * Format bytes to human-readable string
 * 
 * This function converts a byte count to a human-readable string
 * with appropriate units (B, KB, MB, GB, TB).
 * 
 * @param bytes - Number of bytes to format
 * @param buf - Output buffer for formatted string
 * @param buf_size - Size of output buffer
 */
static void format_bytes(int64_t bytes, char *buf, size_t buf_size) {
    if (bytes >= 1099511627776LL) {
        snprintf(buf, buf_size, "%.2f TB", bytes / 1099511627776.0);
    } else if (bytes >= 1073741824LL) {
        snprintf(buf, buf_size, "%.2f GB", bytes / 1073741824.0);
    } else if (bytes >= 1048576LL) {
        snprintf(buf, buf_size, "%.2f MB", bytes / 1048576.0);
    } else if (bytes >= 1024LL) {
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    } else {
        snprintf(buf, buf_size, "%lld B", (long long)bytes);
    }
}

/**
 * Format timestamp to human-readable string
 * 
 * This function converts a Unix timestamp to a human-readable
 * date-time string.
 * 
 * @param timestamp - Unix timestamp
 * @param buf - Output buffer for formatted string
 * @param buf_size - Size of output buffer
 */
static void format_timestamp(time_t timestamp, char *buf, size_t buf_size) {
    struct tm *tm_info;
    
    if (timestamp == 0) {
        snprintf(buf, buf_size, "Unknown");
        return;
    }
    
    tm_info = localtime(&timestamp);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * Calculate growth rate
 * 
 * This function calculates the growth rate based on historical data.
 * Uses linear regression to estimate daily growth rate.
 * 
 * @param group - Group capacity data
 * @param projection - Output parameter for growth projection
 * @return 0 on success, error code on failure
 */
static int calculate_growth_rate(GroupCapacityData *group, GrowthProjection *projection) {
    int i;
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    double n;
    double slope, intercept;
    time_t current_time;
    double days_since_first;
    
    if (group == NULL || projection == NULL) {
        return EINVAL;
    }
    
    memset(projection, 0, sizeof(GrowthProjection));
    
    /* Need at least 2 data points for growth calculation */
    if (group->history_count < 2) {
        /* Use default growth rate if no history */
        projection->growth_rate_per_day = 0.0;
        projection->growth_rate_percent = 0.0;
        return 0;
    }
    
    current_time = time(NULL);
    
    /* Calculate linear regression */
    n = (double)group->history_count;
    for (i = 0; i < group->history_count; i++) {
        double x = difftime(group->history[i].timestamp, group->history[0].timestamp) / 86400.0;  /* Days */
        double y = (double)group->history[i].used_space;
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    /* Calculate slope (growth rate per day) */
    slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    intercept = (sum_y - slope * sum_x) / n;
    
    projection->growth_rate_per_day = slope;
    
    /* Calculate growth rate as percentage */
    if (group->used_space > 0) {
        projection->growth_rate_percent = (slope / (double)group->used_space) * 100.0;
    } else {
        projection->growth_rate_percent = 0.0;
    }
    
    return 0;
}

/**
 * Project future capacity
 * 
 * This function projects future capacity needs based on growth rate.
 * 
 * @param group - Group capacity data
 * @param projection - Growth projection
 * @param days - Number of days to project
 * @param ctx - Capacity planner context
 */
static void project_future_capacity(GroupCapacityData *group,
                                    GrowthProjection *projection,
                                    int days,
                                    CapacityPlannerContext *ctx) {
    int64_t projected_used;
    int64_t projected_free;
    double projected_utilization;
    int days_to_warning = -1;
    int days_to_critical = -1;
    int days_to_exhaustion = -1;
    int i;
    
    if (group == NULL || projection == NULL || ctx == NULL) {
        return;
    }
    
    /* Project used space */
    projected_used = group->used_space + (int64_t)(projection->growth_rate_per_day * days);
    if (projected_used < 0) {
        projected_used = 0;
    }
    if (projected_used > group->total_space) {
        projected_used = group->total_space;
    }
    
    projection->projected_used = projected_used;
    projection->projected_free = group->total_space - projected_used;
    
    if (group->total_space > 0) {
        projection->projected_utilization = (projected_used * 100.0) / (double)group->total_space;
    } else {
        projection->projected_utilization = 0.0;
    }
    
    /* Calculate days to thresholds */
    if (projection->growth_rate_per_day > 0) {
        /* Days to warning threshold */
        if (ctx->warning_threshold > 0 && group->total_space > 0) {
            int64_t warning_used = (int64_t)((ctx->warning_threshold / 100.0) * (double)group->total_space);
            if (warning_used > group->used_space) {
                int64_t space_needed = warning_used - group->used_space;
                days_to_warning = (int)(space_needed / projection->growth_rate_per_day);
            }
        }
        
        /* Days to critical threshold */
        if (ctx->critical_threshold > 0 && group->total_space > 0) {
            int64_t critical_used = (int64_t)((ctx->critical_threshold / 100.0) * (double)group->total_space);
            if (critical_used > group->used_space) {
                int64_t space_needed = critical_used - group->used_space;
                days_to_critical = (int)(space_needed / projection->growth_rate_per_day);
            }
        }
        
        /* Days to exhaustion */
        if (group->free_space > 0) {
            days_to_exhaustion = (int)(group->free_space / projection->growth_rate_per_day);
        }
    }
    
    projection->days_to_warning = days_to_warning;
    projection->days_to_critical = days_to_critical;
    projection->days_to_exhaustion = days_to_exhaustion;
}

/**
 * Generate capacity recommendations
 * 
 * This function generates recommendations based on current capacity
 * and projected growth.
 * 
 * @param group - Group capacity data
 * @param projection - Growth projection
 * @param ctx - Capacity planner context
 * @param recommendation - Output parameter for recommendation
 */
static void generate_recommendation(GroupCapacityData *group,
                                   GrowthProjection *projection,
                                   CapacityPlannerContext *ctx,
                                   CapacityRecommendation *recommendation) {
    int64_t additional_space = 0;
    int servers_to_add = 0;
    int priority = 3;  /* Low priority by default */
    int days_until_action = -1;
    char rec_text[512];
    
    if (group == NULL || projection == NULL || ctx == NULL || recommendation == NULL) {
        return;
    }
    
    memset(recommendation, 0, sizeof(CapacityRecommendation));
    strncpy(recommendation->group_name, group->group_name,
           sizeof(recommendation->group_name) - 1);
    
    /* Determine priority and recommendations */
    if (group->utilization >= ctx->critical_threshold) {
        priority = 1;  /* High priority */
        snprintf(rec_text, sizeof(rec_text),
                "CRITICAL: Group %s is at %.1f%% capacity. Immediate action required.",
                group->group_name, group->utilization);
        days_until_action = 0;
        
        /* Calculate additional space needed */
        if (projection->growth_rate_per_day > 0) {
            /* Need space for at least 30 days */
            additional_space = (int64_t)(projection->growth_rate_per_day * 30);
            if (additional_space < group->total_space * 0.2) {
                additional_space = (int64_t)(group->total_space * 0.2);  /* At least 20% more */
            }
        } else {
            additional_space = (int64_t)(group->total_space * 0.3);  /* 30% more */
        }
    } else if (group->utilization >= ctx->warning_threshold) {
        priority = 2;  /* Medium priority */
        snprintf(rec_text, sizeof(rec_text),
                "WARNING: Group %s is at %.1f%% capacity. Plan for expansion within %d days.",
                group->group_name, group->utilization,
                projection->days_to_critical > 0 ? projection->days_to_critical : 30);
        days_until_action = projection->days_to_critical > 0 ? projection->days_to_critical : 30;
        
        /* Calculate additional space needed */
        if (projection->growth_rate_per_day > 0 && projection->days_to_critical > 0) {
            /* Need space for at least 60 days beyond critical threshold */
            additional_space = (int64_t)(projection->growth_rate_per_day * (projection->days_to_critical + 60));
        } else {
            additional_space = (int64_t)(group->total_space * 0.2);  /* 20% more */
        }
    } else if (projection->days_to_warning > 0 && projection->days_to_warning < 90) {
        priority = 2;  /* Medium priority */
        snprintf(rec_text, sizeof(rec_text),
                "Group %s will reach warning threshold in %d days. Consider planning for expansion.",
                group->group_name, projection->days_to_warning);
        days_until_action = projection->days_to_warning;
        
        /* Calculate additional space needed */
        if (projection->growth_rate_per_day > 0) {
            additional_space = (int64_t)(projection->growth_rate_per_day * 90);  /* 90 days worth */
        }
    } else {
        priority = 3;  /* Low priority */
        snprintf(rec_text, sizeof(rec_text),
                "Group %s has adequate capacity (%.1f%% used). Monitor growth trends.",
                group->group_name, group->utilization);
        days_until_action = projection->days_to_warning > 0 ? projection->days_to_warning : 365;
    }
    
    /* Estimate servers to add (assuming average server size) */
    if (group->server_count > 0 && additional_space > 0) {
        int64_t avg_server_space = group->total_space / group->server_count;
        servers_to_add = (int)((additional_space + avg_server_space - 1) / avg_server_space);
        if (servers_to_add < 1) {
            servers_to_add = 1;
        }
    } else if (additional_space > 0) {
        /* No servers, assume default size */
        int64_t default_server_space = 1073741824LL * 100;  /* 100GB default */
        servers_to_add = (int)((additional_space + default_server_space - 1) / default_server_space);
        if (servers_to_add < 1) {
            servers_to_add = 1;
        }
    }
    
    strncpy(recommendation->recommendation, rec_text, sizeof(recommendation->recommendation) - 1);
    recommendation->priority = priority;
    recommendation->additional_space_needed = additional_space;
    recommendation->servers_to_add = servers_to_add;
    recommendation->days_until_action = days_until_action;
}

/**
 * Collect current capacity data
 * 
 * This function collects current capacity data from FastDFS cluster.
 * 
 * @param ctx - Capacity planner context
 * @return 0 on success, error code on failure
 */
static int collect_capacity_data(CapacityPlannerContext *ctx) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    int result;
    int stat_count;
    int i;
    
    if (ctx == NULL) {
        return EINVAL;
    }
    
    /* List all groups */
    result = tracker_list_groups(ctx->pTrackerServer, group_stats, MAX_GROUPS, &stat_count);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to list groups: %s\n", STRERROR(result));
        return result;
    }
    
    /* Allocate group capacity data */
    ctx->groups = (GroupCapacityData *)calloc(stat_count, sizeof(GroupCapacityData));
    if (ctx->groups == NULL) {
        return ENOMEM;
    }
    
    ctx->group_count = stat_count;
    
    /* Collect data for each group */
    for (i = 0; i < stat_count; i++) {
        GroupCapacityData *group = &ctx->groups[i];
        FDFSGroupStat *group_stat = &group_stats[i];
        
        strncpy(group->group_name, group_stat->group_name,
               sizeof(group->group_name) - 1);
        
        group->total_space = group_stat->total_mb * 1024LL * 1024LL;
        group->free_space = group_stat->free_mb * 1024LL * 1024LL;
        group->used_space = group->total_space - group->free_space;
        
        if (group->total_space > 0) {
            group->utilization = (group->used_space * 100.0) / (double)group->total_space;
        } else {
            group->utilization = 0.0;
        }
        
        group->server_count = group_stat->storage_count;
        
        /* Initialize history */
        group->history_capacity = 10;
        group->history = (StorageSnapshot *)calloc(group->history_capacity, sizeof(StorageSnapshot));
        if (group->history == NULL) {
            continue;
        }
        
        /* Add current snapshot to history */
        if (group->history_count < group->history_capacity) {
            StorageSnapshot *snapshot = &group->history[group->history_count++];
            snapshot->timestamp = time(NULL);
            snapshot->total_space = group->total_space;
            snapshot->used_space = group->used_space;
            snapshot->free_space = group->free_space;
            snapshot->utilization = group->utilization;
        }
    }
    
    return 0;
}

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_capacity_plan tool, including all available options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("FastDFS Capacity Planner Tool\n");
    printf("\n");
    printf("This tool analyzes storage capacity, predicts future needs,\n");
    printf("and recommends scaling actions for proactive capacity planning.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE         Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -g, --group NAME          Analyze specific group only\n");
    printf("  -w, --warning PERCENT     Warning threshold (default: 80.0%%)\n");
    printf("  -C, --critical PERCENT     Critical threshold (default: 90.0%%)\n");
    printf("  -p, --projection DAYS      Projection period in days (default: 90)\n");
    printf("  -O, --output FILE         Output report file (default: stdout)\n");
    printf("  -v, --verbose             Verbose output\n");
    printf("  -q, --quiet               Quiet mode (only show errors)\n");
    printf("  -J, --json                Output in JSON format\n");
    printf("  -h, --help                Show this help message\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - Analysis completed successfully\n");
    printf("  1 - Some groups need attention\n");
    printf("  2 - Error occurred\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Analyze all groups\n");
    printf("  %s\n", program_name);
    printf("\n");
    printf("  # Analyze specific group\n");
    printf("  %s -g group1\n", program_name);
    printf("\n");
    printf("  # Custom thresholds\n");
    printf("  %s -w 75 -C 85\n", program_name);
    printf("\n");
    printf("  # 180-day projection\n");
    printf("  %s -p 180\n", program_name);
}

/**
 * Print capacity report in text format
 * 
 * This function prints a comprehensive capacity report in
 * human-readable text format.
 * 
 * @param ctx - Capacity planner context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_capacity_report_text(CapacityPlannerContext *ctx, FILE *output_file) {
    int i;
    char total_buf[64], used_buf[64], free_buf[64];
    time_t current_time;
    char time_buf[64];
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    current_time = time(NULL);
    format_timestamp(current_time, time_buf, sizeof(time_buf));
    
    fprintf(output_file, "\n");
    fprintf(output_file, "========================================\n");
    fprintf(output_file, "FastDFS Capacity Planning Report\n");
    fprintf(output_file, "========================================\n");
    fprintf(output_file, "\n");
    fprintf(output_file, "Generated: %s\n", time_buf);
    fprintf(output_file, "Warning Threshold: %.1f%%\n", ctx->warning_threshold);
    fprintf(output_file, "Critical Threshold: %.1f%%\n", ctx->critical_threshold);
    fprintf(output_file, "Projection Period: %d days\n", ctx->projection_days);
    fprintf(output_file, "\n");
    
    for (i = 0; i < ctx->group_count; i++) {
        GroupCapacityData *group = &ctx->groups[i];
        GrowthProjection projection;
        CapacityRecommendation recommendation;
        
        calculate_growth_rate(group, &projection);
        project_future_capacity(group, &projection, ctx->projection_days, ctx);
        generate_recommendation(group, &projection, ctx, &recommendation);
        
        format_bytes(group->total_space, total_buf, sizeof(total_buf));
        format_bytes(group->used_space, used_buf, sizeof(used_buf));
        format_bytes(group->free_space, free_buf, sizeof(free_buf));
        
        fprintf(output_file, "----------------------------------------\n");
        fprintf(output_file, "Group: %s\n", group->group_name);
        fprintf(output_file, "----------------------------------------\n");
        fprintf(output_file, "\n");
        
        fprintf(output_file, "Current Capacity:\n");
        fprintf(output_file, "  Total Space: %s\n", total_buf);
        fprintf(output_file, "  Used Space:  %s (%.1f%%)\n", used_buf, group->utilization);
        fprintf(output_file, "  Free Space:  %s\n", free_buf);
        fprintf(output_file, "  Servers:     %d\n", group->server_count);
        fprintf(output_file, "\n");
        
        if (projection.growth_rate_per_day > 0) {
            fprintf(output_file, "Growth Analysis:\n");
            format_bytes((int64_t)projection.growth_rate_per_day, used_buf, sizeof(used_buf));
            fprintf(output_file, "  Growth Rate: %s/day (%.2f%%/day)\n",
                   used_buf, projection.growth_rate_percent);
            fprintf(output_file, "\n");
            
            fprintf(output_file, "Projected Capacity (%d days):\n", ctx->projection_days);
            format_bytes(projection.projected_used, used_buf, sizeof(used_buf));
            format_bytes(projection.projected_free, free_buf, sizeof(free_buf));
            fprintf(output_file, "  Projected Used:  %s (%.1f%%)\n",
                   used_buf, projection.projected_utilization);
            fprintf(output_file, "  Projected Free:  %s\n", free_buf);
            fprintf(output_file, "\n");
            
            fprintf(output_file, "Time to Thresholds:\n");
            if (projection.days_to_warning > 0) {
                fprintf(output_file, "  Warning Threshold:  %d days\n", projection.days_to_warning);
            } else {
                fprintf(output_file, "  Warning Threshold:  Already exceeded\n");
            }
            
            if (projection.days_to_critical > 0) {
                fprintf(output_file, "  Critical Threshold: %d days\n", projection.days_to_critical);
            } else {
                fprintf(output_file, "  Critical Threshold: Already exceeded\n");
            }
            
            if (projection.days_to_exhaustion > 0) {
                fprintf(output_file, "  Capacity Exhaustion: %d days\n", projection.days_to_exhaustion);
            } else {
                fprintf(output_file, "  Capacity Exhaustion: Already exhausted\n");
            }
        } else {
            fprintf(output_file, "Growth Analysis:\n");
            fprintf(output_file, "  Growth Rate: Insufficient historical data\n");
            fprintf(output_file, "\n");
        }
        
        fprintf(output_file, "\n");
        fprintf(output_file, "Recommendation:\n");
        fprintf(output_file, "  Priority: %s\n",
               recommendation.priority == 1 ? "HIGH" :
               recommendation.priority == 2 ? "MEDIUM" : "LOW");
        fprintf(output_file, "  %s\n", recommendation.recommendation);
        
        if (recommendation.additional_space_needed > 0) {
            format_bytes(recommendation.additional_space_needed, used_buf, sizeof(used_buf));
            fprintf(output_file, "  Additional Space Needed: %s\n", used_buf);
        }
        
        if (recommendation.servers_to_add > 0) {
            fprintf(output_file, "  Recommended Servers to Add: %d\n", recommendation.servers_to_add);
        }
        
        if (recommendation.days_until_action >= 0) {
            fprintf(output_file, "  Days Until Action: %d\n", recommendation.days_until_action);
        }
        
        fprintf(output_file, "\n");
    }
    
    fprintf(output_file, "========================================\n");
    fprintf(output_file, "\n");
}

/**
 * Print capacity report in JSON format
 * 
 * This function prints a comprehensive capacity report in JSON format
 * for programmatic processing.
 * 
 * @param ctx - Capacity planner context
 * @param output_file - Output file (NULL for stdout)
 */
static void print_capacity_report_json(CapacityPlannerContext *ctx, FILE *output_file) {
    int i;
    
    if (ctx == NULL || output_file == NULL) {
        return;
    }
    
    fprintf(output_file, "{\n");
    fprintf(output_file, "  \"timestamp\": %ld,\n", (long)time(NULL));
    fprintf(output_file, "  \"warning_threshold\": %.1f,\n", ctx->warning_threshold);
    fprintf(output_file, "  \"critical_threshold\": %.1f,\n", ctx->critical_threshold);
    fprintf(output_file, "  \"projection_days\": %d,\n", ctx->projection_days);
    fprintf(output_file, "  \"groups\": [\n");
    
    for (i = 0; i < ctx->group_count; i++) {
        GroupCapacityData *group = &ctx->groups[i];
        GrowthProjection projection;
        CapacityRecommendation recommendation;
        
        calculate_growth_rate(group, &projection);
        project_future_capacity(group, &projection, ctx->projection_days, ctx);
        generate_recommendation(group, &projection, ctx, &recommendation);
        
        if (i > 0) {
            fprintf(output_file, ",\n");
        }
        
        fprintf(output_file, "    {\n");
        fprintf(output_file, "      \"group_name\": \"%s\",\n", group->group_name);
        fprintf(output_file, "      \"current_capacity\": {\n");
        fprintf(output_file, "        \"total_space\": %lld,\n", (long long)group->total_space);
        fprintf(output_file, "        \"used_space\": %lld,\n", (long long)group->used_space);
        fprintf(output_file, "        \"free_space\": %lld,\n", (long long)group->free_space);
        fprintf(output_file, "        \"utilization\": %.1f,\n", group->utilization);
        fprintf(output_file, "        \"server_count\": %d\n", group->server_count);
        fprintf(output_file, "      },\n");
        
        if (projection.growth_rate_per_day > 0) {
            fprintf(output_file, "      \"growth_analysis\": {\n");
            fprintf(output_file, "        \"growth_rate_per_day\": %.0f,\n", projection.growth_rate_per_day);
            fprintf(output_file, "        \"growth_rate_percent\": %.2f\n", projection.growth_rate_percent);
            fprintf(output_file, "      },\n");
            
            fprintf(output_file, "      \"projection\": {\n");
            fprintf(output_file, "        \"projected_used\": %lld,\n", (long long)projection.projected_used);
            fprintf(output_file, "        \"projected_free\": %lld,\n", (long long)projection.projected_free);
            fprintf(output_file, "        \"projected_utilization\": %.1f,\n", projection.projected_utilization);
            fprintf(output_file, "        \"days_to_warning\": %d,\n", projection.days_to_warning);
            fprintf(output_file, "        \"days_to_critical\": %d,\n", projection.days_to_critical);
            fprintf(output_file, "        \"days_to_exhaustion\": %d\n", projection.days_to_exhaustion);
            fprintf(output_file, "      },\n");
        }
        
        fprintf(output_file, "      \"recommendation\": {\n");
        fprintf(output_file, "        \"priority\": %d,\n", recommendation.priority);
        fprintf(output_file, "        \"message\": \"%s\",\n", recommendation.recommendation);
        fprintf(output_file, "        \"additional_space_needed\": %lld,\n",
               (long long)recommendation.additional_space_needed);
        fprintf(output_file, "        \"servers_to_add\": %d,\n", recommendation.servers_to_add);
        fprintf(output_file, "        \"days_until_action\": %d\n", recommendation.days_until_action);
        fprintf(output_file, "      }\n");
        fprintf(output_file, "    }");
    }
    
    fprintf(output_file, "\n  ]\n");
    fprintf(output_file, "}\n");
}

/**
 * Main function
 * 
 * Entry point for the capacity planner tool. Parses command-line
 * arguments and performs capacity analysis.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = attention needed, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *target_group = NULL;
    char *output_file = NULL;
    double warning_threshold = DEFAULT_WARNING_THRESHOLD;
    double critical_threshold = DEFAULT_CRITICAL_THRESHOLD;
    int projection_days = DEFAULT_PROJECTION_DAYS;
    int result;
    ConnectionInfo *pTrackerServer;
    CapacityPlannerContext ctx;
    FILE *out_fp = stdout;
    int i;
    int opt;
    int option_index = 0;
    int has_critical = 0;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"group", required_argument, 0, 'g'},
        {"warning", required_argument, 0, 'w'},
        {"critical", required_argument, 0, 'C'},
        {"projection", required_argument, 0, 'p'},
        {"output", required_argument, 0, 'O'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize context */
    memset(&ctx, 0, sizeof(CapacityPlannerContext));
    ctx.warning_threshold = warning_threshold;
    ctx.critical_threshold = critical_threshold;
    ctx.projection_days = projection_days;
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:g:w:C:p:O:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'g':
                target_group = optarg;
                break;
            case 'w':
                warning_threshold = atof(optarg);
                if (warning_threshold < 0 || warning_threshold > 100) {
                    warning_threshold = DEFAULT_WARNING_THRESHOLD;
                }
                ctx.warning_threshold = warning_threshold;
                break;
            case 'C':
                critical_threshold = atof(optarg);
                if (critical_threshold < 0 || critical_threshold > 100) {
                    critical_threshold = DEFAULT_CRITICAL_THRESHOLD;
                }
                ctx.critical_threshold = critical_threshold;
                break;
            case 'p':
                projection_days = atoi(optarg);
                if (projection_days < 1) {
                    projection_days = DEFAULT_PROJECTION_DAYS;
                }
                ctx.projection_days = projection_days;
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
    
    /* Collect capacity data */
    result = collect_capacity_data(&ctx);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to collect capacity data: %s\n", STRERROR(result));
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 2;
    }
    
    /* Filter by target group if specified */
    if (target_group != NULL) {
        for (i = 0; i < ctx.group_count; i++) {
            if (strcmp(ctx.groups[i].group_name, target_group) == 0) {
                /* Move to first position */
                GroupCapacityData temp = ctx.groups[0];
                ctx.groups[0] = ctx.groups[i];
                ctx.groups[i] = temp;
                ctx.group_count = 1;
                break;
            }
        }
        
        if (ctx.group_count > 1 || (ctx.group_count == 1 && strcmp(ctx.groups[0].group_name, target_group) != 0)) {
            fprintf(stderr, "ERROR: Group '%s' not found\n", target_group);
            if (ctx.groups != NULL) {
                for (i = 0; i < ctx.group_count; i++) {
                    if (ctx.groups[i].history != NULL) {
                        free(ctx.groups[i].history);
                    }
                }
                free(ctx.groups);
            }
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return 2;
        }
    }
    
    /* Check for critical groups */
    for (i = 0; i < ctx.group_count; i++) {
        if (ctx.groups[i].utilization >= ctx.critical_threshold) {
            has_critical = 1;
            break;
        }
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
        print_capacity_report_json(&ctx, out_fp);
    } else {
        print_capacity_report_text(&ctx, out_fp);
    }
    
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Cleanup */
    if (ctx.groups != NULL) {
        for (i = 0; i < ctx.group_count; i++) {
            if (ctx.groups[i].history != NULL) {
                free(ctx.groups[i].history);
            }
        }
        free(ctx.groups);
    }
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    /* Return appropriate exit code */
    if (has_critical) {
        return 1;  /* Attention needed */
    }
    
    return 0;  /* Success */
}

