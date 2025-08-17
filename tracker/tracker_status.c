/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
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
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/sched_thread.h"
#include "fastcommon/ini_file_reader.h"
#include "tracker_types.h"
#include "tracker_global.h"
#include "tracker_status.h"

#define TRACKER_STATUS_FILENAME_STR		".tracker_status"
#define TRACKER_STATUS_FILENAME_LEN     \
    (sizeof(TRACKER_STATUS_FILENAME_STR) - 1)

#define TRACKER_STATUS_ITEM_UP_TIME_STR  "up_time"
#define TRACKER_STATUS_ITEM_UP_TIME_LEN   \
    (sizeof(TRACKER_STATUS_ITEM_UP_TIME_STR) - 1)

#define TRACKER_STATUS_ITEM_LAST_CHECK_TIME_STR "last_check_time"
#define TRACKER_STATUS_ITEM_LAST_CHECK_TIME_LEN \
    (sizeof(TRACKER_STATUS_ITEM_LAST_CHECK_TIME_STR) - 1)

int tracker_write_status_to_file(void *args)
{
	char full_filename[MAX_PATH_SIZE];
	char buff[256];
    char *p;

    fc_get_one_subdir_full_filename(
            SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN,
            "data", 4, TRACKER_STATUS_FILENAME_STR,
            TRACKER_STATUS_FILENAME_LEN, full_filename);

    p = buff;
    memcpy(p, TRACKER_STATUS_ITEM_UP_TIME_STR,
            TRACKER_STATUS_ITEM_UP_TIME_LEN);
    p += TRACKER_STATUS_ITEM_UP_TIME_LEN;
    *p++ = '=';
    p += fc_itoa(g_sf_global_vars.up_time, p);
    *p++ = '\n';

    memcpy(p, TRACKER_STATUS_ITEM_LAST_CHECK_TIME_STR,
            TRACKER_STATUS_ITEM_LAST_CHECK_TIME_LEN);
    p += TRACKER_STATUS_ITEM_LAST_CHECK_TIME_LEN;
    *p++ = '=';
    p += fc_itoa(g_current_time, p);
    *p++ = '\n';

	return writeToFile(full_filename, buff, p - buff);
}

int tracker_load_status_from_file(TrackerStatus *pStatus)
{
    char full_filename[MAX_PATH_SIZE];
    IniContext iniContext;
    int result;

    fc_get_one_subdir_full_filename(
            SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN,
            "data", 4, TRACKER_STATUS_FILENAME_STR,
            TRACKER_STATUS_FILENAME_LEN, full_filename);
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

    pStatus->up_time = iniGetIntValue(NULL,
            TRACKER_STATUS_ITEM_UP_TIME_STR,
            &iniContext, 0);
    pStatus->last_check_time = iniGetIntValue(NULL,
            TRACKER_STATUS_ITEM_LAST_CHECK_TIME_STR,
            &iniContext, 0);

    iniFreeContext(&iniContext);

    return 0;
}
