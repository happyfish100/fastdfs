/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "fdfs_shared_func.h"
#include "storage_global.h"
#include "storage_param_getter.h"
#include "trunk_mem.h"
#include "trunk_sync.h"

static int storage_convert_src_server_id()
{
	TrackerServerInfo *pTrackerServer;
	TrackerServerInfo *pServerEnd;
	ConnectionInfo *pTrackerConn;
	TrackerServerInfo tracker_server;
	int result;

	result = ENOENT;
	pServerEnd = g_tracker_group.servers + g_tracker_group.server_count;
	for (pTrackerServer=g_tracker_group.servers;
		pTrackerServer<pServerEnd; pTrackerServer++)
	{
		memcpy(&tracker_server, pTrackerServer,
			sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(&tracker_server);
        if ((pTrackerConn=tracker_connect_server(&tracker_server,
			&result)) == NULL)
		{
			continue;
		}

		result = tracker_get_storage_id(pTrackerConn,
			g_group_name, g_sync_src_id, g_sync_src_id);
		tracker_close_connection_ex(pTrackerConn,
			result != 0 && result != ENOENT);
		if (result == 0)
		{
			return 0;
		}
	}

	return result;
}

int storage_get_params_from_tracker()
{
	IniContext iniContext;
	int result;
	bool use_trunk_file;
	char reserved_space_str[32];
	char *pIdType;

	if ((result=fdfs_get_ini_context_from_tracker(&g_tracker_group,
		&iniContext, (bool * volatile)&g_continue_flag,
		g_client_bind_addr, g_bind_addr)) != 0)
	{
		return result;
	}

	g_storage_ip_changed_auto_adjust = iniGetBoolValue(NULL,
			"storage_ip_changed_auto_adjust",
			&iniContext, false);

	g_store_path_mode = iniGetIntValue(NULL, "store_path", &iniContext,
				FDFS_STORE_PATH_ROUND_ROBIN);

	if ((result=fdfs_parse_storage_reserved_space(&iniContext,
		&g_storage_reserved_space)) != 0)
	{
		iniFreeContext(&iniContext);
		return result;
	}
	if (g_storage_reserved_space.flag ==
		TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB)
	{
		g_avg_storage_reserved_mb = g_storage_reserved_space.rs.mb
						/ g_fdfs_store_paths.count;
	}
	else
	{
		g_avg_storage_reserved_mb = 0;
	}

	g_use_storage_id = iniGetBoolValue(NULL, "use_storage_id",
				&iniContext, false);
	use_trunk_file = iniGetBoolValue(NULL, "use_trunk_file",
				&iniContext, false);
	g_slot_min_size = iniGetIntValue(NULL, "slot_min_size",
				&iniContext, 256);
	g_trunk_file_size = iniGetIntValue(NULL, "trunk_file_size",
				&iniContext, 64 * 1024 * 1024);
	g_slot_max_size = iniGetIntValue(NULL, "slot_max_size",
				&iniContext, g_trunk_file_size / 4);
	g_trunk_alloc_alignment_size = iniGetIntValue(NULL,
            "trunk_alloc_alignment_size", &iniContext, 0);
    if (g_slot_min_size < g_trunk_alloc_alignment_size)
    {
        g_slot_min_size = g_trunk_alloc_alignment_size;
    }

	g_trunk_create_file_advance = iniGetBoolValue(NULL,
			"trunk_create_file_advance", &iniContext, false);
	if ((result=get_time_item_from_conf(&iniContext,
               	"trunk_create_file_time_base",
		&g_trunk_create_file_time_base, 2, 0)) != 0)
	{
		iniFreeContext(&iniContext);
		return result;
	}
	g_trunk_create_file_interval = iniGetIntValue(NULL,
			"trunk_create_file_interval", &iniContext,
			86400);
	g_trunk_create_file_space_threshold = iniGetInt64Value(NULL,
			"trunk_create_file_space_threshold",
			&iniContext, 0);

	g_trunk_init_check_occupying = iniGetBoolValue(NULL,
			"trunk_init_check_occupying", &iniContext, false);
	g_trunk_init_reload_from_binlog = iniGetBoolValue(NULL,
			"trunk_init_reload_from_binlog", &iniContext, false);
	g_trunk_free_space_merge = iniGetBoolValue(NULL,
			"trunk_free_space_merge", &iniContext, false);
	g_delete_unused_trunk_files = iniGetBoolValue(NULL,
			"delete_unused_trunk_files", &iniContext, false);
	g_trunk_compress_binlog_min_interval = iniGetIntValue(NULL,
			"trunk_compress_binlog_min_interval", &iniContext, 0);
	g_trunk_compress_binlog_interval = iniGetIntValue(NULL,
			"trunk_compress_binlog_interval", &iniContext, 0);
    if ((result=get_time_item_from_conf(&iniContext,
                    "trunk_compress_binlog_time_base",
                    &g_trunk_compress_binlog_time_base, 3, 0)) != 0)
    {
        return result;
    }

    g_trunk_binlog_max_backups = iniGetIntValue(NULL,
			"trunk_binlog_max_backups", &iniContext, 0);
	g_store_slave_file_use_link = iniGetBoolValue(NULL,
			"store_slave_file_use_link", &iniContext, false);

	pIdType = iniGetStrValue(NULL, "id_type_in_filename", &iniContext);
	if (pIdType != NULL && strcasecmp(pIdType, "id") == 0)
	{
		if (g_use_storage_id)
		{
			g_id_type_in_filename = FDFS_ID_TYPE_SERVER_ID;
		}
		else
		{
			g_id_type_in_filename = FDFS_ID_TYPE_IP_ADDRESS;
		}
	}
	else 
	{
		g_id_type_in_filename = FDFS_ID_TYPE_IP_ADDRESS;
	}

	iniFreeContext(&iniContext);

	if (use_trunk_file && !g_if_use_trunk_file)
	{
		if ((result=trunk_sync_init()) != 0)
		{
			return result;
		}
	}
	g_if_use_trunk_file = use_trunk_file;

	logInfo("file: "__FILE__", line: %d, "
		"use_storage_id=%d, "
		"id_type_in_filename=%s, "
		"storage_ip_changed_auto_adjust=%d, "
		"store_path=%d, "
		"reserved_storage_space=%s, "
		"use_trunk_file=%d, "
		"slot_min_size=%d, "
		"slot_max_size=%d KB, "
		"trunk_alloc_alignment_size=%d, "
		"trunk_file_size=%d MB, "
		"trunk_create_file_advance=%d, "
		"trunk_create_file_time_base=%02d:%02d, "
		"trunk_create_file_interval=%d, "
		"trunk_create_file_space_threshold=%d GB, "
		"trunk_init_check_occupying=%d, "
		"trunk_init_reload_from_binlog=%d, "
		"trunk_free_space_merge=%d, "
		"delete_unused_trunk_files=%d, "
		"trunk_compress_binlog_min_interval=%d, "
		"trunk_compress_binlog_interval=%d, "
		"trunk_compress_binlog_time_base=%02d:%02d, "
		"trunk_binlog_max_backups=%d, "
		"store_slave_file_use_link=%d",
		__LINE__, g_use_storage_id,
		g_id_type_in_filename == FDFS_ID_TYPE_SERVER_ID ? "id" : "ip",
		g_storage_ip_changed_auto_adjust,
		g_store_path_mode, fdfs_storage_reserved_space_to_string(
			&g_storage_reserved_space, reserved_space_str),
		g_if_use_trunk_file, g_slot_min_size,
		g_slot_max_size / 1024,
        g_trunk_alloc_alignment_size,
		g_trunk_file_size / FDFS_ONE_MB,
		g_trunk_create_file_advance,
		g_trunk_create_file_time_base.hour,
		g_trunk_create_file_time_base.minute,
		g_trunk_create_file_interval,
		(int)(g_trunk_create_file_space_threshold /
		(FDFS_ONE_MB * 1024)), g_trunk_init_check_occupying,
		g_trunk_init_reload_from_binlog,
		g_trunk_free_space_merge,
		g_delete_unused_trunk_files,
		g_trunk_compress_binlog_min_interval,
		g_trunk_compress_binlog_interval,
        g_trunk_compress_binlog_time_base.hour,
        g_trunk_compress_binlog_time_base.minute,
        g_trunk_binlog_max_backups,
		g_store_slave_file_use_link);

	if (g_use_storage_id && *g_sync_src_id != '\0' &&
		!fdfs_is_server_id_valid(g_sync_src_id))
	{
		if ((result=storage_convert_src_server_id()) == 0)
		{
			storage_write_to_sync_ini_file();
		}
		else
		{
			if (result == ENOENT)
			{
				*g_sync_src_id = '\0';
				storage_write_to_sync_ini_file();
			}
			else
			{
				return result;
			}
		}
	}

	return 0;
}

