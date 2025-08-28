/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//fdfs_server_id_func.h

#ifndef _FDFS_SERVER_ID_FUNC_H
#define _FDFS_SERVER_ID_FUNC_H 

#include "fastcommon/common_define.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/logger.h"
#include "tracker_types.h"

#define STORAGE_RW_OPTION_TAG_STR  "rw="
#define STORAGE_RW_OPTION_TAG_LEN  (sizeof(STORAGE_RW_OPTION_TAG_STR) - 1)

#define STORAGE_RW_OPTION_VALUE_NONE_STR  "none"
#define STORAGE_RW_OPTION_VALUE_NONE_LEN  \
    (sizeof(STORAGE_RW_OPTION_VALUE_NONE_STR) - 1)

#define STORAGE_RW_OPTION_VALUE_READ_STR  "read"
#define STORAGE_RW_OPTION_VALUE_READ_LEN  \
    (sizeof(STORAGE_RW_OPTION_VALUE_READ_STR) - 1)

#define STORAGE_RW_OPTION_VALUE_READONLY_STR  "readonly"
#define STORAGE_RW_OPTION_VALUE_READONLY_LEN  \
    (sizeof(STORAGE_RW_OPTION_VALUE_READONLY_STR) - 1)

#define STORAGE_RW_OPTION_VALUE_WRITE_STR  "write"
#define STORAGE_RW_OPTION_VALUE_WRITE_LEN  \
    (sizeof(STORAGE_RW_OPTION_VALUE_WRITE_STR) - 1)

#define STORAGE_RW_OPTION_VALUE_WRITEONLY_STR  "writeonly"
#define STORAGE_RW_OPTION_VALUE_WRITEONLY_LEN  \
    (sizeof(STORAGE_RW_OPTION_VALUE_WRITEONLY_STR) - 1)

#define STORAGE_RW_OPTION_VALUE_BOTH_STR  "both"
#define STORAGE_RW_OPTION_VALUE_BOTH_LEN  \
    (sizeof(STORAGE_RW_OPTION_VALUE_BOTH_STR) - 1)

#define STORAGE_RW_OPTION_VALUE_ALL_STR   "all"
#define STORAGE_RW_OPTION_VALUE_ALL_LEN   \
    (sizeof(STORAGE_RW_OPTION_VALUE_ALL_STR) - 1)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    char id[FDFS_STORAGE_ID_MAX_SIZE];
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 8];  //for 8 bytes alignment
    FDFSMultiIP ip_addrs;
    int port;   //since v5.05
    FDFSReadWriteMode rw_mode;  //since v6.13
} FDFSStorageIdInfo;

typedef struct
{
    const char *group_name;
    const char *ip_addr;
    int port;
    FDFSStorageIdInfo *idInfo;
} FDFSStorageIdMap;

typedef struct
{
    int count;
    FDFSStorageIdInfo *ids;
} FDFSStorageIdInfoArray;

typedef struct
{
    int count;
    FDFSStorageIdMap *maps;
} FDFSStorageIdMapArray;

extern FDFSStorageIdInfoArray g_storage_ids_by_id;  //sorted by storage ID
extern FDFSStorageIdMapArray g_storage_ids_by_ip;  //sorted by group name and storage IP

bool fdfs_is_server_id_valid(const char *id);

static inline int fdfs_get_server_id_type(const int id)
{
    if (id > 0 && id <= FDFS_MAX_SERVER_ID)
    {
        return FDFS_ID_TYPE_SERVER_ID;
    }
    else
    {
        return FDFS_ID_TYPE_IP_ADDRESS;
    }
}

int fdfs_load_storage_ids(char *content, const char *pStorageIdsFilename);

FDFSStorageIdInfo *fdfs_get_storage_by_id(const char *id);

FDFSStorageIdInfo *fdfs_get_storage_id_by_ip(const char *group_name,
		const char *pIpAddr);

FDFSStorageIdInfo *fdfs_get_storage_id_by_ip_port(const char *pIpAddr,
        const int port);

int fdfs_check_storage_id(const char *group_name, const char *id);

int fdfs_get_storage_ids_from_tracker_server(TrackerServerInfo *pTrackerServer);

int fdfs_get_storage_ids_from_tracker_group(TrackerServerGroup *pTrackerGroup);

int fdfs_load_storage_ids_from_file(const char *config_filename,
		IniContext *pItemContext);

static inline const char *fdfs_get_storage_rw_caption(
        const FDFSReadWriteMode rw_mode)
{
    switch (rw_mode)
    {
        case fdfs_rw_none:
            return STORAGE_RW_OPTION_VALUE_NONE_STR;
        case fdfs_rw_readonly:
            return STORAGE_RW_OPTION_VALUE_READ_STR;
        case fdfs_rw_writeonly:
            return STORAGE_RW_OPTION_VALUE_WRITE_STR;
        default:
            return STORAGE_RW_OPTION_VALUE_BOTH_STR;
    }
}

#ifdef __cplusplus
}
#endif

#endif

