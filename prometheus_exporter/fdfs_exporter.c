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
static ConnectionInfo *pTrackerServer = NULL;
static int listen_port = DEFAULT_PORT;
static int server_socket = -1;

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
static int append_metric(char *response, size_t *offset, size_t max_size,
                        const char *name, const char *labels,
                        const char *value, const char *help) {
    int written = 0;
    
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

/**
 * Export group-level metrics
 */
static int export_group_metrics(char *response, size_t *offset, size_t max_size,
                               FDFSGroupStat *pGroupStat) {
    char metric_name[256];
    char labels[512];
    char value[64];
    
    // Group label
    snprintf(labels, sizeof(labels), "group=\"%s\"", pGroupStat->group_name);
    
    // Total space
    format_metric_name(metric_name, sizeof(metric_name), "group", "total_mb");
    snprintf(value, sizeof(value), "%"PRId64, pGroupStat->total_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total storage space in MB") != 0) return -1;
    
    // Free space
    format_metric_name(metric_name, sizeof(metric_name), "group", "free_mb");
    snprintf(value, sizeof(value), "%"PRId64, pGroupStat->free_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Free storage space in MB") != 0) return -1;
    
    // Trunk free space
    format_metric_name(metric_name, sizeof(metric_name), "group", "trunk_free_mb");
    snprintf(value, sizeof(value), "%"PRId64, pGroupStat->trunk_free_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Trunk free space in MB") != 0) return -1;
    
    // Storage server count
    format_metric_name(metric_name, sizeof(metric_name), "group", "storage_count");
    snprintf(value, sizeof(value), "%d", pGroupStat->count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Number of storage servers in group") != 0) return -1;
    
    // Active server count
    format_metric_name(metric_name, sizeof(metric_name), "group", "active_count");
    snprintf(value, sizeof(value), "%d", pGroupStat->active_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Number of active storage servers") != 0) return -1;
    
    return 0;
}

/**
 * Export storage-level metrics
 */
static int export_storage_metrics(char *response, size_t *offset, size_t max_size,
                                 const char *group_name,
                                 FDFSStorageBrief *pStorage,
                                 FDFSStorageStat *pStorageStat) {
    char metric_name[256];
    char labels[512];
    char value[64];
    time_t current_time = time(NULL);
    
    // Storage labels
    snprintf(labels, sizeof(labels), 
            "group=\"%s\",storage_id=\"%s\",ip=\"%s\",status=\"%d\"",
            group_name, pStorage->id, pStorage->ip_addr, pStorage->status);
    
    // === Storage Space Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "total_mb");
    snprintf(value, sizeof(value), "%"PRId64, pStorage->total_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total storage space in MB") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "free_mb");
    snprintf(value, sizeof(value), "%"PRId64, pStorage->free_mb);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Free storage space in MB") != 0) return -1;
    
    // === Upload Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "upload_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_upload_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total upload operations") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "upload_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_upload_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Successful upload operations") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "upload_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_upload_bytes);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total uploaded bytes") != 0) return -1;
    
    // === Download Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "download_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_download_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total download operations") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "download_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_download_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Successful download operations") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "download_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_download_bytes);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total downloaded bytes") != 0) return -1;
    
    // === Delete Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "delete_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_delete_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total delete operations") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "delete_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_delete_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Successful delete operations") != 0) return -1;
    
    // === Append Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "append_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_append_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total append operations") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "append_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_append_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Successful append operations") != 0) return -1;
    
    // === Modify Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "modify_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_modify_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total modify operations") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "modify_success");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->success_modify_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Successful modify operations") != 0) return -1;
    
    // === Connection Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "connections_current");
    snprintf(value, sizeof(value), "%d", pStorageStat->connection.current_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Current connection count") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "connections_max");
    snprintf(value, sizeof(value), "%d", pStorageStat->connection.max_count);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Maximum connection count") != 0) return -1;
    
    // === Heartbeat Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "last_heartbeat");
    snprintf(value, sizeof(value), "%ld", (long)pStorageStat->last_heart_beat_time);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Last heartbeat timestamp") != 0) return -1;
    
    // Heartbeat delay
    format_metric_name(metric_name, sizeof(metric_name), "storage", "heartbeat_delay_seconds");
    snprintf(value, sizeof(value), "%ld", 
            (long)(current_time - pStorageStat->last_heart_beat_time));
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Seconds since last heartbeat") != 0) return -1;
    
    // === Sync Metrics ===
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "sync_in_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_sync_in_bytes);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total sync in bytes") != 0) return -1;
    
    format_metric_name(metric_name, sizeof(metric_name), "storage", "sync_out_bytes_total");
    snprintf(value, sizeof(value), "%"PRId64, pStorageStat->total_sync_out_bytes);
    if (append_metric(response, offset, max_size, metric_name, labels, value,
                     "Total sync out bytes") != 0) return -1;
    
    return 0;
}

/**
 * Collect all metrics from FastDFS
 */
static int collect_metrics(char *response, size_t max_size) {
    int result;
    int group_count;
    FDFSGroupStat group_stats[FDFS_MAX_GROUPS];
    FDFSGroupStat *pGroupStat;
    FDFSGroupStat *pGroupEnd;
    size_t offset = 0;
    
    // Get tracker connection
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    // List all groups
    result = tracker_list_groups(pTrackerServer, group_stats,
                                 FDFS_MAX_GROUPS, &group_count);
    if (result != 0) {
        tracker_disconnect_server_ex(pTrackerServer, true);
        return result;
    }
    
    // Export metrics for each group
    pGroupEnd = group_stats + group_count;
    for (pGroupStat = group_stats; pGroupStat < pGroupEnd; pGroupStat++) {
        // Export group metrics
        if (export_group_metrics(response, &offset, max_size, pGroupStat) != 0) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            return -1;
        }
        
        // Export storage metrics for each server in group
        FDFSStorageBrief *pStorage;
        FDFSStorageBrief *pStorageEnd;
        FDFSStorageStat *pStorageStat;
        
        pStorageEnd = pGroupStat->storage_servers + pGroupStat->count;
        pStorageStat = pGroupStat->storage_stats;
        
        for (pStorage = pGroupStat->storage_servers; 
             pStorage < pStorageEnd; 
             pStorage++, pStorageStat++) {
            if (export_storage_metrics(response, &offset, max_size,
                                      pGroupStat->group_name,
                                      pStorage, pStorageStat) != 0) {
                tracker_disconnect_server_ex(pTrackerServer, true);
                return -1;
            }
        }
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    return 0;
}

/**
 * Handle HTTP request
 */
static void handle_request(int client_socket) {
    char request[4096];
    char *response = NULL;
    char http_header[512];
    int bytes_read;
    int result;
    
    // Read request
    bytes_read = recv(client_socket, request, sizeof(request) - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
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
        close(client_socket);
        return;
    }
    
    // Allocate response buffer
    response = (char *)malloc(MAX_RESPONSE_SIZE);
    if (response == NULL) {
        const char *error = "HTTP/1.1 500 Internal Server Error\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: 21\r\n\r\n"
                           "Internal Server Error";
        send(client_socket, error, strlen(error), 0);
        close(client_socket);
        return;
    }
    
    // Collect metrics
    result = collect_metrics(response, MAX_RESPONSE_SIZE);
    if (result != 0) {
        snprintf(response, MAX_RESPONSE_SIZE,
                "# ERROR: Failed to collect metrics (error code: %d)\n", result);
    }
    
    // Send HTTP response
    snprintf(http_header, sizeof(http_header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; version=0.0.4\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            strlen(response));
    
    send(client_socket, http_header, strlen(http_header), 0);
    send(client_socket, response, strlen(response), 0);
    
    free(response);
    close(client_socket);
}

/**
 * Signal handler
 */
static void signal_handler(int sig) {
    if (server_socket >= 0) {
        close(server_socket);
    }
    fdfs_client_destroy();
    exit(0);
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    char *conf_filename;
    struct sockaddr_in server_addr;
    int result;
    int opt = 1;
    
    printf("FastDFS Prometheus Exporter\n");
    printf("===========================\n\n");
    
    // Parse arguments
    if (argc < 2) {
        printf("Usage: %s <config_file> [port]\n", argv[0]);
        printf("Default port: %d\n", DEFAULT_PORT);
        return 1;
    }
    
    conf_filename = argv[1];
    if (argc >= 3) {
        listen_port = atoi(argv[2]);
        if (listen_port <= 0 || listen_port > 65535) {
            printf("Invalid port number: %s\n", argv[2]);
            return 1;
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
    printf("Tracker servers: %d\n", g_tracker_group.server_count);
    
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
            sizeof(server_addr)) < 0) {
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
    }
    
    return 0;
}
