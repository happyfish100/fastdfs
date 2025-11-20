/**
 * FastDFS Storage Statistics Tool
 * 
 * Collects and displays detailed storage server statistics
 * Useful for monitoring, capacity planning, and performance analysis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "logger.h"

#define MAX_GROUPS 64
#define MAX_SERVERS_PER_GROUP 32

typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    char ip_addr[IP_ADDRESS_SIZE];
    int port;
    int64_t total_space;
    int64_t free_space;
    int64_t total_upload_count;
    int64_t success_upload_count;
    int64_t total_download_count;
    int64_t success_download_count;
    int64_t total_set_meta_count;
    int64_t success_set_meta_count;
    int64_t total_delete_count;
    int64_t success_delete_count;
    int64_t total_get_meta_count;
    int64_t success_get_meta_count;
    time_t last_sync_timestamp;
    time_t last_heartbeat;
    int status;
    char version[32];
} StorageStats;

typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int server_count;
    int64_t total_space;
    int64_t free_space;
    int64_t total_files;
    StorageStats servers[MAX_SERVERS_PER_GROUP];
} GroupStats;

static int json_output = 0;
static int verbose = 0;
static int watch_mode = 0;
static int watch_interval = 5;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("Display FastDFS storage server statistics\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -g, --group NAME     Show stats for specific group only\n");
    printf("  -j, --json           Output in JSON format\n");
    printf("  -v, --verbose        Verbose output with detailed metrics\n");
    printf("  -w, --watch          Watch mode (continuous updates)\n");
    printf("  -i, --interval SEC   Watch interval in seconds (default: 5)\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                    # Show all storage stats\n", program_name);
    printf("  %s -g group1          # Show group1 stats only\n", program_name);
    printf("  %s -j                 # JSON output\n", program_name);
    printf("  %s -w -i 10           # Watch mode, update every 10 seconds\n", program_name);
}

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

static void format_time(time_t timestamp, char *buf, size_t buf_size) {
    struct tm *tm_info;
    
    if (timestamp == 0) {
        snprintf(buf, buf_size, "Never");
        return;
    }
    
    tm_info = localtime(&timestamp);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static double calculate_success_rate(int64_t success, int64_t total) {
    if (total == 0) {
        return 0.0;
    }
    return (success * 100.0) / total;
}

static void print_storage_stats_text(GroupStats *groups, int group_count) {
    char total_buf[64], free_buf[64], time_buf[64];
    
    printf("\n");
    printf("=== FastDFS Storage Statistics ===\n");
    printf("Total Groups: %d\n", group_count);
    printf("\n");
    
    for (int i = 0; i < group_count; i++) {
        GroupStats *group = &groups[i];
        
        format_bytes(group->total_space, total_buf, sizeof(total_buf));
        format_bytes(group->free_space, free_buf, sizeof(free_buf));
        
        double usage_percent = 0.0;
        if (group->total_space > 0) {
            usage_percent = ((group->total_space - group->free_space) * 100.0) / group->total_space;
        }
        
        printf("Group: %s\n", group->group_name);
        printf("  Servers: %d\n", group->server_count);
        printf("  Total Space: %s\n", total_buf);
        printf("  Free Space: %s (%.1f%% used)\n", free_buf, usage_percent);
        printf("\n");
        
        for (int j = 0; j < group->server_count; j++) {
            StorageStats *server = &group->servers[j];
            
            printf("  Server %d: %s:%d\n", j + 1, server->ip_addr, server->port);
            printf("    Status: %s\n", server->status == 0 ? "ACTIVE" : "OFFLINE");
            
            if (verbose) {
                printf("    Version: %s\n", server->version);
                
                format_bytes(server->total_space, total_buf, sizeof(total_buf));
                format_bytes(server->free_space, free_buf, sizeof(free_buf));
                printf("    Storage: %s total, %s free\n", total_buf, free_buf);
                
                format_time(server->last_heartbeat, time_buf, sizeof(time_buf));
                printf("    Last Heartbeat: %s\n", time_buf);
                
                format_time(server->last_sync_timestamp, time_buf, sizeof(time_buf));
                printf("    Last Sync: %s\n", time_buf);
                
                printf("    Upload: %lld total, %lld success (%.1f%%)\n",
                       (long long)server->total_upload_count,
                       (long long)server->success_upload_count,
                       calculate_success_rate(server->success_upload_count, server->total_upload_count));
                
                printf("    Download: %lld total, %lld success (%.1f%%)\n",
                       (long long)server->total_download_count,
                       (long long)server->success_download_count,
                       calculate_success_rate(server->success_download_count, server->total_download_count));
                
                printf("    Delete: %lld total, %lld success (%.1f%%)\n",
                       (long long)server->total_delete_count,
                       (long long)server->success_delete_count,
                       calculate_success_rate(server->success_delete_count, server->total_delete_count));
                
                printf("    Set Metadata: %lld total, %lld success (%.1f%%)\n",
                       (long long)server->total_set_meta_count,
                       (long long)server->success_set_meta_count,
                       calculate_success_rate(server->success_set_meta_count, server->total_set_meta_count));
                
                printf("    Get Metadata: %lld total, %lld success (%.1f%%)\n",
                       (long long)server->total_get_meta_count,
                       (long long)server->success_get_meta_count,
                       calculate_success_rate(server->success_get_meta_count, server->total_get_meta_count));
            }
            printf("\n");
        }
    }
}

static void print_storage_stats_json(GroupStats *groups, int group_count) {
    printf("{\n");
    printf("  \"timestamp\": %ld,\n", (long)time(NULL));
    printf("  \"group_count\": %d,\n", group_count);
    printf("  \"groups\": [\n");
    
    for (int i = 0; i < group_count; i++) {
        GroupStats *group = &groups[i];
        
        if (i > 0) {
            printf(",\n");
        }
        
        printf("    {\n");
        printf("      \"name\": \"%s\",\n", group->group_name);
        printf("      \"server_count\": %d,\n", group->server_count);
        printf("      \"total_space\": %lld,\n", (long long)group->total_space);
        printf("      \"free_space\": %lld,\n", (long long)group->free_space);
        printf("      \"servers\": [\n");
        
        for (int j = 0; j < group->server_count; j++) {
            StorageStats *server = &group->servers[j];
            
            if (j > 0) {
                printf(",\n");
            }
            
            printf("        {\n");
            printf("          \"ip\": \"%s\",\n", server->ip_addr);
            printf("          \"port\": %d,\n", server->port);
            printf("          \"status\": %d,\n", server->status);
            printf("          \"version\": \"%s\",\n", server->version);
            printf("          \"total_space\": %lld,\n", (long long)server->total_space);
            printf("          \"free_space\": %lld,\n", (long long)server->free_space);
            printf("          \"last_heartbeat\": %ld,\n", (long)server->last_heartbeat);
            printf("          \"last_sync\": %ld,\n", (long)server->last_sync_timestamp);
            printf("          \"stats\": {\n");
            printf("            \"upload\": {\"total\": %lld, \"success\": %lld},\n",
                   (long long)server->total_upload_count, (long long)server->success_upload_count);
            printf("            \"download\": {\"total\": %lld, \"success\": %lld},\n",
                   (long long)server->total_download_count, (long long)server->success_download_count);
            printf("            \"delete\": {\"total\": %lld, \"success\": %lld},\n",
                   (long long)server->total_delete_count, (long long)server->success_delete_count);
            printf("            \"set_meta\": {\"total\": %lld, \"success\": %lld},\n",
                   (long long)server->total_set_meta_count, (long long)server->success_set_meta_count);
            printf("            \"get_meta\": {\"total\": %lld, \"success\": %lld}\n",
                   (long long)server->total_get_meta_count, (long long)server->success_get_meta_count);
            printf("          }\n");
            printf("        }");
        }
        
        printf("\n      ]\n");
        printf("    }");
    }
    
    printf("\n  ]\n");
    printf("}\n");
}

static int collect_storage_stats(ConnectionInfo *pTrackerServer,
                                 const char *target_group,
                                 GroupStats *groups,
                                 int *group_count) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    int result;
    int stat_count;
    
    result = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &stat_count);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to list groups: %s\n", STRERROR(result));
        return result;
    }
    
    *group_count = 0;
    
    for (int i = 0; i < stat_count; i++) {
        if (target_group != NULL && strcmp(group_stats[i].group_name, target_group) != 0) {
            continue;
        }
        
        GroupStats *group = &groups[*group_count];
        memset(group, 0, sizeof(GroupStats));
        
        strcpy(group->group_name, group_stats[i].group_name);
        group->total_space = group_stats[i].total_mb * 1024LL * 1024LL;
        group->free_space = group_stats[i].free_mb * 1024LL * 1024LL;
        
        FDFSStorageStat storage_stats[MAX_SERVERS_PER_GROUP];
        int storage_count;
        
        result = tracker_list_servers(pTrackerServer, group->group_name,
                                     NULL, storage_stats,
                                     MAX_SERVERS_PER_GROUP, &storage_count);
        if (result != 0) {
            fprintf(stderr, "WARNING: Failed to list servers for group %s: %s\n",
                   group->group_name, STRERROR(result));
            continue;
        }
        
        group->server_count = storage_count;
        
        for (int j = 0; j < storage_count; j++) {
            StorageStats *server = &group->servers[j];
            FDFSStorageStat *stat = &storage_stats[j];
            
            strcpy(server->group_name, group->group_name);
            strcpy(server->ip_addr, stat->ip_addr);
            server->port = stat->port;
            server->status = stat->status;
            strcpy(server->version, stat->version);
            
            server->total_space = stat->total_mb * 1024LL * 1024LL;
            server->free_space = stat->free_mb * 1024LL * 1024LL;
            
            server->total_upload_count = stat->total_upload_count;
            server->success_upload_count = stat->success_upload_count;
            server->total_download_count = stat->total_download_count;
            server->success_download_count = stat->success_download_count;
            server->total_set_meta_count = stat->total_set_meta_count;
            server->success_set_meta_count = stat->success_set_meta_count;
            server->total_delete_count = stat->total_delete_count;
            server->success_delete_count = stat->success_delete_count;
            server->total_get_meta_count = stat->total_get_meta_count;
            server->success_get_meta_count = stat->success_get_meta_count;
            
            server->last_sync_timestamp = stat->last_synced_timestamp;
            server->last_heartbeat = stat->last_heart_beat_time;
        }
        
        (*group_count)++;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *target_group = NULL;
    int result;
    ConnectionInfo *pTrackerServer;
    GroupStats groups[MAX_GROUPS];
    int group_count;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"group", required_argument, 0, 'g'},
        {"json", no_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"watch", no_argument, 0, 'w'},
        {"interval", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:g:jvwi:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'g':
                target_group = optarg;
                break;
            case 'j':
                json_output = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'w':
                watch_mode = 1;
                break;
            case 'i':
                watch_interval = atoi(optarg);
                if (watch_interval < 1) watch_interval = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    log_init();
    g_log_context.log_level = LOG_ERR;
    
    result = fdfs_client_init(conf_filename);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to initialize FastDFS client\n");
        return result;
    }
    
    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL) {
        fprintf(stderr, "ERROR: Failed to connect to tracker server\n");
        fdfs_client_destroy();
        return errno != 0 ? errno : ECONNREFUSED;
    }
    
    do {
        if (watch_mode && !json_output) {
            system("clear");
        }
        
        result = collect_storage_stats(pTrackerServer, target_group, groups, &group_count);
        if (result != 0) {
            tracker_disconnect_server_ex(pTrackerServer, true);
            fdfs_client_destroy();
            return result;
        }
        
        if (json_output) {
            print_storage_stats_json(groups, group_count);
        } else {
            print_storage_stats_text(groups, group_count);
        }
        
        if (watch_mode) {
            if (!json_output) {
                printf("Press Ctrl+C to exit. Refreshing in %d seconds...\n", watch_interval);
            }
            sleep(watch_interval);
        }
    } while (watch_mode);
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return 0;
}
