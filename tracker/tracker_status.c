/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_func.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "fdfs_define.h"
#include "logger.h"
#include "fdfs_global.h"
#include "shared_func.h"
#include "sched_thread.h"
#include "ini_file_reader.h"
#include "tracker_types.h"
#include "tracker_global.h"
#include "tracker_status.h"

#define TRACKER_STATUS_FILENAME			".tracker_status"
#define TRACKER_STATUS_ITEM_UP_TIME		"up_time"
#define TRACKER_STATUS_ITEM_LAST_CHECK_TIME	"last_check_time"

int tracker_write_status_to_file(void *args)
{
	char full_filename[MAX_PATH_SIZE];
	char buff[256];
	int len;

	snprintf(full_filename, sizeof(full_filename), "%s/data/%s", \
		g_fdfs_base_path, TRACKER_STATUS_FILENAME);

	len = sprintf(buff, "%s=%d\n" \
		      "%s=%d\n",
		TRACKER_STATUS_ITEM_UP_TIME, (int)g_up_time,
		TRACKER_STATUS_ITEM_LAST_CHECK_TIME, (int)g_current_time
	);

	return writeToFile(full_filename, buff, len);
}

int tracker_load_status_from_file(TrackerStatus *pStatus)
{
	char full_filename[MAX_PATH_SIZE];
	IniContext iniContext;
	int result;

	snprintf(full_filename, sizeof(full_filename), "%s/data/%s", \
		g_fdfs_base_path, TRACKER_STATUS_FILENAME);
	if (!fileExists(full_filename))
	{
		return 0;
	}

	memset(&iniContext, 0, sizeof(IniContext));
	if ((result=iniLoadFromFile(full_filename, &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"load from status file \"%s\" fail, " \
			"error code: %d", \
			__LINE__, full_filename, result);
		return result;
	}

	pStatus->up_time = iniGetIntValue(NULL, TRACKER_STATUS_ITEM_UP_TIME, \
				&iniContext, 0);
	pStatus->last_check_time = iniGetIntValue(NULL, \
			TRACKER_STATUS_ITEM_LAST_CHECK_TIME, &iniContext, 0);

	iniFreeContext(&iniContext);

	return 0;
}

