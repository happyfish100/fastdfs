/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//fdfs_shared_func.h

#ifndef _FDFS_SHARED_FUNC_H
#define _FDFS_SHARED_FUNC_H

#include "fastcommon/common_define.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/logger.h"
#include "tracker_types.h"
#include "fdfs_server_id_func.h"

#define FDFS_IP_TYPE_UNKNOWN      0
#define FDFS_IP_TYPE_PRIVATE_10   1
#define FDFS_IP_TYPE_PRIVATE_172  2
#define FDFS_IP_TYPE_PRIVATE_192  3
#define FDFS_IP_TYPE_OUTER        4

#define FDFS_IS_AVAILABLE_STATUS(status) \
    (status == FDFS_STORAGE_STATUS_OFFLINE || \
     status == FDFS_STORAGE_STATUS_ONLINE  || \
     status == FDFS_STORAGE_STATUS_ACTIVE)

#ifdef __cplusplus
extern "C" {
#endif

int fdfs_get_tracker_leader_index_ex(TrackerServerGroup *pServerGroup, \
		const char *leaderIp, const int leaderPort);

int fdfs_parse_storage_reserved_space(IniContext *pIniContext, \
		FDFSStorageReservedSpace *pStorageReservedSpace);

const char *fdfs_storage_reserved_space_to_string(FDFSStorageReservedSpace \
			*pStorageReservedSpace, char *buff);

const char *fdfs_storage_reserved_space_to_string_ex(const bool flag, \
	const int space_mb, const int total_mb, const double space_ratio, \
	char *buff);

int fdfs_get_storage_reserved_space_mb(const int total_mb, \
		FDFSStorageReservedSpace *pStorageReservedSpace);

bool fdfs_check_reserved_space(FDFSGroupInfo *pGroup, \
	FDFSStorageReservedSpace *pStorageReservedSpace);

bool fdfs_check_reserved_space_trunk(FDFSGroupInfo *pGroup, \
	FDFSStorageReservedSpace *pStorageReservedSpace);

bool fdfs_check_reserved_space_path(const int64_t total_mb, \
	const int64_t free_mb, const int avg_mb, \
	FDFSStorageReservedSpace *pStorageReservedSpace);

int fdfs_connection_pool_init(const char *config_filename, \
		IniContext *pItemContext);

void fdfs_connection_pool_destroy();

void fdfs_set_log_rotate_size(LogContext *pContext, const int64_t log_rotate_size);

bool fdfs_server_contain(TrackerServerInfo *pServerInfo,
        const char *target_ip, const int target_port);

static inline bool fdfs_server_contain1(TrackerServerInfo *pServerInfo,
        const ConnectionInfo *target)
{
    return fdfs_server_contain(pServerInfo, target->ip_addr, target->port);
}

bool fdfs_server_contain_ex(TrackerServerInfo *pServer1,
        TrackerServerInfo *pServer2);

bool fdfs_server_equal(TrackerServerInfo *pServer1,
        TrackerServerInfo *pServer2);

bool fdfs_server_contain_local_service(TrackerServerInfo *pServerInfo,
        const int target_port);

/**
* tracker group get server
* params:
*       pGroup: the tracker group
*       target_ip: the ip address to find
*       target_port: the port to find
* return: TrackerServerInfo pointer contain target ip and port
**/
TrackerServerInfo *fdfs_tracker_group_get_server(TrackerServerGroup *pGroup,
        const char *target_ip, const int target_port);

void fdfs_server_sock_reset(TrackerServerInfo *pServerInfo);

int fdfs_parse_server_info_ex(char *server_str, const int default_port,
        TrackerServerInfo *pServer, const bool resolve);

static inline int fdfs_parse_server_info(char *server_str, const int default_port,
        TrackerServerInfo *pServer)
{
    const bool resolve = true;
    return fdfs_parse_server_info_ex(server_str, default_port,
            pServer, resolve);
}

int fdfs_server_info_to_string_ex(const TrackerServerInfo *pServer,
        const int port, char *buff, const int buffSize);

static inline int fdfs_server_info_to_string(const TrackerServerInfo *pServer,
        char *buff, const int buffSize)
{
    return fdfs_server_info_to_string_ex(pServer,
            pServer->connections[0].port, buff, buffSize);
}

int fdfs_multi_ips_to_string_ex(const FDFSMultiIP *ip_addrs,
        const char seperator, char *buff, const int buffSize);

static inline int fdfs_multi_ips_to_string(const FDFSMultiIP *ip_addrs,
        char *buff, const int buffSize)
{
    const char seperator = ',';
    return fdfs_multi_ips_to_string_ex(ip_addrs, seperator, buff, buffSize);
}

int fdfs_parse_multi_ips_ex(char *ip_str, FDFSMultiIP *ip_addrs,
        char *error_info, const int error_size, const bool resolve);

static inline int fdfs_parse_multi_ips(char *ip_str, FDFSMultiIP *ip_addrs,
        char *error_info, const int error_size)
{
    const bool resolve = true;
    return fdfs_parse_multi_ips_ex(ip_str, ip_addrs,
            error_info, error_size, resolve);
}

int fdfs_get_ip_type(const char* ip);

int fdfs_check_server_ips(const TrackerServerInfo *pServer,
        char *error_info, const int error_size);

int fdfs_check_and_format_ips(FDFSMultiIP *ip_addrs,
        char *error_info, const int error_size);

const char *fdfs_get_ipaddr_by_peer_ip(const FDFSMultiIP *ip_addrs,
        const char *client_ip);

void fdfs_set_multi_ip_index(FDFSMultiIP *multi_ip, const char *target_ip);

void fdfs_set_server_info_index(TrackerServerInfo *pServer,
        const char *target_ip, const int target_port);

static inline void fdfs_set_server_info_index1(TrackerServerInfo *pServer,
        const ConnectionInfo *target)
{
    return fdfs_set_server_info_index(pServer,
            target->ip_addr, target->port);
}

void fdfs_set_server_info(TrackerServerInfo *pServer,
        const char *ip_addr, const int port);

void fdfs_set_server_info_ex(TrackerServerInfo *pServer,
        const FDFSMultiIP *ip_addrs, const int port);

#ifdef __cplusplus
}
#endif

#endif

