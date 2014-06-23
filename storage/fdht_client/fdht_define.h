/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//fdht_define.h

#ifndef _FDHT_DEFINE_H_
#define _FDHT_DEFINE_H_

#include "common_define.h"

#define FDHT_SERVER_DEFAULT_PORT  24000
#define FDHT_DEFAULT_PROXY_PORT   12200
#define FDHT_MAX_PKG_SIZE         64 * 1024
#define FDHT_MIN_BUFF_SIZE        64 * 1024
#define FDHT_DEFAULT_MAX_THREADS  64
#define DEFAULT_SYNC_DB_INVERVAL  86400
#define DEFAULT_SYNC_WAIT_MSEC    100
#define DEFAULT_CLEAR_EXPIRED_INVERVAL          0
#define DEFAULT_DB_DEAD_LOCK_DETECT_INVERVAL 1000
#define FDHT_MAX_KEY_COUNT_PER_REQ      128
#define SYNC_BINLOG_BUFF_DEF_INTERVAL   60
#define COMPRESS_BINLOG_DEF_INTERVAL    86400
#define DEFAULT_SYNC_STAT_FILE_INTERVAL 300
#define FDHT_DEFAULT_SYNC_MARK_FILE_FREQ     5000

#define FDHT_STORE_TYPE_BDB      1
#define FDHT_STORE_TYPE_MPOOL    2

#define FDHT_DEFAULT_MPOOL_INIT_CAPACITY    10000
#define FDHT_DEFAULT_MPOOL_LOAD_FACTOR       0.75
#define FDHT_DEFAULT_MPOOL_CLEAR_MIN_INTEVAL  300

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif
