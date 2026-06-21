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
static bool show_detail = false;

static void usage(char *argv[])
{
    fprintf(stderr, "Usage: %s [-c config_filename=%s]\n"
            "\t[-A] list ACTIVE trackers\n"
            "\t[-N] list None ACTIVE trackers\n"
            "\t[-L] list leader tracker\n"
            "\t[-F] list follower trackers\n"
            "\t[-d] show detail / more info\n\n",
            argv[0], FDFS_CLIENT_DEFAULT_CONFIG_FILENAME);
}

static void output(const FDFSTrackerInfo *tracker_infos,
        const int tracker_count)
{
    const FDFSTrackerInfo *tracker;
    const FDFSTrackerInfo *end;
    char up_time_buff[32];

    end = tracker_infos + tracker_count;
    for (tracker=tracker_infos; tracker<end; tracker++)
    {
        if (show_detail)
        {
            printf("%d. %s, version: %s, up_time: %s, is_leader: %d, "
                    "is_active: %d\n", (int)(tracker - tracker_infos) + 1,
                    tracker->host, tracker->version, formatDatetime(
                        tracker->up_time, "%Y-%m-%d %H:%M:%S",
                        up_time_buff, sizeof(up_time_buff)),
                    tracker->is_leader, tracker->is_active);
        }
        else
        {
            printf("%d. %s, is_leader: %d, is_active: %d\n",
                    (int)(tracker - tracker_infos) + 1, tracker->host,
                    tracker->is_leader, tracker->is_active);
        }
    }

    printf("\ntracker server count: %d\n", tracker_count);
}

int main(int argc, char *argv[])
{
	char *config_filename = FDFS_CLIENT_DEFAULT_CONFIG_FILENAME;
    TrackerStatFilter filter;
    FDFSTrackerInfo tracker_infos[FDFS_MAX_TRACKERS];
    int tracker_count;
    int ch;
	int result;

    memset(&filter, 0, sizeof(filter));
    while ((ch=getopt(argc, argv, "hc:ANLFd")) != -1) {
        switch (ch) {
            case 'h':
                usage(argv);
                return 0;
            case 'c':
                config_filename = optarg;
                break;
            case 'A':
            case 'N':
                filter.filter_by |= FDFS_TRACKER_STAT_FILTER_BY_IS_ACTIVE;
                filter.is_active = (ch == 'A');
                break;
            case 'L':
            case 'F':
                filter.filter_by |= FDFS_TRACKER_STAT_FILTER_BY_IS_LEADER;
                filter.is_leader = (ch == 'L');
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

    if ((result=tracker_list_trackers(pTrackerServer, &filter, tracker_infos,
                    FDFS_MAX_TRACKERS, &tracker_count)) == 0)
    {
        if (tracker_count > 0)
        {
            output(tracker_infos, tracker_count);
        }
    }

	tracker_close_connection_ex(pTrackerServer, true);
	fdfs_client_destroy();
	return 0;
}
