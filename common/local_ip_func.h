/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//local_ip_func.h

#ifndef _LOCAL_IP_FUNC_H
#define _LOCAL_IP_FUNC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common_define.h"

#define FAST_IF_ALIAS_PREFIX_MAX_SIZE 32
#define FAST_MAX_LOCAL_IP_ADDRS	  16

#ifdef __cplusplus
extern "C" {
#endif

extern int g_local_host_ip_count;
extern char g_local_host_ip_addrs[FAST_MAX_LOCAL_IP_ADDRS * \
				IP_ADDRESS_SIZE];
extern char g_if_alias_prefix[FAST_IF_ALIAS_PREFIX_MAX_SIZE];

void load_local_host_ip_addrs();
bool is_local_host_ip(const char *client_ip);
int insert_into_local_host_ip(const char *client_ip);
void print_local_host_ip_addrs();

#ifdef __cplusplus
}
#endif

#endif

