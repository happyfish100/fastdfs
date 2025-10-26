/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//tracker_func.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/connection_pool.h"
#include "sf/sf_service.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_global.h"
#include "tracker_func.h"
#include "tracker_mem.h"
#include "fastcommon/local_ip_func.h"
#include "fdfs_shared_func.h"

static int tracker_load_store_lookup(const char *filename, \
		IniContext *pItemContext)
{
	char *pGroupName;
	g_groups.store_lookup = iniGetIntValue(NULL, "store_lookup", \
			pItemContext, FDFS_STORE_LOOKUP_ROUND_ROBIN);
	if (g_groups.store_lookup == FDFS_STORE_LOOKUP_ROUND_ROBIN)
	{
		g_groups.store_group[0] = '\0';
		return 0;
	}

	if (g_groups.store_lookup == FDFS_STORE_LOOKUP_LOAD_BALANCE)
	{
		g_groups.store_group[0] = '\0';
		return 0;
	}

	if (g_groups.store_lookup != FDFS_STORE_LOOKUP_SPEC_GROUP)
	{
		logError("file: "__FILE__", line: %d, " \
			"conf file \"%s\", the value of \"store_lookup\" is " \
			"invalid, value=%d!", \
			__LINE__, filename, g_groups.store_lookup);
		return EINVAL;
	}

	pGroupName = iniGetStrValue(NULL, "store_group", pItemContext);
	if (pGroupName == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"conf file \"%s\" must have item " \
			"\"store_group\"!", __LINE__, filename);
		return ENOENT;
	}
	if (pGroupName[0] == '\0')
	{
		logError("file: "__FILE__", line: %d, " \
			"conf file \"%s\", " \
			"store_group is empty!", __LINE__, filename);
		return EINVAL;
	}

	fc_safe_strcpy(g_groups.store_group, pGroupName);
	if (fdfs_validate_group_name(g_groups.store_group) != 0) \
	{
		logError("file: "__FILE__", line: %d, " \
			"config file \"%s\", " \
			"the group name \"%s\" is invalid!", \
			__LINE__, filename, g_groups.store_group);
		return EINVAL;
	}

	return 0;
}

static int tracker_load_storage_id_info(const char *config_filename,
		IniContext *iniContext)
{
	char *pIdType;

	g_use_storage_id = iniGetBoolValue(NULL, "use_storage_id",
				iniContext, false);
	if (!g_use_storage_id)
	{
        if (SF_G_IPV6_ENABLED)
        {
            logError("file: "__FILE__", line: %d, "
                    "config file: %s, use_storage_id MUST set to true "
                    "when IPv6 enabled!", __LINE__, config_filename);
            return EINVAL;
        }

		return 0;
	}

	pIdType = iniGetStrValue(NULL, "id_type_in_filename", iniContext);
	if (pIdType != NULL && strcasecmp(pIdType, "id") == 0)
	{
		g_id_type_in_filename = FDFS_ID_TYPE_SERVER_ID;
	}
	else
    {
        if (SF_G_IPV6_ENABLED)
        {
            logError("file: "__FILE__", line: %d, "
                    "config file: %s, id_type_in_filename MUST set to id "
                    "when IPv6 enabled!", __LINE__, config_filename);
            return EINVAL;
        }
        g_id_type_in_filename = FDFS_ID_TYPE_IP_ADDRESS;
    }

    g_trust_storage_server_id = iniGetBoolValue(NULL,
            "trust_storage_server_id", iniContext, true);
	return fdfs_load_storage_ids_from_file(config_filename, iniContext);
}

int tracker_load_from_conf_file(const char *filename)
{
    const int fixed_buffer_size = 0;
    const int task_buffer_extra_size = 0;
    const bool need_set_run_by = false;
	char *pSlotMinSize;
	char *pSlotMaxSize;
	char *pSpaceThreshold;
	char *pTrunkFileSize;
    char *pResponseIpAddrSize;
	IniContext iniContext;
    SFContextIniConfig config;
	int result;
	int64_t trunk_file_size;
	int64_t slot_min_size;
	int64_t slot_max_size;
	char reserved_space_str[32];
    char sz_global_config[512];
    char sz_service_config[128];

	memset(&g_groups, 0, sizeof(FDFSGroups));
	memset(&iniContext, 0, sizeof(IniContext));
	if ((result=iniLoadFromFile(filename, &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"load conf file \"%s\" fail, ret code: %d", \
			__LINE__, filename, result);
		return result;
	}

	//iniPrintItems(&iniContext);

	do
	{
		if (iniGetBoolValue(NULL, "disabled", &iniContext, false))
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file \"%s\" disabled=true, exit", \
				__LINE__, filename);
			result = ECANCELED;
			break;
		}

        sf_set_current_time();

        SF_SET_CONTEXT_INI_CONFIG_EX(config, fc_comm_type_sock, filename,
                &iniContext, NULL, FDFS_TRACKER_SERVER_DEF_PORT,
                FDFS_TRACKER_SERVER_DEF_PORT, DEFAULT_WORK_THREADS,
                "buff_size", 0);
        if ((result=sf_load_config_ex("trackerd", &config, fixed_buffer_size,
                        task_buffer_extra_size, need_set_run_by)) != 0)
        {
            return result;
        }

		if ((result=tracker_load_store_lookup(filename, &iniContext)) != 0)
		{
			break;
		}

		g_groups.store_server = (byte)iniGetIntValue(NULL, \
				"store_server",  &iniContext, \
				FDFS_STORE_SERVER_ROUND_ROBIN);
		if (!(g_groups.store_server == FDFS_STORE_SERVER_FIRST_BY_IP ||
			g_groups.store_server == FDFS_STORE_SERVER_FIRST_BY_PRI||
			g_groups.store_server == FDFS_STORE_SERVER_ROUND_ROBIN))
		{
			logWarning("file: "__FILE__", line: %d, " \
				"store_server 's value %d is invalid, " \
				"set to %d (round robin)!", \
				__LINE__, g_groups.store_server, \
				FDFS_STORE_SERVER_ROUND_ROBIN);

			g_groups.store_server = FDFS_STORE_SERVER_ROUND_ROBIN;
		}

		g_groups.download_server = (byte)iniGetIntValue(NULL, \
			"download_server", &iniContext, \
			FDFS_DOWNLOAD_SERVER_ROUND_ROBIN);
		if (!(g_groups.download_server==FDFS_DOWNLOAD_SERVER_ROUND_ROBIN
			|| g_groups.download_server == 
				FDFS_DOWNLOAD_SERVER_SOURCE_FIRST))
		{
			logWarning("file: "__FILE__", line: %d, " \
				"download_server 's value %d is invalid, " \
				"set to %d (round robin)!", \
				__LINE__, g_groups.download_server, \
				FDFS_DOWNLOAD_SERVER_ROUND_ROBIN);

			g_groups.download_server = \
				FDFS_DOWNLOAD_SERVER_ROUND_ROBIN;
		}

		g_groups.store_path = (byte)iniGetIntValue(NULL, "store_path", \
			&iniContext, FDFS_STORE_PATH_ROUND_ROBIN);
		if (!(g_groups.store_path == FDFS_STORE_PATH_ROUND_ROBIN || \
			g_groups.store_path == FDFS_STORE_PATH_LOAD_BALANCE))
		{
			logWarning("file: "__FILE__", line: %d, " \
				"store_path 's value %d is invalid, " \
				"set to %d (round robin)!", \
				__LINE__, g_groups.store_path , \
				FDFS_STORE_PATH_ROUND_ROBIN);
			g_groups.store_path = FDFS_STORE_PATH_ROUND_ROBIN;
		}

		if ((result=fdfs_parse_storage_reserved_space(&iniContext, \
				&g_storage_reserved_space)) != 0)
		{
			break;
		}

		if ((result=load_allow_hosts(&iniContext, \
                	 &g_allow_ip_addrs, &g_allow_ip_count)) != 0)
		{
			return result;
		}

		g_check_active_interval = iniGetIntValue(NULL, \
				"check_active_interval", &iniContext, \
				CHECK_ACTIVE_DEF_INTERVAL);
		if (g_check_active_interval <= 0)
		{
			g_check_active_interval = CHECK_ACTIVE_DEF_INTERVAL;
		}

		g_storage_ip_changed_auto_adjust = iniGetBoolValue(NULL, \
				"storage_ip_changed_auto_adjust", \
				&iniContext, true);

		g_storage_sync_file_max_delay = iniGetIntValue(NULL, \
				"storage_sync_file_max_delay", &iniContext, \
				DEFAULT_STORAGE_SYNC_FILE_MAX_DELAY);
		if (g_storage_sync_file_max_delay <= 0)
		{
			g_storage_sync_file_max_delay = \
					DEFAULT_STORAGE_SYNC_FILE_MAX_DELAY;
		}

		g_storage_sync_file_max_time = iniGetIntValue(NULL, \
				"storage_sync_file_max_time", &iniContext, \
				DEFAULT_STORAGE_SYNC_FILE_MAX_TIME);
		if (g_storage_sync_file_max_time <= 0)
		{
			g_storage_sync_file_max_time = \
				DEFAULT_STORAGE_SYNC_FILE_MAX_TIME;
		}

		g_if_use_trunk_file = iniGetBoolValue(NULL, \
			"use_trunk_file", &iniContext, false);

		pSlotMinSize = iniGetStrValue(NULL, \
			"slot_min_size", &iniContext);
		if (pSlotMinSize == NULL)
		{
			slot_min_size = 256;
		}
		else if ((result=parse_bytes(pSlotMinSize, 1, \
				&slot_min_size)) != 0)
		{
			return result;
		}
		g_slot_min_size = (int)slot_min_size;
		if (g_slot_min_size <= 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"slot_min_size\" %d is invalid, " \
				"which <= 0", __LINE__, g_slot_min_size);
			result = EINVAL;
			break;
		}
		if (g_slot_min_size > 64 * 1024)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"item \"slot_min_size\" %d is too large, " \
				"change to 64KB", __LINE__, g_slot_min_size);
			g_slot_min_size = 64 * 1024;
		}

		pTrunkFileSize = iniGetStrValue(NULL, \
			"trunk_file_size", &iniContext);
		if (pTrunkFileSize == NULL)
		{
			trunk_file_size = 64 * 1024 * 1024;
		}
		else if ((result=parse_bytes(pTrunkFileSize, 1, \
				&trunk_file_size)) != 0)
		{
			return result;
		}
		g_trunk_file_size = (int)trunk_file_size;
		if (g_trunk_file_size < 4 * 1024 * 1024)
		{
			logWarning("file: "__FILE__", line: %d, "
				"item \"trunk_file_size\" %d is too small, "
				"change to 4MB", __LINE__, g_trunk_file_size);
			g_trunk_file_size = 4 * 1024 * 1024;
		}

		pSlotMaxSize = iniGetStrValue(NULL,
			"slot_max_size", &iniContext);
		if (pSlotMaxSize == NULL)
		{
			slot_max_size = g_trunk_file_size / 8;
		}
		else if ((result=parse_bytes(pSlotMaxSize, 1,
				&slot_max_size)) != 0)
		{
			return result;
		}
		g_slot_max_size = (int)slot_max_size;
		if (g_slot_max_size <= g_slot_min_size)
		{
			logError("file: "__FILE__", line: %d, "
				"item \"slot_max_size\" %d is invalid, "
				"which <= slot_min_size: %d",
				__LINE__, g_slot_max_size, g_slot_min_size);
			result = EINVAL;
			break;
		}
		if (g_slot_max_size > g_trunk_file_size / 2)
		{
			logWarning("file: "__FILE__", line: %d, "
				"item \"slot_max_size\": %d is too large, "
				"change to %d", __LINE__, g_slot_max_size,
				g_trunk_file_size / 2);
			g_slot_max_size = g_trunk_file_size / 2;
		}

		g_trunk_create_file_advance = iniGetBoolValue(NULL,
			"trunk_create_file_advance", &iniContext, false);
		if ((result=get_time_item_from_conf(&iniContext,
                        "trunk_create_file_time_base",
			&g_trunk_create_file_time_base, 2, 0)) != 0)
		{
			return result;
		}

		g_trunk_create_file_interval = iniGetIntValue(NULL, \
				"trunk_create_file_interval", &iniContext, \
				86400);
		pSpaceThreshold = iniGetStrValue(NULL, \
			"trunk_create_file_space_threshold", &iniContext);
		if (pSpaceThreshold == NULL)
		{
			g_trunk_create_file_space_threshold = 0;
		}
		else if ((result=parse_bytes(pSpaceThreshold, 1, \
				&g_trunk_create_file_space_threshold)) != 0)
		{
			return result;
		}
		g_trunk_compress_binlog_min_interval = iniGetIntValue(NULL,
				"trunk_compress_binlog_min_interval",
				&iniContext, 0);
		g_trunk_compress_binlog_interval = iniGetIntValue(NULL,
				"trunk_compress_binlog_interval",
                &iniContext, 0);
		if ((result=get_time_item_from_conf(&iniContext,
                	"trunk_compress_binlog_time_base",
                    &g_trunk_compress_binlog_time_base, 3, 0)) != 0)
		{
			return result;
		}

        g_trunk_binlog_max_backups = iniGetIntValue(NULL,
				"trunk_binlog_max_backups", &iniContext, 0);

        g_trunk_alloc_alignment_size = iniGetIntValue(NULL,
				"trunk_alloc_alignment_size", &iniContext, 0);
        if (g_slot_min_size < g_trunk_alloc_alignment_size)
        {
            logWarning("file: "__FILE__", line: %d, "
                    "item \"slot_min_size\": %d < "
                    "\"trunk_alloc_alignment_size\": %d, "
                    "change to %d", __LINE__, g_slot_min_size,
                    g_trunk_alloc_alignment_size,
                    g_trunk_alloc_alignment_size);
            g_slot_min_size = g_trunk_alloc_alignment_size;
        }

		g_trunk_init_check_occupying = iniGetBoolValue(NULL,
			"trunk_init_check_occupying", &iniContext, false);

		g_trunk_init_reload_from_binlog = iniGetBoolValue(NULL,
			"trunk_init_reload_from_binlog", &iniContext, false);

		g_trunk_free_space_merge = iniGetBoolValue(NULL,
			"trunk_free_space_merge", &iniContext, false);

		g_delete_unused_trunk_files = iniGetBoolValue(NULL,
			"delete_unused_trunk_files", &iniContext, false);

		if ((result=tracker_load_storage_id_info(
				filename, &iniContext)) != 0)
		{
			return result;
		}

		g_store_slave_file_use_link = iniGetBoolValue(NULL,
			"store_slave_file_use_link", &iniContext, false);

		if ((result=fdfs_connection_pool_init(filename, &iniContext)) != 0)
		{
			break;
		}

        if (g_if_use_trunk_file && g_groups.store_server ==
                FDFS_STORE_SERVER_ROUND_ROBIN)
        {
            logInfo("file: "__FILE__", line: %d, "
                    "set store_server to %d because use_trunk_file is true",
                    __LINE__, FDFS_STORE_SERVER_FIRST_BY_IP);
            g_groups.store_server = FDFS_STORE_SERVER_FIRST_BY_IP;
        }

        pResponseIpAddrSize = iniGetStrValue(NULL,
                "response_ip_addr_size", &iniContext);
        if (pResponseIpAddrSize == NULL || strcasecmp(
                    pResponseIpAddrSize, "auto") == 0)
        {
            if (g_use_storage_id && fdfs_storage_servers_contain_ipv6()) {
                g_response_ip_addr_size = IPV6_ADDRESS_SIZE;
            } else {
                g_response_ip_addr_size = IPV4_ADDRESS_SIZE;
            }
        } else {
            g_response_ip_addr_size = IPV6_ADDRESS_SIZE;
        }

        sf_global_config_to_string(sz_global_config,
                sizeof(sz_global_config));
        sf_context_config_to_string(&g_sf_context,
            sz_service_config, sizeof(sz_service_config));

		logInfo("FastDFS v%d.%d.%d, %s, %s, "
			"store_lookup=%d, store_group=%s, "
			"store_server=%d, store_path=%d, "
			"reserved_storage_space=%s, "
			"download_server=%d, "
			"allow_ip_count=%d, "
			"check_active_interval=%ds, "
			"storage_ip_changed_auto_adjust=%d, "
			"storage_sync_file_max_delay=%ds, "
			"storage_sync_file_max_time=%ds, "
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
			"use_storage_id=%d, "
			"id_type_in_filename=%s, "
			"trust_storage_server_id=%d, "
			"storage_id/ip_count=%d / %d, "
			"store_slave_file_use_link=%d, "
            "response_ip_addr_size=%s (%d), "
			"use_connection_pool=%d, "
			"g_connection_pool_max_idle_time=%ds",
			g_fdfs_version.major, g_fdfs_version.minor,
            g_fdfs_version.patch, sz_global_config, sz_service_config,
			g_groups.store_lookup, g_groups.store_group,
			g_groups.store_server, g_groups.store_path,
			fdfs_storage_reserved_space_to_string(
			    &g_storage_reserved_space, reserved_space_str),
			g_groups.download_server, g_allow_ip_count,
			g_check_active_interval,
			g_storage_ip_changed_auto_adjust,
			g_storage_sync_file_max_delay,
			g_storage_sync_file_max_time,
			g_if_use_trunk_file, g_slot_min_size,
			g_slot_max_size / 1024,
            g_trunk_alloc_alignment_size,
			g_trunk_file_size / FC_BYTES_ONE_MB,
			g_trunk_create_file_advance,
			g_trunk_create_file_time_base.hour,
			g_trunk_create_file_time_base.minute,
			g_trunk_create_file_interval,
			(int)(g_trunk_create_file_space_threshold /
			(FC_BYTES_ONE_MB * 1024)), g_trunk_init_check_occupying,
			g_trunk_init_reload_from_binlog,
            g_trunk_free_space_merge,
            g_delete_unused_trunk_files,
			g_trunk_compress_binlog_min_interval,
			g_trunk_compress_binlog_interval,
            g_trunk_compress_binlog_time_base.hour,
            g_trunk_compress_binlog_time_base.minute,
            g_trunk_binlog_max_backups,
			g_use_storage_id, g_id_type_in_filename ==
			FDFS_ID_TYPE_SERVER_ID ? "id" : "ip",
            g_trust_storage_server_id,
            g_storage_ids_by_id.count, g_storage_ids_by_ip.count,
			g_store_slave_file_use_link,
            (g_response_ip_addr_size == IPV6_ADDRESS_SIZE ? "IPv6" : "IPv4"),
            g_response_ip_addr_size, g_use_connection_pool,
            g_connection_pool_max_idle_time);
	} while (0);

	iniFreeContext(&iniContext);

	load_local_host_ip_addrs();

	return result;
}
