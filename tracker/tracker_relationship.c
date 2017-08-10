/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
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
#include "logger.h"
#include "sockopt.h"
#include "fdfs_global.h"
#include "shared_func.h"
#include "pthread_func.h"
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
	char *pInBuff;
	char *p;
	char *pEnd;
	FDFSGroupInfo *pGroup;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char trunk_server_id[FDFS_STORAGE_ID_MAX_SIZE];

	memset(&header, 0, sizeof(header));
	header.cmd = TRACKER_PROTO_CMD_TRACKER_PING_LEADER;
	result = tcpsenddata_nb(pTrackerServer->sock, &header, \
			sizeof(header), g_fdfs_network_timeout);
	if(result != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server ip: %s, send data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			result, STRERROR(result));
		return result;
	}

	pInBuff = in_buff;
	if ((result=fdfs_recv_response(pTrackerServer, &pInBuff, \
			sizeof(in_buff), &in_bytes)) != 0)
	{
        logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response fail, result: %d",
                __LINE__, result);
		return result;
	}

	if (in_bytes == 0)
	{
		return 0;
	}
	else if (in_bytes % (FDFS_GROUP_NAME_MAX_LEN + \
			FDFS_STORAGE_ID_MAX_SIZE) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server ip: %s, invalid body length: " \
			"%"PRId64, __LINE__, \
			pTrackerServer->ip_addr, in_bytes);
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
		snprintf(pGroup->last_trunk_server_id, sizeof( \
			pGroup->last_trunk_server_id), "%s", trunk_server_id);
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
	ConnectionInfo *pTrackerServer1;
	ConnectionInfo *pTrackerServer2;
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

	pTrackerServer1 = pStatus1->pTrackerServer;
	pTrackerServer2 = pStatus2->pTrackerServer;
	sub = strcmp(pTrackerServer1->ip_addr, pTrackerServer2->ip_addr);
	if (sub != 0)
	{
		return sub;
	}

	return pTrackerServer1->port - pTrackerServer2->port;
}

static int relationship_get_tracker_leader(TrackerRunningStatus *pTrackerStatus)
{
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pTrackerEnd;
	TrackerRunningStatus *pStatus;
	TrackerRunningStatus trackerStatus[FDFS_MAX_TRACKERS];
	int count;
	int result;
	int r;
	int i;

	memset(pTrackerStatus, 0, sizeof(TrackerRunningStatus));
	pStatus = trackerStatus;
	result = 0;
	pTrackerEnd = g_tracker_servers.servers + g_tracker_servers.server_count;
	for (pTrackerServer=g_tracker_servers.servers; \
		pTrackerServer<pTrackerEnd; pTrackerServer++)
	{
		pStatus->pTrackerServer = pTrackerServer;
		r = fdfs_get_tracker_status(pTrackerServer, pStatus);
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

	for (i=0; i<count; i++)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"%s:%d if_leader: %d, running time: %d, " \
			"restart interval: %d", __LINE__, \
			trackerStatus[i].pTrackerServer->ip_addr, \
			trackerStatus[i].pTrackerServer->port, \
			trackerStatus[i].if_leader, \
			trackerStatus[i].running_time, \
			trackerStatus[i].restart_interval);
	}

	memcpy(pTrackerStatus, trackerStatus + (count - 1), \
			sizeof(TrackerRunningStatus));
	return 0;
}

#define relationship_notify_next_leader(pTrackerServer, pLeader, bConnectFail) \
	do_notify_leader_changed(pTrackerServer, pLeader, \
		TRACKER_PROTO_CMD_TRACKER_NOTIFY_NEXT_LEADER, bConnectFail)

#define relationship_commit_next_leader(pTrackerServer, pLeader, bConnectFail) \
	do_notify_leader_changed(pTrackerServer, pLeader, \
		TRACKER_PROTO_CMD_TRACKER_COMMIT_NEXT_LEADER, bConnectFail)

static int do_notify_leader_changed(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pLeader, const char cmd, bool *bConnectFail)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_PROTO_IP_PORT_SIZE];
	char in_buff[1];
	ConnectionInfo *conn;
	TrackerHeader *pHeader;
	char *pInBuff;
	int64_t in_bytes;
	int result;

	pTrackerServer->sock = -1;
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
	pHeader->cmd = cmd;
	sprintf(out_buff + sizeof(TrackerHeader), "%s:%d", \
			pLeader->ip_addr, pLeader->port);
	long2buff(FDFS_PROTO_IP_PORT_SIZE, pHeader->pkg_len);
	if ((result=tcpsenddata_nb(conn->sock, out_buff, \
			sizeof(out_buff), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to tracker server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));

		result = (result == ENOENT ? EACCES : result);
		break;
	}

	pInBuff = in_buff;
	result = fdfs_recv_response(conn, &pInBuff, \
				0, &in_bytes);
	if (result != 0)
	{
        logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response fail, result: %d",
                __LINE__, result);
		break;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d response data " \
			"length: %"PRId64" is invalid, " \
			"expect length: %d.", __LINE__, \
			pTrackerServer->ip_addr, pTrackerServer->port, \
			in_bytes, 0);
		result = EINVAL;
		break;
	}
	} while (0);

	if (pTrackerServer->port == g_server_port && \
		is_local_host_ip(pTrackerServer->ip_addr))
	{
		tracker_disconnect_server_ex(conn, true);
	}
	else
	{
		tracker_disconnect_server_ex(conn, result != 0);
	}

	return result;
}

static int relationship_notify_leader_changed(ConnectionInfo *pLeader)
{
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pTrackerEnd;
	int result;
	bool bConnectFail;
	int success_count;

	result = ENOENT;
	pTrackerEnd = g_tracker_servers.servers + g_tracker_servers.server_count;
	success_count = 0;
	for (pTrackerServer=g_tracker_servers.servers; \
		pTrackerServer<pTrackerEnd; pTrackerServer++)
	{
		if ((result=relationship_notify_next_leader(pTrackerServer, \
				pLeader, &bConnectFail)) != 0)
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
	for (pTrackerServer=g_tracker_servers.servers; \
		pTrackerServer<pTrackerEnd; pTrackerServer++)
	{
		if ((result=relationship_commit_next_leader(pTrackerServer, \
				pLeader, &bConnectFail)) != 0)
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

	if (g_tracker_servers.server_count <= 0)
	{
		return 0;
	}

	logInfo("file: "__FILE__", line: %d, " \
		"selecting leader...", __LINE__);

	if ((result=relationship_get_tracker_leader(&trackerStatus)) != 0)
	{
		return result;
	}

	if (trackerStatus.pTrackerServer->port == g_server_port && \
		is_local_host_ip(trackerStatus.pTrackerServer->ip_addr))
	{
		if ((result=relationship_notify_leader_changed( \
				trackerStatus.pTrackerServer)) != 0)
		{
			return result;
		}

		logInfo("file: "__FILE__", line: %d, " \
			"I am the new tracker leader %s:%d", \
			__LINE__, trackerStatus.pTrackerServer->ip_addr, \
			trackerStatus.pTrackerServer->port);

		tracker_mem_find_trunk_servers();
	}
	else
	{
		if (trackerStatus.if_leader)
		{
			g_tracker_servers.leader_index = \
				trackerStatus.pTrackerServer - \
				g_tracker_servers.servers;
			if (g_tracker_servers.leader_index < 0 || \
				g_tracker_servers.leader_index >= \
				g_tracker_servers.server_count)
			{
                logError("file: "__FILE__", line: %d, "
                        "invalid leader_index: %d",
                        __LINE__, g_tracker_servers.leader_index);
				g_tracker_servers.leader_index = -1;
				return EINVAL;
			}

			logInfo("file: "__FILE__", line: %d, " \
				"the tracker leader %s:%d", __LINE__, \
				trackerStatus.pTrackerServer->ip_addr, \
				trackerStatus.pTrackerServer->port);
		}
		else
		{
			logDebug("file: "__FILE__", line: %d, " \
				"waiting for leader notify", __LINE__);
			return ENOENT;
		}
	}

	return 0;
}

static int relationship_ping_leader()
{
	int result;
	int leader_index;
	ConnectionInfo *pTrackerServer;

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
	if (pTrackerServer->sock < 0)
	{
		if ((result=conn_pool_connect_server(pTrackerServer, \
				g_fdfs_connect_timeout)) != 0)
		{
			return result;
		}
	}

	if ((result=fdfs_ping_leader(pTrackerServer)) != 0)
	{
		close(pTrackerServer->sock);
		pTrackerServer->sock = -1;
	}

	return result;
}

static void *relationship_thread_entrance(void* arg)
{
#define MAX_SLEEP_SECONDS  10

	int fail_count;
	int sleep_seconds;

	fail_count = 0;
	while (g_continue_flag)
	{
		sleep_seconds = 1;
		if (g_tracker_servers.servers != NULL)
		{
			if (g_tracker_servers.leader_index < 0)
			{
				if (relationship_select_leader() != 0)
				{
					sleep_seconds = 1 + (int)((double)rand()
					* (double)MAX_SLEEP_SECONDS / RAND_MAX);
				}
			}
			else
			{
				if (relationship_ping_leader() == 0)
				{
					fail_count = 0;
				}
				else
				{
					fail_count++;
					if (fail_count >= 3)
					{
						g_tracker_servers.leader_index = -1;
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

	if ((result=init_pthread_attr(&thread_attr, g_thread_stack_size)) != 0)
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

