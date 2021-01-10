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
#include "tracker_proto.h"
#include "fdfs_global.h"
#include "fdfs_shared_func.h"
#include "fdfs_server_id_func.h"

FDFSStorageIdInfoArray g_storage_ids_by_id = {0, NULL};   //sorted by storage ID
FDFSStorageIdMapArray g_storage_ids_by_ip = {0, NULL};  //sorted by group name and storage IP
static FDFSStorageIdMapArray g_storage_ids_by_ip_port = {0, NULL};  //sorted by storage ip and port

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
	result = strcmp(((FDFSStorageIdMap *)p1)->group_name,
		((FDFSStorageIdMap *)p2)->group_name);
	if (result != 0)
	{
		return result;
	}

	return strcmp(((FDFSStorageIdMap *)p1)->ip_addr,
		((FDFSStorageIdMap *)p2)->ip_addr);
}

static int fdfs_cmp_server_id(const void *p1, const void *p2)
{
	return strcmp(((FDFSStorageIdInfo *)p1)->id,
		((FDFSStorageIdInfo *)p2)->id);
}

static int fdfs_cmp_ip_and_port(const void *p1, const void *p2)
{
	int result;
    result = strcmp(((FDFSStorageIdMap *)p1)->ip_addr,
            ((FDFSStorageIdMap *)p2)->ip_addr);
    if (result != 0)
    {
        return result;
    }

    return ((FDFSStorageIdMap *)p1)->port -
        ((FDFSStorageIdMap *)p2)->port;
}

FDFSStorageIdInfo *fdfs_get_storage_id_by_ip(const char *group_name,
		const char *pIpAddr)
{
	FDFSStorageIdMap target;
	FDFSStorageIdMap *pFound;

	target.group_name =  group_name;
	target.ip_addr = pIpAddr;
	target.port = 0;
    target.idInfo = NULL;
	pFound = (FDFSStorageIdMap *)bsearch(&target,
            g_storage_ids_by_ip.maps, g_storage_ids_by_ip.count,
            sizeof(FDFSStorageIdMap), fdfs_cmp_group_name_and_ip);
	if (pFound == NULL)
	{
		return NULL;
	}
	else
	{
		return pFound->idInfo;
	}
}

FDFSStorageIdInfo *fdfs_get_storage_by_id(const char *id)
{
	FDFSStorageIdInfo target;

	memset(&target, 0, sizeof(FDFSStorageIdInfo));
	snprintf(target.id, sizeof(target.id), "%s", id);
	return (FDFSStorageIdInfo *)bsearch(&target,
            g_storage_ids_by_id.ids, g_storage_ids_by_id.count,
            sizeof(FDFSStorageIdInfo), fdfs_cmp_server_id);
}

static int fdfs_calc_ip_count()
{
	FDFSStorageIdInfo *idInfo;
	FDFSStorageIdInfo *idEnd;
    int ip_count;

    ip_count = 0;
    idEnd = g_storage_ids_by_id.ids + g_storage_ids_by_id.count;
    for (idInfo=g_storage_ids_by_id.ids; idInfo<idEnd; idInfo++)
    {
        ip_count += idInfo->ip_addrs.count;
    }

    return ip_count;
}

static int fdfs_init_ip_array(FDFSStorageIdMapArray *mapArray,
        int (*compare_func)(const void *, const void *))
{
	int result;
    int i;
	int alloc_bytes;
    FDFSStorageIdMap *idMap;
	FDFSStorageIdInfo *idInfo;
	FDFSStorageIdInfo *idEnd;

    mapArray->count = fdfs_calc_ip_count();
    alloc_bytes = sizeof(FDFSStorageIdMap) * mapArray->count;
    mapArray->maps = (FDFSStorageIdMap *)malloc(alloc_bytes);
    if (mapArray->maps == NULL)
    {
        result = errno != 0 ? errno : ENOMEM;
        logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail, "
                "errno: %d, error info: %s", __LINE__,
                alloc_bytes, result, STRERROR(result));
        return result;
    }
    memset(mapArray->maps, 0, alloc_bytes);

    idEnd = g_storage_ids_by_id.ids + g_storage_ids_by_id.count;
    idMap = mapArray->maps;
    for (idInfo=g_storage_ids_by_id.ids; idInfo<idEnd; idInfo++)
    {
        for (i=0; i<idInfo->ip_addrs.count; i++)
        {
            idMap->idInfo = idInfo;
            idMap->group_name = idInfo->group_name;
            idMap->ip_addr = idInfo->ip_addrs.ips[i].address;
            idMap->port = idInfo->port;
            idMap++;
        }
    }

    qsort(mapArray->maps, mapArray->count,
            sizeof(FDFSStorageIdMap), compare_func);
    return 0;
}

static int fdfs_check_id_duplicated()
{
	FDFSStorageIdInfo *current;
	FDFSStorageIdInfo *idEnd;
    FDFSStorageIdInfo *previous;

    previous = g_storage_ids_by_id.ids + 0;
    idEnd = g_storage_ids_by_id.ids + g_storage_ids_by_id.count;
    for (current=g_storage_ids_by_id.ids + 1; current<idEnd; current++)
    {
        if (strcmp(current->id, previous->id) == 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "config file: storage_ids.conf, "
                    "duplicate storage id: %s",
                    __LINE__, current->id);
            return EEXIST;
        }
        previous = current;
    }

    return 0;
}

static int fdfs_check_ip_port()
{
    int i;
    int port_count;
    FDFSStorageIdMap *previous;
    FDFSStorageIdMap *current;
    FDFSStorageIdMap *end;

    port_count = 0;
    for (i=0; i<g_storage_ids_by_id.count; i++)
    {
        if (g_storage_ids_by_id.ids[i].port > 0)
        {
            port_count++;
        }
    }
    if (port_count > 0 && port_count != g_storage_ids_by_id.count)
    {
        logError("file: "__FILE__", line: %d, "
                "config file: storage_ids.conf, "
                "some storages without port, "
                "must be the same format as host:port", __LINE__);

        return EINVAL;
    }

    previous = g_storage_ids_by_ip_port.maps + 0;
    end = g_storage_ids_by_ip_port.maps + g_storage_ids_by_ip_port.count;
    for (current=g_storage_ids_by_ip_port.maps+1; current<end; current++)
    {
        if (fdfs_cmp_ip_and_port(current, previous) == 0)
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

            free(g_storage_ids_by_ip_port.maps);
            g_storage_ids_by_ip_port.maps = NULL;
            return EEXIST;
        }

        previous = current;
    }

    return 0;
}

FDFSStorageIdInfo *fdfs_get_storage_id_by_ip_port(const char *pIpAddr,
        const int port)
{
	FDFSStorageIdMap target;
	FDFSStorageIdMap *pFound;
    int ports[2];
    int i;

	target.ip_addr = pIpAddr;
	target.group_name = NULL;
    target.idInfo = NULL;
    ports[0] = port;
    ports[1] = 0;
    for (i=0; i<2; i++)
    {
        target.port = ports[i];
        pFound = (FDFSStorageIdMap *)bsearch(&target,
                g_storage_ids_by_ip_port.maps,
                g_storage_ids_by_ip_port.count,
                sizeof(FDFSStorageIdMap), fdfs_cmp_ip_and_port);
        if (pFound != NULL)
        {
            return pFound->idInfo;
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
	char *pPort;
	FDFSStorageIdInfo *pStorageIdInfo;
    char error_info[256];
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
		g_storage_ids_by_id.count = 0;
		for (i=0; i<line_count; i++)
		{
			trim(lines[i]);
			if (*lines[i] == '\0' || *lines[i] == '#')
			{
				continue;
			}
			g_storage_ids_by_id.count++;
		}

		if (g_storage_ids_by_id.count == 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"config file: %s, no storage id!", \
				__LINE__, pStorageIdsFilename);
			result = ENOENT;
			break;
		}

		alloc_bytes = sizeof(FDFSStorageIdInfo) * g_storage_ids_by_id.count;
		g_storage_ids_by_id.ids = (FDFSStorageIdInfo *)malloc(alloc_bytes);
		if (g_storage_ids_by_id.ids == NULL)
		{
			result = errno != 0 ? errno : ENOMEM;
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", __LINE__, \
				alloc_bytes, result, STRERROR(result));
			break;
		}
		memset(g_storage_ids_by_id.ids, 0, alloc_bytes);

		pStorageIdInfo = g_storage_ids_by_id.ids;
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

            if ((result=fdfs_parse_multi_ips(pHost, &pStorageIdInfo->ip_addrs,
                            error_info, sizeof(error_info))) != 0)
            {
				logError("file: "__FILE__", line: %d, "
					"config file: %s, line no: %d, %s", __LINE__,
                    pStorageIdsFilename, i + 1, error_info);
				break;
            }

            if ((result=fdfs_check_and_format_ips(&pStorageIdInfo->ip_addrs,
                            error_info, sizeof(error_info))) != 0)
            {
				logError("file: "__FILE__", line: %d, "
					"config file: %s, line no: %d, %s", __LINE__,
                    pStorageIdsFilename, i + 1, error_info);
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

			snprintf(pStorageIdInfo->id,
				sizeof(pStorageIdInfo->id), "%s", id);
			snprintf(pStorageIdInfo->group_name,
				sizeof(pStorageIdInfo->group_name),
				"%s", group_name);
			pStorageIdInfo++;
		}
	} while (0);

	freeSplit(lines);
	if (result != 0)
	{
		return result;
	}

    if (g_log_context.log_level >= LOG_DEBUG)
    {
        logDebug("file: "__FILE__", line: %d, "
                "g_storage_ids_by_id.count: %d",
                __LINE__, g_storage_ids_by_id.count);

        pStorageIdInfo = g_storage_ids_by_id.ids;
        for (i=0; i<g_storage_ids_by_id.count; i++)
        {
            char szPortPart[16];
            char ip_str[256];
            if (pStorageIdInfo->port > 0)
            {
                sprintf(szPortPart, ":%d", pStorageIdInfo->port);
            }
            else
            {
                *szPortPart = '\0';
            }

            fdfs_multi_ips_to_string(&pStorageIdInfo->ip_addrs,
                    ip_str, sizeof(ip_str));
            logDebug("%s  %s  %s%s", pStorageIdInfo->id,
                    pStorageIdInfo->group_name, ip_str, szPortPart);

            pStorageIdInfo++;
        }
    }

	qsort(g_storage_ids_by_id.ids, g_storage_ids_by_id.count,
		sizeof(FDFSStorageIdInfo), fdfs_cmp_server_id);
    if ((result=fdfs_check_id_duplicated()) != 0)
    {
        return result;
    }

    if ((result=fdfs_init_ip_array(&g_storage_ids_by_ip,
                    fdfs_cmp_group_name_and_ip)) != 0)
    {
        return result;
    }
    if ((result=fdfs_init_ip_array(&g_storage_ids_by_ip_port,
                    fdfs_cmp_ip_and_port)) != 0)
    {
        return result;
    }

	return fdfs_check_ip_port();
}

int fdfs_get_storage_ids_from_tracker_server(TrackerServerInfo *pTrackerServer)
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
		if ((result=tcpsenddata_nb(conn->sock, out_buff,
			sizeof(out_buff), g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, "
				"send data to tracker server %s:%d fail, "
				"errno: %d, error info: %s", __LINE__,
				conn->ip_addr, conn->port,
				result, STRERROR(result));
		}
		else
		{
			response = NULL;
			result = fdfs_recv_response(conn, &response, 0, &in_bytes);
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
			logError("file: "__FILE__", line: %d, "
				"tracker server %s:%d, recv data length: %d "
				"is invalid", __LINE__, conn->ip_addr,
                conn->port, (int)in_bytes);
			result = EINVAL;
			break;
		}

		total_count = buff2int(response);
		current_count = buff2int(response + sizeof(int));
		if (total_count <= start_index)
		{
			logError("file: "__FILE__", line: %d, "
				"tracker server %s:%d, total storage "
				"count: %d is invalid, which <= start "
				"index: %d", __LINE__, conn->ip_addr,
				conn->port, total_count, start_index);
			result = EINVAL;
			break;
		}

		if (current_count <= 0)
		{
			logError("file: "__FILE__", line: %d, "
				"tracker server %s:%d, current storage "
				"count: %d is invalid, which <= 0", __LINE__,
                conn->ip_addr, conn->port, current_count);
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
			logError("file: "__FILE__", line: %d, "
				"response data from tracker "
				"server %s:%d is too large",
				__LINE__, conn->ip_addr, conn->port);
			result = ENOSPC;
			break;
		}
	}

	tracker_close_connection_ex(conn, result != 0);

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
	TrackerServerInfo *pGServer;
	TrackerServerInfo *pTServer;
	TrackerServerInfo *pServerStart;
	TrackerServerInfo *pServerEnd;
	TrackerServerInfo trackerServer;
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
			memcpy(pTServer, pGServer, sizeof(TrackerServerInfo));
            fdfs_server_sock_reset(pTServer);
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

