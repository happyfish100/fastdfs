/**
 * FastDFS Prometheus Exporter
 * 
 * Exposes FastDFS metrics in Prometheus format for monitoring and alerting.
 * Provides comprehensive metrics for TPS, traffic, storage, and node status.
 * 
 * Copyright (C) 2025
 * License: GPL V3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "client_global.h"
#include "fdfs_global.h"
#include "fdfs_client.h"

#define DEFAULT_PORT 9898
#define MAX_RESPONSE_SIZE (1024 * 1024)  // 1MB
#define METRIC_PREFIX "fastdfs_"

// Global variables
static int listen_port = DEFAULT_PORT;
static int server_socket = -1;
static char *server_response = NULL;
static ConnectionInfo *pTrackerServer = NULL;
static TrackerStatFilter filter = {0, 0, 0};

/**
 * Format metric name for Prometheus
 */
static void format_metric_name(char *buffer, size_t size, 
                               const char *metric, const char *type) {
    snprintf(buffer, size, "%s%s_%s", METRIC_PREFIX, metric, type);
}

/**
 * Append metric to response buffer
 */
static int append_metric_ex(char *response, size_t *offset, size_t max_size,
                        const char *name, const char *labels,
                        const char *value, const char *help,
                        const bool add_comment)
{
    int written = 0;

    if (add_comment) {
        // Add HELP comment
        if (help != NULL) {
            written = snprintf(response + *offset, max_size - *offset,
                    "# HELP %s %s\n", name, help);
            if (written < 0 || *offset + written >= max_size) return -1;
            *offset += written;
        }

        // Add TYPE comment
        written = snprintf(response + *offset, max_size - *offset,
                "# TYPE %s gauge\n", name);
        if (written < 0 || *offset + written >= max_size) return -1;
        *offset += written;
    }

    // Add metric value
    if (labels != NULL && strlen(labels) > 0) {
        written = snprintf(response + *offset, max_size - *offset,
                          "%s{%s} %s\n", name, labels, value);
    } else {
        written = snprintf(response + *offset, max_size - *offset,
                          "%s %s\n", name, value);
    }

    if (written < 0 || *offset + written >= max_size) return -1;
    *offset += written;

    return 0;
}

static inline int append_metric(char *response, size_t *offset,
        size_t max_size, const char *name, const char *labels,
        const char *value, const char *help)
{
    const bool add_comment = true;
    return append_metric_ex(response, offset, max_size, name,
            labels, value, help, add_comment);
}

/**
 * Export storage-level metrics
 */
static int export_tracker_metrics(char *response, size_t *offset,
        size_t max_size, const FDFSTrackerStat *pTracker,
        const bool add_comment)
{
    char metric_name[256];
    char labels[512];
    char value[64];

    // Tracker labels
    snprintf(labels, sizeof(labels), "host=\"%s\"", pTracker->host);

    format_metric_name(metric_name, sizeof(metric_name), "tracker", "is_leader");
    snprintf(value, sizeof(value), "%d", pTracker->is_leader);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Is leader", add_comment) != 0) return -1;

    format_metric_name(metric_name, sizeof(metric_name), "tracker", "is_active");
    snprintf(value, sizeof(value), "%d", pTracker->is_active);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Is active", add_comment) != 0) return -1;

    // === Connection Metrics ===
    format_metric_name(metric_name, sizeof(metric_name), "tracker", "connections_alloc");
    snprintf(value, sizeof(value), "%d", pTracker->connection.alloc_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Alloc connection count", add_comment) != 0) return -1;

    format_metric_name(metric_name, sizeof(metric_name), "tracker", "connections_current");
    snprintf(value, sizeof(value), "%d", pTracker->connection.current_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Current connection count", add_comment) != 0) return -1;

    format_metric_name(metric_name, sizeof(metric_name), "tracker", "connections_max");
    snprintf(value, sizeof(value), "%d", pTracker->connection.max_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Maximum connection count", add_comment) != 0) return -1;

    return 0;
}

/**
 * Export group-level metrics
 */
static int export_group_metrics(char *response, size_t *offset,
        size_t max_size, FDFSGroupStat *pGroupStat)
{
    char metric_name[256];
    char labels[512];
    char value[64];
    int64_t avail_space;

    // Group label
    snprintf(labels, sizeof(labels), "group=\"%s\"", pGroupStat->group_name);

    // Total space
    format_metric_name(metric_name, sizeof(metric_name), "group", "total_mb");
    snprintf(value, sizeof(value), "%"PRId64, pGroupStat->total_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Disk total space in MB") != 0) return -1;

    // Free space
    format_metric_name(metric_name, sizeof(metric_name), "group", "free_mb");
    snprintf(value, sizeof(value), "%"PRId64, pGroupStat->free_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Disk free space in MB") != 0) return -1;

    // Reserved space
    format_metric_name(metric_name, sizeof(metric_name), "group", "reserved_mb");
    snprintf(value, sizeof(value), "%"PRId64, pGroupStat->reserved_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Disk reserved space in MB") != 0) return -1;

    avail_space = pGroupStat->free_mb - pGroupStat->reserved_mb;
    if (avail_space < 0) {
        avail_space = 0;
    }
    // Available space
    format_metric_name(metric_name, sizeof(metric_name), "group", "available_mb");
    snprintf(value, sizeof(value), "%"PRId64, avail_space);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Disk available space in MB") != 0) return -1;

    // Trunk free space
    if (pGroupStat->current_trunk_file_id >= 0) { //use trunk file
        format_metric_name(metric_name, sizeof(metric_name), "group", "trunk_free_mb");
        snprintf(value, sizeof(value), "%"PRId64, pGroupStat->trunk_free_mb);
        if (append_metric(response, offset, max_size, metric_name, labels, value,
                    "Trunk free space in MB") != 0) return -1;
    }

    // Storage server count
    format_metric_name(metric_name, sizeof(metric_name), "group", "storage_count");
    snprintf(value, sizeof(value), "%d", pGroupStat->storage_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Number of storage servers in group") != 0) return -1;
    
    // Readable server count
    format_metric_name(metric_name, sizeof(metric_name), "group", "readable_server_count");
    snprintf(value, sizeof(value), "%d", pGroupStat->readable_server_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Number of readable storage servers") != 0) return -1;

    // Writable server count
    format_metric_name(metric_name, sizeof(metric_name), "group", "writable_server_count");
    snprintf(value, sizeof(value), "%d", pGroupStat->writable_server_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Number of writable storage servers") != 0) return -1;
    
    return 0;
}

/**
 * Export storage-level metrics
 */
static int export_storage_metrics(char *response, size_t *offset,
        size_t max_size, const char *group_name,
        const FDFSStorageInfo *pStorage,
        const FDFSStorageStat *pStorageStat,
        const time_t max_last_source_update,
        const bool add_comment)
{
    char metric_name[256];
    char labels[512];
    char value[64];
    long delay_seconds;
    int64_t avail_space;
    time_t current_time = time(NULL);

    // Storage labels
    snprintf(labels, sizeof(labels), 
            "group=\"%s\",storage_id=\"%s\",ip=\"%s\",status=\"%d\"",
            group_name, pStorage->id, pStorage->ip_addr, pStorage->status);

    // === Storage Space Metrics ===

    format_metric_name(metric_name, sizeof(metric_name), "storage", "total_mb");
    snprintf(value, sizeof(value), "%"PRId64, pStorage->total_mb);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Disk total space in MB", add_comment) != 0) return -1;

    format_metric_name(metric_name, sizeof(metric_name), "storage", "free_mb");
    snprintf(value, sizeof(value), "%"PRId64, pStorage->free_mb);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Disk free space in MB", add_comment) != 0) return -1;

    format_metric_name(metric_name, sizeof(metric_name), "storage", "reserved_mb");
    snprintf(value, sizeof(value), "%"PRId64, pStorage->reserved_mb);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Disk reserved space in MB", add_comment) != 0) return -1;

    avail_space = pStorage->free_mb - pStorage->reserved_mb;
    if (avail_space < 0) {
        avail_space = 0;
    }
    format_metric_name(metric_name, sizeof(metric_name), "storage", "available_mb");
    snprintf(value, sizeof(value), "%"PRId64, avail_space);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Disk available space in MB", add_comment) != 0) return -1;

    // === Upload Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "upload_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_upload_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total upload operations", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "upload_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_upload_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Successful upload operations", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "upload_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_upload_bytes);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total uploaded bytes", add_comment) != 0) return -1;
    
    // === Download Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "download_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_download_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total download operations", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "download_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_download_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Successful download operations", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "download_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_download_bytes);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total downloaded bytes", add_comment) != 0) return -1;
    
    // === Delete Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "delete_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_delete_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total delete operations", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "delete_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_delete_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Successful delete operations", add_comment) != 0) return -1;
    
    // === Append Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "append_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_append_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total append operations", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "append_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_append_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Successful append operations", add_comment) != 0) return -1;
    
    // === Modify Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "modify_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_modify_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total modify operations", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "modify_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_modify_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Successful modify operations", add_comment) != 0) return -1;

    // === Connection Metrics ===

    format_metric_name(metric_name, sizeof(metric_name), "storage", "connections_alloc");
    snprintf(value, sizeof(value), "%d", pStorageStat->connection.alloc_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Alloc connection count", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "connections_current");
    snprintf(value, sizeof(value), "%d", pStorageStat->connection.current_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Current connection count", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "connections_max");
    snprintf(value, sizeof(value), "%d", pStorageStat->connection.max_count);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Maximum connection count", add_comment) != 0) return -1;
    
    // === Heartbeat Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "last_heartbeat");
    snprintf(value, sizeof(value), "%ld", (long)pStorageStat->last_heart_beat_time);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Last heartbeat timestamp", add_comment) != 0) return -1;

    // Heartbeat delay
    format_metric_name(metric_name, sizeof(metric_name), "storage", "heartbeat_delay_seconds");
    snprintf(value, sizeof(value), "%ld", 
            (long)(current_time - pStorageStat->last_heart_beat_time));
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Seconds since last heartbeat", add_comment) != 0) return -1;

    // === Sync Metrics ===

    format_metric_name(metric_name, sizeof(metric_name),
            "storage", "last_synced_timestamp");
    snprintf(value, sizeof(value), "%ld",
            (long)pStorageStat->last_synced_timestamp);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Last synced timestamp", add_comment) != 0) return -1;

    // Sync delay seconds
    format_metric_name(metric_name, sizeof(metric_name),
            "storage", "synced_delay_seconds");
    if (max_last_source_update == 0) {
        delay_seconds = 0;
    } else {
        if (pStorageStat->last_synced_timestamp > 0) {
            delay_seconds = max_last_source_update -
                pStorageStat->last_synced_timestamp;
            if (delay_seconds < 0) {
                delay_seconds = 0;
            }
        } else {
            delay_seconds = -1;  //never synced
        }
    }
    snprintf(value, sizeof(value), "%ld", delay_seconds);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Synced delay seconds", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "sync_in_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_sync_in_bytes);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total sync in bytes", add_comment) != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "sync_out_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_sync_out_bytes);
    if (append_metric_ex(response, offset, max_size, metric_name, labels, value,
                     "Total sync out bytes", add_comment) != 0) return -1;
    
    return 0;
}

static int collect_tracker_metrics(char *response, size_t *offset, size_t max_size)
{
    FDFSTrackerStat tracker_infos[FDFS_MAX_TRACKERS];
    FDFSTrackerStat *tracker;
    FDFSTrackerStat *end;
    int tracker_count;
    int result;

    if ((result=tracker_cluster_stat(pTrackerServer, &filter, tracker_infos,
                    FDFS_MAX_TRACKERS, &tracker_count)) != 0)
    {
        return result;
    }

    end = tracker_infos + tracker_count;
    for (tracker=tracker_infos; tracker<end; tracker++)
    {
        if ((result=export_tracker_metrics(response, offset, max_size,
                        tracker, tracker == tracker_infos)) != 0)
        {
            return result;
        }
    }

    return 0;
}

/**
 * Collect all metrics from FastDFS
 */
static int collect_metrics(char *response, size_t max_size, int *length)
{
    int result;
    int group_count;
    int storage_count;
    FDFSGroupStat group_stats[FDFS_MAX_GROUPS];
    FDFSGroupStat *pGroupStat;
    FDFSGroupStat *pGroupEnd;
    FDFSStorageInfo storage_infos[FDFS_MAX_SERVERS_EACH_GROUP];
    FDFSStorageInfo *ps;
    FDFSStorageInfo *pStorage;
    FDFSStorageInfo *pStorageEnd;
    time_t max_last_source_update;
    size_t offset = 0;
    
    // Get tracker connection
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        return errno != 0 ? errno : ECONNREFUSED;
    }

    if ((result=collect_tracker_metrics(response, &offset, max_size)) != 0) {
        conn_pool_disconnect_server(pTrackerServer);
        return result;
    }

    // List all groups
    result = tracker_list_groups(pTrackerServer, group_stats,
                                 FDFS_MAX_GROUPS, &group_count);
    if (result != 0) {
        conn_pool_disconnect_server(pTrackerServer);
        return result;
    }
    
    // Export metrics for each group
    pGroupEnd = group_stats + group_count;
    for (pGroupStat = group_stats; pGroupStat < pGroupEnd; pGroupStat++) {
        // Export group metrics
        if (export_group_metrics(response, &offset, max_size, pGroupStat) != 0) {
            conn_pool_disconnect_server(pTrackerServer);
            return -1;
        }

        if ((result=tracker_list_servers(pTrackerServer, pGroupStat->group_name,
                        NULL, storage_infos, FDFS_MAX_SERVERS_EACH_GROUP,
                        &storage_count)) != 0)
        {
            conn_pool_disconnect_server(pTrackerServer);
            return result;
        }

        // Export storage metrics for each server in group
        pStorageEnd = storage_infos + storage_count;
        for (pStorage = storage_infos; pStorage < pStorageEnd; pStorage++) {
            max_last_source_update = 0;
            for (ps = storage_infos; ps < pStorageEnd; ps++) {
                if (ps != pStorage && ps->stat.last_source_update
                        > max_last_source_update)
                {
                    max_last_source_update = ps->stat.last_source_update;
                }
            }

            if (export_storage_metrics(response, &offset, max_size,
                        pGroupStat->group_name, pStorage, &pStorage->stat,
                        max_last_source_update, pStorage == storage_infos) != 0)
            {
                conn_pool_disconnect_server(pTrackerServer);
                return -1;
            }
        }
    }

    *length = offset;
    conn_pool_disconnect_server(pTrackerServer);
    return 0;
}

/**
 * Handle HTTP request
 */
static void handle_request(int client_socket)
{
    char request[4096];
    char http_header[512];
    int header_length;
    int body_length = 0;
    int bytes_read;
    int result;
    
    // Read request
    bytes_read = recv(client_socket, request, sizeof(request) - 1, 0);
    if (bytes_read <= 0) {
        return;
    }
    request[bytes_read] = '\0';

    // Check if it's a GET request for /metrics
    if (strncmp(request, "GET /metrics", 12) != 0) {
        const char *not_found = "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n\r\n"
            "Not Found";
        send(client_socket, not_found, strlen(not_found), 0);
        return;
    }


    // Collect metrics
    result = collect_metrics(server_response, MAX_RESPONSE_SIZE, &body_length);
    if (result != 0) {
        const char *error = "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 21\r\n\r\n"
            "Internal Server Error";
        send(client_socket, error, strlen(error), 0);
        return;
    }

    // Send HTTP response
    header_length = sprintf(http_header,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "\r\n", body_length);
    send(client_socket, http_header, header_length, 0);
    if (body_length > 0) {
        send(client_socket, server_response, body_length, 0);
    }
}

/**
 * Signal handler
 */
static void signal_handler(int sig) {
    printf("\nShutting down prometheus exporter service...\n");
    if (server_socket >= 0) {
        close(server_socket);
    }
    fdfs_client_destroy();
    printf("Prometheus exporter service exited.\n\n");
    exit(0);
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    char *conf_filename;
    struct sockaddr_in server_addr;
    int daemon_mode = 0;
    int opt = 1;
    int i;
    int result;
    
    printf("FastDFS Prometheus Exporter\n");
    printf("===========================\n\n");

    // Parse arguments
    if (argc < 2) {
        printf("Usage: %s <fdfs_client_config_file> [port=%d]\n",
                argv[0], DEFAULT_PORT);
        printf("Options:\n");
        printf("  -d or --daemon Run as daemon\n\n");
        printf("For example:\n");
        printf("  %s /etc/fdfs/client.conf --daemon\n\n", argv[0]);
        return 1;
    }

    conf_filename = argv[1];

    // Parse options
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = 1;
        } else {
            listen_port = atoi(argv[i]);
            if (listen_port <= 0 || listen_port > 65535) {
                printf("Invalid port number: %s\n", argv[i]);
                return 1;
            }
        }
    }

    // Initialize FastDFS client
    log_init();
    g_log_context.log_level = LOG_ERR;
    ignore_signal_pipe();

    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        printf("ERROR: Failed to initialize FastDFS client\n");
        return result;
    }

    printf("FastDFS client initialized successfully\n");
    printf("Tracker server count: %d\n", g_tracker_group.server_count);
    printf("Mode: %s\n", daemon_mode ? "daemon" : "foreground");

    // Daemonize if requested
    if (daemon_mode) {
        daemon_init(false);
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("ERROR: Failed to create socket\n");
        fdfs_client_destroy();
        return 1;
    }

    // Set socket options
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listen_port);
    if (bind(server_socket, (struct sockaddr *)&server_addr, 
                sizeof(server_addr)) < 0)
    {
        printf("ERROR: Failed to bind socket to port %d\n", listen_port);
        close(server_socket);
        fdfs_client_destroy();
        return 1;
    }

    // Listen
    if (listen(server_socket, 10) < 0) {
        printf("ERROR: Failed to listen on socket\n");
        close(server_socket);
        fdfs_client_destroy();
        return 1;
    }

    // Allocate response buffer
    server_response = (char *)malloc(MAX_RESPONSE_SIZE);
    if (server_response == NULL) {
        printf("ERROR: malloc %d bytes fail\n", MAX_RESPONSE_SIZE);
        return ENOMEM;
    }

    printf("Listening on port %d\n", listen_port);
    printf("Metrics endpoint: http://localhost:%d/metrics\n\n", listen_port);

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Accept connections
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket;

        client_socket = accept(server_socket,
                (struct sockaddr *)&client_addr,
                &client_len);
        if (client_socket < 0) {
            continue;
        }

        handle_request(client_socket);
        close(client_socket);
    }

    return 0;
}
