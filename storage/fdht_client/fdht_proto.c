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
#include "fdht_define.h"
#include "shared_func.h"
#include "logger.h"
#include "sockopt.h"
#include "fdht_types.h"
#include "fdht_proto.h"

extern int g_fdht_network_timeout;

int fdht_recv_header(FDHTServerInfo *pServer, fdht_pkg_size_t *in_bytes)
{
	FDHTProtoHeader resp;
	int result;

	if ((result=tcprecvdata_nb(pServer->sock, &resp, \
		sizeof(resp), g_fdht_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server: %s:%d, recv data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pServer->ip_addr, \
			pServer->port, \
			result, STRERROR(result));
		*in_bytes = 0;
		return result;
	}

	if (resp.status != 0)
	{
		*in_bytes = 0;
		return resp.status;
	}

	*in_bytes = buff2int(resp.pkg_len);
	if (*in_bytes < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server: %s:%d, recv package size %d " \
			"is not correct", \
			__LINE__, pServer->ip_addr, \
			pServer->port, *in_bytes);
		*in_bytes = 0;
		return EINVAL;
	}

	return resp.status;
}

int fdht_recv_response(FDHTServerInfo *pServer, \
		char **buff, const int buff_size, \
		fdht_pkg_size_t *in_bytes)
{
	int result;
	bool bMalloced;

	result = fdht_recv_header(pServer, in_bytes);
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
				"malloc %d bytes fail", \
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
				"server: %s:%d, recv body bytes: %d" \
				" exceed max: %d", \
				__LINE__, pServer->ip_addr, \
				pServer->port, *in_bytes, buff_size);
			*in_bytes = 0;
			return ENOSPC;
		}

		bMalloced = false;
	}

	if ((result=tcprecvdata_nb(pServer->sock, *buff, \
		*in_bytes, g_fdht_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server: %s:%d, recv data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pServer->ip_addr, \
			pServer->port, \
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

int fdht_quit(FDHTServerInfo *pServer)
{
	FDHTProtoHeader header;
	int result;

	memset(&header, 0, sizeof(header));
	header.cmd = FDHT_PROTO_CMD_QUIT;
	result = tcpsenddata_nb(pServer->sock, &header, sizeof(header), \
				g_fdht_network_timeout);
	if(result != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server ip: %s, send data fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pServer->ip_addr, \
			result, STRERROR(result));
		return result;
	}

	return 0;
}

void fdht_disconnect_server(FDHTServerInfo *pServer)
{
	if (pServer->sock > 0)
	{
		close(pServer->sock);
		pServer->sock = -1;
	}
}

int fdht_connect_server_nb(FDHTServerInfo *pServer, const int connect_timeout)
{
	int result;

	if (pServer->sock > 0)
	{
		close(pServer->sock);
	}
	pServer->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(pServer->sock < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"socket create failed, errno: %d, " \
			"error info: %s", __LINE__, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	if ((result=tcpsetnonblockopt(pServer->sock)) != 0)
	{
		close(pServer->sock);
		pServer->sock = -1;
		return result;
	}

	if ((result=connectserverbyip_nb(pServer->sock, \
		pServer->ip_addr, pServer->port, connect_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"connect to %s:%d fail, errno: %d, " \
			"error info: %s", __LINE__, pServer->ip_addr, \
			pServer->port, result, STRERROR(result));

		close(pServer->sock);
		pServer->sock = -1;
		return result;
	}

	return 0;
}

int fdht_connect_server(FDHTServerInfo *pServer)
{
	int result;

	if (pServer->sock > 0)
	{
		close(pServer->sock);
	}
	pServer->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(pServer->sock < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"socket create failed, errno: %d, " \
			"error info: %s", __LINE__, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	if ((result=connectserverbyip(pServer->sock, \
		pServer->ip_addr, pServer->port)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"connect to %s:%d fail, errno: %d, " \
			"error info: %s", __LINE__, pServer->ip_addr, \
			pServer->port, result, STRERROR(result));

		close(pServer->sock);
		pServer->sock = -1;
		return result;
	}

	if ((result=tcpsetnonblockopt(pServer->sock)) != 0)
	{
		close(pServer->sock);
		pServer->sock = -1;
		return result;
	}

	return 0;
}

/**
* request body format:
*       namespace_len:  4 bytes big endian integer
*       namespace: can be emtpy
*       obj_id_len:  4 bytes big endian integer
*       object_id: the object id (can be empty)
*       key_len:  4 bytes big endian integer
*       key:      key name
*       value_len:  4 bytes big endian integer
*       value:      value buff
* response body format:
*      none
*/
int fdht_client_set(FDHTServerInfo *pServer, const char keep_alive, \
	const time_t timestamp, const time_t expires, const int prot_cmd, \
	const int key_hash_code, FDHTKeyInfo *pKeyInfo, \
	const char *pValue, const int value_len)
{
	int result;
	char buff[sizeof(FDHTProtoHeader) + FDHT_MAX_FULL_KEY_LEN + 16 + 1024];
	FDHTProtoHeader *pHeader;
	int in_bytes;
	char *p;

	memset(buff, 0, sizeof(buff));
	pHeader = (FDHTProtoHeader *)buff;
	pHeader->cmd = prot_cmd;
	pHeader->keep_alive = keep_alive;
	int2buff((int)timestamp, pHeader->timestamp);
	int2buff((int)expires, pHeader->expires);
	int2buff(key_hash_code, pHeader->key_hash_code);
	int2buff(16 + pKeyInfo->namespace_len + pKeyInfo->obj_id_len + \
		pKeyInfo->key_len + value_len, pHeader->pkg_len);

	p = buff + sizeof(FDHTProtoHeader);
	PACK_BODY_UNTIL_KEY(pKeyInfo, p)
	int2buff(value_len, p);
	p += 4;

	if ((p - buff) + value_len <= sizeof(buff))
	{
		memcpy(p, pValue, value_len);
		p += value_len;
		if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
					g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"send data to server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			return result;
		}
	}
	else
	{
		if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
					g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"send data to server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			return result;
		}

		if ((result=tcpsenddata_nb(pServer->sock, (char *)pValue, \
					value_len, g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"send data to server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			return result;
		}
	}

	if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"recv data from server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pServer->ip_addr, pServer->port, \
			result, STRERROR(result));
		return result;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server %s:%d reponse bytes: %d != 0", \
			__LINE__, pServer->ip_addr, \
			pServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

/**
* request body format:
*       namespace_len:  4 bytes big endian integer
*       namespace: can be emtpy
*       obj_id_len:  4 bytes big endian integer
*       object_id: the object id (can be empty)
*       key_len:  4 bytes big endian integer
*       key:      key name
* response body format:
*      none
*/
int fdht_client_delete(FDHTServerInfo *pServer, const char keep_alive, \
	const time_t timestamp, const int prot_cmd, \
	const int key_hash_code, FDHTKeyInfo *pKeyInfo)
{
	int result;
	FDHTProtoHeader *pHeader;
	char buff[sizeof(FDHTProtoHeader) + FDHT_MAX_FULL_KEY_LEN + 16];
	int in_bytes;
	char *p;

	memset(buff, 0, sizeof(buff));
	pHeader = (FDHTProtoHeader *)buff;
	pHeader->cmd = prot_cmd;
	pHeader->keep_alive = keep_alive;
	int2buff(timestamp, pHeader->timestamp);
	int2buff(key_hash_code, pHeader->key_hash_code);
	int2buff(12 + pKeyInfo->namespace_len + pKeyInfo->obj_id_len + \
		pKeyInfo->key_len, pHeader->pkg_len);

	p = buff + sizeof(FDHTProtoHeader);
	PACK_BODY_UNTIL_KEY(pKeyInfo, p)

	if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
		g_fdht_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pServer->ip_addr, pServer->port, \
			result, STRERROR(result));
		return result;
	}

	if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
	{
		if (result == ENOENT)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"recv data from server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"recv data from server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
		}

		return result;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server %s:%d reponse bytes: %d != 0", \
			__LINE__, pServer->ip_addr, \
			pServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

int fdht_client_heart_beat(FDHTServerInfo *pServer)
{
	int result;
	FDHTProtoHeader header;
	int in_bytes;

	memset(&header, 0, sizeof(header));
	header.cmd = FDHT_PROTO_CMD_HEART_BEAT;
	header.keep_alive = 1;

	if ((result=tcpsenddata_nb(pServer->sock, &header, \
		sizeof(header), g_fdht_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pServer->ip_addr, pServer->port, \
			result, STRERROR(result));
		return result;
	}

	if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"recv data from server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pServer->ip_addr, pServer->port, \
			result, STRERROR(result));
		return result;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"server %s:%d reponse bytes: %d != 0", \
			__LINE__, pServer->ip_addr, \
			pServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

