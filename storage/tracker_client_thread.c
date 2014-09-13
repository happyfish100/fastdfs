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
#include <time.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include "fdfs_define.h"
#include "logger.h"
#include "fdfs_global.h"
#include "sockopt.h"
#include "shared_func.h"
#include "pthread_func.h"
#include "sched_thread.h"
#include "fast_task_queue.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "tracker_client_thread.h"
#include "storage_global.h"
#include "storage_sync.h"
#include "storage_func.h"
#include "tracker_client.h"
#include "trunk_mem.h"
#include "trunk_sync.h"
#include "storage_param_getter.h"

#define TRUNK_FILE_CREATOR_TASK_ID   88

static pthread_mutex_t reporter_thread_lock;

/* save report thread ids */
static pthread_t *report_tids = NULL;
static int *src_storage_status = NULL; //returned by tracker server
static signed char *my_report_status = NULL;  //returned by tracker server
static bool need_rejoin_tracker = false;

static int tracker_heart_beat(ConnectionInfo *pTrackerServer, \
		int *pstat_chg_sync_count, bool *bServerPortChanged);
static int tracker_report_df_stat(ConnectionInfo *pTrackerServer, \
		bool *bServerPortChanged);
static int tracker_report_sync_timestamp(ConnectionInfo *pTrackerServer, \
		bool *bServerPortChanged);

static int tracker_sync_dest_req(ConnectionInfo *pTrackerServer);
static int tracker_sync_dest_query(ConnectionInfo *pTrackerServer);
static int tracker_sync_notify(ConnectionInfo *pTrackerServer);
static int tracker_storage_changelog_req(ConnectionInfo *pTrackerServer);
static int tracker_report_trunk_fid(ConnectionInfo *pTrackerServer);
static int tracker_fetch_trunk_fid(ConnectionInfo *pTrackerServer);
static int tracker_report_trunk_free_space(ConnectionInfo *pTrackerServer);

static bool tracker_insert_into_sorted_servers( \
		FDFSStorageServer *pInsertedServer);

int tracker_report_init()
{
	int result;

	memset(g_storage_servers, 0, sizeof(g_storage_servers));
	memset(g_sorted_storages, 0, sizeof(g_sorted_storages));

	if ((result=init_pthread_lock(&reporter_thread_lock)) != 0)
	{
		return result;
	}

	return 0;
}
 
int tracker_report_destroy()
{
	int result;

	if ((result=pthread_mutex_destroy(&reporter_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_destroy fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	return 0;
}

int kill_tracker_report_threads()
{
	int result;
	int kill_res;

	if (report_tids == NULL)
	{
		return 0;
	}

	if ((result=pthread_mutex_lock(&reporter_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	kill_res = kill_work_threads(report_tids, g_tracker_reporter_count);

	if ((result=pthread_mutex_unlock(&reporter_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return kill_res;
}

static void thracker_report_thread_exit(ConnectionInfo *pTrackerServer)
{
	int result;
	int i;
	pthread_t tid;

	if ((result=pthread_mutex_lock(&reporter_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	tid = pthread_self();
	for (i=0; i<g_tracker_group.server_count; i++)
	{
		if (pthread_equal(report_tids[i], tid))
		{
			break;
		}
	}

	while (i < g_tracker_group.server_count - 1)
	{
		report_tids[i] = report_tids[i + 1];
		i++;
	}
	
	g_tracker_reporter_count--;
	if ((result=pthread_mutex_unlock(&reporter_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	logDebug("file: "__FILE__", line: %d, " \
		"report thread to tracker server %s:%d exit", \
		__LINE__, pTrackerServer->ip_addr, pTrackerServer->port);
}

static int tracker_unlink_mark_files(const char *storage_id)
{
	int result;

	result = storage_unlink_mark_file(storage_id);
	result += trunk_unlink_mark_file(storage_id);

	return result;
}

static int tracker_rename_mark_files(const char *old_ip_addr, \
	const int old_port, const char *new_ip_addr, const int new_port)
{
	int result;

	result = storage_rename_mark_file(old_ip_addr, old_port, \
				new_ip_addr, new_port);
	result += trunk_rename_mark_file(old_ip_addr, old_port, \
					new_ip_addr, new_port);
	return result;
}

static void *tracker_report_thread_entrance(void *arg)
{
	ConnectionInfo *pTrackerServer;
	char my_server_id[FDFS_STORAGE_ID_MAX_SIZE];
	char tracker_client_ip[IP_ADDRESS_SIZE];
	char szFailPrompt[36];
	bool sync_old_done;
	int stat_chg_sync_count;
	int sync_time_chg_count;
	time_t current_time;
	time_t last_df_report_time;
	time_t last_sync_report_time;
	time_t last_beat_time;
	int last_trunk_file_id;
	int result;
	int previousCode;
	int nContinuousFail;
	int tracker_index;
	int64_t last_trunk_total_free_space;
	bool bServerPortChanged;

	bServerPortChanged = (g_last_server_port != 0) && \
				(g_server_port != g_last_server_port);

	pTrackerServer = (ConnectionInfo *)arg;
	pTrackerServer->sock = -1;
	tracker_index = pTrackerServer - g_tracker_group.servers;

	logDebug("file: "__FILE__", line: %d, " \
		"report thread to tracker server %s:%d started", \
		__LINE__, pTrackerServer->ip_addr, pTrackerServer->port);

	sync_old_done = g_sync_old_done;
	while (g_continue_flag &&  \
		g_tracker_reporter_count < g_tracker_group.server_count)
	{
		sleep(1); //waiting for all thread started
	}

	result = 0;
	previousCode = 0;
	nContinuousFail = 0;
	while (g_continue_flag)
	{
		if (pTrackerServer->sock >= 0)
		{
			close(pTrackerServer->sock);
		}
		pTrackerServer->sock = socket(AF_INET, SOCK_STREAM, 0);
		if(pTrackerServer->sock < 0)
		{
			logCrit("file: "__FILE__", line: %d, " \
				"socket create failed, errno: %d, " \
				"error info: %s. program exit!", \
				__LINE__, errno, STRERROR(errno));
			g_continue_flag = false;
			break;
		}

		if (g_client_bind_addr && *g_bind_addr != '\0')
		{
			socketBind(pTrackerServer->sock, g_bind_addr, 0);
		}

		tcpsetserveropt(pTrackerServer->sock, g_fdfs_network_timeout);

		if (tcpsetnonblockopt(pTrackerServer->sock) != 0)
		{
			nContinuousFail++;
			sleep(g_heart_beat_interval);
			continue;
		}

		if ((result=connectserverbyip_nb(pTrackerServer->sock, \
			pTrackerServer->ip_addr, \
			pTrackerServer->port, g_fdfs_connect_timeout)) != 0)
		{
			if (previousCode != result)
			{
				logError("file: "__FILE__", line: %d, " \
					"connect to tracker server %s:%d fail" \
					", errno: %d, error info: %s", \
					__LINE__, pTrackerServer->ip_addr, \
					pTrackerServer->port, \
					result, STRERROR(result));
				previousCode = result;
			}

			nContinuousFail++;
			if (g_continue_flag)
			{
				sleep(g_heart_beat_interval);
				continue;
			}
			else
			{
				break;
			}
		}

		getSockIpaddr(pTrackerServer->sock, \
				tracker_client_ip, IP_ADDRESS_SIZE);

		if (nContinuousFail == 0)
		{
			*szFailPrompt = '\0';
		}
		else
		{
			sprintf(szFailPrompt, ", continuous fail count: %d", \
				nContinuousFail);
		}
		logInfo("file: "__FILE__", line: %d, " \
			"successfully connect to tracker server %s:%d%s, " \
			"as a tracker client, my ip is %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, szFailPrompt, tracker_client_ip);

		previousCode = 0;
		nContinuousFail = 0;

		if (*g_tracker_client_ip == '\0')
		{
			strcpy(g_tracker_client_ip, tracker_client_ip);
		}
		else if (strcmp(tracker_client_ip, g_tracker_client_ip) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"as a client of tracker server %s:%d, " \
				"my ip: %s != client ip: %s of other " \
				"tracker client", __LINE__, \
				pTrackerServer->ip_addr, pTrackerServer->port, \
				tracker_client_ip, g_tracker_client_ip);

			close(pTrackerServer->sock);
			pTrackerServer->sock = -1;
			break;
		}

		insert_into_local_host_ip(tracker_client_ip);

		/*
		//printf("file: "__FILE__", line: %d, " \
			"tracker_client_ip: %s, g_my_server_id_str: %s\n", \
			__LINE__, tracker_client_ip, g_my_server_id_str);
		//print_local_host_ip_addrs();
		*/

		if (tracker_report_join(pTrackerServer, tracker_index, \
					sync_old_done) != 0)
		{
			sleep(g_heart_beat_interval);
			continue;
		}

		if (g_http_port != g_last_http_port)
		{
			g_last_http_port = g_http_port;
			if ((result=storage_write_to_sync_ini_file()) != 0)
			{
			}
		}

		if (!sync_old_done)
		{
			if ((result=pthread_mutex_lock(&reporter_thread_lock)) \
					 != 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"call pthread_mutex_lock fail, " \
					"errno: %d, error info: %s", \
					__LINE__, result, STRERROR(result));

				fdfs_quit(pTrackerServer);
				sleep(g_heart_beat_interval);
				continue;
			}

			if (!g_sync_old_done)
			{
				if (tracker_sync_dest_req(pTrackerServer) == 0)
				{
					g_sync_old_done = true;
					if (storage_write_to_sync_ini_file() \
						!= 0)
					{
					logCrit("file: "__FILE__", line: %d, " \
						"storage_write_to_sync_ini_file"\
						"  fail, program exit!", \
						__LINE__);

						g_continue_flag = false;
						pthread_mutex_unlock( \
							&reporter_thread_lock);
						break;
					}
				}
				else //request failed or need to try again
				{
					pthread_mutex_unlock( \
						&reporter_thread_lock);

					fdfs_quit(pTrackerServer);
					sleep(g_heart_beat_interval);
					continue;
				}
			}
			else
			{
				if (tracker_sync_notify(pTrackerServer) != 0)
				{
					pthread_mutex_unlock( \
						&reporter_thread_lock);
					fdfs_quit(pTrackerServer);
					sleep(g_heart_beat_interval);
					continue;
				}
			}

			if ((result=pthread_mutex_unlock(&reporter_thread_lock))
				 != 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"call pthread_mutex_unlock fail, " \
					"errno: %d, error info: %s", \
					__LINE__, result, STRERROR(result));
			}

			sync_old_done = true;
		}

		src_storage_status[tracker_index] = \
					tracker_sync_notify(pTrackerServer);
		if (src_storage_status[tracker_index] != 0)
		{
			int k;
			for (k=0; k<g_tracker_group.server_count; k++)
			{
				if (src_storage_status[k] != ENOENT)
				{
					break;
				}
			}

			if (k == g_tracker_group.server_count)
			{ //src storage server already be deleted
				int my_status;
				if (tracker_get_storage_max_status( \
					&g_tracker_group, g_group_name, \
					g_tracker_client_ip, my_server_id, \
					&my_status) == 0)
				{
					tracker_sync_dest_query(pTrackerServer);
					if(my_status<FDFS_STORAGE_STATUS_OFFLINE
						&& g_sync_old_done)
					{  //need re-sync old files
						pthread_mutex_lock( \
							&reporter_thread_lock);
						g_sync_old_done = false;
						sync_old_done = g_sync_old_done;
						storage_write_to_sync_ini_file();
						pthread_mutex_unlock( \
							&reporter_thread_lock);
					}
				}
			}

			fdfs_quit(pTrackerServer);
			sleep(g_heart_beat_interval);
			continue;
		}

		sync_time_chg_count = 0;
		last_df_report_time = 0;
		last_beat_time = 0;
		last_sync_report_time = 0;
		stat_chg_sync_count = 0;
		last_trunk_file_id = 0;
		last_trunk_total_free_space = -1;

		while (g_continue_flag)
		{
			current_time = g_current_time;
			if (current_time - last_beat_time >= \
					g_heart_beat_interval)
			{
				if (tracker_heart_beat(pTrackerServer, \
					&stat_chg_sync_count, \
					&bServerPortChanged) != 0)
				{
					break;
				}

				if (g_storage_ip_changed_auto_adjust && \
					tracker_storage_changelog_req( \
						pTrackerServer) != 0)
				{
					break;
				}

				last_beat_time = current_time;
			}

			if (sync_time_chg_count != g_sync_change_count && \
				current_time - last_sync_report_time >= \
					g_heart_beat_interval)
			{
				if (tracker_report_sync_timestamp( \
					pTrackerServer, &bServerPortChanged)!=0)
				{
					break;
				}

				sync_time_chg_count = g_sync_change_count;
				last_sync_report_time = current_time;
			}

			if (current_time - last_df_report_time >= \
					g_stat_report_interval)
			{
				if (tracker_report_df_stat(pTrackerServer, \
						&bServerPortChanged) != 0)
				{
					break;
				}

				last_df_report_time = current_time;
			}

			if (g_if_trunker_self)
			{
			if (last_trunk_file_id < g_current_trunk_file_id)
			{
				if (tracker_report_trunk_fid(pTrackerServer)!=0)
				{
					break;
				}
				last_trunk_file_id = g_current_trunk_file_id;
			}

			if (last_trunk_total_free_space != g_trunk_total_free_space)
			{
			if (tracker_report_trunk_free_space(pTrackerServer)!=0)
			{
				break;
			}
			last_trunk_total_free_space = g_trunk_total_free_space;
			}
			}

			if (need_rejoin_tracker)
			{
				need_rejoin_tracker = false;
				break;
			}
			sleep(1);
		}

		if ((!g_continue_flag) && fdfs_quit(pTrackerServer) != 0)
		{
		}

		close(pTrackerServer->sock);
		pTrackerServer->sock = -1;
		if (g_continue_flag)
		{
			sleep(1);
		}
	}

	if (nContinuousFail > 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"connect to tracker server %s:%d fail, try count: %d" \
			", errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, nContinuousFail, \
			result, STRERROR(result));
	}

	thracker_report_thread_exit(pTrackerServer);

	return NULL;
}

static bool tracker_insert_into_sorted_servers( \
		FDFSStorageServer *pInsertedServer)
{
	FDFSStorageServer **ppServer;
	FDFSStorageServer **ppEnd;
	int nCompare;

	ppEnd = g_sorted_storages + g_storage_count;
	for (ppServer=ppEnd; ppServer > g_sorted_storages; ppServer--)
	{
		nCompare = strcmp(pInsertedServer->server.id, \
			   	(*(ppServer-1))->server.id);
		if (nCompare > 0)
		{
			*ppServer = pInsertedServer;
			return true;
		}
		else if (nCompare < 0)
		{
			*ppServer = *(ppServer-1);
		}
		else  //nCompare == 0
		{
			for (; ppServer < ppEnd; ppServer++) //restore
			{
				*ppServer = *(ppServer+1);
			}
			return false;
		}
	}

	*ppServer = pInsertedServer;
	return true;
}

int tracker_sync_diff_servers(ConnectionInfo *pTrackerServer, \
		FDFSStorageBrief *briefServers, const int server_count)
{
	TrackerHeader resp;
	int out_len;
	int result;

	if (server_count == 0)
	{
		return 0;
	}

	memset(&resp, 0, sizeof(resp));
	resp.cmd = TRACKER_PROTO_CMD_STORAGE_REPLICA_CHG;

	out_len = sizeof(FDFSStorageBrief) * server_count;
	long2buff(out_len, resp.pkg_len);
	if ((result=tcpsenddata_nb(pTrackerServer->sock, &resp, sizeof(resp), \
			g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"trackert server %s:%d, send data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, result, STRERROR(result));
		return result;
	}

	if ((result=tcpsenddata_nb(pTrackerServer->sock, \
		briefServers, out_len, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"trackert server %s:%d, send data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, result, STRERROR(result));
		return result;
	}


	if ((result=tcprecvdata_nb(pTrackerServer->sock, &resp, \
			sizeof(resp), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, result, STRERROR(result));
		return result;
	}

	if (memcmp(resp.pkg_len, "\0\0\0\0\0\0\0\0", \
		FDFS_PROTO_PKG_LEN_SIZE) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, " \
			"expect pkg len 0, but recv pkg len != 0", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port);
		return EINVAL;
	}

	return resp.status;
}

int tracker_report_storage_status(ConnectionInfo *pTrackerServer, \
		FDFSStorageBrief *briefServer)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			sizeof(FDFSStorageBrief)];
	TrackerHeader *pHeader;
	TrackerHeader resp;
	int result;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_REPORT_STATUS;

	long2buff(FDFS_GROUP_NAME_MAX_LEN + sizeof(FDFSStorageBrief), \
			pHeader->pkg_len);
	strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
	memcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN, \
			briefServer, sizeof(FDFSStorageBrief));
	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
			sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			sizeof(FDFSStorageBrief), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"trackert server %s:%d, send data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, result, STRERROR(result));
		return result;
	}

	if ((result=tcprecvdata_nb(pTrackerServer->sock, &resp, \
			sizeof(resp), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, result, STRERROR(result));
		return result;
	}

	if (memcmp(resp.pkg_len, "\0\0\0\0\0\0\0\0", \
		FDFS_PROTO_PKG_LEN_SIZE) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, " \
			"expect pkg len 0, but recv pkg len != 0", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port);
		return EINVAL;
	}

	return resp.status;
}

static int tracker_start_sync_threads(const FDFSStorageBrief *pStorage)
{
	int result;

	if (strcmp(pStorage->id, g_my_server_id_str) == 0)
	{
		return 0;
	}

	result = storage_sync_thread_start(pStorage);
	if (result == 0)
	{
		if (g_if_trunker_self)
		{
			result = trunk_sync_thread_start(pStorage);
		}
	}

	return result;
}

static int tracker_merge_servers(ConnectionInfo *pTrackerServer, \
		FDFSStorageBrief *briefServers, const int server_count)
{
	FDFSStorageBrief *pServer;
	FDFSStorageBrief *pEnd;
	FDFSStorageServer *pInsertedServer;
	FDFSStorageServer **ppFound;
	FDFSStorageServer **ppGlobalServer;
	FDFSStorageServer **ppGlobalEnd;
	FDFSStorageServer targetServer;
	FDFSStorageServer *pTargetServer;
	FDFSStorageBrief diffServers[FDFS_MAX_SERVERS_EACH_GROUP];
	FDFSStorageBrief *pDiffServer;
	int res;
	int result;
	int nDeletedCount;

	memset(&targetServer, 0, sizeof(targetServer));
	pTargetServer = &targetServer;

	nDeletedCount = 0;
	pDiffServer = diffServers;
	pEnd = briefServers + server_count;
	for (pServer=briefServers; pServer<pEnd; pServer++)
	{
		memcpy(&(targetServer.server),pServer,sizeof(FDFSStorageBrief));

		ppFound = (FDFSStorageServer **)bsearch(&pTargetServer, \
			g_sorted_storages, g_storage_count, \
			sizeof(FDFSStorageServer *), storage_cmp_by_server_id);
		if (ppFound != NULL)
		{
			if (g_use_storage_id)
			{
			strcpy((*ppFound)->server.ip_addr, pServer->ip_addr);
			}

			/*
			//logInfo("ip_addr=%s, local status: %d, " \
				"tracker status: %d", pServer->ip_addr, \
				(*ppFound)->server.status, pServer->status);
			*/
			if ((*ppFound)->server.status == pServer->status)
			{
				continue;
			}

			if (pServer->status == FDFS_STORAGE_STATUS_OFFLINE)
			{
				if ((*ppFound)->server.status == \
						FDFS_STORAGE_STATUS_ACTIVE
				 || (*ppFound)->server.status == \
						FDFS_STORAGE_STATUS_ONLINE)
				{
					(*ppFound)->server.status = \
					FDFS_STORAGE_STATUS_OFFLINE;
				}
				else if ((*ppFound)->server.status != \
						FDFS_STORAGE_STATUS_NONE
				     && (*ppFound)->server.status != \
						FDFS_STORAGE_STATUS_INIT)
				{
					memcpy(pDiffServer++, \
						&((*ppFound)->server), \
						sizeof(FDFSStorageBrief));
				}
			}
			else if ((*ppFound)->server.status == \
					FDFS_STORAGE_STATUS_OFFLINE)
			{
				(*ppFound)->server.status = pServer->status;
			}
			else if ((*ppFound)->server.status == \
					FDFS_STORAGE_STATUS_NONE)
			{
				if (pServer->status == \
					FDFS_STORAGE_STATUS_DELETED \
				 || pServer->status == \
					FDFS_STORAGE_STATUS_IP_CHANGED)
				{ //ignore
				}
				else
				{
					(*ppFound)->server.status = \
							pServer->status;
					if ((result=tracker_start_sync_threads(\
						&((*ppFound)->server))) != 0)
					{
						return result;
					}
				}
			}
			else if (((pServer->status == \
					FDFS_STORAGE_STATUS_WAIT_SYNC) || \
				(pServer->status == \
					FDFS_STORAGE_STATUS_SYNCING)) && \
				((*ppFound)->server.status > pServer->status))
			{
                *(pServer->ip_addr + IP_ADDRESS_SIZE - 1) = '\0';
				if (is_local_host_ip(pServer->ip_addr) && \
					buff2int(pServer->port) == g_server_port)
				{
					need_rejoin_tracker = true;
					logWarning("file: "__FILE__", line: %d, " \
						"tracker response status: %d, " \
						"local status: %d, need rejoin " \
						"tracker server: %s:%d", \
						__LINE__, pServer->status, \
						(*ppFound)->server.status, \
						pTrackerServer->ip_addr,
						pTrackerServer->port);
				}

				memcpy(pDiffServer++, &((*ppFound)->server), \
					sizeof(FDFSStorageBrief));
			}
			else
			{
				(*ppFound)->server.status = pServer->status;
			}
		}
		else if (pServer->status == FDFS_STORAGE_STATUS_DELETED
		      || pServer->status == FDFS_STORAGE_STATUS_IP_CHANGED)
		{   //ignore
			nDeletedCount++;
		}
		else
		{
			/*
			//logInfo("ip_addr=%s, tracker status: %d", 
				pServer->ip_addr, pServer->status);
			*/

			if ((res=pthread_mutex_lock( \
				 &reporter_thread_lock)) != 0)
			{
				logError("file: "__FILE__", line: %d, "\
					"call pthread_mutex_lock fail,"\
					" errno: %d, error info: %s", \
					__LINE__, res, STRERROR(res));
			}

			if (g_storage_count < FDFS_MAX_SERVERS_EACH_GROUP)
			{
				pInsertedServer = g_storage_servers + \
						g_storage_count;
				memcpy(&(pInsertedServer->server), \
					pServer, sizeof(FDFSStorageBrief));
				if (tracker_insert_into_sorted_servers( \
						pInsertedServer))
				{
					g_storage_count++;

					result = tracker_start_sync_threads( \
						&(pInsertedServer->server));
				}
				else
				{
					result = 0;
				}
			}
			else
			{
				logError("file: "__FILE__", line: %d, " \
					"tracker server %s:%d, " \
					"storage servers of group \"%s\" " \
					"exceeds max: %d", \
					__LINE__, pTrackerServer->ip_addr, \
					pTrackerServer->port, g_group_name, \
					FDFS_MAX_SERVERS_EACH_GROUP);
				result = ENOSPC;
			}

			if ((res=pthread_mutex_unlock( \
				&reporter_thread_lock)) != 0)
			{
				logError("file: "__FILE__", line: %d, "\
				"call pthread_mutex_unlock fail, " \
				"errno: %d, error info: %s", \
				__LINE__, res, STRERROR(res));
			}

			if (result != 0)
			{
				return result;
			}
		}
	}

	if (g_storage_count + nDeletedCount == server_count)
	{
		if (pDiffServer - diffServers > 0)
		{
			return tracker_sync_diff_servers(pTrackerServer, \
				diffServers, pDiffServer - diffServers);
		}

		return 0;
	}

	ppGlobalServer = g_sorted_storages;
	ppGlobalEnd = g_sorted_storages + g_storage_count;
	pServer = briefServers;
	while (pServer < pEnd && ppGlobalServer < ppGlobalEnd)
	{
		if ((*ppGlobalServer)->server.status == FDFS_STORAGE_STATUS_NONE)
		{
			ppGlobalServer++;
			continue;
		}

		res = strcmp(pServer->id, (*ppGlobalServer)->server.id);
		if (res < 0)
		{
			if (pServer->status != FDFS_STORAGE_STATUS_DELETED
			 && pServer->status != FDFS_STORAGE_STATUS_IP_CHANGED)
			{
				logError("file: "__FILE__", line: %d, " \
					"tracker server %s:%d, " \
					"group \"%s\", " \
					"enter impossible statement branch", \
					__LINE__, pTrackerServer->ip_addr, \
					pTrackerServer->port, g_group_name);
			}

			pServer++;
		}
		else if (res == 0)
		{
			pServer++;
			ppGlobalServer++;
		}
		else
		{
			memcpy(pDiffServer++, &((*ppGlobalServer)->server), \
				sizeof(FDFSStorageBrief));
			ppGlobalServer++;
		}
	}

	while (ppGlobalServer < ppGlobalEnd)
	{
		if ((*ppGlobalServer)->server.status == FDFS_STORAGE_STATUS_NONE)
		{
			ppGlobalServer++;
			continue;
		}

		memcpy(pDiffServer++, &((*ppGlobalServer)->server), \
			sizeof(FDFSStorageBrief));
		ppGlobalServer++;
	}

	return tracker_sync_diff_servers(pTrackerServer, \
			diffServers, pDiffServer - diffServers);
}

static int _notify_reselect_tleader(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader)];
	TrackerHeader *pHeader;
	int64_t in_bytes;
	int result;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	pHeader->cmd = TRACKER_PROTO_CMD_TRACKER_NOTIFY_RESELECT_LEADER;
	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
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

	if ((result=fdfs_recv_header(pTrackerServer, &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv body length: " \
			"%"PRId64" != 0",  __LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

static int notify_reselect_tracker_leader(ConnectionInfo *pTrackerServer)
{
    int result;
    ConnectionInfo *conn;

	pTrackerServer->sock = -1;
	if ((conn=tracker_connect_server(pTrackerServer, &result)) == NULL)
	{
		return result;
	}

    result = _notify_reselect_tleader(pTrackerServer);
	tracker_disconnect_server_ex(conn, result != 0);
    return result;
}

static void set_tracker_leader(const int leader_index)
{
    int old_index;
    old_index = g_tracker_group.leader_index;
    if (old_index >= 0 && old_index != leader_index)
    {
        TrackerRunningStatus tracker_status;
        ConnectionInfo old_leader_server;
        memcpy(&old_leader_server, g_tracker_group.servers + old_index,
                sizeof(ConnectionInfo));
        if (fdfs_get_tracker_status(&old_leader_server, &tracker_status) == 0)
        {
            if (tracker_status.if_leader)
            {
                ConnectionInfo new_leader_server;
                memcpy(&new_leader_server, g_tracker_group.servers + leader_index,
                        sizeof(ConnectionInfo));
                logWarning("file: "__FILE__", line: %d, "
                        "two tracker leaders occur, old leader is %s:%d, "
                        "new leader is %s:%d, notify to re-select "
                        "tracker leader", __LINE__,
                        old_leader_server.ip_addr, old_leader_server.port,
                        new_leader_server.ip_addr, new_leader_server.port);

                notify_reselect_tracker_leader(&old_leader_server);
                notify_reselect_tracker_leader(&new_leader_server);
                g_tracker_group.leader_index = -1;
                return;
            }
        }
    }
	g_tracker_group.leader_index = leader_index;
}

static int tracker_check_response(ConnectionInfo *pTrackerServer, \
	bool *bServerPortChanged)
{
	int64_t nInPackLen;
	TrackerHeader resp;
	int server_count;
	int result;
	char in_buff[1 + (2 + FDFS_MAX_SERVERS_EACH_GROUP) * \
			sizeof(FDFSStorageBrief)];
	FDFSStorageBrief *pBriefServers;
	char *pFlags;

	if ((result=tcprecvdata_nb(pTrackerServer->sock, &resp, \
			sizeof(resp), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port,    \
			result, STRERROR(result));
		return result;
	}

	//printf("resp status=%d\n", resp.status);
	if (resp.status != 0)
	{
		return resp.status;
	}

	nInPackLen = buff2long(resp.pkg_len);
	if (nInPackLen == 0)
	{
		return 0;
	}

	if ((nInPackLen <= 0) || ((nInPackLen - 1) % \
			sizeof(FDFSStorageBrief) != 0))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, " \
			"package size %"PRId64" is not correct", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, nInPackLen);
		return EINVAL;
	}

	if (nInPackLen > sizeof(in_buff))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, package size " \
			"%"PRId64" is too large, " \
			"exceed max: %d", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			nInPackLen, (int)sizeof(in_buff));
		return EINVAL;
	}

	if ((result=tcprecvdata_nb(pTrackerServer->sock, in_buff, \
			nInPackLen, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	pFlags = in_buff;
	server_count = (nInPackLen - 1) / sizeof(FDFSStorageBrief);
	pBriefServers = (FDFSStorageBrief *)(in_buff + 1);

	if ((*pFlags) & FDFS_CHANGE_FLAG_TRACKER_LEADER)
	{
		char tracker_leader_ip[IP_ADDRESS_SIZE];
		int tracker_leader_port;

		if (server_count < 1)
		{
			logError("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, reponse server " \
				"count: %d < 1", __LINE__, \
				pTrackerServer->ip_addr, \
				pTrackerServer->port, server_count);
			return EINVAL;
		}

		memcpy(tracker_leader_ip, pBriefServers->ip_addr, \
			IP_ADDRESS_SIZE - 1);
		*(tracker_leader_ip + (IP_ADDRESS_SIZE - 1)) = '\0';
		tracker_leader_port = buff2int(pBriefServers->port);

		if (*tracker_leader_ip == '\0')
		{
			if (g_tracker_group.leader_index >= 0)
			{
			ConnectionInfo *pTrackerLeader;
			pTrackerLeader = g_tracker_group.servers + \
					g_tracker_group.leader_index;
			logWarning("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, " \
				"my tracker leader is: %s:%d, " \
				"but reponse tracker leader is null", \
 				__LINE__, pTrackerServer->ip_addr, \
				pTrackerServer->port, pTrackerLeader->ip_addr, \
				pTrackerLeader->port);

			g_tracker_group.leader_index = -1;
			}
		}
		else
		{
			int leader_index;

			leader_index = fdfs_get_tracker_leader_index( \
					tracker_leader_ip, tracker_leader_port);
			if (leader_index < 0)
			{
			logWarning("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, " \
				"reponse tracker leader: %s:%d" \
				" not exist in local", __LINE__, \
				pTrackerServer->ip_addr, \
				pTrackerServer->port, tracker_leader_ip, \
				tracker_leader_port);
			}
			else
			{
			logInfo("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, " \
				"set tracker leader: %s:%d", __LINE__, \
				pTrackerServer->ip_addr, pTrackerServer->port,\
				tracker_leader_ip, tracker_leader_port);

            pthread_mutex_lock(&reporter_thread_lock);
            set_tracker_leader(leader_index);
            pthread_mutex_unlock(&reporter_thread_lock);
			}
		}

		pBriefServers += 1;
		server_count -= 1;
	}

	if ((*pFlags) & FDFS_CHANGE_FLAG_TRUNK_SERVER)
	{
		if (server_count < 1)
		{
			logError("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, reponse server " \
				"count: %d < 1", __LINE__, \
				pTrackerServer->ip_addr, \
				pTrackerServer->port, server_count);
			return EINVAL;
		}

		if (!g_if_use_trunk_file)
		{
			logInfo("file: "__FILE__", line: %d, " \
				"reload parameters from tracker server", \
				__LINE__);
			storage_get_params_from_tracker();
		}

		if (!g_if_use_trunk_file)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, " \
				"my g_if_use_trunk_file is false, " \
				"can't support trunk server!", \
				__LINE__, pTrackerServer->ip_addr, \
				pTrackerServer->port);
		}
		else
		{
		memcpy(g_trunk_server.ip_addr, pBriefServers->ip_addr, \
			IP_ADDRESS_SIZE - 1);
		*(g_trunk_server.ip_addr + (IP_ADDRESS_SIZE - 1)) = '\0';
		g_trunk_server.port = buff2int(pBriefServers->port);
		if (is_local_host_ip(g_trunk_server.ip_addr) && \
			g_trunk_server.port == g_server_port)
		{
			if (g_if_trunker_self)
			{
			logWarning("file: "__FILE__", line: %d, " \
				"I am already the trunk server %s:%d, " \
				"may be the tracker server restart", \
				__LINE__, g_trunk_server.ip_addr, \
				g_trunk_server.port);
			}
			else
			{
			logInfo("file: "__FILE__", line: %d, " \
				"I am the the trunk server %s:%d", __LINE__, \
				g_trunk_server.ip_addr, g_trunk_server.port);

			tracker_fetch_trunk_fid(pTrackerServer);
			g_if_trunker_self = true;

			if ((result=storage_trunk_init()) != 0)
			{
				return result;
			}

			if (g_trunk_create_file_advance && \
				g_trunk_create_file_interval > 0)
			{
			ScheduleArray scheduleArray;
			ScheduleEntry entries[1];

			entries[0].id = TRUNK_FILE_CREATOR_TASK_ID;
			entries[0].time_base = g_trunk_create_file_time_base;
			entries[0].interval = g_trunk_create_file_interval;
			entries[0].task_func = trunk_create_trunk_file_advance;
			entries[0].func_args = NULL;

			scheduleArray.count = 1;
			scheduleArray.entries = entries;
			sched_add_entries(&scheduleArray);
			}

			trunk_sync_thread_start_all();
			}
		}
		else
		{
			logInfo("file: "__FILE__", line: %d, " \
				"the trunk server is %s:%d", __LINE__, \
				g_trunk_server.ip_addr, g_trunk_server.port);

			if (g_if_trunker_self)
			{
				int saved_trunk_sync_thread_count;

				logWarning("file: "__FILE__", line: %d, " \
					"I am the old trunk server, " \
					"the new trunk server is %s:%d", \
					__LINE__, g_trunk_server.ip_addr, \
					g_trunk_server.port);

				tracker_report_trunk_fid(pTrackerServer);
				g_if_trunker_self = false;

				saved_trunk_sync_thread_count = \
						g_trunk_sync_thread_count;
				if (saved_trunk_sync_thread_count > 0)
				{
					logInfo("file: "__FILE__", line: %d, "\
						"waiting %d trunk sync " \
						"threads exit ...", __LINE__, \
						saved_trunk_sync_thread_count);
				}

				while (g_trunk_sync_thread_count > 0)
				{
					usleep(50000);
				}

				if (saved_trunk_sync_thread_count > 0)
				{
					logInfo("file: "__FILE__", line: %d, " \
						"%d trunk sync threads exited",\
						__LINE__, \
						saved_trunk_sync_thread_count);
				}
				
				storage_trunk_destroy_ex(true);
				if (g_trunk_create_file_advance && \
					g_trunk_create_file_interval > 0)
				{
				sched_del_entry(TRUNK_FILE_CREATOR_TASK_ID);
				}
			}
		}
		}

		pBriefServers += 1;
		server_count -= 1;
	}

	if (!((*pFlags) & FDFS_CHANGE_FLAG_GROUP_SERVER))
	{
		return 0;
	}

	/*
	//printf("resp server count=%d\n", server_count);
	{
		int i;
		for (i=0; i<server_count; i++)
		{	
			//printf("%d. %d:%s\n", i+1, pBriefServers[i].status, \
				pBriefServers[i].ip_addr);
		}
	}
	*/

	if (*bServerPortChanged)
	{
		if (!g_use_storage_id)
		{
			FDFSStorageBrief *pStorageEnd;
			FDFSStorageBrief *pStorage;

			*bServerPortChanged = false;
			pStorageEnd = pBriefServers + server_count;
			for (pStorage=pBriefServers; pStorage<pStorageEnd; 
				pStorage++)
			{
				if (strcmp(pStorage->id, g_my_server_id_str) == 0)
				{
					continue;
				}

				tracker_rename_mark_files(pStorage->ip_addr, \
					g_last_server_port, pStorage->ip_addr, \
					g_server_port);
			}
		}

		if (g_server_port != g_last_server_port)
		{
			g_last_server_port = g_server_port;
			if ((result=storage_write_to_sync_ini_file()) != 0)
			{
				return result;
			}
		}
	}

	return tracker_merge_servers(pTrackerServer, \
                pBriefServers, server_count);
}

int tracker_sync_src_req(ConnectionInfo *pTrackerServer, \
			StorageBinLogReader *pReader)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			FDFS_STORAGE_ID_MAX_SIZE];
	char sync_src_id[FDFS_STORAGE_ID_MAX_SIZE];
	TrackerHeader *pHeader;
	TrackerStorageSyncReqBody syncReqbody;
	char *pBuff;
	int64_t in_bytes;
	int result;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	long2buff(FDFS_GROUP_NAME_MAX_LEN + FDFS_STORAGE_ID_MAX_SIZE, \
		pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_SYNC_SRC_REQ;
	strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
	strcpy(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN, \
		pReader->storage_id);
	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
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

	pBuff = (char *)&syncReqbody;
	if ((result=fdfs_recv_response(pTrackerServer, \
                &pBuff, sizeof(syncReqbody), &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes == 0)
	{
		pReader->need_sync_old = false;
        	pReader->until_timestamp = 0;

		return 0;
	}

	if (in_bytes != sizeof(syncReqbody))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, " \
			"recv body length: %"PRId64" is invalid, " \
			"expect body length: %d", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes, \
			(int)sizeof(syncReqbody));
		return EINVAL;
	}

	memcpy(sync_src_id, syncReqbody.src_id, FDFS_STORAGE_ID_MAX_SIZE);
	sync_src_id[FDFS_STORAGE_ID_MAX_SIZE - 1] = '\0';

	pReader->need_sync_old = storage_id_is_myself(sync_src_id);
       	pReader->until_timestamp = (time_t)buff2long( \
					syncReqbody.until_timestamp);

	return 0;
}

static int tracker_sync_dest_req(ConnectionInfo *pTrackerServer)
{
	TrackerHeader header;
	TrackerStorageSyncReqBody syncReqbody;
	char *pBuff;
	int64_t in_bytes;
	int result;

	memset(&header, 0, sizeof(header));
	header.cmd = TRACKER_PROTO_CMD_STORAGE_SYNC_DEST_REQ;
	if ((result=tcpsenddata_nb(pTrackerServer->sock, &header, \
			sizeof(header), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	pBuff = (char *)&syncReqbody;
	if ((result=fdfs_recv_response(pTrackerServer, \
                &pBuff, sizeof(syncReqbody), &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes == 0)
	{
		return result;
	}

	if (in_bytes != sizeof(syncReqbody))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, " \
			"recv body length: %"PRId64" is invalid, " \
			"expect body length: %d", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes, \
			(int)sizeof(syncReqbody));
		return EINVAL;
	}

	memcpy(g_sync_src_id, syncReqbody.src_id, FDFS_STORAGE_ID_MAX_SIZE);
	g_sync_src_id[FDFS_STORAGE_ID_MAX_SIZE - 1] = '\0';

	g_sync_until_timestamp = (time_t)buff2long(syncReqbody.until_timestamp);

	return 0;
}

static int tracker_sync_dest_query(ConnectionInfo *pTrackerServer)
{
	TrackerHeader header;
	TrackerStorageSyncReqBody syncReqbody;
	char *pBuff;
	int64_t in_bytes;
	int result;

	memset(&header, 0, sizeof(header));
	header.cmd = TRACKER_PROTO_CMD_STORAGE_SYNC_DEST_QUERY;
	if ((result=tcpsenddata_nb(pTrackerServer->sock, &header, \
			sizeof(header), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	pBuff = (char *)&syncReqbody;
	if ((result=fdfs_recv_response(pTrackerServer, \
                &pBuff, sizeof(syncReqbody), &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes == 0)
	{
		*g_sync_src_id = '\0';
		g_sync_until_timestamp = 0;
		return result;
	}

	if (in_bytes != sizeof(syncReqbody))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, " \
			"recv body length: %"PRId64" is invalid, " \
			"expect body length: %d", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes, \
			(int)sizeof(syncReqbody));
		return EINVAL;
	}

	memcpy(g_sync_src_id, syncReqbody.src_id, FDFS_STORAGE_ID_MAX_SIZE);
	g_sync_src_id[FDFS_STORAGE_ID_MAX_SIZE - 1] = '\0';

	g_sync_until_timestamp = (time_t)buff2long(syncReqbody.until_timestamp);
	return 0;
}

static int tracker_report_trunk_fid(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader)+sizeof(int)];
	TrackerHeader *pHeader;
	int64_t in_bytes;
	int result;

	pHeader = (TrackerHeader *)out_buff;

	memset(out_buff, 0, sizeof(out_buff));
	long2buff((int)sizeof(int), pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_REPORT_TRUNK_FID;
	int2buff(g_current_trunk_file_id, out_buff + sizeof(TrackerHeader));

	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
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

	if ((result=fdfs_recv_header(pTrackerServer, &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv body length: " \
			"%"PRId64" != 0",  \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

static int tracker_report_trunk_free_space(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader) + 8];
	TrackerHeader *pHeader;
	int64_t in_bytes;
	int result;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	long2buff(8, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_REPORT_TRUNK_FREE;
	long2buff(g_trunk_total_free_space / FDFS_ONE_MB, \
		out_buff + sizeof(TrackerHeader));
	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
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

	if ((result=fdfs_recv_header(pTrackerServer, &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv body length: " \
			"%"PRId64" != 0",  \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

static int tracker_fetch_trunk_fid(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader)];
	char in_buff[4];
	TrackerHeader *pHeader;
	char *pInBuff;
	int64_t in_bytes;
	int trunk_fid;
	int result;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_FETCH_TRUNK_FID;
	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
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

	pInBuff = in_buff;
	if ((result=fdfs_recv_response(pTrackerServer, \
		&pInBuff, sizeof(in_buff), &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes != sizeof(in_buff))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv body length: " \
			"%"PRId64" != %d",  \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes, (int)sizeof(in_buff));
		return EINVAL;
	}

	trunk_fid = buff2int(in_buff);
	if (trunk_fid < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, " \
			"trunk file id: %d is invalid!", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, trunk_fid);
		return EINVAL;
	}

	if (g_current_trunk_file_id < trunk_fid)
	{
		logInfo("file: "__FILE__", line: %d, " \
			"old trunk file id: %d, " \
			"change to new trunk file id: %d", \
			__LINE__, g_current_trunk_file_id, trunk_fid);
	
		g_current_trunk_file_id = trunk_fid;
		storage_write_to_sync_ini_file();
	}

	return 0;
}

static int tracker_sync_notify(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader)+sizeof(TrackerStorageSyncReqBody)];
	TrackerHeader *pHeader;
	TrackerStorageSyncReqBody *pReqBody;
	int64_t in_bytes;
	int result;

	pHeader = (TrackerHeader *)out_buff;
	pReqBody = (TrackerStorageSyncReqBody*)(out_buff+sizeof(TrackerHeader));

	memset(out_buff, 0, sizeof(out_buff));
	long2buff((int)sizeof(TrackerStorageSyncReqBody), pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_SYNC_NOTIFY;
	strcpy(pReqBody->src_id, g_sync_src_id);
	long2buff(g_sync_until_timestamp, pReqBody->until_timestamp);

	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
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

	if ((result=fdfs_recv_header(pTrackerServer, &in_bytes)) != 0)
	{
		return result;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv body length: " \
			"%"PRId64" != 0",  \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

int tracker_report_join(ConnectionInfo *pTrackerServer, \
			const int tracker_index, const bool sync_old_done)
{
	char out_buff[sizeof(TrackerHeader) + sizeof(TrackerStorageJoinBody) + \
			FDFS_MAX_TRACKERS * FDFS_PROTO_IP_PORT_SIZE];
	TrackerHeader *pHeader;
	TrackerStorageJoinBody *pReqBody;
	TrackerStorageJoinBodyResp respBody;
	char *pInBuff;
	char *p;
	ConnectionInfo *pServer;
	ConnectionInfo *pServerEnd;
	FDFSStorageServer *pTargetServer;
	FDFSStorageServer **ppFound;
	FDFSStorageServer targetServer;
	int out_len;
	//int tracker_count;
	int result;
	int i;
	int64_t in_bytes;

	pHeader = (TrackerHeader *)out_buff;
	pReqBody = (TrackerStorageJoinBody *)(out_buff+sizeof(TrackerHeader));

	memset(out_buff, 0, sizeof(out_buff));
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_JOIN;
	strcpy(pReqBody->group_name, g_group_name);
	strcpy(pReqBody->domain_name, g_http_domain);
	snprintf(pReqBody->version, sizeof(pReqBody->version), "%d.%02d", \
		g_fdfs_version.major, g_fdfs_version.minor);
	long2buff(g_server_port, pReqBody->storage_port);
	long2buff(g_http_port, pReqBody->storage_http_port);
	long2buff(g_fdfs_store_paths.count, pReqBody->store_path_count);
	long2buff(g_subdir_count_per_path, pReqBody->subdir_count_per_path);
	long2buff(g_upload_priority, pReqBody->upload_priority);
	long2buff(g_storage_join_time, pReqBody->join_time);
	long2buff(g_up_time, pReqBody->up_time);
	pReqBody->init_flag = sync_old_done ? 0 : 1;

	memset(&targetServer, 0, sizeof(targetServer));
	pTargetServer = &targetServer;

	strcpy(targetServer.server.id, g_my_server_id_str);
	ppFound = (FDFSStorageServer **)bsearch(&pTargetServer, \
			g_sorted_storages, g_storage_count, \
			sizeof(FDFSStorageServer *), storage_cmp_by_server_id);
	if (ppFound != NULL)
	{
		pReqBody->status = (*ppFound)->server.status;
	}
	else
	{
		if (g_tracker_group.server_count > 1)
		{
			for (i=0; i<g_tracker_group.server_count; i++)
			{
				if (my_report_status[i] != EFAULT)
				{
					break;
				}
			}

			if (i == g_tracker_group.server_count)
			{
				pReqBody->status = FDFS_STORAGE_STATUS_INIT;
			}
			else
			{
				pReqBody->status = -1;
			}
		}
		else
		{
			pReqBody->status = FDFS_STORAGE_STATUS_INIT;
		}
	}

	//tracker_count = 0;
	p = out_buff + sizeof(TrackerHeader) + sizeof(TrackerStorageJoinBody);
	pServerEnd = g_tracker_group.servers + g_tracker_group.server_count;
	for (pServer=g_tracker_group.servers; pServer<pServerEnd; pServer++)
	{
		/*
		if (strcmp(pServer->ip_addr, pTrackerServer->ip_addr) == 0 && \
			pServer->port == pTrackerServer->port)
		{
			continue;
		}
		tracker_count++;
		*/

		sprintf(p, "%s:%d", pServer->ip_addr, pServer->port);
		p += FDFS_PROTO_IP_PORT_SIZE;
	}

	out_len = p - out_buff;
	long2buff(g_tracker_group.server_count, pReqBody->tracker_count);
	long2buff(out_len - (int)sizeof(TrackerHeader), pHeader->pkg_len);

	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
			out_len, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

        pInBuff = (char *)&respBody;
	result = fdfs_recv_response(pTrackerServer, \
			&pInBuff, sizeof(respBody), &in_bytes);
	my_report_status[tracker_index] = result;
	if (result != 0)
	{
		return result;
	}

	if (in_bytes != sizeof(respBody))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv data fail, " \
			"expect %d bytes, but recv " \
			"%"PRId64" bytes", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			(int)sizeof(respBody), in_bytes);
		my_report_status[tracker_index] = EINVAL;
		return EINVAL;
	}

	if (*(respBody.src_id) == '\0' && *g_sync_src_id != '\0')
	{
		return tracker_sync_notify(pTrackerServer);
	}
	else
	{
		return 0;
	}
}

static int tracker_report_sync_timestamp(ConnectionInfo *pTrackerServer, \
		bool *bServerPortChanged)
{
	char out_buff[sizeof(TrackerHeader) + (FDFS_STORAGE_ID_MAX_SIZE + 4) * \
			FDFS_MAX_SERVERS_EACH_GROUP];
	char *p;
	TrackerHeader *pHeader;
	FDFSStorageServer *pServer;
	FDFSStorageServer *pEnd;
	int result;
	int body_len;

	if (g_storage_count == 0)
	{
		return 0;
	}

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);

	body_len = (FDFS_STORAGE_ID_MAX_SIZE + 4) * g_storage_count;
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_SYNC_REPORT;
	long2buff(body_len, pHeader->pkg_len);

	pEnd = g_storage_servers + g_storage_count;
	for (pServer=g_storage_servers; pServer<pEnd; pServer++)
	{
		memcpy(p, pServer->server.id, FDFS_STORAGE_ID_MAX_SIZE);
		p += FDFS_STORAGE_ID_MAX_SIZE;
		int2buff(pServer->last_sync_src_timestamp, p);
		p += 4;
	}

	if((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
		sizeof(TrackerHeader) + body_len, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	return tracker_check_response(pTrackerServer, bServerPortChanged);
}

static int tracker_report_df_stat(ConnectionInfo *pTrackerServer, \
		bool *bServerPortChanged)
{
	char out_buff[sizeof(TrackerHeader) + \
			sizeof(TrackerStatReportReqBody) * 16];
	char *pBuff;
	TrackerHeader *pHeader;
	TrackerStatReportReqBody *pStatBuff;
	struct statvfs sbuf;
	int body_len;
	int total_len;
	int store_path_index;
	int i;
	int result;

	body_len = (int)sizeof(TrackerStatReportReqBody) * g_fdfs_store_paths.count;
	total_len = (int)sizeof(TrackerHeader) + body_len;
	if (total_len <= sizeof(out_buff))
	{
		pBuff = out_buff;
	}
	else
	{
		pBuff = (char *)malloc(total_len);
		if (pBuff == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", \
				__LINE__, total_len, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : ENOMEM;
		}
	}

	pHeader = (TrackerHeader *)pBuff;
	pStatBuff = (TrackerStatReportReqBody*) \
			(pBuff + sizeof(TrackerHeader));
	long2buff(body_len, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_REPORT_DISK_USAGE;
	pHeader->status = 0;

	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		if (statvfs(g_fdfs_store_paths.paths[i], &sbuf) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"call statfs fail, errno: %d, error info: %s.",\
				__LINE__, errno, STRERROR(errno));

			if (pBuff != out_buff)
			{
				free(pBuff);
			}
			return errno != 0 ? errno : EACCES;
		}

		g_path_space_list[i].total_mb = ((int64_t)(sbuf.f_blocks) * \
					sbuf.f_frsize) / FDFS_ONE_MB;
		g_path_space_list[i].free_mb = ((int64_t)(sbuf.f_bavail) * \
					sbuf.f_frsize) / FDFS_ONE_MB;
		long2buff(g_path_space_list[i].total_mb, pStatBuff->sz_total_mb);
		long2buff(g_path_space_list[i].free_mb, pStatBuff->sz_free_mb);

		pStatBuff++;
	}

	if (g_store_path_mode == FDFS_STORE_PATH_LOAD_BALANCE)
	{
		int max_free_mb;

		/* find the max free space path */
		max_free_mb = 0;
		store_path_index = -1;
		for (i=0; i<g_fdfs_store_paths.count; i++)
		{
			if (g_path_space_list[i].free_mb > \
				g_avg_storage_reserved_mb \
				&& g_path_space_list[i].free_mb > max_free_mb)
			{
				store_path_index = i;
				max_free_mb = g_path_space_list[i].free_mb;
			}
		}
		if (g_store_path_index != store_path_index)
		{
			g_store_path_index = store_path_index;
		}
	}

	result = tcpsenddata_nb(pTrackerServer->sock, pBuff, \
			total_len, g_fdfs_network_timeout);
	if (pBuff != out_buff)
	{
		free(pBuff);
	}
	if(result != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	return tracker_check_response(pTrackerServer, bServerPortChanged);
}

static int tracker_heart_beat(ConnectionInfo *pTrackerServer, \
		int *pstat_chg_sync_count, bool *bServerPortChanged)
{
	char out_buff[sizeof(TrackerHeader) + sizeof(FDFSStorageStatBuff)];
	TrackerHeader *pHeader;
	FDFSStorageStatBuff *pStatBuff;
	int body_len;
	int result;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	if (*pstat_chg_sync_count != g_stat_change_count)
	{
		pStatBuff = (FDFSStorageStatBuff *)( \
				out_buff + sizeof(TrackerHeader));

		int2buff(free_queue_alloc_connections(), \
			pStatBuff->connection.sz_alloc_count);
		int2buff(g_storage_stat.connection.current_count, \
			pStatBuff->connection.sz_current_count);
		int2buff(g_storage_stat.connection.max_count, \
			pStatBuff->connection.sz_max_count);

		long2buff(g_storage_stat.total_upload_count, \
			pStatBuff->sz_total_upload_count);
		long2buff(g_storage_stat.success_upload_count, \
			pStatBuff->sz_success_upload_count);
		long2buff(g_storage_stat.total_append_count, \
			pStatBuff->sz_total_append_count);
		long2buff(g_storage_stat.success_append_count, \
			pStatBuff->sz_success_append_count);
		long2buff(g_storage_stat.total_modify_count, \
			pStatBuff->sz_total_modify_count);
		long2buff(g_storage_stat.success_modify_count, \
			pStatBuff->sz_success_modify_count);
		long2buff(g_storage_stat.total_truncate_count, \
			pStatBuff->sz_total_truncate_count);
		long2buff(g_storage_stat.success_truncate_count, \
			pStatBuff->sz_success_truncate_count);
		long2buff(g_storage_stat.total_download_count, \
			pStatBuff->sz_total_download_count);
		long2buff(g_storage_stat.success_download_count, \
			pStatBuff->sz_success_download_count);
		long2buff(g_storage_stat.total_set_meta_count, \
			pStatBuff->sz_total_set_meta_count);
		long2buff(g_storage_stat.success_set_meta_count, \
			pStatBuff->sz_success_set_meta_count);
		long2buff(g_storage_stat.total_delete_count, \
			pStatBuff->sz_total_delete_count);
		long2buff(g_storage_stat.success_delete_count, \
			pStatBuff->sz_success_delete_count);
		long2buff(g_storage_stat.total_get_meta_count, \
			pStatBuff->sz_total_get_meta_count);
		long2buff(g_storage_stat.success_get_meta_count, \
		 	pStatBuff->sz_success_get_meta_count);
		long2buff(g_storage_stat.total_create_link_count, \
			pStatBuff->sz_total_create_link_count);
		long2buff(g_storage_stat.success_create_link_count, \
			pStatBuff->sz_success_create_link_count);
		long2buff(g_storage_stat.total_delete_link_count, \
			pStatBuff->sz_total_delete_link_count);
		long2buff(g_storage_stat.success_delete_link_count, \
			pStatBuff->sz_success_delete_link_count);
		long2buff(g_storage_stat.total_upload_bytes, \
			pStatBuff->sz_total_upload_bytes);
		long2buff(g_storage_stat.success_upload_bytes, \
			pStatBuff->sz_success_upload_bytes);
		long2buff(g_storage_stat.total_append_bytes, \
			pStatBuff->sz_total_append_bytes);
		long2buff(g_storage_stat.success_append_bytes, \
			pStatBuff->sz_success_append_bytes);
		long2buff(g_storage_stat.total_modify_bytes, \
			pStatBuff->sz_total_modify_bytes);
		long2buff(g_storage_stat.success_modify_bytes, \
			pStatBuff->sz_success_modify_bytes);
		long2buff(g_storage_stat.total_download_bytes, \
			pStatBuff->sz_total_download_bytes);
		long2buff(g_storage_stat.success_download_bytes, \
			pStatBuff->sz_success_download_bytes);
		long2buff(g_storage_stat.total_sync_in_bytes, \
			pStatBuff->sz_total_sync_in_bytes);
		long2buff(g_storage_stat.success_sync_in_bytes, \
			pStatBuff->sz_success_sync_in_bytes);
		long2buff(g_storage_stat.total_sync_out_bytes, \
			pStatBuff->sz_total_sync_out_bytes);
		long2buff(g_storage_stat.success_sync_out_bytes, \
			pStatBuff->sz_success_sync_out_bytes);
		long2buff(g_storage_stat.total_file_open_count, \
			pStatBuff->sz_total_file_open_count);
		long2buff(g_storage_stat.success_file_open_count, \
			pStatBuff->sz_success_file_open_count);
		long2buff(g_storage_stat.total_file_read_count, \
			pStatBuff->sz_total_file_read_count);
		long2buff(g_storage_stat.success_file_read_count, \
			pStatBuff->sz_success_file_read_count);
		long2buff(g_storage_stat.total_file_write_count, \
			pStatBuff->sz_total_file_write_count);
		long2buff(g_storage_stat.success_file_write_count, \
			pStatBuff->sz_success_file_write_count);
		long2buff(g_storage_stat.last_source_update, \
			pStatBuff->sz_last_source_update);
		long2buff(g_storage_stat.last_sync_update, \
			pStatBuff->sz_last_sync_update);

		*pstat_chg_sync_count = g_stat_change_count;
		body_len = sizeof(FDFSStorageStatBuff);
	}
	else
	{
		body_len = 0;
	}

	long2buff(body_len, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_BEAT;

	if((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
		sizeof(TrackerHeader) + body_len, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	return tracker_check_response(pTrackerServer, bServerPortChanged);
}

static int tracker_storage_changelog_req(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader)];
	TrackerHeader *pHeader;
	int result;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;

	long2buff(0, pHeader->pkg_len);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_CHANGELOG_REQ;

	if((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
		sizeof(TrackerHeader), g_fdfs_network_timeout)) != 0)
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

int tracker_deal_changelog_response(ConnectionInfo *pTrackerServer)
{
#define FDFS_CHANGELOG_FIELDS  5
	int64_t nInPackLen;
	char *pInBuff;
	char *pBuffEnd;
	char *pLineStart;
	char *pLineEnd;
	char *cols[FDFS_CHANGELOG_FIELDS + 1];
	char *pGroupName;
	char *pOldStorageId;
	char *pNewStorageId;
	char szLine[256];
	int server_status;
	int col_count;
	int result;

	pInBuff = NULL;
	result = fdfs_recv_response(pTrackerServer, \
			&pInBuff, 0, &nInPackLen);
	if (result != 0)
	{
		return result;
	}

	if (nInPackLen == 0)
	{
		return result;
	}

	*(pInBuff + nInPackLen) = '\0';

	pLineStart = pInBuff;
	pBuffEnd = pInBuff + nInPackLen;
	while (pLineStart < pBuffEnd)
	{
		if (*pLineStart == '\0')  //skip empty line
		{
			pLineStart++;
			continue;
		}

		pLineEnd = strchr(pLineStart, '\n');
		if (pLineEnd != NULL)
		{
			*pLineEnd = '\0';
		}

		snprintf(szLine, sizeof(szLine), "%s", pLineStart);
		col_count = splitEx(szLine, ' ', cols, \
				FDFS_CHANGELOG_FIELDS + 1);

		do
		{
			if (col_count != FDFS_CHANGELOG_FIELDS)
			{
				logError("file: "__FILE__", line: %d, " \
					"changelog line field count: %d != %d,"\
					"line content=%s", __LINE__, col_count,\
					FDFS_CHANGELOG_FIELDS, pLineStart);
				break;
			}

			pGroupName = cols[1];
			if (strcmp(pGroupName, g_group_name) != 0)
			{   //ignore other group's changelog
				break;
			}

			pOldStorageId = cols[2];
			server_status = atoi(cols[3]);
			pNewStorageId = cols[4];

			if (server_status == FDFS_STORAGE_STATUS_DELETED)
			{
				tracker_unlink_mark_files(pOldStorageId);

				if (strcmp(g_sync_src_id, pOldStorageId) == 0)
				{
					*g_sync_src_id = '\0';
					storage_write_to_sync_ini_file();
				}
			}
			else if (server_status == FDFS_STORAGE_STATUS_IP_CHANGED)
			{
				if (!g_use_storage_id)
				{
					tracker_rename_mark_files(pOldStorageId, \
					g_server_port, pNewStorageId, g_server_port);
					if (strcmp(g_sync_src_id, pOldStorageId) == 0)
					{
						snprintf(g_sync_src_id, \
							sizeof(g_sync_src_id), \
							"%s", pNewStorageId);
						storage_write_to_sync_ini_file();
					}
				}
			}
			else
			{
				logError("file: "__FILE__", line: %d, " \
					"invalid status: %d in changelog, " \
					"line content=%s", __LINE__, \
					server_status, pLineStart);
			}
		} while (0);

		if (pLineEnd == NULL)
		{
			break;
		}

		pLineStart = pLineEnd + 1;
	}

	free(pInBuff);

	return 0;
}

int tracker_report_thread_start()
{
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pServerEnd;
	pthread_attr_t pattr;
	pthread_t tid;
	int result;

	if ((result=init_pthread_attr(&pattr, g_thread_stack_size)) != 0)
	{
		return result;
	}

	report_tids = (pthread_t *)malloc(sizeof(pthread_t) * \
					g_tracker_group.server_count);
	if (report_tids == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(pthread_t) * \
			g_tracker_group.server_count, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	memset(report_tids, 0, sizeof(pthread_t)*g_tracker_group.server_count);

	src_storage_status = (int *)malloc(sizeof(int) * \
					g_tracker_group.server_count);
	if (src_storage_status == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", __LINE__, \
			(int)sizeof(int) * g_tracker_group.server_count, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	memset(src_storage_status,-1,sizeof(int)*g_tracker_group.server_count);

	my_report_status = (signed char *)malloc(sizeof(signed char) * \
					g_tracker_group.server_count);
	if (my_report_status == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", __LINE__, \
			(int)sizeof(signed char) * g_tracker_group.server_count, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	memset(my_report_status, -1, sizeof(char)*g_tracker_group.server_count);
	
	g_tracker_reporter_count = 0;
	pServerEnd = g_tracker_group.servers + g_tracker_group.server_count;
	for (pTrackerServer=g_tracker_group.servers; pTrackerServer<pServerEnd; \
		pTrackerServer++)
	{
		if((result=pthread_create(&tid, &pattr, \
			tracker_report_thread_entrance, pTrackerServer)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"create thread failed, errno: %d, " \
				"error info: %s.", \
				__LINE__, result, STRERROR(result));
			return result;
		}

		if ((result=pthread_mutex_lock(&reporter_thread_lock)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"call pthread_mutex_lock fail, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
		}

		report_tids[g_tracker_reporter_count] = tid;
		g_tracker_reporter_count++;
		if ((result=pthread_mutex_unlock(&reporter_thread_lock)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"call pthread_mutex_unlock fail, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
		}
	}

	pthread_attr_destroy(&pattr);

	return 0;
}

