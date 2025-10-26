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
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fastcommon/shared_func.h"
#include "fdfs_global.h"
#include "fastcommon/sockopt.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client.h"
#include "client_global.h"

int tracker_get_all_connections_ex(TrackerServerGroup *pTrackerGroup)
{
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
    ConnectionInfo *conn;
    int result;
	int success_count;

	success_count = 0;
	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
		if ((conn=tracker_connect_server_no_pool(pServer, &result)) != NULL)
		{
			fdfs_active_test(conn);
			success_count++;
		}
	}

	return success_count > 0 ? 0 : ENOTCONN;
}

void tracker_close_all_connections_ex(TrackerServerGroup *pTrackerGroup)
{
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;

	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
		tracker_disconnect_server_no_pool(pServer);
	}
}

ConnectionInfo *tracker_get_connection_ex(TrackerServerGroup *pTrackerGroup)
{
	ConnectionInfo *conn;
	TrackerServerInfo *pCurrentServer;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
	int server_index;
	int result;

	server_index = pTrackerGroup->server_index;
	if (server_index >= pTrackerGroup->server_count)
	{
		server_index = 0;
	}

	do
	{
	pCurrentServer = pTrackerGroup->servers + server_index;
	if ((conn=tracker_connect_server(pCurrentServer, &result)) != NULL)
	{
		break;
	}

	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pCurrentServer+1; pServer<pEnd; pServer++)
	{
		if ((conn=tracker_connect_server(pServer, &result)) != NULL)
		{
			pTrackerGroup->server_index = pServer -
							pTrackerGroup->servers;
			break;
		}
	}

	if (conn != NULL)
	{
		break;
	}

	for (pServer=pTrackerGroup->servers; pServer<pCurrentServer; pServer++)
	{
		if ((conn=tracker_connect_server(pServer, &result)) != NULL)
		{
			pTrackerGroup->server_index = pServer -
							pTrackerGroup->servers;
			break;
		}
	}
	} while (0);

	pTrackerGroup->server_index++;
	if (pTrackerGroup->server_index >= pTrackerGroup->server_count)
	{
		pTrackerGroup->server_index = 0;
	}

	return conn;
}

ConnectionInfo *tracker_get_connection_no_pool(TrackerServerGroup *pTrackerGroup)
{
	TrackerServerInfo *pCurrentServer;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
	ConnectionInfo *conn;
	int server_index;
	int result;

	server_index = pTrackerGroup->server_index;
	if (server_index >= pTrackerGroup->server_count)
	{
		server_index = 0;
	}

	conn = NULL;
	do
	{
	pCurrentServer = pTrackerGroup->servers + server_index;
	if ((conn=tracker_connect_server_no_pool(pCurrentServer, &result)) != NULL)
	{
		break;
	}

	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pCurrentServer+1; pServer<pEnd; pServer++)
	{
		if ((conn=tracker_connect_server_no_pool(pServer, &result)) != NULL)
		{
			pTrackerGroup->server_index = pServer - pTrackerGroup->servers;
			break;
		}
	}

	if (conn != NULL)
	{
		break;
	}

	for (pServer=pTrackerGroup->servers; pServer<pCurrentServer; pServer++)
	{
		if ((conn=tracker_connect_server_no_pool(pServer, &result)) != NULL)
		{
			pTrackerGroup->server_index = pServer - pTrackerGroup->servers;
			break;
		}
	}
	} while (0);

	pTrackerGroup->server_index++;
	if (pTrackerGroup->server_index >= pTrackerGroup->server_count)
	{
		pTrackerGroup->server_index = 0;
	}

	return conn;
}

ConnectionInfo *tracker_get_connection_r_ex(TrackerServerGroup *pTrackerGroup,
		TrackerServerInfo *pTrackerServer, int *err_no)
{
	ConnectionInfo *conn;
	TrackerServerInfo *pCurrentServer;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
	int server_index;

	server_index = pTrackerGroup->server_index;
	if (server_index >= pTrackerGroup->server_count)
	{
		server_index = 0;
	}

	do
	{
	pCurrentServer = pTrackerGroup->servers + server_index;
	memcpy(pTrackerServer, pCurrentServer, sizeof(TrackerServerInfo));
    fdfs_server_sock_reset(pTrackerServer);
	if ((conn=tracker_connect_server(pTrackerServer, err_no)) != NULL)
	{
		break;
	}

	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pCurrentServer+1; pServer<pEnd; pServer++)
	{
		memcpy(pTrackerServer, pServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(pTrackerServer);
		if ((conn=tracker_connect_server(pTrackerServer, err_no)) != NULL)
		{
			pTrackerGroup->server_index = pServer -
							pTrackerGroup->servers;
			break;
		}
	}

	if (conn != NULL)
	{
		break;
	}

	for (pServer=pTrackerGroup->servers; pServer<pCurrentServer; pServer++)
	{
		memcpy(pTrackerServer, pServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(pTrackerServer);
		if ((conn=tracker_connect_server(pTrackerServer, err_no)) != NULL)
		{
			pTrackerGroup->server_index = pServer -
							pTrackerGroup->servers;
			break;
		}
	}
	} while (0);

	pTrackerGroup->server_index++;
	if (pTrackerGroup->server_index >= pTrackerGroup->server_count)
	{
		pTrackerGroup->server_index = 0;
	}

	return conn;
}

static void decode_storage_stat(FDFSStorageStatBuff *pStatBuff,
        FDFSStorageStat *pStorageStat)
{
    pStorageStat->connection.alloc_count = buff2int(
            pStatBuff->connection.sz_alloc_count);
    pStorageStat->connection.current_count = buff2int(
            pStatBuff->connection.sz_current_count);
    pStorageStat->connection.max_count = buff2int(
            pStatBuff->connection.sz_max_count);

    pStorageStat->total_upload_count = buff2long(
            pStatBuff->sz_total_upload_count);
    pStorageStat->success_upload_count = buff2long(
            pStatBuff->sz_success_upload_count);
    pStorageStat->total_append_count = buff2long(
            pStatBuff->sz_total_append_count);
    pStorageStat->success_append_count = buff2long(
            pStatBuff->sz_success_append_count);
    pStorageStat->total_modify_count = buff2long(
            pStatBuff->sz_total_modify_count);
    pStorageStat->success_modify_count = buff2long(
            pStatBuff->sz_success_modify_count);
    pStorageStat->total_truncate_count = buff2long(
            pStatBuff->sz_total_truncate_count);
    pStorageStat->success_truncate_count = buff2long(
            pStatBuff->sz_success_truncate_count);
    pStorageStat->total_set_meta_count = buff2long(
            pStatBuff->sz_total_set_meta_count);
    pStorageStat->success_set_meta_count = buff2long(
            pStatBuff->sz_success_set_meta_count);
    pStorageStat->total_delete_count = buff2long(
            pStatBuff->sz_total_delete_count);
    pStorageStat->success_delete_count = buff2long(
            pStatBuff->sz_success_delete_count);
    pStorageStat->total_download_count = buff2long(
            pStatBuff->sz_total_download_count);
    pStorageStat->success_download_count = buff2long(
            pStatBuff->sz_success_download_count);
    pStorageStat->total_get_meta_count = buff2long(
            pStatBuff->sz_total_get_meta_count);
    pStorageStat->success_get_meta_count = buff2long(
            pStatBuff->sz_success_get_meta_count);
    pStorageStat->last_source_update = buff2long(
            pStatBuff->sz_last_source_update);
    pStorageStat->last_sync_update = buff2long(
            pStatBuff->sz_last_sync_update);
    pStorageStat->last_synced_timestamp = buff2long(
            pStatBuff->sz_last_synced_timestamp);
    pStorageStat->total_create_link_count = buff2long(
            pStatBuff->sz_total_create_link_count);
    pStorageStat->success_create_link_count = buff2long(
            pStatBuff->sz_success_create_link_count);
    pStorageStat->total_delete_link_count = buff2long(
            pStatBuff->sz_total_delete_link_count);
    pStorageStat->success_delete_link_count = buff2long(
            pStatBuff->sz_success_delete_link_count);
    pStorageStat->total_upload_bytes = buff2long(
            pStatBuff->sz_total_upload_bytes);
    pStorageStat->success_upload_bytes = buff2long(
            pStatBuff->sz_success_upload_bytes);
    pStorageStat->total_append_bytes = buff2long(
            pStatBuff->sz_total_append_bytes);
    pStorageStat->success_append_bytes = buff2long(
            pStatBuff->sz_success_append_bytes);
    pStorageStat->total_modify_bytes = buff2long(
            pStatBuff->sz_total_modify_bytes);
    pStorageStat->success_modify_bytes = buff2long(
            pStatBuff->sz_success_modify_bytes);
    pStorageStat->total_download_bytes = buff2long(
            pStatBuff->sz_total_download_bytes);
    pStorageStat->success_download_bytes = buff2long(
            pStatBuff->sz_success_download_bytes);
    pStorageStat->total_sync_in_bytes = buff2long(
            pStatBuff->sz_total_sync_in_bytes);
    pStorageStat->success_sync_in_bytes = buff2long(
            pStatBuff->sz_success_sync_in_bytes);
    pStorageStat->total_sync_out_bytes = buff2long(
            pStatBuff->sz_total_sync_out_bytes);
    pStorageStat->success_sync_out_bytes = buff2long(
            pStatBuff->sz_success_sync_out_bytes);
    pStorageStat->total_file_open_count = buff2long(
            pStatBuff->sz_total_file_open_count);
    pStorageStat->success_file_open_count = buff2long(
            pStatBuff->sz_success_file_open_count);
    pStorageStat->total_file_read_count = buff2long(
            pStatBuff->sz_total_file_read_count);
    pStorageStat->success_file_read_count = buff2long(
            pStatBuff->sz_success_file_read_count);
    pStorageStat->total_file_write_count = buff2long(
            pStatBuff->sz_total_file_write_count);
    pStorageStat->success_file_write_count = buff2long(
            pStatBuff->sz_success_file_write_count);
    pStorageStat->last_heart_beat_time = buff2long(
            pStatBuff->sz_last_heart_beat_time);
}

#define PARSE_STORAGE_FIELDS(pSrc, pDest, ip_size)  \
    pDest->status = pSrc->status;   \
    pDest->rw_mode = pSrc->rw_mode; \
    memcpy(pDest->id, pSrc->id, FDFS_STORAGE_ID_MAX_SIZE - 1); \
    memcpy(pDest->ip_addr, pSrc->ip_addr, ip_size - 1); \
    memcpy(pDest->src_id, pSrc->src_id,    \
            FDFS_STORAGE_ID_MAX_SIZE - 1); \
    strcpy(pDest->version, pSrc->version); \
    pDest->join_time = buff2long(pSrc->sz_join_time); \
    pDest->up_time = buff2long(pSrc->sz_up_time);     \
    pDest->total_mb = buff2long(pSrc->sz_total_mb);   \
    pDest->free_mb = buff2long(pSrc->sz_free_mb);     \
    pDest->reserved_mb = buff2long(pSrc->sz_reserved_mb); \
    pDest->upload_priority = buff2long(pSrc->sz_upload_priority);   \
    pDest->store_path_count = buff2long(pSrc->sz_store_path_count); \
    pDest->subdir_count_per_path = buff2long( \
            pSrc->sz_subdir_count_per_path);  \
    pDest->storage_port = buff2long(pSrc->sz_storage_port); \
    pDest->current_write_path = buff2long( \
            pSrc->sz_current_write_path);  \
    pDest->if_trunk_server = pSrc->if_trunk_server

int tracker_list_servers(ConnectionInfo *pTrackerServer,
		const char *szGroupName, const char *szStorageId,
		FDFSStorageInfo *storage_infos, const int max_storages,
		int *storage_count)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN +
			IPV6_ADDRESS_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	bool new_connection;
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	int result;
    int struct_size;
	int name_len;
	int id_len;
	char in_buff[sizeof(TrackerStorageStatIPv6) * FDFS_MAX_SERVERS_EACH_GROUP];
	char *pInBuff;
	TrackerStorageStatIPv4 *pIPv4Src;
	TrackerStorageStatIPv4 *pIPv4End;
	TrackerStorageStatIPv6 *pIPv6Src;
	TrackerStorageStatIPv6 *pIPv6End;
	FDFSStorageInfo *pDest;
	int64_t in_bytes;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	name_len = strlen(szGroupName);
	if (name_len > FDFS_GROUP_NAME_MAX_LEN)
	{
		name_len = FDFS_GROUP_NAME_MAX_LEN;
	}
	memcpy(out_buff + sizeof(TrackerHeader), szGroupName, name_len);

	if (szStorageId == NULL)
	{
		id_len = 0;
	}
	else
	{
		id_len = strlen(szStorageId);
		if (id_len >= FDFS_STORAGE_ID_MAX_SIZE)
		{
			id_len = FDFS_STORAGE_ID_MAX_SIZE - 1;
		}

		memcpy(out_buff+sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN,
			szStorageId, id_len);
	}

	long2buff(FDFS_GROUP_NAME_MAX_LEN + id_len, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_SERVER_LIST_STORAGE;
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
		sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + id_len,
		SF_G_NETWORK_TIMEOUT)) != 0)
    {
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
        logError("file: "__FILE__", line: %d, "
                "send data to tracker server %s:%u fail, errno: %d, "
                "error info: %s", __LINE__, formatted_ip,
                pTrackerServer->port, result, STRERROR(result));
    }
	else
	{
		pInBuff = in_buff;
		result = fdfs_recv_response(conn, &pInBuff,
					sizeof(in_buff), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		*storage_count = 0;
		return result;
	}

	if (in_bytes % sizeof(TrackerStorageStatIPv4) == 0)
    {
        struct_size = sizeof(TrackerStorageStatIPv4);
    }
    else if (in_bytes % sizeof(TrackerStorageStatIPv6) == 0)
    {
        struct_size = sizeof(TrackerStorageStatIPv6);
    }
    else
    {
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data length: %"PRId64
			" is invalid", __LINE__, formatted_ip,
			pTrackerServer->port, in_bytes);
		*storage_count = 0;
		return EINVAL;
	}

	*storage_count = in_bytes / struct_size;
	if (*storage_count > max_storages)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
		 	"tracker server %s:%u insufficent space, "
			"max storage count: %d, expect count: %d",
			__LINE__, formatted_ip, pTrackerServer->port,
            max_storages, *storage_count);
		*storage_count = 0;
		return ENOSPC;
	}

	memset(storage_infos, 0, sizeof(FDFSStorageInfo) * max_storages);
	pDest = storage_infos;
    if (struct_size == sizeof(TrackerStorageStatIPv4))
    {
        pIPv4Src = (TrackerStorageStatIPv4 *)in_buff;
        pIPv4End = pIPv4Src + (*storage_count);
        for (; pIPv4Src<pIPv4End; pIPv4Src++, pDest++)
        {
            PARSE_STORAGE_FIELDS(pIPv4Src, pDest, IPV4_ADDRESS_SIZE);
            decode_storage_stat(&pIPv4Src->stat_buff, &pDest->stat);
        }
    }
    else
    {
        pIPv6Src = (TrackerStorageStatIPv6 *)in_buff;
        pIPv6End = pIPv6Src + (*storage_count);
        for (; pIPv6Src<pIPv6End; pIPv6Src++, pDest++)
        {
            PARSE_STORAGE_FIELDS(pIPv6Src, pDest, IPV6_ADDRESS_SIZE);
            decode_storage_stat(&pIPv6Src->stat_buff, &pDest->stat);
        }
    }

	return 0;
}

int tracker_list_one_group(ConnectionInfo *pTrackerServer, \
		const char *group_name, FDFSGroupStat *pDest)
{
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	bool new_connection;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN];
    char formatted_ip[FORMATTED_IP_SIZE];
	TrackerGroupStat src;
	char *pInBuff;
	int result;
	int64_t in_bytes;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
    fdfs_pack_group_name(group_name, out_buff + sizeof(TrackerHeader));
	pHeader->cmd = TRACKER_PROTO_CMD_SERVER_LIST_ONE_GROUP;
	long2buff(FDFS_GROUP_NAME_MAX_LEN, pHeader->pkg_len);
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
			sizeof(out_buff), SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = (char *)&src;
		result = fdfs_recv_response(conn, \
			&pInBuff, sizeof(TrackerGroupStat), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}

	if (in_bytes != sizeof(TrackerGroupStat))
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data length: %"PRId64" "
            "is invalid", __LINE__, formatted_ip,
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	memset(pDest, 0, sizeof(FDFSGroupStat));
	memcpy(pDest->group_name, src.group_name, FDFS_GROUP_NAME_MAX_LEN);
	pDest->total_mb = buff2long(src.sz_total_mb);
	pDest->free_mb = buff2long(src.sz_free_mb);
    pDest->reserved_mb = buff2long(src.sz_reserved_mb);
	pDest->trunk_free_mb = buff2long(src.sz_trunk_free_mb);
	pDest->storage_count = buff2long(src.sz_storage_count);
	pDest->storage_port = buff2long(src.sz_storage_port);
	pDest->readable_server_count = buff2long(src.sz_readable_server_count);
	pDest->writable_server_count = buff2long(src.sz_writable_server_count);
	pDest->current_write_server = buff2long(src.sz_current_write_server);
	pDest->store_path_count = buff2long(src.sz_store_path_count);
	pDest->subdir_count_per_path = buff2long(src.sz_subdir_count_per_path);
	pDest->current_trunk_file_id = buff2long(src.sz_current_trunk_file_id);

	return 0;
}

int tracker_list_groups(ConnectionInfo *pTrackerServer,
		FDFSGroupStat *group_stats, const int max_groups,
		int *group_count)
{
	bool new_connection;
	TrackerHeader header;
	TrackerGroupStat stats[FDFS_MAX_GROUPS];
	char *pInBuff;
	ConnectionInfo *conn;
	TrackerGroupStat *pSrc;
	TrackerGroupStat *pEnd;
	FDFSGroupStat *pDest;
    char formatted_ip[FORMATTED_IP_SIZE];
	int result;
	int64_t in_bytes;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	memset(&header, 0, sizeof(header));
	header.cmd = TRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS;
	header.status = 0;
	if ((result=tcpsenddata_nb(conn->sock, &header,
			sizeof(header), SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = (char *)stats;
		result = fdfs_recv_response(conn, \
			&pInBuff, sizeof(stats), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		*group_count = 0;
		return result;
	}

	if (in_bytes % sizeof(TrackerGroupStat) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data length: %"PRId64" "
            "is invalid", __LINE__, formatted_ip,
			pTrackerServer->port, in_bytes);
		*group_count = 0;
		return EINVAL;
	}

	*group_count = in_bytes / sizeof(TrackerGroupStat);
	if (*group_count > max_groups)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u insufficent space, "
			"max group count: %d, expect count: %d",
			__LINE__, formatted_ip, pTrackerServer->port,
            max_groups, *group_count);
		*group_count = 0;
		return ENOSPC;
	}

	memset(group_stats, 0, sizeof(FDFSGroupStat) * max_groups);
	pDest = group_stats;
	pEnd = stats + (*group_count);
	for (pSrc=stats; pSrc<pEnd; pSrc++)
	{
		memcpy(pDest->group_name, pSrc->group_name, \
				FDFS_GROUP_NAME_MAX_LEN);
		pDest->total_mb = buff2long(pSrc->sz_total_mb);
		pDest->free_mb = buff2long(pSrc->sz_free_mb);
		pDest->reserved_mb = buff2long(pSrc->sz_reserved_mb);
		pDest->trunk_free_mb = buff2long(pSrc->sz_trunk_free_mb);
		pDest->storage_count = buff2long(pSrc->sz_storage_count);
		pDest->storage_port = buff2long(pSrc->sz_storage_port);
		pDest->readable_server_count = buff2long(
                pSrc->sz_readable_server_count);
		pDest->writable_server_count = buff2long(
                pSrc->sz_writable_server_count);
		pDest->current_write_server = buff2long( \
				pSrc->sz_current_write_server);
		pDest->store_path_count = buff2long( \
				pSrc->sz_store_path_count);
		pDest->subdir_count_per_path = buff2long( \
				pSrc->sz_subdir_count_per_path);
		pDest->current_trunk_file_id = buff2long( \
				pSrc->sz_current_trunk_file_id);

		pDest++;
	}

	return 0;
}

int tracker_do_query_storage(ConnectionInfo *pTrackerServer,
		ConnectionInfo *pStorageServer, const byte cmd,
		const char *group_name, const char *filename)
{
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	bool new_connection;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + 128];
	char in_buff[sizeof(TrackerHeader) +
        TRACKER_QUERY_STORAGE_FETCH_IPV6_BODY_LEN];
    char formatted_ip[FORMATTED_IP_SIZE];
	char *pInBuff;
	int64_t in_bytes;
	int body_len;
    int ip_size;
	int result;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	memset(pStorageServer, 0, sizeof(ConnectionInfo));
	pStorageServer->sock = -1;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
    body_len = fdfs_pack_group_name_and_filename(group_name, filename,
            out_buff + sizeof(TrackerHeader), sizeof(out_buff) -
            sizeof(TrackerHeader));
	long2buff(body_len, pHeader->pkg_len);
	pHeader->cmd = cmd;
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
                    sizeof(TrackerHeader) + body_len,
                    SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = in_buff;
		result = fdfs_recv_response(conn, \
			&pInBuff, sizeof(in_buff), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}

	if (in_bytes == TRACKER_QUERY_STORAGE_FETCH_IPV4_BODY_LEN)
	{
        ip_size = IPV4_ADDRESS_SIZE;
    }
    else if (in_bytes == TRACKER_QUERY_STORAGE_FETCH_IPV6_BODY_LEN)
    {
        ip_size = IPV6_ADDRESS_SIZE;
    }
    else
    {
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data length: %"PRId64" "
            "is invalid, expect length: %d or %d", __LINE__,
            formatted_ip, pTrackerServer->port, in_bytes,
			TRACKER_QUERY_STORAGE_FETCH_IPV4_BODY_LEN,
			TRACKER_QUERY_STORAGE_FETCH_IPV6_BODY_LEN);
		return EINVAL;
	}

	memcpy(pStorageServer->ip_addr, in_buff +
			FDFS_GROUP_NAME_MAX_LEN, ip_size - 1);
	pStorageServer->port = (int)buff2long(in_buff +
			FDFS_GROUP_NAME_MAX_LEN + ip_size - 1);
	return 0;
}

int tracker_query_storage_list(ConnectionInfo *pTrackerServer,
		ConnectionInfo *pStorageServer, const int nMaxServerCount,
		int *server_count, char *group_name, const char *filename)
{
	TrackerHeader *pHeader;
	ConnectionInfo *pServer;
	ConnectionInfo *pServerEnd;
	ConnectionInfo *conn;
	bool new_connection;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + 128];
	char in_buff[sizeof(TrackerHeader) +
		TRACKER_QUERY_STORAGE_FETCH_IPV6_BODY_LEN +
		FDFS_MAX_SERVERS_EACH_GROUP * IPV6_ADDRESS_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	char *pInBuff;
	int64_t in_bytes;
	int body_len;
    int ip_list_len;
    int ip_size;
	int result;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
    body_len = fdfs_pack_group_name_and_filename(group_name, filename,
            out_buff + sizeof(TrackerHeader), sizeof(out_buff) -
            sizeof(TrackerHeader));
	long2buff(body_len, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ALL;
    if ((result=tcpsenddata_nb(conn->sock, out_buff,
                    sizeof(TrackerHeader) + body_len,
                    SF_G_NETWORK_TIMEOUT)) != 0)
    {
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, "
			"errno: %d, error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = in_buff;
		result = fdfs_recv_response(conn, &pInBuff,
                sizeof(in_buff), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}

	if ((in_bytes - TRACKER_QUERY_STORAGE_FETCH_IPV4_BODY_LEN) %
            (IPV4_ADDRESS_SIZE - 1) == 0)
	{
        ip_size = IPV4_ADDRESS_SIZE;
        ip_list_len = in_bytes - TRACKER_QUERY_STORAGE_FETCH_IPV4_BODY_LEN;
    } else if ((in_bytes - TRACKER_QUERY_STORAGE_FETCH_IPV6_BODY_LEN) %
            (IPV6_ADDRESS_SIZE - 1) == 0)
    {
        ip_size = IPV6_ADDRESS_SIZE;
        ip_list_len = in_bytes - TRACKER_QUERY_STORAGE_FETCH_IPV6_BODY_LEN;
    }
    else
    {
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data length: %"PRId64" "
            "is invalid", __LINE__, formatted_ip,
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	*server_count = 1 + ip_list_len / (ip_size - 1);
	if (nMaxServerCount < *server_count)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response storage server "
			 "count: %d, exceeds max server count: %d!", __LINE__,
			formatted_ip, pTrackerServer->port,
			*server_count, nMaxServerCount);
		return ENOSPC;
	}

	memset(pStorageServer, 0, nMaxServerCount * sizeof(ConnectionInfo));
	pStorageServer->sock = -1;

	memcpy(group_name, pInBuff, FDFS_GROUP_NAME_MAX_LEN);
	*(group_name + FDFS_GROUP_NAME_MAX_LEN) = '\0';
	pInBuff += FDFS_GROUP_NAME_MAX_LEN;
	memcpy(pStorageServer->ip_addr, pInBuff, ip_size - 1);
	pInBuff += ip_size - 1;
	pStorageServer->port = (int)buff2long(pInBuff);
	pInBuff += FDFS_PROTO_PKG_LEN_SIZE;

	pServerEnd = pStorageServer + (*server_count);
	for (pServer=pStorageServer+1; pServer<pServerEnd; pServer++)
	{
		pServer->sock = -1;
		pServer->port = pStorageServer->port;
		memcpy(pServer->ip_addr, pInBuff, ip_size - 1);
		pInBuff += ip_size - 1;
	}

	return 0;
}

int tracker_query_storage_store_without_group(ConnectionInfo *pTrackerServer,
		ConnectionInfo *pStorageServer, char *group_name, 
		int *store_path_index)
{
	TrackerHeader header;
	char in_buff[sizeof(TrackerHeader) +
		TRACKER_QUERY_STORAGE_STORE_IPV6_BODY_LEN];
    char formatted_ip[FORMATTED_IP_SIZE];
	bool new_connection;
	ConnectionInfo *conn;
	char *pInBuff;
    char *p;
	int64_t in_bytes;
    int ip_size;
	int result;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	memset(pStorageServer, 0, sizeof(ConnectionInfo));
	pStorageServer->sock = -1;

	memset(&header, 0, sizeof(header));
	header.cmd = TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE;
	if ((result=tcpsenddata_nb(conn->sock, &header, \
			sizeof(header), SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, "
			"errno: %d, error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = in_buff;
		result = fdfs_recv_response(conn, \
				&pInBuff, sizeof(in_buff), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}


	if (in_bytes == TRACKER_QUERY_STORAGE_STORE_IPV4_BODY_LEN)
	{
        ip_size = IPV4_ADDRESS_SIZE;
    }
    else if (in_bytes == TRACKER_QUERY_STORAGE_STORE_IPV6_BODY_LEN)
    {
        ip_size = IPV6_ADDRESS_SIZE;
    }
    else
    {
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data length: %"PRId64" "
            "is invalid, expect length: %d or %d", __LINE__,
            formatted_ip, pTrackerServer->port, in_bytes,
            TRACKER_QUERY_STORAGE_STORE_IPV4_BODY_LEN,
            TRACKER_QUERY_STORAGE_STORE_IPV6_BODY_LEN);
		return EINVAL;
	}

	memcpy(group_name, in_buff, FDFS_GROUP_NAME_MAX_LEN);
	*(group_name + FDFS_GROUP_NAME_MAX_LEN) = '\0';
    p = in_buff + FDFS_GROUP_NAME_MAX_LEN;
	memcpy(pStorageServer->ip_addr, p, ip_size - 1);
    p += ip_size - 1;
	pStorageServer->port = (int)buff2long(p);
    p += FDFS_PROTO_PKG_LEN_SIZE;
	*store_path_index = *p;

	return 0;
}

int tracker_query_storage_store_with_group(ConnectionInfo *pTrackerServer, \
		const char *group_name, ConnectionInfo *pStorageServer, \
		int *store_path_index)
{
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	bool new_connection;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN];
	char in_buff[sizeof(TrackerHeader) + \
		TRACKER_QUERY_STORAGE_STORE_IPV6_BODY_LEN];
    char formatted_ip[FORMATTED_IP_SIZE];
	char *pInBuff;
    char *p;
	int64_t in_bytes;
    int ip_size;
	int result;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	memset(pStorageServer, 0, sizeof(ConnectionInfo));
	pStorageServer->sock = -1;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
    fdfs_pack_group_name(group_name, out_buff + sizeof(TrackerHeader));
	long2buff(FDFS_GROUP_NAME_MAX_LEN, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ONE;
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
			sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN,
			SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, "
			"errno: %d, error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = in_buff;
		result = fdfs_recv_response(conn, \
				&pInBuff, sizeof(in_buff), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}

	if (in_bytes == TRACKER_QUERY_STORAGE_STORE_IPV4_BODY_LEN)
	{
        ip_size = IPV4_ADDRESS_SIZE;
    }
    else if (in_bytes == TRACKER_QUERY_STORAGE_STORE_IPV6_BODY_LEN)
    {
        ip_size = IPV6_ADDRESS_SIZE;
    }
    else
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data "
			"length: %"PRId64" is invalid, expect length: %d or %d",
            __LINE__, formatted_ip, pTrackerServer->port,
			in_bytes, TRACKER_QUERY_STORAGE_STORE_IPV4_BODY_LEN,
            TRACKER_QUERY_STORAGE_STORE_IPV6_BODY_LEN);
		return EINVAL;
	}

    p = in_buff + FDFS_GROUP_NAME_MAX_LEN;
	memcpy(pStorageServer->ip_addr, p, ip_size - 1);
    p += ip_size - 1;
	pStorageServer->port = (int)buff2long(p);
    p += FDFS_PROTO_PKG_LEN_SIZE;
	*store_path_index = *p;

	return 0;
}

int tracker_query_storage_store_list_with_group(
	ConnectionInfo *pTrackerServer, const char *group_name,
	ConnectionInfo *storageServers, const int nMaxServerCount,
	int *storage_count, int *store_path_index)
{
	ConnectionInfo *pStorageServer;
	ConnectionInfo *pServerEnd;
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	bool new_connection;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN];
	char in_buff[sizeof(TrackerHeader) + FDFS_MAX_SERVERS_EACH_GROUP *
			TRACKER_QUERY_STORAGE_STORE_IPV6_BODY_LEN];
    char formatted_ip[FORMATTED_IP_SIZE];
	char returned_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char *pInBuff;
	char *p;
	int64_t in_bytes;
	int out_len;
    int record_length;
	int ipPortsLen;
    int ip_size;
	int result;

	*storage_count = 0;
	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));

	if (group_name == NULL || *group_name == '\0')
	{
	pHeader->cmd = TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ALL;
	out_len = 0;
	}
	else
	{
	pHeader->cmd = TRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITH_GROUP_ALL;
    fdfs_pack_group_name(group_name, out_buff + sizeof(TrackerHeader));
	out_len = FDFS_GROUP_NAME_MAX_LEN;
	}

	long2buff(out_len, pHeader->pkg_len);
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
		sizeof(TrackerHeader) + out_len, SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, "
			"errno: %d, error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = in_buff;
		result = fdfs_recv_response(conn, &pInBuff,
                sizeof(in_buff), &in_bytes);
        if (result != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}

	if (in_bytes < TRACKER_QUERY_STORAGE_STORE_IPV4_BODY_LEN)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data "
			"length: %"PRId64" is invalid, expect length >= %d",
            __LINE__, formatted_ip, pTrackerServer->port,
			in_bytes, TRACKER_QUERY_STORAGE_STORE_IPV4_BODY_LEN);
		return EINVAL;
	}

#define IPV4_RECORD_LENGTH (IPV4_ADDRESS_SIZE - 1 + FDFS_PROTO_PKG_LEN_SIZE)
#define IPV6_RECORD_LENGTH (IPV6_ADDRESS_SIZE - 1 + FDFS_PROTO_PKG_LEN_SIZE)

	ipPortsLen = in_bytes - (FDFS_GROUP_NAME_MAX_LEN + 1);
	if (ipPortsLen % IPV4_RECORD_LENGTH == 0)
	{
        ip_size = IPV4_ADDRESS_SIZE;
        record_length = IPV4_RECORD_LENGTH;
    }
    else if (ipPortsLen % IPV6_RECORD_LENGTH == 0)
    {
        ip_size = IPV6_ADDRESS_SIZE;
        record_length = IPV6_RECORD_LENGTH;
    }
    else
    {
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
        logError("file: "__FILE__", line: %d, "
                "tracker server %s:%u response data "
                "length: %"PRId64" is invalid", __LINE__,
                formatted_ip, pTrackerServer->port, in_bytes);
        return EINVAL;
    }

	*storage_count = ipPortsLen / record_length;
	if (nMaxServerCount < *storage_count)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response storage server "
			 "count: %d, exceeds max server count: %d!",
			__LINE__, formatted_ip, pTrackerServer->port,
            *storage_count, nMaxServerCount);
		return ENOSPC;
	}

	memset(storageServers, 0, sizeof(ConnectionInfo) * nMaxServerCount);

	memcpy(returned_group_name, in_buff, FDFS_GROUP_NAME_MAX_LEN);
	p = in_buff + FDFS_GROUP_NAME_MAX_LEN;
	*(returned_group_name + FDFS_GROUP_NAME_MAX_LEN) = '\0';

	pServerEnd = storageServers + (*storage_count);
	for (pStorageServer=storageServers; pStorageServer<pServerEnd; \
		pStorageServer++)
	{
		pStorageServer->sock = -1;
		memcpy(pStorageServer->ip_addr, p, ip_size - 1);
		p += ip_size - 1;

		pStorageServer->port = (int)buff2long(p);
		p += FDFS_PROTO_PKG_LEN_SIZE;
	}

	*store_path_index = *p;
	return 0;
}

int tracker_delete_storage(TrackerServerGroup *pTrackerGroup, \
		const char *group_name, const char *storage_id)
{
	ConnectionInfo *conn;
	TrackerHeader *pHeader;
	TrackerServerInfo tracker_server;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
	FDFSStorageInfo storage_infos[1];
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			FDFS_STORAGE_ID_MAX_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	char *pInBuff;
	int64_t in_bytes;
	int result;
	int body_len;
	int storage_count;
	int enoent_count;

	enoent_count = 0;
	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
		memcpy(&tracker_server, pServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(&tracker_server);
		if ((conn=tracker_connect_server(&tracker_server, &result)) == NULL)
		{
			return result;
		}

		result = tracker_list_servers(conn, group_name, storage_id,
				storage_infos, 1, &storage_count);
		tracker_close_connection_ex(conn, result != 0 && result != ENOENT);
		if (result != 0 && result != ENOENT)
		{
			return result;
		}

		if (result == ENOENT || storage_count == 0)
		{
			enoent_count++;
			continue;
		}

		if (storage_infos[0].status == FDFS_STORAGE_STATUS_ONLINE
		   || storage_infos[0].status == FDFS_STORAGE_STATUS_ACTIVE)
		{
			return EBUSY;
		}
	}
	if (enoent_count == pTrackerGroup->server_count)
	{
		return ENOENT;
	}

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
    body_len = fdfs_pack_group_name_and_storage_id(group_name, storage_id,
            out_buff + sizeof(TrackerHeader), sizeof(out_buff) -
            sizeof(TrackerHeader));
	long2buff(body_len, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_SERVER_DELETE_STORAGE;

	enoent_count = 0;
	result = 0;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
		memcpy(&tracker_server, pServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(&tracker_server);
		if ((conn=tracker_connect_server(&tracker_server, &result)) == NULL)
		{
			return result;
		}

        if ((result=tcpsenddata_nb(conn->sock, out_buff,
                        sizeof(TrackerHeader) + body_len,
                        SF_G_NETWORK_TIMEOUT)) != 0)
        {
            format_ip_address(conn->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"send data to tracker server %s:%u fail, "
				"errno: %d, error info: %s", __LINE__, formatted_ip,
                conn->port, result, STRERROR(result));
		}
		else
		{
			pInBuff = in_buff;
			result = fdfs_recv_response(conn, &pInBuff, 0, &in_bytes);
            if (result != 0)
            {
                logError("file: "__FILE__", line: %d, "
                        "fdfs_recv_response fail, result: %d",
                        __LINE__, result);
            }
        }

		tracker_close_connection_ex(conn, result != 0 && result != ENOENT);
		if (result != 0)
		{
			if (result == ENOENT)
			{
				enoent_count++;
			}
			else if (result == EALREADY)
			{
			}
			else
			{
				return result;
			}
		}
	}

	if (enoent_count == pTrackerGroup->server_count)
	{
		return ENOENT;
	}

	return result == ENOENT ? 0 : result;
}

int tracker_delete_group(TrackerServerGroup *pTrackerGroup, \
		const char *group_name)
{
	ConnectionInfo *conn;
	TrackerHeader *pHeader;
	TrackerServerInfo tracker_server;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN]; 
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	char *pInBuff;
	int64_t in_bytes;
	int result;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
    fdfs_pack_group_name(group_name, out_buff + sizeof(TrackerHeader));
	long2buff(FDFS_GROUP_NAME_MAX_LEN, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_SERVER_DELETE_GROUP;

	result = 0;
	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
		memcpy(&tracker_server, pServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(&tracker_server);
		if ((conn=tracker_connect_server(&tracker_server, &result)) == NULL)
		{
			return result;
		}

		if ((result=tcpsenddata_nb(conn->sock, out_buff,
			sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN,
            SF_G_NETWORK_TIMEOUT)) != 0)
		{
            format_ip_address(conn->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"send data to tracker server %s:%u fail, "
				"errno: %d, error info: %s", __LINE__, formatted_ip,
                conn->port, result, STRERROR(result));
            break;
		}

        pInBuff = in_buff;
        result = fdfs_recv_response(conn, &pInBuff, 0, &in_bytes);
		tracker_close_connection_ex(conn, result != 0 && result != ENOENT);
		if (result != 0)
		{
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
            break;
		}
	}

	return result;
}

int tracker_set_trunk_server(TrackerServerGroup *pTrackerGroup, \
		const char *group_name, const char *storage_id, \
		char *new_trunk_server_id)
{
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
	TrackerServerInfo tracker_server;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN +
			FDFS_STORAGE_ID_MAX_SIZE];
	char in_buff[FDFS_STORAGE_ID_MAX_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	char *pInBuff;
	int64_t in_bytes;
    int body_len;
	int storage_id_len;
	int result;

	*new_trunk_server_id = '\0';
	memset(out_buff, 0, sizeof(out_buff));
	memset(in_buff, 0, sizeof(in_buff));
	pHeader = (TrackerHeader *)out_buff;
	if (storage_id == NULL)
	{
        fdfs_pack_group_name(group_name, out_buff + sizeof(TrackerHeader));
        body_len = FDFS_GROUP_NAME_MAX_LEN;
		storage_id_len = 0;
	}
	else
	{
        body_len = fdfs_pack_group_name_and_storage_id(group_name, storage_id,
                out_buff + sizeof(TrackerHeader), sizeof(out_buff) -
                sizeof(TrackerHeader));
		storage_id_len = body_len - FDFS_GROUP_NAME_MAX_LEN;
	}
	
	long2buff(body_len, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_SERVER_SET_TRUNK_SERVER;

	result = 0;
	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
		memcpy(&tracker_server, pServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(&tracker_server);
		if ((conn=tracker_connect_server(&tracker_server, &result)) == NULL)
		{
			continue;
		}

        if ((result=tcpsenddata_nb(conn->sock, out_buff,
                        sizeof(TrackerHeader) + body_len,
                        SF_G_NETWORK_TIMEOUT)) != 0)
		{
            format_ip_address(conn->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"send data to tracker server %s:%u fail, "
				"errno: %d, error info: %s", __LINE__, formatted_ip,
                conn->port, result, STRERROR(result));

			tracker_close_connection_ex(conn, true);
			continue;
		}

		pInBuff = in_buff;
		result = fdfs_recv_response(conn, &pInBuff,
				sizeof(in_buff) - 1, &in_bytes);

		tracker_close_connection_ex(conn, result != 0);
		if (result == 0)
		{
			strcpy(new_trunk_server_id, in_buff);
			return 0;
		}

		if (result == EOPNOTSUPP)
		{
			continue;
		}
		if (result == EALREADY)
		{
			if (storage_id_len > 0)
			{
				strcpy(new_trunk_server_id, storage_id);
			}
			return result;
		}
		else
		{
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
			return result;
		}
	}

	return result;
}

int tracker_get_storage_status(ConnectionInfo *pTrackerServer,
		const char *group_name, const char *ip_addr,
		FDFSStorageBrief *pDestBuff)
{
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	bool new_connection;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN +
			IPV6_ADDRESS_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	char *pInBuff;
	char *p;
	int result;
	int ip_len;
	int64_t in_bytes;

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);
	
	if (ip_addr == NULL)
	{
		ip_len = 0;
	}
	else
	{
		ip_len = strlen(ip_addr);
	}

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
    fdfs_pack_group_name(group_name, p);
	p += FDFS_GROUP_NAME_MAX_LEN;
	if (ip_len > 0)
	{
		memcpy(p, ip_addr, ip_len);
		p += ip_len;
	}
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_GET_STATUS;
	long2buff(FDFS_GROUP_NAME_MAX_LEN + ip_len, pHeader->pkg_len);
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
			p - out_buff, SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, "
			"errno: %d, error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		pInBuff = (char *)pDestBuff;
		result = fdfs_recv_response(conn, \
			&pInBuff, sizeof(FDFSStorageBrief), &in_bytes);
		if (result != 0)
		{
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
		}
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}

	if (in_bytes != sizeof(FDFSStorageBrief))
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data "
			"length: %"PRId64" is invalid", __LINE__,
            formatted_ip, pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

int tracker_get_storage_id(ConnectionInfo *pTrackerServer, \
		const char *group_name, const char *ip_addr, \
		char *storage_id)
{
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	bool new_connection;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			IPV6_ADDRESS_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	char *p;
	int result;
	int ip_len;
	int64_t in_bytes;

	if (storage_id == NULL)
	{
		return EINVAL;
	}

	CHECK_CONNECTION(pTrackerServer, conn, result, new_connection);
	
	if (ip_addr == NULL)
	{
		ip_len = 0;
	}
	else
	{
		ip_len = strlen(ip_addr);
	}

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
    fdfs_pack_group_name(group_name, p);
	p += FDFS_GROUP_NAME_MAX_LEN;
	if (ip_len > 0)
	{
		memcpy(p, ip_addr, ip_len);
		p += ip_len;
	}
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_GET_SERVER_ID;
	long2buff(FDFS_GROUP_NAME_MAX_LEN + ip_len, pHeader->pkg_len);
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
			p - out_buff, SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, "
			"errno: %d, error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
	}
	else
	{
		result = fdfs_recv_response(conn, \
			&storage_id, FDFS_STORAGE_ID_MAX_SIZE, &in_bytes);
		if (result != 0)
		{
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
		}
	}

	if (new_connection)
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	if (result != 0)
	{
		return result;
	}

	if (in_bytes == 0 || in_bytes >= FDFS_STORAGE_ID_MAX_SIZE)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data length: %"PRId64" "
            "is invalid", __LINE__, formatted_ip,
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	*(storage_id + in_bytes) = '\0';
	return 0;
}

int tracker_get_storage_max_status(TrackerServerGroup *pTrackerGroup, \
		const char *group_name, const char *ip_addr, \
		char *storage_id, int *status)
{
	ConnectionInfo *conn;
	TrackerServerInfo tracker_server;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
	FDFSStorageBrief storage_brief;
	int result;

	memset(&storage_brief, 0, sizeof(FDFSStorageBrief));
	storage_brief.status = -1;

	*storage_id = '\0';
	*status = -1;
	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
		memcpy(&tracker_server, pServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(&tracker_server);
		if ((conn=tracker_connect_server(&tracker_server, &result)) == NULL)
		{
			return result;
		}

		result = tracker_get_storage_status(conn, group_name, \
				ip_addr, &storage_brief);
		tracker_close_connection_ex(conn, result != 0);

		if (result != 0)
		{
			if (result == ENOENT)
			{
				continue;
			}
			return result;
		}

		strcpy(storage_id, storage_brief.id);
		if (storage_brief.status > *status)
		{
			*status = storage_brief.status;
		}
	}

	if (*status == -1)
	{
		return ENOENT;
	}

	return 0;
}
