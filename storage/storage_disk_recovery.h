/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//storage_disk_recovery.h

#ifndef _STORAGE_DISK_RECOVERY_H_
#define _STORAGE_DISK_RECOVERY_H_

#include "tracker_types.h"
#include "tracker_client_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

int storage_disk_recovery_start(const int store_path_index);
int storage_disk_recovery_restore(const char *pBasePath);

#ifdef __cplusplus
}
#endif

#endif

