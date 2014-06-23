/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//fdht_proto.h

#ifndef _FDHT_PROTO_H_
#define _FDHT_PROTO_H_

#include "fdht_types.h"
#include "fdht_proto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int fdht_recv_header(FDHTServerInfo *pServer, fdht_pkg_size_t *in_bytes);

int fdht_recv_response(FDHTServerInfo *pServer, \
		char **buff, const int buff_size, \
		fdht_pkg_size_t *in_bytes);
int fdht_quit(FDHTServerInfo *pServer);

/**
* connect to the server (block mode)
* params:
*	pServer: server
* return: 0 success, !=0 fail, return the error code
**/
int fdht_connect_server(FDHTServerInfo *pServer);

/**
* connect to the server (non-block mode)
* params:
*	pServer: server
* return: 0 success, !=0 fail, return the error code
**/
int fdht_connect_server_nb(FDHTServerInfo *pServer, const int connect_timeout);

/**
* close connection to the server
* params:
*	pServer: server
* return:
**/
void fdht_disconnect_server(FDHTServerInfo *pServer);


int fdht_client_set(FDHTServerInfo *pServer, const char keep_alive, \
	const time_t timestamp, const time_t expires, const int prot_cmd, \
	const int key_hash_code, FDHTKeyInfo *pKeyInfo, \
	const char *pValue, const int value_len);

int fdht_client_delete(FDHTServerInfo *pServer, const char keep_alive, \
	const time_t timestamp, const int prot_cmd, \
	const int key_hash_code, FDHTKeyInfo *pKeyInfo);

int fdht_client_heart_beat(FDHTServerInfo *pServer);

#ifdef __cplusplus
}
#endif

#endif

