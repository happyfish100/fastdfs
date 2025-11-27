/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

/**
* fdfs_config_generator.c
* Configuration generator tool for FastDFS
* Generates optimized configuration files based on system resources
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>

#define MAX_PATH_LENGTH 256
#define MAX_LINE_LENGTH 1024

/* Configuration profiles */
#define PROFILE_MINIMAL 0
#define PROFILE_STANDARD 1
#define PROFILE_PERFORMANCE 2
#define PROFILE_HIGH_AVAILABILITY 3

/* Config types */
#define CONFIG_TRACKER 1
#define CONFIG_STORAGE 2
#define CONFIG_CLIENT 3

typedef struct {
    long total_memory_mb;
    long available_memory_mb;
    int cpu_count;
    long disk_space_gb;
    int is_ssd;
    char hostname[256];
} SystemInfo;

typedef struct {
    int config_type;
    int profile;
    char base_path[MAX_PATH_LENGTH];
    char tracker_server[256];
    int tracker_port;
    char group_name[64];
    int storage_port;
    char store_path[MAX_PATH_LENGTH];
    int store_path_count;
    char output_file[MAX_PATH_LENGTH];
    int verbose;
} GeneratorOptions;

/* Function prototypes */
static void print_usage(const char *program);
static int get_system_info(SystemInfo *info);
static long get_available_memory_mb(void);
static int get_cpu_count(void);
static long get_disk_space_gb(const char *path);
static void generate_tracker_config(GeneratorOptions *options, SystemInfo *info, FILE *out);
static void generate_storage_config(GeneratorOptions *options, SystemInfo *info, FILE *out);
static void generate_client_config(GeneratorOptions *options, SystemInfo *info, FILE *out);
static const char *get_profile_name(int profile);
static void print_header(FILE *out, const char *config_type, GeneratorOptions *options);

static void print_usage(const char *program)
{
    printf("FastDFS Configuration Generator v1.0\n");
    printf("Generates optimized FastDFS configuration files\n\n");
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -t, --type <type>       Config type: tracker, storage, client\n");
    printf("  -p, --profile <prof>    Profile: minimal, standard, performance, ha\n");
    printf("  -b, --base-path <path>  Base path for FastDFS data\n");
    printf("  -T, --tracker <addr>    Tracker server address (for storage/client)\n");
    printf("  -P, --port <port>       Port number\n");
    printf("  -g, --group <name>      Group name (for storage)\n");
    printf("  -s, --store-path <path> Store path (for storage)\n");
    printf("  -o, --output <file>     Output file (default: stdout)\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -h, --help              Show this help\n\n");
    printf("Profiles:\n");
    printf("  minimal      - Minimum resources, suitable for testing\n");
    printf("  standard     - Balanced configuration for general use\n");
    printf("  performance  - Optimized for high throughput\n");
    printf("  ha           - High availability configuration\n\n");
    printf("Examples:\n");
    printf("  %s -t tracker -p standard -b /var/fdfs -o tracker.conf\n", program);
    printf("  %s -t storage -p performance -T 192.168.1.1:22122 -g group1\n", program);
}

static long get_available_memory_mb(void)
{
    FILE *fp;
    char line[256];
    long mem_available = 0;
    long mem_free = 0;
    long buffers = 0;
    long cached = 0;
    
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return 1024; /* Default 1GB */
    }
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%ld", &mem_available);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line + 8, "%ld", &mem_free);
        } else if (strncmp(line, "Buffers:", 8) == 0) {
            sscanf(line + 8, "%ld", &buffers);
        } else if (strncmp(line, "Cached:", 7) == 0) {
            sscanf(line + 7, "%ld", &cached);
        }
    }
    
    fclose(fp);
    
    if (mem_available > 0) {
        return mem_available / 1024;
    }
    
    return (mem_free + buffers + cached) / 1024;
}

static int get_cpu_count(void)
{
    int count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? count : 1;
}

static long get_disk_space_gb(const char *path)
{
    struct statvfs stat;
    
    if (statvfs(path, &stat) != 0) {
        return 100; /* Default 100GB */
    }
    
    return (long)(stat.f_bavail * stat.f_frsize) / (1024 * 1024 * 1024);
}

static int get_system_info(SystemInfo *info)
{
    memset(info, 0, sizeof(SystemInfo));
    
    info->available_memory_mb = get_available_memory_mb();
    info->total_memory_mb = info->available_memory_mb * 2; /* Estimate */
    info->cpu_count = get_cpu_count();
    info->disk_space_gb = get_disk_space_gb("/");
    info->is_ssd = 0; /* Default to HDD */
    
    if (gethostname(info->hostname, sizeof(info->hostname)) != 0) {
        strcpy(info->hostname, "localhost");
    }
    
    return 0;
}

static const char *get_profile_name(int profile)
{
    switch (profile) {
        case PROFILE_MINIMAL: return "minimal";
        case PROFILE_STANDARD: return "standard";
        case PROFILE_PERFORMANCE: return "performance";
        case PROFILE_HIGH_AVAILABILITY: return "high-availability";
        default: return "unknown";
    }
}

static void print_header(FILE *out, const char *config_type, GeneratorOptions *options)
{
    time_t now = time(NULL);
    char time_str[64];
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(out, "# FastDFS %s Configuration\n", config_type);
    fprintf(out, "# Generated by fdfs_config_generator\n");
    fprintf(out, "# Date: %s\n", time_str);
    fprintf(out, "# Profile: %s\n", get_profile_name(options->profile));
    fprintf(out, "#\n");
    fprintf(out, "# This configuration is auto-generated based on system resources.\n");
    fprintf(out, "# Please review and adjust as needed for your environment.\n");
    fprintf(out, "#\n\n");
}

static void generate_tracker_config(GeneratorOptions *options, SystemInfo *info, FILE *out)
{
    int work_threads;
    int max_connections;
    int accept_threads;
    int sync_log_buff_interval;
    int check_active_interval;
    
    print_header(out, "Tracker", options);
    
    /* Calculate optimal values based on profile and system resources */
    switch (options->profile) {
        case PROFILE_MINIMAL:
            work_threads = 2;
            max_connections = 256;
            accept_threads = 1;
            sync_log_buff_interval = 10;
            check_active_interval = 120;
            break;
        case PROFILE_PERFORMANCE:
            work_threads = info->cpu_count * 2;
            if (work_threads > 64) work_threads = 64;
            max_connections = 10240;
            accept_threads = info->cpu_count > 4 ? 4 : info->cpu_count;
            sync_log_buff_interval = 1;
            check_active_interval = 30;
            break;
        case PROFILE_HIGH_AVAILABILITY:
            work_threads = info->cpu_count;
            if (work_threads > 32) work_threads = 32;
            max_connections = 4096;
            accept_threads = 2;
            sync_log_buff_interval = 1;
            check_active_interval = 15;
            break;
        default: /* PROFILE_STANDARD */
            work_threads = info->cpu_count;
            if (work_threads > 16) work_threads = 16;
            max_connections = 1024;
            accept_threads = 1;
            sync_log_buff_interval = 5;
            check_active_interval = 60;
            break;
    }
    
    fprintf(out, "# Disable this config file\n");
    fprintf(out, "disabled = false\n\n");
    
    fprintf(out, "# Bind address (empty for all interfaces)\n");
    fprintf(out, "bind_addr =\n\n");
    
    fprintf(out, "# Tracker server port\n");
    fprintf(out, "port = %d\n\n", options->tracker_port > 0 ? options->tracker_port : 22122);
    
    fprintf(out, "# Connect timeout in seconds\n");
    fprintf(out, "connect_timeout = 10\n\n");
    
    fprintf(out, "# Network timeout in seconds\n");
    fprintf(out, "network_timeout = 60\n\n");
    
    fprintf(out, "# Base path for data and logs\n");
    fprintf(out, "base_path = %s\n\n", options->base_path[0] ? options->base_path : "/var/fdfs");
    
    fprintf(out, "# Maximum connections\n");
    fprintf(out, "max_connections = %d\n\n", max_connections);
    
    fprintf(out, "# Accept threads\n");
    fprintf(out, "accept_threads = %d\n\n", accept_threads);
    
    fprintf(out, "# Work threads\n");
    fprintf(out, "work_threads = %d\n\n", work_threads);
    
    fprintf(out, "# Minimum network buffer size\n");
    fprintf(out, "min_buff_size = 8KB\n\n");
    
    fprintf(out, "# Maximum network buffer size\n");
    fprintf(out, "max_buff_size = 128KB\n\n");
    
    fprintf(out, "# Store lookup method\n");
    fprintf(out, "# 0: round robin\n");
    fprintf(out, "# 1: specify group\n");
    fprintf(out, "# 2: load balance (select group with max free space)\n");
    fprintf(out, "store_lookup = 2\n\n");
    
    fprintf(out, "# Store group (when store_lookup = 1)\n");
    fprintf(out, "store_group = group1\n\n");
    
    fprintf(out, "# Store server selection\n");
    fprintf(out, "# 0: round robin\n");
    fprintf(out, "# 1: first server ordered by IP\n");
    fprintf(out, "# 2: first server ordered by priority\n");
    fprintf(out, "store_server = 0\n\n");
    
    fprintf(out, "# Store path selection\n");
    fprintf(out, "# 0: round robin\n");
    fprintf(out, "# 2: load balance (select path with max free space)\n");
    fprintf(out, "store_path = 0\n\n");
    
    fprintf(out, "# Download server selection\n");
    fprintf(out, "# 0: round robin\n");
    fprintf(out, "# 1: source server\n");
    fprintf(out, "download_server = 0\n\n");
    
    fprintf(out, "# Reserved storage space\n");
    fprintf(out, "reserved_storage_space = 20%%\n\n");
    
    fprintf(out, "# Log level\n");
    fprintf(out, "# emerg, alert, crit, error, warning, notice, info, debug\n");
    fprintf(out, "log_level = info\n\n");
    
    fprintf(out, "# Run as daemon\n");
    fprintf(out, "run_by_group =\n");
    fprintf(out, "run_by_user =\n\n");
    
    fprintf(out, "# Allow hosts (empty for all)\n");
    fprintf(out, "allow_hosts = *\n\n");
    
    fprintf(out, "# Sync log buffer interval in seconds\n");
    fprintf(out, "sync_log_buff_interval = %d\n\n", sync_log_buff_interval);
    
    fprintf(out, "# Check active interval in seconds\n");
    fprintf(out, "check_active_interval = %d\n\n", check_active_interval);
    
    fprintf(out, "# Thread stack size\n");
    fprintf(out, "thread_stack_size = 256KB\n\n");
    
    fprintf(out, "# Storage IP changed auto adjust\n");
    fprintf(out, "storage_ip_changed_auto_adjust = true\n\n");
    
    fprintf(out, "# Storage sync file max delay\n");
    fprintf(out, "storage_sync_file_max_delay = 86400\n\n");
    
    fprintf(out, "# Storage sync file max time\n");
    fprintf(out, "storage_sync_file_max_time = 300\n\n");
    
    fprintf(out, "# Use trunk file\n");
    fprintf(out, "use_trunk_file = false\n\n");
    
    fprintf(out, "# Slot minimum size\n");
    fprintf(out, "slot_min_size = 256\n\n");
    
    fprintf(out, "# Slot maximum size\n");
    fprintf(out, "slot_max_size = 1MB\n\n");
    
    fprintf(out, "# Trunk alloc alignment size\n");
    fprintf(out, "trunk_alloc_alignment_size = 256\n\n");
    
    fprintf(out, "# Trunk file size\n");
    fprintf(out, "trunk_file_size = 64MB\n\n");
    
    fprintf(out, "# Trunk create file advance\n");
    fprintf(out, "trunk_create_file_advance = false\n\n");
    
    fprintf(out, "# Trunk create file time base\n");
    fprintf(out, "trunk_create_file_time_base = 02:00\n\n");
    
    fprintf(out, "# Trunk create file interval\n");
    fprintf(out, "trunk_create_file_interval = 86400\n\n");
    
    fprintf(out, "# Trunk create file space threshold\n");
    fprintf(out, "trunk_create_file_space_threshold = 20G\n\n");
    
    fprintf(out, "# Trunk init check occupying\n");
    fprintf(out, "trunk_init_check_occupying = false\n\n");
    
    fprintf(out, "# Trunk init reload from binlog\n");
    fprintf(out, "trunk_init_reload_from_binlog = false\n\n");
    
    fprintf(out, "# Trunk compress binlog minimum interval\n");
    fprintf(out, "trunk_compress_binlog_min_interval = 86400\n\n");
    
    fprintf(out, "# Trunk compress binlog time base\n");
    fprintf(out, "trunk_compress_binlog_time_base = 03:00\n\n");
    
    fprintf(out, "# Use storage ID\n");
    fprintf(out, "use_storage_id = false\n\n");
    
    fprintf(out, "# Storage IDs filename\n");
    fprintf(out, "storage_ids_filename = storage_ids.conf\n\n");
    
    fprintf(out, "# ID type in filename\n");
    fprintf(out, "# ip: IP address\n");
    fprintf(out, "# id: server ID\n");
    fprintf(out, "id_type_in_filename = id\n\n");
    
    fprintf(out, "# Store slave file use link\n");
    fprintf(out, "store_slave_file_use_link = false\n\n");
    
    fprintf(out, "# Rotate error log\n");
    fprintf(out, "rotate_error_log = false\n\n");
    
    fprintf(out, "# Error log rotate time\n");
    fprintf(out, "error_log_rotate_time = 00:00\n\n");
    
    fprintf(out, "# Compress old error log\n");
    fprintf(out, "compress_old_error_log = false\n\n");
    
    fprintf(out, "# Compress error log days before\n");
    fprintf(out, "compress_error_log_days_before = 7\n\n");
    
    fprintf(out, "# Rotate error log size\n");
    fprintf(out, "rotate_error_log_size = 0\n\n");
    
    fprintf(out, "# Log file keep days\n");
    fprintf(out, "log_file_keep_days = 0\n\n");
    
    fprintf(out, "# Use connection pool\n");
    fprintf(out, "use_connection_pool = true\n\n");
    
    fprintf(out, "# Connection pool max idle time\n");
    fprintf(out, "connection_pool_max_idle_time = 3600\n\n");
    
    fprintf(out, "# HTTP server disabled\n");
    fprintf(out, "http.disabled = true\n\n");
    
    fprintf(out, "# HTTP server port\n");
    fprintf(out, "http.server_port = 8080\n\n");
    
    fprintf(out, "# HTTP check alive interval\n");
    fprintf(out, "http.check_alive_interval = 30\n\n");
    
    fprintf(out, "# HTTP check alive type\n");
    fprintf(out, "http.check_alive_type = tcp\n\n");
    
    fprintf(out, "# HTTP check alive uri\n");
    fprintf(out, "http.check_alive_uri = /status.html\n");
}

static void generate_storage_config(GeneratorOptions *options, SystemInfo *info, FILE *out)
{
    int work_threads;
    int max_connections;
    int buff_size;
    int disk_reader_threads;
    int disk_writer_threads;
    
    print_header(out, "Storage", options);
    
    /* Calculate optimal values based on profile and system resources */
    switch (options->profile) {
        case PROFILE_MINIMAL:
            work_threads = 2;
            max_connections = 256;
            buff_size = 64;
            disk_reader_threads = 1;
            disk_writer_threads = 1;
            break;
        case PROFILE_PERFORMANCE:
            work_threads = info->cpu_count * 2;
            if (work_threads > 64) work_threads = 64;
            max_connections = 10240;
            buff_size = 256;
            disk_reader_threads = info->cpu_count;
            disk_writer_threads = info->cpu_count;
            if (disk_reader_threads > 16) disk_reader_threads = 16;
            if (disk_writer_threads > 16) disk_writer_threads = 16;
            break;
        case PROFILE_HIGH_AVAILABILITY:
            work_threads = info->cpu_count;
            if (work_threads > 32) work_threads = 32;
            max_connections = 4096;
            buff_size = 128;
            disk_reader_threads = info->cpu_count / 2;
            disk_writer_threads = info->cpu_count / 2;
            if (disk_reader_threads < 2) disk_reader_threads = 2;
            if (disk_writer_threads < 2) disk_writer_threads = 2;
            break;
        default: /* PROFILE_STANDARD */
            work_threads = info->cpu_count;
            if (work_threads > 16) work_threads = 16;
            max_connections = 1024;
            buff_size = 128;
            disk_reader_threads = 4;
            disk_writer_threads = 4;
            break;
    }
    
    fprintf(out, "# Disable this config file\n");
    fprintf(out, "disabled = false\n\n");
    
    fprintf(out, "# Group name\n");
    fprintf(out, "group_name = %s\n\n", options->group_name[0] ? options->group_name : "group1");
    
    fprintf(out, "# Bind address (empty for all interfaces)\n");
    fprintf(out, "bind_addr =\n\n");
    
    fprintf(out, "# Client bind enabled\n");
    fprintf(out, "client_bind = true\n\n");
    
    fprintf(out, "# Storage server port\n");
    fprintf(out, "port = %d\n\n", options->storage_port > 0 ? options->storage_port : 23000);
    
    fprintf(out, "# Connect timeout in seconds\n");
    fprintf(out, "connect_timeout = 10\n\n");
    
    fprintf(out, "# Network timeout in seconds\n");
    fprintf(out, "network_timeout = 60\n\n");
    
    fprintf(out, "# Heart beat interval in seconds\n");
    fprintf(out, "heart_beat_interval = 30\n\n");
    
    fprintf(out, "# Stat report interval in seconds\n");
    fprintf(out, "stat_report_interval = 60\n\n");
    
    fprintf(out, "# Base path for data and logs\n");
    fprintf(out, "base_path = %s\n\n", options->base_path[0] ? options->base_path : "/var/fdfs");
    
    fprintf(out, "# Maximum connections\n");
    fprintf(out, "max_connections = %d\n\n", max_connections);
    
    fprintf(out, "# Buffer size in KB\n");
    fprintf(out, "buff_size = %dKB\n\n", buff_size);
    
    fprintf(out, "# Accept threads\n");
    fprintf(out, "accept_threads = 1\n\n");
    
    fprintf(out, "# Work threads\n");
    fprintf(out, "work_threads = %d\n\n", work_threads);
    
    fprintf(out, "# Disk read/write separated\n");
    fprintf(out, "disk_rw_separated = true\n\n");
    
    fprintf(out, "# Disk reader threads\n");
    fprintf(out, "disk_reader_threads = %d\n\n", disk_reader_threads);
    
    fprintf(out, "# Disk writer threads\n");
    fprintf(out, "disk_writer_threads = %d\n\n", disk_writer_threads);
    
    fprintf(out, "# Sync wait msec\n");
    fprintf(out, "sync_wait_msec = 50\n\n");
    
    fprintf(out, "# Sync interval\n");
    fprintf(out, "sync_interval = 0\n\n");
    
    fprintf(out, "# Sync start time\n");
    fprintf(out, "sync_start_time = 00:00\n\n");
    
    fprintf(out, "# Sync end time\n");
    fprintf(out, "sync_end_time = 23:59\n\n");
    
    fprintf(out, "# Write mark file frequency\n");
    fprintf(out, "write_mark_file_freq = 500\n\n");
    
    fprintf(out, "# Store path count\n");
    fprintf(out, "store_path_count = %d\n\n", options->store_path_count > 0 ? options->store_path_count : 1);
    
    fprintf(out, "# Store paths\n");
    if (options->store_path[0]) {
        fprintf(out, "store_path0 = %s\n\n", options->store_path);
    } else {
        fprintf(out, "store_path0 = %s\n\n", options->base_path[0] ? options->base_path : "/var/fdfs");
    }
    
    fprintf(out, "# Subdir count per path\n");
    fprintf(out, "subdir_count_per_path = 256\n\n");
    
    fprintf(out, "# Tracker server\n");
    if (options->tracker_server[0]) {
        fprintf(out, "tracker_server = %s\n\n", options->tracker_server);
    } else {
        fprintf(out, "tracker_server = 127.0.0.1:22122\n\n");
    }
    
    fprintf(out, "# Log level\n");
    fprintf(out, "log_level = info\n\n");
    
    fprintf(out, "# Run as daemon\n");
    fprintf(out, "run_by_group =\n");
    fprintf(out, "run_by_user =\n\n");
    
    fprintf(out, "# Allow hosts (empty for all)\n");
    fprintf(out, "allow_hosts = *\n\n");
    
    fprintf(out, "# File distribute path mode\n");
    fprintf(out, "file_distribute_path_mode = 0\n\n");
    
    fprintf(out, "# File distribute rotate count\n");
    fprintf(out, "file_distribute_rotate_count = 100\n\n");
    
    fprintf(out, "# Fsync after written bytes\n");
    fprintf(out, "fsync_after_written_bytes = 0\n\n");
    
    fprintf(out, "# Sync log buffer interval\n");
    fprintf(out, "sync_log_buff_interval = 1\n\n");
    
    fprintf(out, "# Sync binlog buffer interval\n");
    fprintf(out, "sync_binlog_buff_interval = 1\n\n");
    
    fprintf(out, "# Sync stat file interval\n");
    fprintf(out, "sync_stat_file_interval = 300\n\n");
    
    fprintf(out, "# Thread stack size\n");
    fprintf(out, "thread_stack_size = 512KB\n\n");
    
    fprintf(out, "# Upload priority\n");
    fprintf(out, "upload_priority = 10\n\n");
    
    fprintf(out, "# If domain name as tracker server\n");
    fprintf(out, "if_alias_prefix =\n\n");
    
    fprintf(out, "# Check file duplicate\n");
    fprintf(out, "check_file_duplicate = 0\n\n");
    
    fprintf(out, "# File signature method\n");
    fprintf(out, "file_signature_method = hash\n\n");
    
    fprintf(out, "# Key namespace\n");
    fprintf(out, "key_namespace = FastDFS\n\n");
    
    fprintf(out, "# Keep alive\n");
    fprintf(out, "keep_alive = 0\n\n");
    
    fprintf(out, "# Use access log\n");
    fprintf(out, "use_access_log = false\n\n");
    
    fprintf(out, "# Rotate access log\n");
    fprintf(out, "rotate_access_log = false\n\n");
    
    fprintf(out, "# Access log rotate time\n");
    fprintf(out, "access_log_rotate_time = 00:00\n\n");
    
    fprintf(out, "# Compress old access log\n");
    fprintf(out, "compress_old_access_log = false\n\n");
    
    fprintf(out, "# Compress access log days before\n");
    fprintf(out, "compress_access_log_days_before = 7\n\n");
    
    fprintf(out, "# Rotate access log size\n");
    fprintf(out, "rotate_access_log_size = 0\n\n");
    
    fprintf(out, "# Rotate error log\n");
    fprintf(out, "rotate_error_log = false\n\n");
    
    fprintf(out, "# Error log rotate time\n");
    fprintf(out, "error_log_rotate_time = 00:00\n\n");
    
    fprintf(out, "# Compress old error log\n");
    fprintf(out, "compress_old_error_log = false\n\n");
    
    fprintf(out, "# Compress error log days before\n");
    fprintf(out, "compress_error_log_days_before = 7\n\n");
    
    fprintf(out, "# Rotate error log size\n");
    fprintf(out, "rotate_error_log_size = 0\n\n");
    
    fprintf(out, "# Log file keep days\n");
    fprintf(out, "log_file_keep_days = 0\n\n");
    
    fprintf(out, "# File sync skip invalid record\n");
    fprintf(out, "file_sync_skip_invalid_record = false\n\n");
    
    fprintf(out, "# Use connection pool\n");
    fprintf(out, "use_connection_pool = true\n\n");
    
    fprintf(out, "# Connection pool max idle time\n");
    fprintf(out, "connection_pool_max_idle_time = 3600\n\n");
    
    fprintf(out, "# Compress binlog\n");
    fprintf(out, "compress_binlog = true\n\n");
    
    fprintf(out, "# Compress binlog time\n");
    fprintf(out, "compress_binlog_time = 01:30\n\n");
    
    fprintf(out, "# Check store path mark\n");
    fprintf(out, "check_store_path_mark = true\n\n");
    
    fprintf(out, "# HTTP server disabled\n");
    fprintf(out, "http.disabled = true\n\n");
    
    fprintf(out, "# HTTP server port\n");
    fprintf(out, "http.server_port = 8888\n\n");
    
    fprintf(out, "# HTTP trunk size\n");
    fprintf(out, "http.trunk_size = 256KB\n\n");
}

static void generate_client_config(GeneratorOptions *options, SystemInfo *info, FILE *out)
{
    print_header(out, "Client", options);
    
    fprintf(out, "# Connect timeout in seconds\n");
    fprintf(out, "connect_timeout = 5\n\n");
    
    fprintf(out, "# Network timeout in seconds\n");
    fprintf(out, "network_timeout = 60\n\n");
    
    fprintf(out, "# Base path for logs\n");
    fprintf(out, "base_path = %s\n\n", options->base_path[0] ? options->base_path : "/var/fdfs");
    
    fprintf(out, "# Tracker server\n");
    if (options->tracker_server[0]) {
        fprintf(out, "tracker_server = %s\n\n", options->tracker_server);
    } else {
        fprintf(out, "tracker_server = 127.0.0.1:22122\n\n");
    }
    
    fprintf(out, "# Log level\n");
    fprintf(out, "log_level = info\n\n");
    
    fprintf(out, "# Use connection pool\n");
    fprintf(out, "use_connection_pool = true\n\n");
    
    fprintf(out, "# Connection pool max idle time\n");
    fprintf(out, "connection_pool_max_idle_time = 3600\n\n");
    
    fprintf(out, "# Load fdfs parameters from tracker\n");
    fprintf(out, "load_fdfs_parameters_from_tracker = true\n\n");
    
    fprintf(out, "# Use storage ID\n");
    fprintf(out, "use_storage_id = false\n\n");
    
    fprintf(out, "# Storage IDs filename\n");
    fprintf(out, "storage_ids_filename = storage_ids.conf\n\n");
    
    fprintf(out, "# HTTP tracker server port\n");
    fprintf(out, "http.tracker_server_port = 80\n\n");
}

int main(int argc, char *argv[])
{
    GeneratorOptions options;
    SystemInfo info;
    FILE *out = stdout;
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"type", required_argument, 0, 't'},
        {"profile", required_argument, 0, 'p'},
        {"base-path", required_argument, 0, 'b'},
        {"tracker", required_argument, 0, 'T'},
        {"port", required_argument, 0, 'P'},
        {"group", required_argument, 0, 'g'},
        {"store-path", required_argument, 0, 's'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    /* Initialize options */
    memset(&options, 0, sizeof(options));
    options.config_type = CONFIG_TRACKER;
    options.profile = PROFILE_STANDARD;
    options.tracker_port = 22122;
    options.storage_port = 23000;
    options.store_path_count = 1;
    
    /* Parse command line options */
    while ((opt = getopt_long(argc, argv, "t:p:b:T:P:g:s:o:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 't':
                if (strcmp(optarg, "tracker") == 0) {
                    options.config_type = CONFIG_TRACKER;
                } else if (strcmp(optarg, "storage") == 0) {
                    options.config_type = CONFIG_STORAGE;
                } else if (strcmp(optarg, "client") == 0) {
                    options.config_type = CONFIG_CLIENT;
                } else {
                    fprintf(stderr, "Error: Unknown config type '%s'\n", optarg);
                    return 1;
                }
                break;
            case 'p':
                if (strcmp(optarg, "minimal") == 0) {
                    options.profile = PROFILE_MINIMAL;
                } else if (strcmp(optarg, "standard") == 0) {
                    options.profile = PROFILE_STANDARD;
                } else if (strcmp(optarg, "performance") == 0) {
                    options.profile = PROFILE_PERFORMANCE;
                } else if (strcmp(optarg, "ha") == 0) {
                    options.profile = PROFILE_HIGH_AVAILABILITY;
                } else {
                    fprintf(stderr, "Error: Unknown profile '%s'\n", optarg);
                    return 1;
                }
                break;
            case 'b':
                strncpy(options.base_path, optarg, MAX_PATH_LENGTH - 1);
                break;
            case 'T':
                strncpy(options.tracker_server, optarg, 255);
                break;
            case 'P':
                if (options.config_type == CONFIG_TRACKER) {
                    options.tracker_port = atoi(optarg);
                } else {
                    options.storage_port = atoi(optarg);
                }
                break;
            case 'g':
                strncpy(options.group_name, optarg, 63);
                break;
            case 's':
                strncpy(options.store_path, optarg, MAX_PATH_LENGTH - 1);
                break;
            case 'o':
                strncpy(options.output_file, optarg, MAX_PATH_LENGTH - 1);
                break;
            case 'v':
                options.verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Get system information */
    get_system_info(&info);
    
    if (options.verbose) {
        printf("System Information:\n");
        printf("  Hostname: %s\n", info.hostname);
        printf("  CPU Count: %d\n", info.cpu_count);
        printf("  Available Memory: %ld MB\n", info.available_memory_mb);
        printf("  Disk Space: %ld GB\n", info.disk_space_gb);
        printf("\n");
    }
    
    /* Open output file if specified */
    if (options.output_file[0]) {
        out = fopen(options.output_file, "w");
        if (out == NULL) {
            fprintf(stderr, "Error: Cannot open output file '%s': %s\n",
                    options.output_file, strerror(errno));
            return 1;
        }
    }
    
    /* Generate configuration */
    switch (options.config_type) {
        case CONFIG_TRACKER:
            generate_tracker_config(&options, &info, out);
            break;
        case CONFIG_STORAGE:
            generate_storage_config(&options, &info, out);
            break;
        case CONFIG_CLIENT:
            generate_client_config(&options, &info, out);
            break;
    }
    
    /* Close output file */
    if (options.output_file[0] && out != stdout) {
        fclose(out);
        printf("Configuration written to %s\n", options.output_file);
    }
    
    return 0;
}
