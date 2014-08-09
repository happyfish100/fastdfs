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
	char file_id[128];
	int result;
	FDFSFileInfo file_info;
	
	if (argc < 3)
	{
		printf("Usage: %s <config_file> <file_id>\n", argv[0]);
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

	snprintf(file_id, sizeof(file_id), "%s", argv[2]);
	memset(&file_info, 0, sizeof(file_info));
	result = fdfs_get_file_info_ex1(file_id, true, &file_info);
	if (result != 0)
	{
		printf("query file info fail, " \
			"error no: %d, error info: %s\n", \
			result, STRERROR(result));
	}
	else
	{
		char szDatetime[32];

		printf("source storage id: %d\n", file_info.source_id);
		printf("source ip address: %s\n", file_info.source_ip_addr);
		printf("file create timestamp: %s\n", formatDatetime(
			file_info.create_timestamp, "%Y-%m-%d %H:%M:%S", \
			szDatetime, sizeof(szDatetime)));
		printf("file size: %"PRId64"\n", \
			file_info.file_size);
		printf("file crc32: %u (0x%08X)\n", \
			file_info.crc32, file_info.crc32);
	}

	tracker_close_all_connections();
	fdfs_client_destroy();

	return 0;
}

