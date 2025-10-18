/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "storage_global.h"

int g_subdir_count_per_path = FDFS_DEFAULT_DATA_DIR_COUNT_PER_PATH;

int g_last_server_port = 0;

bool g_disk_rw_direct = false;
bool g_disk_rw_separated = true;
int g_disk_reader_threads = FDFS_DEFAULT_DISK_READER_THREADS;
int g_disk_writer_threads = FDFS_DEFAULT_DISK_WRITER_THREADS;
int g_disk_recovery_threads = 1;
int g_extra_open_file_flags = 0;

int g_file_distribute_path_mode = FDFS_FILE_DIST_PATH_ROUND_ROBIN;
int g_file_distribute_rotate_count = FDFS_FILE_DIST_DEFAULT_ROTATE_COUNT;
int g_fsync_after_written_bytes = -1;

volatile int g_dist_path_index_high = 0; //current write to high path
volatile int g_dist_path_index_low = 0;  //current write to low path
volatile int g_dist_write_file_count = 0;  //current write file count

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
int g_sync_binlog_buff_interval = SYNC_BINLOG_BUFF_DEF_INTERVAL;
int g_write_mark_file_freq = FDFS_DEFAULT_SYNC_MARK_FILE_FREQ;
int g_sync_stat_file_interval = FDFS_DEFAULT_SYNC_STAT_FILE_INTERVAL;

FDFSStorageStat g_storage_stat;
int g_stat_change_count = 1;
int g_sync_change_count = 0;

int g_storage_join_time = 0;
int g_sync_until_timestamp = 0;
bool g_sync_old_done = false;
char g_sync_src_id[FDFS_STORAGE_ID_MAX_SIZE] = {0};

char g_group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
char g_my_server_id_str[FDFS_STORAGE_ID_MAX_SIZE] = {0}; //my server id string
FDFSMultiIP g_tracker_client_ip = {0, 0}; //storage ip as tracker client
FDFSMultiIP g_last_storage_ip = {0, 0};	  //the last storage ip address

LogContext g_access_log_context = {LOG_INFO, STDERR_FILENO, NULL};

in_addr_64_t g_server_id_in_filename = 0;
bool g_use_access_log = false;    //if log to access log
bool g_rotate_access_log = false; //if rotate the access log every day
bool g_compress_old_access_log = false; //if compress the old access log
bool g_use_storage_id = false;    //identify storage by ID instead of IP address
bool g_trust_storage_server_id = false;
byte g_id_type_in_filename = FDFS_ID_TYPE_IP_ADDRESS; //id type of the storage server in the filename
bool g_store_slave_file_use_link = false; //if store slave file use symbol link

bool g_check_file_duplicate = false;
byte g_file_signature_method = STORAGE_FILE_SIGNATURE_METHOD_HASH;
char g_key_namespace[FDHT_MAX_NAMESPACE_LEN+1] = {0};
int g_namespace_len = 0;
int g_allow_ip_count = 0;
in_addr_64_t *g_allow_ip_addrs = NULL;
StorageStatusPerTracker *g_my_report_status = NULL;  //returned by tracker server

TimeInfo g_access_log_rotate_time = {0, 0}; //rotate access log time base

bool g_client_bind_addr = true;
bool g_storage_ip_changed_auto_adjust = false;
bool g_thread_kill_done = false;
bool g_file_sync_skip_invalid_record = false;

bool g_check_store_path_mark = true;
bool g_compress_binlog = false;
TimeInfo g_compress_binlog_time = {0, 0};

int g_upload_priority = FDFS_DEFAULT_UPLOAD_PRIORITY;

int g_response_ip_addr_size = IPV6_ADDRESS_SIZE;

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
char g_exe_name[256] = {0};
#endif

int g_compress_access_log_days_before = 0;
struct storage_dio_thread_data *g_dio_thread_data = NULL;

FDFSStorageIdInfo *g_my_storage_id_info = NULL;

int storage_cmp_by_server_id(const void *p1, const void *p2)
{
	return strcmp((*((FDFSStorageServer **)p1))->server.id,
		(*((FDFSStorageServer **)p2))->server.id);
}


int storage_insert_ip_addr_to_multi_ips(FDFSMultiIP *multi_ip,
        const char *ip_addr, const int ips_limit)
{
    int i;
    if (multi_ip->count == 0)
    {
        multi_ip->count = 1;
        multi_ip->ips[0].type = fdfs_get_ip_type(ip_addr);
        strcpy(multi_ip->ips[0].address, ip_addr);
        return 0;
    }

    for (i = 0; i < multi_ip->count; i++)
    {
        if (strcmp(multi_ip->ips[i].address, ip_addr) == 0)
        {
            return EEXIST;
        }
    }

    if (i >= ips_limit)
    {
        return ENOSPC;
    }

    multi_ip->ips[i].type = fdfs_get_ip_type(ip_addr);
    strcpy(multi_ip->ips[i].address, ip_addr);
    multi_ip->count++;
    return 0;
}
