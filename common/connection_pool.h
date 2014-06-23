/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//connection_pool.h

#ifndef _CONNECTION_POOL_H
#define _CONNECTION_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common_define.h"
#include "pthread_func.h"
#include "hash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int sock;
	int port;
	char ip_addr[IP_ADDRESS_SIZE];
} ConnectionInfo;

struct tagConnectionManager;

typedef struct tagConnectionNode {
	ConnectionInfo *conn;
	struct tagConnectionManager *manager;
	struct tagConnectionNode *next;
	time_t atime;  //last access time
} ConnectionNode;

typedef struct tagConnectionManager {
	ConnectionNode *head;
	int total_count;  //total connections
	int free_count;   //free connections
	pthread_mutex_t lock;
} ConnectionManager;

typedef struct tagConnectionPool {
	HashArray hash_array;  //key is ip:port, value is ConnectionManager
	pthread_mutex_t lock;
	int connect_timeout;
	int max_count_per_entry;  //0 means no limit

	/*
	connections whose the idle time exceeds this time will be closed
	*/
	int max_idle_time;
} ConnectionPool;

int conn_pool_init(ConnectionPool *cp, int connect_timeout, \
	const int max_count_per_entry, const int max_idle_time);
void conn_pool_destroy(ConnectionPool *cp);

ConnectionInfo *conn_pool_get_connection(ConnectionPool *cp, 
	const ConnectionInfo *conn, int *err_no);

#define conn_pool_close_connection(cp, conn) \
	conn_pool_close_connection_ex(cp, conn, false)

int conn_pool_close_connection_ex(ConnectionPool *cp, ConnectionInfo *conn, 
	const bool bForce);

void conn_pool_disconnect_server(ConnectionInfo *pConnection);

int conn_pool_connect_server(ConnectionInfo *pConnection, \
		const int connect_timeout);

int conn_pool_get_connection_count(ConnectionPool *cp);

#ifdef __cplusplus
}
#endif

#endif

