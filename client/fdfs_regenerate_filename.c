/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fdfs_client.h"
#include "fastcommon/logger.h"

int main(int argc, char *argv[])
{
	char *conf_filename;
	ConnectionInfo *pTrackerServer;
	int result;
	char appender_file_id[128];
	char new_file_id[128];
	
	if (argc < 3)
	{
		fprintf(stderr, "regenerate filename for the appender file.\n"
                "NOTE: the regenerated file will be a normal file!\n"
                "Usage: %s <config_file> <appender_file_id>\n",
                argv[0]);
		return 1;
	}

	log_init();
	g_log_context.log_level = LOG_ERR;

	conf_filename = argv[1];
	if ((result=fdfs_client_init(conf_filename)) != 0)
	{
		return result;
	}

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		fdfs_client_destroy();
		return errno != 0 ? errno : ECONNREFUSED;
	}

	snprintf(appender_file_id, sizeof(appender_file_id), "%s", argv[2]);
	if ((result=storage_regenerate_appender_filename1(pTrackerServer,
		NULL, appender_file_id, new_file_id)) != 0)
	{
		fprintf(stderr, "regenerate file %s fail, "
			"error no: %d, error info: %s\n",
			appender_file_id, result, STRERROR(result));
		return result;
	}

    printf("%s\n", new_file_id);

	tracker_close_connection_ex(pTrackerServer, true);
	fdfs_client_destroy();

	return result;
}
