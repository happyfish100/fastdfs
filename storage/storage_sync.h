/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//storage_sync.h

#ifndef _STORAGE_SYNC_H_
#define _STORAGE_SYNC_H_

#include "fastcommon/fc_list.h"
#include "storage_func.h"

#define STORAGE_OP_TYPE_SOURCE_CREATE_FILE    'C'  //upload file
#define STORAGE_OP_TYPE_SOURCE_APPEND_FILE    'A'  //append file
#define STORAGE_OP_TYPE_SOURCE_DELETE_FILE    'D'  //delete file
#define STORAGE_OP_TYPE_SOURCE_UPDATE_FILE    'U'  //for whole file update such as metadata file
#define STORAGE_OP_TYPE_SOURCE_MODIFY_FILE    'M'  //for part modify
#define STORAGE_OP_TYPE_SOURCE_TRUNCATE_FILE  'T'  //truncate file
#define STORAGE_OP_TYPE_SOURCE_CREATE_LINK    'L'  //create symbol link
#define STORAGE_OP_TYPE_SOURCE_RENAME_FILE    'R'  //rename appender file to normal file
#define STORAGE_OP_TYPE_REPLICA_CREATE_FILE   'c'
#define STORAGE_OP_TYPE_REPLICA_APPEND_FILE   'a'
#define STORAGE_OP_TYPE_REPLICA_DELETE_FILE   'd'
#define STORAGE_OP_TYPE_REPLICA_UPDATE_FILE   'u'
#define STORAGE_OP_TYPE_REPLICA_MODIFY_FILE   'm'
#define STORAGE_OP_TYPE_REPLICA_TRUNCATE_FILE 't'
#define STORAGE_OP_TYPE_REPLICA_CREATE_LINK   'l'
#define STORAGE_OP_TYPE_REPLICA_RENAME_FILE   'r'

#define STORAGE_BINLOG_BUFFER_SIZE		64 * 1024
#define STORAGE_BINLOG_LINE_SIZE		256

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    struct fc_list_head link;
	char storage_id[FDFS_STORAGE_ID_MAX_SIZE];
	char mark_filename[MAX_PATH_SIZE];
	bool need_sync_old;
	bool sync_old_done;
	bool last_file_exist;   //if the last file exist on the dest server
	BinLogBuffer binlog_buff;
	time_t until_timestamp;
	int binlog_index;
	int binlog_fd;
	int64_t binlog_offset;
	int64_t scan_row_count;
	int64_t sync_row_count;

	int64_t last_scan_rows;  //for write to mark file
	int64_t last_sync_rows;  //for write to mark file
} StorageBinLogReader;

typedef struct
{
	time_t timestamp;
	char op_type;
	char filename[128];  //filename with path index prefix which should be trimed
	char true_filename[128]; //pure filename
	char src_filename[128];  //src filename with path index prefix
	int filename_len;
	int true_filename_len;
	int src_filename_len;
	int store_path_index;
} StorageBinLogRecord;

extern int g_binlog_fd;
extern int g_binlog_index;

extern volatile int g_storage_sync_thread_count;

int storage_sync_init();
int storage_sync_destroy();

#define storage_binlog_write(timestamp, op_type, filename_str, filename_len) \
	storage_binlog_write_ex(timestamp, op_type, filename_str, filename_len, NULL, 0)

int storage_binlog_write_ex(const time_t timestamp, const char op_type,
		const char *filename_str, const int filename_len,
        const char *extra_str, const int extra_len);

int storage_binlog_read(StorageBinLogReader *pReader,
        StorageBinLogRecord *pRecord, int *record_length);

int storage_sync_thread_start(const FDFSStorageBrief *pStorage);
int kill_storage_sync_threads();
int fdfs_binlog_sync_func(void *args);

char *get_mark_filename_by_reader(StorageBinLogReader *pReader);
int storage_unlink_mark_file(const char *storage_id);
int storage_rename_mark_file(const char *old_ip_addr, const int old_port, \
		const char *new_ip_addr, const int new_port);

int storage_open_readable_binlog(StorageBinLogReader *pReader, \
		get_filename_func filename_func, const void *pArg);

int storage_reader_init(FDFSStorageBrief *pStorage, StorageBinLogReader *pReader);
void storage_reader_destroy(StorageBinLogReader *pReader);

int storage_report_storage_status(const char *storage_id,
		const char *ip_addr, const char status);

int fdfs_binlog_compress_func(void *args);

void storage_reader_add_to_list(StorageBinLogReader *pReader);

void storage_reader_remove_from_list(StorageBinLogReader *pReader);

#ifdef __cplusplus
}
#endif

#endif
