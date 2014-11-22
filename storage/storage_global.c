/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include "logger.h"
#include "sockopt.h"
#include "shared_func.h"
#include "storage_global.h"

volatile bool g_continue_flag = true;
FDFSStorePathInfo *g_path_space_list = NULL;
int g_subdir_count_per_path = DEFAULT_DATA_DIR_COUNT_PER_PATH;

int g_server_port = FDFS_STORAGE_SERVER_DEF_PORT;
char g_http_domain[FDFS_DOMAIN_NAME_MAX_SIZE] = {0};
int g_http_port = 80;
int g_last_server_port = 0;
int g_last_http_port = 0;
int g_max_connections = DEFAULT_MAX_CONNECTONS;
int g_accept_threads = 1;
int g_work_threads = DEFAULT_WORK_THREADS;
int g_buff_size = STORAGE_DEFAULT_BUFF_SIZE;

bool g_disk_rw_direct = false;
bool g_disk_rw_separated = true;
int g_disk_reader_threads = DEFAULT_DISK_READER_THREADS;
int g_disk_writer_threads = DEFAULT_DISK_WRITER_THREADS;
int g_extra_open_file_flags = 0;

int g_file_distribute_path_mode = FDFS_FILE_DIST_PATH_ROUND_ROBIN;
int g_file_distribute_rotate_count = FDFS_FILE_DIST_DEFAULT_ROTATE_COUNT;
int g_fsync_after_written_bytes = -1;

int g_dist_path_index_high = 0; //current write to high path
int g_dist_path_index_low = 0;  //current write to low path
int g_dist_write_file_count = 0;  //current write file count

int g_storage_count = 0;
FDFSStorageServer g_storage_servers[FDFS_MAX_SERVERS_EACH_GROUP];
FDFSStorageServer *g_sorted_storages[FDFS_MAX_SERVERS_EACH_GROUP];

int g_tracker_reporter_count = 0;
int g_heart_beat_interval  = STORAGE_BEAT_DEF_INTERVAL;
int g_stat_report_interval = STORAGE_REPORT_DEF_INTERVAL;

int g_sync_wait_usec = STORAGE_DEF_SYNC_WAIT_MSEC;
int g_sync_interval = 0; //unit: milliseconds
TimeInfo g_sync_start_time = {0, 0};
TimeInfo g_sync_end_time = {23, 59};
bool g_sync_part_time = false;
int g_sync_log_buff_interval = SYNC_LOG_BUFF_DEF_INTERVAL;
int g_sync_binlog_buff_interval = SYNC_BINLOG_BUFF_DEF_INTERVAL;
int g_write_mark_file_freq = FDFS_DEFAULT_SYNC_MARK_FILE_FREQ;
int g_sync_stat_file_interval = DEFAULT_SYNC_STAT_FILE_INTERVAL;

FDFSStorageStat g_storage_stat;
int g_stat_change_count = 1;
int g_sync_change_count = 0;

int g_storage_join_time = 0;
int g_sync_until_timestamp = 0;
bool g_sync_old_done = false;
char g_sync_src_id[FDFS_STORAGE_ID_MAX_SIZE] = {0};

char g_group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
char g_my_server_id_str[FDFS_STORAGE_ID_MAX_SIZE] = {0}; //my server id string
char g_tracker_client_ip[IP_ADDRESS_SIZE] = {0}; //storage ip as tracker client
char g_last_storage_ip[IP_ADDRESS_SIZE] = {0};	 //the last storage ip address

LogContext g_access_log_context = {LOG_INFO, STDERR_FILENO, NULL};

in_addr_t g_server_id_in_filename = 0;
bool g_use_access_log = false;    //if log to access log
bool g_rotate_access_log = false; //if rotate the access log every day
bool g_rotate_error_log = false;  //if rotate the error log every day
bool g_use_storage_id = false;    //identify storage by ID instead of IP address
byte g_id_type_in_filename = FDFS_ID_TYPE_IP_ADDRESS; //id type of the storage server in the filename
bool g_store_slave_file_use_link = false; //if store slave file use symbol link

bool g_check_file_duplicate = false;
byte g_file_signature_method = STORAGE_FILE_SIGNATURE_METHOD_HASH;
char g_key_namespace[FDHT_MAX_NAMESPACE_LEN+1] = {0};
int g_namespace_len = 0;
int g_allow_ip_count = 0;
in_addr_t *g_allow_ip_addrs = NULL;

TimeInfo g_access_log_rotate_time = {0, 0}; //rotate access log time base
TimeInfo g_error_log_rotate_time  = {0, 0}; //rotate error log time base

gid_t g_run_by_gid;
uid_t g_run_by_uid;

char g_run_by_group[32] = {0};
char g_run_by_user[32] = {0};

char g_bind_addr[IP_ADDRESS_SIZE] = {0};
bool g_client_bind_addr = true;
bool g_storage_ip_changed_auto_adjust = false;
bool g_thread_kill_done = false;
bool g_file_sync_skip_invalid_record = false;

int g_thread_stack_size = 512 * 1024;
int g_upload_priority = DEFAULT_UPLOAD_PRIORITY;
time_t g_up_time = 0;

#ifdef WITH_HTTPD
FDFSHTTPParams g_http_params;
int g_http_trunk_size = 64 * 1024;
#endif

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
char g_exe_name[256] = {0};
#endif

int g_log_file_keep_days = 0;
struct storage_nio_thread_data *g_nio_thread_data = NULL;
struct storage_dio_thread_data *g_dio_thread_data = NULL;

int storage_cmp_by_server_id(const void *p1, const void *p2)
{
	return strcmp((*((FDFSStorageServer **)p1))->server.id,
		(*((FDFSStorageServer **)p2))->server.id);
}

