/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "logger.h"
#include "sockopt.h"
#include "shared_func.h"
#include "tracker_proto.h"
#include "fdfs_global.h"
#include "fdfs_shared_func.h"

FDFSStorageIdInfo *g_storage_ids_by_ip = NULL;  //sorted by group name and storage IP
FDFSStorageIdInfo **g_storage_ids_by_id = NULL; //sorted by storage ID
static FDFSStorageIdInfo **g_storage_ids_by_ip_port = NULL;  //sorted by storage ip and port

int g_storage_id_count = 0;

int fdfs_get_tracker_leader_index_ex(TrackerServerGroup *pServerGroup, \
		const char *leaderIp, const int leaderPort)
{
	ConnectionInfo *pServer;
	ConnectionInfo *pEnd;

	if (pServerGroup->server_count == 0)
	{
		return -1;
	}

	pEnd = pServerGroup->servers + pServerGroup->server_count;
	for (pServer=pServerGroup->servers; pServer<pEnd; pServer++)
	{
		if (strcmp(pServer->ip_addr, leaderIp) == 0 && \
			pServer->port == leaderPort)
		{
			return pServer - pServerGroup->servers;
		}
	}

	return -1;
}

int fdfs_parse_storage_reserved_space(IniContext *pIniContext, \
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
		sprintf(buff, "%d MB", pStorageReservedSpace->rs.mb);
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

bool fdfs_is_server_id_valid(const char *id)
{
	long n;
	char *endptr;
	char buff[FDFS_STORAGE_ID_MAX_SIZE];

	if (*id == '\0')
	{
		return false;
	}

	endptr = NULL;
	n = strtol(id, &endptr, 10);
	if (endptr != NULL && *endptr != '\0')
	{
		return false;
	}

	if (n <= 0 || n > FDFS_MAX_SERVER_ID)
	{
		return false;
	}

	snprintf(buff, sizeof(buff), "%ld", n);
	return strcmp(buff, id) == 0;
}

int  fdfs_get_server_id_type(const int id)
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

static int fdfs_cmp_group_name_and_ip(const void *p1, const void *p2)
{
	int result;
	result = strcmp(((FDFSStorageIdInfo *)p1)->group_name,
		((FDFSStorageIdInfo *)p2)->group_name);
	if (result != 0)
	{
		return result;
	}

	return strcmp(((FDFSStorageIdInfo *)p1)->ip_addr, \
		((FDFSStorageIdInfo *)p2)->ip_addr);
}

static int fdfs_cmp_server_id(const void *p1, const void *p2)
{
	return strcmp((*((FDFSStorageIdInfo **)p1))->id, \
		(*((FDFSStorageIdInfo **)p2))->id);
}

static int fdfs_cmp_ip_and_port(const void *p1, const void *p2)
{
	int result;
    result = strcmp((*((FDFSStorageIdInfo **)p1))->ip_addr,
            (*((FDFSStorageIdInfo **)p2))->ip_addr);
    if (result != 0)
    {
        return result;
    }

    return (*((FDFSStorageIdInfo **)p1))->port -
        (*((FDFSStorageIdInfo **)p2))->port;
}

FDFSStorageIdInfo *fdfs_get_storage_id_by_ip(const char *group_name, \
		const char *pIpAddr)
{
	FDFSStorageIdInfo target;
	memset(&target, 0, sizeof(FDFSStorageIdInfo));
	snprintf(target.group_name, sizeof(target.group_name), "%s", group_name);
	snprintf(target.ip_addr, sizeof(target.ip_addr), "%s", pIpAddr);
	return (FDFSStorageIdInfo *)bsearch(&target, g_storage_ids_by_ip, \
		g_storage_id_count, sizeof(FDFSStorageIdInfo), \
		fdfs_cmp_group_name_and_ip);
}

FDFSStorageIdInfo *fdfs_get_storage_by_id(const char *id)
{
	FDFSStorageIdInfo target;
	FDFSStorageIdInfo *pTarget;
	FDFSStorageIdInfo **ppFound;

	memset(&target, 0, sizeof(FDFSStorageIdInfo));
	snprintf(target.id, sizeof(target.id), "%s", id);
	pTarget = &target;
	ppFound = (FDFSStorageIdInfo **)bsearch(&pTarget, g_storage_ids_by_id, \
		g_storage_id_count, sizeof(FDFSStorageIdInfo *), \
		fdfs_cmp_server_id);
	if (ppFound == NULL)
	{
		return NULL;
	}
	else
	{
		return *ppFound;
	}
}

static int fdfs_init_ip_port_array()
{
    int result;
    int alloc_bytes;
    int i;
    int port_count;
    FDFSStorageIdInfo *previous;

    alloc_bytes = sizeof(FDFSStorageIdInfo *) * g_storage_id_count;
    g_storage_ids_by_ip_port = (FDFSStorageIdInfo **)malloc(alloc_bytes);
    if (g_storage_ids_by_ip_port == NULL)
    {
        result = errno != 0 ? errno : ENOMEM;
        logError("file: "__FILE__", line: %d, " \
                "malloc %d bytes fail, " \
                "errno: %d, error info: %s", __LINE__, \
                alloc_bytes, result, STRERROR(result));
        return result;
    }

    port_count = 0;
    for (i=0; i<g_storage_id_count; i++)
    {
        g_storage_ids_by_ip_port[i] = g_storage_ids_by_ip + i;
        if (g_storage_ids_by_ip_port[i]->port > 0)
        {
            port_count++;
        }
    }
    if (port_count > 0 && port_count != g_storage_id_count)
    {
        logError("file: "__FILE__", line: %d, "
                "config file: storage_ids.conf, some storages without port, "
                "must be the same format as host:port", __LINE__);

        free(g_storage_ids_by_ip_port);
        g_storage_ids_by_ip_port = NULL;
        return EINVAL;
    }

	qsort(g_storage_ids_by_ip_port, g_storage_id_count,
		sizeof(FDFSStorageIdInfo *), fdfs_cmp_ip_and_port);

    previous = g_storage_ids_by_ip_port[0];
    for (i=1; i<g_storage_id_count; i++)
    {
        if (fdfs_cmp_ip_and_port(&g_storage_ids_by_ip_port[i],
                    &previous) == 0)
        {
            char szPortPart[16];
            if (previous->port > 0)
            {
                sprintf(szPortPart, ":%d", previous->port);
            }
            else
            {
                *szPortPart = '\0';
            }
            logError("file: "__FILE__", line: %d, "
                    "config file: storage_ids.conf, "
                    "duplicate storage: %s%s", __LINE__,
                    previous->ip_addr, szPortPart);

            free(g_storage_ids_by_ip_port);
            g_storage_ids_by_ip_port = NULL;
            return EEXIST;
        }

        previous = g_storage_ids_by_ip_port[i];
    }

    return 0;
}

FDFSStorageIdInfo *fdfs_get_storage_id_by_ip_port(const char *pIpAddr,
        const int port)
{
	FDFSStorageIdInfo target;
	FDFSStorageIdInfo *pTarget;
	FDFSStorageIdInfo **ppFound;
    int ports[2];
    int i;

    if (g_storage_ids_by_ip_port == NULL)
    {
        if (fdfs_init_ip_port_array() != 0)
        {
            return NULL;
        }
    }

	pTarget = &target;
	memset(&target, 0, sizeof(FDFSStorageIdInfo));
	snprintf(target.ip_addr, sizeof(target.ip_addr), "%s", pIpAddr);
    ports[0] = port;
    ports[1] = 0;
    for (i=0; i<2; i++)
    {
        target.port = ports[i];
        ppFound = (FDFSStorageIdInfo **)bsearch(&pTarget,
                g_storage_ids_by_ip_port, g_storage_id_count,
                sizeof(FDFSStorageIdInfo *), fdfs_cmp_ip_and_port);
        if (ppFound != NULL)
        {
            return *ppFound;
        }
    }

    return NULL;
}

int fdfs_check_storage_id(const char *group_name, const char *id)
{
	FDFSStorageIdInfo *pFound;

	pFound = fdfs_get_storage_by_id(id);
	if (pFound == NULL)
	{
		return ENOENT;
	}

	return strcmp(pFound->group_name, group_name) == 0 ? 0 : EINVAL;
}

int fdfs_load_storage_ids(char *content, const char *pStorageIdsFilename)
{
	char **lines;
	char *line;
	char *id;
	char *group_name;
	char *pHost;
	char *pIpAddr;
	char *pPort;
	FDFSStorageIdInfo *pStorageIdInfo;
	FDFSStorageIdInfo **ppStorageIdInfo;
	FDFSStorageIdInfo **ppStorageIdEnd;
	int alloc_bytes;
	int result;
	int line_count;
	int i;

	lines = split(content, '\n', 0, &line_count);
	if (lines == NULL)
	{
		return ENOMEM;
	}

	result = 0;
	do
	{
		g_storage_id_count = 0;
		for (i=0; i<line_count; i++)
		{
			trim(lines[i]);
			if (*lines[i] == '\0' || *lines[i] == '#')
			{
				continue;
			}
			g_storage_id_count++;
		}

		if (g_storage_id_count == 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"config file: %s, no storage id!", \
				__LINE__, pStorageIdsFilename);
			result = ENOENT;
			break;
		}

		alloc_bytes = sizeof(FDFSStorageIdInfo) * g_storage_id_count;
		g_storage_ids_by_ip = (FDFSStorageIdInfo *)malloc(alloc_bytes);
		if (g_storage_ids_by_ip == NULL)
		{
			result = errno != 0 ? errno : ENOMEM;
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", __LINE__, \
				alloc_bytes, result, STRERROR(result));
			break;
		}
		memset(g_storage_ids_by_ip, 0, alloc_bytes);

		alloc_bytes = sizeof(FDFSStorageIdInfo *) * g_storage_id_count;
		g_storage_ids_by_id = (FDFSStorageIdInfo **)malloc(alloc_bytes);
		if (g_storage_ids_by_id == NULL)
		{
			result = errno != 0 ? errno : ENOMEM;
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", __LINE__, \
				alloc_bytes, result, STRERROR(result));
			free(g_storage_ids_by_ip);
			break;
		}
		memset(g_storage_ids_by_id, 0, alloc_bytes);

		pStorageIdInfo = g_storage_ids_by_ip;
		for (i=0; i<line_count; i++)
		{
			line = lines[i];
			if (*line == '\0' || *line == '#')
			{
				continue;
			}

			id = line;
			group_name = line;
			while (!(*group_name == ' ' || *group_name == '\t' \
				|| *group_name == '\0'))
			{
				group_name++;
			}

			if (*group_name == '\0')
			{
				logError("file: "__FILE__", line: %d, " \
					"config file: %s, line no: %d, " \
					"content: %s, invalid format, " \
					"expect group name and ip address!", \
					__LINE__, pStorageIdsFilename, \
					i + 1, line);
				result = EINVAL;
				break;
			}

			*group_name = '\0';
			group_name++;  //skip space char
			while (*group_name == ' ' || *group_name == '\t')
			{
				group_name++;
			}
		
			pHost = group_name;
			while (!(*pHost == ' ' || *pHost == '\t' \
				|| *pHost == '\0'))
			{
				pHost++;
			}

			if (*pHost == '\0')
			{
				logError("file: "__FILE__", line: %d, " \
					"config file: %s, line no: %d, " \
					"content: %s, invalid format, " \
					"expect ip address!", __LINE__, \
					pStorageIdsFilename, i + 1, line);
				result = EINVAL;
				break;
			}

			*pHost = '\0';
			pHost++;  //skip space char
			while (*pHost == ' ' || *pHost == '\t')
			{
				pHost++;
			}

            pIpAddr = pHost;
            pPort = strchr(pHost, ':');
            if (pPort != NULL)
            {
                *pPort = '\0';
                pStorageIdInfo->port = atoi(pPort + 1);
            }
            else
            {
                pStorageIdInfo->port = 0;
            }
			if (getIpaddrByName(pIpAddr, pStorageIdInfo->ip_addr, \
				sizeof(pStorageIdInfo->ip_addr)) == INADDR_NONE)
			{
				logError("file: "__FILE__", line: %d, " \
					"invalid host name: %s", __LINE__, pIpAddr);
				result = EINVAL;
				break;
			}

			if (!fdfs_is_server_id_valid(id))
			{
				logError("file: "__FILE__", line: %d, " \
					"invalid server id: \"%s\", " \
					"which must be a none zero start " \
					"integer, such as 100001", __LINE__, id);
				result = EINVAL;
				break;
			}

			snprintf(pStorageIdInfo->id, \
				sizeof(pStorageIdInfo->id), "%s", id);
			snprintf(pStorageIdInfo->group_name, \
				sizeof(pStorageIdInfo->group_name), \
				"%s", group_name);
			pStorageIdInfo++;
		}
	} while (0);

	freeSplit(lines);
	if (result != 0)
	{
		return result;
	}

	logDebug("file: "__FILE__", line: %d, " \
		"g_storage_id_count: %d", __LINE__, g_storage_id_count);
	pStorageIdInfo = g_storage_ids_by_ip;
	for (i=0; i<g_storage_id_count; i++)
	{
        char szPortPart[16];
        if (pStorageIdInfo->port > 0)
        {
            sprintf(szPortPart, ":%d", pStorageIdInfo->port);
        }
        else
        {
            *szPortPart = '\0';
        }
		logDebug("%s  %s  %s%s", pStorageIdInfo->id,
			pStorageIdInfo->group_name,
			pStorageIdInfo->ip_addr, szPortPart);

		pStorageIdInfo++;
	}
	
	ppStorageIdEnd = g_storage_ids_by_id + g_storage_id_count;
	pStorageIdInfo = g_storage_ids_by_ip;
	for (ppStorageIdInfo=g_storage_ids_by_id; ppStorageIdInfo < \
		ppStorageIdEnd; ppStorageIdInfo++)
	{
		*ppStorageIdInfo = pStorageIdInfo++;
	}

	qsort(g_storage_ids_by_ip, g_storage_id_count, \
		sizeof(FDFSStorageIdInfo), fdfs_cmp_group_name_and_ip);
	qsort(g_storage_ids_by_id, g_storage_id_count, \
		sizeof(FDFSStorageIdInfo *), fdfs_cmp_server_id);

	return result;
}

int fdfs_get_storage_ids_from_tracker_server(ConnectionInfo *pTrackerServer)
{
#define MAX_REQUEST_LOOP   32
	TrackerHeader *pHeader;
	ConnectionInfo *conn;
	char out_buff[sizeof(TrackerHeader) + sizeof(int)];
	char *p;
	char *response;
	struct data_info {
		char *buffer;  //for free
		char *content;
		int length;
	} data_list[MAX_REQUEST_LOOP];
	int list_count;
	int total_count;
	int current_count;
	int result;
	int i;
	int start_index;
	int64_t in_bytes;

	if ((conn=tracker_connect_server(pTrackerServer, &result)) == NULL)
	{
		return result;
	}

	memset(data_list, 0, sizeof(data_list));
	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_FETCH_STORAGE_IDS;
	long2buff(sizeof(int), pHeader->pkg_len);

	start_index = 0;
	list_count = 0;
	result = 0;
	while (1)
	{
		int2buff(start_index, p);
		if ((result=tcpsenddata_nb(conn->sock, out_buff, \
			sizeof(out_buff), g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"send data to tracker server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pTrackerServer->ip_addr, \
				pTrackerServer->port, \
				result, STRERROR(result));
		}
		else
		{
			response = NULL;
			result = fdfs_recv_response(conn, \
				&response, 0, &in_bytes);
            if (result != 0)
            {
                logError("file: "__FILE__", line: %d, "
                        "fdfs_recv_response fail, result: %d",
                        __LINE__, result);
            }
		}

		if (result != 0)
		{
			break;
		}

		if (in_bytes < 2 * sizeof(int))
		{
			logError("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, recv data length: %d "\
				"is invalid", __LINE__, 
				pTrackerServer->ip_addr, \
				pTrackerServer->port, (int)in_bytes);
			result = EINVAL;
			break;
		}

		total_count = buff2int(response);
		current_count = buff2int(response + sizeof(int));
		if (total_count <= start_index)
		{
			logError("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, total storage " \
				"count: %d is invalid, which <= start " \
				"index: %d", __LINE__, pTrackerServer->ip_addr,\
				pTrackerServer->port, total_count, start_index);
			result = EINVAL;
			break;
		}

		if (current_count <= 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"tracker server %s:%d, current storage " \
				"count: %d is invalid, which <= 0", \
				__LINE__, pTrackerServer->ip_addr,\
				pTrackerServer->port, current_count);
			result = EINVAL;
			break;
		}

		data_list[list_count].buffer = response;
		data_list[list_count].content = response + 2 * sizeof(int);
		data_list[list_count].length = in_bytes - 2 * sizeof(int);
		list_count++;

		/*
		//logInfo("list_count: %d, total_count: %d, current_count: %d", 
			list_count, total_count, current_count);
		*/

		start_index += current_count;
		if (start_index >= total_count)
		{
			break;
		}

		if (list_count == MAX_REQUEST_LOOP)
		{
			logError("file: "__FILE__", line: %d, " \
				"response data from tracker " \
				"server %s:%d is too large", \
				__LINE__, pTrackerServer->ip_addr,\
				pTrackerServer->port);
			result = ENOSPC;
			break;
		}
	}

	tracker_disconnect_server_ex(conn, result != 0);

	if (result == 0)
	{
		do
		{
			int total_length;
			char *content;

			total_length = 0;
			for (i=0; i<list_count; i++)
			{
				total_length += data_list[i].length;
			}

			content = (char *)malloc(total_length + 1);
			if (content == NULL)
			{
				result = errno != 0 ? errno : ENOMEM;
				logError("file: "__FILE__", line: %d, " \
					"malloc %d bytes fail, " \
					"errno: %d, error info: %s", \
					__LINE__, total_length + 1, \
					result, STRERROR(result));
				break;
			}

			p = content;
			for (i=0; i<list_count; i++)
			{
				memcpy(p, data_list[i].content, data_list[i].length);
				p += data_list[i].length;
			}
			*p = '\0';

			//logInfo("list_count: %d, storage ids:\n%s", list_count, content);

			result = fdfs_load_storage_ids(content, \
					"storage-ids-from-tracker");
			free(content);
		} while (0);
	}

	for (i=0; i<list_count; i++)
	{
		free(data_list[i].buffer);
	}

	return result;
}

int fdfs_get_storage_ids_from_tracker_group(TrackerServerGroup *pTrackerGroup)
{
	ConnectionInfo *pGServer;
	ConnectionInfo *pTServer;
	ConnectionInfo *pServerStart;
	ConnectionInfo *pServerEnd;
	ConnectionInfo trackerServer;
	int result;
	int leader_index;
	int i;

	pTServer = &trackerServer;
	pServerEnd = pTrackerGroup->servers + pTrackerGroup->server_count;

	leader_index = pTrackerGroup->leader_index;
	if (leader_index >= 0)
	{
		pServerStart = pTrackerGroup->servers + leader_index;
	}
	else
	{
		pServerStart = pTrackerGroup->servers;
	}

	result = ENOENT;
	for (i=0; i<5; i++)
	{
		for (pGServer=pServerStart; pGServer<pServerEnd; pGServer++)
		{
			memcpy(pTServer, pGServer, sizeof(ConnectionInfo));
			pTServer->sock = -1;
			result = fdfs_get_storage_ids_from_tracker_server(pTServer);
			if (result == 0)
			{
				return result;
			}
		}

		if (pServerStart != pTrackerGroup->servers)
		{
			pServerStart = pTrackerGroup->servers;
		}
		sleep(1);
	}

	return result;
}

int fdfs_load_storage_ids_from_file(const char *config_filename, \
		IniContext *pItemContext)
{
	char *pStorageIdsFilename;
	char *content;
	int64_t file_size;
	int result;

	pStorageIdsFilename = iniGetStrValue(NULL, "storage_ids_filename", \
				pItemContext);
	if (pStorageIdsFilename == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"conf file \"%s\" must have item " \
			"\"storage_ids_filename\"!", __LINE__, config_filename);
		return ENOENT;
	}

	if (*pStorageIdsFilename == '\0')
	{
		logError("file: "__FILE__", line: %d, " \
			"conf file \"%s\", storage_ids_filename is emtpy!", \
			__LINE__, config_filename);
		return EINVAL;
	}

	if (*pStorageIdsFilename == '/')  //absolute path
	{
		result = getFileContent(pStorageIdsFilename, \
				&content, &file_size);
	}
	else
	{
		const char *lastSlash = strrchr(config_filename, '/');
		if (lastSlash == NULL)
		{
			result = getFileContent(pStorageIdsFilename, \
					&content, &file_size);
		}
		else
		{
			char filepath[MAX_PATH_SIZE];
			char full_filename[MAX_PATH_SIZE];
			int len;

			len = lastSlash - config_filename;
			if (len >= sizeof(filepath))
			{
				logError("file: "__FILE__", line: %d, " \
					"conf filename: \"%s\" is too long!", \
					__LINE__, config_filename);
				return ENOSPC;
			}
			memcpy(filepath, config_filename, len);
			*(filepath + len) = '\0';
			snprintf(full_filename, sizeof(full_filename), \
				"%s/%s", filepath, pStorageIdsFilename);
			result = getFileContent(full_filename, \
					&content, &file_size);
		}
	}
	if (result != 0)
	{
		return result;
	}

	result = fdfs_load_storage_ids(content, pStorageIdsFilename);
	free(content);
	return result;
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

