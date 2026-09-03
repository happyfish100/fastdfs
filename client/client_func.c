/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
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

static int fdfs_get_params_from_tracker(bool *use_storage_id)
{
    IniContext iniContext;
	int result;
	bool continue_flag;

	continue_flag = false;
	if ((result=fdfs_get_ini_context_from_tracker(&g_tracker_group,
                    &iniContext, &continue_flag)) != 0)
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
		strcpy(SF_G_BASE_PATH_STR, "/tmp");
	}
	else
	{
        fc_safe_strcpy(SF_G_BASE_PATH_STR, pBasePath);
		chopPath(SF_G_BASE_PATH_STR);
		if (!fileExists(SF_G_BASE_PATH_STR))
		{
			logError("file: "__FILE__", line: %d, " \
				"\"%s\" can't be accessed, error info: %s", \
				__LINE__, SF_G_BASE_PATH_STR, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}
		if (!isDir(SF_G_BASE_PATH_STR))
		{
			logError("file: "__FILE__", line: %d, " \
				"\"%s\" is not a directory!", \
				__LINE__, SF_G_BASE_PATH_STR);
			return ENOTDIR;
		}
	}
    SF_G_BASE_PATH_LEN = strlen(SF_G_BASE_PATH_STR);

	SF_G_CONNECT_TIMEOUT = iniGetIntValue(NULL, "connect_timeout", \
				iniContext, DEFAULT_CONNECT_TIMEOUT);
	if (SF_G_CONNECT_TIMEOUT <= 0)
	{
		SF_G_CONNECT_TIMEOUT = DEFAULT_CONNECT_TIMEOUT;
	}

	SF_G_NETWORK_TIMEOUT = iniGetIntValue(NULL, "network_timeout", \
				iniContext, DEFAULT_NETWORK_TIMEOUT);
	if (SF_G_NETWORK_TIMEOUT <= 0)
	{
		SF_G_NETWORK_TIMEOUT = DEFAULT_NETWORK_TIMEOUT;
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

	if ((result=fdfs_connection_pool_init(conf_filename, iniContext)) != 0)
	{
		return result;
	}

	load_fdfs_parameters_from_tracker = iniGetBoolValue(NULL,
				"load_fdfs_parameters_from_tracker",
				iniContext, false);
	if (load_fdfs_parameters_from_tracker)
	{
		if ((result=fdfs_get_params_from_tracker(&use_storage_id)) != 0)
        {
            return result;
        }
	}
	else
    {
        use_storage_id = iniGetBoolValue(NULL, "use_storage_id",
                iniContext, false);
        if (use_storage_id)
        {
            if ((result=fdfs_load_storage_ids_from_file(
                            conf_filename, iniContext)) != 0)
            {
                return result;
            }
        }
    }

    if (use_storage_id)
    {
        FDFSStorageIdInfo *idInfo;
        FDFSStorageIdInfo *end;
        char *connect_first_by;

        end = g_storage_ids_by_id.ids + g_storage_ids_by_id.count;
        for (idInfo=g_storage_ids_by_id.ids; idInfo<end; idInfo++)
        {
            if (idInfo->ip_addrs.count > 1)
            {
                g_multi_storage_ips = true;
                break;
            }
        }

        if (g_multi_storage_ips)
        {
            connect_first_by = iniGetStrValue(NULL,
                    "connect_first_by", iniContext);
            if (connect_first_by != NULL && strncasecmp(connect_first_by,
                        "last", 4) == 0)
            {
                g_connect_first_by = fdfs_connect_first_by_last_connected;
            }
        }
    }

#ifdef DEBUG_FLAG
	logDebug("base_path=%s, "
		"connect_timeout=%d, "
		"network_timeout=%d, "
		"tracker_server_count=%d, "
		"anti_steal_token=%d, "
		"anti_steal_secret_key length=%d, "
		"use_connection_pool=%d, "
		"g_connection_pool_max_idle_time=%ds, "
		"use_storage_id=%d, connect_first_by=%s, "
        "storage server id count: %d, "
        "multi storage ips: %d\n",
		SF_G_BASE_PATH_STR, SF_G_CONNECT_TIMEOUT,
		SF_G_NETWORK_TIMEOUT, pTrackerGroup->server_count,
		g_anti_steal_token, g_anti_steal_secret_key.length,
		g_use_connection_pool, g_connection_pool_max_idle_time,
		use_storage_id, g_connect_first_by == fdfs_connect_first_by_tracker ?
        "tracker" : "last-connected", g_storage_ids_by_id.count,
        g_multi_storage_ips);
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

void fdfs_client_destroy_ex(TrackerServerGroup *pTrackerGroup)
{
    fdfs_destroy_tracker_group(pTrackerGroup);
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

