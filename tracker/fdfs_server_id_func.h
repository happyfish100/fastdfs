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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	char id[FDFS_STORAGE_ID_MAX_SIZE];
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 8];  //for 8 bytes alignment
	FDFSMultiIP ip_addrs;
    int port;   //since v5.05
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

int fdfs_get_server_id_type(const int id);

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

#ifdef __cplusplus
}
#endif

#endif

