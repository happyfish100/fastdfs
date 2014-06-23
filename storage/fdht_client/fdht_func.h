/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//fdht_func.h

#ifndef _FDHT_FUNC_H_
#define _FDHT_FUNC_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "fdht_types.h"
#include "ini_file_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

int fdht_split_ids(const char *szIds, int **ppIds, int *id_count);

#define fdht_load_groups(pIniContext, pGroupArray) \
	fdht_load_groups_ex(pIniContext, pGroupArray, true)

int fdht_load_groups_ex(IniContext *pIniContext, \
		GroupArray *pGroupArray, const bool bLoadProxyParams);

int fdht_copy_group_array(GroupArray *pDestGroupArray, \
		GroupArray *pSrcGroupArray);
void fdht_free_group_array(GroupArray *pGroupArray);

#ifdef __cplusplus
}
#endif

#endif

