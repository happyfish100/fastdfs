/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <netdb.h>
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/local_ip_func.h"
#include "tracker_proto.h"
#include "fdfs_global.h"
#include "fdfs_shared_func.h"


bool fdfs_server_contain(TrackerServerInfo *pServerInfo,
        const char *target_ip, const int target_port)
{
	ConnectionInfo *conn;
	ConnectionInfo *end;

    if (pServerInfo->count == 1)
    {
		return FC_CONNECTION_SERVER_EQUAL(pServerInfo->connections[0],
                target_ip, target_port);
    }
    else if (pServerInfo->count == 2)
    {
		return FC_CONNECTION_SERVER_EQUAL(pServerInfo->connections[0],
                target_ip, target_port) ||
            FC_CONNECTION_SERVER_EQUAL(pServerInfo->connections[1],
                    target_ip, target_port);
    }

	end = pServerInfo->connections + pServerInfo->count;
	for (conn=pServerInfo->connections; conn<end; conn++)
    {
		if (FC_CONNECTION_SERVER_EQUAL(*conn, target_ip, target_port))
        {
            return true;
        }
    }

    return false;
}

bool fdfs_server_contain_ex(TrackerServerInfo *pServer1,
        TrackerServerInfo *pServer2)
{
	ConnectionInfo *conn;
	ConnectionInfo *end;

    if (pServer1->count == 1)
    {
        return fdfs_server_contain1(pServer2, pServer1->connections + 0);
    }
    else if (pServer1->count == 2)
    {
        if (fdfs_server_contain1(pServer2, pServer1->connections + 0))
        {
            return true;
        }
        return fdfs_server_contain1(pServer2, pServer1->connections + 1);
    }

	end = pServer1->connections + pServer1->count;
	for (conn=pServer1->connections; conn<end; conn++)
    {
		if (fdfs_server_contain1(pServer2, conn))
        {
            return true;
        }
    }

    return false;
}

bool fdfs_server_equal(TrackerServerInfo *pServer1,
        TrackerServerInfo *pServer2)
{
	ConnectionInfo *conn;
	ConnectionInfo *end;

    if (pServer1->count != pServer2->count)
    {
        return false;
    }

    if (pServer1->count == 1)
    {
        return (pServer1->connections->port == pServer2->connections->port &&
            strcmp(pServer1->connections->ip_addr, pServer2->connections->ip_addr) == 0);
    }

	end = pServer1->connections + pServer1->count;
	for (conn=pServer1->connections; conn<end; conn++)
    {
		if (!fdfs_server_contain1(pServer2, conn))
        {
            return false;
        }
    }

    return true;
}

bool fdfs_server_contain_local_service(TrackerServerInfo *pServerInfo,
        const int target_port)
{
    const char *current_ip;
    
    current_ip = get_first_local_ip();
    while (current_ip != NULL)
    {
        if (fdfs_server_contain(pServerInfo, current_ip, target_port))
        {
            return true;
        }
        current_ip = get_next_local_ip(current_ip);
    }

    return false;
}

TrackerServerInfo *fdfs_tracker_group_get_server(TrackerServerGroup *pGroup,
        const char *target_ip, const int target_port)
{
    TrackerServerInfo *pServer;
    TrackerServerInfo *pEnd;

    pEnd = pGroup->servers + pGroup->server_count;
    for (pServer=pGroup->servers; pServer<pEnd; pServer++)
    {
        if (fdfs_server_contain(pServer, target_ip, target_port))
        {
            return pServer;
        }
    }

    return NULL;
}

void fdfs_server_sock_reset(TrackerServerInfo *pServerInfo)
{
	ConnectionInfo *conn;
	ConnectionInfo *end;

    if (pServerInfo->count == 1)
    {
		pServerInfo->connections[0].sock = -1;
    }
    else if (pServerInfo->count == 2)
    {
		pServerInfo->connections[0].sock = -1;
		pServerInfo->connections[1].sock = -1;
    }
    else
    {
        end = pServerInfo->connections + pServerInfo->count;
        for (conn=pServerInfo->connections; conn<end; conn++)
        {
            conn->sock = -1;
        }
    }
}

int fdfs_get_tracker_leader_index_ex(TrackerServerGroup *pServerGroup,
		const char *leaderIp, const int leaderPort)
{
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;

	if (pServerGroup->server_count == 0)
	{
		return -1;
	}

	pEnd = pServerGroup->servers + pServerGroup->server_count;
	for (pServer=pServerGroup->servers; pServer<pEnd; pServer++)
	{
        if (fdfs_server_contain(pServer, leaderIp, leaderPort))
		{
			return pServer - pServerGroup->servers;
		}
	}

	return -1;
}

int fdfs_parse_storage_reserved_space(IniContext *pIniContext,
		FDFSStorageReservedSpace *pStorageReservedSpace)
{
	int result;
	int len;
	char *pReservedSpaceStr;
	int64_t storage_reserved;

	pReservedSpaceStr = iniGetStrValue(NULL, \
			"reserved_storage_space", pIniContext);
	if (pReservedSpaceStr == NULL)
	{
		pStorageReservedSpace->flag = \
				TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB;
		pStorageReservedSpace->rs.mb = FDFS_DEF_STORAGE_RESERVED_MB;
		return 0;
	}
	if (*pReservedSpaceStr == '\0')
	{
		logError("file: "__FILE__", line: %d, " \
			"item \"reserved_storage_space\" is empty!", \
			__LINE__);
		return EINVAL;
	}

	len = strlen(pReservedSpaceStr);
	if (*(pReservedSpaceStr + len - 1) == '%')
	{
		char *endptr;
		pStorageReservedSpace->flag = \
				TRACKER_STORAGE_RESERVED_SPACE_FLAG_RATIO;
		endptr = NULL;
		*(pReservedSpaceStr + len - 1) = '\0';
		pStorageReservedSpace->rs.ratio = \
					strtod(pReservedSpaceStr, &endptr);
		if (endptr != NULL && *endptr != '\0')
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"reserved_storage_space\": %s%%"\
				" is invalid!", __LINE__, \
				pReservedSpaceStr);
			return EINVAL;
		}

		if (pStorageReservedSpace->rs.ratio <= 0.00 || \
			pStorageReservedSpace->rs.ratio >= 100.00)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"reserved_storage_space\": %s%%"\
				" is invalid!", __LINE__, \
				pReservedSpaceStr);
			return EINVAL;
		}

		pStorageReservedSpace->rs.ratio /= 100.00;
		return 0;
	}

	if ((result=parse_bytes(pReservedSpaceStr, 1, &storage_reserved)) != 0)
	{
		return result;
	}

	pStorageReservedSpace->flag = TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB;
	pStorageReservedSpace->rs.mb = storage_reserved / FDFS_ONE_MB;
	return 0;
}

const char *fdfs_storage_reserved_space_to_string(FDFSStorageReservedSpace \
			*pStorageReservedSpace, char *buff)
{
	if (pStorageReservedSpace->flag == \
			TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB)
	{
		sprintf(buff, "%dMB", pStorageReservedSpace->rs.mb);
	}
	else
	{
		sprintf(buff, "%.2f%%", 100.00 * \
			pStorageReservedSpace->rs.ratio);
	}

	return buff;
}

const char *fdfs_storage_reserved_space_to_string_ex(const bool flag, \
	const int space_mb, const int total_mb, const double space_ratio, \
	char *buff)
{
	if (flag == TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB)
	{
		sprintf(buff, "%d MB", space_mb);
	}
	else
	{
		sprintf(buff, "%d MB(%.2f%%)", (int)(total_mb * space_ratio), \
			 100.00 * space_ratio);
	}

	return buff;
}

int fdfs_get_storage_reserved_space_mb(const int total_mb, \
		FDFSStorageReservedSpace *pStorageReservedSpace)
{
	if (pStorageReservedSpace->flag == \
			TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB)
	{
		return pStorageReservedSpace->rs.mb;
	}
	else
	{
		return (int)(total_mb * pStorageReservedSpace->rs.ratio);
	}
}

bool fdfs_check_reserved_space(FDFSGroupInfo *pGroup, \
	FDFSStorageReservedSpace *pStorageReservedSpace)
{
	if (pStorageReservedSpace->flag == \
			TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB)
	{
		return pGroup->free_mb > pStorageReservedSpace->rs.mb;
	}
	else
	{
		if (pGroup->total_mb == 0)
		{
			return false;
		}

		/*
		logInfo("storage=%.4f, rs.ratio=%.4f", 
			((double)pGroup->free_mb / (double)pGroup->total_mb),
			pStorageReservedSpace->rs.ratio);
		*/

		return ((double)pGroup->free_mb / (double)pGroup->total_mb) > \
			pStorageReservedSpace->rs.ratio;
	}
}

bool fdfs_check_reserved_space_trunk(FDFSGroupInfo *pGroup, \
	FDFSStorageReservedSpace *pStorageReservedSpace)
{
	if (pStorageReservedSpace->flag == \
			TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB)
	{
		return (pGroup->free_mb + pGroup->trunk_free_mb > 
			pStorageReservedSpace->rs.mb);
	}
	else
	{
		if (pGroup->total_mb == 0)
		{
			return false;
		}

		/*
		logInfo("storage trunk=%.4f, rs.ratio=%.4f", 
		((double)(pGroup->free_mb + pGroup->trunk_free_mb) / \
		(double)pGroup->total_mb), pStorageReservedSpace->rs.ratio);
		*/

		return ((double)(pGroup->free_mb + pGroup->trunk_free_mb) / \
		(double)pGroup->total_mb) > pStorageReservedSpace->rs.ratio;
	}
}

bool fdfs_check_reserved_space_path(const int64_t total_mb, \
	const int64_t free_mb, const int avg_mb, \
	FDFSStorageReservedSpace *pStorageReservedSpace)
{
	if (pStorageReservedSpace->flag == \
			TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB)
	{
		return free_mb > avg_mb;
	}
	else
	{
		if (total_mb == 0)
		{
			return false;
		}

		/*
		logInfo("storage path, free_mb=%"PRId64 \
			", total_mb=%"PRId64", " \
			"real ratio=%.4f, rs.ratio=%.4f", \
			free_mb, total_mb, ((double)free_mb / total_mb), \
			pStorageReservedSpace->rs.ratio);
		*/

		return ((double)free_mb / (double)total_mb) > \
			pStorageReservedSpace->rs.ratio;
	}
}

int fdfs_connection_pool_init(const char *config_filename, \
		IniContext *pItemContext)
{
	g_use_connection_pool = iniGetBoolValue(NULL, "use_connection_pool", \
				pItemContext, false);
	if (!g_use_connection_pool)
	{
		return 0;
	}

	g_connection_pool_max_idle_time = iniGetIntValue(NULL, \
			"connection_pool_max_idle_time", \
			pItemContext, 3600);
	if (g_connection_pool_max_idle_time <= 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"connection_pool_max_idle_time: %d of conf " \
			"filename: \"%s\" is invalid!", __LINE__, \
			g_connection_pool_max_idle_time, config_filename);
		return EINVAL;
	}

	return conn_pool_init(&g_connection_pool, g_fdfs_connect_timeout, \
        		0, g_connection_pool_max_idle_time);
}

void fdfs_connection_pool_destroy()
{
	conn_pool_destroy(&g_connection_pool);
}

void fdfs_set_log_rotate_size(LogContext *pContext, const int64_t log_rotate_size)
{
	if (log_rotate_size > 0)
	{
		pContext->rotate_size = log_rotate_size;
		log_set_rotate_time_format(pContext, "%Y%m%d_%H%M%S");
	}
	else
	{
		pContext->rotate_size = 0;
		log_set_rotate_time_format(pContext, "%Y%m%d");
	}
}

int fdfs_parse_server_info_ex(char *server_str, const int default_port,
        TrackerServerInfo *pServer, const bool resolve)
{
	char *pColon;
    char *hosts[FDFS_MULTI_IP_MAX_COUNT];
    ConnectionInfo *conn;
    int port;
    int i;

    memset(pServer, 0, sizeof(TrackerServerInfo));
    if ((pColon=strrchr(server_str, ':')) == NULL)
    {
        logInfo("file: "__FILE__", line: %d, "
                "no port part in %s, set port to %d",
                __LINE__, server_str, default_port);
        port = default_port;
    }
    else
    {
        *pColon = '\0';
        port = atoi(pColon + 1);
    }

    conn = pServer->connections;
    pServer->count =  splitEx(server_str, ',',
            hosts, FDFS_MULTI_IP_MAX_COUNT);
    for (i=0; i<pServer->count; i++)
    {
        if (resolve)
        {
            if (getIpaddrByName(hosts[i], conn->ip_addr,
                        sizeof(conn->ip_addr)) == INADDR_NONE)
            {
                logError("file: "__FILE__", line: %d, "
                        "host \"%s\" is invalid, error info: %s",
                        __LINE__, hosts[i], hstrerror(h_errno));
                return EINVAL;
            }
        }
        else
        {
            snprintf(conn->ip_addr, sizeof(conn->ip_addr), "%s", hosts[i]);
        }
        conn->port = port;
        conn->sock = -1;
        conn++;
    }

    return 0;
}

int fdfs_server_info_to_string_ex(const TrackerServerInfo *pServer,
        const int port, char *buff, const int buffSize)
{
	const ConnectionInfo *conn;
	const ConnectionInfo *end;
    int len;

    if (pServer->count <= 0)
    {
        *buff = '\0';
        return 0;
    }
    if (pServer->count == 1)
    {
        return snprintf(buff, buffSize, "%s:%d",
                pServer->connections[0].ip_addr, port);
    }

    len = snprintf(buff, buffSize, "%s", pServer->connections[0].ip_addr);
	end = pServer->connections + pServer->count;
	for (conn=pServer->connections + 1; conn<end; conn++)
    {
        len += snprintf(buff + len, buffSize - len, ",%s", conn->ip_addr);
    }
    len += snprintf(buff + len, buffSize - len, ":%d", port);
    return len;
}

int fdfs_get_ip_type(const char* ip)
{
    if (ip == NULL || (int)strlen(ip) < 8)
    {
        return FDFS_IP_TYPE_UNKNOWN;
    }

    if (memcmp(ip, "10.", 3) == 0)
    {
        return FDFS_IP_TYPE_PRIVATE_10;
    }
    if (memcmp(ip, "192.168.", 8) == 0)
    {
        return FDFS_IP_TYPE_PRIVATE_192;
    }

    if (memcmp(ip, "172.", 4) == 0)
    {
        int b;
        b = atoi(ip + 4);
        if (b >= 16 && b < 32)
        {
            return FDFS_IP_TYPE_PRIVATE_172;
        }
    }

    return FDFS_IP_TYPE_OUTER;
}

int fdfs_check_server_ips(const TrackerServerInfo *pServer,
        char *error_info, const int error_size)
{
    int type0;
    int type1;
    if (pServer->count == 1)
    {
        *error_info = '\0';
        return 0;
    }

    if (pServer->count <= 0)
    {
        logError("file: "__FILE__", line: %d, "
                "empty server", __LINE__);
        return EINVAL;
    }

    if (pServer->count > FDFS_MULTI_IP_MAX_COUNT)
    {
        snprintf(error_info, error_size,
                "too many server ip addresses: %d, exceeds %d",
                pServer->count, FDFS_MULTI_IP_MAX_COUNT);
        return EINVAL;
    }

    type0 = fdfs_get_ip_type(pServer->connections[0].ip_addr);
    type1 = fdfs_get_ip_type(pServer->connections[1].ip_addr);
    if (type0 == type1)
    {
        snprintf(error_info, error_size,
                "invalid ip addresses %s and %s, "
                "one MUST be an inner IP and another is a outer IP, "
                "or two different types of inner IP addresses",
                pServer->connections[0].ip_addr,
                pServer->connections[1].ip_addr);
        return EINVAL;
    }

    *error_info = '\0';
    return 0;
}

int fdfs_parse_multi_ips_ex(char *ip_str, FDFSMultiIP *ip_addrs,
        char *error_info, const int error_size, const bool resolve)
{
    char *hosts[FDFS_MULTI_IP_MAX_COUNT];
    int i;

    ip_addrs->index = 0;
    ip_addrs->count = splitEx(ip_str, ',', hosts, FDFS_MULTI_IP_MAX_COUNT);
    for (i=0; i<ip_addrs->count; i++)
    {
        if (resolve)
        {
            if (getIpaddrByName(hosts[i], ip_addrs->ips[i].address,
                        sizeof(ip_addrs->ips[i].address)) == INADDR_NONE)
            {
                snprintf(error_info, error_size,
                        "host \"%s\" is invalid, error info: %s",
                        hosts[i], hstrerror(h_errno));
                return EINVAL;
            }
        }
        else
        {
            snprintf(ip_addrs->ips[i].address,
                    sizeof(ip_addrs->ips[i].address), "%s", hosts[i]);
        }

        ip_addrs->ips[i].type = fdfs_get_ip_type(ip_addrs->ips[i].address);
        if (ip_addrs->ips[i].type == FDFS_IP_TYPE_UNKNOWN)
        {
            snprintf(error_info, error_size,
                    "ip address \"%s\" is invalid",
                    ip_addrs->ips[i].address);
            return EINVAL;
        }
    }

    *error_info = '\0';
    return 0;
}

int fdfs_multi_ips_to_string_ex(const FDFSMultiIP *ip_addrs,
        const char seperator, char *buff, const int buffSize)
{
    int i;
    int len;

    if (ip_addrs->count <= 0)
    {
        *buff = '\0';
        return 0;
    }
    if (ip_addrs->count == 1)
    {
        return snprintf(buff, buffSize, "%s",
                ip_addrs->ips[0].address);
    }

    len = snprintf(buff, buffSize, "%s", ip_addrs->ips[0].address);
	for (i=1; i<ip_addrs->count; i++)
    {
        len += snprintf(buff + len, buffSize - len, "%c%s",
                seperator, ip_addrs->ips[i].address);
    }
    return len;
}

const char *fdfs_get_ipaddr_by_peer_ip(const FDFSMultiIP *ip_addrs,
        const char *client_ip)
{
    int ip_type;
    int index;
    if (ip_addrs->count == 1)
    {
        return ip_addrs->ips[0].address;
    }

    if (ip_addrs->count <= 0)
    {
        return "";
    }

    ip_type = fdfs_get_ip_type(client_ip);
    index = ip_addrs->ips[FDFS_MULTI_IP_INDEX_OUTER].type == ip_type ?
        FDFS_MULTI_IP_INDEX_OUTER : FDFS_MULTI_IP_INDEX_INNER;
    return ip_addrs->ips[index].address;
}

int fdfs_check_and_format_ips(FDFSMultiIP *ip_addrs,
        char *error_info, const int error_size)
{
    FDFSIPInfo swap_ip;
    if (ip_addrs->count == 1)
    {
        *error_info = '\0';
        return 0;
    }

    if (ip_addrs->count <= 0)
    {
        logError("file: "__FILE__", line: %d, "
                "empty server", __LINE__);
        return EINVAL;
    }

    if (ip_addrs->count > FDFS_MULTI_IP_MAX_COUNT)
    {
        snprintf(error_info, error_size,
                "too many server ip addresses: %d, exceeds %d",
                ip_addrs->count, FDFS_MULTI_IP_MAX_COUNT);
        return EINVAL;
    }

    if (ip_addrs->ips[FDFS_MULTI_IP_INDEX_INNER].type ==
            ip_addrs->ips[FDFS_MULTI_IP_INDEX_OUTER].type)
    {
        snprintf(error_info, error_size,
                "invalid ip addresses %s and %s, "
                "one MUST be an inner IP and another is a outer IP, "
                "or two different types of inner IP addresses",
                ip_addrs->ips[0].address, ip_addrs->ips[1].address);
        return EINVAL;
    }

    if (ip_addrs->ips[FDFS_MULTI_IP_INDEX_INNER].type == FDFS_IP_TYPE_OUTER)
    {
        swap_ip = ip_addrs->ips[FDFS_MULTI_IP_INDEX_INNER];
        ip_addrs->ips[FDFS_MULTI_IP_INDEX_INNER] =
            ip_addrs->ips[FDFS_MULTI_IP_INDEX_OUTER];
        ip_addrs->ips[FDFS_MULTI_IP_INDEX_OUTER] = swap_ip;
    }

    *error_info = '\0';
    return 0;
}

void fdfs_set_multi_ip_index(FDFSMultiIP *multi_ip, const char *target_ip)
{
    int i;
    if (multi_ip->count <= 1)
    {
        return;
    }

    for (i=0; i<multi_ip->count; i++)
    {
        if (strcmp(multi_ip->ips[i].address, target_ip) == 0)
        {
            multi_ip->index = i;
            break;
        }
    }
}

void fdfs_set_server_info_index(TrackerServerInfo *pServer,
        const char *target_ip, const int target_port)
{
    int i;
    if (pServer->count <= 1)
    {
        return;
    }

    for (i=0; i<pServer->count; i++)
    {
        if (FC_CONNECTION_SERVER_EQUAL(pServer->connections[i],
                    target_ip, target_port))
        {
            pServer->index = i;
            break;
        }
    }
}

void fdfs_set_server_info(TrackerServerInfo *pServer,
        const char *ip_addr, const int port)
{
    pServer->count = 1;
    pServer->index = 0;
    conn_pool_set_server_info(pServer->connections + 0, ip_addr, port);
}

void fdfs_set_server_info_ex(TrackerServerInfo *pServer,
        const FDFSMultiIP *ip_addrs, const int port)
{
    int i;

    pServer->count = ip_addrs->count;
    pServer->index = 0;
    for (i=0; i<ip_addrs->count; i++)
    {
        conn_pool_set_server_info(pServer->connections + i,
                ip_addrs->ips[i].address, port);
    }
}

