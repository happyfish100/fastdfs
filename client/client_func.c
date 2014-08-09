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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "fdfs_define.h"
#include "logger.h"
#include "fdfs_global.h"
#include "base64.h"
#include "sockopt.h"
#include "shared_func.h"
#include "ini_file_reader.h"
#include "connection_pool.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "client_global.h"
#include "client_func.h"

static int storage_cmp_by_ip_and_port(const void *p1, const void *p2)
{
	int res;

	res = strcmp(((ConnectionInfo *)p1)->ip_addr, \
			((ConnectionInfo *)p2)->ip_addr);
	if (res != 0)
	{
		return res;
	}

	return ((ConnectionInfo *)p1)->port - \
			((ConnectionInfo *)p2)->port;
}

static void insert_into_sorted_servers(TrackerServerGroup *pTrackerGroup, \
		ConnectionInfo *pInsertedServer)
{
	ConnectionInfo *pDestServer;
	for (pDestServer=pTrackerGroup->servers+pTrackerGroup->server_count; \
		pDestServer>pTrackerGroup->servers; pDestServer--)
	{
		if (storage_cmp_by_ip_and_port(pInsertedServer, \
			pDestServer-1) > 0)
		{
			memcpy(pDestServer, pInsertedServer, \
				sizeof(ConnectionInfo));
			return;
		}

		memcpy(pDestServer, pDestServer-1, sizeof(ConnectionInfo));
	}

	memcpy(pDestServer, pInsertedServer, sizeof(ConnectionInfo));
}

static int copy_tracker_servers(TrackerServerGroup *pTrackerGroup, \
		const char *filename, char **ppTrackerServers)
{
	char **ppSrc;
	char **ppEnd;
	ConnectionInfo destServer;
	char *pSeperator;
	char szHost[128];
	int nHostLen;

	memset(&destServer, 0, sizeof(ConnectionInfo));
	destServer.sock = -1;

	ppEnd = ppTrackerServers + pTrackerGroup->server_count;

	pTrackerGroup->server_count = 0;
	for (ppSrc=ppTrackerServers; ppSrc<ppEnd; ppSrc++)
	{
		if ((pSeperator=strchr(*ppSrc, ':')) == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file \"%s\", " \
				"tracker_server \"%s\" is invalid, " \
				"correct format is host:port", \
				__LINE__, filename, *ppSrc);
			return EINVAL;
		}

		nHostLen = pSeperator - (*ppSrc);
		if (nHostLen >= sizeof(szHost))
		{
			nHostLen = sizeof(szHost) - 1;
		}
		memcpy(szHost, *ppSrc, nHostLen);
		szHost[nHostLen] = '\0';

		if (getIpaddrByName(szHost, destServer.ip_addr, \
			sizeof(destServer.ip_addr)) == INADDR_NONE)
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file \"%s\", " \
				"host \"%s\" is invalid", \
				__LINE__, filename, szHost);
			return EINVAL;
		}
		destServer.port = atoi(pSeperator+1);
		if (destServer.port <= 0)
		{
			destServer.port = FDFS_TRACKER_SERVER_DEF_PORT;
		}

		if (bsearch(&destServer, pTrackerGroup->servers, \
			pTrackerGroup->server_count, \
			sizeof(ConnectionInfo), \
			storage_cmp_by_ip_and_port) == NULL)
		{
			insert_into_sorted_servers(pTrackerGroup, &destServer);
			pTrackerGroup->server_count++;
		}
	}

	/*
	{
	ConnectionInfo *pServer;
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

int fdfs_load_tracker_group_ex(TrackerServerGroup *pTrackerGroup, \
		const char *conf_filename, IniContext *pIniContext)
{
	int result;
	char *ppTrackerServers[FDFS_MAX_TRACKERS];

	if ((pTrackerGroup->server_count=iniGetValues(NULL, "tracker_server", \
		pIniContext, ppTrackerServers, FDFS_MAX_TRACKERS)) <= 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"conf file \"%s\", " \
			"get item \"tracker_server\" fail", \
			__LINE__, conf_filename);
		return ENOENT;
	}

	pTrackerGroup->servers = (ConnectionInfo *)malloc( \
		sizeof(ConnectionInfo) * pTrackerGroup->server_count);
	if (pTrackerGroup->servers == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail", __LINE__, \
			(int)sizeof(ConnectionInfo) * \
			pTrackerGroup->server_count);
		pTrackerGroup->server_count = 0;
		return errno != 0 ? errno : ENOMEM;
	}

	memset(pTrackerGroup->servers, 0, \
		sizeof(ConnectionInfo) * pTrackerGroup->server_count);
	if ((result=copy_tracker_servers(pTrackerGroup, conf_filename, \
			ppTrackerServers)) != 0)
	{
		pTrackerGroup->server_count = 0;
		free(pTrackerGroup->servers);
		pTrackerGroup->servers = NULL;
		return result;
	}

	return 0;
}

int fdfs_load_tracker_group(TrackerServerGroup *pTrackerGroup, \
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

	result = fdfs_load_tracker_group_ex(pTrackerGroup, conf_filename, \
			&iniContext);
	iniFreeContext(&iniContext);

	return result;
}

static int fdfs_get_params_from_tracker(bool *use_storage_id)
{
        IniContext iniContext;
	int result;
	bool continue_flag;

	continue_flag = false;
	if ((result=fdfs_get_ini_context_from_tracker(&g_tracker_group, \
		&iniContext, &continue_flag, false, NULL)) != 0)
        {
                return result;
        }

	*use_storage_id = iniGetBoolValue(NULL, "use_storage_id", \
				&iniContext, false);
        iniFreeContext(&iniContext);

	if (*use_storage_id)
	{
		result = fdfs_get_storage_ids_from_tracker_group( \
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
		use_storage_id, g_storage_id_count);
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
	ConnectionInfo *pDestServer;
	ConnectionInfo *pDestServerEnd;

	bytes = sizeof(ConnectionInfo) * pSrcTrackerGroup->server_count;
	pDestTrackerGroup->servers = (ConnectionInfo *)malloc(bytes);
	if (pDestTrackerGroup->servers == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail", __LINE__, bytes);
		return errno != 0 ? errno : ENOMEM;
	}

	pDestTrackerGroup->server_index = 0;
	pDestTrackerGroup->server_count = pSrcTrackerGroup->server_count;
	memcpy(pDestTrackerGroup->servers, pSrcTrackerGroup->servers, bytes);

	pDestServerEnd = pDestTrackerGroup->servers + \
			pDestTrackerGroup->server_count;
	for (pDestServer=pDestTrackerGroup->servers; \
		pDestServer<pDestServerEnd; pDestServer++)
	{
		pDestServer->sock = -1;
	}

	return 0;
}

bool fdfs_tracker_group_equals(TrackerServerGroup *pGroup1, \
        TrackerServerGroup *pGroup2)
{
    ConnectionInfo *pServer1;
    ConnectionInfo *pServer2;
    ConnectionInfo *pEnd1;

    if (pGroup1->server_count != pGroup2->server_count)
    {
        return false;
    }

    pEnd1 = pGroup1->servers + pGroup1->server_count;
    pServer1 = pGroup1->servers;
    pServer2 = pGroup2->servers;
    while (pServer1 < pEnd1)
    {
        if (!(strcmp(pServer1->ip_addr, pServer2->ip_addr) == 0 && 
                    pServer1->port == pServer2->port))
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

