/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//client_func.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "fastcommon/base64.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/connection_pool.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "client_global.h"
#include "client_func.h"

static int storage_cmp_by_ip_and_port(const void *p1, const void *p2)
{
	int res;

	res = strcmp(((ConnectionInfo *)p1)->ip_addr,
			((ConnectionInfo *)p2)->ip_addr);
	if (res != 0)
	{
		return res;
	}

	return ((ConnectionInfo *)p1)->port -
			((ConnectionInfo *)p2)->port;
}

static int storage_cmp_server_info(const void *p1, const void *p2)
{
	TrackerServerInfo *server1;
	TrackerServerInfo *server2;
	ConnectionInfo *pc1;
	ConnectionInfo *pc2;
	ConnectionInfo *end1;
	int res;

    server1 = (TrackerServerInfo *)p1;
    server2 = (TrackerServerInfo *)p2;

    res = server1->count - server2->count;
    if (res != 0)
    {
        return res;
    }

    if (server1->count == 1)
    {
        return storage_cmp_by_ip_and_port(server1->connections + 0,
                server2->connections + 0);
    }

    end1 = server1->connections + server1->count;
    for (pc1=server1->connections,pc2=server2->connections; pc1<end1; pc1++,pc2++)
    {
        if ((res=storage_cmp_by_ip_and_port(pc1, pc2)) != 0)
        {
            return res;
        }
    }

    return 0;
}

static void insert_into_sorted_servers(TrackerServerGroup *pTrackerGroup, \
		TrackerServerInfo *pInsertedServer)
{
	TrackerServerInfo *pDestServer;
	for (pDestServer=pTrackerGroup->servers+pTrackerGroup->server_count;
		pDestServer>pTrackerGroup->servers; pDestServer--)
	{
		if (storage_cmp_server_info(pInsertedServer, pDestServer-1) > 0)
		{
			memcpy(pDestServer, pInsertedServer,
				sizeof(TrackerServerInfo));
			return;
		}

		memcpy(pDestServer, pDestServer-1, sizeof(TrackerServerInfo));
	}

	memcpy(pDestServer, pInsertedServer, sizeof(TrackerServerInfo));
}

static int copy_tracker_servers(TrackerServerGroup *pTrackerGroup,
		const char *filename, char **ppTrackerServers)
{
	char **ppSrc;
	char **ppEnd;
	TrackerServerInfo destServer;
    int result;

	memset(&destServer, 0, sizeof(TrackerServerInfo));
    fdfs_server_sock_reset(&destServer);

	ppEnd = ppTrackerServers + pTrackerGroup->server_count;
	pTrackerGroup->server_count = 0;
	for (ppSrc=ppTrackerServers; ppSrc<ppEnd; ppSrc++)
	{
        if ((result=fdfs_parse_server_info(*ppSrc,
                        FDFS_TRACKER_SERVER_DEF_PORT, &destServer)) != 0)
        {
            return result;
        }

		if (bsearch(&destServer, pTrackerGroup->servers,
			pTrackerGroup->server_count,
			sizeof(TrackerServerInfo),
			storage_cmp_server_info) == NULL)
		{
			insert_into_sorted_servers(pTrackerGroup, &destServer);
			pTrackerGroup->server_count++;
		}
	}

	/*
	{
	TrackerServerInfo *pServer;
	for (pServer=pTrackerGroup->servers; pServer<pTrackerGroup->servers+ \
		pTrackerGroup->server_count;	pServer++)
	{
		//printf("server=%s:%d\n", \
			pServer->ip_addr, pServer->port);
	}
	}
	*/

	return 0;
}

static int fdfs_check_tracker_group(TrackerServerGroup *pTrackerGroup,
		const char *conf_filename)
{
    int result;
	TrackerServerInfo *pServer;
	TrackerServerInfo *pEnd;
    char error_info[256];

	pEnd = pTrackerGroup->servers + pTrackerGroup->server_count;
	for (pServer=pTrackerGroup->servers; pServer<pEnd; pServer++)
	{
        if ((result=fdfs_check_server_ips(pServer,
                        error_info, sizeof(error_info))) != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "conf file: %s, tracker_server is invalid, "
                    "error info: %s", __LINE__, conf_filename, error_info);
            return result;
        }
	}

    return 0;
}

int fdfs_load_tracker_group_ex(TrackerServerGroup *pTrackerGroup,
		const char *conf_filename, IniContext *pIniContext)
{
	int result;
    int bytes;
	char *ppTrackerServers[FDFS_MAX_TRACKERS];

	if ((pTrackerGroup->server_count=iniGetValues(NULL, "tracker_server",
		pIniContext, ppTrackerServers, FDFS_MAX_TRACKERS)) <= 0)
	{
		logError("file: "__FILE__", line: %d, "
			"conf file \"%s\", item \"tracker_server\" not exist",
			__LINE__, conf_filename);
		return ENOENT;
	}

    bytes = sizeof(TrackerServerInfo) * pTrackerGroup->server_count;
	pTrackerGroup->servers = (TrackerServerInfo *)malloc(bytes);
	if (pTrackerGroup->servers == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"malloc %d bytes fail", __LINE__, bytes);
		pTrackerGroup->server_count = 0;
		return errno != 0 ? errno : ENOMEM;
	}

	memset(pTrackerGroup->servers, 0, bytes);
	if ((result=copy_tracker_servers(pTrackerGroup, conf_filename,
			ppTrackerServers)) != 0)
	{
		pTrackerGroup->server_count = 0;
		free(pTrackerGroup->servers);
		pTrackerGroup->servers = NULL;
		return result;
	}

	return fdfs_check_tracker_group(pTrackerGroup, conf_filename);
}

int fdfs_load_tracker_group(TrackerServerGroup *pTrackerGroup,
		const char *conf_filename)
{
	IniContext iniContext;
	int result;

	if ((result=iniLoadFromFile(conf_filename, &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"load conf file \"%s\" fail, ret code: %d",
			__LINE__, conf_filename, result);
		return result;
	}

	result = fdfs_load_tracker_group_ex(pTrackerGroup,
            conf_filename, &iniContext);
	iniFreeContext(&iniContext);

	return result;
}

static int fdfs_get_params_from_tracker(bool *use_storage_id)
{
    IniContext iniContext;
	int result;
	bool continue_flag;

	continue_flag = false;
	if ((result=fdfs_get_ini_context_from_tracker(&g_tracker_group,
		&iniContext, &continue_flag, false, NULL)) != 0)
    {
        return result;
    }

	*use_storage_id = iniGetBoolValue(NULL, "use_storage_id",
            &iniContext, false);
    iniFreeContext(&iniContext);

	if (*use_storage_id)
	{
		result = fdfs_get_storage_ids_from_tracker_group(
				&g_tracker_group);
	}

    return result;
}

static int fdfs_client_do_init_ex(TrackerServerGroup *pTrackerGroup, \
		const char *conf_filename, IniContext *iniContext)
{
	char *pBasePath;
	int result;
	bool use_storage_id;
	bool load_fdfs_parameters_from_tracker;

	pBasePath = iniGetStrValue(NULL, "base_path", iniContext);
	if (pBasePath == NULL)
	{
		strcpy(g_fdfs_base_path, "/tmp");
	}
	else
	{
		snprintf(g_fdfs_base_path, sizeof(g_fdfs_base_path), 
			"%s", pBasePath);
		chopPath(g_fdfs_base_path);
		if (!fileExists(g_fdfs_base_path))
		{
			logError("file: "__FILE__", line: %d, " \
				"\"%s\" can't be accessed, error info: %s", \
				__LINE__, g_fdfs_base_path, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}
		if (!isDir(g_fdfs_base_path))
		{
			logError("file: "__FILE__", line: %d, " \
				"\"%s\" is not a directory!", \
				__LINE__, g_fdfs_base_path);
			return ENOTDIR;
		}
	}

	g_fdfs_connect_timeout = iniGetIntValue(NULL, "connect_timeout", \
				iniContext, DEFAULT_CONNECT_TIMEOUT);
	if (g_fdfs_connect_timeout <= 0)
	{
		g_fdfs_connect_timeout = DEFAULT_CONNECT_TIMEOUT;
	}

	g_fdfs_network_timeout = iniGetIntValue(NULL, "network_timeout", \
				iniContext, DEFAULT_NETWORK_TIMEOUT);
	if (g_fdfs_network_timeout <= 0)
	{
		g_fdfs_network_timeout = DEFAULT_NETWORK_TIMEOUT;
	}

	if ((result=fdfs_load_tracker_group_ex(pTrackerGroup, \
			conf_filename, iniContext)) != 0)
	{
		return result;
	}

	g_anti_steal_token = iniGetBoolValue(NULL, \
				"http.anti_steal.check_token", \
				iniContext, false);
	if (g_anti_steal_token)
	{
		char *anti_steal_secret_key;

		anti_steal_secret_key = iniGetStrValue(NULL, \
					"http.anti_steal.secret_key", \
					iniContext);
		if (anti_steal_secret_key == NULL || \
			*anti_steal_secret_key == '\0')
		{
			logError("file: "__FILE__", line: %d, " \
				"param \"http.anti_steal.secret_key\""\
				" not exist or is empty", __LINE__);
			return EINVAL;
		}

		buffer_strcpy(&g_anti_steal_secret_key, anti_steal_secret_key);
	}

	g_tracker_server_http_port = iniGetIntValue(NULL, \
				"http.tracker_server_port", \
				iniContext, 80);
	if (g_tracker_server_http_port <= 0)
	{
		g_tracker_server_http_port = 80;
	}

	if ((result=fdfs_connection_pool_init(conf_filename, iniContext)) != 0)
	{
		return result;
	}

	load_fdfs_parameters_from_tracker = iniGetBoolValue(NULL, \
				"load_fdfs_parameters_from_tracker", \
				iniContext, false);
	if (load_fdfs_parameters_from_tracker)
	{
		fdfs_get_params_from_tracker(&use_storage_id);
	}
	else
	{
		use_storage_id = iniGetBoolValue(NULL, "use_storage_id", \
				iniContext, false);
		if (use_storage_id)
		{
			result = fdfs_load_storage_ids_from_file( \
					conf_filename, iniContext);
		}
	}

#ifdef DEBUG_FLAG
	logDebug("base_path=%s, " \
		"connect_timeout=%d, "\
		"network_timeout=%d, "\
		"tracker_server_count=%d, " \
		"anti_steal_token=%d, " \
		"anti_steal_secret_key length=%d, " \
		"use_connection_pool=%d, " \
		"g_connection_pool_max_idle_time=%ds, " \
		"use_storage_id=%d, storage server id count: %d\n", \
		g_fdfs_base_path, g_fdfs_connect_timeout, \
		g_fdfs_network_timeout, pTrackerGroup->server_count, \
		g_anti_steal_token, g_anti_steal_secret_key.length, \
		g_use_connection_pool, g_connection_pool_max_idle_time, \
		use_storage_id, g_storage_ids_by_id.count);
#endif

	return 0;
}

int fdfs_client_init_from_buffer_ex(TrackerServerGroup *pTrackerGroup, \
		const char *buffer)
{
	IniContext iniContext;
	char *new_buff;
	int result;

	new_buff = strdup(buffer);
	if (new_buff == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"strdup %d bytes fail", __LINE__, (int)strlen(buffer));
		return ENOMEM;
	}

	result = iniLoadFromBuffer(new_buff, &iniContext);
	free(new_buff);
	if (result != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"load parameters from buffer fail, ret code: %d", \
			 __LINE__, result);
		return result;
	}

	result = fdfs_client_do_init_ex(pTrackerGroup, "buffer", &iniContext);
	iniFreeContext(&iniContext);
	return result;
}

int fdfs_client_init_ex(TrackerServerGroup *pTrackerGroup, \
		const char *conf_filename)
{
	IniContext iniContext;
	int result;

	if ((result=iniLoadFromFile(conf_filename, &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"load conf file \"%s\" fail, ret code: %d", \
			__LINE__, conf_filename, result);
		return result;
	}

	result = fdfs_client_do_init_ex(pTrackerGroup, conf_filename, \
				&iniContext);
	iniFreeContext(&iniContext);
	return result;
}

int fdfs_copy_tracker_group(TrackerServerGroup *pDestTrackerGroup, \
		TrackerServerGroup *pSrcTrackerGroup)
{
	int bytes;
	TrackerServerInfo *pDestServer;
	TrackerServerInfo *pDestServerEnd;

	bytes = sizeof(TrackerServerInfo) * pSrcTrackerGroup->server_count;
	pDestTrackerGroup->servers = (TrackerServerInfo *)malloc(bytes);
	if (pDestTrackerGroup->servers == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"malloc %d bytes fail", __LINE__, bytes);
		return errno != 0 ? errno : ENOMEM;
	}

	pDestTrackerGroup->server_index = 0;
	pDestTrackerGroup->leader_index = 0;
	pDestTrackerGroup->server_count = pSrcTrackerGroup->server_count;
	memcpy(pDestTrackerGroup->servers, pSrcTrackerGroup->servers, bytes);

	pDestServerEnd = pDestTrackerGroup->servers +
			pDestTrackerGroup->server_count;
	for (pDestServer=pDestTrackerGroup->servers;
		pDestServer<pDestServerEnd; pDestServer++)
	{
        fdfs_server_sock_reset(pDestServer);
	}

	return 0;
}

bool fdfs_tracker_group_equals(TrackerServerGroup *pGroup1,
        TrackerServerGroup *pGroup2)
{
    TrackerServerInfo *pServer1;
    TrackerServerInfo *pServer2;
    TrackerServerInfo *pEnd1;

    if (pGroup1->server_count != pGroup2->server_count)
    {
        return false;
    }

    pEnd1 = pGroup1->servers + pGroup1->server_count;
    pServer1 = pGroup1->servers;
    pServer2 = pGroup2->servers;
    while (pServer1 < pEnd1)
    {
        if (!fdfs_server_equal(pServer1, pServer2))
        {
            return false;
        }

        pServer1++;
        pServer2++;
    }

    return true;
}

void fdfs_client_destroy_ex(TrackerServerGroup *pTrackerGroup)
{
	if (pTrackerGroup->servers != NULL)
	{
		free(pTrackerGroup->servers);
		pTrackerGroup->servers = NULL;

		pTrackerGroup->server_count = 0;
		pTrackerGroup->server_index = 0;
	}
}

const char *fdfs_get_file_ext_name_ex(const char *filename, 
	const bool twoExtName)
{
	const char *fileExtName;
	const char *p;
	const char *pStart;
	int extNameLen;

	fileExtName = strrchr(filename, '.');
	if (fileExtName == NULL)
	{
		return NULL;
	}

	extNameLen = strlen(fileExtName + 1);
	if (extNameLen > FDFS_FILE_EXT_NAME_MAX_LEN)
	{
		return NULL;
	}

	if (strchr(fileExtName + 1, '/') != NULL) //invalid extension name
	{
		return NULL;
	}

	if (!twoExtName)
	{
		return fileExtName + 1;
	}

	pStart = fileExtName - (FDFS_FILE_EXT_NAME_MAX_LEN - extNameLen) - 1;
	if (pStart < filename)
	{
		pStart = filename;
	}

	p = fileExtName - 1;  //before .
	while ((p > pStart) && (*p != '.'))
	{
		p--;
	}

	if (p > pStart)  //found (extension name have a dot)
	{
		if (strchr(p + 1, '/') == NULL)  //valid extension name
		{
			return p + 1;   //skip .
		}
	}

	return fileExtName + 1;  //skip .
}

