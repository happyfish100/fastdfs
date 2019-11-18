/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_relationship.h

#ifndef _TRACKER_RELATIONSHIP_H_
#define _TRACKER_RELATIONSHIP_H_

#include <time.h>
#include <pthread.h>
#include "tracker_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool g_if_leader_self;  //if I am leader

int tracker_relationship_init();
int tracker_relationship_destroy();

void relationship_set_tracker_leader(const int server_index,
        ConnectionInfo *pLeader, const bool if_leader_self);

#ifdef __cplusplus
}
#endif

#endif

