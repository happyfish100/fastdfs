/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//storage_dump.h

#ifndef _STORAGE_DUMP_H
#define _STORAGE_DUMP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdfs_define.h"
#include "tracker_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int fdfs_dump_storage_global_vars_to_file(const char *filename);

#ifdef __cplusplus
}
#endif

#endif
