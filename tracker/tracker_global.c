/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include "tracker_global.h"

int g_check_active_interval = CHECK_ACTIVE_DEF_INTERVAL;

FDFSGroups g_groups;
int g_storage_stat_chg_count = 0;
int g_storage_sync_time_chg_count = 0; //sync timestamp
FDFSStorageReservedSpace g_storage_reserved_space = { \
		TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB};

int g_allow_ip_count = 0;
in_addr_64_t *g_allow_ip_addrs = NULL;

bool g_storage_ip_changed_auto_adjust = true;
bool g_use_storage_id = false;  //if use storage ID instead of IP address
bool g_trust_storage_server_id = true;
byte g_id_type_in_filename = FDFS_ID_TYPE_IP_ADDRESS; //id type of the storage server in the filename

int g_storage_sync_file_max_delay = DEFAULT_STORAGE_SYNC_FILE_MAX_DELAY;
int g_storage_sync_file_max_time = DEFAULT_STORAGE_SYNC_FILE_MAX_TIME;

bool g_store_slave_file_use_link = false; //if store slave file use symbol link
bool g_if_use_trunk_file = false;   //if use trunk file
bool g_trunk_create_file_advance = false;
bool g_trunk_init_check_occupying = false;
bool g_trunk_init_reload_from_binlog = false;
bool g_trunk_free_space_merge = false;
bool g_delete_unused_trunk_files  = false;
int g_slot_min_size = 256;    //slot min size, such as 256 bytes
int g_slot_max_size = 16 * 1024 * 1024;    //slot max size, such as 16MB
int g_trunk_file_size = 64 * 1024 * 1024;  //the trunk file size, such as 64MB
TimeInfo g_trunk_create_file_time_base = {0, 0};
TimeInfo g_trunk_compress_binlog_time_base = {0, 0};
int g_trunk_create_file_interval = 86400;
int g_trunk_compress_binlog_interval = 0;
int g_trunk_compress_binlog_min_interval = 0;
int g_trunk_binlog_max_backups = 0;
int g_trunk_alloc_alignment_size = 0;
int64_t g_trunk_create_file_space_threshold = 0;

TrackerStatus g_tracker_last_status = {0, 0};

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
char g_exe_name[256] = {0};
#endif
