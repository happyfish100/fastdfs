/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fdfs_client.h"
#include "logger.h"

int main(int argc, char *argv[])
{
	char *conf_filename;
	char *local_filename;
	ConnectionInfo *pTrackerServer;
	int result;
	char file_id[128];
	int64_t file_size;
	int64_t file_offset;
	int64_t download_bytes;
	
	if (argc < 3)
	{
		printf("Usage: %s <config_file> <file_id> " \
			"[local_filename] [<download_offset> " \
			"<download_bytes>]\n", argv[0]);
		return 1;
	}

	log_init();
	g_log_context.log_level = LOG_ERR;
	ignore_signal_pipe();

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

	snprintf(file_id, sizeof(file_id), "%s", argv[2]);

	file_offset = 0;
	download_bytes = 0;
	if (argc >= 4)
	{
		local_filename = argv[3];
		if (argc >= 6)
		{
			file_offset = strtoll(argv[4], NULL, 10);
			download_bytes = strtoll(argv[5], NULL, 10);
		}
	}
	else
	{
		local_filename = strrchr(file_id, '/');
		if (local_filename != NULL)
		{
			local_filename++;  //skip /
		}
		else
		{
			local_filename = file_id;
		}
	}

	result = storage_do_download_file1_ex(pTrackerServer, \
                NULL, FDFS_DOWNLOAD_TO_FILE, file_id, \
                file_offset, download_bytes, \
                &local_filename, NULL, &file_size);
	if (result != 0)
	{
		printf("download file fail, " \
			"error no: %d, error info: %s\n", \
			result, STRERROR(result));
	}

	tracker_disconnect_server_ex(pTrackerServer, true);
	fdfs_client_destroy();

	return 0;
}

