/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//tracker_global.h

#ifndef _TRACKER_GLOBAL_H
#define _TRACKER_GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fastcommon/common_define.h"
#include "fdfs_define.h"
#include "tracker_types.h"
#include "tracker_status.h"
#include "fastcommon/base64.h"
#include "fastcommon/hash.h"
#include "fastcommon/local_ip_func.h"

#ifdef WITH_HTTPD
#include "fdfs_http_shared.h"
#endif

#define TRACKER_SYNC_TO_FILE_FREQ		1000
#define TRACKER_MAX_PACKAGE_SIZE		(8 * 1024)
#define TRACKER_SYNC_STATUS_FILE_INTERVAL	300   //5 minute

#ifdef __cplusplus
extern "C" {
#endif

extern FDFSGroups g_groups;
extern int g_storage_stat_chg_count;
extern int g_storage_sync_time_chg_count; //sync timestamp

extern FDFSStorageReservedSpace g_storage_reserved_space;
extern int g_check_active_interval; //check storage server alive every interval seconds

extern int g_allow_ip_count;  /* -1 means match any ip address */
extern in_addr_t *g_allow_ip_addrs;  /* sorted array, asc order */
extern struct base64_context g_base64_context;

extern bool g_storage_ip_changed_auto_adjust;
extern bool g_use_storage_id;  //identify storage by ID instead of IP address
extern byte g_id_type_in_filename; //id type of the storage server in the filename

extern int g_storage_sync_file_max_delay;
extern int g_storage_sync_file_max_time;

extern bool g_store_slave_file_use_link; //if store slave file use symbol link
extern bool g_if_use_trunk_file;   //if use trunk file
extern bool g_trunk_create_file_advance;
extern bool g_trunk_init_check_occupying;
extern bool g_trunk_init_reload_from_binlog;
extern bool g_trunk_free_space_merge;
extern bool g_delete_unused_trunk_files;
extern int g_slot_min_size;    //slot min size, such as 256 bytes
extern int g_slot_max_size;    //slot max size, such as 16MB
extern int g_trunk_file_size;  //the trunk file size, such as 64MB
extern TimeInfo g_trunk_create_file_time_base;
extern TimeInfo g_trunk_compress_binlog_time_base;
extern int g_trunk_create_file_interval;
extern int g_trunk_compress_binlog_interval;
extern int g_trunk_compress_binlog_min_interval;
extern int g_trunk_binlog_max_backups;
extern int g_trunk_alloc_alignment_size;
extern int64_t g_trunk_create_file_space_threshold;

extern TrackerStatus g_tracker_last_status;  //the status of last running

#ifdef WITH_HTTPD
extern FDFSHTTPParams g_http_params;
extern int g_http_check_interval;
extern int g_http_check_type;
extern char g_http_check_uri[128];
extern bool g_http_servers_dirty;
#endif

#if defined(DEBUG_FLAG) && defined(OS_LINUX)
extern char g_exe_name[256];
#endif

extern FDFSConnectionStat g_connection_stat;

#ifdef __cplusplus
}
#endif

#endif
