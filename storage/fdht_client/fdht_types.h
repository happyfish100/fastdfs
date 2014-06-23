/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//fdht_types.h

#ifndef _FDHT_TYPES_H
#define _FDHT_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdht_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FDHT_MAX_NAMESPACE_LEN	 64
#define FDHT_MAX_OBJECT_ID_LEN	128
#define FDHT_MAX_SUB_KEY_LEN	128
#define FDHT_FULL_KEY_SEPERATOR	'\x1'

#define FDHT_EXPIRES_NEVER	 0  //never timeout
#define FDHT_EXPIRES_NONE	-1  //invalid timeout, should ignore

#define FDHT_MAX_FULL_KEY_LEN    (FDHT_MAX_NAMESPACE_LEN + 1 + \
			FDHT_MAX_OBJECT_ID_LEN + 1 + FDHT_MAX_SUB_KEY_LEN)

#define FDHT_PACK_FULL_KEY(key_info, full_key, full_key_len, p) \
	p = full_key; \
	if (key_info.namespace_len > 0) \
	{ \
		memcpy(p, key_info.szNameSpace, key_info.namespace_len); \
		p += key_info.namespace_len; \
	} \
	*p++ = FDHT_FULL_KEY_SEPERATOR; /*field seperator*/  \
	if (key_info.obj_id_len > 0) \
	{ \
		memcpy(p, key_info.szObjectId, key_info.obj_id_len); \
		p += key_info.obj_id_len; \
	} \
	*p++ = FDHT_FULL_KEY_SEPERATOR; /*field seperator*/  \
	memcpy(p, key_info.szKey, key_info.key_len); \
	p += key_info.key_len; \
	full_key_len = p - full_key;
	

typedef struct
{
	int namespace_len;
	int obj_id_len;
	int key_len;
	char szNameSpace[FDHT_MAX_NAMESPACE_LEN + 1];
	char szObjectId[FDHT_MAX_OBJECT_ID_LEN + 1];
	char szKey[FDHT_MAX_SUB_KEY_LEN + 1];
} FDHTKeyInfo;

typedef struct
{
	int namespace_len;
	int obj_id_len;
	char szNameSpace[FDHT_MAX_NAMESPACE_LEN + 1];
	char szObjectId[FDHT_MAX_OBJECT_ID_LEN + 1];
} FDHTObjectInfo;

typedef struct
{
	int key_len;
	char szKey[FDHT_MAX_SUB_KEY_LEN + 1];
} FDHTSubKey;

typedef struct
{
	int key_len;
	int value_len;
	char szKey[FDHT_MAX_SUB_KEY_LEN + 1];
	char *pValue;
	char status;
} FDHTKeyValuePair;

typedef struct
{
	int sock;
	int port;
	char ip_addr[IP_ADDRESS_SIZE];
} FDHTServerInfo;

typedef struct
{
	char ip_addr[IP_ADDRESS_SIZE];
	bool sync_old_done;
	int port;
	int sync_req_count;    //sync req count
	int64_t update_count;  //runtime var
} FDHTGroupServer;

typedef struct {
	uint64_t total_set_count;
	uint64_t success_set_count;
	uint64_t total_inc_count;
	uint64_t success_inc_count;
	uint64_t total_delete_count;
	uint64_t success_delete_count;
	uint64_t total_get_count;
	uint64_t success_get_count;
} FDHTServerStat;

typedef struct
{
	FDHTServerInfo **servers;
	int count;  //server count
} ServerArray;

typedef struct
{
	ServerArray *groups;
	FDHTServerInfo *servers;
	int group_count;  //group count
	int server_count;
	FDHTServerInfo proxy_server;
	bool use_proxy;
} GroupArray;

#ifdef __cplusplus
}
#endif

#endif

