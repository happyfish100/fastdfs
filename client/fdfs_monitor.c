/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include "sockopt.h"
#include "logger.h"
#include "client_global.h"
#include "fdfs_global.h"
#include "fdfs_client.h"

static ConnectionInfo *pTrackerServer;

static int list_all_groups(const char *group_name);

static void usage(char *argv[])
{
	printf("Usage: %s <config_file> [-h <tracker_server>] [list|delete|set_trunk_server <group_name> " \
		"[storage_id]]\n", argv[0]);
}

int main(int argc, char *argv[])
{
	char *conf_filename;
	int result;
	char *op_type;
	char *tracker_server;
	int arg_index;
	char *group_name;

	if (argc < 2)
	{
		usage(argv);
		return 1;
	}

	tracker_server = NULL;
	conf_filename = argv[1];
	arg_index = 2;

	if (arg_index >= argc)
	{
		op_type = "list";
	}
	else
	{
		int len;

		len = strlen(argv[arg_index]); 
		if (len >= 2 && strncmp(argv[arg_index], "-h", 2) == 0)
		{
			if (len == 2)
			{
				arg_index++;
				if (arg_index >= argc)
				{
					usage(argv);
					return 1;
				}

				tracker_server = argv[arg_index++];
			}
			else
			{
				tracker_server = argv[arg_index] + 2;
				arg_index++;
			}

			if (arg_index < argc)
			{
				op_type = argv[arg_index++];
			}
			else
			{
				op_type = "list";
			}
		}
		else
		{
			op_type = argv[arg_index++];
		}
	}

	log_init();
	g_log_context.log_level = LOG_DEBUG;
	ignore_signal_pipe();

	if ((result=fdfs_client_init(conf_filename)) != 0)
	{
		return result;
	}
	load_log_level_ex(conf_filename);

	if (tracker_server == NULL)
	{
		if (g_tracker_group.server_count > 1)
		{
			srand(time(NULL));
			rand();  //discard the first
			g_tracker_group.server_index = (int)( \
				(g_tracker_group.server_count * (double)rand()) \
				/ (double)RAND_MAX);
		}
	}
	else
	{
		int i;
		char ip_addr[IP_ADDRESS_SIZE];

		*ip_addr = '\0';
		if (getIpaddrByName(tracker_server, ip_addr, sizeof(ip_addr)) \
			 == INADDR_NONE)
		{
			printf("resolve ip address of tracker server: %s " \
				"fail!\n", tracker_server);
			return 2;
		}

		for (i=0; i<g_tracker_group.server_count; i++)
		{
			if (strcmp(g_tracker_group.servers[i].ip_addr, \
					ip_addr) == 0)
			{
				g_tracker_group.server_index = i;
				break;
			}
		}

		if (i == g_tracker_group.server_count)
		{
			printf("tracker server: %s not exists!\n", tracker_server);
			return 2;
		}
	}

	printf("server_count=%d, server_index=%d\n", g_tracker_group.server_count, g_tracker_group.server_index);

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		fdfs_client_destroy();
		return errno != 0 ? errno : ECONNREFUSED;
	}
	printf("\ntracker server is %s:%d\n\n", pTrackerServer->ip_addr, pTrackerServer->port);

	if (arg_index < argc)
	{
		group_name = argv[arg_index++];
	}
	else
	{
		group_name = NULL;
	}

	if (strcmp(op_type, "list") == 0)
	{
		if (group_name == NULL)
		{
			result = list_all_groups(NULL);
		}
		else
		{
			result = list_all_groups(group_name);
		}
	}
	else if (strcmp(op_type, "delete") == 0)
	{
		if (arg_index >= argc)
		{
		if ((result=tracker_delete_group(&g_tracker_group, \
				group_name)) == 0)
		{
			printf("delete group: %s success\n", \
				group_name);
		}
		else
		{
			printf("delete group: %s fail, " \
				"error no: %d, error info: %s\n", \
				group_name, result, STRERROR(result));
		}
		}
        else
        {
		char *storage_id;

		storage_id = argv[arg_index++];
		if ((result=tracker_delete_storage(&g_tracker_group, \
				group_name, storage_id)) == 0)
		{
			printf("delete storage server %s::%s success\n", \
				group_name, storage_id);
		}
		else
		{
			printf("delete storage server %s::%s fail, " \
				"error no: %d, error info: %s\n", \
				group_name, storage_id, \
				result, STRERROR(result));
		}
        }
	}
	else if (strcmp(op_type, "set_trunk_server") == 0)
	{
		char *storage_id;
		char new_trunk_server_id[FDFS_STORAGE_ID_MAX_SIZE];

		if (group_name == NULL)
		{
			usage(argv);
			return 1;
		}
		if (arg_index >= argc)
		{
			storage_id = "";
		}
		else
		{
			storage_id = argv[arg_index++];
		}

		if ((result=tracker_set_trunk_server(&g_tracker_group, \
			group_name, storage_id, new_trunk_server_id)) == 0)
		{
			printf("set trunk server %s::%s success, " \
				"new trunk server: %s\n", group_name, \
				storage_id, new_trunk_server_id);
		}
		else
		{
			printf("set trunk server %s::%s fail, " \
				"error no: %d, error info: %s\n", \
				group_name, storage_id, \
				result, STRERROR(result));
		}
	}
	else
	{
		printf("Invalid command %s\n\n", op_type);
		usage(argv);
	}

	tracker_disconnect_server_ex(pTrackerServer, true);
	fdfs_client_destroy();
	return 0;
}

static int list_storages(FDFSGroupStat *pGroupStat)
{
	int result;
	int storage_count;
	FDFSStorageInfo storage_infos[FDFS_MAX_SERVERS_EACH_GROUP];
	FDFSStorageInfo *p;
	FDFSStorageInfo *pStorage;
	FDFSStorageInfo *pStorageEnd;
	FDFSStorageStat *pStorageStat;
	char szJoinTime[32];
	char szUpTime[32];
	char szLastHeartBeatTime[32];
	char szSrcUpdTime[32];
	char szSyncUpdTime[32];
	char szSyncedTimestamp[32];
	char szSyncedDelaySeconds[128];
	char szHostname[128];
	char szHostnamePrompt[128+8];
	int k;
	int max_last_source_update;

	printf( "group name = %s\n" \
		"disk total space = %"PRId64" MB\n" \
		"disk free space = %"PRId64" MB\n" \
		"trunk free space = %"PRId64" MB\n" \
		"storage server count = %d\n" \
		"active server count = %d\n" \
		"storage server port = %d\n" \
		"storage HTTP port = %d\n" \
		"store path count = %d\n" \
		"subdir count per path = %d\n" \
		"current write server index = %d\n" \
		"current trunk file id = %d\n\n", \
		pGroupStat->group_name, \
		pGroupStat->total_mb, \
		pGroupStat->free_mb, \
		pGroupStat->trunk_free_mb, \
		pGroupStat->count, \
		pGroupStat->active_count, \
		pGroupStat->storage_port, \
		pGroupStat->storage_http_port, \
		pGroupStat->store_path_count, \
		pGroupStat->subdir_count_per_path, \
		pGroupStat->current_write_server, \
		pGroupStat->current_trunk_file_id
	);

	result = tracker_list_servers(pTrackerServer, \
		pGroupStat->group_name, NULL, \
		storage_infos, FDFS_MAX_SERVERS_EACH_GROUP, \
		&storage_count);
	if (result != 0)
	{
		return result;
	}

	k = 0;
	pStorageEnd = storage_infos + storage_count;
	for (pStorage=storage_infos; pStorage<pStorageEnd; \
		pStorage++)
	{
		max_last_source_update = 0;
		for (p=storage_infos; p<pStorageEnd; p++)
		{
			if (p != pStorage && p->stat.last_source_update
				> max_last_source_update)
			{
				max_last_source_update = \
					p->stat.last_source_update;
			}
		}

		pStorageStat = &(pStorage->stat);
		if (max_last_source_update == 0)
		{
			*szSyncedDelaySeconds = '\0';
		}
		else
		{
			if (pStorageStat->last_synced_timestamp == 0)
			{
				strcpy(szSyncedDelaySeconds, "(never synced)");
			}
			else
			{
			int delay_seconds;
			int remain_seconds;
			int day;
			int hour;
			int minute;
			int second;
			char szDelayTime[64];
			
			delay_seconds = (int)(max_last_source_update - \
				pStorageStat->last_synced_timestamp);
			day = delay_seconds / (24 * 3600);
			remain_seconds = delay_seconds % (24 * 3600);
			hour = remain_seconds / 3600;
			remain_seconds %= 3600;
			minute = remain_seconds / 60;
			second = remain_seconds % 60;

			if (day != 0)
			{
				sprintf(szDelayTime, "%d days " \
					"%02dh:%02dm:%02ds", \
					day, hour, minute, second);
			}
			else if (hour != 0)
			{
				sprintf(szDelayTime, "%02dh:%02dm:%02ds", \
					hour, minute, second);
			}
			else if (minute != 0)
			{
				sprintf(szDelayTime, "%02dm:%02ds", minute, second);
			}
			else
			{
				sprintf(szDelayTime, "%ds", second);
			}

			sprintf(szSyncedDelaySeconds, "(%s delay)", szDelayTime);
			}
		}

		getHostnameByIp(pStorage->ip_addr, szHostname, sizeof(szHostname));
		if (*szHostname != '\0')
		{
			sprintf(szHostnamePrompt, " (%s)", szHostname);
		}
		else
		{
			*szHostnamePrompt = '\0';
		}

		if (pStorage->up_time != 0)
		{
			formatDatetime(pStorage->up_time, \
				"%Y-%m-%d %H:%M:%S", \
				szUpTime, sizeof(szUpTime));
		}
		else
		{
			*szUpTime = '\0';
		}

		printf( "\tStorage %d:\n" \
			"\t\tid = %s\n" \
			"\t\tip_addr = %s%s  %s\n" \
			"\t\thttp domain = %s\n" \
			"\t\tversion = %s\n" \
			"\t\tjoin time = %s\n" \
			"\t\tup time = %s\n" \
			"\t\ttotal storage = %d MB\n" \
			"\t\tfree storage = %d MB\n" \
			"\t\tupload priority = %d\n" \
			"\t\tstore_path_count = %d\n" \
			"\t\tsubdir_count_per_path = %d\n" \
			"\t\tstorage_port = %d\n" \
			"\t\tstorage_http_port = %d\n" \
			"\t\tcurrent_write_path = %d\n" \
			"\t\tsource storage id = %s\n" \
			"\t\tif_trunk_server = %d\n" \
			"\t\tconnection.alloc_count = %d\n"   \
			"\t\tconnection.current_count = %d\n"   \
			"\t\tconnection.max_count = %d\n"   \
			"\t\ttotal_upload_count = %"PRId64"\n"   \
			"\t\tsuccess_upload_count = %"PRId64"\n" \
			"\t\ttotal_append_count = %"PRId64"\n"   \
			"\t\tsuccess_append_count = %"PRId64"\n" \
			"\t\ttotal_modify_count = %"PRId64"\n"   \
			"\t\tsuccess_modify_count = %"PRId64"\n" \
			"\t\ttotal_truncate_count = %"PRId64"\n"   \
			"\t\tsuccess_truncate_count = %"PRId64"\n" \
			"\t\ttotal_set_meta_count = %"PRId64"\n" \
			"\t\tsuccess_set_meta_count = %"PRId64"\n" \
			"\t\ttotal_delete_count = %"PRId64"\n" \
			"\t\tsuccess_delete_count = %"PRId64"\n" \
			"\t\ttotal_download_count = %"PRId64"\n" \
			"\t\tsuccess_download_count = %"PRId64"\n" \
			"\t\ttotal_get_meta_count = %"PRId64"\n" \
			"\t\tsuccess_get_meta_count = %"PRId64"\n" \
			"\t\ttotal_create_link_count = %"PRId64"\n" \
			"\t\tsuccess_create_link_count = %"PRId64"\n"\
			"\t\ttotal_delete_link_count = %"PRId64"\n" \
			"\t\tsuccess_delete_link_count = %"PRId64"\n" \
			"\t\ttotal_upload_bytes = %"PRId64"\n" \
			"\t\tsuccess_upload_bytes = %"PRId64"\n" \
			"\t\ttotal_append_bytes = %"PRId64"\n" \
			"\t\tsuccess_append_bytes = %"PRId64"\n" \
			"\t\ttotal_modify_bytes = %"PRId64"\n" \
			"\t\tsuccess_modify_bytes = %"PRId64"\n" \
			"\t\tstotal_download_bytes = %"PRId64"\n" \
			"\t\tsuccess_download_bytes = %"PRId64"\n" \
			"\t\ttotal_sync_in_bytes = %"PRId64"\n" \
			"\t\tsuccess_sync_in_bytes = %"PRId64"\n" \
			"\t\ttotal_sync_out_bytes = %"PRId64"\n" \
			"\t\tsuccess_sync_out_bytes = %"PRId64"\n" \
			"\t\ttotal_file_open_count = %"PRId64"\n" \
			"\t\tsuccess_file_open_count = %"PRId64"\n" \
			"\t\ttotal_file_read_count = %"PRId64"\n" \
			"\t\tsuccess_file_read_count = %"PRId64"\n" \
			"\t\ttotal_file_write_count = %"PRId64"\n" \
			"\t\tsuccess_file_write_count = %"PRId64"\n" \
			"\t\tlast_heart_beat_time = %s\n" \
			"\t\tlast_source_update = %s\n" \
			"\t\tlast_sync_update = %s\n"   \
			"\t\tlast_synced_timestamp = %s %s\n",  \
			++k, pStorage->id, pStorage->ip_addr, \
			szHostnamePrompt, get_storage_status_caption( \
			    pStorage->status), pStorage->domain_name, \
			pStorage->version,  \
			formatDatetime(pStorage->join_time, \
				"%Y-%m-%d %H:%M:%S", \
				szJoinTime, sizeof(szJoinTime)), \
			szUpTime, pStorage->total_mb, \
			pStorage->free_mb,  \
			pStorage->upload_priority,  \
			pStorage->store_path_count,  \
			pStorage->subdir_count_per_path,  \
			pStorage->storage_port,  \
			pStorage->storage_http_port,  \
			pStorage->current_write_path,  \
			pStorage->src_id,  \
			pStorage->if_trunk_server,  \
			pStorageStat->connection.alloc_count, \
			pStorageStat->connection.current_count, \
			pStorageStat->connection.max_count, \
			pStorageStat->total_upload_count, \
			pStorageStat->success_upload_count, \
			pStorageStat->total_append_count, \
			pStorageStat->success_append_count, \
			pStorageStat->total_modify_count, \
			pStorageStat->success_modify_count, \
			pStorageStat->total_truncate_count, \
			pStorageStat->success_truncate_count, \
			pStorageStat->total_set_meta_count, \
			pStorageStat->success_set_meta_count, \
			pStorageStat->total_delete_count, \
			pStorageStat->success_delete_count, \
			pStorageStat->total_download_count, \
			pStorageStat->success_download_count, \
			pStorageStat->total_get_meta_count, \
			pStorageStat->success_get_meta_count, \
			pStorageStat->total_create_link_count, \
			pStorageStat->success_create_link_count, \
			pStorageStat->total_delete_link_count, \
			pStorageStat->success_delete_link_count, \
			pStorageStat->total_upload_bytes, \
			pStorageStat->success_upload_bytes, \
			pStorageStat->total_append_bytes, \
			pStorageStat->success_append_bytes, \
			pStorageStat->total_modify_bytes, \
			pStorageStat->success_modify_bytes, \
			pStorageStat->total_download_bytes, \
			pStorageStat->success_download_bytes, \
			pStorageStat->total_sync_in_bytes, \
			pStorageStat->success_sync_in_bytes, \
			pStorageStat->total_sync_out_bytes, \
			pStorageStat->success_sync_out_bytes, \
			pStorageStat->total_file_open_count, \
			pStorageStat->success_file_open_count, \
			pStorageStat->total_file_read_count, \
			pStorageStat->success_file_read_count, \
			pStorageStat->total_file_write_count, \
			pStorageStat->success_file_write_count, \
			formatDatetime(pStorageStat->last_heart_beat_time, \
				"%Y-%m-%d %H:%M:%S", \
				szLastHeartBeatTime, sizeof(szLastHeartBeatTime)), \
			formatDatetime(pStorageStat->last_source_update, \
				"%Y-%m-%d %H:%M:%S", \
				szSrcUpdTime, sizeof(szSrcUpdTime)), \
			formatDatetime(pStorageStat->last_sync_update, \
				"%Y-%m-%d %H:%M:%S", \
				szSyncUpdTime, sizeof(szSyncUpdTime)), \
			formatDatetime(pStorageStat->last_synced_timestamp, \
				"%Y-%m-%d %H:%M:%S", \
				szSyncedTimestamp, sizeof(szSyncedTimestamp)),\
			szSyncedDelaySeconds);
	}

	return 0;
}

static int list_all_groups(const char *group_name)
{
	int result;
	int group_count;
	FDFSGroupStat group_stats[FDFS_MAX_GROUPS];
	FDFSGroupStat *pGroupStat;
	FDFSGroupStat *pGroupEnd;
	int i;

	result = tracker_list_groups(pTrackerServer, \
		group_stats, FDFS_MAX_GROUPS, \
		&group_count);
	if (result != 0)
	{
		tracker_close_all_connections();
		fdfs_client_destroy();
		return result;
	}

	pGroupEnd = group_stats + group_count;
	if (group_name == NULL)
	{
		printf("group count: %d\n", group_count);
		i = 0;
		for (pGroupStat=group_stats; pGroupStat<pGroupEnd; \
			pGroupStat++)
		{
			printf( "\nGroup %d:\n", ++i);
			list_storages(pGroupStat);
		}
	}
	else
	{
		for (pGroupStat=group_stats; pGroupStat<pGroupEnd; \
			pGroupStat++)
		{
			if (strcmp(pGroupStat->group_name, group_name) == 0)
			{
				list_storages(pGroupStat);
				break;
			}
		}
	}

	return 0;
}

