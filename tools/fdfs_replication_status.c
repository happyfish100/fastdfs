/**
 * FastDFS Replication Status Checker Tool
 * 
 * This tool provides comprehensive replication status monitoring for FastDFS
 * storage groups. It monitors replication lag, pending sync operations, and
 * overall replication health across all storage servers within groups.
 * 
 * Features:
 * - Monitor replication lag per server
 * - Track pending sync operations
 * - Assess replication health per group
 * - Calculate sync delays and identify lagging servers
 * - Monitor sync throughput (bytes in/out)
 * - Track sync success rates
 * - Alert on replication issues
 * - Watch mode for continuous monitoring
 * - JSON and text output formats
 * 
 * Replication Health Indicators:
 * - Replication lag (time difference between source and destination)
 * - Sync status (syncing, synced, not synced)
 * - Sync throughput and success rates
 * - Pending sync operations count
 * - Server synchronization state
 * 
 * Use Cases:
 * - Monitor replication health in production
 * - Identify lagging storage servers
 * - Detect replication failures
 * - Track sync performance
 * - Capacity planning for replication
 * - Troubleshooting replication issues
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
#include <sys/time.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum group name length */
#define MAX_GROUP_NAME_LEN 32

/* Maximum number of storage groups */
#define MAX_GROUPS 64

/* Maximum number of servers per group */
#define MAX_SERVERS_PER_GROUP 32

/* Replication status enumeration */
typedef enum {
    REPLICATION_STATUS_HEALTHY = 0,      /* Replication is healthy */
    REPLICATION_STATUS_LAGGING = 1,      /* Replication is lagging */
    REPLICATION_STATUS_STALLED = 2,      /* Replication appears stalled */
    REPLICATION_STATUS_FAILED = 3,       /* Replication has failed */
    REPLICATION_STATUS_UNKNOWN = 4       /* Status cannot be determined */
} ReplicationStatus;

/* Replication lag information for a server pair */
typedef struct {
    char src_server_id[FDFS_STORAGE_ID_MAX_SIZE];  /* Source server ID */
    char dest_server_id[FDFS_STORAGE_ID_MAX_SIZE]; /* Destination server ID */
    char src_ip[IP_ADDRESS_SIZE];                  /* Source server IP */
    char dest_ip[IP_ADDRESS_SIZE];                 /* Destination server IP */
    int src_port;                                   /* Source server port */
    int dest_port;                                  /* Destination server port */
    time_t last_synced_timestamp;                   /* Last synced timestamp */
    time_t current_time;                            /* Current time */
    int64_t sync_lag_seconds;                       /* Sync lag in seconds */
    int64_t total_sync_in_bytes;                    /* Total bytes synced in */
    int64_t success_sync_in_bytes;                  /* Successfully synced bytes in */
    int64_t total_sync_out_bytes;                   /* Total bytes synced out */
    int64_t success_sync_out_bytes;                 /* Successfully synced bytes out */
    time_t last_sync_update;                        /* Last sync update time */
    ReplicationStatus status;                        /* Replication status */
    char status_message[512];                        /* Human-readable status message */
} ReplicationLagInfo;

/* Server replication information */
typedef struct {
    char server_id[FDFS_STORAGE_ID_MAX_SIZE];       /* Server ID */
    char ip_addr[IP_ADDRESS_SIZE];                  /* Server IP address */
    int port;                                       /* Server port */
    time_t last_synced_timestamp;                    /* Last synced timestamp */
    time_t last_sync_update;                        /* Last sync update time */
    time_t last_heartbeat;                          /* Last heartbeat time */
    int64_t sync_lag_seconds;                       /* Maximum sync lag in seconds */
    int64_t total_sync_in_bytes;                    /* Total bytes synced in */
    int64_t success_sync_in_bytes;                  /* Successfully synced bytes in */
    int64_t total_sync_out_bytes;                   /* Total bytes synced out */
    int64_t success_sync_out_bytes;                  /* Successfully synced bytes out */
    int pending_sync_operations;                    /* Estimated pending sync operations */
    ReplicationStatus status;                       /* Overall replication status */
    int is_syncing;                                 /* Whether server is currently syncing */
    ReplicationLagInfo lag_info[MAX_SERVERS_PER_GROUP]; /* Lag info for each source */
    int lag_info_count;                             /* Number of lag info entries */
} ServerReplicationInfo;

/* Group replication information */
typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];  /* Group name */
    int server_count;                               /* Number of servers in group */
    int healthy_servers;                            /* Number of healthy servers */
    int lagging_servers;                            /* Number of lagging servers */
    int stalled_servers;                            /* Number of stalled servers */
    int failed_servers;                             /* Number of failed servers */
    int64_t max_sync_lag_seconds;                   /* Maximum sync lag in group */
    int64_t avg_sync_lag_seconds;                   /* Average sync lag in group */
    int total_pending_operations;                   /* Total pending sync operations */
    double sync_success_rate;                       /* Overall sync success rate */
    ReplicationStatus overall_status;               /* Overall group replication status */
    ServerReplicationInfo servers[MAX_SERVERS_PER_GROUP]; /* Server replication info */
    time_t check_time;                              /* When check was performed */
} GroupReplicationInfo;

/* Global configuration flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;
static int watch_mode = 0;
static int watch_interval = 5;
static int64_t lag_warning_threshold = 300;  /* Warning threshold: 5 minutes */
static int64_t lag_critical_threshold = 3600; /* Critical threshold: 1 hour */

/**
 * Print usage information
 * 
 * This function displays comprehensive usage information for the
 * fdfs_replication_status tool, including all available options.
 * 
 * @param program_name - Name of the program (argv[0])
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("FastDFS Replication Status Checker Tool\n");
    printf("\n");
    printf("This tool monitors replication status and lag across FastDFS\n");
    printf("storage groups. It tracks sync operations, calculates replication\n");
    printf("lag, and identifies replication health issues.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -g, --group NAME     Show status for specific group only\n");
    printf("  -w, --watch          Watch mode (continuous monitoring)\n");
    printf("  -i, --interval SEC   Watch interval in seconds (default: 5)\n");
    printf("  --lag-warning SEC    Warning threshold for lag in seconds (default: 300)\n");
    printf("  --lag-critical SEC   Critical threshold for lag in seconds (default: 3600)\n");
    printf("  -o, --output FILE    Output report file (default: stdout)\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -q, --quiet          Quiet mode (only show issues)\n");
    printf("  -J, --json           Output in JSON format\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Replication Status Levels:\n");
    printf("  HEALTHY  - Replication is working normally\n");
    printf("  LAGGING  - Replication lag exceeds warning threshold\n");
    printf("  STALLED  - Replication appears to be stalled\n");
    printf("  FAILED   - Replication has failed or server is offline\n");
    printf("  UNKNOWN  - Status cannot be determined\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - All replication is healthy\n");
    printf("  1 - Some replication issues detected\n");
    printf("  2 - Critical replication failures\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Check replication status for all groups\n");
    printf("  %s\n", program_name);
    printf("\n");
    printf("  # Check specific group\n");
    printf("  %s -g group1\n", program_name);
    printf("\n");
    printf("  # Watch mode with custom thresholds\n");
    printf("  %s -w -i 10 --lag-warning 600 --lag-critical 7200\n", program_name);
    printf("\n");
    printf("  # JSON output\n");
    printf("  %s -J -o status.json\n", program_name);
}

/**
 * Format duration to human-readable string
 * 
 * This function converts a duration in seconds to a human-readable
 * string (e.g., "5 minutes", "2 hours", "30 seconds").
 * 
 * @param seconds - Duration in seconds
 * @param buf - Output buffer for formatted string
 * @param buf_size - Size of output buffer
 */
static void format_duration(int64_t seconds, char *buf, size_t buf_size) {
    if (seconds < 0) {
        snprintf(buf, buf_size, "Unknown");
        return;
    }
    
    if (seconds >= 86400LL) {
        snprintf(buf, buf_size, "%.1f days", seconds / 86400.0);
    } else if (seconds >= 3600LL) {
        snprintf(buf, buf_size, "%.1f hours", seconds / 3600.0);
    } else if (seconds >= 60LL) {
        snprintf(buf, buf_size, "%.1f minutes", seconds / 60.0);
    } else {
        snprintf(buf, buf_size, "%lld seconds", (long long)seconds);
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
        snprintf(buf, buf_size, "Never");
        return;
    }
    
    tm_info = localtime(&timestamp);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

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
 * Calculate replication status from lag information
 * 
 * This function determines the replication status based on sync lag
 * and other factors.
 * 
 * @param lag_seconds - Sync lag in seconds
 * @param last_synced_timestamp - Last synced timestamp
 * @param last_sync_update - Last sync update time
 * @param is_online - Whether server is online
 * @return ReplicationStatus value
 */
static ReplicationStatus calculate_replication_status(int64_t lag_seconds,
                                                      time_t last_synced_timestamp,
                                                      time_t last_sync_update,
                                                      int is_online) {
    time_t current_time;
    
    current_time = time(NULL);
    
    /* Check if server is online */
    if (!is_online) {
        return REPLICATION_STATUS_FAILED;
    }
    
    /* Check if never synced */
    if (last_synced_timestamp == 0) {
        return REPLICATION_STATUS_UNKNOWN;
    }
    
    /* Check if sync appears stalled */
    if (last_sync_update > 0) {
        time_t time_since_update = current_time - last_sync_update;
        if (time_since_update > 3600) {  /* No update in over an hour */
            return REPLICATION_STATUS_STALLED;
        }
    }
    
    /* Check lag thresholds */
    if (lag_seconds >= lag_critical_threshold) {
        return REPLICATION_STATUS_FAILED;
    } else if (lag_seconds >= lag_warning_threshold) {
        return REPLICATION_STATUS_LAGGING;
    } else if (lag_seconds < 0) {
        return REPLICATION_STATUS_UNKNOWN;
    } else {
        return REPLICATION_STATUS_HEALTHY;
    }
}

/**
 * Get replication status for a storage group
 * 
 * This function retrieves replication status information for all
 * servers in a storage group from the tracker server.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param group_name - Group name (NULL for all groups)
 * @param group_info - Output parameter for group replication info
 * @return 0 on success, error code on failure
 */
static int get_group_replication_status(ConnectionInfo *pTrackerServer,
                                      const char *group_name,
                                      GroupReplicationInfo *group_info) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    FDFSStorageInfo storage_infos[MAX_SERVERS_PER_GROUP];
    int group_count;
    int storage_count;
    int i, j, k;
    int ret;
    time_t current_time;
    int64_t total_lag = 0;
    int lag_count = 0;
    
    if (pTrackerServer == NULL || group_info == NULL) {
        return EINVAL;
    }
    
    current_time = time(NULL);
    
    /* Initialize group info */
    memset(group_info, 0, sizeof(GroupReplicationInfo));
    group_info->check_time = current_time;
    
    /* List groups */
    ret = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &group_count);
    if (ret != 0) {
        return ret;
    }
    
    /* Process each group */
    for (i = 0; i < group_count; i++) {
        if (group_name != NULL && strcmp(group_stats[i].group_name, group_name) != 0) {
            continue;
        }
        
        /* Store group name */
        strncpy(group_info->group_name, group_stats[i].group_name,
               sizeof(group_info->group_name) - 1);
        
        /* List servers in group */
        ret = tracker_list_servers(pTrackerServer, group_stats[i].group_name,
                                  NULL, storage_infos,
                                  MAX_SERVERS_PER_GROUP, &storage_count);
        if (ret != 0) {
            if (verbose) {
                fprintf(stderr, "WARNING: Failed to list servers for group %s: %s\n",
                       group_stats[i].group_name, STRERROR(ret));
            }
            continue;
        }
        
        group_info->server_count = storage_count;
        
        /* Process each server */
        for (j = 0; j < storage_count; j++) {
            ServerReplicationInfo *server_info = &group_info->servers[j];
            FDFSStorageInfo *storage_info = &storage_infos[j];
            FDFSStorageStat *stat = &storage_info->stat;
            
            /* Store server information */
            strncpy(server_info->server_id, storage_info->id, sizeof(server_info->server_id) - 1);
            strncpy(server_info->ip_addr, storage_info->ip_addr, sizeof(server_info->ip_addr) - 1);
            server_info->port = storage_info->storage_port;
            server_info->last_synced_timestamp = stat->last_synced_timestamp;
            server_info->last_sync_update = stat->last_sync_update;
            server_info->last_heartbeat = stat->last_heart_beat_time;
            server_info->total_sync_in_bytes = stat->total_sync_in_bytes;
            server_info->success_sync_in_bytes = stat->success_sync_in_bytes;
            server_info->total_sync_out_bytes = stat->total_sync_out_bytes;
            server_info->success_sync_out_bytes = stat->success_sync_out_bytes;
            
            /* Calculate sync lag */
            if (server_info->last_synced_timestamp > 0) {
                server_info->sync_lag_seconds = current_time - server_info->last_synced_timestamp;
            } else {
                server_info->sync_lag_seconds = -1;
            }
            
            /* Determine if server is syncing */
            server_info->is_syncing = (storage_info->status == FDFS_STORAGE_STATUS_SYNCING ||
                                      storage_info->status == FDFS_STORAGE_STATUS_WAIT_SYNC);
            
            /* Calculate replication status */
            int is_online = (storage_info->status == FDFS_STORAGE_STATUS_ACTIVE ||
                           storage_info->status == FDFS_STORAGE_STATUS_ONLINE);
            
            server_info->status = calculate_replication_status(
                server_info->sync_lag_seconds,
                server_info->last_synced_timestamp,
                server_info->last_sync_update,
                is_online);
            
            /* Generate status message */
            switch (server_info->status) {
                case REPLICATION_STATUS_HEALTHY:
                    snprintf(server_info->status_message, sizeof(server_info->status_message),
                            "Healthy (lag: %lld seconds)", (long long)server_info->sync_lag_seconds);
                    break;
                case REPLICATION_STATUS_LAGGING:
                    snprintf(server_info->status_message, sizeof(server_info->status_message),
                            "Lagging (lag: %lld seconds)", (long long)server_info->sync_lag_seconds);
                    break;
                case REPLICATION_STATUS_STALLED:
                    snprintf(server_info->status_message, sizeof(server_info->status_message),
                            "Stalled (no sync update in %lld seconds)",
                            (long long)(current_time - server_info->last_sync_update));
                    break;
                case REPLICATION_STATUS_FAILED:
                    snprintf(server_info->status_message, sizeof(server_info->status_message),
                            "Failed (server offline or critical lag)");
                    break;
                default:
                    snprintf(server_info->status_message, sizeof(server_info->status_message),
                            "Unknown status");
                    break;
            }
            
            /* Estimate pending sync operations */
            /* This is a rough estimate based on sync lag and throughput */
            if (server_info->sync_lag_seconds > 0 && server_info->total_sync_in_bytes > 0) {
                /* Very rough estimate: assume average file size */
                /* Estimate based on sync lag - this is approximate */
                server_info->pending_sync_operations = (int)(server_info->sync_lag_seconds / 10);
            } else {
                server_info->pending_sync_operations = 0;
            }
            
            /* Update group statistics */
            switch (server_info->status) {
                case REPLICATION_STATUS_HEALTHY:
                    group_info->healthy_servers++;
                    break;
                case REPLICATION_STATUS_LAGGING:
                    group_info->lagging_servers++;
                    break;
                case REPLICATION_STATUS_STALLED:
                    group_info->stalled_servers++;
                    break;
                case REPLICATION_STATUS_FAILED:
                    group_info->failed_servers++;
                    break;
                default:
                    break;
            }
            
            if (server_info->sync_lag_seconds > 0) {
                if (server_info->sync_lag_seconds > group_info->max_sync_lag_seconds) {
                    group_info->max_sync_lag_seconds = server_info->sync_lag_seconds;
                }
                total_lag += server_info->sync_lag_seconds;
                lag_count++;
            }
        }
        
        /* Calculate average lag */
        if (lag_count > 0) {
            group_info->avg_sync_lag_seconds = total_lag / lag_count;
        }
        
        /* Calculate total pending operations */
        for (j = 0; j < storage_count; j++) {
            group_info->total_pending_operations += group_info->servers[j].pending_sync_operations;
        }
        
        /* Calculate sync success rate */
        int64_t total_sync_bytes = 0;
        int64_t success_sync_bytes = 0;
        
        for (j = 0; j < storage_count; j++) {
            total_sync_bytes += group_info->servers[j].total_sync_in_bytes +
                               group_info->servers[j].total_sync_out_bytes;
            success_sync_bytes += group_info->servers[j].success_sync_in_bytes +
                                 group_info->servers[j].success_sync_out_bytes;
        }
        
        if (total_sync_bytes > 0) {
            group_info->sync_success_rate = (success_sync_bytes * 100.0) / total_sync_bytes;
        } else {
            group_info->sync_success_rate = 100.0;
        }
        
        /* Determine overall group status */
        if (group_info->failed_servers > 0) {
            group_info->overall_status = REPLICATION_STATUS_FAILED;
        } else if (group_info->stalled_servers > 0) {
            group_info->overall_status = REPLICATION_STATUS_STALLED;
        } else if (group_info->lagging_servers > 0) {
            group_info->overall_status = REPLICATION_STATUS_LAGGING;
        } else if (group_info->healthy_servers == group_info->server_count) {
            group_info->overall_status = REPLICATION_STATUS_HEALTHY;
        } else {
            group_info->overall_status = REPLICATION_STATUS_UNKNOWN;
        }
        
        /* Return after processing first matching group */
        if (group_name != NULL) {
            return 0;
        }
    }
    
    return 0;
}

/**
 * Print replication status in text format
 * 
 * This function prints replication status information in a
 * human-readable text format.
 * 
 * @param group_info - Group replication information
 * @param output_file - Output file (NULL for stdout)
 */
static void print_replication_status_text(GroupReplicationInfo *group_info,
                                         FILE *output_file) {
    char lag_buf[64];
    char timestamp_buf[64];
    char bytes_buf[64];
    const char *status_str;
    const char *status_symbol;
    int i, j;
    
    if (group_info == NULL || output_file == NULL) {
        return;
    }
    
    fprintf(output_file, "\n");
    fprintf(output_file, "=== FastDFS Replication Status ===\n");
    fprintf(output_file, "Group: %s\n", group_info->group_name);
    fprintf(output_file, "Check Time: %s\n", ctime(&group_info->check_time));
    fprintf(output_file, "\n");
    
    /* Overall status */
    switch (group_info->overall_status) {
        case REPLICATION_STATUS_HEALTHY:
            status_str = "HEALTHY";
            status_symbol = "✓";
            break;
        case REPLICATION_STATUS_LAGGING:
            status_str = "LAGGING";
            status_symbol = "⚠";
            break;
        case REPLICATION_STATUS_STALLED:
            status_str = "STALLED";
            status_symbol = "✗";
            break;
        case REPLICATION_STATUS_FAILED:
            status_str = "FAILED";
            status_symbol = "✗";
            break;
        default:
            status_str = "UNKNOWN";
            status_symbol = "?";
            break;
    }
    
    fprintf(output_file, "Overall Status: %s %s\n", status_symbol, status_str);
    fprintf(output_file, "\n");
    
    /* Group statistics */
    fprintf(output_file, "=== Group Statistics ===\n");
    fprintf(output_file, "Total Servers: %d\n", group_info->server_count);
    fprintf(output_file, "Healthy: %d\n", group_info->healthy_servers);
    fprintf(output_file, "Lagging: %d\n", group_info->lagging_servers);
    fprintf(output_file, "Stalled: %d\n", group_info->stalled_servers);
    fprintf(output_file, "Failed: %d\n", group_info->failed_servers);
    
    if (group_info->max_sync_lag_seconds > 0) {
        format_duration(group_info->max_sync_lag_seconds, lag_buf, sizeof(lag_buf));
        fprintf(output_file, "Max Sync Lag: %s\n", lag_buf);
    }
    
    if (group_info->avg_sync_lag_seconds > 0) {
        format_duration(group_info->avg_sync_lag_seconds, lag_buf, sizeof(lag_buf));
        fprintf(output_file, "Avg Sync Lag: %s\n", lag_buf);
    }
    
    fprintf(output_file, "Total Pending Operations: %d\n", group_info->total_pending_operations);
    fprintf(output_file, "Sync Success Rate: %.2f%%\n", group_info->sync_success_rate);
    fprintf(output_file, "\n");
    
    /* Server details */
    fprintf(output_file, "=== Server Replication Status ===\n");
    fprintf(output_file, "\n");
    
    for (i = 0; i < group_info->server_count; i++) {
        ServerReplicationInfo *server = &group_info->servers[i];
        
        /* Skip healthy servers in quiet mode */
        if (quiet && server->status == REPLICATION_STATUS_HEALTHY) {
            continue;
        }
        
        fprintf(output_file, "Server: %s (%s:%d)\n",
               server->server_id, server->ip_addr, server->port);
        
        /* Status */
        switch (server->status) {
            case REPLICATION_STATUS_HEALTHY:
                status_str = "HEALTHY";
                status_symbol = "✓";
                break;
            case REPLICATION_STATUS_LAGGING:
                status_str = "LAGGING";
                status_symbol = "⚠";
                break;
            case REPLICATION_STATUS_STALLED:
                status_str = "STALLED";
                status_symbol = "✗";
                break;
            case REPLICATION_STATUS_FAILED:
                status_str = "FAILED";
                status_symbol = "✗";
                break;
            default:
                status_str = "UNKNOWN";
                status_symbol = "?";
                break;
        }
        
        fprintf(output_file, "  Status: %s %s\n", status_symbol, status_str);
        fprintf(output_file, "  %s\n", server->status_message);
        
        if (verbose) {
            /* Last synced timestamp */
            format_timestamp(server->last_synced_timestamp, timestamp_buf, sizeof(timestamp_buf));
            fprintf(output_file, "  Last Synced: %s\n", timestamp_buf);
            
            /* Sync lag */
            if (server->sync_lag_seconds >= 0) {
                format_duration(server->sync_lag_seconds, lag_buf, sizeof(lag_buf));
                fprintf(output_file, "  Sync Lag: %s\n", lag_buf);
            } else {
                fprintf(output_file, "  Sync Lag: Unknown\n");
            }
            
            /* Last sync update */
            format_timestamp(server->last_sync_update, timestamp_buf, sizeof(timestamp_buf));
            fprintf(output_file, "  Last Sync Update: %s\n", timestamp_buf);
            
            /* Last heartbeat */
            format_timestamp(server->last_heartbeat, timestamp_buf, sizeof(timestamp_buf));
            fprintf(output_file, "  Last Heartbeat: %s\n", timestamp_buf);
            
            /* Sync statistics */
            format_bytes(server->total_sync_in_bytes, bytes_buf, sizeof(bytes_buf));
            fprintf(output_file, "  Total Sync In: %s\n", bytes_buf);
            
            format_bytes(server->success_sync_in_bytes, bytes_buf, sizeof(bytes_buf));
            fprintf(output_file, "  Success Sync In: %s\n", bytes_buf);
            
            format_bytes(server->total_sync_out_bytes, bytes_buf, sizeof(bytes_buf));
            fprintf(output_file, "  Total Sync Out: %s\n", bytes_buf);
            
            format_bytes(server->success_sync_out_bytes, bytes_buf, sizeof(bytes_buf));
            fprintf(output_file, "  Success Sync Out: %s\n", bytes_buf);
            
            /* Sync success rates */
            if (server->total_sync_in_bytes > 0) {
                double in_rate = (server->success_sync_in_bytes * 100.0) /
                                server->total_sync_in_bytes;
                fprintf(output_file, "  Sync In Success Rate: %.2f%%\n", in_rate);
            }
            
            if (server->total_sync_out_bytes > 0) {
                double out_rate = (server->success_sync_out_bytes * 100.0) /
                                 server->total_sync_out_bytes;
                fprintf(output_file, "  Sync Out Success Rate: %.2f%%\n", out_rate);
            }
            
            /* Pending operations */
            fprintf(output_file, "  Pending Sync Operations: %d\n",
                   server->pending_sync_operations);
            
            /* Syncing status */
            fprintf(output_file, "  Currently Syncing: %s\n",
                   server->is_syncing ? "Yes" : "No");
        }
        
        fprintf(output_file, "\n");
    }
    
    /* Summary */
    fprintf(output_file, "=== Summary ===\n");
    if (group_info->overall_status == REPLICATION_STATUS_HEALTHY) {
        fprintf(output_file, "✓ Replication is healthy across all servers\n");
    } else if (group_info->overall_status == REPLICATION_STATUS_LAGGING) {
        fprintf(output_file, "⚠ WARNING: Some servers are lagging in replication\n");
    } else if (group_info->overall_status == REPLICATION_STATUS_STALLED) {
        fprintf(output_file, "✗ CRITICAL: Replication appears stalled on some servers\n");
    } else if (group_info->overall_status == REPLICATION_STATUS_FAILED) {
        fprintf(output_file, "✗ CRITICAL: Replication failures detected\n");
    }
    fprintf(output_file, "\n");
}

/**
 * Print replication status in JSON format
 * 
 * This function prints replication status information in JSON
 * format for programmatic processing.
 * 
 * @param group_info - Group replication information
 * @param output_file - Output file (NULL for stdout)
 */
static void print_replication_status_json(GroupReplicationInfo *group_info,
                                         FILE *output_file) {
    int i;
    
    if (group_info == NULL || output_file == NULL) {
        return;
    }
    
    fprintf(output_file, "{\n");
    fprintf(output_file, "  \"timestamp\": %ld,\n", (long)group_info->check_time);
    fprintf(output_file, "  \"group_name\": \"%s\",\n", group_info->group_name);
    fprintf(output_file, "  \"overall_status\": \"%s\",\n",
           group_info->overall_status == REPLICATION_STATUS_HEALTHY ? "healthy" :
           group_info->overall_status == REPLICATION_STATUS_LAGGING ? "lagging" :
           group_info->overall_status == REPLICATION_STATUS_STALLED ? "stalled" :
           group_info->overall_status == REPLICATION_STATUS_FAILED ? "failed" : "unknown");
    fprintf(output_file, "  \"statistics\": {\n");
    fprintf(output_file, "    \"total_servers\": %d,\n", group_info->server_count);
    fprintf(output_file, "    \"healthy_servers\": %d,\n", group_info->healthy_servers);
    fprintf(output_file, "    \"lagging_servers\": %d,\n", group_info->lagging_servers);
    fprintf(output_file, "    \"stalled_servers\": %d,\n", group_info->stalled_servers);
    fprintf(output_file, "    \"failed_servers\": %d,\n", group_info->failed_servers);
    fprintf(output_file, "    \"max_sync_lag_seconds\": %lld,\n",
           (long long)group_info->max_sync_lag_seconds);
    fprintf(output_file, "    \"avg_sync_lag_seconds\": %lld,\n",
           (long long)group_info->avg_sync_lag_seconds);
    fprintf(output_file, "    \"total_pending_operations\": %d,\n",
           group_info->total_pending_operations);
    fprintf(output_file, "    \"sync_success_rate\": %.2f\n", group_info->sync_success_rate);
    fprintf(output_file, "  },\n");
    fprintf(output_file, "  \"servers\": [\n");
    
    for (i = 0; i < group_info->server_count; i++) {
        ServerReplicationInfo *server = &group_info->servers[i];
        
        if (i > 0) {
            fprintf(output_file, ",\n");
        }
        
        fprintf(output_file, "    {\n");
        fprintf(output_file, "      \"server_id\": \"%s\",\n", server->server_id);
        fprintf(output_file, "      \"ip_addr\": \"%s\",\n", server->ip_addr);
        fprintf(output_file, "      \"port\": %d,\n", server->port);
        fprintf(output_file, "      \"status\": \"%s\",\n",
               server->status == REPLICATION_STATUS_HEALTHY ? "healthy" :
               server->status == REPLICATION_STATUS_LAGGING ? "lagging" :
               server->status == REPLICATION_STATUS_STALLED ? "stalled" :
               server->status == REPLICATION_STATUS_FAILED ? "failed" : "unknown");
        fprintf(output_file, "      \"status_message\": \"%s\",\n", server->status_message);
        fprintf(output_file, "      \"last_synced_timestamp\": %ld,\n",
               (long)server->last_synced_timestamp);
        fprintf(output_file, "      \"last_sync_update\": %ld,\n",
               (long)server->last_sync_update);
        fprintf(output_file, "      \"last_heartbeat\": %ld,\n",
               (long)server->last_heartbeat);
        fprintf(output_file, "      \"sync_lag_seconds\": %lld,\n",
               (long long)server->sync_lag_seconds);
        fprintf(output_file, "      \"total_sync_in_bytes\": %lld,\n",
               (long long)server->total_sync_in_bytes);
        fprintf(output_file, "      \"success_sync_in_bytes\": %lld,\n",
               (long long)server->success_sync_in_bytes);
        fprintf(output_file, "      \"total_sync_out_bytes\": %lld,\n",
               (long long)server->total_sync_out_bytes);
        fprintf(output_file, "      \"success_sync_out_bytes\": %lld,\n",
               (long long)server->success_sync_out_bytes);
        fprintf(output_file, "      \"pending_sync_operations\": %d,\n",
               server->pending_sync_operations);
        fprintf(output_file, "      \"is_syncing\": %s\n",
               server->is_syncing ? "true" : "false");
        fprintf(output_file, "    }");
    }
    
    fprintf(output_file, "\n  ]\n");
    fprintf(output_file, "}\n");
}

/**
 * Main function
 * 
 * Entry point for the replication status checker tool. Parses command-line
 * arguments and performs replication status monitoring.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = healthy, 1 = issues, 2 = critical failures)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *group_name = NULL;
    char *output_file = NULL;
    int result;
    ConnectionInfo *pTrackerServer;
    GroupReplicationInfo group_info;
    FILE *out_fp = stdout;
    int exit_code = 0;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"group", required_argument, 0, 'g'},
        {"watch", no_argument, 0, 'w'},
        {"interval", required_argument, 0, 'i'},
        {"lag-warning", required_argument, 0, 1000},
        {"lag-critical", required_argument, 0, 1001},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"json", no_argument, 0, 'J'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:g:wi:o:vqJh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'g':
                group_name = optarg;
                break;
            case 'w':
                watch_mode = 1;
                break;
            case 'i':
                watch_interval = atoi(optarg);
                if (watch_interval < 1) watch_interval = 1;
                break;
            case 1000:
                lag_warning_threshold = atoll(optarg);
                if (lag_warning_threshold < 0) lag_warning_threshold = 300;
                break;
            case 1001:
                lag_critical_threshold = atoll(optarg);
                if (lag_critical_threshold < 0) lag_critical_threshold = 3600;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'J':
                json_output = 1;
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
    
    /* Open output file if specified */
    if (output_file != NULL && !watch_mode) {
        out_fp = fopen(output_file, "w");
        if (out_fp == NULL) {
            fprintf(stderr, "ERROR: Failed to open output file: %s\n", output_file);
            out_fp = stdout;
        }
    }
    
    /* Monitor replication status */
    do {
        if (watch_mode && !json_output) {
            system("clear");
        }
        
        /* Get replication status */
        result = get_group_replication_status(pTrackerServer, group_name, &group_info);
        if (result != 0) {
            fprintf(stderr, "ERROR: Failed to get replication status: %s\n", STRERROR(result));
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            if (output_file != NULL && out_fp != stdout) {
                fclose(out_fp);
            }
            return 2;
        }
        
        /* Print results */
        if (json_output) {
            print_replication_status_json(&group_info, out_fp);
        } else {
            print_replication_status_text(&group_info, out_fp);
        }
        
        /* Determine exit code */
        if (group_info.overall_status == REPLICATION_STATUS_FAILED) {
            exit_code = 2;  /* Critical failure */
        } else if (group_info.overall_status == REPLICATION_STATUS_STALLED ||
                   group_info.overall_status == REPLICATION_STATUS_LAGGING) {
            exit_code = 1;  /* Issues detected */
        } else {
            exit_code = 0;  /* Healthy */
        }
        
        if (watch_mode) {
            if (!json_output) {
                printf("Press Ctrl+C to exit. Refreshing in %d seconds...\n", watch_interval);
            }
            sleep(watch_interval);
        }
    } while (watch_mode);
    
    /* Close output file if opened */
    if (output_file != NULL && out_fp != stdout) {
        fclose(out_fp);
    }
    
    /* Disconnect from tracker */
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return exit_code;
}

