/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_client_thread.h

#ifndef _TRACKER_CLIENT_THREAD_H_
#define _TRACKER_CLIENT_THREAD_H_

#include "tracker_types.h"
#include "storage_sync.h"

#ifdef __cplusplus
extern "C" {
#endif

int tracker_report_init();
int tracker_report_destroy();
int tracker_report_thread_start();
int kill_tracker_report_threads();

int tracker_report_join(ConnectionInfo *pTrackerServer, \
		const int tracker_index, const bool sync_old_done);
int tracker_report_storage_status(ConnectionInfo *pTrackerServer, \
		FDFSStorageBrief *briefServer);
int tracker_sync_src_req(ConnectionInfo *pTrackerServer, \
		StorageBinLogReader *pReader);
int tracker_sync_diff_servers(ConnectionInfo *pTrackerServer, \
		FDFSStorageBrief *briefServers, const int server_count);
int tracker_deal_changelog_response(ConnectionInfo *pTrackerServer);

#ifdef __cplusplus
}
#endif

#endif
