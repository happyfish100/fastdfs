/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_config_validator.c
* Configuration validator tool for FastDFS
* Validates tracker.conf and storage.conf files for common misconfigurations
* that can affect performance
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024
#define MAX_PATH_LENGTH 256
#define MAX_CONFIG_ITEMS 100

#define VALIDATION_OK 0
#define VALIDATION_WARNING 1
#define VALIDATION_ERROR 2

typedef struct {
    char key[64];
    char value[256];
    int line_number;
} ConfigItem;

typedef struct {
    int level;  /* 0=OK, 1=WARNING, 2=ERROR */
    char message[512];
} ValidationResult;

typedef struct {
    ConfigItem items[MAX_CONFIG_ITEMS];
    int count;
    char filename[MAX_PATH_LENGTH];
} ConfigFile;

typedef struct {
    ValidationResult results[MAX_CONFIG_ITEMS];
    int count;
    int errors;
    int warnings;
} ValidationReport;

/* Function prototypes */
static void print_usage(const char *program);
static int load_config_file(const char *filename, ConfigFile *config);
static char *trim_string(char *str);
static const char *get_config_value(ConfigFile *config, const char *key);
static int get_config_int(ConfigFile *config, const char *key, int default_val);
static void add_result(ValidationReport *report, int level, const char *format, ...);
static void validate_tracker_config(ConfigFile *config, ValidationReport *report);
static void validate_storage_config(ConfigFile *config, ValidationReport *report);
static void validate_common_settings(ConfigFile *config, ValidationReport *report, int is_tracker);
static void print_report(ValidationReport *report, const char *filename);
static int check_path_exists(const char *path);
static int check_path_writable(const char *path);
static long get_available_memory_mb(void);
static int get_cpu_count(void);

static void print_usage(const char *program)
{
    printf("FastDFS Configuration Validator v1.0\n");
    printf("Validates tracker.conf and storage.conf for performance issues\n\n");
    printf("Usage: %s [options] <config_file>\n", program);
    printf("Options:\n");
    printf("  -t          Validate as tracker config\n");
    printf("  -s          Validate as storage config\n");
    printf("  -a          Auto-detect config type\n");
    printf("  -v          Verbose output\n");
    printf("  -h          Show this help\n\n");
    printf("Examples:\n");
    printf("  %s -t /etc/fdfs/tracker.conf\n", program);
    printf("  %s -s /etc/fdfs/storage.conf\n", program);
    printf("  %s -a /etc/fdfs/tracker.conf\n", program);
}

static char *trim_string(char *str)
{
    char *end;
    
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

static int load_config_file(const char *filename, ConfigFile *config)
{
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *key, *value, *eq_pos;
    int line_number = 0;
    
    memset(config, 0, sizeof(ConfigFile));
    strncpy(config->filename, filename, MAX_PATH_LENGTH - 1);
    
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open config file: %s\n", filename);
        return -1;
    }
    
    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        line_number++;
        
        /* Skip comments and empty lines */
        char *trimmed = trim_string(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') {
            continue;
        }
        
        /* Find key=value */
        eq_pos = strchr(trimmed, '=');
        if (eq_pos == NULL) {
            continue;
        }
        
        *eq_pos = '\0';
        key = trim_string(trimmed);
        value = trim_string(eq_pos + 1);
        
        if (config->count < MAX_CONFIG_ITEMS) {
            strncpy(config->items[config->count].key, key, 63);
            strncpy(config->items[config->count].value, value, 255);
            config->items[config->count].line_number = line_number;
            config->count++;
        }
    }
    
    fclose(fp);
    return 0;
}

static const char *get_config_value(ConfigFile *config, const char *key)
{
    int i;
    for (i = 0; i < config->count; i++) {
        if (strcmp(config->items[i].key, key) == 0) {
            return config->items[i].value;
        }
    }
    return NULL;
}

static int get_config_int(ConfigFile *config, const char *key, int default_val)
{
    const char *value = get_config_value(config, key);
    if (value == NULL) {
        return default_val;
    }
    return atoi(value);
}

static void add_result(ValidationReport *report, int level, const char *format, ...)
{
    va_list args;
    
    if (report->count >= MAX_CONFIG_ITEMS) {
        return;
    }
    
    report->results[report->count].level = level;
    
    va_start(args, format);
    vsnprintf(report->results[report->count].message, 511, format, args);
    va_end(args);
    
    if (level == VALIDATION_ERROR) {
        report->errors++;
    } else if (level == VALIDATION_WARNING) {
        report->warnings++;
    }
    
    report->count++;
}

static int check_path_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

static int check_path_writable(const char *path)
{
    return (access(path, W_OK) == 0);
}

static long get_available_memory_mb(void)
{
    FILE *fp;
    char line[256];
    long mem_total = 0;
    
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return 4096; /* Default 4GB */
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%ld", &mem_total);
            break;
        }
    }
    fclose(fp);
    
    return mem_total / 1024; /* Convert KB to MB */
}

static int get_cpu_count(void)
{
    FILE *fp;
    char line[256];
    int cpu_count = 0;
    
    fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        return 4; /* Default */
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0) {
            cpu_count++;
        }
    }
    fclose(fp);
    
    return cpu_count > 0 ? cpu_count : 4;
}

static void validate_common_settings(ConfigFile *config, ValidationReport *report, int is_tracker)
{
    int max_connections;
    int work_threads;
    int accept_threads;
    int buff_size;
    const char *base_path;
    const char *log_level;
    long mem_mb;
    int cpu_count;
    
    mem_mb = get_available_memory_mb();
    cpu_count = get_cpu_count();
    
    /* Check base_path */
    base_path = get_config_value(config, "base_path");
    if (base_path == NULL) {
        add_result(report, VALIDATION_ERROR, "base_path is not set");
    } else if (!check_path_exists(base_path)) {
        add_result(report, VALIDATION_ERROR, "base_path '%s' does not exist", base_path);
    } else if (!check_path_writable(base_path)) {
        add_result(report, VALIDATION_ERROR, "base_path '%s' is not writable", base_path);
    } else {
        add_result(report, VALIDATION_OK, "base_path '%s' is valid", base_path);
    }
    
    /* Check max_connections */
    max_connections = get_config_int(config, "max_connections", 256);
    if (max_connections < 256) {
        add_result(report, VALIDATION_WARNING, 
            "max_connections=%d is low, recommend at least 1024 for production", max_connections);
    } else if (max_connections < 1024) {
        add_result(report, VALIDATION_WARNING,
            "max_connections=%d may be insufficient for high load", max_connections);
    } else {
        add_result(report, VALIDATION_OK, "max_connections=%d is good", max_connections);
    }
    
    /* Check work_threads */
    work_threads = get_config_int(config, "work_threads", 4);
    if (work_threads < cpu_count / 2) {
        add_result(report, VALIDATION_WARNING,
            "work_threads=%d is low for %d CPUs, recommend %d-%d",
            work_threads, cpu_count, cpu_count / 2, cpu_count);
    } else if (work_threads > cpu_count * 2) {
        add_result(report, VALIDATION_WARNING,
            "work_threads=%d may be too high for %d CPUs", work_threads, cpu_count);
    } else {
        add_result(report, VALIDATION_OK, "work_threads=%d is appropriate for %d CPUs",
            work_threads, cpu_count);
    }
    
    /* Check accept_threads */
    accept_threads = get_config_int(config, "accept_threads", 1);
    if (accept_threads > 1 && max_connections < 10000) {
        add_result(report, VALIDATION_WARNING,
            "accept_threads=%d > 1 is only needed for very high connection rates", accept_threads);
    } else {
        add_result(report, VALIDATION_OK, "accept_threads=%d is fine", accept_threads);
    }
    
    /* Check buff_size */
    buff_size = get_config_int(config, "buff_size", 64);
    if (buff_size < 64) {
        add_result(report, VALIDATION_WARNING,
            "buff_size=%dKB is too small, recommend 256KB or 512KB", buff_size);
    } else if (buff_size < 256) {
        add_result(report, VALIDATION_WARNING,
            "buff_size=%dKB is small, recommend 256KB for better performance", buff_size);
    } else {
        add_result(report, VALIDATION_OK, "buff_size=%dKB is good", buff_size);
    }
    
    /* Check log_level */
    log_level = get_config_value(config, "log_level");
    if (log_level != NULL && strcmp(log_level, "debug") == 0) {
        add_result(report, VALIDATION_WARNING,
            "log_level=debug will impact performance, use 'info' or 'warn' in production");
    }
    
    /* Check connect_timeout */
    int connect_timeout = get_config_int(config, "connect_timeout", 30);
    if (connect_timeout > 30) {
        add_result(report, VALIDATION_WARNING,
            "connect_timeout=%ds is high, recommend 5-10s for LAN", connect_timeout);
    }
    
    /* Check network_timeout */
    int network_timeout = get_config_int(config, "network_timeout", 60);
    if (network_timeout > 120) {
        add_result(report, VALIDATION_WARNING,
            "network_timeout=%ds is very high", network_timeout);
    }
}

static void validate_tracker_config(ConfigFile *config, ValidationReport *report)
{
    int store_lookup;
    int reserved_storage_space;
    const char *value;
    
    add_result(report, VALIDATION_OK, "=== Tracker Configuration Validation ===");
    
    /* Common settings */
    validate_common_settings(config, report, 1);
    
    /* Check store_lookup */
    store_lookup = get_config_int(config, "store_lookup", 2);
    if (store_lookup == 0) {
        add_result(report, VALIDATION_OK, "store_lookup=0 (round robin) - good for load balancing");
    } else if (store_lookup == 1) {
        add_result(report, VALIDATION_WARNING,
            "store_lookup=1 (specify group) - ensure group is correctly set");
    } else if (store_lookup == 2) {
        add_result(report, VALIDATION_OK, "store_lookup=2 (load balance) - recommended");
    }
    
    /* Check reserved_storage_space */
    value = get_config_value(config, "reserved_storage_space");
    if (value != NULL) {
        if (strstr(value, "GB") != NULL || strstr(value, "G") != NULL) {
            add_result(report, VALIDATION_OK, "reserved_storage_space=%s is set", value);
        } else if (strstr(value, "%") != NULL) {
            int pct = atoi(value);
            if (pct < 10) {
                add_result(report, VALIDATION_WARNING,
                    "reserved_storage_space=%s is low, recommend at least 10%%", value);
            }
        }
    } else {
        add_result(report, VALIDATION_WARNING,
            "reserved_storage_space is not set, using default");
    }
    
    /* Check use_trunk_file */
    value = get_config_value(config, "use_trunk_file");
    if (value != NULL && (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)) {
        add_result(report, VALIDATION_OK, "use_trunk_file=true - good for small files");
        
        /* Check trunk settings */
        int slot_min = get_config_int(config, "slot_min_size", 256);
        int slot_max = get_config_int(config, "slot_max_size", 16384);
        if (slot_max < 1024 * 1024) {
            add_result(report, VALIDATION_OK, 
                "trunk slot_max_size=%d is appropriate for small files", slot_max);
        }
    }
    
    /* Check download_server */
    int download_server = get_config_int(config, "download_server", 0);
    if (download_server == 0) {
        add_result(report, VALIDATION_OK, "download_server=0 (round robin)");
    } else if (download_server == 1) {
        add_result(report, VALIDATION_OK, "download_server=1 (source first) - reduces sync traffic");
    }
}

static void validate_storage_config(ConfigFile *config, ValidationReport *report)
{
    int disk_reader_threads;
    int disk_writer_threads;
    int store_path_count;
    int sync_interval;
    int cpu_count;
    const char *value;
    char path_key[32];
    int i;
    
    cpu_count = get_cpu_count();
    
    add_result(report, VALIDATION_OK, "=== Storage Configuration Validation ===");
    
    /* Common settings */
    validate_common_settings(config, report, 0);
    
    /* Check disk_rw_separated */
    value = get_config_value(config, "disk_rw_separated");
    if (value != NULL && (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)) {
        add_result(report, VALIDATION_OK, "disk_rw_separated=true - good for high concurrency");
    } else {
        add_result(report, VALIDATION_WARNING,
            "disk_rw_separated=false - consider enabling for better performance");
    }
    
    /* Check disk_reader_threads */
    disk_reader_threads = get_config_int(config, "disk_reader_threads", 1);
    if (disk_reader_threads < 2) {
        add_result(report, VALIDATION_WARNING,
            "disk_reader_threads=%d is low, recommend 2-4 for SSD, 1-2 for HDD",
            disk_reader_threads);
    } else {
        add_result(report, VALIDATION_OK, "disk_reader_threads=%d", disk_reader_threads);
    }
    
    /* Check disk_writer_threads */
    disk_writer_threads = get_config_int(config, "disk_writer_threads", 1);
    if (disk_writer_threads < 1) {
        add_result(report, VALIDATION_ERROR,
            "disk_writer_threads=%d must be at least 1", disk_writer_threads);
    } else {
        add_result(report, VALIDATION_OK, "disk_writer_threads=%d", disk_writer_threads);
    }
    
    /* Check store_path_count and paths */
    store_path_count = get_config_int(config, "store_path_count", 1);
    add_result(report, VALIDATION_OK, "store_path_count=%d", store_path_count);
    
    for (i = 0; i < store_path_count; i++) {
        snprintf(path_key, sizeof(path_key), "store_path%d", i);
        value = get_config_value(config, path_key);
        if (value == NULL) {
            add_result(report, VALIDATION_ERROR, "%s is not set", path_key);
        } else if (!check_path_exists(value)) {
            add_result(report, VALIDATION_ERROR, "%s='%s' does not exist", path_key, value);
        } else if (!check_path_writable(value)) {
            add_result(report, VALIDATION_ERROR, "%s='%s' is not writable", path_key, value);
        } else {
            add_result(report, VALIDATION_OK, "%s='%s' is valid", path_key, value);
        }
    }
    
    /* Check sync_interval */
    sync_interval = get_config_int(config, "sync_interval", 0);
    if (sync_interval > 0) {
        add_result(report, VALIDATION_WARNING,
            "sync_interval=%dms adds delay between syncs, set to 0 for fastest sync",
            sync_interval);
    } else {
        add_result(report, VALIDATION_OK, "sync_interval=0 - fastest sync");
    }
    
    /* Check fsync_after_written_bytes */
    int fsync_bytes = get_config_int(config, "fsync_after_written_bytes", 0);
    if (fsync_bytes == 0) {
        add_result(report, VALIDATION_WARNING,
            "fsync_after_written_bytes=0 - no fsync, fast but risky on power loss");
    } else {
        add_result(report, VALIDATION_OK, 
            "fsync_after_written_bytes=%d - data safety enabled", fsync_bytes);
    }
    
    /* Check use_connection_pool */
    value = get_config_value(config, "use_connection_pool");
    if (value == NULL || strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
        add_result(report, VALIDATION_WARNING,
            "use_connection_pool=false - enable for better performance");
    } else {
        add_result(report, VALIDATION_OK, "use_connection_pool=true - good");
    }
    
    /* Check tracker_server entries */
    int tracker_count = 0;
    for (i = 0; i < config->count; i++) {
        if (strcmp(config->items[i].key, "tracker_server") == 0) {
            tracker_count++;
        }
    }
    if (tracker_count == 0) {
        add_result(report, VALIDATION_ERROR, "No tracker_server configured");
    } else if (tracker_count == 1) {
        add_result(report, VALIDATION_WARNING,
            "Only 1 tracker_server - consider adding more for high availability");
    } else {
        add_result(report, VALIDATION_OK, "%d tracker_servers configured", tracker_count);
    }
    
    /* Check subdir_count_per_path */
    int subdir_count = get_config_int(config, "subdir_count_per_path", 256);
    if (subdir_count < 256) {
        add_result(report, VALIDATION_WARNING,
            "subdir_count_per_path=%d is low, recommend 256 for large deployments", subdir_count);
    }
}

static void print_report(ValidationReport *report, const char *filename)
{
    int i;
    const char *level_str;
    const char *color;
    
    printf("\n");
    printf("========================================\n");
    printf("Configuration Validation Report\n");
    printf("File: %s\n", filename);
    printf("========================================\n\n");
    
    for (i = 0; i < report->count; i++) {
        switch (report->results[i].level) {
            case VALIDATION_OK:
                level_str = "[OK]";
                color = "\033[32m"; /* Green */
                break;
            case VALIDATION_WARNING:
                level_str = "[WARN]";
                color = "\033[33m"; /* Yellow */
                break;
            case VALIDATION_ERROR:
                level_str = "[ERROR]";
                color = "\033[31m"; /* Red */
                break;
            default:
                level_str = "[INFO]";
                color = "\033[0m";
        }
        
        printf("%s%-8s\033[0m %s\n", color, level_str, report->results[i].message);
    }
    
    printf("\n========================================\n");
    printf("Summary: %d errors, %d warnings\n", report->errors, report->warnings);
    printf("========================================\n");
    
    if (report->errors > 0) {
        printf("\n\033[31mConfiguration has errors that must be fixed!\033[0m\n");
    } else if (report->warnings > 0) {
        printf("\n\033[33mConfiguration is valid but has performance recommendations.\033[0m\n");
    } else {
        printf("\n\033[32mConfiguration looks good!\033[0m\n");
    }
}

int main(int argc, char *argv[])
{
    int opt;
    int config_type = 0; /* 0=auto, 1=tracker, 2=storage */
    int verbose = 0;
    const char *config_file = NULL;
    ConfigFile config;
    ValidationReport report;
    
    while ((opt = getopt(argc, argv, "tsavh")) != -1) {
        switch (opt) {
            case 't':
                config_type = 1;
                break;
            case 's':
                config_type = 2;
                break;
            case 'a':
                config_type = 0;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: No config file specified\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    config_file = argv[optind];
    
    /* Load config file */
    if (load_config_file(config_file, &config) != 0) {
        return 1;
    }
    
    if (verbose) {
        printf("Loaded %d configuration items from %s\n", config.count, config_file);
    }
    
    /* Auto-detect config type */
    if (config_type == 0) {
        if (strstr(config_file, "tracker") != NULL) {
            config_type = 1;
        } else if (strstr(config_file, "storage") != NULL) {
            config_type = 2;
        } else if (get_config_value(&config, "store_path0") != NULL) {
            config_type = 2;
        } else {
            config_type = 1;
        }
        
        if (verbose) {
            printf("Auto-detected config type: %s\n", 
                config_type == 1 ? "tracker" : "storage");
        }
    }
    
    /* Initialize report */
    memset(&report, 0, sizeof(report));
    
    /* Validate based on type */
    if (config_type == 1) {
        validate_tracker_config(&config, &report);
    } else {
        validate_storage_config(&config, &report);
    }
    
    /* Print report */
    print_report(&report, config_file);
    
    return report.errors > 0 ? 1 : 0;
}
