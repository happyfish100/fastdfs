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
#include "fdfs_define.h"
#include "shared_func.h"
#include "logger.h"
#include "fdfs_global.h"
#include "sockopt.h"
#include "tracker_types.h"
#include "tracker_proto.h"

int fdfs_recv_header(ConnectionInfo *pTrackerServer, int64_t *in_bytes)
{
	TrackerHeader resp;
	int result;

	if ((result=tcprecvdata_nb(pTrackerServer->sock, &resp, \
		sizeof(resp), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server: %s:%d, recv data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		*in_bytes = 0;
		return result;
	}

	if (resp.status != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server: %s:%d, response status %d != 0", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, resp.status);

		*in_bytes = 0;
		return resp.status;
	}

	*in_bytes = buff2long(resp.pkg_len);
	if (*in_bytes < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server: %s:%d, recv package size " \
			"%"PRId64" is not correct", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, *in_bytes);
		*in_bytes = 0;
		return EINVAL;
	}

	return resp.status;
}

int fdfs_recv_response(ConnectionInfo *pTrackerServer, \
		char **buff, const int buff_size, \
		int64_t *in_bytes)
{
	int result;
	bool bMalloced;

	result = fdfs_recv_header(pTrackerServer, in_bytes);
	if (result != 0)
	{
		return result;
	}

	if (*in_bytes == 0)
	{
		return 0;
	}

	if (*buff == NULL)
	{
		*buff = (char *)malloc((*in_bytes) + 1);
		if (*buff == NULL)
		{
			*in_bytes = 0;

			logError("file: "__FILE__", line: %d, " \
				"malloc %"PRId64" bytes fail", \
				__LINE__, (*in_bytes) + 1);
			return errno != 0 ? errno : ENOMEM;
		}

		bMalloced = true;
	}
	else 
	{
		if (*in_bytes > buff_size)
		{
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%d, recv body bytes: " \
				"%"PRId64" exceed max: %d", \
				__LINE__, pTrackerServer->ip_addr, \
				pTrackerServer->port, *in_bytes, buff_size);
			*in_bytes = 0;
			return ENOSPC;
		}

		bMalloced = false;
	}

	if ((result=tcprecvdata_nb(pTrackerServer->sock, *buff, \
		*in_bytes, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server: %s:%d, recv data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		*in_bytes = 0;
		if (bMalloced)
		{
			free(*buff);
			*buff = NULL;
		}
		return result;
	}

	return 0;
}

int fdfs_quit(ConnectionInfo *pTrackerServer)
{
	TrackerHeader header;
	int result;

	memset(&header, 0, sizeof(header));
	header.cmd = FDFS_PROTO_CMD_QUIT;
	result = tcpsenddata_nb(pTrackerServer->sock, &header, \
			sizeof(header), g_fdfs_network_timeout);
	if(result != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server: %s:%d, send data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	return 0;
}

int fdfs_deal_no_body_cmd(ConnectionInfo *pTrackerServer, const int cmd)
{
	TrackerHeader header;
	int result;
	int64_t in_bytes;

	memset(&header, 0, sizeof(header));
	header.cmd = cmd;
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

	result = fdfs_recv_header(pTrackerServer, &in_bytes);
	if (result != 0)
	{
		logError("file: "__FILE__", line: %d, "
                "fdfs_recv_header fail, cmd: %d, result: %d",
                __LINE__, cmd, result);
		return result;
	}

	if (in_bytes == 0)
	{
		return 0;
	}
	else
	{
		logError("file: "__FILE__", line: %d, " \
			"server ip: %s, expect body length 0, " \
			"but received: %"PRId64, __LINE__, \
			pTrackerServer->ip_addr, in_bytes);
		return EINVAL;
	}
}

int fdfs_deal_no_body_cmd_ex(const char *ip_addr, const int port, const int cmd)
{
	ConnectionInfo *conn;
	ConnectionInfo server_info;
	int result;

	strcpy(server_info.ip_addr, ip_addr);
	server_info.port = port;
	server_info.sock = -1;
	if ((conn=tracker_connect_server(&server_info, &result)) == NULL)
	{
		return result;
	}

	result = fdfs_deal_no_body_cmd(conn, cmd);
	tracker_disconnect_server_ex(conn, result != 0);
	return result;
}

int fdfs_validate_group_name(const char *group_name)
{
	const char *p;
	const char *pEnd;
	int len;

	len = strlen(group_name);
	if (len == 0)
	{
		return EINVAL;
	}

	pEnd = group_name + len;
	for (p=group_name; p<pEnd; p++)
	{
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || \
			(*p >= '0' && *p <= '9')))
		{
			return EINVAL;
		}
	}

	return 0;
}

int fdfs_validate_filename(const char *filename)
{
	const char *p;
	const char *pEnd;
	int len;

	len = strlen(filename);
	pEnd = filename + len;
	for (p=filename; p<pEnd; p++)
	{
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || \
			(*p >= '0' && *p <= '9') || (*p == '-') || (*p == '_')\
			|| (*p == '.')))
		{
			return EINVAL;
		}
	}

	return 0;
}

int metadata_cmp_by_name(const void *p1, const void *p2)
{
	return strcmp(((FDFSMetaData *)p1)->name, ((FDFSMetaData *)p2)->name);
}

const char *get_storage_status_caption(const int status)
{
	switch (status)
	{
		case FDFS_STORAGE_STATUS_INIT:
			return "INIT";
		case FDFS_STORAGE_STATUS_WAIT_SYNC:
			return "WAIT_SYNC";
		case FDFS_STORAGE_STATUS_SYNCING:
			return "SYNCING";
		case FDFS_STORAGE_STATUS_OFFLINE:
			return "OFFLINE";
		case FDFS_STORAGE_STATUS_ONLINE:
			return "ONLINE";
		case FDFS_STORAGE_STATUS_DELETED:
			return "DELETED";
		case FDFS_STORAGE_STATUS_IP_CHANGED:
			return "IP_CHANGED";
		case FDFS_STORAGE_STATUS_ACTIVE:
			return "ACTIVE";
		case FDFS_STORAGE_STATUS_RECOVERY:
			return "RECOVERY";
		default:
			return "UNKOWN";
	}
}

FDFSMetaData *fdfs_split_metadata_ex(char *meta_buff, \
		const char recordSeperator, const char filedSeperator, \
		int *meta_count, int *err_no)
{
	char **rows;
	char **ppRow;
	char **ppEnd;
	FDFSMetaData *meta_list;
	FDFSMetaData *pMetadata;
	char *pSeperator;
	int nNameLen;
	int nValueLen;

	*meta_count = getOccurCount(meta_buff, recordSeperator) + 1;
	meta_list = (FDFSMetaData *)malloc(sizeof(FDFSMetaData) * \
						(*meta_count));
	if (meta_list == NULL)
	{
		*meta_count = 0;
		*err_no = ENOMEM;

		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail", \
			__LINE__, (int)sizeof(FDFSMetaData) * (*meta_count));
		return NULL;
	}

	rows = (char **)malloc(sizeof(char *) * (*meta_count));
	if (rows == NULL)
	{
		free(meta_list);
		*meta_count = 0;
		*err_no = ENOMEM;

		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail", \
			__LINE__, (int)sizeof(char *) * (*meta_count));
		return NULL;
	}

	*meta_count = splitEx(meta_buff, recordSeperator, \
				rows, *meta_count);
	ppEnd = rows + (*meta_count);
	pMetadata = meta_list;
	for (ppRow=rows; ppRow<ppEnd; ppRow++)
	{
		pSeperator = strchr(*ppRow, filedSeperator);
		if (pSeperator == NULL)
		{
			continue;
		}

		nNameLen = pSeperator - (*ppRow);
		nValueLen = strlen(pSeperator+1);
		if (nNameLen > FDFS_MAX_META_NAME_LEN)
		{
			nNameLen = FDFS_MAX_META_NAME_LEN;
		}
		if (nValueLen > FDFS_MAX_META_VALUE_LEN)
		{
			nValueLen = FDFS_MAX_META_VALUE_LEN;
		}

		memcpy(pMetadata->name, *ppRow, nNameLen);
		memcpy(pMetadata->value, pSeperator+1, nValueLen);
		pMetadata->name[nNameLen] = '\0';
		pMetadata->value[nValueLen] = '\0';

		pMetadata++;
	}

	*meta_count = pMetadata - meta_list;
	free(rows);

	*err_no = 0;
	return meta_list;
}

char *fdfs_pack_metadata(const FDFSMetaData *meta_list, const int meta_count, \
			char *meta_buff, int *buff_bytes)
{
	const FDFSMetaData *pMetaCurr;
	const FDFSMetaData *pMetaEnd;
	char *p;
	int name_len;
	int value_len;

	if (meta_buff == NULL)
	{
		meta_buff = (char *)malloc(sizeof(FDFSMetaData) * meta_count);
		if (meta_buff == NULL)
		{
			*buff_bytes = 0;

			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail", __LINE__, \
				(int)sizeof(FDFSMetaData) * meta_count);
			return NULL;
		}
	}

	p = meta_buff;
	pMetaEnd = meta_list + meta_count;
	for (pMetaCurr=meta_list; pMetaCurr<pMetaEnd; pMetaCurr++)
	{
		name_len = strlen(pMetaCurr->name);
		value_len = strlen(pMetaCurr->value);
		memcpy(p, pMetaCurr->name, name_len);
		p += name_len;
		*p++ = FDFS_FIELD_SEPERATOR;
		memcpy(p, pMetaCurr->value, value_len);
		p += value_len;
		*p++ = FDFS_RECORD_SEPERATOR;
	}

	*(--p) = '\0'; //omit the last record seperator
	*buff_bytes = p - meta_buff;
	return meta_buff;
}

void tracker_disconnect_server_ex(ConnectionInfo *conn, \
	const bool bForceClose)
{
	if (g_use_connection_pool)
	{
		conn_pool_close_connection_ex(&g_connection_pool, \
			conn, bForceClose);
	}
	else
	{
		conn_pool_disconnect_server(conn);
	}
}

ConnectionInfo *tracker_connect_server_ex(ConnectionInfo *pTrackerServer, \
		const int connect_timeout, int *err_no)
{
	if (g_use_connection_pool)
	{
		return conn_pool_get_connection(&g_connection_pool,
			pTrackerServer, err_no);
	}
	else
	{
		*err_no = conn_pool_connect_server(pTrackerServer, \
				connect_timeout);
		if (*err_no != 0)
		{
			return NULL;
		}
		else
		{
			return pTrackerServer;
		}
	}
}

int tracker_connect_server_no_pool(ConnectionInfo *pTrackerServer)
{
	if (pTrackerServer->sock >= 0)
	{
		return 0;
	}

	return conn_pool_connect_server(pTrackerServer, \
				g_fdfs_connect_timeout);
}

static int fdfs_do_parameter_req(ConnectionInfo *pTrackerServer, \
	char *buff, const int buff_size)
{
	char out_buff[sizeof(TrackerHeader)];
	TrackerHeader *pHeader;
	int64_t in_bytes;
	int result;

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_PARAMETER_REQ;
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

	result = fdfs_recv_response(pTrackerServer, &buff, buff_size, &in_bytes);
	if (result != 0)
	{
		return result;
	}

	if (in_bytes >= buff_size)
	{
		logError("file: "__FILE__", line: %d, " \
			"server: %s:%d, recv body bytes: " \
			"%"PRId64" exceed max: %d", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes, buff_size);
		return ENOSPC;
	}

	*(buff + in_bytes) = '\0';
	return 0;
}

int fdfs_get_ini_context_from_tracker(TrackerServerGroup *pTrackerGroup, \
		IniContext *iniContext, bool * volatile continue_flag, \
		const bool client_bind_addr, const char *bind_addr)
{
	ConnectionInfo *pGlobalServer;
	ConnectionInfo *pTServer;
	ConnectionInfo *pServerStart;
	ConnectionInfo *pServerEnd;
	ConnectionInfo trackerServer;
	char in_buff[1024];
	int result;
	int leader_index;
	int i;

	result = 0;
	pTServer = &trackerServer;
	pServerEnd = pTrackerGroup->servers + pTrackerGroup->server_count;

	leader_index = pTrackerGroup->leader_index;
	if (leader_index >= 0)
	{
		pServerStart = pTrackerGroup->servers + leader_index;
	}
	else
	{
		pServerStart = pTrackerGroup->servers;
	}

	do
	{
	for (pGlobalServer=pServerStart; pGlobalServer<pServerEnd; \
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(ConnectionInfo));
		for (i=0; i < 3; i++)
		{
			pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
			if(pTServer->sock < 0)
			{
				result = errno != 0 ? errno : EPERM;
				logError("file: "__FILE__", line: %d, " \
					"socket create failed, errno: %d, " \
					"error info: %s.", \
					__LINE__, result, STRERROR(result));
				sleep(5);
				break;
			}

			if (client_bind_addr && (bind_addr != NULL && \
						*bind_addr != '\0'))
			{
				socketBind(pTServer->sock, bind_addr, 0);
			}

			if (tcpsetnonblockopt(pTServer->sock) != 0)
			{
				close(pTServer->sock);
				pTServer->sock = -1;
				sleep(1);
				continue;
			}

			if ((result=connectserverbyip_nb(pTServer->sock, \
				pTServer->ip_addr, pTServer->port, \
				g_fdfs_connect_timeout)) == 0)
			{
				break;
			}

			close(pTServer->sock);
			pTServer->sock = -1;
			sleep(1);
		}

		if (pTServer->sock < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"connect to tracker server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pTServer->ip_addr, pTServer->port, \
				result, STRERROR(result));

			continue;
		}

		result = fdfs_do_parameter_req(pTServer, in_buff, \
						sizeof(in_buff));
		if (result == 0)
		{
			result = iniLoadFromBuffer(in_buff, iniContext);

			close(pTServer->sock);
			return result;
		}

		fdfs_quit(pTServer);
		close(pTServer->sock);
		sleep(1);
	}

	if (pServerStart != pTrackerGroup->servers)
	{
		pServerStart = pTrackerGroup->servers;
	}
	} while (*continue_flag);

	return EINTR;
}

int fdfs_get_tracker_status(ConnectionInfo *pTrackerServer, \
		TrackerRunningStatus *pStatus)
{
	char in_buff[1 + 2 * FDFS_PROTO_PKG_LEN_SIZE];
	TrackerHeader header;
	char *pInBuff;
	ConnectionInfo *conn;
	int64_t in_bytes;
	int result;

	pTrackerServer->sock = -1;
	if ((conn=tracker_connect_server(pTrackerServer, &result)) == NULL)
	{
		return result;
	}

	do
	{
	memset(&header, 0, sizeof(header));
	header.cmd = TRACKER_PROTO_CMD_TRACKER_GET_STATUS;
	if ((result=tcpsenddata_nb(conn->sock, &header, \
			sizeof(header), g_fdfs_network_timeout)) != 0)
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
				sizeof(in_buff), &in_bytes);
	if (result != 0)
	{
		break;
	}

	if (in_bytes != sizeof(in_buff))
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d response data " \
			"length: %"PRId64" is invalid, " \
			"expect length: %d.", __LINE__, \
			pTrackerServer->ip_addr, pTrackerServer->port, \
			in_bytes, (int)sizeof(in_buff));
		result = EINVAL;
		break;
	}

	pStatus->if_leader = *in_buff;
	pStatus->running_time = buff2long(in_buff + 1);
	pStatus->restart_interval = buff2long(in_buff + 1 + \
					FDFS_PROTO_PKG_LEN_SIZE);

	} while (0);

	tracker_disconnect_server_ex(conn, result != 0);

	return result;
}

