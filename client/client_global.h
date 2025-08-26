/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//client_global.h

#ifndef _CLIENT_GLOBAL_H
#define _CLIENT_GLOBAL_H

#include "fastcommon/common_define.h"
#include "tracker_types.h"
#include "fdfs_shared_func.h"

typedef enum {
    fdfs_connect_first_by_tracker,
    fdfs_connect_first_by_last_connected
} FDFSConnectFirstBy;

#ifdef __cplusplus
extern "C" {
#endif

extern TrackerServerGroup g_tracker_group;

extern bool g_multi_storage_ips;
extern FDFSConnectFirstBy g_connect_first_by;
extern bool g_anti_steal_token;
extern BufferInfo g_anti_steal_secret_key;

#define fdfs_get_tracker_leader_index(leaderIp, leaderPort) \
	fdfs_get_tracker_leader_index_ex(&g_tracker_group, \
					leaderIp, leaderPort)
#ifdef __cplusplus
}
#endif

#endif
