/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//storage_func.h

#ifndef _STORAGE_FUNC_H_
#define _STORAGE_FUNC_H_

#include "tracker_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef char * (*get_filename_func)(const void *pArg, \
			char *full_filename);

int storage_write_to_fd(int fd, get_filename_func filename_func, \
		const void *pArg, const char *buff, const int len);
int storage_func_init(const char *filename, \
		char *bind_addr, const int addr_size);
int storage_func_destroy();

int storage_write_to_stat_file();

int storage_write_to_sync_ini_file();

bool storage_server_is_myself(const FDFSStorageBrief *pStorageBrief);

bool storage_id_is_myself(const char *storage_id);

int storage_set_tracker_client_ips(ConnectionInfo *conn,
        const int tracker_index);

int storage_check_and_make_global_data_path();

int storage_logic_to_local_full_filename(const char *logic_filename,
        const int logic_filename_len, int *store_path_index,
        char *full_filename, const int filename_size);

/*
int write_serialized(int fd, const char *buff, size_t count, const bool bSync);
int fsync_serialized(int fd);
int recv_file_serialized(int sock, const char *filename, \
		const int64_t file_bytes);
*/

#ifdef __cplusplus
}
#endif

#endif
