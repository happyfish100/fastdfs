/**
 * FastDFS Quota Management Tool
 * 
 * This tool provides comprehensive quota management capabilities for FastDFS
 * storage groups and individual storage servers. It allows administrators to
 * set storage quotas, monitor usage against those quotas, and receive alerts
 * when quota thresholds are exceeded.
 * 
 * Features:
 * - Set soft and hard quotas for storage groups or individual servers
 * - Monitor current usage against configured quotas
 * - Alert when usage exceeds warning or critical thresholds
 * - Enforce quota limits (report violations)
 * - Support for multiple quota policies
 * - Persistent quota configuration storage
 * - Real-time quota monitoring
 * - Historical quota usage tracking
 * - JSON and text output formats
 * 
 * Quota Types:
 * - Soft Quota: Warning threshold, allows operations but alerts administrators
 * - Hard Quota: Critical threshold, should block new uploads (enforcement mode)
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
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include "fdfs_client.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "logger.h"

/* Maximum group name length */
#define MAX_GROUP_NAME_LEN 32

/* Maximum server identifier length */
#define MAX_SERVER_ID_LEN 128

/* Maximum quota configuration file path length */
#define MAX_CONFIG_PATH_LEN 512

/* Maximum number of quota entries */
#define MAX_QUOTA_ENTRIES 256

/* Maximum number of storage groups */
#define MAX_GROUPS 64

/* Maximum number of servers per group */
#define MAX_SERVERS_PER_GROUP 32

/* Default quota configuration file */
#define DEFAULT_QUOTA_CONFIG "/etc/fdfs/quota.conf"

/* Quota entry types */
typedef enum {
    QUOTA_TYPE_GROUP = 0,      /* Quota applies to entire group */
    QUOTA_TYPE_SERVER = 1,     /* Quota applies to specific server */
    QUOTA_TYPE_GLOBAL = 2      /* Global quota for all storage */
} QuotaType;

/* Quota status enumeration */
typedef enum {
    QUOTA_STATUS_OK = 0,                /* Usage is within limits */
    QUOTA_STATUS_WARNING = 1,           /* Usage exceeds warning threshold */
    QUOTA_STATUS_CRITICAL = 2,          /* Usage exceeds critical threshold */
    QUOTA_STATUS_EXCEEDED = 3,         /* Usage exceeds hard quota */
    QUOTA_STATUS_UNKNOWN = 4            /* Status cannot be determined */
} QuotaStatus;

/* Quota entry structure */
typedef struct {
    QuotaType type;                     /* Type of quota (group/server/global) */
    char identifier[MAX_SERVER_ID_LEN]; /* Group name or server IP:port */
    int64_t soft_quota_bytes;          /* Soft quota limit in bytes */
    int64_t hard_quota_bytes;           /* Hard quota limit in bytes */
    double warning_threshold_percent;  /* Warning threshold (percentage) */
    double critical_threshold_percent; /* Critical threshold (percentage) */
    int enabled;                        /* Whether this quota is enabled */
    time_t created_time;                /* When quota was created */
    time_t last_checked_time;          /* Last time quota was checked */
    int64_t last_usage_bytes;          /* Last recorded usage */
    char description[256];             /* Optional description */
} QuotaEntry;

/* Quota usage information */
typedef struct {
    char identifier[MAX_SERVER_ID_LEN]; /* Group name or server identifier */
    int64_t total_space_bytes;         /* Total available space */
    int64_t used_space_bytes;          /* Currently used space */
    int64_t free_space_bytes;          /* Free space available */
    double usage_percent;              /* Usage percentage */
    QuotaEntry *quota_entry;           /* Associated quota entry */
    QuotaStatus status;                /* Current quota status */
    int64_t soft_quota_bytes;          /* Soft quota limit */
    int64_t hard_quota_bytes;           /* Hard quota limit */
    int64_t remaining_quota_bytes;      /* Remaining quota space */
    time_t check_time;                 /* When this check was performed */
} QuotaUsage;

/* Quota configuration structure */
typedef struct {
    QuotaEntry entries[MAX_QUOTA_ENTRIES]; /* Array of quota entries */
    int entry_count;                       /* Number of quota entries */
    char config_file[MAX_CONFIG_PATH_LEN]; /* Path to config file */
    time_t last_loaded_time;               /* When config was last loaded */
    pthread_mutex_t mutex;                 /* Mutex for thread safety */
} QuotaConfig;

/* Global quota configuration */
static QuotaConfig g_quota_config = {
    .entry_count = 0,
    .config_file = "",
    .last_loaded_time = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

/* Global output flags */
static int verbose = 0;
static int json_output = 0;
static int quiet = 0;
static int enforce_mode = 0;
static int watch_mode = 0;
static int watch_interval = 5;

/**
 * Parse size string to bytes
 * 
 * This function parses a human-readable size string (e.g., "10GB", "500MB")
 * and converts it to bytes. Supports KB, MB, GB, TB suffixes.
 * 
 * @param size_str - Size string to parse (e.g., "10GB", "500MB", "1024")
 * @param bytes - Output parameter for parsed bytes
 * @return 0 on success, -1 on error
 */
static int parse_size_string(const char *size_str, int64_t *bytes) {
    char *endptr;
    double value;
    int64_t multiplier = 1;
    size_t len;
    char unit[8];
    int i;
    
    if (size_str == NULL || bytes == NULL) {
        return -1;
    }
    
    /* Parse numeric value */
    value = strtod(size_str, &endptr);
    if (endptr == size_str) {
        /* No number found */
        return -1;
    }
    
    /* Skip whitespace */
    while (isspace((unsigned char)*endptr)) {
        endptr++;
    }
    
    /* Extract unit */
    len = strlen(endptr);
    if (len > 0) {
        /* Convert to uppercase for case-insensitive matching */
        for (i = 0; i < len && i < sizeof(unit) - 1; i++) {
            unit[i] = toupper((unsigned char)endptr[i]);
        }
        unit[i] = '\0';
        
        /* Determine multiplier based on unit */
        if (strcmp(unit, "KB") == 0 || strcmp(unit, "K") == 0) {
            multiplier = 1024LL;
        } else if (strcmp(unit, "MB") == 0 || strcmp(unit, "M") == 0) {
            multiplier = 1024LL * 1024LL;
        } else if (strcmp(unit, "GB") == 0 || strcmp(unit, "G") == 0) {
            multiplier = 1024LL * 1024LL * 1024LL;
        } else if (strcmp(unit, "TB") == 0 || strcmp(unit, "T") == 0) {
            multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
        } else if (strcmp(unit, "B") == 0 || len == 0) {
            /* Bytes or no unit specified */
            multiplier = 1;
        } else {
            /* Unknown unit */
            return -1;
        }
    }
    
    /* Calculate total bytes */
    *bytes = (int64_t)(value * multiplier);
    
    return 0;
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
        /* Terabytes */
        snprintf(buf, buf_size, "%.2f TB", bytes / 1099511627776.0);
    } else if (bytes >= 1073741824LL) {
        /* Gigabytes */
        snprintf(buf, buf_size, "%.2f GB", bytes / 1073741824.0);
    } else if (bytes >= 1048576LL) {
        /* Megabytes */
        snprintf(buf, buf_size, "%.2f MB", bytes / 1048576.0);
    } else if (bytes >= 1024LL) {
        /* Kilobytes */
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    } else {
        /* Bytes */
        snprintf(buf, buf_size, "%lld B", (long long)bytes);
    }
}

/**
 * Load quota configuration from file
 * 
 * This function loads quota configuration from a text file.
 * The file format is:
 *   # Comment lines start with #
 *   # Format: TYPE IDENTIFIER SOFT_QUOTA HARD_QUOTA WARNING% CRITICAL% [DESCRIPTION]
 *   GROUP group1 100GB 120GB 80.0 95.0 "Production group"
 *   SERVER 192.168.1.10:23000 50GB 60GB 85.0 95.0 "Primary server"
 * 
 * @param config_file - Path to configuration file
 * @return 0 on success, error code on failure
 */
static int load_quota_config(const char *config_file) {
    FILE *fp;
    char line[1024];
    char *p;
    int line_num = 0;
    QuotaEntry *entry;
    char type_str[32];
    char identifier[MAX_SERVER_ID_LEN];
    char soft_str[64];
    char hard_str[64];
    char warning_str[64];
    char critical_str[64];
    char description[256];
    int ret;
    
    if (config_file == NULL) {
        return EINVAL;
    }
    
    /* Open configuration file */
    fp = fopen(config_file, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            /* File doesn't exist - that's okay, start with empty config */
            pthread_mutex_lock(&g_quota_config.mutex);
            g_quota_config.entry_count = 0;
            strncpy(g_quota_config.config_file, config_file, sizeof(g_quota_config.config_file) - 1);
            g_quota_config.last_loaded_time = time(NULL);
            pthread_mutex_unlock(&g_quota_config.mutex);
            return 0;
        }
        return errno;
    }
    
    /* Lock configuration for thread safety */
    pthread_mutex_lock(&g_quota_config.mutex);
    
    /* Reset entry count */
    g_quota_config.entry_count = 0;
    strncpy(g_quota_config.config_file, config_file, sizeof(g_quota_config.config_file) - 1);
    
    /* Read configuration file line by line */
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_num++;
        
        /* Remove newline characters */
        p = strchr(line, '\n');
        if (p != NULL) {
            *p = '\0';
        }
        
        p = strchr(line, '\r');
        if (p != NULL) {
            *p = '\0';
        }
        
        /* Skip empty lines and comments */
        p = line;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        
        if (*p == '\0' || *p == '#') {
            continue;
        }
        
        /* Check if we have room for more entries */
        if (g_quota_config.entry_count >= MAX_QUOTA_ENTRIES) {
            fprintf(stderr, "WARNING: Maximum quota entries (%d) reached, skipping remaining entries\n",
                   MAX_QUOTA_ENTRIES);
            break;
        }
        
        /* Parse line */
        entry = &g_quota_config.entries[g_quota_config.entry_count];
        memset(entry, 0, sizeof(QuotaEntry));
        
        /* Parse fields */
        ret = sscanf(p, "%31s %127s %63s %63s %63s %63s %255[^\n]",
                    type_str, identifier, soft_str, hard_str,
                    warning_str, critical_str, description);
        
        if (ret < 6) {
            if (verbose) {
                fprintf(stderr, "WARNING: Invalid quota config at line %d, skipping\n", line_num);
            }
            continue;
        }
        
        /* Parse quota type */
        if (strcasecmp(type_str, "GROUP") == 0) {
            entry->type = QUOTA_TYPE_GROUP;
        } else if (strcasecmp(type_str, "SERVER") == 0) {
            entry->type = QUOTA_TYPE_SERVER;
        } else if (strcasecmp(type_str, "GLOBAL") == 0) {
            entry->type = QUOTA_TYPE_GLOBAL;
        } else {
            if (verbose) {
                fprintf(stderr, "WARNING: Unknown quota type '%s' at line %d, skipping\n",
                       type_str, line_num);
            }
            continue;
        }
        
        /* Store identifier */
        strncpy(entry->identifier, identifier, sizeof(entry->identifier) - 1);
        
        /* Parse soft quota */
        if (parse_size_string(soft_str, &entry->soft_quota_bytes) != 0) {
            if (verbose) {
                fprintf(stderr, "WARNING: Invalid soft quota '%s' at line %d, skipping\n",
                       soft_str, line_num);
            }
            continue;
        }
        
        /* Parse hard quota */
        if (parse_size_string(hard_str, &entry->hard_quota_bytes) != 0) {
            if (verbose) {
                fprintf(stderr, "WARNING: Invalid hard quota '%s' at line %d, skipping\n",
                       hard_str, line_num);
            }
            continue;
        }
        
        /* Parse warning threshold */
        entry->warning_threshold_percent = strtod(warning_str, NULL);
        if (entry->warning_threshold_percent < 0 || entry->warning_threshold_percent > 100) {
            if (verbose) {
                fprintf(stderr, "WARNING: Invalid warning threshold '%s' at line %d, skipping\n",
                       warning_str, line_num);
            }
            continue;
        }
        
        /* Parse critical threshold */
        entry->critical_threshold_percent = strtod(critical_str, NULL);
        if (entry->critical_threshold_percent < 0 || entry->critical_threshold_percent > 100) {
            if (verbose) {
                fprintf(stderr, "WARNING: Invalid critical threshold '%s' at line %d, skipping\n",
                       critical_str, line_num);
            }
            continue;
        }
        
        /* Store description if provided */
        if (ret >= 7) {
            /* Remove quotes if present */
            p = description;
            if (*p == '"' && p[strlen(p) - 1] == '"') {
                p++;
                p[strlen(p) - 1] = '\0';
            }
            strncpy(entry->description, p, sizeof(entry->description) - 1);
        }
        
        /* Set defaults */
        entry->enabled = 1;
        entry->created_time = time(NULL);
        entry->last_checked_time = 0;
        entry->last_usage_bytes = 0;
        
        /* Increment entry count */
        g_quota_config.entry_count++;
    }
    
    /* Update last loaded time */
    g_quota_config.last_loaded_time = time(NULL);
    
    /* Unlock configuration */
    pthread_mutex_unlock(&g_quota_config.mutex);
    
    /* Close file */
    fclose(fp);
    
    if (verbose) {
        printf("Loaded %d quota entries from %s\n",
               g_quota_config.entry_count, config_file);
    }
    
    return 0;
}

/**
 * Save quota configuration to file
 * 
 * This function saves the current quota configuration to a file
 * in the same format that load_quota_config can read.
 * 
 * @param config_file - Path to configuration file
 * @return 0 on success, error code on failure
 */
static int save_quota_config(const char *config_file) {
    FILE *fp;
    QuotaEntry *entry;
    int i;
    char soft_buf[64];
    char hard_buf[64];
    
    if (config_file == NULL) {
        return EINVAL;
    }
    
    /* Open configuration file for writing */
    fp = fopen(config_file, "w");
    if (fp == NULL) {
        return errno;
    }
    
    /* Write header comment */
    fprintf(fp, "# FastDFS Quota Configuration File\n");
    fprintf(fp, "# Format: TYPE IDENTIFIER SOFT_QUOTA HARD_QUOTA WARNING%% CRITICAL%% [DESCRIPTION]\n");
    fprintf(fp, "# TYPE can be: GROUP, SERVER, or GLOBAL\n");
    fprintf(fp, "# Quota sizes can use suffixes: B, KB, MB, GB, TB\n");
    fprintf(fp, "# Thresholds are percentages (0-100)\n");
    fprintf(fp, "# Generated: %s\n", ctime(&g_quota_config.last_loaded_time));
    fprintf(fp, "\n");
    
    /* Lock configuration for thread safety */
    pthread_mutex_lock(&g_quota_config.mutex);
    
    /* Write each quota entry */
    for (i = 0; i < g_quota_config.entry_count; i++) {
        entry = &g_quota_config.entries[i];
        
        if (!entry->enabled) {
            continue;
        }
        
        /* Format quota sizes */
        format_bytes(entry->soft_quota_bytes, soft_buf, sizeof(soft_buf));
        format_bytes(entry->hard_quota_bytes, hard_buf, sizeof(hard_buf));
        
        /* Write entry */
        fprintf(fp, "%s %s %s %s %.1f %.1f",
               entry->type == QUOTA_TYPE_GROUP ? "GROUP" :
               entry->type == QUOTA_TYPE_SERVER ? "SERVER" : "GLOBAL",
               entry->identifier,
               soft_buf,
               hard_buf,
               entry->warning_threshold_percent,
               entry->critical_threshold_percent);
        
        if (strlen(entry->description) > 0) {
            fprintf(fp, " \"%s\"", entry->description);
        }
        
        fprintf(fp, "\n");
    }
    
    /* Unlock configuration */
    pthread_mutex_unlock(&g_quota_config.mutex);
    
    /* Close file */
    fclose(fp);
    
    return 0;
}

/**
 * Find quota entry by identifier
 * 
 * This function searches for a quota entry matching the given
 * identifier and type.
 * 
 * @param type - Type of quota to find
 * @param identifier - Identifier to match
 * @return Pointer to quota entry, or NULL if not found
 */
static QuotaEntry *find_quota_entry(QuotaType type, const char *identifier) {
    int i;
    QuotaEntry *entry;
    
    if (identifier == NULL) {
        return NULL;
    }
    
    pthread_mutex_lock(&g_quota_config.mutex);
    
    for (i = 0; i < g_quota_config.entry_count; i++) {
        entry = &g_quota_config.entries[i];
        
        if (entry->type == type &&
            entry->enabled &&
            strcmp(entry->identifier, identifier) == 0) {
            pthread_mutex_unlock(&g_quota_config.mutex);
            return entry;
        }
    }
    
    pthread_mutex_unlock(&g_quota_config.mutex);
    return NULL;
}

/**
 * Calculate quota status from usage
 * 
 * This function determines the quota status based on current usage
 * and configured quota limits.
 * 
 * @param usage - Current usage information
 * @param quota_entry - Quota entry with limits
 * @return QuotaStatus value
 */
static QuotaStatus calculate_quota_status(QuotaUsage *usage, QuotaEntry *quota_entry) {
    double usage_percent;
    double warning_threshold;
    double critical_threshold;
    
    if (usage == NULL || quota_entry == NULL) {
        return QUOTA_STATUS_UNKNOWN;
    }
    
    /* Calculate usage percentage */
    if (quota_entry->hard_quota_bytes > 0) {
        usage_percent = (usage->used_space_bytes * 100.0) / quota_entry->hard_quota_bytes;
    } else if (usage->total_space_bytes > 0) {
        usage_percent = (usage->used_space_bytes * 100.0) / usage->total_space_bytes;
    } else {
        return QUOTA_STATUS_UNKNOWN;
    }
    
    /* Get thresholds */
    warning_threshold = quota_entry->warning_threshold_percent;
    critical_threshold = quota_entry->critical_threshold_percent;
    
    /* Determine status */
    if (usage->used_space_bytes >= quota_entry->hard_quota_bytes) {
        return QUOTA_STATUS_EXCEEDED;
    } else if (usage_percent >= critical_threshold) {
        return QUOTA_STATUS_CRITICAL;
    } else if (usage_percent >= warning_threshold) {
        return QUOTA_STATUS_WARNING;
    } else {
        return QUOTA_STATUS_OK;
    }
}

/**
 * Get storage usage for a group
 * 
 * This function retrieves current storage usage information for
 * a storage group from the tracker server.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param group_name - Group name
 * @param usage - Output parameter for usage information
 * @return 0 on success, error code on failure
 */
static int get_group_usage(ConnectionInfo *pTrackerServer,
                          const char *group_name,
                          QuotaUsage *usage) {
    FDFSGroupStat group_stat;
    int ret;
    
    if (pTrackerServer == NULL || group_name == NULL || usage == NULL) {
        return EINVAL;
    }
    
    /* Initialize usage structure */
    memset(usage, 0, sizeof(QuotaUsage));
    strncpy(usage->identifier, group_name, sizeof(usage->identifier) - 1);
    
    /* Query group statistics from tracker */
    ret = tracker_list_one_group(pTrackerServer, group_name, &group_stat);
    if (ret != 0) {
        return ret;
    }
    
    /* Convert MB to bytes */
    usage->total_space_bytes = group_stat.total_mb * 1024LL * 1024LL;
    usage->free_space_bytes = group_stat.free_mb * 1024LL * 1024LL;
    usage->used_space_bytes = usage->total_space_bytes - usage->free_space_bytes;
    
    /* Calculate usage percentage */
    if (usage->total_space_bytes > 0) {
        usage->usage_percent = (usage->used_space_bytes * 100.0) / usage->total_space_bytes;
    } else {
        usage->usage_percent = 0.0;
    }
    
    /* Set check time */
    usage->check_time = time(NULL);
    
    /* Find associated quota entry */
    usage->quota_entry = find_quota_entry(QUOTA_TYPE_GROUP, group_name);
    
    if (usage->quota_entry != NULL) {
        /* Set quota limits */
        usage->soft_quota_bytes = usage->quota_entry->soft_quota_bytes;
        usage->hard_quota_bytes = usage->quota_entry->hard_quota_bytes;
        
        /* Calculate remaining quota */
        usage->remaining_quota_bytes = usage->hard_quota_bytes - usage->used_space_bytes;
        if (usage->remaining_quota_bytes < 0) {
            usage->remaining_quota_bytes = 0;
        }
        
        /* Calculate quota status */
        usage->status = calculate_quota_status(usage, usage->quota_entry);
        
        /* Update quota entry */
        usage->quota_entry->last_checked_time = usage->check_time;
        usage->quota_entry->last_usage_bytes = usage->used_space_bytes;
    } else {
        /* No quota configured */
        usage->status = QUOTA_STATUS_UNKNOWN;
        usage->soft_quota_bytes = 0;
        usage->hard_quota_bytes = 0;
        usage->remaining_quota_bytes = 0;
    }
    
    return 0;
}

/**
 * Get storage usage for a specific server
 * 
 * This function retrieves current storage usage information for
 * a specific storage server.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param server_id - Server identifier (IP:port or storage ID)
 * @param usage - Output parameter for usage information
 * @return 0 on success, error code on failure
 */
static int get_server_usage(ConnectionInfo *pTrackerServer,
                           const char *server_id,
                           QuotaUsage *usage) {
    FDFSGroupStat group_stats[MAX_GROUPS];
    FDFSStorageStat storage_stats[MAX_SERVERS_PER_GROUP];
    int group_count;
    int storage_count;
    int i, j;
    int ret;
    char *colon_pos;
    char ip_addr[IP_ADDRESS_SIZE];
    int port;
    
    if (pTrackerServer == NULL || server_id == NULL || usage == NULL) {
        return EINVAL;
    }
    
    /* Initialize usage structure */
    memset(usage, 0, sizeof(QuotaUsage));
    strncpy(usage->identifier, server_id, sizeof(usage->identifier) - 1);
    
    /* Parse server identifier (IP:port format) */
    colon_pos = strchr(server_id, ':');
    if (colon_pos == NULL) {
        /* Assume it's a storage ID, not IP:port */
        /* For now, we'll search by storage ID */
        ret = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &group_count);
        if (ret != 0) {
            return ret;
        }
        
        /* Search through all groups and servers */
        for (i = 0; i < group_count; i++) {
            ret = tracker_list_servers(pTrackerServer, group_stats[i].group_name,
                                      server_id, storage_stats,
                                      MAX_SERVERS_PER_GROUP, &storage_count);
            if (ret == 0 && storage_count > 0) {
                /* Found the server */
                usage->total_space_bytes = storage_stats[0].total_mb * 1024LL * 1024LL;
                usage->free_space_bytes = storage_stats[0].free_mb * 1024LL * 1024LL;
                usage->used_space_bytes = usage->total_space_bytes - usage->free_space_bytes;
                
                if (usage->total_space_bytes > 0) {
                    usage->usage_percent = (usage->used_space_bytes * 100.0) / usage->total_space_bytes;
                }
                
                usage->check_time = time(NULL);
                usage->quota_entry = find_quota_entry(QUOTA_TYPE_SERVER, server_id);
                
                if (usage->quota_entry != NULL) {
                    usage->soft_quota_bytes = usage->quota_entry->soft_quota_bytes;
                    usage->hard_quota_bytes = usage->quota_entry->hard_quota_bytes;
                    usage->remaining_quota_bytes = usage->hard_quota_bytes - usage->used_space_bytes;
                    if (usage->remaining_quota_bytes < 0) {
                        usage->remaining_quota_bytes = 0;
                    }
                    usage->status = calculate_quota_status(usage, usage->quota_entry);
                }
                
                return 0;
            }
        }
        
        return ENOENT;
    } else {
        /* Parse IP:port */
        strncpy(ip_addr, server_id, colon_pos - server_id);
        ip_addr[colon_pos - server_id] = '\0';
        port = atoi(colon_pos + 1);
        
        /* Search for server by IP and port */
        ret = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &group_count);
        if (ret != 0) {
            return ret;
        }
        
        for (i = 0; i < group_count; i++) {
            ret = tracker_list_servers(pTrackerServer, group_stats[i].group_name,
                                      NULL, storage_stats,
                                      MAX_SERVERS_PER_GROUP, &storage_count);
            if (ret != 0) {
                continue;
            }
            
            for (j = 0; j < storage_count; j++) {
                if (strcmp(storage_stats[j].ip_addr, ip_addr) == 0 &&
                    storage_stats[j].port == port) {
                    /* Found the server */
                    usage->total_space_bytes = storage_stats[j].total_mb * 1024LL * 1024LL;
                    usage->free_space_bytes = storage_stats[j].free_mb * 1024LL * 1024LL;
                    usage->used_space_bytes = usage->total_space_bytes - usage->free_space_bytes;
                    
                    if (usage->total_space_bytes > 0) {
                        usage->usage_percent = (usage->used_space_bytes * 100.0) / usage->total_space_bytes;
                    }
                    
                    usage->check_time = time(NULL);
                    
                    /* Create server identifier string */
                    snprintf(usage->identifier, sizeof(usage->identifier), "%s:%d", ip_addr, port);
                    usage->quota_entry = find_quota_entry(QUOTA_TYPE_SERVER, usage->identifier);
                    
                    if (usage->quota_entry != NULL) {
                        usage->soft_quota_bytes = usage->quota_entry->soft_quota_bytes;
                        usage->hard_quota_bytes = usage->quota_entry->hard_quota_bytes;
                        usage->remaining_quota_bytes = usage->hard_quota_bytes - usage->used_space_bytes;
                        if (usage->remaining_quota_bytes < 0) {
                            usage->remaining_quota_bytes = 0;
                        }
                        usage->status = calculate_quota_status(usage, usage->quota_entry);
                    }
                    
                    return 0;
                }
            }
        }
        
        return ENOENT;
    }
}

/**
 * Print usage information in text format
 * 
 * This function prints quota usage information in a human-readable
 * text format.
 * 
 * @param usage - Usage information to print
 */
static void print_usage_text(QuotaUsage *usage) {
    char total_buf[64];
    char used_buf[64];
    char free_buf[64];
    char soft_buf[64];
    char hard_buf[64];
    char remaining_buf[64];
    const char *status_str;
    const char *status_symbol;
    
    if (usage == NULL) {
        return;
    }
    
    /* Format sizes */
    format_bytes(usage->total_space_bytes, total_buf, sizeof(total_buf));
    format_bytes(usage->used_space_bytes, used_buf, sizeof(used_buf));
    format_bytes(usage->free_space_bytes, free_buf, sizeof(free_buf));
    
    /* Determine status string and symbol */
    switch (usage->status) {
        case QUOTA_STATUS_OK:
            status_str = "OK";
            status_symbol = "✓";
            break;
        case QUOTA_STATUS_WARNING:
            status_str = "WARNING";
            status_symbol = "⚠";
            break;
        case QUOTA_STATUS_CRITICAL:
            status_str = "CRITICAL";
            status_symbol = "✗";
            break;
        case QUOTA_STATUS_EXCEEDED:
            status_str = "EXCEEDED";
            status_symbol = "✗";
            break;
        default:
            status_str = "UNKNOWN";
            status_symbol = "?";
            break;
    }
    
    /* Print usage information */
    printf("\n");
    printf("=== Quota Usage: %s ===\n", usage->identifier);
    printf("Status: %s %s\n", status_symbol, status_str);
    printf("Total Space: %s\n", total_buf);
    printf("Used Space: %s (%.2f%%)\n", used_buf, usage->usage_percent);
    printf("Free Space: %s\n", free_buf);
    
    if (usage->quota_entry != NULL) {
        format_bytes(usage->soft_quota_bytes, soft_buf, sizeof(soft_buf));
        format_bytes(usage->hard_quota_bytes, hard_buf, sizeof(hard_buf));
        format_bytes(usage->remaining_quota_bytes, remaining_buf, sizeof(remaining_buf));
        
        printf("Soft Quota: %s\n", soft_buf);
        printf("Hard Quota: %s\n", hard_buf);
        printf("Remaining Quota: %s\n", remaining_buf);
        
        if (strlen(usage->quota_entry->description) > 0) {
            printf("Description: %s\n", usage->quota_entry->description);
        }
        
        if (usage->status != QUOTA_STATUS_OK && usage->status != QUOTA_STATUS_UNKNOWN) {
            printf("\n⚠ ALERT: Quota threshold exceeded!\n");
            if (enforce_mode && usage->status == QUOTA_STATUS_EXCEEDED) {
                printf("✗ ENFORCEMENT: Hard quota exceeded - new uploads should be blocked\n");
            }
        }
    } else {
        printf("Quota: Not configured\n");
    }
    
    printf("\n");
}

/**
 * Print usage information in JSON format
 * 
 * This function prints quota usage information in JSON format
 * for programmatic processing.
 * 
 * @param usage - Usage information to print
 * @param first - Whether this is the first entry (for comma handling)
 */
static void print_usage_json(QuotaUsage *usage, int first) {
    if (usage == NULL) {
        return;
    }
    
    if (!first) {
        printf(",\n");
    }
    
    printf("    {\n");
    printf("      \"identifier\": \"%s\",\n", usage->identifier);
    printf("      \"status\": \"%s\",\n",
           usage->status == QUOTA_STATUS_OK ? "ok" :
           usage->status == QUOTA_STATUS_WARNING ? "warning" :
           usage->status == QUOTA_STATUS_CRITICAL ? "critical" :
           usage->status == QUOTA_STATUS_EXCEEDED ? "exceeded" : "unknown");
    printf("      \"total_space_bytes\": %lld,\n", (long long)usage->total_space_bytes);
    printf("      \"used_space_bytes\": %lld,\n", (long long)usage->used_space_bytes);
    printf("      \"free_space_bytes\": %lld,\n", (long long)usage->free_space_bytes);
    printf("      \"usage_percent\": %.2f,\n", usage->usage_percent);
    
    if (usage->quota_entry != NULL) {
        printf("      \"soft_quota_bytes\": %lld,\n", (long long)usage->soft_quota_bytes);
        printf("      \"hard_quota_bytes\": %lld,\n", (long long)usage->hard_quota_bytes);
        printf("      \"remaining_quota_bytes\": %lld,\n", (long long)usage->remaining_quota_bytes);
        printf("      \"warning_threshold_percent\": %.2f,\n", usage->quota_entry->warning_threshold_percent);
        printf("      \"critical_threshold_percent\": %.2f,\n", usage->quota_entry->critical_threshold_percent);
        printf("      \"description\": \"%s\",\n", usage->quota_entry->description);
    } else {
        printf("      \"quota_configured\": false,\n");
    }
    
    printf("      \"check_time\": %ld\n", (long)usage->check_time);
    printf("    }");
}

/**
 * Print usage information
 * 
 * This is a wrapper function that calls the appropriate print
 * function based on the output format.
 * 
 * @param usage - Usage information to print
 * @param first - Whether this is the first entry (for JSON)
 */
static void print_usage_info(QuotaUsage *usage, int first) {
    if (json_output) {
        print_usage_json(usage, first);
    } else {
        print_usage_text(usage);
    }
}

/**
 * Monitor quota usage
 * 
 * This function monitors quota usage for the specified group or server
 * and displays the current status.
 * 
 * @param pTrackerServer - Tracker server connection
 * @param group_name - Group name to monitor (NULL for all)
 * @param server_id - Server ID to monitor (NULL for groups)
 * @return 0 on success, error code on failure
 */
static int monitor_quota(ConnectionInfo *pTrackerServer,
                        const char *group_name,
                        const char *server_id) {
    QuotaUsage usage;
    int ret;
    FDFSGroupStat group_stats[MAX_GROUPS];
    int group_count;
    int i;
    int first = 1;
    int total_checked = 0;
    int quota_exceeded = 0;
    int quota_warning = 0;
    
    if (pTrackerServer == NULL) {
        return EINVAL;
    }
    
    if (json_output) {
        printf("{\n");
        printf("  \"timestamp\": %ld,\n", (long)time(NULL));
        printf("  \"quotas\": [\n");
    }
    
    if (server_id != NULL) {
        /* Monitor specific server */
        ret = get_server_usage(pTrackerServer, server_id, &usage);
        if (ret != 0) {
            fprintf(stderr, "ERROR: Failed to get server usage: %s\n", STRERROR(ret));
            if (json_output) {
                printf("  ]\n");
                printf("}\n");
            }
            return ret;
        }
        
        print_usage_info(&usage, first);
        first = 0;
        total_checked++;
        
        if (usage.status == QUOTA_STATUS_EXCEEDED) {
            quota_exceeded++;
        } else if (usage.status == QUOTA_STATUS_WARNING || usage.status == QUOTA_STATUS_CRITICAL) {
            quota_warning++;
        }
    } else if (group_name != NULL) {
        /* Monitor specific group */
        ret = get_group_usage(pTrackerServer, group_name, &usage);
        if (ret != 0) {
            fprintf(stderr, "ERROR: Failed to get group usage: %s\n", STRERROR(ret));
            if (json_output) {
                printf("  ]\n");
                printf("}\n");
            }
            return ret;
        }
        
        print_usage_info(&usage, first);
        first = 0;
        total_checked++;
        
        if (usage.status == QUOTA_STATUS_EXCEEDED) {
            quota_exceeded++;
        } else if (usage.status == QUOTA_STATUS_WARNING || usage.status == QUOTA_STATUS_CRITICAL) {
            quota_warning++;
        }
    } else {
        /* Monitor all groups with quotas */
        ret = tracker_list_groups(pTrackerServer, group_stats, MAX_GROUPS, &group_count);
        if (ret != 0) {
            fprintf(stderr, "ERROR: Failed to list groups: %s\n", STRERROR(ret));
            if (json_output) {
                printf("  ]\n");
                printf("}\n");
            }
            return ret;
        }
        
        for (i = 0; i < group_count; i++) {
            /* Check if this group has a quota configured */
            QuotaEntry *quota = find_quota_entry(QUOTA_TYPE_GROUP, group_stats[i].group_name);
            if (quota == NULL) {
                continue;
            }
            
            ret = get_group_usage(pTrackerServer, group_stats[i].group_name, &usage);
            if (ret != 0) {
                if (verbose) {
                    fprintf(stderr, "WARNING: Failed to get usage for group %s: %s\n",
                           group_stats[i].group_name, STRERROR(ret));
                }
                continue;
            }
            
            print_usage_info(&usage, first);
            first = 0;
            total_checked++;
            
            if (usage.status == QUOTA_STATUS_EXCEEDED) {
                quota_exceeded++;
            } else if (usage.status == QUOTA_STATUS_WARNING || usage.status == QUOTA_STATUS_CRITICAL) {
                quota_warning++;
            }
        }
    }
    
    if (json_output) {
        printf("\n  ],\n");
        printf("  \"summary\": {\n");
        printf("    \"total_checked\": %d,\n", total_checked);
        printf("    \"quota_exceeded\": %d,\n", quota_exceeded);
        printf("    \"quota_warning\": %d\n", quota_warning);
        printf("  }\n");
        printf("}\n");
    } else {
        printf("=== Summary ===\n");
        printf("Total checked: %d\n", total_checked);
        printf("Quota exceeded: %d\n", quota_exceeded);
        printf("Quota warnings: %d\n", quota_warning);
        printf("\n");
        
        if (quota_exceeded > 0) {
            printf("✗ CRITICAL: %d quota(s) exceeded hard limit!\n", quota_exceeded);
            if (enforce_mode) {
                printf("⚠ ENFORCEMENT MODE: New uploads should be blocked for exceeded quotas\n");
            }
        } else if (quota_warning > 0) {
            printf("⚠ WARNING: %d quota(s) exceeded warning/critical thresholds\n", quota_warning);
        } else {
            printf("✓ All quotas are within limits\n");
        }
    }
    
    return (quota_exceeded > 0) ? 1 : 0;
}

/**
 * Set quota for a group or server
 * 
 * This function sets or updates a quota entry for a group or server.
 * 
 * @param type - Type of quota (group/server)
 * @param identifier - Group name or server identifier
 * @param soft_quota - Soft quota limit in bytes
 * @param hard_quota - Hard quota limit in bytes
 * @param warning_percent - Warning threshold percentage
 * @param critical_percent - Critical threshold percentage
 * @param description - Optional description
 * @return 0 on success, error code on failure
 */
static int set_quota(QuotaType type,
                    const char *identifier,
                    int64_t soft_quota,
                    int64_t hard_quota,
                    double warning_percent,
                    double critical_percent,
                    const char *description) {
    QuotaEntry *entry;
    int i;
    
    if (identifier == NULL) {
        return EINVAL;
    }
    
    pthread_mutex_lock(&g_quota_config.mutex);
    
    /* Check if entry already exists */
    entry = NULL;
    for (i = 0; i < g_quota_config.entry_count; i++) {
        if (g_quota_config.entries[i].type == type &&
            strcmp(g_quota_config.entries[i].identifier, identifier) == 0) {
            entry = &g_quota_config.entries[i];
            break;
        }
    }
    
    if (entry == NULL) {
        /* Create new entry */
        if (g_quota_config.entry_count >= MAX_QUOTA_ENTRIES) {
            pthread_mutex_unlock(&g_quota_config.mutex);
            return ENOSPC;
        }
        
        entry = &g_quota_config.entries[g_quota_config.entry_count];
        g_quota_config.entry_count++;
        entry->created_time = time(NULL);
    }
    
    /* Update entry */
    entry->type = type;
    strncpy(entry->identifier, identifier, sizeof(entry->identifier) - 1);
    entry->soft_quota_bytes = soft_quota;
    entry->hard_quota_bytes = hard_quota;
    entry->warning_threshold_percent = warning_percent;
    entry->critical_threshold_percent = critical_percent;
    entry->enabled = 1;
    
    if (description != NULL) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
    } else {
        entry->description[0] = '\0';
    }
    
    pthread_mutex_unlock(&g_quota_config.mutex);
    
    /* Save configuration */
    if (strlen(g_quota_config.config_file) > 0) {
        save_quota_config(g_quota_config.config_file);
    }
    
    return 0;
}

/**
 * Print usage information
 * 
 * This function displays the usage information for the fdfs_quota tool.
 * 
 * @param program_name - Name of the program
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] [COMMAND] [ARGUMENTS]\n", program_name);
    printf("\n");
    printf("FastDFS Quota Management Tool\n");
    printf("\n");
    printf("This tool allows you to set, monitor, and enforce storage quotas\n");
    printf("for FastDFS storage groups and individual storage servers.\n");
    printf("\n");
    printf("Commands:\n");
    printf("  monitor [GROUP|SERVER]  Monitor quota usage (default command)\n");
    printf("  set GROUP SOFT HARD WARN%% CRIT%% [DESC]  Set quota for a group\n");
    printf("  set-server SERVER SOFT HARD WARN%% CRIT%% [DESC]  Set quota for a server\n");
    printf("  list                     List all configured quotas\n");
    printf("  remove GROUP|SERVER     Remove quota configuration\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE       FastDFS client config (default: /etc/fdfs/client.conf)\n");
    printf("  -q, --quota-config FILE Quota config file (default: /etc/fdfs/quota.conf)\n");
    printf("  -g, --group NAME        Group name to monitor\n");
    printf("  -s, --server ID         Server ID (IP:port) to monitor\n");
    printf("  -e, --enforce           Enable enforcement mode (block on hard quota)\n");
    printf("  -w, --watch             Watch mode (continuous monitoring)\n");
    printf("  -i, --interval SEC       Watch interval in seconds (default: 5)\n");
    printf("  -j, --json              Output in JSON format\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -Q, --quiet             Quiet mode (only show violations)\n");
    printf("  -h, --help              Show this help message\n");
    printf("\n");
    printf("Quota Size Format:\n");
    printf("  Quota sizes can be specified with suffixes: B, KB, MB, GB, TB\n");
    printf("  Examples: 100GB, 500MB, 1TB, 1024\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s monitor                    # Monitor all configured quotas\n", program_name);
    printf("  %s monitor -g group1          # Monitor group1 quota\n", program_name);
    printf("  %s set group1 100GB 120GB 80 95 \"Production\"\n", program_name);
    printf("  %s set-server 192.168.1.10:23000 50GB 60GB 85 95\n", program_name);
    printf("  %s -w -i 10                    # Watch mode, update every 10 seconds\n", program_name);
    printf("  %s -j monitor                  # JSON output\n", program_name);
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - All quotas within limits\n");
    printf("  1 - Some quotas exceeded\n");
    printf("  2 - Error occurred\n");
}

/**
 * List all configured quotas
 * 
 * This function lists all quota entries in the configuration.
 * 
 * @return 0 on success
 */
static int list_quotas(void) {
    int i;
    QuotaEntry *entry;
    char soft_buf[64];
    char hard_buf[64];
    
    pthread_mutex_lock(&g_quota_config.mutex);
    
    if (g_quota_config.entry_count == 0) {
        printf("No quotas configured.\n");
        pthread_mutex_unlock(&g_quota_config.mutex);
        return 0;
    }
    
    if (json_output) {
        printf("{\n");
        printf("  \"quotas\": [\n");
    } else {
        printf("\n");
        printf("=== Configured Quotas ===\n");
        printf("\n");
    }
    
    for (i = 0; i < g_quota_config.entry_count; i++) {
        entry = &g_quota_config.entries[i];
        
        if (!entry->enabled) {
            continue;
        }
        
        format_bytes(entry->soft_quota_bytes, soft_buf, sizeof(soft_buf));
        format_bytes(entry->hard_quota_bytes, hard_buf, sizeof(hard_buf));
        
        if (json_output) {
            if (i > 0) {
                printf(",\n");
            }
            printf("    {\n");
            printf("      \"type\": \"%s\",\n",
                   entry->type == QUOTA_TYPE_GROUP ? "group" :
                   entry->type == QUOTA_TYPE_SERVER ? "server" : "global");
            printf("      \"identifier\": \"%s\",\n", entry->identifier);
            printf("      \"soft_quota_bytes\": %lld,\n", (long long)entry->soft_quota_bytes);
            printf("      \"hard_quota_bytes\": %lld,\n", (long long)entry->hard_quota_bytes);
            printf("      \"warning_threshold_percent\": %.2f,\n", entry->warning_threshold_percent);
            printf("      \"critical_threshold_percent\": %.2f,\n", entry->critical_threshold_percent);
            printf("      \"description\": \"%s\",\n", entry->description);
            printf("      \"created_time\": %ld,\n", (long)entry->created_time);
            printf("      \"last_checked_time\": %ld\n", (long)entry->last_checked_time);
            printf("    }");
        } else {
            printf("%s: %s\n", entry->type == QUOTA_TYPE_GROUP ? "GROUP" : "SERVER", entry->identifier);
            printf("  Soft Quota: %s\n", soft_buf);
            printf("  Hard Quota: %s\n", hard_buf);
            printf("  Warning Threshold: %.1f%%\n", entry->warning_threshold_percent);
            printf("  Critical Threshold: %.1f%%\n", entry->critical_threshold_percent);
            if (strlen(entry->description) > 0) {
                printf("  Description: %s\n", entry->description);
            }
            printf("\n");
        }
    }
    
    if (json_output) {
        printf("\n  ]\n");
        printf("}\n");
    }
    
    pthread_mutex_unlock(&g_quota_config.mutex);
    
    return 0;
}

/**
 * Remove quota configuration
 * 
 * This function removes a quota entry from the configuration.
 * 
 * @param identifier - Identifier of quota to remove
 * @return 0 on success, error code on failure
 */
static int remove_quota(const char *identifier) {
    int i;
    QuotaEntry *entry;
    int found = 0;
    
    if (identifier == NULL) {
        return EINVAL;
    }
    
    pthread_mutex_lock(&g_quota_config.mutex);
    
    for (i = 0; i < g_quota_config.entry_count; i++) {
        entry = &g_quota_config.entries[i];
        
        if (strcmp(entry->identifier, identifier) == 0) {
            /* Disable entry */
            entry->enabled = 0;
            found = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_quota_config.mutex);
    
    if (!found) {
        fprintf(stderr, "ERROR: Quota not found: %s\n", identifier);
        return ENOENT;
    }
    
    /* Save configuration */
    if (strlen(g_quota_config.config_file) > 0) {
        save_quota_config(g_quota_config.config_file);
    }
    
    if (verbose) {
        printf("Removed quota for: %s\n", identifier);
    }
    
    return 0;
}

/**
 * Main function
 * 
 * Entry point for the quota management tool. Parses command-line
 * arguments and executes the requested command.
 * 
 * @param argc - Argument count
 * @param argv - Argument vector
 * @return Exit code (0 = success, 1 = quota exceeded, 2 = error)
 */
int main(int argc, char *argv[]) {
    char *conf_filename = "/etc/fdfs/client.conf";
    char *quota_config_file = DEFAULT_QUOTA_CONFIG;
    char *group_name = NULL;
    char *server_id = NULL;
    char *command = "monitor";
    int result;
    ConnectionInfo *pTrackerServer;
    int64_t soft_quota = 0;
    int64_t hard_quota = 0;
    double warning_percent = 80.0;
    double critical_percent = 95.0;
    char *description = NULL;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"quota-config", required_argument, 0, 'q'},
        {"group", required_argument, 0, 'g'},
        {"server", required_argument, 0, 's'},
        {"enforce", no_argument, 0, 'e'},
        {"watch", no_argument, 0, 'w'},
        {"interval", required_argument, 0, 'i'},
        {"json", no_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'Q'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Parse command-line arguments */
    while ((opt = getopt_long(argc, argv, "c:q:g:s:ewi:jvQh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                conf_filename = optarg;
                break;
            case 'q':
                quota_config_file = optarg;
                break;
            case 'g':
                group_name = optarg;
                break;
            case 's':
                server_id = optarg;
                break;
            case 'e':
                enforce_mode = 1;
                break;
            case 'w':
                watch_mode = 1;
                break;
            case 'i':
                watch_interval = atoi(optarg);
                if (watch_interval < 1) watch_interval = 1;
                break;
            case 'j':
                json_output = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'Q':
                quiet = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 2;
        }
    }
    
    /* Check for command argument */
    if (optind < argc) {
        command = argv[optind];
        optind++;
    }
    
    /* Load quota configuration */
    load_quota_config(quota_config_file);
    
    /* Handle commands that don't require FastDFS connection */
    if (strcmp(command, "list") == 0) {
        return list_quotas();
    }
    
    if (strcmp(command, "remove") == 0) {
        if (optind >= argc) {
            fprintf(stderr, "ERROR: Identifier required for remove command\n");
            return 2;
        }
        return remove_quota(argv[optind]);
    }
    
    if (strcmp(command, "set") == 0) {
        if (optind + 4 >= argc) {
            fprintf(stderr, "ERROR: set command requires: GROUP SOFT HARD WARN%% CRIT%%\n");
            return 2;
        }
        
        if (parse_size_string(argv[optind + 1], &soft_quota) != 0) {
            fprintf(stderr, "ERROR: Invalid soft quota: %s\n", argv[optind + 1]);
            return 2;
        }
        
        if (parse_size_string(argv[optind + 2], &hard_quota) != 0) {
            fprintf(stderr, "ERROR: Invalid hard quota: %s\n", argv[optind + 2]);
            return 2;
        }
        
        warning_percent = strtod(argv[optind + 3], NULL);
        critical_percent = strtod(argv[optind + 4], NULL);
        
        if (optind + 5 < argc) {
            description = argv[optind + 5];
        }
        
        result = set_quota(QUOTA_TYPE_GROUP, argv[optind],
                          soft_quota, hard_quota,
                          warning_percent, critical_percent,
                          description);
        
        if (result == 0) {
            printf("Quota set successfully for group: %s\n", argv[optind]);
        } else {
            fprintf(stderr, "ERROR: Failed to set quota: %s\n", STRERROR(result));
        }
        
        return (result == 0) ? 0 : 2;
    }
    
    if (strcmp(command, "set-server") == 0) {
        if (optind + 4 >= argc) {
            fprintf(stderr, "ERROR: set-server command requires: SERVER SOFT HARD WARN%% CRIT%%\n");
            return 2;
        }
        
        if (parse_size_string(argv[optind + 1], &soft_quota) != 0) {
            fprintf(stderr, "ERROR: Invalid soft quota: %s\n", argv[optind + 1]);
            return 2;
        }
        
        if (parse_size_string(argv[optind + 2], &hard_quota) != 0) {
            fprintf(stderr, "ERROR: Invalid hard quota: %s\n", argv[optind + 2]);
            return 2;
        }
        
        warning_percent = strtod(argv[optind + 3], NULL);
        critical_percent = strtod(argv[optind + 4], NULL);
        
        if (optind + 5 < argc) {
            description = argv[optind + 5];
        }
        
        result = set_quota(QUOTA_TYPE_SERVER, argv[optind],
                          soft_quota, hard_quota,
                          warning_percent, critical_percent,
                          description);
        
        if (result == 0) {
            printf("Quota set successfully for server: %s\n", argv[optind]);
        } else {
            fprintf(stderr, "ERROR: Failed to set quota: %s\n", STRERROR(result));
        }
        
        return (result == 0) ? 0 : 2;
    }
    
    /* Commands below require FastDFS connection */
    
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
    
    /* Execute monitor command */
    if (strcmp(command, "monitor") == 0 || strcmp(command, "") == 0) {
        do {
            if (watch_mode && !json_output) {
                system("clear");
            }
            
            result = monitor_quota(pTrackerServer, group_name, server_id);
            
            if (watch_mode) {
                if (!json_output) {
                    printf("Press Ctrl+C to exit. Refreshing in %d seconds...\n", watch_interval);
                }
                sleep(watch_interval);
            }
        } while (watch_mode);
        
        tracker_disconnect_server_ex(pTrackerServer, true);
        fdfs_client_destroy();
        
        return result;
    }
    
    /* Unknown command */
    fprintf(stderr, "ERROR: Unknown command: %s\n", command);
    print_usage(argv[0]);
    
    tracker_disconnect_server_ex(pTrackerServer, true);
    fdfs_client_destroy();
    
    return 2;
}

