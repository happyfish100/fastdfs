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

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [options] <config_file> <file_id>\n"
            "  options: \n"
            "    -s: keep silence, when this file not exist, "
            "do not log error on storage server\n"
            "    -n: do NOT calculate CRC32 for appender file "
            "or slave file\n\n", program);
}

int main(int argc, char *argv[])
{
	char *conf_filename;
    const char *file_type_str;
	char file_id[128];
	int result;
    int ch;
    char flags;
	FDFSFileInfo file_info;
	
	if (argc < 3)
    {
        usage(argv[0]);
        return 1;
    }

    flags = 0;
    while ((ch=getopt(argc, argv, "ns")) != -1) {
        switch (ch) {
            case 'n':
                flags |= FDFS_QUERY_FINFO_FLAGS_NOT_CALC_CRC32;
                break;
            case 's':
                flags |= FDFS_QUERY_FINFO_FLAGS_KEEP_SILENCE;
                break;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    if (optind + 2 > argc) {
        usage(argv[0]);
        return 1;
    }

	log_init();
	g_log_context.log_level = LOG_ERR;
	ignore_signal_pipe();

	conf_filename = argv[optind];
	if ((result=fdfs_client_init(conf_filename)) != 0)
	{
		return result;
	}

	fc_safe_strcpy(file_id, argv[optind+1]);
	memset(&file_info, 0, sizeof(file_info));
	result = fdfs_get_file_info_ex1(file_id, true, &file_info, flags);
	if (result != 0)
	{
		fprintf(stderr, "query file info fail, " \
			"error no: %d, error info: %s\n", \
			result, STRERROR(result));
	}
	else
	{
		char szDatetime[32];

        switch (file_info.file_type)
        {
            case FDFS_FILE_TYPE_NORMAL:
                file_type_str = "normal";
                break;
            case FDFS_FILE_TYPE_SLAVE:
                file_type_str = "slave";
                break;
            case FDFS_FILE_TYPE_APPENDER:
                file_type_str = "appender";
                break;
            default:
                file_type_str = "unknown";
                break;
        }

		printf("GET FROM SERVER: %s\n\n",
                file_info.get_from_server ? "true" : "false");
		printf("file type: %s\n", file_type_str);
		printf("source storage id: %d\n", file_info.source_id);
		printf("source ip address: %s\n", file_info.source_ip_addr);
		printf("file create timestamp: %s\n", formatDatetime(
			file_info.create_timestamp, "%Y-%m-%d %H:%M:%S",
			szDatetime, sizeof(szDatetime)));
		printf("file size: %"PRId64"\n", file_info.file_size);

        if ((flags & FDFS_QUERY_FINFO_FLAGS_NOT_CALC_CRC32) == 0 ||
                file_info.crc32 != 0)
        {
            printf("file crc32: %d (0x%08x)\n",
                    file_info.crc32, file_info.crc32);
        }
        printf("\n");
	}

	tracker_close_all_connections();
	fdfs_client_destroy();

	return 0;
}

