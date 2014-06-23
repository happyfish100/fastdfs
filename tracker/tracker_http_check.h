/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_httpd.h

#ifndef _TRACKER_HTTP_CHECK_H_
#define _TRACKER_HTTP_CHECK_H_

#ifdef __cplusplus
extern "C" {
#endif

extern bool g_http_check_flag;

int tracker_http_check_start();
int tracker_http_check_stop();

#ifdef __cplusplus
}
#endif

#endif
