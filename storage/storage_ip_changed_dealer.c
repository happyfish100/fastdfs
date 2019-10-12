/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_ip_changed_dealer.h"

static int storage_do_changelog_req(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			FDFS_STORAGE_ID_MAX_SIZE];
	TrackerHeader *pHeader;
	int result;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;

	long2buff(FDFS_GROUP_NAME_MAX_LEN + FDFS_STORAGE_ID_MAX_SIZE, \
		pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_CHANGELOG_REQ;
	strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
	strcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN,
		g_my_server_id_str);
	if((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
		sizeof(out_buff), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	return tracker_deal_changelog_response(pTrackerServer);
}

static int storage_report_ip_changed(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
		2 * IP_ADDRESS_SIZE];
	char in_buff[1];
	char *pInBuff;
	TrackerHeader *pHeader;
	int result;
	int64_t in_bytes;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;

	long2buff(FDFS_GROUP_NAME_MAX_LEN+2*IP_ADDRESS_SIZE, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_REPORT_IP_CHANGED;
	strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
	strcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN, \
		g_last_storage_ip.ips[0]);
	strcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
		IP_ADDRESS_SIZE, g_tracker_client_ip.ips[0]);

	if((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
		sizeof(out_buff), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, result, STRERROR(result));
		return result;
	}

	pInBuff = in_buff;
	result = fdfs_recv_response(pTrackerServer, \
                &pInBuff, 0, &in_bytes);

	if (result == 0 || result == EALREADY || result == ENOENT)
	{
        if (result != 0)
        {
            logWarning("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
		return 0;
	}
	else
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv data fail or " \
			"response status != 0, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, result, STRERROR(result));
		return result == EBUSY ? 0 : result;
	}
}

static int storage_get_my_ip_from_tracker(ConnectionInfo *conn,
        char *ip_addrs, const int buff_size)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN];
	TrackerHeader *pHeader;
	int result;
    int64_t in_bytes;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;

	long2buff(FDFS_GROUP_NAME_MAX_LEN, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_GET_MY_IP;
	strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
	if((result=tcpsenddata_nb(conn->sock, out_buff,
		sizeof(out_buff), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%d, send data fail, "
			"errno: %d, error info: %s.",
			__LINE__, conn->ip_addr, conn->port,
			result, STRERROR(result));
		return result;
	}

    if ((result=fdfs_recv_response(conn, &ip_addrs,
                    buff_size - 1, &in_bytes)) != 0)
    {
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%d, recv response fail, "
			"errno: %d, error info: %s.",
			__LINE__, conn->ip_addr, conn->port,
			result, STRERROR(result));
		return result;
    }

    *(ip_addrs + in_bytes) = '\0';
    return 0;
}

static int storage_set_tracker_client_ips(ConnectionInfo *conn)
{
    char my_ip_addrs[256];
    char error_info[256];
    FDFSMultiIP multi_ip;
	int result;
	int i;

    if ((result=storage_get_my_ip_from_tracker(conn, my_ip_addrs,
                    sizeof(my_ip_addrs))) != 0)
    {
        return result;
    }

    if ((result=fdfs_parse_multi_ips_ex(my_ip_addrs, &multi_ip,
                    error_info, sizeof(error_info), false)) != 0)
    {
        return result;
    }

    for (i = 0; i < multi_ip.count; i++)
    {
        result = storage_insert_ip_addr_to_multi_ips(&g_tracker_client_ip,
                multi_ip.ips[i], multi_ip.count);
        if (result == 0)
        {
            if ((result=fdfs_check_and_format_ips(&g_tracker_client_ip,
                        error_info, sizeof(error_info))) != 0)
            {
                logCrit("file: "__FILE__", line: %d, "
                        "as a client of tracker server %s:%d, "
                        "my ip: %s not valid, error info: %s. "
                        "program exit!", __LINE__,
                        conn->ip_addr, conn->port,
                        multi_ip.ips[i], error_info);

                return result;
            }

            insert_into_local_host_ip(multi_ip.ips[i]);
        }
        else if (result != EEXIST)
        {
            char ip_str[256];

            fdfs_multi_ips_to_string(&g_tracker_client_ip,
                    ip_str, sizeof(ip_str));
            logError("file: "__FILE__", line: %d, "
                    "as a client of tracker server %s:%d, "
                    "my ip: %s not consistent with client ips: %s "
                    "of other tracker client. program exit!", __LINE__,
                    conn->ip_addr, conn->port,
                    multi_ip.ips[i], ip_str);

            return result;
        }
    }

    return 0;
}

int storage_get_my_tracker_client_ip()
{
	TrackerServerInfo *pGlobalServer;
	TrackerServerInfo *pTServer;
	TrackerServerInfo *pTServerEnd;
	TrackerServerInfo trackerServer;
    ConnectionInfo *conn;
	char tracker_client_ip[IP_ADDRESS_SIZE];
	int success_count;
	int result;
	int i;

    conn = NULL;
	result = 0;
	success_count = 0;
	pTServer = &trackerServer;
	pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;

	while (success_count == 0 && g_continue_flag)
	{
	for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd;
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(pTServer);
		for (i=0; i < 3; i++)
		{
            conn = tracker_connect_server_no_pool_ex(pTServer,
                    g_client_bind_addr ? g_bind_addr : NULL, &result, false);
            if (conn != NULL)
            {
				break;
            }

			sleep(5);
		}

		if (conn == NULL)
		{
			logError("file: "__FILE__", line: %d, "
				"connect to tracker server %s:%d fail, "
				"errno: %d, error info: %s",
				__LINE__, pTServer->connections[0].ip_addr,
                pTServer->connections[0].port,
				result, STRERROR(result));

			continue;
		}

        if ((result=storage_set_tracker_client_ips(conn)) != 0)
        {
            close(conn->sock);
            return result;
        }

		getSockIpaddr(conn->sock, tracker_client_ip, IP_ADDRESS_SIZE);
        insert_into_local_host_ip(tracker_client_ip);

		fdfs_quit(conn);
		close(conn->sock);
		success_count++;
	}
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	return 0;
}

static int storage_report_storage_ip_addr()
{
	TrackerServerInfo *pGlobalServer;
	TrackerServerInfo *pTServer;
	TrackerServerInfo *pTServerEnd;
	TrackerServerInfo trackerServer;
    ConnectionInfo *conn;
	int success_count;
	int result;
	int i;

	result = 0;
	success_count = 0;
	pTServer = &trackerServer;
	pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;

	logDebug("file: "__FILE__", line: %d, "
		"last my ip is %s, current my ip is %s",
		__LINE__, g_last_storage_ip.ips[0],
        g_tracker_client_ip.ips[0]);

	if (g_last_storage_ip.count == 0)
	{
		return storage_write_to_sync_ini_file();
	}
	else if (strcmp(g_tracker_client_ip.ips[0],
                g_last_storage_ip.ips[0]) == 0)
	{
		return 0;
	}

	success_count = 0;
	while (success_count == 0 && g_continue_flag)
	{
	for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(pTServer);
		for (i=0; i < 3; i++)
		{
            conn = tracker_connect_server_no_pool_ex(pTServer,
                    g_client_bind_addr ? g_bind_addr : NULL, &result, false);
            if (conn != NULL)
            {
				break;
            }

			sleep(1);
		}

        if (conn == NULL)
		{
			logError("file: "__FILE__", line: %d, "
				"connect to tracker server %s:%d fail, "
				"errno: %d, error info: %s",
				__LINE__, pTServer->connections[0].ip_addr,
                pTServer->connections[0].port,
				result, STRERROR(result));

			continue;
		}

		if ((result=storage_report_ip_changed(conn)) == 0)
		{
			success_count++;
		}
		else
		{
			sleep(1);
		}

		fdfs_quit(conn);
		close(conn->sock);
	}
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	return storage_write_to_sync_ini_file();
}

int storage_changelog_req()
{
	TrackerServerInfo *pGlobalServer;
	TrackerServerInfo *pTServer;
	TrackerServerInfo *pTServerEnd;
	TrackerServerInfo trackerServer;
    ConnectionInfo *conn;
	int success_count;
	int result;
	int i;

	result = 0;
	success_count = 0;
	pTServer = &trackerServer;
	pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;

	while (success_count == 0 && g_continue_flag)
	{
	for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(pTServer);
		for (i=0; i < 3; i++)
		{
            conn = tracker_connect_server_no_pool_ex(pTServer,
                    g_client_bind_addr ? g_bind_addr : NULL, &result, false);
            if (conn != NULL)
            {
				break;
            }

			sleep(1);
		}

        if (conn == NULL)
		{
			logError("file: "__FILE__", line: %d, "
				"connect to tracker server %s:%d fail, "
				"errno: %d, error info: %s",
				__LINE__, pTServer->connections[0].ip_addr,
                pTServer->connections[0].port,
				result, STRERROR(result));

			continue;
		}

		result = storage_do_changelog_req(conn);
		if (result == 0 || result == ENOENT)
		{
			success_count++;
		}
		else
		{
			sleep(1);
		}

		fdfs_quit(conn);
		close(conn->sock);
	}
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	return 0;
}

int storage_check_ip_changed()
{
	int result;

	if ((!g_storage_ip_changed_auto_adjust) || g_use_storage_id)
	{
		return 0;
	}

	if ((result=storage_report_storage_ip_addr()) != 0)
	{
		return result;
	}

	if (g_last_storage_ip.count == 0) //first run
	{
		return 0;
	}

	return storage_changelog_req();
}

