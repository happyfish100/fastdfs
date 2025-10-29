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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "fastcommon/sockopt.h"
#include "fastcommon/logger.h"
#include "fastcommon/hash.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/ini_file_reader.h"
#include "fdht_types.h"
#include "fdht_proto.h"
#include "fdht_global.h"
#include "fdht_client.h"

GroupArray g_group_array = {NULL, 0};
bool g_keep_alive = false;

static void fdht_proxy_extra_deal(GroupArray *pGroupArray, bool *bKeepAlive)
{
	int group_id;
	ServerArray *pServerArray;
	FDHTServerInfo **ppServer;
	FDHTServerInfo **ppServerEnd;

	if (!pGroupArray->use_proxy)
	{
		return;
	}

	*bKeepAlive = true;
	pGroupArray->server_count = 1;
	memcpy(pGroupArray->servers, &pGroupArray->proxy_server, \
			sizeof(FDHTServerInfo));

	pServerArray = pGroupArray->groups;
	for (group_id=0; group_id<pGroupArray->group_count; group_id++)
	{
		ppServerEnd = pServerArray->servers + pServerArray->count;
		for (ppServer=pServerArray->servers; \
				ppServer<ppServerEnd; ppServer++)
		{
			*ppServer = pGroupArray->servers;
		}

		pServerArray++;
	}
}

int fdht_client_init(const char *filename)
{
	char *pBasePath;
	IniContext iniContext;
	char szProxyPrompt[64];
	int result;

	memset(&iniContext, 0, sizeof(IniContext));
	if ((result=iniLoadFromFile(filename, &iniContext)) != 0)
	{
		logError("load conf file \"%s\" fail, ret code: %d", \
			filename, result);
		return result;
	}

	//iniPrintItems(&iniContext);

	while (1)
	{
		pBasePath = iniGetStrValue(NULL, "base_path", &iniContext);
		if (pBasePath == NULL)
		{
			logError("conf file \"%s\" must have item " \
				"\"base_path\"!", filename);
			result = ENOENT;
			break;
		}

		snprintf(g_fdht_base_path, sizeof(g_fdht_base_path), "%s", pBasePath);
		chopPath(g_fdht_base_path);
		if (!fileExists(g_fdht_base_path))
		{
			logError("\"%s\" can't be accessed, error info: %s", \
				g_fdht_base_path, STRERROR(errno));
			result = errno != 0 ? errno : ENOENT;
			break;
		}
		if (!isDir(g_fdht_base_path))
		{
			logError("\"%s\" is not a directory!", g_fdht_base_path);
			result = ENOTDIR;
			break;
		}

		g_fdht_connect_timeout = iniGetIntValue(NULL, "connect_timeout", \
				&iniContext, DEFAULT_CONNECT_TIMEOUT);
		if (g_fdht_connect_timeout <= 0)
		{
			g_fdht_connect_timeout = DEFAULT_CONNECT_TIMEOUT;
		}

		g_fdht_network_timeout = iniGetIntValue(NULL, "network_timeout", \
				&iniContext, DEFAULT_NETWORK_TIMEOUT);
		if (g_fdht_network_timeout <= 0)
		{
			g_fdht_network_timeout = DEFAULT_NETWORK_TIMEOUT;
		}

		g_keep_alive = iniGetBoolValue(NULL, "keep_alive", \
				&iniContext, false);

		if ((result=fdht_load_groups(&iniContext, \
				&g_group_array)) != 0)
		{
			break;
		}

		if (g_group_array.use_proxy)
		{
			sprintf(szProxyPrompt, "proxy_addr=%s, proxy_port=%d, ",
				g_group_array.proxy_server.ip_addr, 
				g_group_array.proxy_server.port);
		}
		else
		{
			*szProxyPrompt = '\0';
		}

		load_log_level(&iniContext);

		logDebug("file: "__FILE__", line: %d, " \
			"base_path=%s, " \
			"connect_timeout=%ds, network_timeout=%ds, " \
			"keep_alive=%d, use_proxy=%d, %s"\
			"group_count=%d, server_count=%d", __LINE__, \
			g_fdht_base_path, g_fdht_connect_timeout, \
			g_fdht_network_timeout, g_keep_alive, \
			g_group_array.use_proxy, szProxyPrompt, \
			g_group_array.group_count, g_group_array.server_count);

		fdht_proxy_extra_deal(&g_group_array, &g_keep_alive);

		break;
	}

	iniFreeContext(&iniContext);

	return result;
}

int fdht_load_conf(const char *filename, GroupArray *pGroupArray, \
		bool *bKeepAlive)
{
	IniContext iniContext;
	int result;

	if ((result=iniLoadFromFile(filename, &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"load conf file \"%s\" fail, " \
			"ret code: %d", __LINE__, \
			filename, result);
		return result;
	}

	*bKeepAlive = iniGetBoolValue(NULL, "keep_alive", &iniContext, false);
	if ((result=fdht_load_groups(&iniContext, pGroupArray)) != 0)
	{
		iniFreeContext(&iniContext);
		return result;
	}

	fdht_proxy_extra_deal(pGroupArray, bKeepAlive);

	iniFreeContext(&iniContext);
	return 0;
}

void fdht_client_destroy()
{
	fdht_free_group_array(&g_group_array);
}

#define get_readable_connection(pServerArray, bKeepAlive, hash_code, err_no) \
	  get_connection(pServerArray, bKeepAlive, hash_code, err_no)

#define get_writable_connection(pServerArray, bKeepAlive, hash_code, err_no) \
	  get_connection(pServerArray, bKeepAlive, hash_code, err_no)

static FDHTServerInfo *get_connection(ServerArray *pServerArray, \
		const bool bKeepAlive, const int hash_code, int *err_no)
{
	FDHTServerInfo **ppServer;
	FDHTServerInfo **ppEnd;
	int server_index;
	int new_hash_code;

	*err_no = ENOENT;
	new_hash_code = (hash_code << 16) | (hash_code >> 16);
	if (new_hash_code < 0)
	{
		new_hash_code &= 0x7FFFFFFF;
	}
	server_index = new_hash_code % pServerArray->count;
	ppEnd = pServerArray->servers + pServerArray->count;
	for (ppServer = pServerArray->servers + server_index; \
		ppServer<ppEnd; ppServer++)
	{
		if ((*ppServer)->sock > 0)  //already connected
		{
			return *ppServer;
		}

		if ((*err_no=fdht_connect_server_nb(*ppServer, \
			g_fdht_connect_timeout)) == 0)
		{
			if (bKeepAlive)
			{
				tcpsetnodelay((*ppServer)->sock, 3600);
			}
			return *ppServer;
		}
	}

	ppEnd = pServerArray->servers + server_index;
	for (ppServer = pServerArray->servers; ppServer<ppEnd; ppServer++)
	{
		if ((*ppServer)->sock > 0)  //already connected
		{
			return *ppServer;
		}

		if ((*err_no=fdht_connect_server_nb(*ppServer, \
			g_fdht_connect_timeout)) == 0)
		{
			if (bKeepAlive)
			{
				tcpsetnodelay((*ppServer)->sock, 3600);
			}
			return *ppServer;
		}
	}

	return NULL;
}

#define CALC_KEY_HASH_CODE(pKeyInfo, hash_key, hash_key_len, key_hash_code) \
	if (pKeyInfo->namespace_len > FDHT_MAX_NAMESPACE_LEN) \
	{ \
		fprintf(stderr, "namespace length: %d exceeds, " \
			"max length:  %d\n", \
			pKeyInfo->namespace_len, FDHT_MAX_NAMESPACE_LEN); \
		return EINVAL; \
	} \
 \
	if (pKeyInfo->obj_id_len > FDHT_MAX_OBJECT_ID_LEN) \
	{ \
		fprintf(stderr, "object ID length: %d exceeds, " \
			"max length: %d\n", \
			pKeyInfo->obj_id_len, FDHT_MAX_OBJECT_ID_LEN); \
		return EINVAL; \
	} \
 \
	if (pKeyInfo->key_len > FDHT_MAX_SUB_KEY_LEN) \
	{ \
		fprintf(stderr, "key length: %d exceeds, max length: %d\n", \
			pKeyInfo->key_len, FDHT_MAX_SUB_KEY_LEN); \
		return EINVAL; \
	} \
 \
	if (pKeyInfo->namespace_len == 0 && pKeyInfo->obj_id_len == 0) \
	{ \
		hash_key_len = pKeyInfo->key_len; \
		memcpy(hash_key, pKeyInfo->szKey, pKeyInfo->key_len); \
	} \
	else if (pKeyInfo->namespace_len > 0 && pKeyInfo->obj_id_len > 0) \
	{ \
		hash_key_len = pKeyInfo->namespace_len+1+pKeyInfo->obj_id_len; \
		memcpy(hash_key,pKeyInfo->szNameSpace,pKeyInfo->namespace_len);\
		*(hash_key + pKeyInfo->namespace_len)=FDHT_FULL_KEY_SEPERATOR; \
		memcpy(hash_key + pKeyInfo->namespace_len + 1, \
			pKeyInfo->szObjectId, pKeyInfo->obj_id_len); \
	} \
	else \
	{ \
		fprintf(stderr, "invalid namespace length: %d and " \
				"object ID length: %d\n", \
				pKeyInfo->namespace_len, pKeyInfo->obj_id_len); \
		return EINVAL; \
	} \
 \
	key_hash_code = Time33Hash(hash_key, hash_key_len); \
	if (key_hash_code < 0) \
	{ \
		key_hash_code &= 0x7FFFFFFF; \
	} \


#define CALC_OBJECT_HASH_CODE(pObjectInfo, hash_key, hash_key_len, key_hash_code) \
	if (pObjectInfo->namespace_len <= 0 || pObjectInfo->obj_id_len <= 0) \
	{ \
		fprintf(stderr, "invalid namespace length: %d and " \
			"object ID length: %d\n", \
			pObjectInfo->namespace_len, pObjectInfo->obj_id_len); \
		return EINVAL; \
	} \
 \
	if (pObjectInfo->namespace_len > FDHT_MAX_NAMESPACE_LEN) \
	{ \
		fprintf(stderr, "namespace length: %d exceeds, " \
			"max length:  %d\n", \
			pObjectInfo->namespace_len, FDHT_MAX_NAMESPACE_LEN); \
		return EINVAL; \
	} \
 \
	if (pObjectInfo->obj_id_len > FDHT_MAX_OBJECT_ID_LEN) \
	{ \
		fprintf(stderr, "object ID length: %d exceeds, " \
			"max length: %d\n", \
			pObjectInfo->obj_id_len, FDHT_MAX_OBJECT_ID_LEN); \
		return EINVAL; \
	} \
	hash_key_len = pObjectInfo->namespace_len+1+pObjectInfo->obj_id_len; \
	memcpy(hash_key, pObjectInfo->szNameSpace, pObjectInfo->namespace_len);\
	*(hash_key + pObjectInfo->namespace_len) = FDHT_FULL_KEY_SEPERATOR; \
	memcpy(hash_key + pObjectInfo->namespace_len + 1, \
		pObjectInfo->szObjectId, pObjectInfo->obj_id_len); \
 \
	key_hash_code = Time33Hash(hash_key, hash_key_len); \
	if (key_hash_code < 0) \
	{ \
		key_hash_code &= 0x7FFFFFFF; \
	} \


/**
* request body format:
*       namespace_len:  4 bytes big endian integer
*       namespace: can be emtpy
*       obj_id_len:  4 bytes big endian integer
*       object_id: the object id (can be empty)
*       key_len:  4 bytes big endian integer
*       key:      key name
* response body format:
*       value_len:  4 bytes big endian integer
*       value:      value buff
*/
int fdht_get_ex1(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTKeyInfo *pKeyInfo, const time_t expires, \
		char **ppValue, int *value_len, MallocFunc malloc_func)
{
	int result;
	FDHTProtoHeader *pHeader;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	char buff[sizeof(FDHTProtoHeader) + FDHT_MAX_FULL_KEY_LEN + 16];
	int in_bytes;
	int vlen;
	int group_id;
	int hash_key_len;
	int key_hash_code;
	int i;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;
	char *p;

	CALC_KEY_HASH_CODE(pKeyInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	//printf("get group_id=%d\n", group_id);

	pGroup = pGroupArray->groups + group_id;
	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_readable_connection(pGroup, bKeepAlive, \
			key_hash_code, &result);
	if (pServer == NULL)
	{
		return result;
	}

	memset(buff, 0, sizeof(buff));
	pHeader = (FDHTProtoHeader *)buff;

	pHeader->cmd = FDHT_PROTO_CMD_GET;
	pHeader->keep_alive = bKeepAlive;
	int2buff((int)time(NULL), pHeader->timestamp);
	int2buff((int)expires, pHeader->expires);
	int2buff(key_hash_code, pHeader->key_hash_code);
	int2buff(12 + pKeyInfo->namespace_len + pKeyInfo->obj_id_len + \
		pKeyInfo->key_len, pHeader->pkg_len);

	do
	{
		p = buff + sizeof(FDHTProtoHeader);
		PACK_BODY_UNTIL_KEY(pKeyInfo, p)
		if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
			g_fdht_network_timeout)) != 0)
		{
			logError("send data to server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
		{
			break;
		}

		if (in_bytes < 4)
		{
			logError("server %s:%u reponse bytes: %d < 4", \
				pServer->ip_addr, pServer->port, in_bytes);
			result = EINVAL;
			break;
		}

		if ((result=tcprecvdata_nb(pServer->sock, buff, \
			4, g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, recv data fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pServer->ip_addr, \
				pServer->port, \
				result, STRERROR(result));
			break;
		}

		vlen = buff2int(buff);
		if (vlen != in_bytes - 4)
		{
			logError("server %s:%u reponse bytes: %d " \
				"is not correct, %d != %d", pServer->ip_addr, \
				pServer->port, in_bytes, vlen, in_bytes - 4);
			result = EINVAL;
			break;
		}

		if (*ppValue != NULL)
		{
			if (vlen >= *value_len)
			{
				*value_len = 0;
				result = ENOSPC;
				break;
			}

			*value_len = vlen;
		}
		else
		{
			*value_len = vlen;
			*ppValue = (char *)malloc_func((*value_len + 1));
			if (*ppValue == NULL)
			{
				*value_len = 0;
				logError("malloc %d bytes fail, " \
					"errno: %d, error info: %s", \
					*value_len + 1, errno, STRERROR(errno));
				result = errno != 0 ? errno : ENOMEM;
				break;
			}
		}

		if ((result=tcprecvdata_nb(pServer->sock, *ppValue, \
			*value_len, g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, recv data fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pServer->ip_addr, \
				pServer->port, \
				result, STRERROR(result));
			break;
		}

		*(*ppValue + *value_len) = '\0';
	} while(0);

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

int fdht_batch_set_ex(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTObjectInfo *pObjectInfo, FDHTKeyValuePair *key_list, \
		const int key_count, const time_t expires, int *success_count)
{
	int result;
	FDHTProtoHeader *pHeader;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	char buff[sizeof(FDHTProtoHeader) + FDHT_MAX_FULL_KEY_LEN + \
		(8 + FDHT_MAX_SUB_KEY_LEN) * FDHT_MAX_KEY_COUNT_PER_REQ + \
		32 * 1024];
	char *pBuff;
	int in_bytes;
	int total_key_len;
	int total_value_len;
	int pkg_total_len;
	int group_id;
	int hash_key_len;
	int key_hash_code;
	int i;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;
	FDHTKeyValuePair *pKeyValuePair;
	FDHTKeyValuePair *pKeyValueEnd;
	char *p;

	*success_count = 0;
	if (key_count <= 0 || key_count > FDHT_MAX_KEY_COUNT_PER_REQ)
	{
		logError("invalid key_count: %d", key_count);
		return EINVAL;
	}

	CALC_OBJECT_HASH_CODE(pObjectInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	pGroup = pGroupArray->groups + group_id;

	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_writable_connection(pGroup, bKeepAlive, \
			key_hash_code, &result);
	if (pServer == NULL)
	{
		return result;
	}

	total_key_len = 0;
	total_value_len = 0;
	pKeyValueEnd = key_list + key_count;
	for (pKeyValuePair=key_list; pKeyValuePair<pKeyValueEnd; pKeyValuePair++)
	{
		total_key_len += pKeyValuePair->key_len;
		total_value_len += pKeyValuePair->value_len;
	}
	pkg_total_len = sizeof(FDHTProtoHeader) + 12 + pObjectInfo->namespace_len + \
			pObjectInfo->obj_id_len + 8 * key_count + \
			total_key_len + total_value_len;

	if (pkg_total_len <= sizeof(buff))
	{
		pBuff = buff;
	}
	else
	{
		pBuff = (char *)malloc(pkg_total_len);
		if (pBuff == NULL)
		{
			result = errno != 0 ? errno : ENOMEM;
			logError("malloc %d bytes fail, " \
				"errno: %d, error info: %s", \
				pkg_total_len, result, STRERROR(result));
			return result;
		}
	}

	memset(pBuff, 0, pkg_total_len);
	pHeader = (FDHTProtoHeader *)pBuff;

	pHeader->cmd = FDHT_PROTO_CMD_BATCH_SET;
	pHeader->keep_alive = bKeepAlive;
	int2buff((int)time(NULL), pHeader->timestamp);
	int2buff((int)expires, pHeader->expires);
	int2buff(key_hash_code, pHeader->key_hash_code);

	p = pBuff + sizeof(FDHTProtoHeader);
	PACK_BODY_OBJECT(pObjectInfo, p)
	int2buff(key_count, p);
	p += 4;

	for (pKeyValuePair=key_list; pKeyValuePair<pKeyValueEnd; pKeyValuePair++)
	{
		int2buff(pKeyValuePair->key_len, p);
		memcpy(p + 4, pKeyValuePair->szKey, pKeyValuePair->key_len);
		p += 4 + pKeyValuePair->key_len;

		int2buff(pKeyValuePair->value_len, p);
		memcpy(p + 4, pKeyValuePair->pValue, pKeyValuePair->value_len);
		p += 4 + pKeyValuePair->value_len;
	}

	do
	{
		int2buff(pkg_total_len - sizeof(FDHTProtoHeader), pHeader->pkg_len);
		if ((result=tcpsenddata_nb(pServer->sock, pBuff, pkg_total_len, \
			g_fdht_network_timeout)) != 0)
		{
			logError("send data to server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
		{
			break;
		}

		if (in_bytes != 8 + 5 * key_count + total_key_len)
		{
			logError("server %s:%u reponse bytes: %d != %d", \
				pServer->ip_addr, pServer->port, in_bytes, \
				8 + 5 * key_count + total_key_len);
			result = EINVAL;
			break;
		}

		if ((result=tcprecvdata_nb(pServer->sock, pBuff, \
			in_bytes, g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, recv data fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if (buff2int(pBuff) != key_count)
		{
			result = EINVAL;
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, invalid key_count: %d, " \
				"expect key count: %d", \
				__LINE__, pServer->ip_addr, pServer->port, \
				buff2int(pBuff), key_count);
			break;
		}

		*success_count = buff2int(pBuff + 4);
		p = pBuff + 8;
		for (pKeyValuePair=key_list; pKeyValuePair<pKeyValueEnd; \
			pKeyValuePair++)
		{
			pKeyValuePair->key_len = buff2int(p);

			memcpy(pKeyValuePair->szKey, p + 4, \
				pKeyValuePair->key_len);
			p += 4 + pKeyValuePair->key_len;
			pKeyValuePair->status = *p++;
		}
	} while (0);

	if (pBuff != buff)
	{
		free(pBuff);
	}

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}
	break;
	}

	return result;
}

int fdht_batch_delete_ex(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTObjectInfo *pObjectInfo, FDHTKeyValuePair *key_list, \
		const int key_count, int *success_count)
{
	int result;
	FDHTProtoHeader *pHeader;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	char buff[sizeof(FDHTProtoHeader) + FDHT_MAX_FULL_KEY_LEN + 8 + \
		(5 + FDHT_MAX_SUB_KEY_LEN) * FDHT_MAX_KEY_COUNT_PER_REQ];
	int in_bytes;
	int total_key_len;
	int group_id;
	int hash_key_len;
	int key_hash_code;
	int i;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;
	FDHTKeyValuePair *pKeyValuePair;
	FDHTKeyValuePair *pKeyValueEnd;
	char *p;

	*success_count = 0;
	if (key_count <= 0 || key_count > FDHT_MAX_KEY_COUNT_PER_REQ)
	{
		logError("invalid key_count: %d", key_count);
		return EINVAL;
	}

	CALC_OBJECT_HASH_CODE(pObjectInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	pGroup = pGroupArray->groups + group_id;

	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_readable_connection(pGroup, bKeepAlive, \
			key_hash_code, &result);
	if (pServer == NULL)
	{
		return result;
	}

	memset(buff, 0, sizeof(buff));
	pHeader = (FDHTProtoHeader *)buff;

	pHeader->cmd = FDHT_PROTO_CMD_BATCH_DEL;
	pHeader->keep_alive = bKeepAlive;
	int2buff((int)time(NULL), pHeader->timestamp);
	int2buff(key_hash_code, pHeader->key_hash_code);

	p = buff + sizeof(FDHTProtoHeader);
	PACK_BODY_OBJECT(pObjectInfo, p)
	int2buff(key_count, p);
	p += 4;

	total_key_len = 0;
	pKeyValueEnd = key_list + key_count;
	for (pKeyValuePair=key_list; pKeyValuePair<pKeyValueEnd; pKeyValuePair++)
	{
		int2buff(pKeyValuePair->key_len, p);
		memcpy(p + 4, pKeyValuePair->szKey, pKeyValuePair->key_len);
		p += 4 + pKeyValuePair->key_len;

		total_key_len += pKeyValuePair->key_len;
	}

	do
	{
		int2buff((p - buff) - sizeof(FDHTProtoHeader), pHeader->pkg_len);
		if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
			g_fdht_network_timeout)) != 0)
		{
			logError("send data to server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
		{
			break;
		}

		if (in_bytes != 8 + 5 * key_count + total_key_len)
		{
			logError("server %s:%u reponse bytes: %d != %d", \
				pServer->ip_addr, pServer->port, in_bytes, \
				8 + 5 * key_count + total_key_len);
			result = EINVAL;
			break;
		}

		if ((result=tcprecvdata_nb(pServer->sock, buff, \
			in_bytes, g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, recv data fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if (buff2int(buff) != key_count)
		{
			result = EINVAL;
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, invalid key_count: %d, " \
				"expect key count: %d", \
				__LINE__, pServer->ip_addr, pServer->port, \
				buff2int(buff), key_count);
			break;
		}

		*success_count = buff2int(buff + 4);
		p = buff + 8;
		for (pKeyValuePair=key_list; pKeyValuePair<pKeyValueEnd; \
			pKeyValuePair++)
		{
			pKeyValuePair->key_len = buff2int(p);

			memcpy(pKeyValuePair->szKey, p + 4, \
				pKeyValuePair->key_len);
			p += 4 + pKeyValuePair->key_len;
			pKeyValuePair->status = *p++;
		}
	} while (0);

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

int fdht_batch_get_ex1(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTObjectInfo *pObjectInfo, FDHTKeyValuePair *key_list, \
		const int key_count, const time_t expires, \
		MallocFunc malloc_func, int *success_count)
{
	int result;
	FDHTProtoHeader *pHeader;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	char buff[sizeof(FDHTProtoHeader) + FDHT_MAX_FULL_KEY_LEN + \
		(4 + FDHT_MAX_SUB_KEY_LEN) * FDHT_MAX_KEY_COUNT_PER_REQ + \
		32 * 1024];
	int in_bytes;
	int value_len;
	int group_id;
	int hash_key_len;
	int key_hash_code;
	char *pInBuff;
	int i;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;
	FDHTKeyValuePair *pKeyValuePair;
	FDHTKeyValuePair *pKeyValueEnd;
	char *p;

	*success_count = 0;
	if (key_count <= 0 || key_count > FDHT_MAX_KEY_COUNT_PER_REQ)
	{
		logError("invalid key_count: %d", key_count);
		return EINVAL;
	}

	CALC_OBJECT_HASH_CODE(pObjectInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	pGroup = pGroupArray->groups + group_id;

	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_readable_connection(pGroup, bKeepAlive, \
			key_hash_code, &result);
	if (pServer == NULL)
	{
		return result;
	}

	memset(buff, 0, sizeof(buff));
	pHeader = (FDHTProtoHeader *)buff;

	pHeader->cmd = FDHT_PROTO_CMD_BATCH_GET;
	pHeader->keep_alive = bKeepAlive;
	int2buff((int)time(NULL), pHeader->timestamp);
	int2buff((int)expires, pHeader->expires);
	int2buff(key_hash_code, pHeader->key_hash_code);

	p = buff + sizeof(FDHTProtoHeader);
	PACK_BODY_OBJECT(pObjectInfo, p)
	int2buff(key_count, p);
	p += 4;

	pKeyValueEnd = key_list + key_count;
	for (pKeyValuePair=key_list; pKeyValuePair<pKeyValueEnd; pKeyValuePair++)
	{
		int2buff(pKeyValuePair->key_len, p);
		memcpy(p + 4, pKeyValuePair->szKey, pKeyValuePair->key_len);
		p += 4 + pKeyValuePair->key_len;
	}

	pInBuff = buff;
	do
	{
		int2buff((p - buff) - sizeof(FDHTProtoHeader), pHeader->pkg_len);
		if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
			g_fdht_network_timeout)) != 0)
		{
			logError("send data to server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
		{
			break;
		}

		if (in_bytes < 17)
		{
			logError("server %s:%u reponse bytes: %d < 17", \
				pServer->ip_addr, pServer->port, in_bytes);
			result = EINVAL;
			break;
		}

		if (in_bytes > sizeof(buff))
		{
			pInBuff = (char *)malloc(in_bytes);
			if (pInBuff == NULL)
			{
				result = errno != 0 ? errno : ENOMEM;
				logError("file: "__FILE__", line: %d, " \
					"malloc %d bytes fail, " \
					"errno: %d, error info: %s", \
					__LINE__, in_bytes, \
					result, STRERROR(result));
				break;
			}
		}

		if ((result=tcprecvdata_nb(pServer->sock, pInBuff, \
			in_bytes, g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, recv data fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if (buff2int(pInBuff) != key_count)
		{
			result = EINVAL;
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, invalid key_count: %d, " \
				"expect key count: %d", \
				__LINE__, pServer->ip_addr, pServer->port, \
				buff2int(pInBuff), key_count);
			break;
		}

		*success_count = buff2int(pInBuff + 4);
		p = pInBuff + 8;
		for (pKeyValuePair=key_list; pKeyValuePair<pKeyValueEnd; \
			pKeyValuePair++)
		{
			pKeyValuePair->key_len = buff2int(p);

			memcpy(pKeyValuePair->szKey, p + 4, \
				pKeyValuePair->key_len);
			p += 4 + pKeyValuePair->key_len;
			pKeyValuePair->status = *p++;
			if (pKeyValuePair->status != 0)
			{
				pKeyValuePair->value_len = 0;
				continue;
			}

			value_len = buff2int(p);
			p += 4;
			if (pKeyValuePair->pValue != NULL)
			{
				if (value_len >= pKeyValuePair->value_len)
				{
					*(pKeyValuePair->pValue) = '\0';
					pKeyValuePair->value_len = 0;
					pKeyValuePair->status = ENOSPC;
				}
				else
				{
					pKeyValuePair->value_len = value_len;
					memcpy(pKeyValuePair->pValue, p, \
						value_len);
					*(pKeyValuePair->pValue+value_len)='\0';
				}
			}
			else
			{
				pKeyValuePair->pValue = (char *)malloc_func( \
						value_len + 1);
				if (pKeyValuePair->pValue == NULL)
				{
					pKeyValuePair->value_len = 0;
					pKeyValuePair->status = errno != 0 ? \
								errno : ENOMEM;
					logError("malloc %d bytes fail, " \
						"errno: %d, error info: %s", \
						value_len+1, errno, \
						STRERROR(errno));
				}
				else
				{
					pKeyValuePair->value_len = value_len;
					memcpy(pKeyValuePair->pValue, p, \
						value_len);
					*(pKeyValuePair->pValue+value_len)='\0';
				}
			}

			p += value_len;
		}

		if (in_bytes != p - pInBuff)
		{
			*success_count = 0;
			logError("server %s:%u reponse bytes: %d != %d", \
				pServer->ip_addr, pServer->port, \
				in_bytes, (int)(p - pInBuff));
			result = EINVAL;
			break;
		}
	} while (0);

	if (pInBuff != buff)
	{
		free(pInBuff);
	}

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

int fdht_set_ex(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTKeyInfo *pKeyInfo, const time_t expires, \
		const char *pValue, const int value_len)
{
	int result;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	int group_id;
	int hash_key_len;
	int key_hash_code;
	int i;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;

	CALC_KEY_HASH_CODE(pKeyInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	pGroup = pGroupArray->groups + group_id;

	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_writable_connection(pGroup, bKeepAlive, \
			key_hash_code, &result);
	if (pServer == NULL)
	{
		return result;
	}

	//printf("key_hash_code=%d, group_id=%d\n", key_hash_code, group_id);

	//printf("set group_id=%d\n", group_id);
	result = fdht_client_set(pServer, bKeepAlive, time(NULL), expires, \
			FDHT_PROTO_CMD_SET, key_hash_code, \
			pKeyInfo, pValue, value_len);

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

/**
* request body format:
*       namespace_len:  4 bytes big endian integer
*       namespace: can be emtpy
*       obj_id_len:  4 bytes big endian integer
*       object_id: the object id (can be empty)
*       key_len:  4 bytes big endian integer
*       key:      key name
*       incr      4 bytes big endian integer
* response body format:
*      value_len: 4 bytes big endian integer
*      value :  value_len bytes
*/
int fdht_inc_ex(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTKeyInfo *pKeyInfo, const time_t expires, \
		const int increase, char *pValue, int *value_len)
{
	int result;
	FDHTProtoHeader *pHeader;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	char buff[FDHT_MAX_FULL_KEY_LEN + 32];
	char *in_buff;
	int in_bytes;
	int group_id;
	int hash_key_len;
	int key_hash_code;
	int i;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;
	char *p;

	CALC_KEY_HASH_CODE(pKeyInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	pGroup = pGroupArray->groups + group_id;

	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_writable_connection(pGroup, bKeepAlive, \
			key_hash_code, &result);
	if (pServer == NULL)
	{
		return result;
	}

	//printf("inc group_id=%d\n", group_id);

	memset(buff, 0, sizeof(buff));
	pHeader = (FDHTProtoHeader *)buff;

	pHeader->cmd = FDHT_PROTO_CMD_INC;
	pHeader->keep_alive = bKeepAlive;
	int2buff((int)time(NULL), pHeader->timestamp);
	int2buff((int)expires, pHeader->expires);
	int2buff(key_hash_code, pHeader->key_hash_code);
	int2buff(16 + pKeyInfo->namespace_len + pKeyInfo->obj_id_len + \
		pKeyInfo->key_len, pHeader->pkg_len);

	while (1)
	{
		p = buff + sizeof(FDHTProtoHeader);
		PACK_BODY_UNTIL_KEY(pKeyInfo, p)
		int2buff(increase, p);
		p += 4;
		if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
			g_fdht_network_timeout)) != 0)
		{
			logError("send data to server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		in_buff = buff;
		if ((result=fdht_recv_response(pServer, &in_buff, \
			sizeof(buff), &in_bytes)) != 0)
		{
			logError("recv data from server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if (in_bytes < 4)
		{
			logError("server %s:%u reponse bytes: %d < 4!", \
				pServer->ip_addr, pServer->port, in_bytes);
			result = EINVAL;
			break;
		}

		if (in_bytes - 4 >= *value_len)
		{
			*value_len = 0;
			result = ENOSPC;
			break;
		}

		*value_len = in_bytes - 4;
		memcpy(pValue, in_buff + 4, *value_len);
		*(pValue + (*value_len)) = '\0';
		break;
	}

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

int fdht_delete_ex(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTKeyInfo *pKeyInfo)
{
	int result;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	int group_id;
	int hash_key_len;
	int key_hash_code;
	int i;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;

	CALC_KEY_HASH_CODE(pKeyInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	pGroup = pGroupArray->groups + group_id;

	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_writable_connection(pGroup, bKeepAlive, \
			key_hash_code , &result);
	if (pServer == NULL)
	{
		return result;
	}

	//printf("del group_id=%d\n", group_id);
	result = fdht_client_delete(pServer, bKeepAlive, time(NULL), \
			FDHT_PROTO_CMD_DEL, key_hash_code, pKeyInfo);

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

int fdht_connect_all_servers(GroupArray *pGroupArray, const bool bKeepAlive, \
			int *success_count, int *fail_count)
{
	FDHTServerInfo *pServerInfo;
	FDHTServerInfo *pServerEnd;
	int conn_result;
	int result;

	*success_count = 0;
	*fail_count = 0;
	if (pGroupArray->servers == NULL)
	{
		return ENOENT;
	}

	result = 0;

	pServerEnd = pGroupArray->servers + pGroupArray->server_count;
	for (pServerInfo=pGroupArray->servers; \
			pServerInfo<pServerEnd; pServerInfo++)
	{
		if ((conn_result=fdht_connect_server_nb(pServerInfo, \
				g_fdht_connect_timeout)) != 0)
		{
			result = conn_result;
			(*fail_count)++;
		}
		else //connect success
		{
			(*success_count)++;
			if (bKeepAlive || pGroupArray->use_proxy)
			{
				tcpsetnodelay(pServerInfo->sock, 3600);
			}
		}
	}

	if (result != 0)
	{
		return result;
	}
	else
	{
		return  *success_count > 0 ? 0: ENOENT;
	}
}

void fdht_disconnect_all_servers(GroupArray *pGroupArray)
{
	FDHTServerInfo *pServerInfo;
	FDHTServerInfo *pServerEnd;

	if (pGroupArray->servers != NULL)
	{
		pServerEnd = pGroupArray->servers + pGroupArray->server_count;
		for (pServerInfo=pGroupArray->servers; \
				pServerInfo<pServerEnd; pServerInfo++)
		{
			if (pServerInfo->sock >= 0)
			{
				if (!pGroupArray->use_proxy)
				{
					fdht_quit(pServerInfo);
				}
				close(pServerInfo->sock);
				pServerInfo->sock = -1;
			}
		}
	}
}

int fdht_stat_ex(GroupArray *pGroupArray, const bool bKeepAlive, \
		const int server_index, char *buff, const int size)
{
	int result;
	int in_bytes;
	int i;
	FDHTProtoHeader header;
	FDHTServerInfo *pServer;

	memset(buff, 0, size);
	if (server_index < 0 || server_index > pGroupArray->server_count)
	{
		logError("invalid servier_index: %d", server_index);
		return EINVAL;
	}

	pServer = pGroupArray->servers + server_index;
	for (i=0; i<2; i++)
	{
	if ((result=fdht_connect_server_nb(pServer, \
			g_fdht_connect_timeout)) != 0)
	{
		return result;
	}

	if (bKeepAlive)
	{
		tcpsetnodelay(pServer->sock, 3600);
	}

	memset(&header, 0, sizeof(header));
	header.cmd = FDHT_PROTO_CMD_STAT;
	header.keep_alive = bKeepAlive;
	int2buff((int)time(NULL), header.timestamp);

	do
	{
		if ((result=tcpsenddata_nb(pServer->sock, &header, \
			sizeof(header), g_fdht_network_timeout)) != 0)
		{
			logError("send data to server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if ((result=fdht_recv_header(pServer, &in_bytes)) != 0)
		{
			break;
		}

		if (in_bytes >= size)
		{
			logError("server %s:%u reponse bytes: %d >= " \
				"buff size: %d", pServer->ip_addr, \
				pServer->port, in_bytes, size);
			result = ENOSPC;
			break;
		}

		if ((result=tcprecvdata_nb(pServer->sock, buff, \
			in_bytes, g_fdht_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"server: %s:%u, recv data fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pServer->ip_addr, \
				pServer->port, \
				result, STRERROR(result));
			break;
		}
	} while (0);

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

int fdht_get_sub_keys_ex(GroupArray *pGroupArray, const bool bKeepAlive, \
		FDHTObjectInfo *pObjectInfo, char *key_list, \
		const int key_size)
{
	int result;
	FDHTProtoHeader *pHeader;
	char hash_key[FDHT_MAX_FULL_KEY_LEN + 1];
	char buff[sizeof(FDHTProtoHeader) + FDHT_MAX_FULL_KEY_LEN];
	int in_bytes;
	int group_id;
	int hash_key_len;
	int key_hash_code;
	int i;
	char *p;
	ServerArray *pGroup;
	FDHTServerInfo *pServer;

	CALC_OBJECT_HASH_CODE(pObjectInfo, hash_key, hash_key_len, key_hash_code)
	group_id = ((unsigned int)key_hash_code) % pGroupArray->group_count;
	pGroup = pGroupArray->groups + group_id;

	result = ENOENT;
	for (i=0; i<=pGroup->count; i++)
	{
	pServer = get_readable_connection(pGroup, bKeepAlive, \
			key_hash_code, &result);
	if (pServer == NULL)
	{
		return result;
	}

	memset(buff, 0, sizeof(buff));
	pHeader = (FDHTProtoHeader *)buff;

	pHeader->cmd = FDHT_PROTO_CMD_GET_SUB_KEYS;
	pHeader->keep_alive = bKeepAlive;
	int2buff(key_hash_code, pHeader->key_hash_code);

	p = buff + sizeof(FDHTProtoHeader);
	PACK_BODY_OBJECT(pObjectInfo, p)

	do
	{
		int2buff((p - buff) - sizeof(FDHTProtoHeader), pHeader->pkg_len);
		if ((result=tcpsenddata_nb(pServer->sock, buff, p - buff, \
			g_fdht_network_timeout)) != 0)
		{
			logError("send data to server %s:%u fail, " \
				"errno: %d, error info: %s", \
				pServer->ip_addr, pServer->port, \
				result, STRERROR(result));
			break;
		}

		if ((result=fdht_recv_response(pServer, &key_list, \
					key_size - 1, &in_bytes)) != 0)
		{
			break;
		}

		*(key_list + in_bytes) = '\0';
	} while (0);

	if (bKeepAlive)
	{
		if (result >= ENETDOWN) //network error
		{
			fdht_disconnect_server(pServer);
			if (result == ENOTCONN)
			{
				continue;  //retry
			}
		}
	}
	else
	{
		fdht_disconnect_server(pServer);
	}

	break;
	}

	return result;
}

