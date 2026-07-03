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

static ConnectionInfo *pTrackerServer;
static bool use_trunk_file;

static void usage(char *argv[])
{
    fprintf(stderr, "Usage: %s [-c config_filename=%s]\n"
            "\t[-A] list space available groups / volumns\n"
            "\t[-N] list None space available groups / volumns\n"
            "\t[-D] list disk space available groups / volumns\n"
            "\t[-d] list None disk space available groups / volumns\n"
            "\nWhen use_trunk_file is true, including options:\n"
            "\t[-T] list trunk space available groups / volumns\n"
            "\t[-t] list None trunk space available groups / volumns\n\n"
            "Note: space available when disk space available "
            "or trunk space available\n\n"
            "For example:\n" "\t%s -A\n" "\t%s -N\n\n",
            argv[0], FDFS_CLIENT_DEFAULT_CONFIG_FILENAME,
            argv[0], argv[0]);
}

static void group_output(const FDFSStorageVolumnStat *group)
{
    int64_t total_avail_space;
    int64_t total_mb;
    double space_util;
    double avail_ratio;
    char szDiskTotalSpace[32];
    char szDiskFreeSpace[32];
    char szDiskReservedSpace[32];
    char szDiskAvailSpace[32];
    char szTrunkSpace[32];
    char szTotalAvailSpace[32];

    printf("%s disk_available: %d", group->group_name,
            group->is_disk_available);
    if (use_trunk_file)
    {
        printf(", trunk_available: %d", group->is_trunk_available);
    }

    total_mb = group->total_mb - group->reserved_mb;
    if (total_mb > 0)
    {
        space_util = (double)(total_mb - group->avail_mb) /
            (double)total_mb * 100.00;
    }
    else
    {
        space_util = 0.00;
    }
    printf(", disk space {total: %s, free: %s, reserved: %s, "
            "avail: %s, util: %.2f%%}",
            FDFS_MB_TO_HUMAN_STR(group->total_mb, szDiskTotalSpace),
            FDFS_MB_TO_HUMAN_STR(group->free_mb, szDiskFreeSpace),
            FDFS_MB_TO_HUMAN_STR(group->reserved_mb, szDiskReservedSpace),
            FDFS_MB_TO_HUMAN_STR(group->avail_mb, szDiskAvailSpace),
            space_util);
    if (use_trunk_file)
    {
        total_avail_space = group->avail_mb + group->trunk_free_mb;
        if (total_mb > 0)
        {
            avail_ratio = (double)total_avail_space /
                (double)total_mb * 100.00;
        }
        else
        {
            avail_ratio = 0.00;
        }
        printf(", trunk free space: %s, total available {space: %s, "
                "ratio: %.2f%%}\n\n",
                FDFS_MB_TO_HUMAN_STR(group->trunk_free_mb, szTrunkSpace),
                FDFS_MB_TO_HUMAN_STR(total_avail_space, szTotalAvailSpace),
                avail_ratio);
    }
    else
    {
        printf("\n\n");
    }
}

static void output(const FDFSStorageVolumnStat *groups,
        const int group_count)
{
    const FDFSStorageVolumnStat *group;
    const FDFSStorageVolumnStat *end;
    FDFSStorageVolumnStat sum;
    int avail_count;
    int i;

    avail_count = 0;
    memset(&sum, 0, sizeof(sum));
    printf("\n");
    end = groups + group_count;
    for (group=groups; group<end; group++)
    {
        sum.total_mb += group->total_mb;
        sum.free_mb += group->free_mb;
        sum.reserved_mb += group->reserved_mb;
        sum.avail_mb += group->avail_mb;
        sum.trunk_free_mb += group->trunk_free_mb;
        if ((!sum.is_disk_available) && group->is_disk_available)
        {
            sum.is_disk_available = 1;
        }
        if ((!sum.is_trunk_available) && group->is_trunk_available)
        {
            sum.is_trunk_available = 1;
        }

        if (group->is_disk_available || group->is_trunk_available)
        {
            ++avail_count;
        }

        group_output(group);
    }

    if (group_count <= 1)
    {
        printf("storage volumn / group count: %d\n\n", group_count);
    }
    else
    {
        for (i=0; i<32; i++)
        {
            putchar('=');
        }
        printf(" VOLUMNS SUMMARY ");
        for (i=0; i<32; i++)
        {
            putchar('=');
        }
        printf("\n");
        group_output(&sum);
        printf("volumn / group counts {total: %d, available: %d}\n\n",
                group_count, avail_count);
    }
}

int main(int argc, char *argv[])
{
	char *config_filename = FDFS_CLIENT_DEFAULT_CONFIG_FILENAME;
    VolumnStatFilter filter;
    FDFSStorageVolumnStat groups[FDFS_MAX_GROUPS];
    int group_count;
    int ch;
	int result;

    memset(&filter, 0, sizeof(filter));
    while ((ch=getopt(argc, argv, "hc:ANDdTt")) != -1) {
        switch (ch) {
            case 'h':
                usage(argv);
                return 0;
            case 'c':
                config_filename = optarg;
                break;
            case 'A':
            case 'N':
                filter.filter_by |= FDFS_VOLUMN_STAT_FILTER_BY_IS_SPACE_AVAILABLE;
                filter.is_space_available = (ch == 'A' ? 1 : 0);
                break;
            case 'D':
            case 'd':
                filter.filter_by |= FDFS_VOLUMN_STAT_FILTER_BY_IS_DISK_AVAILABLE;
                filter.is_disk_available = (ch == 'D' ? 1 : 0);
                break;
            case 'T':
            case 't':
                filter.filter_by |= FDFS_VOLUMN_STAT_FILTER_BY_IS_TRUNK_AVAILABLE;
                filter.is_trunk_available = (ch == 'T' ? 1 : 0);
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

    if ((result=storage_volumn_stat(pTrackerServer, &filter,
                    &use_trunk_file, groups,
                    FDFS_MAX_GROUPS, &group_count)) == 0)
    {
        if (group_count > 0)
        {
            output(groups, group_count);
        }
    }

	tracker_close_connection_ex(pTrackerServer, true);
	fdfs_client_destroy();
	return 0;
}
