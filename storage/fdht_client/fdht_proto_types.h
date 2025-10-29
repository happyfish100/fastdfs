/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//fdht_proto_types.h

#ifndef _FDHT_PROTO_TYPES_H_
#define _FDHT_PROTO_TYPES_H_

#define FDHT_PROTO_CMD_QUIT	10

#define FDHT_PROTO_CMD_SET		11
#define FDHT_PROTO_CMD_INC		12
#define FDHT_PROTO_CMD_GET		13
#define FDHT_PROTO_CMD_DEL		14
#define FDHT_PROTO_CMD_BATCH_SET	15
#define FDHT_PROTO_CMD_BATCH_GET	16
#define FDHT_PROTO_CMD_BATCH_DEL	17
#define FDHT_PROTO_CMD_STAT		18
#define FDHT_PROTO_CMD_GET_SUB_KEYS	19

#define FDHT_PROTO_CMD_SYNC_REQ	   21
#define FDHT_PROTO_CMD_SYNC_NOTIFY 22  //sync done notify
#define FDHT_PROTO_CMD_SYNC_SET	   23
#define FDHT_PROTO_CMD_SYNC_DEL	   24

#define FDHT_PROTO_CMD_HEART_BEAT  30

#define FDHT_PROTO_CMD_RESP        40

#define FDHT_PROTO_PKG_LEN_SIZE		4
#define FDHT_PROTO_CMD_SIZE		1

typedef int fdht_pkg_size_t;

#define PACK_BODY_UNTIL_KEY(pKeyInfo, p) \
	int2buff(pKeyInfo->namespace_len, p); \
	p += 4; \
	if (pKeyInfo->namespace_len > 0) \
	{ \
		memcpy(p, pKeyInfo->szNameSpace, pKeyInfo->namespace_len); \
		p += pKeyInfo->namespace_len; \
	} \
	int2buff(pKeyInfo->obj_id_len, p);  \
	p += 4; \
	if (pKeyInfo->obj_id_len> 0) \
	{ \
		memcpy(p, pKeyInfo->szObjectId, pKeyInfo->obj_id_len); \
		p += pKeyInfo->obj_id_len; \
	} \
	int2buff(pKeyInfo->key_len, p); \
	p += 4; \
	memcpy(p, pKeyInfo->szKey, pKeyInfo->key_len); \
	p += pKeyInfo->key_len; \


#define PACK_BODY_OBJECT(pObjectInfo, p) \
	int2buff(pObjectInfo->namespace_len, p); \
	p += 4; \
	memcpy(p, pObjectInfo->szNameSpace, pObjectInfo->namespace_len); \
	p += pObjectInfo->namespace_len; \
	int2buff(pObjectInfo->obj_id_len, p);  \
	p += 4; \
	memcpy(p, pObjectInfo->szObjectId, pObjectInfo->obj_id_len); \
	p += pObjectInfo->obj_id_len; \


typedef struct
{
	char pkg_len[FDHT_PROTO_PKG_LEN_SIZE];  //body length
	char key_hash_code[FDHT_PROTO_PKG_LEN_SIZE]; //the key hash code
	char timestamp[FDHT_PROTO_PKG_LEN_SIZE]; //current time

   	/* key expires, remain timeout = expires - timestamp */
	char expires[FDHT_PROTO_PKG_LEN_SIZE];
	char cmd;
	char keep_alive;
	char status;
} FDHTProtoHeader;

#endif

