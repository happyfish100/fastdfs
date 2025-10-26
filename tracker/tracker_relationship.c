/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/pthread_func.h"
#include "fdfs_global.h"
#include "fdfs_shared_func.h"
#include "tracker_global.h"
#include "tracker_proto.h"
#include "tracker_mem.h"
#include "tracker_relationship.h"

bool g_if_leader_self = false;  //if I am leader

static int fdfs_ping_leader(ConnectionInfo *pTrackerServer)
{
	TrackerHeader header;
	int result;
	int success_count;
	int64_t in_bytes;
	char in_buff[(FDFS_GROUP_NAME_MAX_LEN + FDFS_STORAGE_ID_MAX_SIZE) * \
			FDFS_MAX_GROUPS];
    char formatted_ip[FORMATTED_IP_SIZE];
	char *pInBuff;
	char *p;
	char *pEnd;
	FDFSGroupInfo *pGroup;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char trunk_server_id[FDFS_STORAGE_ID_MAX_SIZE];

	memset(&header, 0, sizeof(header));
	header.cmd = TRACKER_PROTO_CMD_TRACKER_PING_LEADER;
	result = tcpsenddata_nb(pTrackerServer->sock, &header,
			sizeof(header), SF_G_NETWORK_TIMEOUT);
	if(result != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u, send data fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pTrackerServer->port, result, STRERROR(result));
		return result;
	}

	pInBuff = in_buff;
	if ((result=fdfs_recv_response(pTrackerServer, &pInBuff,
			sizeof(in_buff), &in_bytes)) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
        logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response from %s:%u fail, result: %d",
                __LINE__, formatted_ip, pTrackerServer->port, result);
		return result;
	}

	if (in_bytes == 0)
	{
		return 0;
	}
	else if (in_bytes % (FDFS_GROUP_NAME_MAX_LEN + \
			FDFS_STORAGE_ID_MAX_SIZE) != 0)
	{
        format_ip_address(pTrackerServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u, invalid body length: "
			"%"PRId64, __LINE__, formatted_ip,
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	success_count = 0;
	memset(group_name, 0, sizeof(group_name));
	memset(trunk_server_id, 0, sizeof(trunk_server_id));

	pEnd = in_buff + in_bytes;
	for (p=in_buff; p<pEnd; p += FDFS_GROUP_NAME_MAX_LEN + \
					FDFS_STORAGE_ID_MAX_SIZE)
	{
		memcpy(group_name, p, FDFS_GROUP_NAME_MAX_LEN);
		memcpy(trunk_server_id, p + FDFS_GROUP_NAME_MAX_LEN, \
			FDFS_STORAGE_ID_MAX_SIZE - 1);

		pGroup = tracker_mem_get_group(group_name);
		if (pGroup == NULL)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"tracker server ip: %s, group: %s not exists", \
				__LINE__, pTrackerServer->ip_addr, group_name);
			continue;
		}

		if (*trunk_server_id == '\0')
		{
			*(pGroup->last_trunk_server_id) = '\0';
			pGroup->pTrunkServer = NULL;
			success_count++;
			continue;
		}

		pGroup->pTrunkServer = tracker_mem_get_storage(pGroup, \
							trunk_server_id);
		if (pGroup->pTrunkServer == NULL)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"tracker server ip: %s, group: %s, " \
				"trunk server: %s not exists", \
				__LINE__, pTrackerServer->ip_addr, \
				group_name, trunk_server_id);
		}
		fc_safe_strcpy(pGroup->last_trunk_server_id, trunk_server_id);
		success_count++;
	}

	if (success_count > 0)
	{
		tracker_save_groups();
	}

	return 0;
}

static int relationship_cmp_tracker_status(const void *p1, const void *p2)
{
	TrackerRunningStatus *pStatus1;
	TrackerRunningStatus *pStatus2;
	ConnectionInfo *conn1;
	ConnectionInfo *conn2;
	int sub;

	pStatus1 = (TrackerRunningStatus *)p1;
	pStatus2 = (TrackerRunningStatus *)p2;
	sub = pStatus1->if_leader - pStatus2->if_leader;
	if (sub != 0)
	{
		return sub;
	}

	sub = pStatus1->running_time - pStatus2->running_time;
	if (sub != 0)
	{
		return sub;
	}

	sub = pStatus2->restart_interval - pStatus1->restart_interval;
	if (sub != 0)
	{
		return sub;
	}

	conn1 = pStatus1->pTrackerServer->connections;
	conn2 = pStatus2->pTrackerServer->connections;
	sub = strcmp(conn1->ip_addr, conn2->ip_addr);
	if (sub != 0)
	{
		return sub;
	}

	return conn1->port - conn2->port;
}

static int relationship_get_tracker_status(TrackerRunningStatus *pStatus)
{
    if (fdfs_server_contain_local_service(pStatus->pTrackerServer,
                SF_G_INNER_PORT))
    {
        tracker_calc_running_times(pStatus);
        pStatus->if_leader = g_if_leader_self;
        return 0;
    }
    else
    {
        return fdfs_get_tracker_status(pStatus->pTrackerServer, pStatus);
    }
}

static int relationship_get_tracker_leader(TrackerRunningStatus *pTrackerStatus)
{
	TrackerServerInfo *pTrackerServer;
	TrackerServerInfo *pTrackerEnd;
	TrackerRunningStatus *pStatus;
	TrackerRunningStatus trackerStatus[FDFS_MAX_TRACKERS];
    char formatted_ip[FORMATTED_IP_SIZE];
	int count;
	int result;
	int r;
	int i;

	memset(pTrackerStatus, 0, sizeof(TrackerRunningStatus));
	pStatus = trackerStatus;
	result = 0;
	pTrackerEnd = g_tracker_servers.servers + g_tracker_servers.server_count;
	for (pTrackerServer=g_tracker_servers.servers;
		pTrackerServer<pTrackerEnd; pTrackerServer++)
	{
		pStatus->pTrackerServer = pTrackerServer;
        r = relationship_get_tracker_status(pStatus);
		if (r == 0)
		{
			pStatus++;
		}
		else if (r != ENOENT)
		{
			result = r;
		}
	}

	count = pStatus - trackerStatus;
	if (count == 0)
	{
		logError("file: "__FILE__", line: %d, "
                "get tracker status fail, "
                "tracker server count: %d", __LINE__,
                g_tracker_servers.server_count);
		return result == 0 ? ENOENT : result;
	}

	qsort(trackerStatus, count, sizeof(TrackerRunningStatus), \
		relationship_cmp_tracker_status);

    if (FC_LOG_BY_LEVEL(LOG_DEBUG)) {
        for (i=0; i<count; i++)
        {
            format_ip_address(trackerStatus[i].pTrackerServer->
                    connections->ip_addr, formatted_ip);
            logDebug("file: "__FILE__", line: %d, "
                    "%s:%u if_leader: %d, running time: %d, "
                    "restart interval: %d", __LINE__, formatted_ip,
                    trackerStatus[i].pTrackerServer->connections->port,
                    trackerStatus[i].if_leader,
                    trackerStatus[i].running_time,
                    trackerStatus[i].restart_interval);
        }
    }

	memcpy(pTrackerStatus, trackerStatus + (count - 1), \
			sizeof(TrackerRunningStatus));
	return 0;
}

static int do_notify_leader_changed(TrackerServerInfo *pTrackerServer, \
		ConnectionInfo *pLeader, const char cmd, bool *bConnectFail)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_MAX_IP_PORT_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	ConnectionInfo *conn;
	TrackerHeader *pHeader;
	char *pInBuff;
    char *pBody;
	int64_t in_bytes;
    int body_len;
	int result;

    fdfs_server_sock_reset(pTrackerServer);
	if ((conn=tracker_connect_server(pTrackerServer, &result)) == NULL)
	{
		*bConnectFail = true;
		return result;
	}
	*bConnectFail = false;

	do
	{
	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
    pBody = (char *)(pHeader + 1);
	pHeader->cmd = cmd;
    format_ip_port(pLeader->ip_addr, pLeader->port, pBody);
    body_len = strlen(pBody);
	long2buff(body_len, pHeader->pkg_len);
	if ((result=tcpsenddata_nb(conn->sock, out_buff,
                    sizeof(TrackerHeader) + body_len,
                    SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(conn->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to tracker server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip, conn->port,
            result, STRERROR(result));

		result = (result == ENOENT ? EACCES : result);
		break;
	}

	pInBuff = in_buff;
	result = fdfs_recv_response(conn, &pInBuff, 0, &in_bytes);
	if (result != 0)
	{
        format_ip_address(conn->ip_addr, formatted_ip);
        logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response from tracker server %s:%u fail, "
                "result: %d", __LINE__, formatted_ip, conn->port, result);
		break;
	}

	if (in_bytes != 0)
	{
        format_ip_address(conn->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"tracker server %s:%u response data "
			"length: %"PRId64" is invalid, "
			"expect length: %d.", __LINE__,
			formatted_ip, conn->port, in_bytes, 0);
		result = EINVAL;
		break;
	}
	} while (0);

	if (conn->port == SF_G_INNER_PORT &&
		is_local_host_ip(conn->ip_addr))
	{
		tracker_close_connection_ex(conn, true);
	}
	else
	{
		tracker_close_connection_ex(conn, result != 0);
	}

	return result;
}

void relationship_set_tracker_leader(const int server_index,
        ConnectionInfo *pLeader, const bool leader_self)
{
    char formatted_ip[FORMATTED_IP_SIZE];

    g_tracker_servers.leader_index = server_index;
    g_next_leader_index = -1;

    if (leader_self)
    {
        g_if_leader_self = true;
        g_tracker_leader_chg_count++;
    }
    else
    {
        format_ip_address(pLeader->ip_addr, formatted_ip);
        logInfo("file: "__FILE__", line: %d, "
            "the tracker leader is %s:%u", __LINE__,
            formatted_ip, pLeader->port);
    }
}

static int relationship_notify_next_leader(TrackerServerInfo *pTrackerServer,
        TrackerRunningStatus *pTrackerStatus, bool *bConnectFail)
{
    if (pTrackerStatus->pTrackerServer == pTrackerServer)
    {
        g_next_leader_index = pTrackerServer - g_tracker_servers.servers;
        return 0;
    }
    else
    {
        ConnectionInfo *pLeader;
        pLeader = pTrackerStatus->pTrackerServer->connections;
        return do_notify_leader_changed(pTrackerServer, pLeader,
                TRACKER_PROTO_CMD_TRACKER_NOTIFY_NEXT_LEADER, bConnectFail);
    }
}

static int relationship_commit_next_leader(TrackerServerInfo *pTrackerServer,
        TrackerRunningStatus *pTrackerStatus, bool *bConnectFail)
{
    ConnectionInfo *pLeader;

    pLeader = pTrackerStatus->pTrackerServer->connections;
    if (pTrackerStatus->pTrackerServer == pTrackerServer)
    {
        int server_index;
        int expect_index;
        server_index = g_next_leader_index;
        expect_index = pTrackerServer - g_tracker_servers.servers;
        if (server_index != expect_index)
        {
            logError("file: "__FILE__", line: %d, "
                    "g_next_leader_index: %d != expected: %d",
                    __LINE__, server_index, expect_index);
            g_next_leader_index = -1;
            return EBUSY;
        }

        relationship_set_tracker_leader(server_index, pLeader, true);
        return 0;
    }
    else
    {
        return do_notify_leader_changed(pTrackerServer, pLeader,
                TRACKER_PROTO_CMD_TRACKER_COMMIT_NEXT_LEADER, bConnectFail);
    }
}

static int relationship_notify_leader_changed(TrackerRunningStatus *pTrackerStatus)
{
	TrackerServerInfo *pTrackerServer;
	TrackerServerInfo *pTrackerEnd;
	int result;
	bool bConnectFail;
	int success_count;

	result = ENOENT;
	pTrackerEnd = g_tracker_servers.servers + g_tracker_servers.server_count;
	success_count = 0;
	for (pTrackerServer=g_tracker_servers.servers;
		pTrackerServer<pTrackerEnd; pTrackerServer++)
	{
		if ((result=relationship_notify_next_leader(pTrackerServer,
				pTrackerStatus, &bConnectFail)) != 0)
		{
			if (!bConnectFail)
			{
				return result;
			}
		}
		else
		{
			success_count++;
		}
	}

	if (success_count == 0)
	{
		return result;
	}

	result = ENOENT;
	success_count = 0;
	for (pTrackerServer=g_tracker_servers.servers;
		pTrackerServer<pTrackerEnd; pTrackerServer++)
	{
		if ((result=relationship_commit_next_leader(pTrackerServer,
				pTrackerStatus, &bConnectFail)) != 0)
		{
			if (!bConnectFail)
			{
				return result;
			}
		}
		else
		{
			success_count++;
		}
	}
	if (success_count == 0)
	{
		return result;
	}

	return 0;
}

static int relationship_select_leader()
{
	int result;
	TrackerRunningStatus trackerStatus;
    ConnectionInfo *conn;
    char formatted_ip[FORMATTED_IP_SIZE];

	if (g_tracker_servers.server_count <= 0)
	{
		return 0;
	}

	logInfo("file: "__FILE__", line: %d, " \
		"selecting tracker leader...", __LINE__);

	if ((result=relationship_get_tracker_leader(&trackerStatus)) != 0)
	{
		return result;
	}

    conn = trackerStatus.pTrackerServer->connections;
    if (fdfs_server_contain_local_service(trackerStatus.
                pTrackerServer, SF_G_INNER_PORT))
	{
		if ((result=relationship_notify_leader_changed(
                        &trackerStatus)) != 0)
		{
			return result;
		}

        format_ip_address(conn->ip_addr, formatted_ip);
		logInfo("file: "__FILE__", line: %d, "
			"I am the new tracker leader %s:%u",
			__LINE__, formatted_ip, conn->port);

		tracker_mem_find_trunk_servers();
	}
	else
	{
		if (trackerStatus.if_leader)
		{
			g_tracker_servers.leader_index =
				trackerStatus.pTrackerServer -
				g_tracker_servers.servers;
			if (g_tracker_servers.leader_index < 0 ||
				g_tracker_servers.leader_index >=
				g_tracker_servers.server_count)
			{
                logError("file: "__FILE__", line: %d, "
                        "invalid tracker leader index: %d",
                        __LINE__, g_tracker_servers.leader_index);
				g_tracker_servers.leader_index = -1;
				return EINVAL;
			}
		}

        if (g_tracker_servers.leader_index >= 0)
        {
            format_ip_address(conn->ip_addr, formatted_ip);
			logInfo("file: "__FILE__", line: %d, "
				"the tracker leader %s:%u", __LINE__,
				formatted_ip, conn->port);
        }
        else
		{
            format_ip_address(conn->ip_addr, formatted_ip);
			logInfo("file: "__FILE__", line: %d, "
				"waiting for the candidate tracker leader %s:%u notify ...",
                __LINE__, formatted_ip, conn->port);
			return ENOENT;
		}
	}

	return 0;
}

static int relationship_ping_leader()
{
	int result;
	int leader_index;
	TrackerServerInfo *pTrackerServer;
    ConnectionInfo *conn;

	if (g_if_leader_self)
	{
		return 0;  //do not need ping myself
	}

	leader_index = g_tracker_servers.leader_index;
	if (leader_index < 0)
	{
		return EINVAL;
	}

	pTrackerServer = g_tracker_servers.servers + leader_index;
    if ((conn=tracker_connect_server(pTrackerServer, &result)) == NULL)
    {
        return result;
	}

	result = fdfs_ping_leader(conn);
    tracker_close_connection_ex(conn, result != 0);
	return result;
}

static void *relationship_thread_entrance(void* arg)
{
#define MAX_SLEEP_SECONDS  10

	int fail_count;
	int sleep_seconds;
    char formatted_ip[FORMATTED_IP_SIZE];

#ifdef OS_LINUX
    {
        prctl(PR_SET_NAME, "relationship");
    }
#endif

	fail_count = 0;
    sleep_seconds = 1;
	while (SF_G_CONTINUE_FLAG)
	{
		if (g_tracker_servers.servers != NULL)
		{
			if (g_tracker_servers.leader_index < 0)
			{
				if (relationship_select_leader() != 0)
				{
					sleep_seconds = 1 + (int)((double)rand()
					* (double)MAX_SLEEP_SECONDS / RAND_MAX);
				}
                else
                {
                    sleep_seconds = 1;
                }
			}
			else
			{
                int leader_index;
                leader_index = g_tracker_servers.leader_index;
				if (relationship_ping_leader() == 0)
				{
					fail_count = 0;
                    sleep_seconds = 1;
				}
				else
				{
                    char leader_str[64];
                    ConnectionInfo *pLeader;

                    if (leader_index < 0)
                    {
                        strcpy(leader_str, "unknown leader");
                    }
                    else
                    {
                        pLeader = g_tracker_servers.servers
                            [leader_index].connections;
                        format_ip_address(pLeader->ip_addr, formatted_ip);
                        sprintf(leader_str, "leader %s:%u",
                                formatted_ip, pLeader->port);
                    }

                    ++fail_count;
                    logError("file: "__FILE__", line: %d, "
                            "%dth ping %s fail", __LINE__,
                            fail_count, leader_str);

                    sleep_seconds *= 2;
					if (fail_count >= 3)
					{
						g_tracker_servers.leader_index = -1;
						fail_count = 0;
                        sleep_seconds = 1;
					}
				}
			}
		}

		if (g_last_tracker_servers != NULL)
		{
			tracker_mem_file_lock();

			free(g_last_tracker_servers);
			g_last_tracker_servers = NULL;

			tracker_mem_file_unlock();
		}

		sleep(sleep_seconds);
	}

	return NULL;
}

int tracker_relationship_init()
{
	int result;
	pthread_t tid;
	pthread_attr_t thread_attr;

	if ((result=init_pthread_attr(&thread_attr, SF_G_THREAD_STACK_SIZE)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"init_pthread_attr fail, program exit!", __LINE__);
		return result;
	}

	if ((result=pthread_create(&tid, &thread_attr, \
			relationship_thread_entrance, NULL)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"create thread failed, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	pthread_attr_destroy(&thread_attr);

	return 0;
}

int tracker_relationship_destroy()
{
	return 0;
}

