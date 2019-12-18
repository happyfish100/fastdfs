/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//trunk_sync.h

#ifndef _TRUNK_SYNC_H_
#define _TRUNK_SYNC_H_

#include "tracker_types.h"
#include "storage_func.h"
#include "trunk_mem.h"

#define TRUNK_OP_TYPE_ADD_SPACE		'A'
#define TRUNK_OP_TYPE_DEL_SPACE		'D'

#define TRUNK_BINLOG_BUFFER_SIZE	(64 * 1024)
#define TRUNK_BINLOG_LINE_SIZE		128

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	char storage_id[FDFS_STORAGE_ID_MAX_SIZE];
	char mark_filename[MAX_PATH_SIZE];
	BinLogBuffer binlog_buff;
	int binlog_fd;
	int64_t binlog_offset;
	int64_t last_binlog_offset;  //for write to mark file
} TrunkBinLogReader;

typedef struct
{
	time_t timestamp;
	char op_type;
	FDFSTrunkFullInfo trunk;
} TrunkBinLogRecord;

extern int g_trunk_sync_thread_count;

int trunk_sync_init();
int trunk_sync_destroy();

int trunk_binlog_write_buffer(const char *buff, const int length);

int trunk_binlog_write(const int timestamp, const char op_type, \
		const FDFSTrunkFullInfo *pTrunk);

int trunk_binlog_truncate();

int trunk_binlog_read(TrunkBinLogReader *pReader, \
		      TrunkBinLogRecord *pRecord, int *record_length);

int trunk_sync_thread_start_all();
int trunk_sync_thread_start(const FDFSStorageBrief *pStorage);
int kill_trunk_sync_threads();
int trunk_binlog_sync_func(void *args);
void trunk_waiting_sync_thread_exit();

char *get_trunk_binlog_filename(char *full_filename);
char *trunk_mark_filename_by_reader(const void *pArg, char *full_filename);
int trunk_unlink_all_mark_files(const bool force_delete);
int trunk_unlink_mark_file(const char *storage_id);
int trunk_rename_mark_file(const char *old_ip_addr, const int old_port, \
		const char *new_ip_addr, const int new_port);

int trunk_open_readable_binlog(TrunkBinLogReader *pReader, \
		get_filename_func filename_func, const void *pArg);

int trunk_reader_init(const FDFSStorageBrief *pStorage,
        TrunkBinLogReader *pReader, const bool reset_binlog_offset);
void trunk_reader_destroy(TrunkBinLogReader *pReader);

//trunk binlog compress
int trunk_binlog_compress_delete_binlog_rollback_file(const bool silence);
int trunk_binlog_compress_delete_rollback_files(const bool silence);
int trunk_binlog_compress_delete_temp_files_after_commit();
int trunk_binlog_compress_apply();
int trunk_binlog_compress_commit();
int trunk_binlog_compress_rollback();

int trunk_sync_notify_thread_reset_offset();
int trunk_binlog_get_write_version();

char *get_trunk_binlog_tmp_filename_ex(const char *binlog_filename,
        char *tmp_filename);

static inline char *get_trunk_binlog_tmp_filename(char *tmp_filename)
{
    return get_trunk_binlog_tmp_filename_ex(NULL, tmp_filename);
}

#ifdef __cplusplus
}
#endif

#endif
