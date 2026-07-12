/**
 * FastDFS Prometheus Exporter
 * 
 * Exposes FastDFS metrics in Prometheus format for monitoring and alerting.
 * Provides comprehensive metrics for TPS, traffic, storage, and node status.
 * 
 * Copyright (C) 2025
 * License: GPL V3
 */

#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "client_global.h"
#include "fdfs_global.h"
#include "fdfs_client.h"

#define DEFAULT_PORT 9898
#define MAX_RESPONSE_SIZE (1024 * 1024)  // 1MB
#define METRIC_PREFIX "fastdfs_"

typedef enum {
    auth_method_none,
    auth_method_basic
} AuthMethod;

// Global variables
static char *config_filename = NULL;
static char *fdfs_client_config = FDFS_CLIENT_DEFAULT_CONFIG_FILENAME;
static char *bind_addr = NULL;
static AuthMethod auth_method = auth_method_none;
static string_t authorization_basic = {NULL, 0};
static int daemon_mode = 0;
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

static bool check_authorization(char *request)
{
#define HTTP_AUTHORIZATION_TAG_STR  "Authorization:"
#define HTTP_AUTHORIZATION_TAG_LEN  (sizeof(HTTP_AUTHORIZATION_TAG_STR) - 1)

    string_t auth;
    char *line;
    char *lend;

    line = request;
    while (1) {
        lend = strstr(line, "\r\n");
        if (lend == NULL) {
            return false;
        }

        line = lend + 2;
        if (strncasecmp(line, HTTP_AUTHORIZATION_TAG_STR,
                    HTTP_AUTHORIZATION_TAG_LEN) == 0)
        {
            break;
        }
    }

    auth.str = line + HTTP_AUTHORIZATION_TAG_LEN;
    lend = strstr(auth.str, "\r\n");
    if (lend == NULL) {
        return false;
    }

    while (auth.str < lend && (*auth.str == ' ' || *auth.str == '\t')) {
        auth.str++;
    }

    if (strncasecmp(auth.str, "Basic ", 6) != 0)
    {
        return false;
    }
    auth.str += 6;
    auth.len = lend - auth.str;
    return fc_string_equals(&authorization_basic, &auth);
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

    if (auth_method == auth_method_basic) {
        if (!check_authorization(request)) {
            const char *unauthorized = "HTTP/1.1 401 Unauthorized\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n\r\n"
                "Unauthorized";
            send(client_socket, unauthorized, strlen(unauthorized), 0);
            return;
        }
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

#define OPTION_NAME_BIND_ADDR       "bind_addr"
#define OPTION_NAME_CONFIG_FILE     "config_file"
#define OPTION_NAME_DAEMON          "daemon"
#define OPTION_NAME_FDFS_CLIENT_CFG "fdfs_client_cfg"
#define OPTION_NAME_HELP            "help"
#define OPTION_NAME_PORT            "port"

static void usage(char *argv[])
{
    printf("\nUsage: %s [options]\n\n", argv[0]);
    printf("Options:\n");
    printf("  --"OPTION_NAME_BIND_ADDR" or -B: bind local ip address, "
            "such as 127.0.0.1\n");
    printf("  --"OPTION_NAME_CONFIG_FILE" or -c: specify exporter "
            "config filename\n");
    printf("  --"OPTION_NAME_FDFS_CLIENT_CFG" or -f: specify FastDFS "
            "client config filename, default is %s\n",
            FDFS_CLIENT_DEFAULT_CONFIG_FILENAME);
    printf("  --"OPTION_NAME_DAEMON" or -d: run as daemon\n");
    printf("  --"OPTION_NAME_HELP" or -h: show help\n");
    printf("  --"OPTION_NAME_PORT" or -P: specify listen port, "
            "default is %d\n\n", DEFAULT_PORT);
    printf("For example:\n");
    printf("  %s -f /etc/fdfs/client.conf -d\n", argv[0]);
    printf("  %s --bind_addr 127.0.0.1 --daemon\n\n", argv[0]);
}

const struct option longopts[] = {
    {OPTION_NAME_BIND_ADDR,       required_argument, NULL, 'B'},
    {OPTION_NAME_CONFIG_FILE,     required_argument, NULL, 'c'},
    {OPTION_NAME_FDFS_CLIENT_CFG, required_argument, NULL, 'f'},
    {OPTION_NAME_DAEMON,          no_argument,       NULL, 'd'},
    {OPTION_NAME_PORT,            required_argument, NULL, 'P'},
    {NULL, 0, NULL, 0}
};

#define OPT_STRING  "B:c:df:hP:"

static int load_from_config_file()
{
    IniContext ini_context;
    struct base64_context base64_ctx;
    char *addr;
    char *client_cfg;
    char *method;
    char *username;
    char *password;
    char buff[128];
    int len;
    int bytes;
    int result;

    if ((result=iniLoadFromFile(config_filename, &ini_context)) != 0) {
        logError("file: "__FILE__", line: %d, "
                "load conf file \"%s\" fail, ret code: %d",
                __LINE__, config_filename, result);
        return result;
    }

    do {
        addr = iniGetStrValue(NULL, "bind_addr", &ini_context);
        if (addr != NULL && *addr != '\0') {
            if ((bind_addr=fc_strdup(addr)) == NULL) {
                result = ENOMEM;
                break;
            }
        }

        listen_port = iniGetIntValue(NULL, "port", &ini_context, DEFAULT_PORT);
        if (listen_port <= 0 || listen_port > 65535) {
            logError("file: "__FILE__", line: %d, "
                    "invalid port number: %d",
                    __LINE__, listen_port);
            result = EINVAL;
            break;
        }
        daemon_mode = iniGetBoolValue(NULL, "daemon", &ini_context, false);

        client_cfg = iniGetStrValue(NULL, "fdfs_client_cfg", &ini_context);
        if (client_cfg != NULL) {
            if (*client_cfg == '\0') {
                logError("file: "__FILE__", line: %d, "
                        "empty FDFS client config filename!", __LINE__);
                result = EINVAL;
                break;
            }

            if ((fdfs_client_config=fc_strdup(client_cfg)) == NULL) {
                result = ENOMEM;
                break;
            }
        }

        method = iniGetStrValue("auth", "method", &ini_context);
        if (method == NULL || *method == '\0' ||
                strcasecmp(method, "none") == 0)
        {
            break;
        }
        if (strcasecmp(method, "basic") != 0) {
            logError("file: "__FILE__", line: %d, "
                    "unkown auth method: %s!",
                    __LINE__, method);
            result = EINVAL;
            break;
        }

        auth_method = auth_method_basic;
        username = iniGetStrValue("auth", "username", &ini_context);
        if (username == NULL || *username == '\0') {
            logError("file: "__FILE__", line: %d, "
                    "username not exist or username is empty !",
                    __LINE__);
            result = EINVAL;
            break;
        }

        password = iniGetStrValue("auth", "password", &ini_context);
        if (password == NULL || *password == '\0') {
            logError("file: "__FILE__", line: %d, "
                    "password not exist or password is empty !",
                    __LINE__);
            result = EINVAL;
            break;
        }

        len = snprintf(buff, sizeof(buff), "%s:%s", username, password);
        if (len >= sizeof(buff)) {
            logError("file: "__FILE__", line: %d, "
                    "username and password are too long, exceed %d!",
                    __LINE__, (int)sizeof(buff));
            result = EOVERFLOW;
            break;
        }

        bytes = (len + 2) / 3 * 4;
        if ((authorization_basic.str=fc_malloc(bytes + 1)) == NULL) {
            result = ENOMEM;
            break;
        }

        base64_init(&base64_ctx, 0);
        base64_encode(&base64_ctx, buff, len, authorization_basic.str,
                &authorization_basic.len);
    } while (0);

    iniFreeContext(&ini_context);
    return result;
}

static int check_load_from_cfg_file(int argc, char *argv[])
{
    int ch;

    while ((ch=getopt_long(argc, argv, OPT_STRING, longopts, NULL)) != -1) {
        if (ch == 'c') {
            config_filename = optarg;
            break;
        }
        if (ch == ':' || ch == '?') {
            usage(argv);
            return EINVAL;
        }
    }

    if (config_filename == NULL) {
        return 0;
    }
    if (*config_filename == '\0') {
        printf("ERROR: empty config filename!\n");
        return EINVAL;
    }

    return load_from_config_file();
}

int main(int argc, char *argv[])
{
    int ch;
    int result;

    log_init();
    g_log_context.log_level = LOG_ERR;
    if ((result=check_load_from_cfg_file(argc, argv)) != 0) {
        return result;
    }

    //Reset for calling getopt_long again
    optind = 1;
    while ((ch=getopt_long(argc, argv, OPT_STRING, longopts, NULL)) != -1) {
        switch (ch) {
            case 'B':
                if (bind_addr != NULL) {
                    free(bind_addr);
                }
                bind_addr = optarg;
                break;
            case 'd':
                daemon_mode = 1;
                break;
            case 'c':
                break;
            case 'f':
                if (fdfs_client_config != NULL) {
                    free(fdfs_client_config);
                }
                fdfs_client_config = optarg;
                break;
            case 'h':
                usage(argv);
                return 0;
            case 'P':
                listen_port = atoi(optarg);
                if (listen_port <= 0 || listen_port > 65535) {
                    printf("invalid port number: %s\n", optarg);
                    return EINVAL;
                }
                break;
            default:
                usage(argv);
                return EINVAL;
        }
    }

    printf("FastDFS Prometheus Exporter\n");
    printf("===========================\n\n");
    ignore_signal_pipe();

    result = fdfs_client_init(fdfs_client_config);
    if (result != 0) {
        printf("ERROR: Failed to initialize FastDFS client\n");
        return result;
    }

    printf("FastDFS client initialized successfully\n");
    printf("Tracker server count: %d\n", g_tracker_group.server_count);
    printf("Mode: %s\n", daemon_mode ? "daemon" : "foreground");
    printf("Auth method: %s\n", (auth_method == auth_method_basic ?
                "basic" : "none"));

    // Daemonize if requested
    if (daemon_mode) {
        daemon_init(false);
    }

    server_socket = socketServer(bind_addr, listen_port, &result);
    if (server_socket < 0) {
        fdfs_client_destroy();
        return 1;
    }

    // Allocate response buffer
    server_response = (char *)malloc(MAX_RESPONSE_SIZE);
    if (server_response == NULL) {
        printf("ERROR: malloc %d bytes fail\n", MAX_RESPONSE_SIZE);
        return ENOMEM;
    }

    if (bind_addr == NULL || *bind_addr == '\0') {
        printf("Listening on port %d\n", listen_port);
        printf("Metrics endpoint: http://localhost:%d/metrics\n\n", listen_port);
    } else {
        printf("Listening on %s:%d\n", bind_addr, listen_port);
        printf("Metrics endpoint: http://%s:%d/metrics\n\n", bind_addr, listen_port);
    }

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
