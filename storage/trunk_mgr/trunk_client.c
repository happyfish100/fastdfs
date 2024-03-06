/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//trunk_client.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "trunk_client.h"

static int trunk_client_trunk_do_alloc_space(ConnectionInfo *pTrunkServer, \
		const int file_size, FDFSTrunkFullInfo *pTrunkInfo)
{
	TrackerHeader *pHeader;
	char *p;
	int result;
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + 5];
    char formatted_ip[FORMATTED_IP_SIZE];
	FDFSTrunkInfoBuff trunkBuff;
	int64_t in_bytes;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	p = out_buff + sizeof(TrackerHeader);
	snprintf(p, sizeof(out_buff) - sizeof(TrackerHeader), \
		"%s", g_group_name);
	p += FDFS_GROUP_NAME_MAX_LEN;
	int2buff(file_size, p);
	p += 4;
	*p++ = pTrunkInfo->path.store_path_index;
	long2buff(FDFS_GROUP_NAME_MAX_LEN + 5, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_TRUNK_ALLOC_SPACE;

	if ((result=tcpsenddata_nb(pTrunkServer->sock, out_buff,
			sizeof(out_buff), SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrunkServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
            pTrunkServer->port, result, STRERROR(result));
		return result;
	}

	p = (char *)&trunkBuff;
	if ((result=fdfs_recv_response(pTrunkServer, \
		&p, sizeof(FDFSTrunkInfoBuff), &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response fail, result: %d",
                __LINE__, result);
		return result;
	}

	if (in_bytes != sizeof(FDFSTrunkInfoBuff))
	{
        format_ip_address(pTrunkServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"storage server %s:%u, recv body length: %d invalid, "
			"expect body length: %d", __LINE__, formatted_ip,
            pTrunkServer->port, (int)in_bytes,
            (int)sizeof(FDFSTrunkInfoBuff));
		return EINVAL;
	}

	pTrunkInfo->path.store_path_index = trunkBuff.store_path_index;
	pTrunkInfo->path.sub_path_high = trunkBuff.sub_path_high;
	pTrunkInfo->path.sub_path_low = trunkBuff.sub_path_low;
	pTrunkInfo->file.id = buff2int(trunkBuff.id);
	pTrunkInfo->file.offset = buff2int(trunkBuff.offset);
	pTrunkInfo->file.size = buff2int(trunkBuff.size);
	pTrunkInfo->status = FDFS_TRUNK_STATUS_HOLD;

	return 0;
}

static int trunk_client_connect_trunk_server(TrackerServerInfo *trunk_server,
        ConnectionInfo **conn, const char *prompt)
{
	int result;
    char formatted_ip[FORMATTED_IP_SIZE];

	if (g_trunk_server.count == 0)
	{
		logError("file: "__FILE__", line: %d, "
			"no trunk server", __LINE__);
		return EAGAIN;
	}

	memcpy(trunk_server, &g_trunk_server, sizeof(TrackerServerInfo));
	if ((*conn=tracker_connect_server(trunk_server, &result)) == NULL)
	{
        format_ip_address(trunk_server->connections[0].
                ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"%s because connect to trunk server %s:%u fail, "
            "errno: %d", __LINE__, prompt, formatted_ip,
            trunk_server->connections[0].port, result);
		return result;
	}

    if (g_trunk_server.index != trunk_server->index)
    {
        g_trunk_server.index = trunk_server->index;
    }

    return 0;
}

int trunk_client_trunk_alloc_space(const int file_size, \
		FDFSTrunkFullInfo *pTrunkInfo)
{
	int result;
	TrackerServerInfo trunk_server;
	ConnectionInfo *pTrunkServer;

	if (g_if_trunker_self)
	{
		return trunk_alloc_space(file_size, pTrunkInfo);
	}

    if ((result=trunk_client_connect_trunk_server(&trunk_server,
                    &pTrunkServer, "can't alloc trunk space")) != 0)
    {
        return result;
    }

	result = trunk_client_trunk_do_alloc_space(pTrunkServer, \
			file_size, pTrunkInfo);

	tracker_close_connection_ex(pTrunkServer, result != 0);
	return result;
}

#define trunk_client_trunk_do_alloc_confirm(pTrunkServer, pTrunkInfo, status) \
	trunk_client_trunk_confirm_or_free(pTrunkServer, pTrunkInfo, \
		STORAGE_PROTO_CMD_TRUNK_ALLOC_CONFIRM, status)

#define trunk_client_trunk_do_free_space(pTrunkServer, pTrunkInfo) \
	trunk_client_trunk_confirm_or_free(pTrunkServer, pTrunkInfo, \
		STORAGE_PROTO_CMD_TRUNK_FREE_SPACE, 0)

static int trunk_client_trunk_confirm_or_free(ConnectionInfo *pTrunkServer,\
		const FDFSTrunkFullInfo *pTrunkInfo, const int cmd, \
		const int status)
{
	TrackerHeader *pHeader;
	FDFSTrunkInfoBuff *pTrunkBuff;
	int64_t in_bytes;
	int result;
	char out_buff[sizeof(TrackerHeader) \
		+ STORAGE_TRUNK_ALLOC_CONFIRM_REQ_BODY_LEN];
    char formatted_ip[FORMATTED_IP_SIZE];

	pHeader = (TrackerHeader *)out_buff;
	pTrunkBuff = (FDFSTrunkInfoBuff *)(out_buff + sizeof(TrackerHeader) \
			 + FDFS_GROUP_NAME_MAX_LEN);
	memset(out_buff, 0, sizeof(out_buff));
	snprintf(out_buff + sizeof(TrackerHeader), sizeof(out_buff) - \
		sizeof(TrackerHeader),  "%s", g_group_name);
	long2buff(STORAGE_TRUNK_ALLOC_CONFIRM_REQ_BODY_LEN, pHeader->pkg_len);
	pHeader->cmd = cmd;
	pHeader->status = status;

	pTrunkBuff->store_path_index = pTrunkInfo->path.store_path_index;
	pTrunkBuff->sub_path_high = pTrunkInfo->path.sub_path_high;
	pTrunkBuff->sub_path_low = pTrunkInfo->path.sub_path_low;
	int2buff(pTrunkInfo->file.id, pTrunkBuff->id);
	int2buff(pTrunkInfo->file.offset, pTrunkBuff->offset);
	int2buff(pTrunkInfo->file.size, pTrunkBuff->size);

	if ((result=tcpsenddata_nb(pTrunkServer->sock, out_buff,
			sizeof(out_buff), SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pTrunkServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
            pTrunkServer->port, result, STRERROR(result));
		return result;
	}

	if ((result=fdfs_recv_header(pTrunkServer, &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
                "fdfs_recv_header fail, result: %d",
                __LINE__, result);
		return result;
	}

	if (in_bytes != 0)
	{
        format_ip_address(pTrunkServer->ip_addr, formatted_ip);
		logError("file: "__FILE__", line: %d, "
			"storage server %s:%u response data length: "
			"%"PRId64" is invalid, should == 0", __LINE__,
            formatted_ip, pTrunkServer->port, in_bytes);
		return EINVAL;
	}

	return 0;
}

int trunk_client_trunk_alloc_confirm(const FDFSTrunkFullInfo *pTrunkInfo, \
		const int status)
{
	int result;
	TrackerServerInfo trunk_server;
	ConnectionInfo *pTrunkServer;

	if (g_if_trunker_self)
	{
		return trunk_alloc_confirm(pTrunkInfo, status);
	}

    if ((result=trunk_client_connect_trunk_server(&trunk_server,
                    &pTrunkServer, "trunk alloc confirm fail")) != 0)
    {
        return result;
    }

	result = trunk_client_trunk_do_alloc_confirm(pTrunkServer, \
			pTrunkInfo, status);

	tracker_close_connection_ex(pTrunkServer, result != 0);
	return result;
}

int trunk_client_trunk_free_space(const FDFSTrunkFullInfo *pTrunkInfo)
{
	int result;
	TrackerServerInfo trunk_server;
	ConnectionInfo *pTrunkServer;

	if (g_if_trunker_self)
	{
		return trunk_free_space(pTrunkInfo, true);
	}

    if ((result=trunk_client_connect_trunk_server(&trunk_server,
                    &pTrunkServer, "free trunk space fail")) != 0)
    {
        return result;
    }

	result = trunk_client_trunk_do_free_space(pTrunkServer, pTrunkInfo);
	tracker_close_connection_ex(pTrunkServer, result != 0);
	return result;
}

