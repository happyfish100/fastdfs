/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#ifndef TRACKER_CLIENT_H
#define TRACKER_CLIENT_H

#include "tracker_types.h"
#include "tracker_proto.h"
#include "client_global.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	bool if_trunk_server;
	char status;
	char id[FDFS_STORAGE_ID_MAX_SIZE];
	char ip_addr[IP_ADDRESS_SIZE];
	char src_id[FDFS_STORAGE_ID_MAX_SIZE];  //src storage id
	char domain_name[FDFS_DOMAIN_NAME_MAX_SIZE]; //http domain name
	char version[FDFS_VERSION_SIZE];
	int total_mb;  //total disk storage in MB
	int free_mb;  //free disk storage in MB
	int upload_priority;  //upload priority
	time_t join_time; //storage join timestamp (create timestamp)
	time_t up_time;   //storage service started timestamp
	int store_path_count;  //store base path count of each storage server
	int subdir_count_per_path;
	int storage_port;
	int storage_http_port; //storage http server port
	int current_write_path; //current write path index
        FDFSStorageStat stat;
} FDFSStorageInfo;


#define CHECK_CONNECTION(pTrackerServer, conn, result, new_connection) \
	do { \
		if (pTrackerServer->sock < 0) \
		{ \
			if ((conn=tracker_make_connection( \
				pTrackerServer, &result)) != NULL) \
			{ \
				return result; \
			} \
			new_connection = true; \
		} \
		else \
		{ \
			conn = pTrackerServer;  \
			new_connection = false; \
		} \
	} while (0)


#define tracker_get_connection() \
	tracker_get_connection_ex((&g_tracker_group))

/**
* get a connection to tracker server
* params:
*	pTrackerGroup: the tracker group
* return: != NULL for success, NULL for fail
**/
ConnectionInfo *tracker_get_connection_ex(TrackerServerGroup *pTrackerGroup);


#define tracker_get_connection_r(pTrackerServer, err_no) \
	tracker_get_connection_r_ex((&g_tracker_group), pTrackerServer, err_no)

/**
* get a connection to tracker server
* params:
*	pTrackerGroup: the tracker group
*       pTrackerServer: tracker server
* return: 0 success, !=0 fail
**/
ConnectionInfo *tracker_get_connection_r_ex(TrackerServerGroup *pTrackerGroup, \
		TrackerServerInfo *pTrackerServer, int *err_no);

#define tracker_get_all_connections() \
	tracker_get_all_connections_ex((&g_tracker_group))


/**
* get a connection to tracker server without connection pool
* params:
*	pTrackerGroup: the tracker group
* return: != NULL for success, NULL for fail
**/
ConnectionInfo *tracker_get_connection_no_pool( \
			TrackerServerGroup *pTrackerGroup);

/**
* connect to all tracker servers
* params:
*	pTrackerGroup: the tracker group
* return: 0 success, !=0 fail, return the error code
**/
int tracker_get_all_connections_ex(TrackerServerGroup *pTrackerGroup);

#define tracker_close_all_connections() \
	tracker_close_all_connections_ex((&g_tracker_group))

/**
* close all connections to tracker servers
* params:
*	pTrackerGroup: the tracker group
* return:
**/
void tracker_close_all_connections_ex(TrackerServerGroup *pTrackerGroup);

/**
* list one group
* params:
*	pTrackerServer: tracker server
*	group_name: the group name
*	pDest: return the group info
* return: 0 success, !=0 fail, return the error code
**/
int tracker_list_one_group(ConnectionInfo *pTrackerServer, \
		const char *group_name, FDFSGroupStat *pDest);


/**
* list all groups
* params:
*	pTrackerServer: tracker server
*	group_stats: return group info array
*	max_groups: max group count(group array capacity)
*	group_count: return group count
* return: 0 success, !=0 fail, return the error code
**/
int tracker_list_groups(ConnectionInfo *pTrackerServer, \
		FDFSGroupStat *group_stats, const int max_groups, \
		int *group_count);

/**
* list all servers of the specified group
* params:
*	pTrackerServer: tracker server
*	szGroupName: group name to query
*	szStorageId: the storage id to query, can be NULL or empty
*	storage_infos: return storage info array
*	max_storages: max storage count(storage array capacity)
*	storage_count: return storage count
* return: 0 success, !=0 fail, return the error code
**/
int tracker_list_servers(ConnectionInfo *pTrackerServer, \
		const char *szGroupName, const char *szStorageId, \
		FDFSStorageInfo *storage_infos, const int max_storages, \
		int *storage_count);

#define tracker_query_storage_store(pTrackerServer, pStorageServer, \
		group_name, store_path_index) \
	 tracker_query_storage_store_without_group(pTrackerServer, \
		pStorageServer, group_name, store_path_index)
/**
* query storage server to upload file
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*       store_path_index: return the index of path on the storage server
* return: 0 success, !=0 fail, return the error code
**/
int tracker_query_storage_store_without_group(ConnectionInfo *pTrackerServer,
		ConnectionInfo *pStorageServer, char *group_name, 
		int *store_path_index);

/**
* query storage servers/list to upload file
* params:
*	pTrackerServer: tracker server
*	storageServers: store the storage server list
*       nMaxServerCount: max storage server count
*	storage_count: return the storage server count
*       store_path_index: return the index of path on the storage server
* return: 0 success, !=0 fail, return the error code
**/
#define tracker_query_storage_store_list_without_group( \
		pTrackerServer, storageServers, nMaxServerCount, \
		storage_count, group_name, store_path_index) \
	tracker_query_storage_store_list_with_group( \
		pTrackerServer, NULL, storageServers, nMaxServerCount, \
		storage_count, store_path_index)

/**
* query storage server to upload file
* params:
*	pTrackerServer: tracker server
*       group_name: the group name to upload file to
*	pStorageServer: return storage server
*       store_path_index: return the index of path on the storage server
* return: 0 success, !=0 fail, return the error code
**/
int tracker_query_storage_store_with_group(ConnectionInfo *pTrackerServer, \
		const char *group_name, ConnectionInfo *pStorageServer, \
		int *store_path_index);

/**
* query storage servers/list to upload file
* params:
*	pTrackerServer: tracker server
*       group_name: the group name to upload file to
*	storageServers: store the storage server list
*       nMaxServerCount: max storage server count
*	storage_count: return the storage server count
*       store_path_index: return the index of path on the storage server
* return: 0 success, !=0 fail, return the error code
**/
int tracker_query_storage_store_list_with_group( \
	ConnectionInfo *pTrackerServer, const char *group_name, \
	ConnectionInfo *storageServers, const int nMaxServerCount, \
	int *storage_count, int *store_path_index);

/**
* query storage server to update (delete file or set meta data)
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*       group_name: the group name of storage server
*       filename: filename on storage server
* return: 0 success, !=0 fail, return the error code
**/
#define tracker_query_storage_update(pTrackerServer, \
		pStorageServer, group_name, filename) \
	tracker_do_query_storage(pTrackerServer, \
		pStorageServer, TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE,\
		group_name, filename)

/**
* query storage server to download file
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*       group_name: the group name of storage server
*       filename: filename on storage server
* return: 0 success, !=0 fail, return the error code
**/
#define tracker_query_storage_fetch(pTrackerServer, \
		pStorageServer, group_name, filename) \
	tracker_do_query_storage(pTrackerServer, \
		pStorageServer, TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE,\
		group_name, filename)

/**
* query storage server to fetch or update
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*       cmd : command, TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE or 
*             TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE
*       group_name: the group name of storage server
*       filename: filename on storage server
* return: 0 success, !=0 fail, return the error code
**/
int tracker_do_query_storage(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const byte cmd, \
		const char *group_name, const char *filename);

/**
* query storage server list to fetch file
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*       nMaxServerCount: max storage server count
*       server_count:  return storage server count
*       group_name: the group name of storage server
*       filename: filename on storage server
* return: 0 success, !=0 fail, return the error code
**/
int tracker_query_storage_list(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int nMaxServerCount, \
		int *server_count, char *group_name, const char *filename);

/**
* delete a storage server from cluster
* params:
*	pTrackerGroup: the tracker group
*	group_name: the group name which the storage server belongs to
*	storage_id: the storage server id
* return: 0 success, !=0 fail, return the error code
**/
int tracker_delete_storage(TrackerServerGroup *pTrackerGroup, \
		const char *group_name, const char *storage_id);


/**
* delete a group from cluster
* params:
*	pTrackerGroup: the tracker group
*	group_name: the group name to delete
* return: 0 success, !=0 fail, return the error code
**/
int tracker_delete_group(TrackerServerGroup *pTrackerGroup, \
		const char *group_name);

/**
* set trunk server of the specified group
* params:
*	pTrackerGroup: the tracker group
*	group_name: the group name which the storage server belongs to
*	storage_id: the storage server id, can be NULL or empty
*       new_trunk_server_id: the new trunk server id
* return: 0 success, !=0 fail, return the error code
**/
int tracker_set_trunk_server(TrackerServerGroup *pTrackerGroup, \
		const char *group_name, const char *storage_id, \
		char *new_trunk_server_id);


/**
* get storage server status from the tracker server
* params:
*	pTrackerServer: tracker server
*	group_name: the group name which the storage server belongs to
*	ip_addr: the ip addr of the storage server
*	pDestBuff: return the storage server brief info
* return: 0 success, !=0 fail, return the error code
**/
int tracker_get_storage_status(ConnectionInfo *pTrackerServer, \
		const char *group_name, const char *ip_addr, \
		FDFSStorageBrief *pDestBuff);


/**
* get storage server id from the tracker server
* params:
*	pTrackerServer: tracker server
*	group_name: the group name which the storage server belongs to
*	ip_addr: the ip addr of the storage server
*	storage_id: return the storage server id
* return: 0 success, !=0 fail, return the error code
**/
int tracker_get_storage_id(ConnectionInfo *pTrackerServer, \
		const char *group_name, const char *ip_addr, \
		char *storage_id);

/**
* get storage server highest level status from all tracker servers
* params:
*	pTrackerGroup: the tracker group
*	group_name: the group name which the storage server belongs to
*	ip_addr: the ip addr of the storage server
*	storage_id: return the storage server id
*	status: return the highest level status
* return: 0 success, !=0 fail, return the error code
**/
int tracker_get_storage_max_status(TrackerServerGroup *pTrackerGroup, \
		const char *group_name, const char *ip_addr, \
		char *storage_id, int *status);

#ifdef __cplusplus
}
#endif

#endif
