/**
 * FastDFS Cluster Management Tool
 * 
 * Manages cluster operations including rebalancing, monitoring, and maintenance
 * Provides cluster-wide statistics and health monitoring
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include "fdfs_client.h"
#include "dfs_func.h"
#include "logger.h"
#include "tracker_types.h"
#include "tracker_proto.h"

#define MAX_GROUPS 256
#define MAX_SERVERS_PER_GROUP 32
#define MAX_PATH_LEN 1024

typedef struct {
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
    int server_count;
    int64_t total_space;
    int64_t free_space;
    int64_t total_upload_count;
    int64_t total_download_count;
    int active_count;
    int online_count;
    double avg_load;
} GroupStats;

typedef struct {
    char ip_addr[IP_ADDRESS_SIZE];
    int port;
    char status[32];
    int64_t total_space;
    int64_t free_space;
    int64_t upload_count;
    int64_t download_count;
    time_t last_heartbeat;
    int is_active;
} ServerInfo;

typedef struct {
    GroupStats groups[MAX_GROUPS];
    int group_count;
    ServerInfo servers[MAX_GROUPS][MAX_SERVERS_PER_GROUP];
    int server_counts[MAX_GROUPS];
} ClusterInfo;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <command>\n", program_name);
    printf("\n");
    printf("FastDFS cluster management tool\n");
    printf("\n");
    printf("Commands:\n");
    printf("  status         Show cluster status\n");
    printf("  groups         List all groups\n");
    printf("  servers        List all servers\n");
    printf("  balance        Check cluster balance\n");
    printf("  health         Perform health check\n");
    printf("  summary        Show cluster summary\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file (default: /etc/fdfs/client.conf)\n");
    printf("  -g, --group NAME     Filter by group name\n");
    printf("  -j, --json           Output in JSON format\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s status\n", program_name);
    printf("  %s groups -j\n", program_name);
    printf("  %s servers -g group1\n", program_name);
    printf("  %s balance\n", program_name);
}

static int query_cluster_info(ConnectionInfo *pTrackerServer, ClusterInfo *cluster) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    int group_count;
    int result;
    
    result = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &group_count);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to list groups: %s\n", STRERROR(result));
        return result;
    }
    
    cluster->group_count = group_count;
    
    for (int i = 0; i < group_count; i++) {
        GroupStats *gs = &cluster->groups[i];
        strncpy(gs->group_name, group_stats[i].group_name, FDFS_GROUP_NAME_MAX_LEN);
        gs->server_count = group_stats[i].count;
        gs->total_space = group_stats[i].total_mb * 1024 * 1024;
        gs->free_space = group_stats[i].free_mb * 1024 * 1024;
        gs->total_upload_count = group_stats[i].total_upload_count;
        gs->total_download_count = group_stats[i].total_download_count;
        gs->active_count = group_stats[i].active_count;
        gs->online_count = group_stats[i].count;
        
        FDFSStorageInfo storage_infos[MAX_SERVERS_PER_GROUP];
        int storage_count;
        
        result = tracker_list_servers(pTrackerServer, gs->group_name,
                                     NULL, storage_infos,
                                     MAX_SERVERS_PER_GROUP, &storage_count);
        
        if (result == 0) {
            cluster->server_counts[i] = storage_count;
            
            for (int j = 0; j < storage_count; j++) {
                ServerInfo *si = &cluster->servers[i][j];
                strncpy(si->ip_addr, storage_infos[j].ip_addr, IP_ADDRESS_SIZE - 1);
                si->port = storage_infos[j].port;
                
                if (storage_infos[j].status == FDFS_STORAGE_STATUS_ACTIVE) {
                    strcpy(si->status, "ACTIVE");
                    si->is_active = 1;
                } else if (storage_infos[j].status == FDFS_STORAGE_STATUS_ONLINE) {
                    strcpy(si->status, "ONLINE");
                    si->is_active = 1;
                } else if (storage_infos[j].status == FDFS_STORAGE_STATUS_OFFLINE) {
                    strcpy(si->status, "OFFLINE");
                    si->is_active = 0;
                } else {
                    strcpy(si->status, "UNKNOWN");
                    si->is_active = 0;
                }
                
                si->total_space = storage_infos[j].total_mb * 1024 * 1024;
                si->free_space = storage_infos[j].free_mb * 1024 * 1024;
                si->upload_count = storage_infos[j].total_upload_count;
                si->download_count = storage_infos[j].total_download_count;
                si->last_heartbeat = storage_infos[j].last_heart_beat_time;
            }
        }
    }
    
    return 0;
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

static void print_cluster_status(ClusterInfo *cluster, int json_output) {
    if (json_output) {
        printf("{\n");
        printf("  \"cluster\": {\n");
        printf("    \"group_count\": %d,\n", cluster->group_count);
        printf("    \"groups\": [\n");
        
        for (int i = 0; i < cluster->group_count; i++) {
            GroupStats *gs = &cluster->groups[i];
            printf("      {\n");
            printf("        \"name\": \"%s\",\n", gs->group_name);
            printf("        \"server_count\": %d,\n", gs->server_count);
            printf("        \"active_count\": %d,\n", gs->active_count);
            printf("        \"total_space\": %lld,\n", (long long)gs->total_space);
            printf("        \"free_space\": %lld,\n", (long long)gs->free_space);
            printf("        \"upload_count\": %lld,\n", (long long)gs->total_upload_count);
            printf("        \"download_count\": %lld\n", (long long)gs->total_download_count);
            printf("      }%s\n", i < cluster->group_count - 1 ? "," : "");
        }
        
        printf("    ]\n");
        printf("  }\n");
        printf("}\n");
    } else {
        char total_str[64], free_str[64];
        
        printf("\n=== FastDFS Cluster Status ===\n\n");
        printf("Total Groups: %d\n\n", cluster->group_count);
        
        for (int i = 0; i < cluster->group_count; i++) {
            GroupStats *gs = &cluster->groups[i];
            
            format_bytes(gs->total_space, total_str, sizeof(total_str));
            format_bytes(gs->free_space, free_str, sizeof(free_str));
            
            double usage_percent = 0.0;
            if (gs->total_space > 0) {
                usage_percent = ((gs->total_space - gs->free_space) * 100.0) / gs->total_space;
            }
            
            printf("Group: %s\n", gs->group_name);
            printf("  Servers: %d (Active: %d)\n", gs->server_count, gs->active_count);
            printf("  Storage: %s total, %s free (%.1f%% used)\n",
                   total_str, free_str, usage_percent);
            printf("  Operations: %lld uploads, %lld downloads\n",
                   (long long)gs->total_upload_count,
                   (long long)gs->total_download_count);
            printf("\n");
        }
    }
}

static void print_group_list(ClusterInfo *cluster, int json_output) {
    if (json_output) {
        printf("{\n");
        printf("  \"groups\": [\n");
        
        for (int i = 0; i < cluster->group_count; i++) {
            GroupStats *gs = &cluster->groups[i];
            printf("    {\n");
            printf("      \"name\": \"%s\",\n", gs->group_name);
            printf("      \"servers\": %d,\n", gs->server_count);
            printf("      \"active\": %d,\n", gs->active_count);
            printf("      \"total_space_bytes\": %lld,\n", (long long)gs->total_space);
            printf("      \"free_space_bytes\": %lld\n", (long long)gs->free_space);
            printf("    }%s\n", i < cluster->group_count - 1 ? "," : "");
        }
        
        printf("  ]\n");
        printf("}\n");
    } else {
        char total_str[64], free_str[64];
        
        printf("\n=== FastDFS Groups ===\n\n");
        printf("%-15s %10s %10s %15s %15s\n",
               "Group", "Servers", "Active", "Total Space", "Free Space");
        printf("%-15s %10s %10s %15s %15s\n",
               "-----", "-------", "------", "-----------", "----------");
        
        for (int i = 0; i < cluster->group_count; i++) {
            GroupStats *gs = &cluster->groups[i];
            
            format_bytes(gs->total_space, total_str, sizeof(total_str));
            format_bytes(gs->free_space, free_str, sizeof(free_str));
            
            printf("%-15s %10d %10d %15s %15s\n",
                   gs->group_name,
                   gs->server_count,
                   gs->active_count,
                   total_str,
                   free_str);
        }
        printf("\n");
    }
}

static void print_server_list(ClusterInfo *cluster, const char *filter_group, int json_output) {
    if (json_output) {
        printf("{\n");
        printf("  \"servers\": [\n");
        
        int first = 1;
        for (int i = 0; i < cluster->group_count; i++) {
            if (filter_group != NULL && strcmp(cluster->groups[i].group_name, filter_group) != 0) {
                continue;
            }
            
            for (int j = 0; j < cluster->server_counts[i]; j++) {
                ServerInfo *si = &cluster->servers[i][j];
                
                if (!first) printf(",\n");
                first = 0;
                
                printf("    {\n");
                printf("      \"group\": \"%s\",\n", cluster->groups[i].group_name);
                printf("      \"ip\": \"%s\",\n", si->ip_addr);
                printf("      \"port\": %d,\n", si->port);
                printf("      \"status\": \"%s\",\n", si->status);
                printf("      \"total_space\": %lld,\n", (long long)si->total_space);
                printf("      \"free_space\": %lld,\n", (long long)si->free_space);
                printf("      \"uploads\": %lld,\n", (long long)si->upload_count);
                printf("      \"downloads\": %lld\n", (long long)si->download_count);
                printf("    }");
            }
        }
        
        printf("\n  ]\n");
        printf("}\n");
    } else {
        char total_str[64], free_str[64];
        
        printf("\n=== FastDFS Storage Servers ===\n\n");
        printf("%-15s %-20s %8s %-10s %15s %15s\n",
               "Group", "IP Address", "Port", "Status", "Total Space", "Free Space");
        printf("%-15s %-20s %8s %-10s %15s %15s\n",
               "-----", "----------", "----", "------", "-----------", "----------");
        
        for (int i = 0; i < cluster->group_count; i++) {
            if (filter_group != NULL && strcmp(cluster->groups[i].group_name, filter_group) != 0) {
                continue;
            }
            
            for (int j = 0; j < cluster->server_counts[i]; j++) {
                ServerInfo *si = &cluster->servers[i][j];
                
                format_bytes(si->total_space, total_str, sizeof(total_str));
                format_bytes(si->free_space, free_str, sizeof(free_str));
                
                printf("%-15s %-20s %8d %-10s %15s %15s\n",
                       cluster->groups[i].group_name,
                       si->ip_addr,
                       si->port,
                       si->status,
                       total_str,
                       free_str);
            }
        }
        printf("\n");
    }
}

static void check_cluster_balance(ClusterInfo *cluster) {
    printf("\n=== Cluster Balance Analysis ===\n\n");
    
    for (int i = 0; i < cluster->group_count; i++) {
        GroupStats *gs = &cluster->groups[i];
        
        if (cluster->server_counts[i] == 0) {
            continue;
        }
        
        printf("Group: %s\n", gs->group_name);
        
        int64_t min_free = LLONG_MAX;
        int64_t max_free = 0;
        int64_t total_free = 0;
        
        for (int j = 0; j < cluster->server_counts[i]; j++) {
            ServerInfo *si = &cluster->servers[i][j];
            
            if (!si->is_active) {
                continue;
            }
            
            if (si->free_space < min_free) {
                min_free = si->free_space;
            }
            if (si->free_space > max_free) {
                max_free = si->free_space;
            }
            total_free += si->free_space;
        }
        
        int64_t avg_free = total_free / gs->active_count;
        double imbalance = 0.0;
        
        if (avg_free > 0) {
            imbalance = ((max_free - min_free) * 100.0) / avg_free;
        }
        
        char min_str[64], max_str[64], avg_str[64];
        format_bytes(min_free, min_str, sizeof(min_str));
        format_bytes(max_free, max_str, sizeof(max_str));
        format_bytes(avg_free, avg_str, sizeof(avg_str));
        
        printf("  Free space: min=%s, max=%s, avg=%s\n", min_str, max_str, avg_str);
        printf("  Imbalance: %.1f%%\n", imbalance);
        
        if (imbalance < 10.0) {
            printf("  Status: ✓ Well balanced\n");
        } else if (imbalance < 30.0) {
            printf("  Status: ⚠ Slightly imbalanced\n");
        } else {
            printf("  Status: ❌ Highly imbalanced - rebalancing recommended\n");
        }
        
        printf("\n");
    }
}

static void perform_health_check(ClusterInfo *cluster) {
    int total_servers = 0;
    int active_servers = 0;
    int offline_servers = 0;
    int warnings = 0;
    int errors = 0;
    
    printf("\n=== Cluster Health Check ===\n\n");
    
    for (int i = 0; i < cluster->group_count; i++) {
        for (int j = 0; j < cluster->server_counts[i]; j++) {
            ServerInfo *si = &cluster->servers[i][j];
            total_servers++;
            
            if (si->is_active) {
                active_servers++;
                
                double usage_percent = 0.0;
                if (si->total_space > 0) {
                    usage_percent = ((si->total_space - si->free_space) * 100.0) / si->total_space;
                }
                
                if (usage_percent > 95.0) {
                    printf("❌ ERROR: %s:%d - Disk usage critical (%.1f%%)\n",
                           si->ip_addr, si->port, usage_percent);
                    errors++;
                } else if (usage_percent > 85.0) {
                    printf("⚠ WARNING: %s:%d - Disk usage high (%.1f%%)\n",
                           si->ip_addr, si->port, usage_percent);
                    warnings++;
                }
                
                time_t now = time(NULL);
                int heartbeat_age = now - si->last_heartbeat;
                
                if (heartbeat_age > 300) {
                    printf("⚠ WARNING: %s:%d - Last heartbeat %d seconds ago\n",
                           si->ip_addr, si->port, heartbeat_age);
                    warnings++;
                }
            } else {
                offline_servers++;
                printf("❌ ERROR: %s:%d - Server offline\n", si->ip_addr, si->port);
                errors++;
            }
        }
    }
    
    printf("\n=== Health Summary ===\n");
    printf("Total servers: %d\n", total_servers);
    printf("Active servers: %d\n", active_servers);
    printf("Offline servers: %d\n", offline_servers);
    printf("Warnings: %d\n", warnings);
    printf("Errors: %d\n", errors);
    
    if (errors == 0 && warnings == 0) {
        printf("\n✓ Cluster is healthy\n");
    } else if (errors == 0) {
        printf("\n⚠ Cluster has warnings\n");
    } else {
        printf("\n❌ Cluster has errors - immediate attention required\n");
    }
    
    printf("\n");
}

static void print_cluster_summary(ClusterInfo *cluster) {
    int64_t total_space = 0;
    int64_t total_free = 0;
    int64_t total_uploads = 0;
    int64_t total_downloads = 0;
    int total_servers = 0;
    int active_servers = 0;
    
    for (int i = 0; i < cluster->group_count; i++) {
        total_space += cluster->groups[i].total_space;
        total_free += cluster->groups[i].free_space;
        total_uploads += cluster->groups[i].total_upload_count;
        total_downloads += cluster->groups[i].total_download_count;
        total_servers += cluster->groups[i].server_count;
        active_servers += cluster->groups[i].active_count;
    }
    
    char total_str[64], free_str[64], used_str[64];
    format_bytes(total_space, total_str, sizeof(total_str));
    format_bytes(total_free, free_str, sizeof(free_str));
    format_bytes(total_space - total_free, used_str, sizeof(used_str));
    
    double usage_percent = 0.0;
    if (total_space > 0) {
        usage_percent = ((total_space - total_free) * 100.0) / total_space;
    }
    
    printf("\n=== FastDFS Cluster Summary ===\n\n");
    printf("Cluster Configuration:\n");
    printf("  Groups: %d\n", cluster->group_count);
    printf("  Total servers: %d\n", total_servers);
    printf("  Active servers: %d\n", active_servers);
    printf("  Offline servers: %d\n", total_servers - active_servers);
    printf("\n");
    printf("Storage Capacity:\n");
    printf("  Total: %s\n", total_str);
    printf("  Used: %s (%.1f%%)\n", used_str, usage_percent);
    printf("  Free: %s\n", free_str);
    printf("\n");
    printf("Operations:\n");
    printf("  Total uploads: %lld\n", (long long)total_uploads);
    printf("  Total downloads: %lld\n", (long long)total_downloads);
    printf("  Total operations: %lld\n", (long long)(total_uploads + total_downloads));
    printf("\n");
}

int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *command = NULL;
    char *filter_group = NULL;
    int json_output = 0;
    int verbose = 0;
    int result;
    ConnectionInfo *pTrackerServer;
    ClusterInfo cluster;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"group", required_argument, 0, 'g'},
        {"json", no_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:g:jvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'g':
                filter_group = optarg;
                break;
            case 'j':
                json_output = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind < argc) {
        command = argv[optind];
    } else {
        fprintf(stderr, "ERROR: Command required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init();
    g_log_context.log_level = verbose ? LOG_INFO : LOG_ERR;
    
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
    
    memset(&cluster, 0, sizeof(cluster));
    
    result = query_cluster_info(pTrackerServer, &cluster);
    if (result != 0) {
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return result;
    }
    
    if (strcmp(command, "status") == 0) {
        print_cluster_status(&cluster, json_output);
    } else if (strcmp(command, "groups") == 0) {
        print_group_list(&cluster, json_output);
    } else if (strcmp(command, "servers") == 0) {
        print_server_list(&cluster, filter_group, json_output);
    } else if (strcmp(command, "balance") == 0) {
        check_cluster_balance(&cluster);
    } else if (strcmp(command, "health") == 0) {
        perform_health_check(&cluster);
    } else if (strcmp(command, "summary") == 0) {
        print_cluster_summary(&cluster);
    } else {
        fprintf(stderr, "ERROR: Unknown command: %s\n", command);
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        return 1;
    }
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return 0;
}
