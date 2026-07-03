/**
* Copyright (C) 2026 Happy Fish / YuQing
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
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include "fastcommon/sockopt.h"
#include "fastcommon/logger.h"
#include "client_global.h"
#include "fdfs_global.h"
#include "fdfs_client.h"

#define FDFS_MAX_STORAGES  (2 * FDFS_MAX_GROUPS)

static ConnectionInfo *pTrackerServer;
static bool show_detail = false;
static bool use_storage_id;
static bool use_trunk_file;

static void usage(char *argv[])
{
    fprintf(stderr, "Usage: %s [-c config_filename=%s]\n"
            "\t[-A] list ACTIVE storages\n"
            "\t[-N] list None ACTIVE storages\n"
            "\t[-d] show detail / more info\n"
            "\nWhen use_trunk_file is true, including options:\n"
            "\t[-T] list trunk servers / storages\n"
            "\t[-t] list None trunk servers / storages\n\n"
            "For example:\n" "\t%s -A\n" "\t%s -N\n\n",
            argv[0], FDFS_CLIENT_DEFAULT_CONFIG_FILENAME,
            argv[0], argv[0]);
}

static void output(const FDFSStorageClusterStat *storage_infos,
        const int storage_count)
{
    const FDFSStorageClusterStat *storage;
    const FDFSStorageClusterStat *end;
    char up_time_buff[32];
    char last_hb_time_buff[32];

    printf("\n");
    end = storage_infos + storage_count;
    for (storage=storage_infos; storage<end; storage++)
    {
        if (use_storage_id)
        {
            printf("%s ", storage->id);
        }
        printf("%s %s, ", storage->host, get_storage_status_caption(
                    storage->status));
        if (use_trunk_file)
        {
            printf("is_trunk_server: %d, ", storage->is_trunk_server);
        }

        printf("connections {alloc: %d, current: %d, max: %d}\n",
                storage->connection.alloc_count,
                storage->connection.current_count,
                storage->connection.max_count);
        if (show_detail)
        {
            printf("  version: %s, up_time: %s, last_heartbeat_time: %s\n\n",
                    storage->version, formatDatetime(storage->up_time,
                        "%Y-%m-%d %H:%M:%S", up_time_buff,
                        sizeof(up_time_buff)), formatDatetime(
                        storage->last_heartbeat_time, "%Y-%m-%d %H:%M:%S",
                        last_hb_time_buff, sizeof(last_hb_time_buff)));
        }
    }

    printf("\nstorage server count: %d\n\n", storage_count);
}

int main(int argc, char *argv[])
{
	char *config_filename = FDFS_CLIENT_DEFAULT_CONFIG_FILENAME;
    StorageStatFilter filter;
    FDFSStorageClusterStat storage_infos[FDFS_MAX_STORAGES];
    int storage_count;
    int ch;
	int result;

    memset(&filter, 0, sizeof(filter));
    while ((ch=getopt(argc, argv, "hc:ANTtd")) != -1) {
        switch (ch) {
            case 'h':
                usage(argv);
                return 0;
            case 'c':
                config_filename = optarg;
                break;
            case 'A':
            case 'N':
                filter.filter_by |= FDFS_STORAGE_STAT_FILTER_BY_STATUS;
                filter.status = FDFS_STORAGE_STATUS_ACTIVE;
                filter.op_type = (ch == 'A' ? '=' : '!');
                break;
            case 'T':
            case 't':
                filter.filter_by |= FDFS_STORAGE_STAT_FILTER_BY_IS_TRUNK_SERVER;
                filter.is_trunk_server = (ch == 'T' ? 1 : 0);
                break;
            case 'd':
                show_detail = true;
                break;
            default:
                usage(argv);
                return 1;
        }
    }

	log_init();
	//g_log_context.log_level = LOG_DEBUG;
	ignore_signal_pipe();

	if ((result=fdfs_client_init(config_filename)) != 0)
    {
        return result;
    }
	load_log_level_ex(config_filename);

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		fdfs_client_destroy();
		return errno != 0 ? errno : ECONNREFUSED;
	}

    if ((result=storage_cluster_stat(pTrackerServer, &filter,
                    &use_storage_id, &use_trunk_file, storage_infos,
                    FDFS_MAX_STORAGES, &storage_count)) == 0)
    {
        if (storage_count > 0)
        {
            output(storage_infos, storage_count);
        }
    }

	tracker_close_connection_ex(pTrackerServer, true);
	fdfs_client_destroy();
	return 0;
}
