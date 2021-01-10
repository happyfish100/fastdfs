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
		g_last_storage_ip.ips[0].address);
	strcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
		IP_ADDRESS_SIZE, g_tracker_client_ip.ips[0].address);

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
	result = fdfs_recv_response(pTrackerServer,
                &pInBuff, 0, &in_bytes);

	if (result == 0 || result == EALREADY || result == ENOENT
            || result == EEXIST)
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
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%d, recv data fail or "
			"response status != 0, "
			"errno: %d, error info: %s",
			__LINE__, pTrackerServer->ip_addr,
			pTrackerServer->port, result, STRERROR(result));
		return result == EBUSY ? 0 : result;
	}
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

        if ((result=storage_set_tracker_client_ips(conn,
                        pGlobalServer - g_tracker_group.servers)) != 0)
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
		__LINE__, g_last_storage_ip.ips[0].address,
        g_tracker_client_ip.ips[0].address);

	if (g_last_storage_ip.count == 0)
	{
		return storage_write_to_sync_ini_file();
	}
	else if (strcmp(g_tracker_client_ip.ips[0].address,
                g_last_storage_ip.ips[0].address) == 0)
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

