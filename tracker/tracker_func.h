/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_func.h

#ifndef _TRACKER_FUNC_H_
#define _TRACKER_FUNC_H_

#include "tracker_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int tracker_load_from_conf_file(const char *filename, \
		char *bind_addr, const int addr_size);

#ifdef __cplusplus
}
#endif

#endif
