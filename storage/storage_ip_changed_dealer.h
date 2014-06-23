/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//storage_ip_changed_dealer.h

#ifndef _STORAGE_IP_CHANGED_DEALER_H_
#define _STORAGE_IP_CHANGED_DEALER_H_

#include "tracker_types.h"
#include "tracker_client_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

int storage_get_my_tracker_client_ip();

int storage_changelog_req();
int storage_check_ip_changed();

#ifdef __cplusplus
}
#endif

#endif

