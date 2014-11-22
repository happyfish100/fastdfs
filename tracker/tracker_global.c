/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include "tracker_global.h"

volatile bool g_continue_flag = true;
int g_server_port = FDFS_TRACKER_SERVER_DEF_PORT;
int g_max_connections = DEFAULT_MAX_CONNECTONS;
int g_accept_threads = 1;
int g_work_threads = DEFAULT_WORK_THREADS;
int g_sync_log_buff_interval = SYNC_LOG_BUFF_DEF_INTERVAL;
int g_check_active_interval = CHECK_ACTIVE_DEF_INTERVAL;

FDFSGroups g_groups;
int g_storage_stat_chg_count = 0;
int g_storage_sync_time_chg_count = 0; //sync timestamp
FDFSStorageReservedSpace g_storage_reserved_space = { \
		TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB};

int g_allow_ip_count = 0;
in_addr_t *g_allow_ip_addrs = NULL;

struct base64_context g_base64_context;

gid_t g_run_by_gid;
uid_t g_run_by_uid;

char g_run_by_group[32] = {0};
char g_run_by_user[32] = {0};

bool g_storage_ip_changed_auto_adjust = true;
bool g_use_storage_id = false;  //if use storage ID instead of IP address
byte g_id_type_in_filename = FDFS_ID_TYPE_IP_ADDRESS; //id type of the storage server in the filename
bool g_rotate_error_log = false;  //if rotate the error log every day
TimeInfo g_error_log_rotate_time  = {0, 0}; //rotate error log time base

int g_thread_stack_size = 64 * 1024;
int g_storage_sync_file_max_delay = DEFAULT_STORAGE_SYNC_FILE_MAX_DELAY;
int g_storage_sync_file_max_time = DEFAULT_STORAGE_SYNC_FILE_MAX_TIME;

bool g_store_slave_file_use_link = false; //if store slave file use symbol link
bool g_if_use_trunk_file = false;   //if use trunk file
bool g_trunk_create_file_advance = false;
bool g_trunk_init_check_occupying = false;
bool g_trunk_init_reload_from_binlog = false;
int g_slot_min_size = 256;    //slot min size, such as 256 bytes
int g_slot_max_size = 16 * 1024 * 1024;    //slot max size, such as 16MB
int g_trunk_file_size = 64 * 1024 * 1024;  //the trunk file size, such as 64MB
TimeInfo g_trunk_create_file_time_base = {0, 0};
int g_trunk_create_file_interval = 86400;
int g_trunk_compress_binlog_min_interval = 0;
int64_t g_trunk_create_file_space_threshold = 0;

time_t g_up_time = 0;
TrackerStatus g_tracker_last_status = {0, 0};

#ifdef WITH_HTTPD
FDFSHTTPParams g_http_params;
int g_http_check_interval = 30;
int g_http_check_type = FDFS_HTTP_CHECK_ALIVE_TYPE_TCP;
char g_http_check_uri[128] = {0};
bool g_http_servers_dirty = false;
#endif

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
char g_exe_name[256] = {0};
#endif

int g_log_file_keep_days = 0;
FDFSConnectionStat g_connection_stat = {0, 0};

