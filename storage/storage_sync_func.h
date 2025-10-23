/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//storage_sync_func.h

#ifndef _STORAGE_SYNC_FUNC_H_
#define _STORAGE_SYNC_FUNC_H_

#include "fastcommon/common_define.h"

#ifdef __cplusplus
extern "C" {
#endif

int storage_sync_connect_storage_server_ex(const FDFSStorageBrief *pStorage,
        ConnectionInfo *conn, bool *check_flag);

static inline int storage_sync_connect_storage_server_always(
        const FDFSStorageBrief *pStorage, ConnectionInfo *conn)
{
    bool check_flag = true;
    return storage_sync_connect_storage_server_ex(
            pStorage, conn, &check_flag);
}

static inline int storage_sync_connect_storage_server_once(
        const FDFSStorageBrief *pStorage, ConnectionInfo *conn)
{
    bool check_flag = false;
    return storage_sync_connect_storage_server_ex(
            pStorage, conn, &check_flag);
}

#ifdef __cplusplus
}
#endif

#endif
