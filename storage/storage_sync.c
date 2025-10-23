/** * Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//storage_sync.c

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
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/pthread_func.h"
#include "fastcommon/sched_thread.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/fc_atomic.h"
#include "fastcommon/thread_pool.h"
#include "sf/sf_func.h"
#include "fdfs_define.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_ip_changed_dealer.h"
#include "tracker_client_thread.h"
#include "storage_client.h"
#include "trunk_mem.h"
#include "storage_sync_func.h"
#include "storage_sync.h"

#define SYNC_BINLOG_FILE_MAX_SIZE	(1024 * 1024 * 1024)
#define SYNC_BINLOG_WRITE_BUFF_SIZE	(16 * 1024)

#define SYNC_BINLOG_FILE_PREFIX_STR  "binlog"
#define SYNC_BINLOG_FILE_PREFIX_LEN  \
    (sizeof(SYNC_BINLOG_FILE_PREFIX_STR) - 1)

#define SYNC_BINLOG_INDEX_FILENAME_OLD_STR  \
    SYNC_BINLOG_FILE_PREFIX_STR".index"
#define SYNC_BINLOG_INDEX_FILENAME_OLD_LEN  \
    (sizeof(SYNC_BINLOG_INDEX_FILENAME_OLD_STR) - 1)

#define SYNC_BINLOG_INDEX_FILENAME_STR  \
    SYNC_BINLOG_FILE_PREFIX_STR"_index.dat"
#define SYNC_BINLOG_INDEX_FILENAME_LEN  \
    (sizeof(SYNC_BINLOG_INDEX_FILENAME_STR) - 1)

#define SYNC_MARK_FILE_EXT_STR  ".mark"
#define SYNC_MARK_FILE_EXT_LEN  \
    (sizeof(SYNC_MARK_FILE_EXT_STR) - 1)

#define SYNC_BINLOG_FILE_EXT_LEN  3
#define SYNC_BINLOG_FILE_EXT_FMT  ".%0"FC_MACRO_TOSTRING(SYNC_BINLOG_FILE_EXT_LEN)"d"

#define SYNC_DIR_NAME_STR  "sync"
#define SYNC_DIR_NAME_LEN  \
    (sizeof(SYNC_DIR_NAME_STR) - 1)

#define SYNC_SUBDIR_NAME_STR  "data/"SYNC_DIR_NAME_STR
#define SYNC_SUBDIR_NAME_LEN  \
    (sizeof(SYNC_SUBDIR_NAME_STR) - 1)

#define MARK_ITEM_BINLOG_FILE_INDEX_STR  "binlog_index"
#define MARK_ITEM_BINLOG_FILE_INDEX_LEN  \
    (sizeof(MARK_ITEM_BINLOG_FILE_INDEX_STR) - 1)

#define MARK_ITEM_BINLOG_FILE_OFFSET_STR  "binlog_offset"
#define MARK_ITEM_BINLOG_FILE_OFFSET_LEN  \
    (sizeof(MARK_ITEM_BINLOG_FILE_OFFSET_STR) - 1)

#define MARK_ITEM_NEED_SYNC_OLD_STR  "need_sync_old"
#define MARK_ITEM_NEED_SYNC_OLD_LEN  \
    (sizeof(MARK_ITEM_NEED_SYNC_OLD_STR) - 1)

#define MARK_ITEM_SYNC_OLD_DONE_STR  "sync_old_done"
#define MARK_ITEM_SYNC_OLD_DONE_LEN  \
    (sizeof(MARK_ITEM_SYNC_OLD_DONE_STR) - 1)

#define MARK_ITEM_UNTIL_TIMESTAMP_STR  "until_timestamp"
#define MARK_ITEM_UNTIL_TIMESTAMP_LEN  \
    (sizeof(MARK_ITEM_UNTIL_TIMESTAMP_STR) - 1)

#define MARK_ITEM_SCAN_ROW_COUNT_STR  "scan_row_count"
#define MARK_ITEM_SCAN_ROW_COUNT_LEN  \
    (sizeof(MARK_ITEM_SCAN_ROW_COUNT_STR) - 1)

#define MARK_ITEM_SYNC_ROW_COUNT_STR  "sync_row_count"
#define MARK_ITEM_SYNC_ROW_COUNT_LEN  \
    (sizeof(MARK_ITEM_SYNC_ROW_COUNT_STR) - 1)

#define BINLOG_INDEX_ITEM_CURRENT_WRITE_STR  "current_write"
#define BINLOG_INDEX_ITEM_CURRENT_WRITE_LEN  \
    (sizeof(BINLOG_INDEX_ITEM_CURRENT_WRITE_STR) - 1)

#define BINLOG_INDEX_ITEM_CURRENT_COMPRESS_STR  "current_compress"
#define BINLOG_INDEX_ITEM_CURRENT_COMPRESS_LEN  \
    (sizeof(BINLOG_INDEX_ITEM_CURRENT_COMPRESS_STR) - 1)

int g_binlog_fd = -1;
int g_binlog_index = 0;

volatile int g_storage_sync_thread_count;

struct storage_dispatch_context;
typedef struct {
    int result;
    int record_len;
    int binlog_index;
    int64_t binlog_offset;
    int64_t scan_row_count;
    struct storage_dispatch_context *dispatch_ctx;
    ConnectionInfo storage_server;
    StorageBinLogRecord record;
} StorageSyncTaskInfo;

typedef struct {
    StorageSyncTaskInfo *tasks;
    StorageSyncTaskInfo *end;
    int count;
} StorageSyncTaskArray;

typedef struct storage_dispatch_context {
    int last_binlog_index;
    int64_t last_binlog_offset;
    int64_t scan_row_count;
    StorageSyncTaskArray task_array;
    SFSynchronizeContext notify_ctx;
    StorageBinLogReader *pReader;
} StorageDispatchContext;

static struct {
    int64_t binlog_file_size;
    int binlog_compress_index;

    char *binlog_write_cache_buff;
    int binlog_write_cache_len;
    int binlog_write_version;

    /* save sync thread ids */
    pthread_t *tids;

    pthread_mutex_t lock;

    struct fc_list_head reader_head;
    FCThreadPool thread_pool;
} storage_sync_ctx = {0, 0, NULL, 0, 1, NULL};

#define BINLOG_WRITE_CACHE_BUFF  storage_sync_ctx.binlog_write_cache_buff
#define BINLOG_WRITE_CACHE_LEN   storage_sync_ctx.binlog_write_cache_len
#define BINLOG_WRITE_VERSION     storage_sync_ctx.binlog_write_version
#define STORAGE_SYNC_TIDS        storage_sync_ctx.tids
#define SYNC_THREAD_LOCK         storage_sync_ctx.lock
#define SYNC_READER_HEAD         storage_sync_ctx.reader_head
#define SYNC_THREAD_POOL         storage_sync_ctx.thread_pool

static int storage_write_to_mark_file(StorageBinLogReader *pReader);
static int storage_binlog_reader_skip(StorageBinLogReader *pReader);
static int storage_binlog_fsync(const bool bNeedLock);
static int storage_binlog_preread(StorageBinLogReader *pReader);

/**
8 bytes: filename bytes
8 bytes: file size
4 bytes: source op timestamp
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
filename bytes : filename
file size bytes: file content
**/
static int storage_sync_copy_file(ConnectionInfo *pStorageServer,
	StorageBinLogReader *pReader, const StorageBinLogRecord *pRecord,
	char proto_cmd)
{
	TrackerHeader *pHeader;
	char *p;
	char *pBuff;
	char full_filename[MAX_PATH_SIZE];
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+256];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	struct stat stat_buf;
	FDFSTrunkFullInfo trunkInfo;
	FDFSTrunkHeader trunkHeader;
	int64_t file_offset;
	int64_t in_bytes;
	int64_t total_send_bytes;
	int result;
	bool need_sync_file;

	if ((result=trunk_file_stat(pRecord->store_path_index,
		pRecord->true_filename, pRecord->true_filename_len,
		&stat_buf, &trunkInfo, &trunkHeader)) != 0)
	{
		if (result == ENOENT)
		{
			if(pRecord->op_type==STORAGE_OP_TYPE_SOURCE_CREATE_FILE)
			{
				logDebug("file: "__FILE__", line: %d, " \
					"sync data file, logic file: %s " \
					"not exists, maybe deleted later?", \
					__LINE__, pRecord->filename);
			}

			return 0;
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"call stat fail, logic file: %s, "\
				"error no: %d, error info: %s", \
				__LINE__, pRecord->filename, \
				result, STRERROR(result));
			return result;
		}
	}

	need_sync_file = true;
	if (pReader->last_file_exist && proto_cmd ==
			STORAGE_PROTO_CMD_SYNC_CREATE_FILE)
	{
		FDFSFileInfo file_info;
		result = storage_query_file_info_ex(NULL,
				pStorageServer, g_group_name,
				pRecord->filename, &file_info, true);
		if (result == 0)
		{
			if (file_info.file_size == stat_buf.st_size)
			{
                if (FC_LOG_BY_LEVEL(LOG_DEBUG)) {
                    format_ip_address(pStorageServer->ip_addr, formatted_ip);
                    logDebug("file: "__FILE__", line: %d, "
                            "sync data file, logic file: %s "
                            "on dest server %s:%u already exists, "
                            "and same as mine, ignore it", __LINE__,
                            pRecord->filename, formatted_ip,
                            pStorageServer->port);
                }
				need_sync_file = false;
			}
			else
			{
                format_ip_address(pStorageServer->ip_addr, formatted_ip);
				logWarning("file: "__FILE__", line: %d, "
					"sync data file, logic file: %s "
					"on dest server %s:%u already exists, "
					"but file size: %"PRId64" not same as mine: %"PRId64
					", need re-sync it", __LINE__, pRecord->filename,
                    formatted_ip, pStorageServer->port, file_info.file_size,
                    (int64_t)stat_buf.st_size);

				proto_cmd = STORAGE_PROTO_CMD_SYNC_UPDATE_FILE;
			}
		}
		else if (result != ENOENT)
		{
			return result;
		}
	}

	if (IS_TRUNK_FILE_BY_ID(trunkInfo))
	{
		file_offset = TRUNK_FILE_START_OFFSET(trunkInfo);
		trunk_get_full_filename((&trunkInfo), full_filename, \
				sizeof(full_filename));
	}
	else
	{
		file_offset = 0;
        fc_get_one_subdir_full_filename(
                FDFS_STORE_PATH_STR(pRecord->store_path_index),
                FDFS_STORE_PATH_LEN(pRecord->store_path_index),
                "data", 4, pRecord->true_filename,
                pRecord->true_filename_len, full_filename);
	}

	total_send_bytes = 0;
	//printf("sync create file: %s\n", pRecord->filename);
	do
	{
		int64_t body_len;

		pHeader = (TrackerHeader *)out_buff;
		memset(pHeader, 0, sizeof(TrackerHeader));

		body_len = 2 * FDFS_PROTO_PKG_LEN_SIZE + \
				4 + FDFS_GROUP_NAME_MAX_LEN + \
				pRecord->filename_len;
		if (need_sync_file)
		{
			body_len += stat_buf.st_size;
		}

		long2buff(body_len, pHeader->pkg_len);
		pHeader->cmd = proto_cmd;
		pHeader->status = need_sync_file ? 0 : EEXIST;

		p = out_buff + sizeof(TrackerHeader);

		long2buff(pRecord->filename_len, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		long2buff(stat_buf.st_size, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		int2buff(pRecord->timestamp, p);
		p += 4;

		strcpy(p, g_group_name);
		p += FDFS_GROUP_NAME_MAX_LEN;
		memcpy(p, pRecord->filename, pRecord->filename_len);
		p += pRecord->filename_len;

		if((result=tcpsenddata_nb(pStorageServer->sock, out_buff,
			p - out_buff, SF_G_NETWORK_TIMEOUT)) != 0)
		{
            format_ip_address(pStorageServer->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"sync data to storage server %s:%u fail, errno: %d, "
				"error info: %s", __LINE__, formatted_ip,
				pStorageServer->port, result, STRERROR(result));
			break;
		}

		if (need_sync_file && (stat_buf.st_size > 0) &&
			((result=tcpsendfile_ex(pStorageServer->sock,
			full_filename, file_offset, stat_buf.st_size,
			SF_G_NETWORK_TIMEOUT, &total_send_bytes)) != 0))
		{
            format_ip_address(pStorageServer->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"sync data to storage server %s:%u fail, errno: %d, "
				"error info: %s", __LINE__, formatted_ip,
				pStorageServer->port, result, STRERROR(result));
			break;
		}

		pBuff = in_buff;
		if ((result=fdfs_recv_response(pStorageServer,
			&pBuff, 0, &in_bytes)) != 0)
		{
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
			break;
		}
	} while (0);

	__sync_add_and_fetch(&g_storage_stat.total_sync_out_bytes,
            total_send_bytes);
	if (result == 0)
    {
        __sync_add_and_fetch(&g_storage_stat.success_sync_out_bytes,
                total_send_bytes);
    }

	if (result == EEXIST)
	{
		if (need_sync_file && pRecord->op_type ==
                STORAGE_OP_TYPE_SOURCE_CREATE_FILE)
		{
            format_ip_address(pStorageServer->ip_addr, formatted_ip);
			logWarning("file: "__FILE__", line: %d, "
				"storage server ip: %s:%u, data file: %s already exists, "
				"maybe some mistake?", __LINE__, formatted_ip,
				pStorageServer->port, pRecord->filename);
		}

		pReader->last_file_exist = true;
		return 0;
	}
	else if (result == 0)
	{
		pReader->last_file_exist = false;
		return 0;
	}
	else
	{
		return result;
	}
}

/**
8 bytes: filename bytes
8 bytes: start offset
8 bytes: append length
4 bytes: source op timestamp
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
filename bytes : filename
file size bytes: file content
**/
static int storage_sync_modify_file(ConnectionInfo *pStorageServer, \
	StorageBinLogReader *pReader, StorageBinLogRecord *pRecord, \
	const char cmd)
{
#define SYNC_MODIFY_FIELD_COUNT  3
	TrackerHeader *pHeader;
	char *p;
	char *pBuff;
	char *fields[SYNC_MODIFY_FIELD_COUNT];
	char full_filename[MAX_PATH_SIZE];
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+256];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	struct stat stat_buf;
	int64_t in_bytes;
	int64_t total_send_bytes;
	int64_t start_offset;
	int64_t modify_length;
	int result;
	int count;

	if ((count=splitEx(pRecord->filename, ' ', fields, SYNC_MODIFY_FIELD_COUNT))
			!= SYNC_MODIFY_FIELD_COUNT)
	{
		logError("file: "__FILE__", line: %d, " \
			"the format of binlog not correct, filename: %s", \
			__LINE__, pRecord->filename);
		return EINVAL;
	}

	start_offset = strtoll((fields[1]), NULL, 10);
	modify_length = strtoll((fields[2]), NULL, 10);
	
	pRecord->filename_len = strlen(pRecord->filename);
	pRecord->true_filename_len = pRecord->filename_len;
	if ((result=storage_split_filename_ex(pRecord->filename, \
			&pRecord->true_filename_len, pRecord->true_filename, \
			&pRecord->store_path_index)) != 0)
	{
		return result;
	}

    fc_get_one_subdir_full_filename(
            FDFS_STORE_PATH_STR(pRecord->store_path_index),
            FDFS_STORE_PATH_LEN(pRecord->store_path_index),
            "data", 4, pRecord->true_filename,
            pRecord->true_filename_len, full_filename);
	if (lstat(full_filename, &stat_buf) != 0)
	{
		if (errno == ENOENT)
		{
			logDebug("file: "__FILE__", line: %d, " \
				"sync appender file, file: %s not exists, "\
				"maybe deleted later?", \
				__LINE__, full_filename);

			return 0;
		}
		else
		{
			result = errno != 0 ? errno : EPERM;
			logError("file: "__FILE__", line: %d, " \
				"call stat fail, appender file: %s, "\
				"error no: %d, error info: %s", \
				__LINE__, full_filename, \
				result, STRERROR(result));
			return result;
		}
	}

	if (stat_buf.st_size < start_offset + modify_length)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"appender file: %s 'size: %"PRId64 \
			" < %"PRId64", maybe some mistakes " \
			"happened, skip sync this appender file", __LINE__, \
			full_filename, stat_buf.st_size, \
			start_offset + modify_length);

		return 0;
	}

	total_send_bytes = 0;
	//printf("sync create file: %s\n", pRecord->filename);
	do
	{
		int64_t body_len;

		pHeader = (TrackerHeader *)out_buff;
		memset(pHeader, 0, sizeof(TrackerHeader));

		body_len = 3 * FDFS_PROTO_PKG_LEN_SIZE + \
				4 + FDFS_GROUP_NAME_MAX_LEN + \
				pRecord->filename_len + modify_length;

		long2buff(body_len, pHeader->pkg_len);
		pHeader->cmd = cmd;
		pHeader->status = 0;

		p = out_buff + sizeof(TrackerHeader);

		long2buff(pRecord->filename_len, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		long2buff(start_offset, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		long2buff(modify_length, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		int2buff(pRecord->timestamp, p);
		p += 4;

		strcpy(p, g_group_name);
		p += FDFS_GROUP_NAME_MAX_LEN;
		memcpy(p, pRecord->filename, pRecord->filename_len);
		p += pRecord->filename_len;

		if((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
			p - out_buff, SF_G_NETWORK_TIMEOUT)) != 0)
		{
            format_ip_address(pStorageServer->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"sync data to storage server %s:%u fail, errno: %d, "
				"error info: %s", __LINE__, formatted_ip,
				pStorageServer->port, result, STRERROR(result));
			break;
		}

		if ((result=tcpsendfile_ex(pStorageServer->sock, \
			full_filename, start_offset, modify_length, \
			SF_G_NETWORK_TIMEOUT, &total_send_bytes)) != 0)
		{
            format_ip_address(pStorageServer->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"sync data to storage server %s:%u fail, errno: %d, "
				"error info: %s", __LINE__, formatted_ip,
				pStorageServer->port, result, STRERROR(result));
			break;
		}

		pBuff = in_buff;
		if ((result=fdfs_recv_response(pStorageServer, \
			&pBuff, 0, &in_bytes)) != 0)
		{
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
			break;
		}
	} while (0);

	__sync_add_and_fetch(&g_storage_stat.total_sync_out_bytes,
            total_send_bytes);
	if (result == 0)
    {
        __sync_add_and_fetch(&g_storage_stat.success_sync_out_bytes,
                total_send_bytes);
    }

	return result == EEXIST ? 0 : result;
}

/**
8 bytes: filename bytes
8 bytes: old file size
8 bytes: new file size
4 bytes: source op timestamp
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
filename bytes : filename
**/
static int storage_sync_truncate_file(ConnectionInfo *pStorageServer, \
	StorageBinLogReader *pReader, StorageBinLogRecord *pRecord)
{
#define SYNC_TRUNCATE_FIELD_COUNT  3
	TrackerHeader *pHeader;
	char *p;
	char *pBuff;
	char *fields[SYNC_TRUNCATE_FIELD_COUNT];
	char full_filename[MAX_PATH_SIZE];
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+256];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	struct stat stat_buf;
	int64_t in_bytes;
	int64_t old_file_size;
	int64_t new_file_size;
	int result;
	int count;

	if ((count=splitEx(pRecord->filename, ' ', fields,
                    SYNC_TRUNCATE_FIELD_COUNT)) != SYNC_TRUNCATE_FIELD_COUNT)
	{
		logError("file: "__FILE__", line: %d, " \
			"the format of binlog not correct, filename: %s", \
			__LINE__, pRecord->filename);
		return EINVAL;
	}

	old_file_size = strtoll((fields[1]), NULL, 10);
	new_file_size = strtoll((fields[2]), NULL, 10);
	
	pRecord->filename_len = strlen(pRecord->filename);
	pRecord->true_filename_len = pRecord->filename_len;
	if ((result=storage_split_filename_ex(pRecord->filename, \
			&pRecord->true_filename_len, pRecord->true_filename, \
			&pRecord->store_path_index)) != 0)
	{
		return result;
	}

    fc_get_one_subdir_full_filename(
            FDFS_STORE_PATH_STR(pRecord->store_path_index),
            FDFS_STORE_PATH_LEN(pRecord->store_path_index),
            "data", 4, pRecord->true_filename,
            pRecord->true_filename_len, full_filename);
	if (lstat(full_filename, &stat_buf) != 0)
	{
		if (errno == ENOENT)
		{
			logDebug("file: "__FILE__", line: %d, " \
				"sync appender file, file: %s not exists, "\
				"maybe deleted later?", \
				__LINE__, full_filename);

			return 0;
		}
		else
		{
			result = errno != 0 ? errno : EPERM;
			logError("file: "__FILE__", line: %d, " \
				"call stat fail, appender file: %s, "\
				"error no: %d, error info: %s", \
				__LINE__, full_filename, \
				result, STRERROR(result));
			return result;
		}
	}

	if (stat_buf.st_size != new_file_size)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"appender file: %s 'size: %"PRId64 \
			" != %"PRId64", maybe append/modify later",\
			__LINE__, full_filename, stat_buf.st_size, 
			new_file_size);
	}

	do
	{
		int64_t body_len;

		pHeader = (TrackerHeader *)out_buff;
		memset(pHeader, 0, sizeof(TrackerHeader));

		body_len = 3 * FDFS_PROTO_PKG_LEN_SIZE + \
				4 + FDFS_GROUP_NAME_MAX_LEN + \
				pRecord->filename_len;

		long2buff(body_len, pHeader->pkg_len);
		pHeader->cmd = STORAGE_PROTO_CMD_SYNC_TRUNCATE_FILE;
		pHeader->status = 0;

		p = out_buff + sizeof(TrackerHeader);

		long2buff(pRecord->filename_len, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		long2buff(old_file_size, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		long2buff(new_file_size, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;

		int2buff(pRecord->timestamp, p);
		p += 4;

		strcpy(p, g_group_name);
		p += FDFS_GROUP_NAME_MAX_LEN;
		memcpy(p, pRecord->filename, pRecord->filename_len);
		p += pRecord->filename_len;

		if((result=tcpsenddata_nb(pStorageServer->sock, out_buff,
			p - out_buff, SF_G_NETWORK_TIMEOUT)) != 0)
		{
            format_ip_address(pStorageServer->ip_addr, formatted_ip);
			logError("file: "__FILE__", line: %d, "
				"sync data to storage server %s:%u fail, errno: %d, "
				"error info: %s", __LINE__, formatted_ip,
				pStorageServer->port, result, STRERROR(result));
			break;
		}

		pBuff = in_buff;
		if ((result=fdfs_recv_response(pStorageServer, \
			&pBuff, 0, &in_bytes)) != 0)
		{
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
			break;
		}
	} while (0);

	return result == EEXIST ? 0 : result;
}

/**
send pkg format:
4 bytes: source delete timestamp
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
remain bytes: filename
**/
static int storage_sync_delete_file(ConnectionInfo *pStorageServer, \
			const StorageBinLogRecord *pRecord)
{
	TrackerHeader *pHeader;
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+256];
    char formatted_ip[FORMATTED_IP_SIZE];
	struct stat stat_buf;
	FDFSTrunkFullInfo trunkInfo;
	FDFSTrunkHeader trunkHeader;
	char in_buff[1];
	char *pBuff;
	int64_t in_bytes;
	int result;

	if ((result=trunk_file_stat(pRecord->store_path_index, \
		pRecord->true_filename, pRecord->true_filename_len, \
		&stat_buf, &trunkInfo, &trunkHeader)) == 0)
	{
		if (pRecord->op_type == STORAGE_OP_TYPE_SOURCE_DELETE_FILE)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"sync data file, logic file: %s exists, " \
				"maybe created later?", \
				__LINE__, pRecord->filename);
		}

		return 0;
	}

	memset(out_buff, 0, sizeof(out_buff));
	int2buff(pRecord->timestamp, out_buff + sizeof(TrackerHeader));
	memcpy(out_buff + sizeof(TrackerHeader) + 4, g_group_name, \
		sizeof(g_group_name));
	memcpy(out_buff + sizeof(TrackerHeader) + 4 + FDFS_GROUP_NAME_MAX_LEN, \
		pRecord->filename, pRecord->filename_len);

	pHeader = (TrackerHeader *)out_buff;
	long2buff(4 + FDFS_GROUP_NAME_MAX_LEN + pRecord->filename_len, \
			pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_SYNC_DELETE_FILE;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		sizeof(TrackerHeader) + 4 + FDFS_GROUP_NAME_MAX_LEN + \
		pRecord->filename_len, SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pStorageServer->ip_addr, formatted_ip);
		logError("FILE: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pStorageServer->port, result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	result = fdfs_recv_response(pStorageServer, &pBuff, 0, &in_bytes);
    if (result != 0)
    {
        if (result == ENOENT)
        {
            result = 0;
        }
        else
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
    }
	
	return result;
}

/**
FDFS_STORAGE_ID_MAX_SIZE bytes: my server id
**/
static int storage_report_my_server_id(ConnectionInfo *pStorageServer)
{
	int result;
	TrackerHeader *pHeader;
	char out_buff[sizeof(TrackerHeader) + FDFS_STORAGE_ID_MAX_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	char *pBuff;
	int64_t in_bytes;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	
	long2buff(FDFS_STORAGE_ID_MAX_SIZE, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_REPORT_SERVER_ID;
	strcpy(out_buff + sizeof(TrackerHeader), g_my_server_id_str);
	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		sizeof(TrackerHeader) + FDFS_STORAGE_ID_MAX_SIZE, \
		SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pStorageServer->ip_addr, formatted_ip);
		logError("FILE: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pStorageServer->port, result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	result = fdfs_recv_response(pStorageServer, &pBuff, 0, &in_bytes);
    if (result != 0)
    {
        logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response fail, result: %d",
                __LINE__, result);
    }
    return result;
}

/**
8 bytes: dest(link) filename length
8 bytes: source filename length
4 bytes: source op timestamp
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
dest filename length: dest filename
source filename length: source filename
**/
static int storage_sync_link_file(ConnectionInfo *pStorageServer, \
		StorageBinLogRecord *pRecord)
{
	TrackerHeader *pHeader;
	int result;
	char out_buff[sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE + \
			4 + FDFS_GROUP_NAME_MAX_LEN + 256];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	FDFSTrunkFullInfo trunkInfo;
	FDFSTrunkHeader trunkHeader;
	int out_body_len;
	int64_t in_bytes;
	char *pBuff;
	struct stat stat_buf;
	int fd;

	fd = -1;
	if ((result=trunk_file_lstat_ex(pRecord->store_path_index, \
		pRecord->true_filename, pRecord->true_filename_len, \
		&stat_buf, &trunkInfo, &trunkHeader, &fd)) != 0)
	{
		if (result == ENOENT)
		{
		if (pRecord->op_type == STORAGE_OP_TYPE_SOURCE_CREATE_LINK)
		{
			logDebug("file: "__FILE__", line: %d, " \
				"sync data file, logic file: %s does not " \
				"exist, maybe delete later?", \
				__LINE__, pRecord->filename);
		}
		}
		else
		{
			logError("file: "__FILE__", line: %d, " \
				"call stat fail, logic file: %s, "\
				"error no: %d, error info: %s", \
				__LINE__, pRecord->filename, \
				result, STRERROR(result));
		}

		return 0;
	}

	if (!S_ISLNK(stat_buf.st_mode))
	{
		if (fd >= 0)
		{
			close(fd);
		}

		if (pRecord->op_type == STORAGE_OP_TYPE_SOURCE_CREATE_LINK)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"sync data file, logic file %s is not " \
				"a symbol link, maybe create later?", \
				__LINE__, pRecord->filename);
		}

		return 0;
	}

	if (pRecord->src_filename_len > 0)
	{
		if (fd >= 0)
		{
			close(fd);
		}
	}
	else if (IS_TRUNK_FILE_BY_ID(trunkInfo))
	{
        result = trunk_file_get_content(&trunkInfo,
                stat_buf.st_size, &fd, pRecord->src_filename,
                sizeof(pRecord->src_filename));
        close(fd);

		if (result != 0)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"logic file: %s, get file content fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pRecord->filename, \
				result, STRERROR(result));
			return 0;
		}

		pRecord->src_filename_len = stat_buf.st_size;
		*(pRecord->src_filename + pRecord->src_filename_len) = '\0';
	}
	else
	{
	char full_filename[MAX_PATH_SIZE];
	char src_full_filename[MAX_PATH_SIZE];
	char *p;
	char *pSrcFilename;
    int filename_len;
	int src_path_index;
	int src_filename_len;

    fc_get_one_subdir_full_filename(
            FDFS_STORE_PATH_STR(pRecord->store_path_index),
            FDFS_STORE_PATH_LEN(pRecord->store_path_index),
            "data", 4, pRecord->true_filename,
            pRecord->true_filename_len, full_filename);
	src_filename_len = readlink(full_filename, src_full_filename,
				sizeof(src_full_filename) - 1);
	if (src_filename_len <= 0)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"data file: %s, readlink fail, "\
			"errno: %d, error info: %s", \
			__LINE__, src_full_filename, errno, STRERROR(errno));
		return 0;
	}
	*(src_full_filename + src_filename_len) = '\0';

	pSrcFilename = strstr(src_full_filename, "/data/");
	if (pSrcFilename == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"source data file: %s is invalid", \
			__LINE__, src_full_filename);
		return EINVAL;
	}

	pSrcFilename += 6;
	p = strstr(pSrcFilename, "/data/");
	while (p != NULL)
	{
		pSrcFilename = p + 6;
		p = strstr(pSrcFilename, "/data/");
	}

	if (g_fdfs_store_paths.count == 1)
	{
		src_path_index = 0;
	}
	else
	{
		*(pSrcFilename - 6) = '\0';

		for (src_path_index=0; src_path_index<g_fdfs_store_paths.count;
			src_path_index++)
		{
			if (strcmp(src_full_filename, FDFS_STORE_PATH_STR(
                            src_path_index)) == 0)
			{
				break;
			}
		}

		if (src_path_index == g_fdfs_store_paths.count)
		{
			logError("file: "__FILE__", line: %d, " \
				"source data file: %s is invalid", \
				__LINE__, src_full_filename);
			return EINVAL;
		}
	}

    filename_len = src_filename_len - (pSrcFilename - src_full_filename);
	pRecord->src_filename_len = filename_len + 4;
	if (pRecord->src_filename_len >= sizeof(pRecord->src_filename))
	{
		logError("file: "__FILE__", line: %d, "
			"source data file: %s is invalid",
			__LINE__, src_full_filename);
		return EINVAL;
	}

    p = pRecord->src_filename;
    *p++ = FDFS_STORAGE_STORE_PATH_PREFIX_CHAR;
    *p++ = g_upper_hex_chars[(src_path_index >> 4) & 0x0F];
    *p++ = g_upper_hex_chars[src_path_index & 0x0F];
    *p++ = '/';
    memcpy(p, pSrcFilename, filename_len);
    p += filename_len;
    *p = '\0';
	}

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	long2buff(pRecord->filename_len, out_buff + sizeof(TrackerHeader));
	long2buff(pRecord->src_filename_len, out_buff + sizeof(TrackerHeader) +
			FDFS_PROTO_PKG_LEN_SIZE);
	int2buff(pRecord->timestamp, out_buff + sizeof(TrackerHeader) +
			2 * FDFS_PROTO_PKG_LEN_SIZE);
	strcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE
		 + 4, g_group_name);
	memcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE
		+ 4 + FDFS_GROUP_NAME_MAX_LEN, pRecord->filename,
        pRecord->filename_len);
	memcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE
		+ 4 + FDFS_GROUP_NAME_MAX_LEN + pRecord->filename_len,
		pRecord->src_filename, pRecord->src_filename_len);

	out_body_len = 2 * FDFS_PROTO_PKG_LEN_SIZE + 4 + \
		FDFS_GROUP_NAME_MAX_LEN + pRecord->filename_len + \
		pRecord->src_filename_len;
	long2buff(out_body_len, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_SYNC_CREATE_LINK;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff,
		sizeof(TrackerHeader) + out_body_len,
		SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pStorageServer->ip_addr, formatted_ip);
		logError("FILE: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pStorageServer->port, result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	result = fdfs_recv_response(pStorageServer, &pBuff, 0, &in_bytes);
    if (result != 0)
    {
        if (result == ENOENT)
        {
            result = 0;
        }
        else
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
    }
	
	return result;
}

/**
8 bytes: dest filename length
8 bytes: source filename length
4 bytes: source op timestamp
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
dest filename length: dest filename
source filename length: source filename
**/
static int storage_sync_rename_file(ConnectionInfo *pStorageServer,
		StorageBinLogReader *pReader, StorageBinLogRecord *pRecord)
{
	TrackerHeader *pHeader;
	int result;
	char out_buff[sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE +
			4 + FDFS_GROUP_NAME_MAX_LEN + 256];
    char formatted_ip[FORMATTED_IP_SIZE];
	char in_buff[1];
	int out_body_len;
	int64_t in_bytes;
	char *pBuff;
	char full_filename[MAX_PATH_SIZE];
	struct stat stat_buf;

    if (pRecord->op_type == STORAGE_OP_TYPE_REPLICA_RENAME_FILE)
    {
        return storage_sync_copy_file(pStorageServer, pReader,
                pRecord, STORAGE_PROTO_CMD_SYNC_CREATE_FILE);
    }

    fc_get_one_subdir_full_filename(
            FDFS_STORE_PATH_STR(pRecord->store_path_index),
            FDFS_STORE_PATH_LEN(pRecord->store_path_index),
            "data", 4, pRecord->true_filename,
            pRecord->true_filename_len, full_filename);
	if (lstat(full_filename, &stat_buf) != 0)
	{
		if (errno == ENOENT)
		{
			logWarning("file: "__FILE__", line: %d, "
				"sync file rename, file: %s not exists, "
				"maybe deleted later?", __LINE__, full_filename);

			return 0;
		}
		else
		{
			result = errno != 0 ? errno : EPERM;
			logError("file: "__FILE__", line: %d, "
				"call stat fail, file: %s, "
				"error no: %d, error info: %s",
				__LINE__, full_filename,
				result, STRERROR(result));
			return result;
		}
	}

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	long2buff(pRecord->filename_len, out_buff + sizeof(TrackerHeader));
	long2buff(pRecord->src_filename_len, out_buff + sizeof(TrackerHeader) +
			FDFS_PROTO_PKG_LEN_SIZE);
	int2buff(pRecord->timestamp, out_buff + sizeof(TrackerHeader) +
			2 * FDFS_PROTO_PKG_LEN_SIZE);
	strcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE
		 + 4, g_group_name);
	memcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE
		+ 4 + FDFS_GROUP_NAME_MAX_LEN,
		pRecord->filename, pRecord->filename_len);
	memcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE
		+ 4 + FDFS_GROUP_NAME_MAX_LEN + pRecord->filename_len,
		pRecord->src_filename, pRecord->src_filename_len);

	out_body_len = 2 * FDFS_PROTO_PKG_LEN_SIZE + 4 +
		FDFS_GROUP_NAME_MAX_LEN + pRecord->filename_len +
		pRecord->src_filename_len;
	long2buff(out_body_len, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_SYNC_RENAME_FILE;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff,
		sizeof(TrackerHeader) + out_body_len,
		SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pStorageServer->ip_addr, formatted_ip);
		logError("FILE: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
			pStorageServer->port, result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	result = fdfs_recv_response(pStorageServer, &pBuff, 0, &in_bytes);
    if (result != 0)
    {
        if (result == ENOENT)
        {
            return storage_sync_copy_file(pStorageServer, pReader,
                    pRecord, STORAGE_PROTO_CMD_SYNC_CREATE_FILE);
        }
        else if (result == EEXIST)
        {
            if (FC_LOG_BY_LEVEL(LOG_DEBUG)) {
                format_ip_address(pStorageServer->ip_addr, formatted_ip);
                logDebug("file: "__FILE__", line: %d, "
                        "storage server ip: %s:%u, data file: %s "
                        "already exists", __LINE__, formatted_ip,
                        pStorageServer->port, pRecord->filename);
            }
            return 0;
        }
        else
        {
            logError("file: "__FILE__", line: %d, "
                    "fdfs_recv_response fail, result: %d",
                    __LINE__, result);
        }
    }
	
	return result;
}

static int storage_check_need_sync(StorageBinLogReader *pReader,
			StorageBinLogRecord *pRecord)
{
    switch(pRecord->op_type) {
        case STORAGE_OP_TYPE_SOURCE_CREATE_FILE:
        case STORAGE_OP_TYPE_SOURCE_DELETE_FILE:
        case STORAGE_OP_TYPE_SOURCE_UPDATE_FILE:
        case STORAGE_OP_TYPE_SOURCE_APPEND_FILE:
        case STORAGE_OP_TYPE_SOURCE_MODIFY_FILE:
        case STORAGE_OP_TYPE_SOURCE_TRUNCATE_FILE:
        case STORAGE_OP_TYPE_SOURCE_RENAME_FILE:
        case STORAGE_OP_TYPE_SOURCE_CREATE_LINK:
            return 0;
        case STORAGE_OP_TYPE_REPLICA_CREATE_FILE:
        case STORAGE_OP_TYPE_REPLICA_DELETE_FILE:
        case STORAGE_OP_TYPE_REPLICA_UPDATE_FILE:
        case STORAGE_OP_TYPE_REPLICA_CREATE_LINK:
        case STORAGE_OP_TYPE_REPLICA_RENAME_FILE:
            if ((!pReader->need_sync_old) || pReader->sync_old_done ||
                    (pRecord->timestamp > pReader->until_timestamp))
            {
                return EALREADY;
            } else {
                return 0;
            }
        case STORAGE_OP_TYPE_REPLICA_APPEND_FILE:
        case STORAGE_OP_TYPE_REPLICA_MODIFY_FILE:
        case STORAGE_OP_TYPE_REPLICA_TRUNCATE_FILE:
            return EALREADY;
        default:
            logError("file: "__FILE__", line: %d, " \
                    "invalid file operation type: %d", \
                    __LINE__, pRecord->op_type);
            return EINVAL;
    }
}

static int storage_sync_data(StorageBinLogReader *pReader,
			ConnectionInfo *pStorageServer,
			StorageBinLogRecord *pRecord)
{
	int result;
	switch(pRecord->op_type)
	{
		case STORAGE_OP_TYPE_SOURCE_CREATE_FILE:
			result = storage_sync_copy_file(pStorageServer, pReader,
                    pRecord, STORAGE_PROTO_CMD_SYNC_CREATE_FILE);
			break;
		case STORAGE_OP_TYPE_SOURCE_DELETE_FILE:
			result = storage_sync_delete_file(pStorageServer, pRecord);
			break;
		case STORAGE_OP_TYPE_SOURCE_UPDATE_FILE:
			result = storage_sync_copy_file(pStorageServer, pReader,
                    pRecord, STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			break;
		case STORAGE_OP_TYPE_SOURCE_APPEND_FILE:
			result = storage_sync_modify_file(pStorageServer, pReader,
                    pRecord, STORAGE_PROTO_CMD_SYNC_APPEND_FILE);
			if (result == ENOENT)  //resync appender file
			{
			result = storage_sync_copy_file(pStorageServer, pReader,
                    pRecord, STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			}
			break;
		case STORAGE_OP_TYPE_SOURCE_MODIFY_FILE:
			result = storage_sync_modify_file(pStorageServer, pReader,
                    pRecord, STORAGE_PROTO_CMD_SYNC_MODIFY_FILE);
			if (result == ENOENT)  //resync appender file
			{
			result = storage_sync_copy_file(pStorageServer, pReader,
                    pRecord, STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			}
			break;
		case STORAGE_OP_TYPE_SOURCE_TRUNCATE_FILE:
			result = storage_sync_truncate_file(pStorageServer,
                    pReader, pRecord);
			break;
		case STORAGE_OP_TYPE_SOURCE_RENAME_FILE:
			result = storage_sync_rename_file(pStorageServer,
					pReader, pRecord);
			break;
		case STORAGE_OP_TYPE_SOURCE_CREATE_LINK:
			result = storage_sync_link_file(pStorageServer,
					pRecord);
			break;
		case STORAGE_OP_TYPE_REPLICA_CREATE_FILE:
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_CREATE_FILE);
			break;
		case STORAGE_OP_TYPE_REPLICA_DELETE_FILE:
			result = storage_sync_delete_file( \
					pStorageServer, pRecord);
			break;
		case STORAGE_OP_TYPE_REPLICA_UPDATE_FILE:
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			break;
		case STORAGE_OP_TYPE_REPLICA_CREATE_LINK:
			result = storage_sync_link_file(pStorageServer, \
					pRecord);
			break;
        case STORAGE_OP_TYPE_REPLICA_RENAME_FILE:
			result = storage_sync_rename_file(pStorageServer,
					pReader, pRecord);
			break;
		case STORAGE_OP_TYPE_REPLICA_APPEND_FILE:
			return 0;
		case STORAGE_OP_TYPE_REPLICA_MODIFY_FILE:
			return 0;
		case STORAGE_OP_TYPE_REPLICA_TRUNCATE_FILE:
			return 0;
		default:
			logError("file: "__FILE__", line: %d, "
				"invalid file operation type: %d",
				__LINE__, pRecord->op_type);
			return EINVAL;
	}

	return result;
}

static void sync_data_func(StorageSyncTaskInfo *task, void *thread_data)
{
    //TODO check and make connection

    task->result = storage_sync_data(task->dispatch_ctx->pReader,
            &task->storage_server, &task->record);
    sf_synchronize_counter_notify(&task->dispatch_ctx->notify_ctx, 1);
}

static int storage_batch_sync_data(StorageDispatchContext *dispatch_ctx)
{
    int result;
    int sync_row_count;
    StorageSyncTaskInfo *task;
    StorageSyncTaskInfo *end;

    if (dispatch_ctx->task_array.count == 1) {
        task = dispatch_ctx->task_array.tasks;
        if ((result=storage_sync_data(dispatch_ctx->pReader, &task->
                        storage_server, &task->record)) == 0)
        {
            sync_row_count = 1;
        } else {
            dispatch_ctx->last_binlog_index = task->binlog_index;
            dispatch_ctx->last_binlog_offset = task->binlog_offset;
            dispatch_ctx->scan_row_count = 0;
            sync_row_count = 0;
        }
    } else {
        dispatch_ctx->notify_ctx.waiting_count = dispatch_ctx->task_array.count;
        end = dispatch_ctx->task_array.tasks + dispatch_ctx->task_array.count;
        for (task=dispatch_ctx->task_array.tasks; task<end; task++) {
            if ((result=fc_thread_pool_run(&SYNC_THREAD_POOL,
                            (fc_thread_pool_callback)sync_data_func,
                            task)) != 0)
            {
                logCrit("file: "__FILE__", line: %d, "
                        "fc_thread_pool_run fail, error info: %s, "
                        "program exit!", __LINE__, STRERROR(result));
                SF_G_CONTINUE_FLAG = false;
                return result;
            }
        }

        sf_synchronize_counter_wait(&dispatch_ctx->notify_ctx);
        if (!SF_G_CONTINUE_FLAG) {
            return EINTR;
        }

        sync_row_count = 0;
        result = 0;
        for (task=dispatch_ctx->task_array.tasks; task<end; task++) {
            if (task->result == 0) {
                ++sync_row_count;
            } else {
                result = task->result;
                dispatch_ctx->last_binlog_index = task->binlog_index;
                dispatch_ctx->last_binlog_offset = task->binlog_offset;

                end = task;
                dispatch_ctx->scan_row_count = 0;
                for (task=dispatch_ctx->task_array.tasks; task<end; task++) {
                    dispatch_ctx->scan_row_count += task->scan_row_count;
                }
                break;
            }
        }
    }

    if (sync_row_count > 0) {
        dispatch_ctx->pReader->sync_row_count += sync_row_count;
        if (dispatch_ctx->pReader->sync_row_count - dispatch_ctx->
                pReader->last_sync_rows >= g_write_mark_file_freq)
        {
            if ((result=storage_write_to_mark_file(dispatch_ctx->
                            pReader)) != 0)
            {
                logCrit("file: "__FILE__", line: %d, "
                        "storage_write_to_mark_file "
                        "fail, program exit!", __LINE__);
                SF_G_CONTINUE_FLAG = false;
                return result;
            }
        }
    }

    return result;
}

static int write_to_binlog_index(const int binlog_index)
{
	char full_filename[MAX_PATH_SIZE];
	char buff[256];
    char *p;
	int fd;
	int len;

    fc_get_one_subdir_full_filename(
            SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN,
            SYNC_SUBDIR_NAME_STR, SYNC_SUBDIR_NAME_LEN,
            SYNC_BINLOG_INDEX_FILENAME_STR,
            SYNC_BINLOG_INDEX_FILENAME_LEN,
            full_filename);
	if ((fd=open(full_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
	{
		logError("file: "__FILE__", line: %d, "
			"open file \"%s\" fail, "
			"errno: %d, error info: %s",
			__LINE__, full_filename,
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

    p = buff;
    memcpy(p, BINLOG_INDEX_ITEM_CURRENT_WRITE_STR,
            BINLOG_INDEX_ITEM_CURRENT_WRITE_LEN);
    p += BINLOG_INDEX_ITEM_CURRENT_WRITE_LEN;
    *p++ = '=';
    p += fc_itoa(binlog_index, p);
    *p++ = '\n';

    memcpy(p, BINLOG_INDEX_ITEM_CURRENT_COMPRESS_STR,
            BINLOG_INDEX_ITEM_CURRENT_COMPRESS_LEN);
    p += BINLOG_INDEX_ITEM_CURRENT_COMPRESS_LEN;
    *p++ = '=';
    p += fc_itoa(storage_sync_ctx.binlog_compress_index, p);
    *p++ = '\n';
	len = p - buff;
	if (fc_safe_write(fd, buff, len) != len)
	{
		logError("file: "__FILE__", line: %d, "
			"write to file \"%s\" fail, "
			"errno: %d, error info: %s",
			__LINE__, full_filename,
			errno, STRERROR(errno));
		close(fd);
		return errno != 0 ? errno : EIO;
	}

	close(fd);

	SF_CHOWN_TO_RUNBY_RETURN_ON_ERROR(full_filename);

	return 0;
}

static int get_binlog_index_from_file_old()
{
	char full_filename[MAX_PATH_SIZE];
	char file_buff[64];
	int fd;
	int bytes;

    fc_get_one_subdir_full_filename(
            SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN,
            SYNC_SUBDIR_NAME_STR, SYNC_SUBDIR_NAME_LEN,
            SYNC_BINLOG_INDEX_FILENAME_OLD_STR,
            SYNC_BINLOG_INDEX_FILENAME_OLD_LEN,
            full_filename);
	if ((fd=open(full_filename, O_RDONLY)) >= 0)
	{
		bytes = fc_safe_read(fd, file_buff, sizeof(file_buff) - 1);
		close(fd);
		if (bytes <= 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"read file \"%s\" fail, bytes read: %d", \
				__LINE__, full_filename, bytes);
			return errno != 0 ? errno : EIO;
		}

		file_buff[bytes] = '\0';
		g_binlog_index = atoi(file_buff);
		if (g_binlog_index < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"in file \"%s\", binlog_index: %d < 0", \
				__LINE__, full_filename, g_binlog_index);
			return EINVAL;
		}
	}
	else
	{
		g_binlog_index = 0;
	}

    return 0;
}

static int get_binlog_index_from_file()
{
    char full_filename[MAX_PATH_SIZE];
    IniContext iniContext;
    int result;

    fc_get_one_subdir_full_filename(
            SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN,
            SYNC_SUBDIR_NAME_STR, SYNC_SUBDIR_NAME_LEN,
            SYNC_BINLOG_INDEX_FILENAME_STR,
            SYNC_BINLOG_INDEX_FILENAME_LEN,
            full_filename);
    if (access(full_filename, F_OK) != 0)
    {
        if (errno == ENOENT)
        {
            if ((result=get_binlog_index_from_file_old()) == 0)
            {
                if ((result=write_to_binlog_index(g_binlog_index)) != 0)
                {
                    return result;
                }
            }

            return result;
        }
    }

    memset(&iniContext, 0, sizeof(IniContext));
    if ((result=iniLoadFromFile(full_filename, &iniContext)) != 0)
    {
        logError("file: "__FILE__", line: %d, "
                "load from file \"%s\" fail, "
                "error code: %d",
                __LINE__, full_filename, result);
        return result;
    }

    g_binlog_index = iniGetIntValue(NULL,
            BINLOG_INDEX_ITEM_CURRENT_WRITE_STR,
            &iniContext, 0);
    storage_sync_ctx.binlog_compress_index = iniGetIntValue(NULL,
            BINLOG_INDEX_ITEM_CURRENT_COMPRESS_STR,
            &iniContext, 0);

    iniFreeContext(&iniContext);
    return 0;
}

static char *get_binlog_filename(char *full_filename,
		const int binlog_index)
{
#define SYNC_SUBDIR_AND_FILE_PREFIX_STR       \
    SYNC_SUBDIR_NAME_STR"/"SYNC_BINLOG_FILE_PREFIX_STR
#define SYNC_SUBDIR_AND_FILE_PREFIX_LEN       \
    (sizeof(SYNC_SUBDIR_AND_FILE_PREFIX_STR) - 1)

    char *p;

    if (SF_G_BASE_PATH_LEN + SYNC_SUBDIR_NAME_LEN +
            SYNC_BINLOG_FILE_PREFIX_LEN + 8 > MAX_PATH_SIZE)
    {
        snprintf(full_filename, MAX_PATH_SIZE,
                "%s/"SYNC_SUBDIR_AND_FILE_PREFIX_STR""
                SYNC_BINLOG_FILE_EXT_FMT,
                SF_G_BASE_PATH_STR, binlog_index);
    }
    else
    {
        p = full_filename;
        memcpy(p, SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN);
        p += SF_G_BASE_PATH_LEN;
        *p++ = '/';
        memcpy(p, SYNC_SUBDIR_AND_FILE_PREFIX_STR,
                SYNC_SUBDIR_AND_FILE_PREFIX_LEN);
        p += SYNC_SUBDIR_AND_FILE_PREFIX_LEN;
        *p++ = '.';
        fc_ltostr_ex(binlog_index, p, SYNC_BINLOG_FILE_EXT_LEN);
    }

	return full_filename;
}

static inline char *get_writable_binlog_filename(char *full_filename)
{
    static char buff[MAX_PATH_SIZE];

    if (full_filename == NULL)
    {
        full_filename = buff;
    }

    return get_binlog_filename(full_filename, g_binlog_index);
}

static char *get_binlog_readable_filename_ex(
        const int binlog_index, char *full_filename)
{
	static char buff[MAX_PATH_SIZE];

	if (full_filename == NULL)
	{
		full_filename = buff;
	}

    return get_binlog_filename(full_filename, binlog_index);
}

static inline char *get_binlog_readable_filename(const void *pArg,
		char *full_filename)
{
    return get_binlog_readable_filename_ex(
            ((const StorageBinLogReader *)pArg)->binlog_index,
            full_filename);
}

static int open_next_writable_binlog()
{
	char full_filename[MAX_PATH_SIZE];

	if (g_binlog_fd >= 0)
	{
		close(g_binlog_fd);
		g_binlog_fd = -1;
	}

	get_binlog_filename(full_filename, g_binlog_index + 1);
	if (fileExists(full_filename))
	{
		if (unlink(full_filename) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"unlink file \"%s\" fail, " \
				"errno: %d, error info: %s", \
				__LINE__, full_filename, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}

		logError("file: "__FILE__", line: %d, " \
			"binlog file \"%s\" already exists, truncate", \
			__LINE__, full_filename);
	}

	g_binlog_fd = open(full_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (g_binlog_fd < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EACCES;
	}
	SF_FCHOWN_TO_RUNBY_RETURN_ON_ERROR(g_binlog_fd, full_filename);

	g_binlog_index++;
	return 0;
}

int storage_sync_init()
{
    const int max_idle_time = 300;
	char data_path[MAX_PATH_SIZE];
	char sync_path[MAX_PATH_SIZE];
	char full_filename[MAX_PATH_SIZE];
    int path_len;
    int limit;
	int result;

    path_len = fc_get_full_filepath(SF_G_BASE_PATH_STR,
            SF_G_BASE_PATH_LEN, "data", 4, data_path);
	if (!fileExists(data_path))
	{
		if (mkdir(data_path, 0755) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"mkdir \"%s\" fail, " \
				"errno: %d, error info: %s", \
				__LINE__, data_path, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}

		SF_CHOWN_TO_RUNBY_RETURN_ON_ERROR(data_path);
	}

    fc_get_full_filepath(data_path, path_len,
            SYNC_DIR_NAME_STR, SYNC_DIR_NAME_LEN,
            sync_path);
	if (!fileExists(sync_path))
	{
		if (mkdir(sync_path, 0755) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"mkdir \"%s\" fail, " \
				"errno: %d, error info: %s", \
				__LINE__, sync_path, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}

		SF_CHOWN_TO_RUNBY_RETURN_ON_ERROR(sync_path);
	}

	BINLOG_WRITE_CACHE_BUFF = (char *)malloc(
            SYNC_BINLOG_WRITE_BUFF_SIZE);
	if (BINLOG_WRITE_CACHE_BUFF == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"malloc %d bytes fail, errno: %d, error info: %s",
			__LINE__, SYNC_BINLOG_WRITE_BUFF_SIZE,
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}

    if ((result=get_binlog_index_from_file()) != 0)
    {
        return result;
    }

	get_writable_binlog_filename(full_filename);
	g_binlog_fd = open(full_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (g_binlog_fd < 0)
	{
		logError("file: "__FILE__", line: %d, "
			"open file \"%s\" fail, errno: %d, error info: %s",
			__LINE__, full_filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EACCES;
	}

	storage_sync_ctx.binlog_file_size = lseek(g_binlog_fd, 0, SEEK_END);
	if (storage_sync_ctx.binlog_file_size < 0)
	{
		logError("file: "__FILE__", line: %d, "
			"ftell file \"%s\" fail, errno: %d, error info: %s",
			__LINE__, full_filename, errno, STRERROR(errno));
		storage_sync_destroy();
		return errno != 0 ? errno : EIO;
	}

	SF_FCHOWN_TO_RUNBY_RETURN_ON_ERROR(g_binlog_fd, full_filename);

	/*
	//printf("full_filename=%s, binlog_file_size=%d\n", \
			full_filename, storage_sync_ctx.binlog_file_size);
	*/
	
	if ((result=init_pthread_lock(&SYNC_THREAD_LOCK)) != 0)
	{
		return result;
	}

    limit = g_sync_max_threads * FDFS_MAX_SERVERS_EACH_GROUP;
    if ((result=fc_thread_pool_init(&SYNC_THREAD_POOL, "storage-sync-pool",
                    limit, SF_G_THREAD_STACK_SIZE, max_idle_time,
                    g_sync_min_threads, (bool *)&SF_G_CONTINUE_FLAG)) != 0)
    {
        return result;
    }

	load_local_host_ip_addrs();

    FC_INIT_LIST_HEAD(&SYNC_READER_HEAD);

	return 0;
}

int storage_sync_destroy()
{
	int result;

	if (g_binlog_fd >= 0)
	{
		storage_binlog_fsync(true);
		close(g_binlog_fd);
		g_binlog_fd = -1;
	}

	if (BINLOG_WRITE_CACHE_BUFF != NULL)
	{
		free(BINLOG_WRITE_CACHE_BUFF);
		BINLOG_WRITE_CACHE_BUFF = NULL;
		if ((result=pthread_mutex_destroy(&SYNC_THREAD_LOCK)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"call pthread_mutex_destroy fail, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
			return result;
		}
	}

	return 0;
}

int kill_storage_sync_threads()
{
	int result;
	int kill_res;

	if (STORAGE_SYNC_TIDS == NULL)
	{
		return 0;
	}

	if ((result=pthread_mutex_lock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	kill_res = kill_work_threads(STORAGE_SYNC_TIDS, FC_ATOMIC_GET(
                g_storage_sync_thread_count));

	if ((result=pthread_mutex_unlock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	while (FC_ATOMIC_GET(g_storage_sync_thread_count) > 0)
	{
		usleep(50000);
	}

	return kill_res;
}

int fdfs_binlog_sync_func(void *args)
{
	if (BINLOG_WRITE_CACHE_LEN > 0)
	{
		return storage_binlog_fsync(true);
	}
	else
	{
		return 0;
	}
}

static int storage_binlog_fsync(const bool bNeedLock)
{
	int result;
	int write_ret;

	if (bNeedLock && (result=pthread_mutex_lock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if (BINLOG_WRITE_CACHE_LEN == 0) //ignore
	{
		write_ret = 0;  //skip
	}
	else if (fc_safe_write(g_binlog_fd, BINLOG_WRITE_CACHE_BUFF,
		BINLOG_WRITE_CACHE_LEN) != BINLOG_WRITE_CACHE_LEN)
	{
		logError("file: "__FILE__", line: %d, " \
			"write to binlog file \"%s\" fail, fd=%d, " \
			"errno: %d, error info: %s",  \
			__LINE__, get_writable_binlog_filename(NULL), \
			g_binlog_fd, errno, STRERROR(errno));
		write_ret = errno != 0 ? errno : EIO;
	}
	else if (fsync(g_binlog_fd) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"sync to binlog file \"%s\" fail, " \
			"errno: %d, error info: %s",  \
			__LINE__, get_writable_binlog_filename(NULL), \
			errno, STRERROR(errno));
		write_ret = errno != 0 ? errno : EIO;
	}
	else
	{
		storage_sync_ctx.binlog_file_size += BINLOG_WRITE_CACHE_LEN;
		if (storage_sync_ctx.binlog_file_size >= SYNC_BINLOG_FILE_MAX_SIZE)
		{
			if ((write_ret=write_to_binlog_index( \
				g_binlog_index + 1)) == 0)
			{
				write_ret = open_next_writable_binlog();
			}

			storage_sync_ctx.binlog_file_size = 0;
			if (write_ret != 0)
			{
				SF_G_CONTINUE_FLAG = false;
				logCrit("file: "__FILE__", line: %d, " \
					"open binlog file \"%s\" fail, " \
					"program exit!", \
					__LINE__, \
					get_writable_binlog_filename(NULL));
			}
		}
		else
		{
			write_ret = 0;
		}
	}

	BINLOG_WRITE_VERSION++;
	BINLOG_WRITE_CACHE_LEN = 0;  //reset cache buff

	if (bNeedLock && (result=pthread_mutex_unlock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return write_ret;
}

int storage_binlog_write_ex(const time_t timestamp, const char op_type,
		const char *filename_str, const int filename_len,
        const char *extra_str, const int extra_len)
{
	int result;
	int write_ret;
    char *p;

	if ((result=pthread_mutex_lock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

    p = BINLOG_WRITE_CACHE_BUFF + BINLOG_WRITE_CACHE_LEN;
    p += fc_itoa(timestamp, p);
    *p++ = ' ';
    *p++ = op_type;
    *p++ = ' ';
    memcpy(p, filename_str, filename_len);
    p += filename_len;
	if (extra_str != NULL)
    {
        *p++ = ' ';
        memcpy(p, extra_str, extra_len);
        p += extra_len;
    }
    *p++ = '\n';
    BINLOG_WRITE_CACHE_LEN = p - BINLOG_WRITE_CACHE_BUFF;

	//check if buff full
	if (SYNC_BINLOG_WRITE_BUFF_SIZE - BINLOG_WRITE_CACHE_LEN < 256)
	{
		write_ret = storage_binlog_fsync(false);  //sync to disk
	}
	else
	{
		write_ret = 0;
	}

	if ((result=pthread_mutex_unlock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return write_ret;
}

static void get_binlog_flag_file(const char *filepath,
        char *flag_filename, const int size)
{
    const char *filename;
    char *p;
    int path_len;
    int base_len;
    int file_len;

    path_len = strlen(filepath);
    filename = strrchr(filepath, '/');
    if (path_len + 6 >= size)
    {
        if (filename == NULL)
        {
            snprintf(flag_filename, size, ".%s.flag", filepath);
        }
        else
        {
            snprintf(flag_filename, size, "%.*s.%s.flag",
                    (int)(filename - filepath + 1), filepath, filename + 1);
        }
    }
    else
    {
        p = flag_filename;
        if (filename == NULL)
        {
            *p++ = '.';
            memcpy(p, filepath, path_len);
            p += path_len;
        }
        else
        {
            base_len = (filename + 1) - filepath;
            file_len = path_len - base_len;
            memcpy(p, filepath, base_len);
            p += base_len;
            *p++ = '.';
            memcpy(p, filename + 1, file_len);
            p += file_len;
        }
        *p++ = '.';
        *p++ = 'f';
        *p++ = 'l';
        *p++ = 'a';
        *p++ = 'g';
        *p = '\0';
    }
}

static int uncompress_binlog_file(StorageBinLogReader *pReader,
        const char *filename)
{
	char gzip_filename[MAX_PATH_SIZE];
	char flag_filename[MAX_PATH_SIZE];
	char command[MAX_PATH_SIZE];
    char output[256];
    struct stat flag_stat;
    int result;

    fc_combine_two_strings(filename, "gz", '.', gzip_filename);
    if (access(gzip_filename, F_OK) != 0)
    {
        return errno != 0 ? errno : ENOENT;
    }

    get_binlog_flag_file(filename, flag_filename, sizeof(flag_filename));
    if (stat(flag_filename, &flag_stat) == 0)
    {
        if (g_current_time - flag_stat.st_mtime > 3600)
        {
            logInfo("file: "__FILE__", line: %d, "
                    "flag file %s expired, continue to uncompress",
                    __LINE__, flag_filename);
        }
        else
        {
            logWarning("file: "__FILE__", line: %d, "
                    "uncompress %s already in progress",
                    __LINE__, gzip_filename);
            return EINPROGRESS;
        }
    }

    if ((result=writeToFile(flag_filename, "unzip", 5)) != 0)
    {
        return result;
    }

    logInfo("file: "__FILE__", line: %d, "
            "try to uncompress binlog %s",
            __LINE__, gzip_filename);
    snprintf(command, sizeof(command), "%s -d %s 2>&1",
            get_gzip_command_filename(), gzip_filename);
    result = getExecResult(command, output, sizeof(output));
    unlink(flag_filename);
    if (result != 0)
    {
		logError("file: "__FILE__", line: %d, "
                "exec command \"%s\" fail, "
                "errno: %d, error info: %s",
                __LINE__, command, result, STRERROR(result));
        return result;
    }
    if (*output != '\0')
    {
        logWarning("file: "__FILE__", line: %d, "
                "exec command \"%s\", output: %s",
                __LINE__, command, output);
    }

    if (access(filename, F_OK) == 0)
    {
        if (pReader->binlog_index < storage_sync_ctx.binlog_compress_index)
        {
            storage_sync_ctx.binlog_compress_index = pReader->binlog_index;
            write_to_binlog_index(g_binlog_index);
        }
    }

    logInfo("file: "__FILE__", line: %d, "
            "uncompress binlog %s done",
            __LINE__, gzip_filename);
    return 0;
}

static int compress_binlog_file(const char *filename)
{
	char gzip_filename[MAX_PATH_SIZE];
	char flag_filename[MAX_PATH_SIZE];
	char command[MAX_PATH_SIZE];
    char output[256];
    struct stat flag_stat;
    int result;

    fc_combine_two_strings(filename, "gz", '.', gzip_filename);
    if (access(gzip_filename, F_OK) == 0)
    {
        return 0;
    }

    if (access(filename, F_OK) != 0)
    {
        return errno != 0 ? errno : ENOENT;
    }

    get_binlog_flag_file(filename, flag_filename, sizeof(flag_filename));
    if (stat(flag_filename, &flag_stat) == 0)
    {
        if (g_current_time - flag_stat.st_mtime > 3600)
        {
            logInfo("file: "__FILE__", line: %d, "
                    "flag file %s expired, continue to compress",
                    __LINE__, flag_filename);
        }
        else
        {
            logWarning("file: "__FILE__", line: %d, "
                    "compress %s already in progress",
                    __LINE__, filename);
            return EINPROGRESS;
        }
    }

    if ((result=writeToFile(flag_filename, "zip", 3)) != 0)
    {
        return result;
    }

    logInfo("file: "__FILE__", line: %d, "
            "try to compress binlog %s",
            __LINE__, filename);

    snprintf(command, sizeof(command), "%s %s 2>&1",
            get_gzip_command_filename(), filename);
    result = getExecResult(command, output, sizeof(output));
    unlink(flag_filename);
    if (result != 0)
    {
		logError("file: "__FILE__", line: %d, "
                "exec command \"%s\" fail, "
                "errno: %d, error info: %s",
                __LINE__, command, result, STRERROR(result));
        return result;
    }
    if (*output != '\0')
    {
        logWarning("file: "__FILE__", line: %d, "
                "exec command \"%s\", output: %s",
                __LINE__, command, output);
    }

    logInfo("file: "__FILE__", line: %d, "
            "compress binlog %s done",
            __LINE__, filename);
    return 0;
}

int storage_open_readable_binlog(StorageBinLogReader *pReader, \
		get_filename_func filename_func, const void *pArg)
{
	char full_filename[MAX_PATH_SIZE];

	if (pReader->binlog_fd >= 0)
	{
		close(pReader->binlog_fd);
	}

	filename_func(pArg, full_filename);
    if (access(full_filename, F_OK) != 0)
    {
        if (errno == ENOENT)
        {
            uncompress_binlog_file(pReader, full_filename);
        }
    }

	pReader->binlog_fd = open(full_filename, O_RDONLY);
	if (pReader->binlog_fd < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open binlog file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	if (pReader->binlog_offset > 0 && \
	    lseek(pReader->binlog_fd, pReader->binlog_offset, SEEK_SET) < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"seek binlog file \"%s\" fail, file offset=" \
			"%"PRId64", errno: %d, error info: %s", \
			__LINE__, full_filename, pReader->binlog_offset, \
			errno, STRERROR(errno));

		close(pReader->binlog_fd);
		pReader->binlog_fd = -1;
		return errno != 0 ? errno : ESPIPE;
	}

	return 0;
}

static char *get_mark_filename_by_ip_and_port(const char *ip_addr,
		const int port, char *full_filename, const int filename_size)
{
    int ip_len;
    char *p;

    ip_len = strlen(ip_addr);
    if (SF_G_BASE_PATH_LEN + SYNC_SUBDIR_NAME_LEN + ip_len +
            SYNC_MARK_FILE_EXT_LEN + 10 >= filename_size)
    {
        snprintf(full_filename, filename_size,
                "%s/"SYNC_SUBDIR_NAME_STR"/%s_%d%s",
                SF_G_BASE_PATH_STR, ip_addr, port,
                SYNC_MARK_FILE_EXT_STR);
    }
    else
    {
        p = full_filename;
        memcpy(p, SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN);
        p += SF_G_BASE_PATH_LEN;
        *p++ = '/';
        memcpy(p, SYNC_SUBDIR_NAME_STR, SYNC_SUBDIR_NAME_LEN);
        p += SYNC_SUBDIR_NAME_LEN;
        *p++ = '/';
        memcpy(p, ip_addr, ip_len);
        p += ip_len;
        *p++ = '_';
        p += fc_itoa(port, p);
        memcpy(p, SYNC_MARK_FILE_EXT_STR, SYNC_MARK_FILE_EXT_LEN);
        p += SYNC_MARK_FILE_EXT_LEN;
        *p = '\0';
    }
    return full_filename;
}

static char *get_mark_filename_by_id_and_port(const char *storage_id,
		const int port, char *full_filename, const int filename_size)
{
    int id_len;
    char *p;

    if (g_use_storage_id)
    {
        id_len = strlen(storage_id);
        if (SF_G_BASE_PATH_LEN + SYNC_SUBDIR_NAME_LEN + id_len +
                SYNC_MARK_FILE_EXT_LEN + 2 >= filename_size)
        {
            snprintf(full_filename, filename_size,
                    "%s/"SYNC_SUBDIR_NAME_STR"/%s%s",
                    SF_G_BASE_PATH_STR, storage_id,
                    SYNC_MARK_FILE_EXT_STR);
        }
        else
        {
            p = full_filename;
            memcpy(p, SF_G_BASE_PATH_STR, SF_G_BASE_PATH_LEN);
            p += SF_G_BASE_PATH_LEN;
            *p++ = '/';
            memcpy(p, SYNC_SUBDIR_NAME_STR, SYNC_SUBDIR_NAME_LEN);
            p += SYNC_SUBDIR_NAME_LEN;
            *p++ = '/';
            memcpy(p, storage_id, id_len);
            p += id_len;
            memcpy(p, SYNC_MARK_FILE_EXT_STR, SYNC_MARK_FILE_EXT_LEN);
            p += SYNC_MARK_FILE_EXT_LEN;
            *p = '\0';
        }

        return full_filename;
    }
    else
    {
        return get_mark_filename_by_ip_and_port(storage_id,
                port, full_filename, filename_size);
    }
}

char *get_mark_filename_by_reader(StorageBinLogReader *pReader)
{
	return get_mark_filename_by_id_and_port(pReader->storage_id,
			SF_G_INNER_PORT, pReader->mark_filename,
            sizeof(pReader->mark_filename));
}

static char *get_mark_filename_by_id(const char *storage_id,
		char *full_filename, const int filename_size)
{
	return get_mark_filename_by_id_and_port(storage_id,
            SF_G_INNER_PORT, full_filename, filename_size);
}

int storage_report_storage_status(const char *storage_id, \
		const char *ip_addr, const char status)
{
	FDFSStorageBrief briefServer;
	TrackerServerInfo trackerServer;
	TrackerServerInfo *pGlobalServer;
	TrackerServerInfo *pTServer;
	TrackerServerInfo *pTServerEnd;
    char formatted_ip[FORMATTED_IP_SIZE];
    ConnectionInfo *conn;
	int result;
	int report_count;
	int success_count;
	int i;

	memset(&briefServer, 0, sizeof(FDFSStorageBrief));
	strcpy(briefServer.id, storage_id);
	strcpy(briefServer.ip_addr, ip_addr);
	briefServer.status = status;

	logDebug("file: "__FILE__", line: %d, " \
		"begin to report storage %s 's status as: %d", \
		__LINE__, ip_addr, status);

	if (!g_sync_old_done)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"report storage %s 's status as: %d, " \
			"waiting for g_sync_old_done turn to true...", \
			__LINE__, ip_addr, status);

		while (SF_G_CONTINUE_FLAG && !g_sync_old_done)
		{
			sleep(1);
		}

		if (!SF_G_CONTINUE_FLAG)
		{
			return 0;
		}

		logDebug("file: "__FILE__", line: %d, " \
			"report storage %s 's status as: %d, " \
			"ok, g_sync_old_done turn to true", \
			__LINE__, ip_addr, status);
	}

    conn = NULL;
	report_count = 0;
	success_count = 0;

	result = 0;
	pTServer = &trackerServer;
	pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;
	for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(TrackerServerInfo));
        fdfs_server_sock_reset(pTServer);
		for (i=0; i < 3; i++)
		{
            conn = tracker_connect_server_no_pool_ex(pTServer,
                    (g_client_bind_addr ? SF_G_INNER_BIND_ADDR4 : NULL),
                    (g_client_bind_addr ? SF_G_INNER_BIND_ADDR6 : NULL),
                    &result, false);
            if (conn != NULL)
            {
				break;
            }

			sleep(5);
		}

        if (conn == NULL)
        {
            format_ip_address(pTServer->connections[0].
                    ip_addr, formatted_ip);
            logError("file: "__FILE__", line: %d, "
                    "connect to tracker server %s:%u fail, errno: %d, "
                    "error info: %s", __LINE__, formatted_ip, pTServer->
                    connections[0].port, result, STRERROR(result));
            continue;
        }

		report_count++;
		if ((result=tracker_report_storage_status(conn,
			&briefServer)) == 0)
		{
			success_count++;
		}

		fdfs_quit(conn);
		close(conn->sock);
	}

	logDebug("file: "__FILE__", line: %d, " \
		"report storage %s 's status as: %d done, " \
		"report count: %d, success count: %d", \
		__LINE__, ip_addr, status, report_count, success_count);

	return success_count > 0 ? 0 : EAGAIN;
}

static int storage_reader_sync_init_req(StorageBinLogReader *pReader)
{
	TrackerServerInfo *pTrackerServers;
	TrackerServerInfo *pTServer;
	TrackerServerInfo *pTServerEnd;
    ConnectionInfo *conn;
	char tracker_client_ip[IP_ADDRESS_SIZE];
	int result;

	if (!g_sync_old_done)
	{
		while (SF_G_CONTINUE_FLAG && !g_sync_old_done)
		{
			sleep(1);
		}

		if (!SF_G_CONTINUE_FLAG)
		{
			return EINTR;
		}
	}

	pTrackerServers = (TrackerServerInfo *)malloc(
		sizeof(TrackerServerInfo) * g_tracker_group.server_count);
	if (pTrackerServers == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"malloc %d bytes fail", __LINE__,
			(int)sizeof(TrackerServerInfo) *
			g_tracker_group.server_count);
		return errno != 0 ? errno : ENOMEM;
	}

	memcpy(pTrackerServers, g_tracker_group.servers,
		sizeof(TrackerServerInfo) * g_tracker_group.server_count);
	pTServerEnd = pTrackerServers + g_tracker_group.server_count;
	for (pTServer=pTrackerServers; pTServer<pTServerEnd; pTServer++)
	{
        fdfs_server_sock_reset(pTServer);
	}

	result = EINTR;
	if (g_tracker_group.leader_index >= 0 &&
		g_tracker_group.leader_index < g_tracker_group.server_count)
	{
		pTServer = pTrackerServers + g_tracker_group.leader_index;
	}
	else
	{
		pTServer = pTrackerServers;
	}
	do
	{
        conn = NULL;
		while (SF_G_CONTINUE_FLAG)
		{
            conn = tracker_connect_server_no_pool_ex(pTServer,
                    (g_client_bind_addr ? SF_G_INNER_BIND_ADDR4 : NULL),
                    (g_client_bind_addr ? SF_G_INNER_BIND_ADDR6 : NULL),
                    &result, true);
            if (conn != NULL)
            {
				break;
            }

			pTServer++;
			if (pTServer >= pTServerEnd)
			{
				pTServer = pTrackerServers;
			}

			sleep(g_heart_beat_interval);
		}

		if (!SF_G_CONTINUE_FLAG)
		{
			break;
		}

		getSockIpaddr(conn->sock, tracker_client_ip, IP_ADDRESS_SIZE);
		insert_into_local_host_ip(tracker_client_ip);

		if ((result=tracker_sync_src_req(conn, pReader)) != 0)
		{
			fdfs_quit(conn);
			close(conn->sock);
			sleep(g_heart_beat_interval);
			continue;
		}

		fdfs_quit(conn);
		close(conn->sock);

		break;
	} while (1);

	free(pTrackerServers);

	/*
	//printf("need_sync_old=%d, until_timestamp=%d\n", \
		pReader->need_sync_old, pReader->until_timestamp);
	*/

	return result;
}

int storage_reader_init(FDFSStorageBrief *pStorage, StorageBinLogReader *pReader)
{
	IniContext iniContext;
	int result;
	bool bFileExist;
	bool bNeedSyncOld;

    memset(pReader, 0, sizeof(StorageBinLogReader));
    pReader->binlog_fd = -1;

	pReader->binlog_buff.buffer = (char *)malloc(
				STORAGE_BINLOG_BUFFER_SIZE);
	if (pReader->binlog_buff.buffer == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"malloc %d bytes fail, "
			"errno: %d, error info: %s",
			__LINE__, STORAGE_BINLOG_BUFFER_SIZE,
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	pReader->binlog_buff.current = pReader->binlog_buff.buffer;

	if (pStorage == NULL)
	{
		strcpy(pReader->storage_id, "0.0.0.0");
	}
	else
	{
		strcpy(pReader->storage_id, pStorage->id);
	}
	get_mark_filename_by_reader(pReader);

	if (pStorage == NULL)
	{
		bFileExist = false;
	}
	else if (pStorage->status <= FDFS_STORAGE_STATUS_WAIT_SYNC)
	{
		bFileExist = false;
	}
	else
	{
		bFileExist = fileExists(pReader->mark_filename);
		if (!bFileExist && (g_use_storage_id && pStorage != NULL))
		{
			char old_mark_filename[MAX_PATH_SIZE];
			get_mark_filename_by_ip_and_port(pStorage->ip_addr,
				SF_G_INNER_PORT, old_mark_filename,
				sizeof(old_mark_filename));
			if (fileExists(old_mark_filename))
			{
				if (rename(old_mark_filename,
                            pReader->mark_filename) !=0 )
				{
					logError("file: "__FILE__", line: %d, "
						"rename file %s to %s fail"
						", errno: %d, error info: %s",
						__LINE__, old_mark_filename,
						pReader->mark_filename, errno,
						STRERROR(errno));
					return errno != 0 ? errno : EACCES;
				}
				bFileExist = true;
			}
		}
	}

	if (pStorage != NULL && !bFileExist)
	{
		if ((result=storage_reader_sync_init_req(pReader)) != 0)
		{
			return result;
		}
	}

	if (bFileExist)
	{
		memset(&iniContext, 0, sizeof(IniContext));
		if ((result=iniLoadFromFile(pReader->mark_filename,
                        &iniContext)) != 0)
		{
			logError("file: "__FILE__", line: %d, "
				"load from mark file \"%s\" fail, "
				"error code: %d", __LINE__,
                pReader->mark_filename, result);
			return result;
		}

		if (iniContext.global.count < 7)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, "
				"in mark file \"%s\", item count: %d < 7",
				__LINE__, pReader->mark_filename,
                iniContext.global.count);
			return ENOENT;
		}

		bNeedSyncOld = iniGetBoolValue(NULL,
				MARK_ITEM_NEED_SYNC_OLD_STR,
				&iniContext, false);
		if (pStorage != NULL && pStorage->status ==
			FDFS_STORAGE_STATUS_SYNCING)
		{
			if ((result=storage_reader_sync_init_req(pReader)) != 0)
			{
				iniFreeContext(&iniContext);
				return result;
			}

			if (pReader->need_sync_old && !bNeedSyncOld)
			{
				bFileExist = false;  //re-sync
			}
			else
			{
				pReader->need_sync_old = bNeedSyncOld;
			}
		}
		else
		{
			pReader->need_sync_old = bNeedSyncOld;
		}

		if (bFileExist)
		{
			pReader->binlog_index = iniGetIntValue(NULL, \
					MARK_ITEM_BINLOG_FILE_INDEX_STR, \
					&iniContext, -1);
			pReader->binlog_offset = iniGetInt64Value(NULL, \
					MARK_ITEM_BINLOG_FILE_OFFSET_STR, \
					&iniContext, -1);
			pReader->sync_old_done = iniGetBoolValue(NULL,  \
					MARK_ITEM_SYNC_OLD_DONE_STR, \
					&iniContext, false);
			pReader->until_timestamp = iniGetIntValue(NULL, \
					MARK_ITEM_UNTIL_TIMESTAMP_STR, \
					&iniContext, -1);
			pReader->scan_row_count = iniGetInt64Value(NULL, \
					MARK_ITEM_SCAN_ROW_COUNT_STR, \
					&iniContext, 0);
			pReader->sync_row_count = iniGetInt64Value(NULL, \
					MARK_ITEM_SYNC_ROW_COUNT_STR, \
					&iniContext, 0);

			if (pReader->binlog_index < 0)
			{
				iniFreeContext(&iniContext);
				logError("file: "__FILE__", line: %d, " \
					"in mark file \"%s\", " \
					"binlog_index: %d < 0", \
					__LINE__, pReader->mark_filename, \
					pReader->binlog_index);
				return EINVAL;
			}
			if (pReader->binlog_offset < 0)
			{
				iniFreeContext(&iniContext);
				logError("file: "__FILE__", line: %d, "
					"in mark file \"%s\", binlog_offset: "
					"%"PRId64" < 0", __LINE__,
                    pReader->mark_filename,
					pReader->binlog_offset);
				return EINVAL;
			}
		}

		iniFreeContext(&iniContext);
	}

	pReader->last_scan_rows = pReader->scan_row_count;
	pReader->last_sync_rows = pReader->sync_row_count;

	if ((result=storage_open_readable_binlog(pReader,
			get_binlog_readable_filename, pReader)) != 0)
	{
		return result;
	}

	if (pStorage != NULL && !bFileExist)
	{
        	if (!pReader->need_sync_old && pReader->until_timestamp > 0)
		{
			if ((result=storage_binlog_reader_skip(pReader)) != 0)
			{
				return result;
			}
		}

		if ((result=storage_write_to_mark_file(pReader)) != 0)
		{
			return result;
		}
	}

	result = storage_binlog_preread(pReader);
	if (result != 0 && result != ENOENT)
	{
		return result;
	}

	return 0;
}

void storage_reader_destroy(StorageBinLogReader *pReader)
{
	if (pReader->binlog_fd >= 0)
	{
		close(pReader->binlog_fd);
		pReader->binlog_fd = -1;
	}

	if (pReader->binlog_buff.buffer != NULL)
	{
		free(pReader->binlog_buff.buffer);
		pReader->binlog_buff.buffer = NULL;
		pReader->binlog_buff.current = NULL;
		pReader->binlog_buff.length = 0;
	}
}

static int storage_write_to_mark_file(StorageBinLogReader *pReader)
{
	char buff[256];
    char *p;
	int result;

    p = buff;
    memcpy(p, MARK_ITEM_BINLOG_FILE_INDEX_STR,
            MARK_ITEM_BINLOG_FILE_INDEX_LEN);
    p += MARK_ITEM_BINLOG_FILE_INDEX_LEN;
    *p++ = '=';
    p += fc_itoa(pReader->binlog_index, p);
    *p++ = '\n';

    memcpy(p, MARK_ITEM_BINLOG_FILE_OFFSET_STR,
            MARK_ITEM_BINLOG_FILE_OFFSET_LEN);
    p += MARK_ITEM_BINLOG_FILE_OFFSET_LEN;
    *p++ = '=';
    p += fc_itoa(pReader->binlog_offset, p);
    *p++ = '\n';

    memcpy(p, MARK_ITEM_NEED_SYNC_OLD_STR,
            MARK_ITEM_NEED_SYNC_OLD_LEN);
    p += MARK_ITEM_NEED_SYNC_OLD_LEN;
    *p++ = '=';
    if (pReader->need_sync_old == 0)
    {
        *p++ = '0';
    }
    else if (pReader->need_sync_old == 1)
    {
        *p++ = '1';
    }
    else
    {
        p += fc_itoa(pReader->need_sync_old, p);
    }
    *p++ = '\n';

    memcpy(p, MARK_ITEM_SYNC_OLD_DONE_STR,
            MARK_ITEM_SYNC_OLD_DONE_LEN);
    p += MARK_ITEM_SYNC_OLD_DONE_LEN;
    *p++ = '=';
    if (pReader->sync_old_done == 0)
    {
        *p++ = '0';
    }
    else if (pReader->sync_old_done == 1)
    {
        *p++ = '1';
    }
    else
    {
        p += fc_itoa(pReader->sync_old_done, p);
    }
    *p++ = '\n';

    memcpy(p, MARK_ITEM_UNTIL_TIMESTAMP_STR,
            MARK_ITEM_UNTIL_TIMESTAMP_LEN);
    p += MARK_ITEM_UNTIL_TIMESTAMP_LEN;
    *p++ = '=';
    p += fc_itoa(pReader->until_timestamp, p);
    *p++ = '\n';

    memcpy(p, MARK_ITEM_SCAN_ROW_COUNT_STR,
            MARK_ITEM_SCAN_ROW_COUNT_LEN);
    p += MARK_ITEM_SCAN_ROW_COUNT_LEN;
    *p++ = '=';
    p += fc_itoa(pReader->scan_row_count, p);
    *p++ = '\n';

    memcpy(p, MARK_ITEM_SYNC_ROW_COUNT_STR,
            MARK_ITEM_SYNC_ROW_COUNT_LEN);
    p += MARK_ITEM_SYNC_ROW_COUNT_LEN;
    *p++ = '=';
    p += fc_itoa(pReader->sync_row_count, p);
    *p++ = '\n';

    if ((result=safeWriteToFile(pReader->mark_filename, buff, p - buff)) == 0)
	{
        SF_CHOWN_TO_RUNBY_RETURN_ON_ERROR(pReader->mark_filename);
		pReader->last_scan_rows = pReader->scan_row_count;
		pReader->last_sync_rows = pReader->sync_row_count;
	}

	return result;
}

static int rewind_to_prev_rec_end_ex(StorageBinLogReader *pReader,
        const int64_t binlog_offset)
{
    if (lseek(pReader->binlog_fd, binlog_offset, SEEK_SET) < 0) {
        logError("file: "__FILE__", line: %d, "
                "seek binlog file \"%s\"fail, file offset: %"PRId64", "
                "errno: %d, error info: %s", __LINE__,
                get_binlog_readable_filename(pReader, NULL),
                binlog_offset, errno, STRERROR(errno));
        return errno != 0 ? errno : ENOENT;
    }

    pReader->binlog_buff.current = pReader->binlog_buff.buffer;
    pReader->binlog_buff.length = 0;
    return 0;
}

static inline int rewind_to_prev_rec_end(StorageBinLogReader *pReader)
{
    return rewind_to_prev_rec_end_ex(pReader, pReader->binlog_offset);
}

static int storage_binlog_preread(StorageBinLogReader *pReader)
{
	int bytes_read;
	int saved_binlog_write_version;

	if (pReader->binlog_buff.version == BINLOG_WRITE_VERSION &&
		pReader->binlog_buff.length == 0)
	{
		return ENOENT;
	}

	saved_binlog_write_version = BINLOG_WRITE_VERSION;
	if (pReader->binlog_buff.current != pReader->binlog_buff.buffer)
	{
		if (pReader->binlog_buff.length > 0)
		{
			memcpy(pReader->binlog_buff.buffer,
				pReader->binlog_buff.current,
				pReader->binlog_buff.length);
		}

		pReader->binlog_buff.current = pReader->binlog_buff.buffer;
	}

    bytes_read = fc_safe_read(pReader->binlog_fd,
            pReader->binlog_buff.buffer + pReader->binlog_buff.length,
            STORAGE_BINLOG_BUFFER_SIZE - pReader->binlog_buff.length);
    if (bytes_read < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"read from binlog file \"%s\" fail, " \
			"file offset: %"PRId64", " \
			"error no: %d, error info: %s", __LINE__, \
			get_binlog_readable_filename(pReader, NULL), \
			pReader->binlog_offset + pReader->binlog_buff.length, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EIO;
	}
	else if (bytes_read == 0) //end of binlog file
	{
		pReader->binlog_buff.version = saved_binlog_write_version;
		return ENOENT;
	}

	pReader->binlog_buff.length += bytes_read;
	return 0;
}

static int storage_binlog_do_line_read(StorageBinLogReader *pReader,
		char *line, const int line_size, int *line_length)
{
	char *pLineEnd;

	if (pReader->binlog_buff.length == 0)
	{
		*line_length = 0;
		return ENOENT;
	}

	pLineEnd = (char *)memchr(pReader->binlog_buff.current, '\n',
			pReader->binlog_buff.length);
	if (pLineEnd == NULL)
	{
		*line_length = 0;
		return ENOENT;
	}

	*line_length = (pLineEnd - pReader->binlog_buff.current) + 1;
	if (*line_length >= line_size)
	{
		logError("file: "__FILE__", line: %d, " \
			"read from binlog file \"%s\" fail, " \
			"file offset: %"PRId64", " \
			"line buffer size: %d is too small! " \
			"<= line length: %d", __LINE__, \
			get_binlog_readable_filename(pReader, NULL), \
			pReader->binlog_offset, line_size, *line_length);
		return ENOSPC;
	}

	memcpy(line, pReader->binlog_buff.current, *line_length);
	*(line + *line_length) = '\0';

	pReader->binlog_buff.current = pLineEnd + 1;
	pReader->binlog_buff.length -= *line_length;
	return 0;
}

static int storage_binlog_read_line(StorageBinLogReader *pReader, \
		char *line, const int line_size, int *line_length)
{
	int result;

	result = storage_binlog_do_line_read(pReader, line,
			line_size, line_length);
	if (result != ENOENT)
	{
		return result;
	}

	result = storage_binlog_preread(pReader);
	if (result != 0)
	{
		return result;
	}

	return storage_binlog_do_line_read(pReader, line,
			line_size, line_length);
}

int storage_binlog_read(StorageBinLogReader *pReader,
        StorageBinLogRecord *pRecord, int *record_length)
{
	char line[STORAGE_BINLOG_LINE_SIZE];
	char *cols[3];
	int result;

	while (1)
	{
		result = storage_binlog_read_line(pReader, line,
				sizeof(line), record_length);
		if (result == 0)
		{
			break;
		}
		else if (result != ENOENT)
		{
			return result;
		}

		if (pReader->binlog_index >= g_binlog_index)
		{
			return ENOENT;
		}

		if (pReader->binlog_buff.length != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"binlog file \"%s\" not ended by \\n, " \
				"file offset: %"PRId64, __LINE__, \
				get_binlog_readable_filename(pReader, NULL), \
				pReader->binlog_offset);
			return ENOENT;
		}

		//rotate
		pReader->binlog_index++;
		pReader->binlog_offset = 0;
		pReader->binlog_buff.version = 0;
		if ((result=storage_open_readable_binlog(pReader, \
				get_binlog_readable_filename, pReader)) != 0)
		{
			return result;
		}

		if ((result=storage_write_to_mark_file(pReader)) != 0)
		{
			return result;
		}
	}

	if ((result=splitEx(line, ' ', cols, 3)) < 3)
	{
		logError("file: "__FILE__", line: %d, " \
			"read data from binlog file \"%s\" fail, " \
			"file offset: %"PRId64", " \
			"read item count: %d < 3", \
			__LINE__, get_binlog_readable_filename(pReader, NULL), \
			pReader->binlog_offset, result);
		return EINVAL;
	}

	pRecord->timestamp = atoi(cols[0]);
	pRecord->op_type = *(cols[1]);
	pRecord->filename_len = strlen(cols[2]) - 1; //need trim new line \n
	if (pRecord->filename_len > sizeof(pRecord->filename) - 1)
	{
		logError("file: "__FILE__", line: %d, " \
			"item \"filename\" in binlog " \
			"file \"%s\" is invalid, file offset: " \
			"%"PRId64", filename length: %d > %d", \
			__LINE__, get_binlog_readable_filename(pReader, NULL), \
			pReader->binlog_offset, \
			pRecord->filename_len, (int)sizeof(pRecord->filename)-1);
		return EINVAL;
	}

	memcpy(pRecord->filename, cols[2], pRecord->filename_len);
	*(pRecord->filename + pRecord->filename_len) = '\0';
	if (pRecord->op_type == STORAGE_OP_TYPE_SOURCE_CREATE_LINK  ||
	    pRecord->op_type == STORAGE_OP_TYPE_REPLICA_CREATE_LINK ||
        pRecord->op_type == STORAGE_OP_TYPE_SOURCE_RENAME_FILE  ||
	    pRecord->op_type == STORAGE_OP_TYPE_REPLICA_RENAME_FILE)
	{
		char *p;

		p = strchr(pRecord->filename, ' ');
		if (p == NULL)
		{
			*(pRecord->src_filename) = '\0';
			pRecord->src_filename_len = 0;
		}
		else
		{
			pRecord->src_filename_len = pRecord->filename_len -
						(p - pRecord->filename) - 1;
			pRecord->filename_len = p - pRecord->filename;
			*p = '\0';

			memcpy(pRecord->src_filename, p + 1,
				pRecord->src_filename_len);
			*(pRecord->src_filename +
				pRecord->src_filename_len) = '\0';
		}
	}
	else
	{
		*(pRecord->src_filename) = '\0';
		pRecord->src_filename_len = 0;
	}

	pRecord->true_filename_len = pRecord->filename_len;
	if ((result=storage_split_filename_ex(pRecord->filename,
			&pRecord->true_filename_len, pRecord->true_filename,
			&pRecord->store_path_index)) != 0)
	{
		return result;
	}

	/*
	//printf("timestamp=%d, op_type=%c, filename=%s(%d), line length=%d, " \
		"offset=%d\n", \
		pRecord->timestamp, pRecord->op_type, \
		pRecord->filename, strlen(pRecord->filename), \
		*record_length, pReader->binlog_offset);
	*/

	return 0;
}

static int storage_binlog_reader_skip(StorageBinLogReader *pReader)
{
	StorageBinLogRecord record;
	int result;
	int record_len;

	while (1)
	{
		result = storage_binlog_read(pReader,
				&record, &record_len);
		if (result != 0)
		{
			if (result == ENOENT)
			{
				return 0;
			}

			if (result == EINVAL && g_file_sync_skip_invalid_record)
			{
				logWarning("file: "__FILE__", line: %d, " \
					"skip invalid record!", __LINE__);
			}
			else
			{
				return result;
			}
		}

		if (record.timestamp >= pReader->until_timestamp)
		{
			result = rewind_to_prev_rec_end(pReader);
			break;
		}

		pReader->binlog_offset += record_len;
	}

	return result;
}

static inline int storage_check_conflict(
        StorageSyncTaskInfo *start,
        StorageSyncTaskInfo *last)
{
    StorageSyncTaskInfo *task;

    for (task=start; task<last; task++) {
        /* check filename */
        if (fc_string_equals_ex(task->record.filename,
                    task->record.filename_len,
                    last->record.filename,
                    last->record.filename_len))
        {
            return EBUSY;
        }

        if (fc_string_equals_ex(task->record.src_filename,
                    task->record.src_filename_len,
                    last->record.filename,
                    last->record.filename_len))
        {
            return EBUSY;
        }

        if (last->record.src_filename_len == 0) {
            continue;
        }

        /* check src filename */
        if (fc_string_equals_ex(task->record.filename,
                    task->record.filename_len,
                    last->record.src_filename,
                    last->record.src_filename_len))
        {
            return EBUSY;
        }

        if (fc_string_equals_ex(task->record.src_filename,
                    task->record.src_filename_len,
                    last->record.src_filename,
                    last->record.src_filename_len))
        {
            return EBUSY;
        }
    }

    return 0;
}

static inline void storage_binlog_rewind_buff(StorageBinLogReader *pReader,
        const int record_len)
{
    pReader->binlog_buff.current -= record_len;
    pReader->binlog_buff.length += record_len;
}

static int storage_binlog_batch_read(StorageDispatchContext *dispatch_ctx)
{
	int result;
    StorageSyncTaskInfo *task;

    task = dispatch_ctx->task_array.tasks;
    while (1) {
        result = storage_binlog_read(dispatch_ctx->pReader,
                &task->record, &task->record_len);
        if (result != 0) {
            if (result == EINVAL) {
                dispatch_ctx->last_binlog_index = dispatch_ctx->
                    pReader->binlog_index;
                dispatch_ctx->last_binlog_offset = dispatch_ctx->
                    pReader->binlog_offset + task->record_len;
                dispatch_ctx->scan_row_count = 1;
            } else {
                dispatch_ctx->last_binlog_index = dispatch_ctx->
                    pReader->binlog_index;
                dispatch_ctx->last_binlog_offset = dispatch_ctx->
                    pReader->binlog_offset;
                dispatch_ctx->scan_row_count = 0;
            }

            return result;
        }

        result = storage_check_need_sync(dispatch_ctx->pReader, &task->record);
        if (result == 0) {  //OK, need sync
            task->binlog_index = dispatch_ctx->pReader->binlog_index;
            task->binlog_offset = dispatch_ctx->pReader->binlog_offset;
            task->scan_row_count = 1;
            break;
        } else if (result == EINVAL) {
            dispatch_ctx->last_binlog_index = dispatch_ctx->
                pReader->binlog_index;
            dispatch_ctx->last_binlog_offset = dispatch_ctx->
                pReader->binlog_offset + task->record_len;
            dispatch_ctx->scan_row_count = 1;
            return result;
        }

        /* skip NOT need sync record directly */
        dispatch_ctx->pReader->binlog_offset += task->record_len;
        dispatch_ctx->pReader->scan_row_count++;
    }

    dispatch_ctx->scan_row_count = task->scan_row_count;
    dispatch_ctx->last_binlog_index = task->binlog_index;
    dispatch_ctx->last_binlog_offset = task->binlog_offset + task->record_len;
    ++task;
    while (task < dispatch_ctx->task_array.end) {
        task->scan_row_count = 0;
        while (1) {
            result = storage_binlog_read(dispatch_ctx->pReader,
                    &task->record, &task->record_len);
            if (result != 0) {
                break;
            }

            result = storage_check_need_sync(dispatch_ctx->
                    pReader, &task->record);
            if (result == 0) {  //OK, need sync
                if ((result=storage_check_conflict(dispatch_ctx->task_array.
                                tasks, task)) == 0)
                {
                    task->scan_row_count++;
                    if (dispatch_ctx->last_binlog_index != dispatch_ctx->
                            pReader->binlog_index)
                    {
                        dispatch_ctx->last_binlog_index =
                            dispatch_ctx->pReader->binlog_index;
                        dispatch_ctx->last_binlog_offset =
                            dispatch_ctx->pReader->binlog_offset;
                    }

                    task->binlog_index = dispatch_ctx->pReader->binlog_index;
                    task->binlog_offset = dispatch_ctx->last_binlog_offset;
                    dispatch_ctx->last_binlog_offset += task->record_len;
                } else {
                    storage_binlog_rewind_buff(dispatch_ctx->
                            pReader, task->record_len);
                }

                break;
            } else if (result == EINVAL) {
                storage_binlog_rewind_buff(dispatch_ctx->
                        pReader, task->record_len);
                break;
            } else {  //do NOT need sync, just skip
                task->scan_row_count++;
                if (dispatch_ctx->last_binlog_index != dispatch_ctx->
                        pReader->binlog_index)
                {
                    dispatch_ctx->last_binlog_index = dispatch_ctx->
                        pReader->binlog_index;
                    dispatch_ctx->last_binlog_offset = dispatch_ctx->
                        pReader->binlog_offset;
                }
                dispatch_ctx->last_binlog_offset += task->record_len;
            }
        }

        dispatch_ctx->scan_row_count += task->scan_row_count;
        if (result != 0) {
            break;
        }
        ++task;
    }

    dispatch_ctx->task_array.count = task - dispatch_ctx->task_array.tasks;
    return 0;
}

int storage_unlink_mark_file(const char *storage_id)
{
	char old_filename[MAX_PATH_SIZE];
	char new_filename[MAX_PATH_SIZE];
	time_t t;
	struct tm tm;

	t = g_current_time;
	localtime_r(&t, &tm);

	get_mark_filename_by_id(storage_id, old_filename, sizeof(old_filename));
	if (!fileExists(old_filename))
	{
		return ENOENT;
	}

	snprintf(new_filename, sizeof(new_filename), \
		"%s.%04d%02d%02d%02d%02d%02d", old_filename, \
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, \
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (rename(old_filename, new_filename) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"rename file %s to %s fail" \
			", errno: %d, error info: %s", \
			__LINE__, old_filename, new_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EACCES;
	}

	return 0;
}

int storage_rename_mark_file(const char *old_ip_addr, const int old_port, \
		const char *new_ip_addr, const int new_port)
{
	char old_filename[MAX_PATH_SIZE];
	char new_filename[MAX_PATH_SIZE];

	get_mark_filename_by_id_and_port(old_ip_addr, old_port,
			old_filename, sizeof(old_filename));
	if (!fileExists(old_filename))
	{
		return ENOENT;
	}

	get_mark_filename_by_id_and_port(new_ip_addr, new_port,
			new_filename, sizeof(new_filename));
	if (fileExists(new_filename))
	{
		logWarning("file: "__FILE__", line: %d, " \
			"mark file %s already exists, " \
			"ignore rename file %s to %s", \
			__LINE__, new_filename, old_filename, new_filename);
		return EEXIST;
	}

	if (rename(old_filename, new_filename) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"rename file %s to %s fail" \
			", errno: %d, error info: %s", \
			__LINE__, old_filename, new_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EACCES;
	}

	return 0;
}

static void storage_sync_get_start_end_times(time_t current_time, \
	const TimeInfo *pStartTime, const TimeInfo *pEndTime, \
	time_t *start_time, time_t *end_time)
{
	struct tm tm_time;
	//char buff[32];

	localtime_r(&current_time, &tm_time);
	tm_time.tm_sec = 0;

	/*
	strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", &tm_time);
	//printf("current time: %s\n", buff);
	*/

	tm_time.tm_hour = pStartTime->hour;
	tm_time.tm_min = pStartTime->minute;
	*start_time = mktime(&tm_time);

	//end time < start time
	if (pEndTime->hour < pStartTime->hour || (pEndTime->hour == \
		pStartTime->hour && pEndTime->minute < pStartTime->minute))
	{
		current_time += 24 * 3600;
		localtime_r(&current_time, &tm_time);
		tm_time.tm_sec = 0;
	}

	tm_time.tm_hour = pEndTime->hour;
	tm_time.tm_min = pEndTime->minute;
	*end_time = mktime(&tm_time);
}

static void storage_sync_thread_exit(const FDFSStorageBrief *pStorage)
{
	int result;
	int i;
    int thread_count;
	pthread_t tid;
    char formatted_ip[FORMATTED_IP_SIZE];

	if ((result=pthread_mutex_lock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

    thread_count = FC_ATOMIC_GET(g_storage_sync_thread_count);
	tid = pthread_self();
	for (i=0; i<thread_count; i++)
	{
		if (pthread_equal(STORAGE_SYNC_TIDS[i], tid))
		{
			break;
		}
	}

	while (i < thread_count - 1)
	{
		STORAGE_SYNC_TIDS[i] = STORAGE_SYNC_TIDS[i + 1];
		i++;
	}

	FC_ATOMIC_DEC(g_storage_sync_thread_count);

	if ((result=pthread_mutex_unlock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_unlock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

    if (FC_LOG_BY_LEVEL(LOG_DEBUG)) {
        format_ip_address(pStorage->ip_addr, formatted_ip);
        logDebug("file: "__FILE__", line: %d, "
                "sync thread to storage server %s:%u exit",
                __LINE__, formatted_ip, SF_G_INNER_PORT);
    }
}

static int init_task_array(StorageDispatchContext *dispatch_ctx,
        const FDFSStorageBrief *pStorage)
{
    StorageSyncTaskInfo *tasks;
    StorageSyncTaskInfo *task;
    StorageSyncTaskInfo *end;
    int bytes;

    bytes = sizeof(StorageSyncTaskInfo) * g_sync_max_threads;
    if ((tasks=fc_malloc(bytes)) == NULL) {
        return ENOMEM;
    }

    end = tasks + g_sync_max_threads;
    for (task=tasks; task<end; task++) {
        task->dispatch_ctx = dispatch_ctx;
        conn_pool_set_server_info(&task->storage_server,
                pStorage->ip_addr, SF_G_INNER_PORT);
    }

    dispatch_ctx->task_array.count = 0;
    dispatch_ctx->task_array.tasks = tasks;
    dispatch_ctx->task_array.end = end;
    return 0;
}

static int init_dispatch_ctx(StorageDispatchContext *dispatch_ctx,
        const FDFSStorageBrief *pStorage)
{
    int result;

    if ((result=init_task_array(dispatch_ctx, pStorage)) != 0) {
        return result;
    }

    if ((result=sf_synchronize_ctx_init(&dispatch_ctx->notify_ctx)) != 0) {
        return result;
    }

    dispatch_ctx->pReader = malloc(sizeof(StorageBinLogReader));
    if (dispatch_ctx->pReader == NULL) {
        logCrit("file: "__FILE__", line: %d, "
                "malloc %d bytes fail, "
                "fail, program exit!",
                __LINE__, (int)sizeof(StorageBinLogReader));
        return ENOMEM;
    }

    memset(dispatch_ctx->pReader, 0, sizeof(StorageBinLogReader));
    dispatch_ctx->pReader->binlog_fd = -1;
    storage_reader_add_to_list(dispatch_ctx->pReader);
    return 0;
}

static void* storage_sync_thread_entrance(void* arg)
{
    StorageDispatchContext dispatch_ctx;
	FDFSStorageBrief *pStorage;
	ConnectionInfo *storage_server;  //first connection
	char local_ip_addr[IP_ADDRESS_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	int read_result;
	int sync_result;
	int result;
    time_t current_time;
	time_t start_time;
	time_t end_time;
	time_t last_keep_alive_time;

	pStorage = (FDFSStorageBrief *)arg;
#ifdef OS_LINUX
    {
        char thread_name[32];
        snprintf(thread_name, sizeof(thread_name), "data-sync[%d]",
                FC_ATOMIC_GET(g_storage_sync_thread_count));
        prctl(PR_SET_NAME, thread_name);
    }
#endif

	memset(local_ip_addr, 0, sizeof(local_ip_addr));
    if (init_dispatch_ctx(&dispatch_ctx, pStorage) != 0) {
        SF_G_CONTINUE_FLAG = false;
        storage_sync_thread_exit(pStorage);
        return NULL;
    }
    storage_server = &dispatch_ctx.task_array.tasks[0].storage_server;

	current_time = g_current_time;
	last_keep_alive_time = 0;
	start_time = 0;
	end_time = 0;

    if (FC_LOG_BY_LEVEL(LOG_DEBUG)) {
        format_ip_address(storage_server->ip_addr, formatted_ip);
        logDebug("file: "__FILE__", line: %d, "
                "sync thread to storage server %s:%u started",
                __LINE__, formatted_ip, storage_server->port);
    }
 
	while (SF_G_CONTINUE_FLAG &&
		pStorage->status != FDFS_STORAGE_STATUS_DELETED &&
		pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED &&
		pStorage->status != FDFS_STORAGE_STATUS_NONE)
	{
		while (SF_G_CONTINUE_FLAG &&
			(pStorage->status == FDFS_STORAGE_STATUS_INIT ||
			 pStorage->status == FDFS_STORAGE_STATUS_OFFLINE ||
			 pStorage->status == FDFS_STORAGE_STATUS_ONLINE))
		{
			sleep(1);
		}

		if ((!SF_G_CONTINUE_FLAG) ||
			pStorage->status == FDFS_STORAGE_STATUS_DELETED ||
			pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED ||
			pStorage->status == FDFS_STORAGE_STATUS_NONE)
		{
			break;
		}

		if (g_sync_part_time) {
            current_time = g_current_time;
            storage_sync_get_start_end_times(current_time,
                    &g_sync_end_time, &g_sync_start_time,
                    &start_time, &end_time);
            start_time += 60;
            end_time -= 60;
            while (SF_G_CONTINUE_FLAG && (current_time >= start_time
                        && current_time <= end_time))
            {
                current_time = g_current_time;
                sleep(1);
            }
        }

        if (storage_sync_connect_storage_server_always(pStorage,
                    storage_server) != 0)
        {
            break;
        }

		if (pStorage->status == FDFS_STORAGE_STATUS_DELETED ||
			pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED ||
			pStorage->status == FDFS_STORAGE_STATUS_NONE)
        {
            break;
        }

		if (pStorage->status != FDFS_STORAGE_STATUS_ACTIVE &&
			pStorage->status != FDFS_STORAGE_STATUS_WAIT_SYNC &&
			pStorage->status != FDFS_STORAGE_STATUS_SYNCING)
        {
            close(storage_server->sock);
            storage_server->sock = -1;
            sleep(5);
            continue;
        }

        storage_reader_remove_from_list(dispatch_ctx.pReader);
        result = storage_reader_init(pStorage, dispatch_ctx.pReader);
        storage_reader_add_to_list(dispatch_ctx.pReader);
		if (result != 0) {
			logCrit("file: "__FILE__", line: %d, "
				"storage_reader_init fail, errno=%d, "
				"program exit!", __LINE__, result);
			SF_G_CONTINUE_FLAG = false;
			break;
		}

		if (!dispatch_ctx.pReader->need_sync_old) {
			while (SF_G_CONTINUE_FLAG &&
			(pStorage->status != FDFS_STORAGE_STATUS_ACTIVE &&
			 pStorage->status != FDFS_STORAGE_STATUS_DELETED &&
			 pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED &&
			 pStorage->status != FDFS_STORAGE_STATUS_NONE))
			{
				sleep(1);
			}

			if (pStorage->status != FDFS_STORAGE_STATUS_ACTIVE) {
				close(storage_server->sock);
				storage_reader_destroy(dispatch_ctx.pReader);
				continue;
			}
		}

		getSockIpaddr(storage_server->sock,
                local_ip_addr, IP_ADDRESS_SIZE);
		insert_into_local_host_ip(local_ip_addr);

		/*
		//printf("file: "__FILE__", line: %d, " \
			"storage_server->ip_addr=%s, " \
			"local_ip_addr: %s\n", \
			__LINE__, pStorage->ip_addr, local_ip_addr);
		*/

        if (strcmp(pStorage->id, g_my_server_id_str) == 0 ||
                is_local_host_ip(pStorage->ip_addr))
        {  //can't self sync to self
			logError("file: "__FILE__", line: %d, " \
				"ip_addr %s belong to the local host," \
				" sync thread exit.", \
				__LINE__, pStorage->ip_addr);
			fdfs_quit(storage_server);
			close(storage_server->sock);
			break;
		}

		if (storage_report_my_server_id(storage_server) != 0) {
			close(storage_server->sock);
			storage_reader_destroy(dispatch_ctx.pReader);
			sleep(1);
			continue;
		}

        if (pStorage->status == FDFS_STORAGE_STATUS_WAIT_SYNC) {
            pStorage->status = FDFS_STORAGE_STATUS_SYNCING;
            storage_report_storage_status(pStorage->id,
                    pStorage->ip_addr, pStorage->status);
        }

		if (pStorage->status == FDFS_STORAGE_STATUS_SYNCING) {
			if (dispatch_ctx.pReader->need_sync_old && dispatch_ctx.pReader->sync_old_done) {
				pStorage->status = FDFS_STORAGE_STATUS_OFFLINE;
				storage_report_storage_status(pStorage->id,
					pStorage->ip_addr, pStorage->status);
			}
		}

		if (g_sync_part_time) {
			current_time = g_current_time;
			storage_sync_get_start_end_times(current_time,
				&g_sync_start_time, &g_sync_end_time,
				&start_time, &end_time);
		}

		sync_result = 0;
		while (SF_G_CONTINUE_FLAG && (!g_sync_part_time ||
			(current_time >= start_time && current_time <= end_time)) &&
			(pStorage->status == FDFS_STORAGE_STATUS_ACTIVE ||
			pStorage->status == FDFS_STORAGE_STATUS_SYNCING))
		{
			read_result = storage_binlog_batch_read(&dispatch_ctx);
			if (read_result == ENOENT) {
				if (dispatch_ctx.pReader->need_sync_old &&
                        !dispatch_ctx.pReader->sync_old_done)
                {
				dispatch_ctx.pReader->sync_old_done = true;
				if (storage_write_to_mark_file(dispatch_ctx.pReader) != 0)
                {
                    logCrit("file: "__FILE__", line: %d, "
                            "storage_write_to_mark_file "
                            "fail, program exit!", __LINE__);
                    SF_G_CONTINUE_FLAG = false;
                    break;
                }

				if (pStorage->status == FDFS_STORAGE_STATUS_SYNCING) {
					pStorage->status = FDFS_STORAGE_STATUS_OFFLINE;
					storage_report_storage_status(pStorage->id,
                            pStorage->ip_addr, pStorage->status);
				}
				}

				if (dispatch_ctx.pReader->last_scan_rows !=
                        dispatch_ctx.pReader->scan_row_count)
				{
					if (storage_write_to_mark_file(dispatch_ctx.pReader)!=0) {
					logCrit("file: "__FILE__", line: %d, " \
						"storage_write_to_mark_file fail, " \
						"program exit!", __LINE__);
					SF_G_CONTINUE_FLAG = false;
					break;
					}
				}

				current_time = g_current_time;
                if (current_time - last_keep_alive_time >=
                        g_heart_beat_interval)
                {
                    if (fdfs_active_test(storage_server) != 0) {
                        break;
                    }

                    last_keep_alive_time = current_time;
                }

				usleep(g_sync_wait_usec);
				continue;
			}

			if (g_sync_part_time) {
				current_time = g_current_time;
			}

            if (read_result != 0) {
                if (read_result == EINVAL && g_file_sync_skip_invalid_record) {
                    logWarning("file: "__FILE__", line: %d, "
                            "skip invalid record, binlog index: "
                            "%d, offset: %"PRId64, __LINE__,
                            dispatch_ctx.pReader->binlog_index,
                            dispatch_ctx.pReader->binlog_offset);
                } else {
                    sleep(5);
                    break;
                }
            } else if ((sync_result=storage_batch_sync_data(
                            &dispatch_ctx)) != 0)
            {
                if (!SF_G_CONTINUE_FLAG) {
                    break;
                }

                /*
				logDebug("file: "__FILE__", line: %d, " \
					"binlog index: %d, current record " \
					"offset: %"PRId64", next " \
					"record offset: %"PRId64, \
					__LINE__, dispatch_ctx.pReader->binlog_index, \
					dispatch_ctx.pReader->binlog_offset, \
					dispatch_ctx.pReader->binlog_offset + record_len);
                    */
                if (dispatch_ctx.last_binlog_index == dispatch_ctx.
                        pReader->binlog_index)
                {
                    if (rewind_to_prev_rec_end_ex(dispatch_ctx.pReader,
                                dispatch_ctx.last_binlog_offset) != 0)
                    {
                        logCrit("file: "__FILE__", line: %d, "
                                "rewind_to_prev_rec_end fail, "
                                "program exit!", __LINE__);
                        SF_G_CONTINUE_FLAG = false;
                    }
                } else {
                    //TODO
                }

				break;
			}

            if (dispatch_ctx.last_binlog_index == dispatch_ctx.
                    pReader->binlog_index)
            {
                dispatch_ctx.pReader->binlog_offset =
                    dispatch_ctx.last_binlog_offset;
            } else {
                //TODO
            }
			dispatch_ctx.pReader->scan_row_count +=
                dispatch_ctx.scan_row_count;

			if (g_sync_interval > 0)
			{
				usleep(g_sync_interval);
			}
		}

        if (dispatch_ctx.pReader->last_scan_rows !=
                dispatch_ctx.pReader->scan_row_count)
        {
            if (storage_write_to_mark_file(dispatch_ctx.pReader) != 0) {
                logCrit("file: "__FILE__", line: %d, "
                        "storage_write_to_mark_file fail, "
                        "program exit!", __LINE__);
                SF_G_CONTINUE_FLAG = false;
                break;
            }
        }

		close(storage_server->sock);
		storage_server->sock = -1;
		storage_reader_destroy(dispatch_ctx.pReader);

		if (!SF_G_CONTINUE_FLAG)
		{
			break;
		}

		if (!(sync_result == ENOTCONN || sync_result == EIO))
		{
			sleep(1);
		}
	}

    storage_reader_remove_from_list(dispatch_ctx.pReader);
	storage_reader_destroy(dispatch_ctx.pReader);

	if (pStorage->status == FDFS_STORAGE_STATUS_DELETED
	 || pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED)
	{
		storage_changelog_req();
		sleep(2 * g_heart_beat_interval + 1);
		pStorage->status = FDFS_STORAGE_STATUS_NONE;
	}

	storage_sync_thread_exit(pStorage);
	return NULL;
}

int storage_sync_thread_start(const FDFSStorageBrief *pStorage)
{
	int result;
    int thread_count;
	pthread_attr_t pattr;
	pthread_t tid;

	if (pStorage->status == FDFS_STORAGE_STATUS_DELETED || \
	    pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED || \
	    pStorage->status == FDFS_STORAGE_STATUS_NONE)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"storage id: %s 's status: %d is invalid, " \
			"can't start sync thread!", __LINE__, \
			pStorage->id, pStorage->status);
		return 0;
	}

	if (strcmp(pStorage->id, g_my_server_id_str) == 0 ||
		is_local_host_ip(pStorage->ip_addr)) //can't self sync to self
	{
		logWarning("file: "__FILE__", line: %d, "
			"storage id: %s is myself, can't start sync thread!",
			__LINE__, pStorage->id);
		return 0;
	}

	if ((result=init_pthread_attr(&pattr, SF_G_THREAD_STACK_SIZE)) != 0)
	{
		return result;
	}

	/*
	//printf("start storage ip_addr: %s, g_storage_sync_thread_count=%d\n", 
			pStorage->ip_addr, g_storage_sync_thread_count);
	*/

	if ((result=pthread_create(&tid, &pattr, storage_sync_thread_entrance,
                    (void *)pStorage)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"create thread failed, errno: %d, error info: %s",
            __LINE__, result, STRERROR(result));

		pthread_attr_destroy(&pattr);
		return result;
	}

	if ((result=pthread_mutex_lock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

    thread_count = FC_ATOMIC_INC(g_storage_sync_thread_count);
    pthread_t *new_sync_tids = (pthread_t *)realloc(STORAGE_SYNC_TIDS,
            sizeof(pthread_t) * thread_count);
	if (new_sync_tids == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"malloc %d bytes fail, errno: %d, error info: %s",
			__LINE__, (int)sizeof(pthread_t) * thread_count,
            errno, STRERROR(errno));
        free(STORAGE_SYNC_TIDS);
        STORAGE_SYNC_TIDS = NULL;
	}
	else
	{
        STORAGE_SYNC_TIDS = new_sync_tids;
		STORAGE_SYNC_TIDS[thread_count - 1] = tid;
	}

	if ((result=pthread_mutex_unlock(&SYNC_THREAD_LOCK)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	pthread_attr_destroy(&pattr);

	return 0;
}


void storage_reader_add_to_list(StorageBinLogReader *pReader)
{
	pthread_mutex_lock(&SYNC_THREAD_LOCK);
    fc_list_add_tail(&pReader->link, &SYNC_READER_HEAD);
	pthread_mutex_unlock(&SYNC_THREAD_LOCK);
}

void storage_reader_remove_from_list(StorageBinLogReader *pReader)
{
	pthread_mutex_lock(&SYNC_THREAD_LOCK);
    fc_list_del_init(&pReader->link);
	pthread_mutex_unlock(&SYNC_THREAD_LOCK);
}

static int calc_compress_until_binlog_index()
{
    StorageBinLogReader *pReader;
    int min_index;

	pthread_mutex_lock(&SYNC_THREAD_LOCK);

    min_index = g_binlog_index;
    fc_list_for_each_entry(pReader, &SYNC_READER_HEAD, link)
    {
        if (pReader->binlog_fd >= 0 && pReader->binlog_index >= 0 &&
                pReader->binlog_index < min_index)
        {
            min_index = pReader->binlog_index;
        }
    }
	pthread_mutex_unlock(&SYNC_THREAD_LOCK);

    return min_index;
}

int fdfs_binlog_compress_func(void *args)
{
	char full_filename[MAX_PATH_SIZE];
    int until_index;
    int bindex;
    int result;

    if (storage_sync_ctx.binlog_compress_index >= g_binlog_index)
    {
        return 0;
    }

    until_index = calc_compress_until_binlog_index();
    for (bindex = storage_sync_ctx.binlog_compress_index;
            bindex < until_index; bindex++)
    {
        get_binlog_readable_filename_ex(bindex, full_filename);
        result = compress_binlog_file(full_filename);
        if (!(result == 0 || result == ENOENT))
        {
            break;
        }

        storage_sync_ctx.binlog_compress_index = bindex + 1;
        write_to_binlog_index(g_binlog_index);
    }

    return 0;
}
