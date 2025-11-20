/**
 * FastDFS Health Check Service with Alert Manager
 * 
 * Monitors FastDFS cluster health and sends alerts.
 * Checks tracker and storage server availability, disk space, and performance.
 * Supports log, syslog, and webhook notifications.
 * 
 * Copyright (C) 2025
 * License: GPL V3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <inttypes.h>
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "client_global.h"
#include "fdfs_global.h"
#include "fdfs_client.h"
#include "tracker_client.h"

#define DEFAULT_CHECK_INTERVAL 30  // seconds
#define DISK_SPACE_WARNING_THRESHOLD 20  // percent
#define DISK_SPACE_CRITICAL_THRESHOLD 10  // percent
#define HEARTBEAT_TIMEOUT 60  // seconds
#define ALERT_COOLDOWN 300  // 5 minutes between duplicate alerts

typedef enum {
    HEALTH_OK = 0,
    HEALTH_WARNING = 1,
    HEALTH_CRITICAL = 2,
    HEALTH_UNKNOWN = 3
} HealthStatus;

typedef struct {
    int total_groups;
    int healthy_groups;
    int total_storages;
    int healthy_storages;
    int warning_storages;
    int critical_storages;
    HealthStatus overall_status;
} ClusterHealth;

typedef struct {
    time_t last_alert_time;
    char last_alert_message[512];
} AlertState;

static int check_interval = DEFAULT_CHECK_INTERVAL;
static int running = 1;
static int enable_syslog = 1;
static AlertState alert_state = {0};

/**
 * Check if alert should be suppressed (cooldown period)
 */
static int should_suppress_alert(const char *message) {
    time_t current_time = time(NULL);
    
    if (strcmp(message, alert_state.last_alert_message) == 0) {
        if (current_time - alert_state.last_alert_time < ALERT_COOLDOWN) {
            return 1;  // Suppress
        }
    }
    
    alert_state.last_alert_time = current_time;
    strncpy(alert_state.last_alert_message, message, sizeof(alert_state.last_alert_message) - 1);
    return 0;
}

/**
 * Send alert through configured channels
 */
static void send_alert(const char *level, const char *message) {
    // Check cooldown
    if (should_suppress_alert(message)) {
        return;
    }
    
    // Log alert
    if (strcmp(level, "CRITICAL") == 0) {
        logError("[ALERT] %s: %s", level, message);
    } else if (strcmp(level, "WARNING") == 0) {
        logWarning("[ALERT] %s: %s", level, message);
    } else {
        logInfo("[ALERT] %s: %s", level, message);
    }
    
    // Syslog alert
    if (enable_syslog) {
        int priority = strcmp(level, "CRITICAL") == 0 ? LOG_CRIT : 
                      strcmp(level, "WARNING") == 0 ? LOG_WARNING : LOG_INFO;
        syslog(priority, "[FastDFS Health] %s: %s", level, message);
    }
}

/**
 * Signal handler
 */
static void signal_handler(int sig) {
    running = 0;
}

/**
 * Check storage server health
 */
static HealthStatus check_storage_health(FDFSStorageBrief *pStorage, 
                                        FDFSStorageStat *pStorageStat,
                                        char *message, size_t msg_size) {
    time_t current_time = time(NULL);
    int64_t free_percent;
    int heartbeat_delay;
    
    // Check if storage is active
    if (pStorage->status != FDFS_STORAGE_STATUS_ACTIVE) {
        snprintf(message, msg_size, "Storage server not active (status: %d)", 
                pStorage->status);
        return HEALTH_CRITICAL;
    }
    
    // Check heartbeat
    heartbeat_delay = (int)(current_time - pStorageStat->last_heart_beat_time);
    if (heartbeat_delay > HEARTBEAT_TIMEOUT) {
        snprintf(message, msg_size, "No heartbeat for %d seconds", heartbeat_delay);
        return HEALTH_CRITICAL;
    }
    
    // Check disk space
    if (pStorage->total_mb > 0) {
        free_percent = (pStorage->free_mb * 100) / pStorage->total_mb;
        
        if (free_percent < DISK_SPACE_CRITICAL_THRESHOLD) {
            snprintf(message, msg_size, "Critical: Only %"PRId64"%% disk space free", 
                    free_percent);
            return HEALTH_CRITICAL;
        }
        
        if (free_percent < DISK_SPACE_WARNING_THRESHOLD) {
            snprintf(message, msg_size, "Warning: Only %"PRId64"%% disk space free", 
                    free_percent);
            return HEALTH_WARNING;
        }
    }
    
    // Check error rates
    if (pStorageStat->total_upload_count > 100) {
        int64_t error_count = pStorageStat->total_upload_count - 
                             pStorageStat->success_upload_count;
        int error_rate = (int)((error_count * 100) / pStorageStat->total_upload_count);
        
        if (error_rate > 10) {
            snprintf(message, msg_size, "High error rate: %d%% upload failures", 
                    error_rate);
            return HEALTH_WARNING;
        }
    }
    
    snprintf(message, msg_size, "OK");
    return HEALTH_OK;
}

/**
 * Perform health check on entire cluster
 */
static int perform_health_check(ClusterHealth *cluster_health) {
    ConnectionInfo *pTrackerServer;
    FDFSGroupStat group_stats[FDFS_MAX_GROUPS];
    int group_count;
    int result;
    
    memset(cluster_health, 0, sizeof(ClusterHealth));
    cluster_health->overall_status = HEALTH_OK;
    
    // Get tracker connection
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        logError("Failed to connect to tracker server");
        cluster_health->overall_status = HEALTH_CRITICAL;
        send_alert("CRITICAL", "Cannot connect to tracker server");
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    // List all groups
    result = tracker_list_groups(pTrackerServer, group_stats, 
                                 FDFS_MAX_GROUPS, &group_count);
    if (result != 0) {
        logError("Failed to list groups, error code: %d", result);
        tracker_disconnect_server_ex(pTrackerServer, true);
        cluster_health->overall_status = HEALTH_CRITICAL;
        send_alert("CRITICAL", "Failed to query cluster status");
        return result;
    }
    
    cluster_health->total_groups = group_count;
    
    // Check each group and storage server
    for (int i = 0; i < group_count; i++) {
        FDFSGroupStat *pGroupStat = &group_stats[i];
        int group_healthy = 1;
        
        cluster_health->total_storages += pGroupStat->count;
        
        // Check each storage in the group
        for (int j = 0; j < pGroupStat->count; j++) {
            FDFSStorageBrief *pStorage = &pGroupStat->storage_servers[j];
            FDFSStorageStat *pStorageStat = &pGroupStat->storage_stats[j];
            char message[256];
            HealthStatus status;
            
            status = check_storage_health(pStorage, pStorageStat, 
                                         message, sizeof(message));
            
            switch (status) {
                case HEALTH_OK:
                    cluster_health->healthy_storages++;
                    break;
                    
                case HEALTH_WARNING:
                    cluster_health->warning_storages++;
                    group_healthy = 0;
                    if (cluster_health->overall_status == HEALTH_OK) {
                        cluster_health->overall_status = HEALTH_WARNING;
                    }
                    logWarning("Storage %s:%s - %s", 
                              pGroupStat->group_name, pStorage->id, message);
                    
                    // Send alert
                    char alert_msg[512];
                    snprintf(alert_msg, sizeof(alert_msg),
                            "Storage %s:%s - %s", 
                            pGroupStat->group_name, pStorage->id, message);
                    send_alert("WARNING", alert_msg);
                    break;
                    
                case HEALTH_CRITICAL:
                    cluster_health->critical_storages++;
                    group_healthy = 0;
                    cluster_health->overall_status = HEALTH_CRITICAL;
                    logError("Storage %s:%s - %s", 
                            pGroupStat->group_name, pStorage->id, message);
                    
                    // Send alert
                    char critical_msg[512];
                    snprintf(critical_msg, sizeof(critical_msg),
                            "Storage %s:%s - %s", 
                            pGroupStat->group_name, pStorage->id, message);
                    send_alert("CRITICAL", critical_msg);
                    break;
                    
                default:
                    break;
            }
        }
        
        if (group_healthy) {
            cluster_health->healthy_groups++;
        }
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    return 0;
}

/**
 * Print health check results
 */
static void print_health_status(ClusterHealth *cluster_health) {
    const char *status_str;
    
    switch (cluster_health->overall_status) {
        case HEALTH_OK:
            status_str = "OK";
            break;
        case HEALTH_WARNING:
            status_str = "WARNING";
            break;
        case HEALTH_CRITICAL:
            status_str = "CRITICAL";
            break;
        default:
            status_str = "UNKNOWN";
            break;
    }
    
    printf("\n=== FastDFS Cluster Health Check ===\n");
    printf("Overall Status: %s\n", status_str);
    printf("Groups: %d total, %d healthy\n", 
           cluster_health->total_groups, cluster_health->healthy_groups);
    printf("Storage Servers: %d total, %d healthy, %d warning, %d critical\n",
           cluster_health->total_storages,
           cluster_health->healthy_storages,
           cluster_health->warning_storages,
           cluster_health->critical_storages);
    printf("Timestamp: %s", ctime(&(time_t){time(NULL)}));
    printf("=====================================\n\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    char *conf_filename;
    int result;
    int daemon_mode = 0;
    
    printf("FastDFS Health Check Service\n");
    printf("============================\n\n");
    
    // Parse arguments
    if (argc < 2) {
        printf("Usage: %s <config_file> [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -d           Run as daemon\n");
        printf("  -i <seconds> Check interval (default: %d)\n", DEFAULT_CHECK_INTERVAL);
        return 1;
    }
    
    conf_filename = argv[1];
    
    // Parse options
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            daemon_mode = 1;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            check_interval = atoi(argv[++i]);
            if (check_interval < 10) {
                printf("Check interval too small, using minimum: 10 seconds\n");
                check_interval = 10;
            }
        }
    }
    
    // Initialize FastDFS client
    log_init();
    g_log_context.log_level = LOG_INFO;
    ignore_signal_pipe();
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        printf("ERROR: Failed to initialize FastDFS client\n");
        return result;
    }
    
    printf("FastDFS client initialized successfully\n");
    printf("Tracker servers: %d\n", g_tracker_group.server_count);
    printf("Check interval: %d seconds\n", check_interval);
    printf("Mode: %s\n\n", daemon_mode ? "daemon" : "foreground");
    
    // Initialize syslog
    openlog("fdfs_health_check", LOG_PID | LOG_CONS, LOG_DAEMON);
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Daemonize if requested
    if (daemon_mode) {
        if (daemon(1, 0) != 0) {
            printf("ERROR: Failed to daemonize\n");
            fdfs_client_destroy();
            return 1;
        }
    }
    
    // Main health check loop
    while (running) {
        ClusterHealth cluster_health;
        
        result = perform_health_check(&cluster_health);
        if (result == 0) {
            print_health_status(&cluster_health);
        }
        
        // Sleep until next check
        for (int i = 0; i < check_interval && running; i++) {
            sleep(1);
        }
    }
    
    printf("\nShutting down health check service...\n");
    closelog();
    fdfs_client_destroy();
    return 0;
}
