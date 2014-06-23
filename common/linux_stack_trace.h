/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//linux_stack_trace.h
#ifndef LINUX_STACK_TRACE_H
#define LINUX_STACK_TRACE_H 

#include "common_define.h"

#ifdef __cplusplus
extern "C" {
#endif

void signal_stack_trace_print(int signum, siginfo_t *info, void *ptr);

#ifdef __cplusplus
}
#endif

#endif

