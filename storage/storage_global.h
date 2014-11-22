/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//storage_global.h

#ifndef _STORAGE_GLOBAL_H
#define _STORAGE_GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common_define.h"
#include "fdfs_define.h"
#include "tracker_types.h"
#include "client_global.h"
#include "fdht_types.h"
#include "local_ip_func.h"
#include "trunk_shared.h"

#ifdef WITH_HTTPD
#include "fdfs_http_shared.h"
#endif

#define STORAGE_BEAT_DEF_INTERVAL    30
#define STORAGE_REPORT_DEF_INTERVAL  300
#define STORAGE_DEF_SYNC_WAIT_MSEC   100
#define DEFAULT_DISK_READER_THREADS  1
#define DEFAULT_DISK_WRITER_THREADS  1
#define DEFAULT_SYNC_STAT_FILE_INTERVAL  300
#define DEFAULT_DATA_DIR_COUNT_PER_PATH	 256
#define DEFAULT_UPLOAD_PRIORITY           10
#define FDFS_DEFAULT_SYNC_MARK_FILE_FREQ  500
#define STORAGE_DEFAULT_BUFF_SIZE    (64 * 1024)

#define STORAGE_FILE_SIGNATURE_METHOD_HASH  1
#define STORAGE_FILE_SIGNATURE_METHOD_MD5   2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	FDFSStorageBrief server;
	int last_sync_src_timestamp;
} FDFSStorageServer;

typedef struct
{
	int total_mb; //total spaces
	int free_mb;  //free spaces
} FDFSStorePathInfo;

extern volatile bool g_continue_flag;

extern FDFSStorePathInfo *g_path_space_list;

/* subdirs under store path, g_subdir_count * g_subdir_count 2 level subdirs */
extern int g_subdir_count_per_path;

extern int g_server_port;
extern int g_http_port;  //http server port
extern int g_last_server_port;
extern int g_last_http_port;  //last http server port
extern char g_http_domain[FDFS_DOMAIN_NAME_MAX_SIZE];  //http server domain name
extern int g_max_connections;
extern int g_accept_threads;
extern int g_work_threads;
extern int g_buff_size;
 
extern bool g_disk_rw_direct;     //if file read / write directly
extern bool g_disk_rw_separated;  //if disk read / write separated
extern int g_disk_reader_threads; //disk reader thread count per store base path
extern int g_disk_writer_threads; //disk writer thread count per store base path
extern int g_extra_open_file_flags; //extra open file flags

extern int g_file_distribute_path_mode;
extern int g_file_distribute_rotate_count;
extern int g_fsync_after_written_bytes;

extern int g_dist_path_index_high; //current write to high path
extern int g_dist_path_index_low;  //current write to low path
extern int g_dist_write_file_count; //current write file count

extern int g_storage_count;  //stoage server count in my group
extern FDFSStorageServer g_storage_servers[FDFS_MAX_SERVERS_EACH_GROUP];
extern FDFSStorageServer *g_sorted_storages[FDFS_MAX_SERVERS_EACH_GROUP];

extern int g_tracker_reporter_count;
extern int g_heart_beat_interval;
extern int g_stat_report_interval;
extern int g_sync_wait_usec;
extern int g_sync_interval; //unit: milliseconds
extern TimeInfo g_sync_start_time;
extern TimeInfo g_sync_end_time;
extern bool g_sync_part_time; //true for part time, false for all time of a day
extern int g_sync_log_buff_interval; //sync log buff to disk every interval seconds
extern int g_sync_binlog_buff_interval; //sync binlog buff to disk every interval seconds
extern int g_write_mark_file_freq;      //write to mark file after sync N files
extern int g_sync_stat_file_interval;   //sync storage stat info to disk interval

extern FDFSStorageStat g_storage_stat;
extern int g_stat_change_count;
extern int g_sync_change_count; //sync src timestamp change counter

extern int g_storage_join_time;  //my join timestamp
extern int g_sync_until_timestamp;
extern bool g_sync_old_done;     //if old files synced to me done
extern char g_sync_src_id[FDFS_STORAGE_ID_MAX_SIZE]; //the source storage server id

extern char g_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
extern char g_my_server_id_str[FDFS_STORAGE_ID_MAX_SIZE]; //my server id string
extern char g_tracker_client_ip[IP_ADDRESS_SIZE]; //storage ip as tracker client
extern char g_last_storage_ip[IP_ADDRESS_SIZE];	//the last storage ip address

extern LogContext g_access_log_context;

extern in_addr_t g_server_id_in_filename;
extern bool g_store_slave_file_use_link; //if store slave file use symbol link
extern bool g_use_storage_id;  //identify storage by ID instead of IP address
extern byte g_id_type_in_filename; //id type of the storage server in the filename
extern bool g_use_access_log;  //if log to access log
extern bool g_rotate_access_log;  //if rotate the access log every day
extern bool g_rotate_error_log;  //if rotate the error log every day

extern TimeInfo g_access_log_rotate_time; //rotate access log time base
extern TimeInfo g_error_log_rotate_time;  //rotate error log time base

extern bool g_check_file_duplicate;  //if check file content duplicate
extern byte g_file_signature_method; //file signature method
extern char g_key_namespace[FDHT_MAX_NAMESPACE_LEN+1];
extern int g_namespace_len;

extern int g_allow_ip_count;  /* -1 means match any ip address */
extern in_addr_t *g_allow_ip_addrs;  /* sorted array, asc order */

extern gid_t g_run_by_gid;
extern uid_t g_run_by_uid;

extern char g_run_by_group[32];
extern char g_run_by_user[32];

extern char g_bind_addr[IP_ADDRESS_SIZE];
extern bool g_client_bind_addr;
extern bool g_storage_ip_changed_auto_adjust;
extern bool g_thread_kill_done;

extern bool g_file_sync_skip_invalid_record;

extern int g_thread_stack_size;
extern int g_upload_priority;
extern time_t g_up_time;

#ifdef WITH_HTTPD
extern FDFSHTTPParams g_http_params;
extern int g_http_trunk_size;
#endif

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
extern char g_exe_name[256];
#endif

extern int g_log_file_keep_days;

extern struct storage_nio_thread_data *g_nio_thread_data;  //network io thread data
extern struct storage_dio_thread_data *g_dio_thread_data;  //disk io thread data

int storage_cmp_by_server_id(const void *p1, const void *p2);

#ifdef __cplusplus
}
#endif

#endif

