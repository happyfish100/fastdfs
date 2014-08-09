/** * Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
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
#include "fdfs_define.h"
#include "logger.h"
#include "fdfs_global.h"
#include "sockopt.h"
#include "shared_func.h"
#include "pthread_func.h"
#include "sched_thread.h"
#include "ini_file_reader.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_ip_changed_dealer.h"
#include "tracker_client_thread.h"
#include "storage_client.h"
#include "storage_sync.h"
#include "trunk_mem.h"

#define SYNC_BINLOG_FILE_MAX_SIZE	1024 * 1024 * 1024
#define SYNC_BINLOG_FILE_PREFIX		"binlog"
#define SYNC_BINLOG_INDEX_FILENAME	SYNC_BINLOG_FILE_PREFIX".index"
#define SYNC_MARK_FILE_EXT		".mark"
#define SYNC_BINLOG_FILE_EXT_FMT	".%03d"
#define SYNC_DIR_NAME			"sync"
#define MARK_ITEM_BINLOG_FILE_INDEX	"binlog_index"
#define MARK_ITEM_BINLOG_FILE_OFFSET	"binlog_offset"
#define MARK_ITEM_NEED_SYNC_OLD		"need_sync_old"
#define MARK_ITEM_SYNC_OLD_DONE		"sync_old_done"
#define MARK_ITEM_UNTIL_TIMESTAMP	"until_timestamp"
#define MARK_ITEM_SCAN_ROW_COUNT	"scan_row_count"
#define MARK_ITEM_SYNC_ROW_COUNT	"sync_row_count"
#define SYNC_BINLOG_WRITE_BUFF_SIZE	(16 * 1024)

int g_binlog_fd = -1;
int g_binlog_index = 0;
static int64_t binlog_file_size = 0;

int g_storage_sync_thread_count = 0;
static pthread_mutex_t sync_thread_lock;
static char *binlog_write_cache_buff = NULL;
static int binlog_write_cache_len = 0;
static int binlog_write_version = 1;

/* save sync thread ids */
static pthread_t *sync_tids = NULL;

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
static int storage_sync_copy_file(ConnectionInfo *pStorageServer, \
	StorageBinLogReader *pReader, const StorageBinLogRecord *pRecord, \
	char proto_cmd)
{
	TrackerHeader *pHeader;
	char *p;
	char *pBuff;
	char full_filename[MAX_PATH_SIZE];
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+256];
	char in_buff[1];
	struct stat stat_buf;
	FDFSTrunkFullInfo trunkInfo;
	FDFSTrunkHeader trunkHeader;
	int64_t file_offset;
	int64_t in_bytes;
	int64_t total_send_bytes;
	int result;
	bool need_sync_file;

	if ((result=trunk_file_stat(pRecord->store_path_index, \
		pRecord->true_filename, pRecord->true_filename_len, \
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
	if (pReader->last_file_exist && proto_cmd == \
			STORAGE_PROTO_CMD_SYNC_CREATE_FILE)
	{
		FDFSFileInfo file_info;
		result = storage_query_file_info_ex(NULL, \
				pStorageServer,  g_group_name, \
				pRecord->filename, &file_info, true);
		if (result == 0)
		{
			if (file_info.file_size == stat_buf.st_size)
			{
				logDebug("file: "__FILE__", line: %d, " \
					"sync data file, logic file: %s " \
					"on dest server %s:%d already exists, "\
					"and same as mine, ignore it", \
					__LINE__, pRecord->filename, \
					pStorageServer->ip_addr, \
					pStorageServer->port);
				need_sync_file = false;
			}
			else
			{
				logWarning("file: "__FILE__", line: %d, " \
					"sync data file, logic file: %s " \
					"on dest server %s:%d already exists, "\
					"but file size: %"PRId64 \
					" not same as mine: "OFF_PRINTF_FORMAT\
					", need re-sync it", __LINE__, \
					pRecord->filename, pStorageServer->ip_addr,\
					pStorageServer->port, \
					file_info.file_size, stat_buf.st_size);

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
		sprintf(full_filename, "%s/data/%s", \
			g_fdfs_store_paths.paths[pRecord->store_path_index], \
			pRecord->true_filename);
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

		sprintf(p, "%s", g_group_name);
		p += FDFS_GROUP_NAME_MAX_LEN;
		memcpy(p, pRecord->filename, pRecord->filename_len);
		p += pRecord->filename_len;

		if((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
			p - out_buff, g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"sync data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pStorageServer->ip_addr, \
				pStorageServer->port, \
				result, STRERROR(result));

			break;
		}

		if (need_sync_file && (stat_buf.st_size > 0) && \
			((result=tcpsendfile_ex(pStorageServer->sock, \
			full_filename, file_offset, stat_buf.st_size, \
			g_fdfs_network_timeout, &total_send_bytes)) != 0))
		{
			logError("file: "__FILE__", line: %d, " \
				"sync data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pStorageServer->ip_addr, \
				pStorageServer->port, \
				result, STRERROR(result));

			break;
		}

		pBuff = in_buff;
		if ((result=fdfs_recv_response(pStorageServer, \
			&pBuff, 0, &in_bytes)) != 0)
		{
			break;
		}
	} while (0);

	pthread_mutex_lock(&sync_thread_lock);
	g_storage_stat.total_sync_out_bytes += total_send_bytes;
	if (result == 0)
	{
		g_storage_stat.success_sync_out_bytes += total_send_bytes;
	}
	pthread_mutex_unlock(&sync_thread_lock);

	if (result == EEXIST)
	{
		if (need_sync_file && pRecord->op_type == \
			STORAGE_OP_TYPE_SOURCE_CREATE_FILE)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"storage server ip: %s:%d, data file: %s " \
				"already exists, maybe some mistake?", \
				__LINE__, pStorageServer->ip_addr, \
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
#define FIELD_COUNT  3
	TrackerHeader *pHeader;
	char *p;
	char *pBuff;
	char *fields[FIELD_COUNT];
	char full_filename[MAX_PATH_SIZE];
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+256];
	char in_buff[1];
	struct stat stat_buf;
	int64_t in_bytes;
	int64_t total_send_bytes;
	int64_t start_offset;
	int64_t modify_length;
	int result;
	int count;

	if ((count=splitEx(pRecord->filename, ' ', fields, FIELD_COUNT)) \
			!= FIELD_COUNT)
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

	snprintf(full_filename, sizeof(full_filename), \
		"%s/data/%s", g_fdfs_store_paths.paths[pRecord->store_path_index], \
		pRecord->true_filename);
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

		sprintf(p, "%s", g_group_name);
		p += FDFS_GROUP_NAME_MAX_LEN;
		memcpy(p, pRecord->filename, pRecord->filename_len);
		p += pRecord->filename_len;

		if((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
			p - out_buff, g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"sync data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pStorageServer->ip_addr, \
				pStorageServer->port, \
				result, STRERROR(result));

			break;
		}

		if ((result=tcpsendfile_ex(pStorageServer->sock, \
			full_filename, start_offset, modify_length, \
			g_fdfs_network_timeout, &total_send_bytes)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"sync data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pStorageServer->ip_addr, \
				pStorageServer->port, \
				result, STRERROR(result));

			break;
		}

		pBuff = in_buff;
		if ((result=fdfs_recv_response(pStorageServer, \
			&pBuff, 0, &in_bytes)) != 0)
		{
			break;
		}
	} while (0);

	pthread_mutex_lock(&sync_thread_lock);
	g_storage_stat.total_sync_out_bytes += total_send_bytes;
	if (result == 0)
	{
		g_storage_stat.success_sync_out_bytes += total_send_bytes;
	}
	pthread_mutex_unlock(&sync_thread_lock);

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
#define FIELD_COUNT  3
	TrackerHeader *pHeader;
	char *p;
	char *pBuff;
	char *fields[FIELD_COUNT];
	char full_filename[MAX_PATH_SIZE];
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+256];
	char in_buff[1];
	struct stat stat_buf;
	int64_t in_bytes;
	int64_t old_file_size;
	int64_t new_file_size;
	int result;
	int count;

	if ((count=splitEx(pRecord->filename, ' ', fields, FIELD_COUNT)) \
			!= FIELD_COUNT)
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

	snprintf(full_filename, sizeof(full_filename), \
		"%s/data/%s", g_fdfs_store_paths.paths[pRecord->store_path_index], \
		pRecord->true_filename);
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

		sprintf(p, "%s", g_group_name);
		p += FDFS_GROUP_NAME_MAX_LEN;
		memcpy(p, pRecord->filename, pRecord->filename_len);
		p += pRecord->filename_len;

		if((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
			p - out_buff, g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"sync data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pStorageServer->ip_addr, \
				pStorageServer->port, \
				result, STRERROR(result));

			break;
		}

		pBuff = in_buff;
		if ((result=fdfs_recv_response(pStorageServer, \
			&pBuff, 0, &in_bytes)) != 0)
		{
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
		pRecord->filename_len, g_fdfs_network_timeout)) != 0)
	{
		logError("FILE: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pStorageServer->ip_addr, \
			pStorageServer->port, \
			result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	result = fdfs_recv_response(pStorageServer, &pBuff, 0, &in_bytes);
	if (result == ENOENT)
	{
		result = 0;
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
	char in_buff[1];
	char *pBuff;
	int64_t in_bytes;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	
	long2buff(IP_ADDRESS_SIZE, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_REPORT_SERVER_ID;
	strcpy(out_buff + sizeof(TrackerHeader), g_my_server_id_str);
	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		sizeof(TrackerHeader) + FDFS_STORAGE_ID_MAX_SIZE, \
		g_fdfs_network_timeout)) != 0)
	{
		logError("FILE: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pStorageServer->ip_addr, \
			pStorageServer->port, \
			result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	return fdfs_recv_response(pStorageServer, &pBuff, 0, &in_bytes);
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
		result = trunk_file_get_content(&trunkInfo, \
                	stat_buf.st_size, &fd, pRecord->src_filename, \
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
	int src_path_index;
	int src_filename_len;

	snprintf(full_filename, sizeof(full_filename), \
		"%s/data/%s", g_fdfs_store_paths.paths[pRecord->store_path_index], \
		pRecord->true_filename);
	src_filename_len = readlink(full_filename, src_full_filename, \
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

		for (src_path_index=0; src_path_index<g_fdfs_store_paths.count; \
			src_path_index++)
		{
			if (strcmp(src_full_filename, \
				g_fdfs_store_paths.paths[src_path_index]) == 0)
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

	pRecord->src_filename_len = src_filename_len - (pSrcFilename - \
					src_full_filename) + 4;
	if (pRecord->src_filename_len >= sizeof(pRecord->src_filename))
	{
		logError("file: "__FILE__", line: %d, " \
			"source data file: %s is invalid", \
			__LINE__, src_full_filename);
		return EINVAL;
	}

	sprintf(pRecord->src_filename, "%c"FDFS_STORAGE_DATA_DIR_FORMAT"/%s", \
			FDFS_STORAGE_STORE_PATH_PREFIX_CHAR, \
			src_path_index, pSrcFilename);
	}

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	long2buff(pRecord->filename_len, out_buff + sizeof(TrackerHeader));
	long2buff(pRecord->src_filename_len, out_buff + sizeof(TrackerHeader) + \
			FDFS_PROTO_PKG_LEN_SIZE);
	int2buff(pRecord->timestamp, out_buff + sizeof(TrackerHeader) + \
			2 * FDFS_PROTO_PKG_LEN_SIZE);
	sprintf(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE\
		 + 4, "%s", g_group_name);
	memcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE \
		+ 4 + FDFS_GROUP_NAME_MAX_LEN, \
		pRecord->filename, pRecord->filename_len);
	memcpy(out_buff + sizeof(TrackerHeader) + 2 * FDFS_PROTO_PKG_LEN_SIZE \
		+ 4 + FDFS_GROUP_NAME_MAX_LEN + pRecord->filename_len, \
		pRecord->src_filename, pRecord->src_filename_len);

	out_body_len = 2 * FDFS_PROTO_PKG_LEN_SIZE + 4 + \
		FDFS_GROUP_NAME_MAX_LEN + pRecord->filename_len + \
		pRecord->src_filename_len;
	long2buff(out_body_len, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_SYNC_CREATE_LINK;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		sizeof(TrackerHeader) + out_body_len, \
		g_fdfs_network_timeout)) != 0)
	{
		logError("FILE: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pStorageServer->ip_addr, \
			pStorageServer->port, \
			result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	result = fdfs_recv_response(pStorageServer, &pBuff, 0, &in_bytes);
	if (result == ENOENT)
	{
		result = 0;
	}
	
	return result;
}

#define STARAGE_CHECK_IF_NEED_SYNC_OLD(pReader, pRecord) \
	if ((!pReader->need_sync_old) || pReader->sync_old_done || \
		(pRecord->timestamp > pReader->until_timestamp)) \
	{ \
		return 0; \
	} \

static int storage_sync_data(StorageBinLogReader *pReader, \
			ConnectionInfo *pStorageServer, \
			StorageBinLogRecord *pRecord)
{
	int result;
	switch(pRecord->op_type)
	{
		case STORAGE_OP_TYPE_SOURCE_CREATE_FILE:
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_CREATE_FILE);
			break;
		case STORAGE_OP_TYPE_SOURCE_DELETE_FILE:
			result = storage_sync_delete_file( \
					pStorageServer, pRecord);
			break;
		case STORAGE_OP_TYPE_SOURCE_UPDATE_FILE:
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			break;
		case STORAGE_OP_TYPE_SOURCE_APPEND_FILE:
			result = storage_sync_modify_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_APPEND_FILE);
			if (result == ENOENT)  //resync appender file
			{
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			}
			break;
		case STORAGE_OP_TYPE_SOURCE_MODIFY_FILE:
			result = storage_sync_modify_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_MODIFY_FILE);
			if (result == ENOENT)  //resync appender file
			{
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			}
			break;
		case STORAGE_OP_TYPE_SOURCE_TRUNCATE_FILE:
			result = storage_sync_truncate_file(pStorageServer, \
					pReader, pRecord);
			break;
		case STORAGE_OP_TYPE_SOURCE_CREATE_LINK:
			result = storage_sync_link_file(pStorageServer, \
					pRecord);
			break;
		case STORAGE_OP_TYPE_REPLICA_CREATE_FILE:
			STARAGE_CHECK_IF_NEED_SYNC_OLD(pReader, pRecord)
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_CREATE_FILE);
			break;
		case STORAGE_OP_TYPE_REPLICA_DELETE_FILE:
			STARAGE_CHECK_IF_NEED_SYNC_OLD(pReader, pRecord)
			result = storage_sync_delete_file( \
					pStorageServer, pRecord);
			break;
		case STORAGE_OP_TYPE_REPLICA_UPDATE_FILE:
			STARAGE_CHECK_IF_NEED_SYNC_OLD(pReader, pRecord)
			result = storage_sync_copy_file(pStorageServer, \
					pReader, pRecord, \
					STORAGE_PROTO_CMD_SYNC_UPDATE_FILE);
			break;
		case STORAGE_OP_TYPE_REPLICA_CREATE_LINK:
			STARAGE_CHECK_IF_NEED_SYNC_OLD(pReader, pRecord)
			result = storage_sync_link_file(pStorageServer, \
					pRecord);
			break;
		case STORAGE_OP_TYPE_REPLICA_APPEND_FILE:
			return 0;
		case STORAGE_OP_TYPE_REPLICA_MODIFY_FILE:
			return 0;
		case STORAGE_OP_TYPE_REPLICA_TRUNCATE_FILE:
			return 0;
		default:
			logError("file: "__FILE__", line: %d, " \
				"invalid file operation type: %d", \
				__LINE__, pRecord->op_type);
			return EINVAL;
	}

	if (result == 0)
	{
		pReader->sync_row_count++;

		if (pReader->sync_row_count - pReader->last_sync_rows >= \
			g_write_mark_file_freq)
		{
			if ((result=storage_write_to_mark_file(pReader)) != 0)
			{
				logCrit("file: "__FILE__", line: %d, " \
					"storage_write_to_mark_file " \
					"fail, program exit!", __LINE__);
				g_continue_flag = false;
				return result;
			}
		}
	}

	return result;
}

static int write_to_binlog_index(const int binlog_index)
{
	char full_filename[MAX_PATH_SIZE];
	char buff[16];
	int fd;
	int len;

	snprintf(full_filename, sizeof(full_filename), \
			"%s/data/"SYNC_DIR_NAME"/%s", g_fdfs_base_path, \
			SYNC_BINLOG_INDEX_FILENAME);
	if ((fd=open(full_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	len = sprintf(buff, "%d", binlog_index);
	if (write(fd, buff, len) != len)
	{
		logError("file: "__FILE__", line: %d, " \
			"write to file \"%s\" fail, " \
			"errno: %d, error info: %s",  \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		close(fd);
		return errno != 0 ? errno : EIO;
	}

	close(fd);

	STORAGE_CHOWN(full_filename, geteuid(), getegid())

	return 0;
}

static char *get_writable_binlog_filename(char *full_filename)
{
	static char buff[MAX_PATH_SIZE];

	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	snprintf(full_filename, MAX_PATH_SIZE, \
			"%s/data/"SYNC_DIR_NAME"/"SYNC_BINLOG_FILE_PREFIX"" \
			SYNC_BINLOG_FILE_EXT_FMT, \
			g_fdfs_base_path, g_binlog_index);
	return full_filename;
}

static char *get_writable_binlog_filename1(char *full_filename, \
		const int binlog_index)
{
	snprintf(full_filename, MAX_PATH_SIZE, \
			"%s/data/"SYNC_DIR_NAME"/"SYNC_BINLOG_FILE_PREFIX"" \
			SYNC_BINLOG_FILE_EXT_FMT, \
			g_fdfs_base_path, binlog_index);
	return full_filename;
}

static int open_next_writable_binlog()
{
	char full_filename[MAX_PATH_SIZE];

	if (g_binlog_fd >= 0)
	{
		close(g_binlog_fd);
		g_binlog_fd = -1;
	}

	get_writable_binlog_filename1(full_filename, g_binlog_index + 1);
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
	STORAGE_FCHOWN(g_binlog_fd, full_filename, geteuid(), getegid())

	g_binlog_index++;
	return 0;
}

int storage_sync_init()
{
	char data_path[MAX_PATH_SIZE];
	char sync_path[MAX_PATH_SIZE];
	char full_filename[MAX_PATH_SIZE];
	char file_buff[64];
	int bytes;
	int result;
	int fd;

	snprintf(data_path, sizeof(data_path), "%s/data", g_fdfs_base_path);
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

		STORAGE_CHOWN(data_path, geteuid(), getegid())
	}

	snprintf(sync_path, sizeof(sync_path), \
			"%s/"SYNC_DIR_NAME, data_path);
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

		STORAGE_CHOWN(sync_path, geteuid(), getegid())
	}

	binlog_write_cache_buff = (char *)malloc(SYNC_BINLOG_WRITE_BUFF_SIZE);
	if (binlog_write_cache_buff == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, SYNC_BINLOG_WRITE_BUFF_SIZE, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}

	snprintf(full_filename, sizeof(full_filename), \
			"%s/%s", sync_path, SYNC_BINLOG_INDEX_FILENAME);
	if ((fd=open(full_filename, O_RDONLY)) >= 0)
	{
		bytes = read(fd, file_buff, sizeof(file_buff) - 1);
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
		if ((result=write_to_binlog_index(g_binlog_index)) != 0)
		{
			return result;
		}
	}

	get_writable_binlog_filename(full_filename);
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

	binlog_file_size = lseek(g_binlog_fd, 0, SEEK_END);
	if (binlog_file_size < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"ftell file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		storage_sync_destroy();
		return errno != 0 ? errno : EIO;
	}

	STORAGE_FCHOWN(g_binlog_fd, full_filename, geteuid(), getegid())

	/*
	//printf("full_filename=%s, binlog_file_size=%d\n", \
			full_filename, binlog_file_size);
	*/
	
	if ((result=init_pthread_lock(&sync_thread_lock)) != 0)
	{
		return result;
	}

	load_local_host_ip_addrs();

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

	if (binlog_write_cache_buff != NULL)
	{
		free(binlog_write_cache_buff);
		binlog_write_cache_buff = NULL;
		if ((result=pthread_mutex_destroy(&sync_thread_lock)) != 0)
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

	if (sync_tids == NULL)
	{
		return 0;
	}

	if ((result=pthread_mutex_lock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	kill_res = kill_work_threads(sync_tids, g_storage_sync_thread_count);

	if ((result=pthread_mutex_unlock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	while (g_storage_sync_thread_count > 0)
	{
		usleep(50000);
	}

	return kill_res;
}

int fdfs_binlog_sync_func(void *args)
{
	if (binlog_write_cache_len > 0)
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

	if (bNeedLock && (result=pthread_mutex_lock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if (binlog_write_cache_len == 0) //ignore
	{
		write_ret = 0;  //skip
	}
	else if (write(g_binlog_fd, binlog_write_cache_buff, \
		binlog_write_cache_len) != binlog_write_cache_len)
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
		binlog_file_size += binlog_write_cache_len;
		if (binlog_file_size >= SYNC_BINLOG_FILE_MAX_SIZE)
		{
			if ((write_ret=write_to_binlog_index( \
				g_binlog_index + 1)) == 0)
			{
				write_ret = open_next_writable_binlog();
			}

			binlog_file_size = 0;
			if (write_ret != 0)
			{
				g_continue_flag = false;
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

	binlog_write_version++;
	binlog_write_cache_len = 0;  //reset cache buff

	if (bNeedLock && (result=pthread_mutex_unlock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return write_ret;
}

int storage_binlog_write_ex(const int timestamp, const char op_type, \
		const char *filename, const char *extra)
{
	int result;
	int write_ret;

	if ((result=pthread_mutex_lock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if (extra != NULL)
	{
		binlog_write_cache_len += sprintf(binlog_write_cache_buff + \
					binlog_write_cache_len, "%d %c %s %s\n",\
					timestamp, op_type, filename, extra);
	}
	else
	{
		binlog_write_cache_len += sprintf(binlog_write_cache_buff + \
					binlog_write_cache_len, "%d %c %s\n", \
					timestamp, op_type, filename);
	}

	//check if buff full
	if (SYNC_BINLOG_WRITE_BUFF_SIZE - binlog_write_cache_len < 256)
	{
		write_ret = storage_binlog_fsync(false);  //sync to disk
	}
	else
	{
		write_ret = 0;
	}

	if ((result=pthread_mutex_unlock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return write_ret;
}

static char *get_binlog_readable_filename(const void *pArg, \
		char *full_filename)
{
	const StorageBinLogReader *pReader;
	static char buff[MAX_PATH_SIZE];

	pReader = (const StorageBinLogReader *)pArg;
	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	snprintf(full_filename, MAX_PATH_SIZE, \
			"%s/data/"SYNC_DIR_NAME"/"SYNC_BINLOG_FILE_PREFIX"" \
			SYNC_BINLOG_FILE_EXT_FMT, \
			g_fdfs_base_path, pReader->binlog_index);
	return full_filename;
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

static char *get_mark_filename_by_id_and_port(const char *storage_id, \
		const int port, char *full_filename, const int filename_size)
{
	if (g_use_storage_id)
	{
		snprintf(full_filename, filename_size, \
			"%s/data/"SYNC_DIR_NAME"/%s%s", g_fdfs_base_path, \
			storage_id, SYNC_MARK_FILE_EXT);
	}
	else
	{
		snprintf(full_filename, filename_size, \
			"%s/data/"SYNC_DIR_NAME"/%s_%d%s", g_fdfs_base_path, \
			storage_id, port, SYNC_MARK_FILE_EXT);
	}
	return full_filename;
}

static char *get_mark_filename_by_ip_and_port(const char *ip_addr, \
		const int port, char *full_filename, const int filename_size)
{
	snprintf(full_filename, filename_size, \
		"%s/data/"SYNC_DIR_NAME"/%s_%d%s", g_fdfs_base_path, \
		ip_addr, port, SYNC_MARK_FILE_EXT);
	return full_filename;
}

char *get_mark_filename_by_reader(const void *pArg, char *full_filename)
{
	const StorageBinLogReader *pReader;
	static char buff[MAX_PATH_SIZE];

	pReader = (const StorageBinLogReader *)pArg;
	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	return get_mark_filename_by_id_and_port(pReader->storage_id, \
			g_server_port, full_filename, MAX_PATH_SIZE);
}

static char *get_mark_filename_by_id(const char *storage_id, \
		char *full_filename, const int filename_size)
{
	return get_mark_filename_by_id_and_port(storage_id, g_server_port, \
				full_filename, filename_size);
}

int storage_report_storage_status(const char *storage_id, \
		const char *ip_addr, const char status)
{
	FDFSStorageBrief briefServer;
	ConnectionInfo trackerServer;
	ConnectionInfo *pGlobalServer;
	ConnectionInfo *pTServer;
	ConnectionInfo *pTServerEnd;
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

		while (g_continue_flag && !g_sync_old_done)
		{
			sleep(1);
		}

		if (!g_continue_flag)
		{
			return 0;
		}

		logDebug("file: "__FILE__", line: %d, " \
			"report storage %s 's status as: %d, " \
			"ok, g_sync_old_done turn to true", \
			__LINE__, ip_addr, status);
	}

	report_count = 0;
	success_count = 0;

	result = 0;
	pTServer = &trackerServer;
	pTServerEnd = g_tracker_group.servers + g_tracker_group.server_count;
	for (pGlobalServer=g_tracker_group.servers; pGlobalServer<pTServerEnd; \
			pGlobalServer++)
	{
		memcpy(pTServer, pGlobalServer, sizeof(ConnectionInfo));
		for (i=0; i < 3; i++)
		{
			pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
			if(pTServer->sock < 0)
			{
				result = errno != 0 ? errno : EPERM;
				logError("file: "__FILE__", line: %d, " \
					"socket create failed, errno: %d, " \
					"error info: %s.", \
					__LINE__, result, STRERROR(result));
				sleep(5);
				break;
			}

			if (g_client_bind_addr && *g_bind_addr != '\0')
			{
				socketBind(pTServer->sock, g_bind_addr, 0);
			}

			tcpsetserveropt(pTServer->sock, g_fdfs_network_timeout);

			if (tcpsetnonblockopt(pTServer->sock) != 0)
			{
				close(pTServer->sock);
				pTServer->sock = -1;
				sleep(5);
				continue;
			}

			if ((result=connectserverbyip_nb(pTServer->sock, \
				pTServer->ip_addr, pTServer->port, \
				g_fdfs_connect_timeout)) == 0)
			{
				break;
			}

			close(pTServer->sock);
			pTServer->sock = -1;
			sleep(5);
		}

		if (pTServer->sock < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"connect to tracker server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pTServer->ip_addr, pTServer->port, \
				result, STRERROR(result));

			continue;
		}

		report_count++;
		if ((result=tracker_report_storage_status(pTServer, \
			&briefServer)) == 0)
		{
			success_count++;
		}

		fdfs_quit(pTServer);
		close(pTServer->sock);
	}

	logDebug("file: "__FILE__", line: %d, " \
		"report storage %s 's status as: %d done, " \
		"report count: %d, success count: %d", \
		__LINE__, ip_addr, status, report_count, success_count);

	return success_count > 0 ? 0 : EAGAIN;
}

static int storage_reader_sync_init_req(StorageBinLogReader *pReader)
{
	ConnectionInfo *pTrackerServers;
	ConnectionInfo *pTServer;
	ConnectionInfo *pTServerEnd;
	char tracker_client_ip[IP_ADDRESS_SIZE];
	int result;
	int conn_ret;

	if (!g_sync_old_done)
	{
		while (g_continue_flag && !g_sync_old_done)
		{
			sleep(1);
		}

		if (!g_continue_flag)
		{
			return EINTR;
		}
	}

	pTrackerServers = (ConnectionInfo *)malloc( \
		sizeof(ConnectionInfo) * g_tracker_group.server_count);
	if (pTrackerServers == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail", __LINE__, \
			(int)sizeof(ConnectionInfo) * \
			g_tracker_group.server_count);
		return errno != 0 ? errno : ENOMEM;
	}

	memcpy(pTrackerServers, g_tracker_group.servers, \
		sizeof(ConnectionInfo) * g_tracker_group.server_count);
	pTServerEnd = pTrackerServers + g_tracker_group.server_count;
	for (pTServer=pTrackerServers; pTServer<pTServerEnd; pTServer++)
	{
		pTServer->sock = -1;
	}

	result = EINTR;
	if (g_tracker_group.leader_index >= 0 && \
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
		while (g_continue_flag)
		{
			pTServer->sock = socket(AF_INET, SOCK_STREAM, 0);
			if(pTServer->sock < 0)
			{
				logCrit("file: "__FILE__", line: %d, " \
					"socket create failed, errno: %d, " \
					"error info: %s. program exit!", \
					__LINE__, errno, STRERROR(errno));
				g_continue_flag = false;
				result = errno != 0 ? errno : EPERM;
				break;
			}

			if (g_client_bind_addr && *g_bind_addr != '\0')
			{
				socketBind(pTServer->sock, g_bind_addr, 0);
			}

			if (tcpsetnonblockopt(pTServer->sock) != 0)
			{
				close(pTServer->sock);
				sleep(g_heart_beat_interval);
				continue;
			}

			if ((conn_ret=connectserverbyip_nb(pTServer->sock, \
				pTServer->ip_addr, pTServer->port, \
				g_fdfs_connect_timeout)) == 0)
			{
				break;
			}

			logError("file: "__FILE__", line: %d, " \
				"connect to tracker server %s:%d fail, " \
				"errno: %d, error info: %s", \
				__LINE__, pTServer->ip_addr, pTServer->port, \
				conn_ret, STRERROR(conn_ret));

			close(pTServer->sock);

			pTServer++;
			if (pTServer >= pTServerEnd)
			{
				pTServer = pTrackerServers;
			}

			sleep(g_heart_beat_interval);
		}

		if (!g_continue_flag)
		{
			break;
		}

		getSockIpaddr(pTServer->sock, \
				tracker_client_ip, IP_ADDRESS_SIZE);
		insert_into_local_host_ip(tracker_client_ip);

		if ((result=tracker_sync_src_req(pTServer, pReader)) != 0)
		{
			fdfs_quit(pTServer);
			close(pTServer->sock);
			sleep(g_heart_beat_interval);
			continue;
		}

		fdfs_quit(pTServer);
		close(pTServer->sock);

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
	char full_filename[MAX_PATH_SIZE];
	IniContext iniContext;
	int result;
	bool bFileExist;
	bool bNeedSyncOld;

	memset(pReader, 0, sizeof(StorageBinLogReader));
	pReader->mark_fd = -1;
	pReader->binlog_fd = -1;

	pReader->binlog_buff.buffer = (char *)malloc( \
				STORAGE_BINLOG_BUFFER_SIZE);
	if (pReader->binlog_buff.buffer == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, STORAGE_BINLOG_BUFFER_SIZE, \
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
	get_mark_filename_by_reader(pReader, full_filename);

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
		bFileExist = fileExists(full_filename);
		if (!bFileExist && (g_use_storage_id && pStorage != NULL))
		{
			char old_mark_filename[MAX_PATH_SIZE];
			get_mark_filename_by_ip_and_port(pStorage->ip_addr, \
				g_server_port, old_mark_filename, \
				sizeof(old_mark_filename));
			if (fileExists(old_mark_filename))
			{
				if (rename(old_mark_filename, full_filename)!=0)
				{
					logError("file: "__FILE__", line: %d, "\
						"rename file %s to %s fail" \
						", errno: %d, error info: %s", \
						__LINE__, old_mark_filename, \
						full_filename, errno, \
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
		if ((result=iniLoadFromFile(full_filename, &iniContext)) \
			 != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"load from mark file \"%s\" fail, " \
				"error code: %d", \
				__LINE__, full_filename, result);
			return result;
		}

		if (iniContext.global.count < 7)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, " \
				"in mark file \"%s\", item count: %d < 7", \
				__LINE__, full_filename, iniContext.global.count);
			return ENOENT;
		}

		bNeedSyncOld = iniGetBoolValue(NULL,  \
				MARK_ITEM_NEED_SYNC_OLD, \
				&iniContext, false);
		if (pStorage != NULL && pStorage->status == \
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
					MARK_ITEM_BINLOG_FILE_INDEX, \
					&iniContext, -1);
			pReader->binlog_offset = iniGetInt64Value(NULL, \
					MARK_ITEM_BINLOG_FILE_OFFSET, \
					&iniContext, -1);
			pReader->sync_old_done = iniGetBoolValue(NULL,  \
					MARK_ITEM_SYNC_OLD_DONE, \
					&iniContext, false);
			pReader->until_timestamp = iniGetIntValue(NULL, \
					MARK_ITEM_UNTIL_TIMESTAMP, \
					&iniContext, -1);
			pReader->scan_row_count = iniGetInt64Value(NULL, \
					MARK_ITEM_SCAN_ROW_COUNT, \
					&iniContext, 0);
			pReader->sync_row_count = iniGetInt64Value(NULL, \
					MARK_ITEM_SYNC_ROW_COUNT, \
					&iniContext, 0);

			if (pReader->binlog_index < 0)
			{
				iniFreeContext(&iniContext);
				logError("file: "__FILE__", line: %d, " \
					"in mark file \"%s\", " \
					"binlog_index: %d < 0", \
					__LINE__, full_filename, \
					pReader->binlog_index);
				return EINVAL;
			}
			if (pReader->binlog_offset < 0)
			{
				iniFreeContext(&iniContext);
				logError("file: "__FILE__", line: %d, " \
					"in mark file \"%s\", binlog_offset: "\
					"%"PRId64" < 0", \
					__LINE__, full_filename, \
					pReader->binlog_offset);
				return EINVAL;
			}
		}

		iniFreeContext(&iniContext);
	}

	pReader->last_scan_rows = pReader->scan_row_count;
	pReader->last_sync_rows = pReader->sync_row_count;

	pReader->mark_fd = open(full_filename, O_WRONLY | O_CREAT, 0644);
	if (pReader->mark_fd < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open mark file \"%s\" fail, " \
			"error no: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}
	STORAGE_FCHOWN(pReader->mark_fd, full_filename, geteuid(), getegid())

	if ((result=storage_open_readable_binlog(pReader, \
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
	if (pReader->mark_fd >= 0)
	{
		close(pReader->mark_fd);
		pReader->mark_fd = -1;
	}

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
	int len;
	int result;

	len = sprintf(buff, \
		"%s=%d\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%d\n"  \
		"%s=%d\n"  \
		"%s=%d\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n", \
		MARK_ITEM_BINLOG_FILE_INDEX, pReader->binlog_index, \
		MARK_ITEM_BINLOG_FILE_OFFSET, pReader->binlog_offset, \
		MARK_ITEM_NEED_SYNC_OLD, pReader->need_sync_old, \
		MARK_ITEM_SYNC_OLD_DONE, pReader->sync_old_done, \
		MARK_ITEM_UNTIL_TIMESTAMP, (int)pReader->until_timestamp, \
		MARK_ITEM_SCAN_ROW_COUNT, pReader->scan_row_count, \
		MARK_ITEM_SYNC_ROW_COUNT, pReader->sync_row_count);

	if ((result=storage_write_to_fd(pReader->mark_fd, \
		get_mark_filename_by_reader, pReader, buff, len)) == 0)
	{
		pReader->last_scan_rows = pReader->scan_row_count;
		pReader->last_sync_rows = pReader->sync_row_count;
	}

	return result;
}

static int rewind_to_prev_rec_end(StorageBinLogReader *pReader)
{
	if (lseek(pReader->binlog_fd, pReader->binlog_offset, SEEK_SET) < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"seek binlog file \"%s\"fail, " \
			"file offset: %"PRId64", " \
			"errno: %d, error info: %s", \
			__LINE__, get_binlog_readable_filename(pReader, NULL), \
			pReader->binlog_offset, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	pReader->binlog_buff.current = pReader->binlog_buff.buffer;
	pReader->binlog_buff.length = 0;

	return 0;
}

static int storage_binlog_preread(StorageBinLogReader *pReader)
{
	int bytes_read;
	int saved_binlog_write_version;

	if (pReader->binlog_buff.version == binlog_write_version && \
		pReader->binlog_buff.length == 0)
	{
		return ENOENT;
	}

	saved_binlog_write_version = binlog_write_version;
	if (pReader->binlog_buff.current != pReader->binlog_buff.buffer)
	{
		if (pReader->binlog_buff.length > 0)
		{
			memcpy(pReader->binlog_buff.buffer, \
				pReader->binlog_buff.current, \
				pReader->binlog_buff.length);
		}

		pReader->binlog_buff.current = pReader->binlog_buff.buffer;
	}

	bytes_read = read(pReader->binlog_fd, pReader->binlog_buff.buffer \
		+ pReader->binlog_buff.length, \
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

static int storage_binlog_do_line_read(StorageBinLogReader *pReader, \
		char *line, const int line_size, int *line_length)
{
	char *pLineEnd;

	if (pReader->binlog_buff.length == 0)
	{
		*line_length = 0;
		return ENOENT;
	}

	pLineEnd = (char *)memchr(pReader->binlog_buff.current, '\n', \
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

	result = storage_binlog_do_line_read(pReader, line, \
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

	return storage_binlog_do_line_read(pReader, line, \
			line_size, line_length);
}

int storage_binlog_read(StorageBinLogReader *pReader, \
			StorageBinLogRecord *pRecord, int *record_length)
{
	char line[STORAGE_BINLOG_LINE_SIZE];
	char *cols[3];
	int result;

	while (1)
	{
		result = storage_binlog_read_line(pReader, line, \
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
	if (pRecord->op_type == STORAGE_OP_TYPE_SOURCE_CREATE_LINK || \
	    pRecord->op_type == STORAGE_OP_TYPE_REPLICA_CREATE_LINK)
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
			pRecord->src_filename_len = pRecord->filename_len - \
						(p - pRecord->filename) - 1;
			pRecord->filename_len = p - pRecord->filename;
			*p = '\0';

			memcpy(pRecord->src_filename, p + 1, \
				pRecord->src_filename_len);
			*(pRecord->src_filename + \
				pRecord->src_filename_len) = '\0';
		}
	}
	else
	{
		*(pRecord->src_filename) = '\0';
		pRecord->src_filename_len = 0;
	}

	pRecord->true_filename_len = pRecord->filename_len;
	if ((result=storage_split_filename_ex(pRecord->filename, \
			&pRecord->true_filename_len, pRecord->true_filename, \
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
		result = storage_binlog_read(pReader, \
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

	get_mark_filename_by_id_and_port(old_ip_addr, old_port, \
			old_filename, sizeof(old_filename));
	if (!fileExists(old_filename))
	{
		return ENOENT;
	}

	get_mark_filename_by_id_and_port(new_ip_addr, new_port, \
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

static void storage_sync_thread_exit(ConnectionInfo *pStorage)
{
	int result;
	int i;
	pthread_t tid;

	if ((result=pthread_mutex_lock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	tid = pthread_self();
	for (i=0; i<g_storage_sync_thread_count; i++)
	{
		if (pthread_equal(sync_tids[i], tid))
		{
			break;
		}
	}

	while (i < g_storage_sync_thread_count - 1)
	{
		sync_tids[i] = sync_tids[i + 1];
		i++;
	}
	
	g_storage_sync_thread_count--;

	if ((result=pthread_mutex_unlock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	logDebug("file: "__FILE__", line: %d, " \
		"sync thread to storage server %s:%d exit", 
		__LINE__, pStorage->ip_addr, pStorage->port);
}

static void* storage_sync_thread_entrance(void* arg)
{
	FDFSStorageBrief *pStorage;
	StorageBinLogReader reader;
	StorageBinLogRecord record;
	ConnectionInfo storage_server;
	char local_ip_addr[IP_ADDRESS_SIZE];
	int read_result;
	int sync_result;
	int conn_result;
	int result;
	int record_len;
	int previousCode;
	int nContinuousFail;
	time_t current_time;
	time_t start_time;
	time_t end_time;
	time_t last_keep_alive_time;
	
	memset(local_ip_addr, 0, sizeof(local_ip_addr));
	memset(&reader, 0, sizeof(reader));
	reader.mark_fd = -1;
	reader.binlog_fd = -1;

	current_time =  g_current_time;
	last_keep_alive_time = 0;
	start_time = 0;
	end_time = 0;

	pStorage = (FDFSStorageBrief *)arg;

	strcpy(storage_server.ip_addr, pStorage->ip_addr);
	storage_server.port = g_server_port;
	storage_server.sock = -1;

	logDebug("file: "__FILE__", line: %d, " \
		"sync thread to storage server %s:%d started", \
		__LINE__, storage_server.ip_addr, storage_server.port);

	while (g_continue_flag && \
		pStorage->status != FDFS_STORAGE_STATUS_DELETED && \
		pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED && \
		pStorage->status != FDFS_STORAGE_STATUS_NONE)
	{
		while (g_continue_flag && \
			(pStorage->status == FDFS_STORAGE_STATUS_INIT ||
			 pStorage->status == FDFS_STORAGE_STATUS_OFFLINE ||
			 pStorage->status == FDFS_STORAGE_STATUS_ONLINE))
		{
			sleep(1);
		}

		if ((!g_continue_flag) ||
			pStorage->status == FDFS_STORAGE_STATUS_DELETED || \
			pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED || \
			pStorage->status == FDFS_STORAGE_STATUS_NONE)
		{
			break;
		}

		if (g_sync_part_time)
		{
			current_time = g_current_time;
			storage_sync_get_start_end_times(current_time, \
				&g_sync_end_time, &g_sync_start_time, \
				&start_time, &end_time);
			start_time += 60;
			end_time -= 60;
			while (g_continue_flag && (current_time >= start_time \
					&& current_time <= end_time))
			{
				current_time = g_current_time;
				sleep(1);
			}
		}

		previousCode = 0;
		nContinuousFail = 0;
		conn_result = 0;
		while (g_continue_flag && \
			pStorage->status != FDFS_STORAGE_STATUS_DELETED && \
			pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED && \
			pStorage->status != FDFS_STORAGE_STATUS_NONE)
		{
			strcpy(storage_server.ip_addr, pStorage->ip_addr);
			storage_server.sock = \
				socket(AF_INET, SOCK_STREAM, 0);
			if(storage_server.sock < 0)
			{
				logCrit("file: "__FILE__", line: %d," \
					" socket create fail, " \
					"errno: %d, error info: %s. " \
					"program exit!", __LINE__, \
					errno, STRERROR(errno));
				g_continue_flag = false;
				break;
			}

			if (g_client_bind_addr && *g_bind_addr != '\0')
			{
				socketBind(storage_server.sock, g_bind_addr, 0);
			}

			if (tcpsetnonblockopt(storage_server.sock) != 0)
			{
				nContinuousFail++;
				close(storage_server.sock);
				storage_server.sock = -1;
				sleep(1);

				continue;
			}

			if ((conn_result=connectserverbyip_nb(storage_server.sock,\
				pStorage->ip_addr, g_server_port, \
				g_fdfs_connect_timeout)) == 0)
			{
				char szFailPrompt[64];
				if (nContinuousFail == 0)
				{
					*szFailPrompt = '\0';
				}
				else
				{
					sprintf(szFailPrompt, \
						", continuous fail count: %d", \
						nContinuousFail);
				}
				logInfo("file: "__FILE__", line: %d, " \
					"successfully connect to " \
					"storage server %s:%d%s", __LINE__, \
					pStorage->ip_addr, \
					g_server_port, szFailPrompt);
				nContinuousFail = 0;
				break;
			}

			if (previousCode != conn_result)
			{
				logError("file: "__FILE__", line: %d, " \
					"connect to storage server %s:%d fail" \
					", errno: %d, error info: %s", \
					__LINE__, \
					pStorage->ip_addr, g_server_port, \
					conn_result, STRERROR(conn_result));
				previousCode = conn_result;
			}

			nContinuousFail++;
			close(storage_server.sock);
			storage_server.sock = -1;

			if (!g_continue_flag)
			{
				break;
			}

			sleep(1);
		}

		if (nContinuousFail > 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"connect to storage server %s:%d fail, " \
				"try count: %d, errno: %d, error info: %s", \
				__LINE__, pStorage->ip_addr, \
				g_server_port, nContinuousFail, \
				conn_result, STRERROR(conn_result));
		}

		if ((!g_continue_flag) ||
			pStorage->status == FDFS_STORAGE_STATUS_DELETED || \
			pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED || \
			pStorage->status == FDFS_STORAGE_STATUS_NONE)
		{
			break;
		}

		if (pStorage->status != FDFS_STORAGE_STATUS_ACTIVE && \
			pStorage->status != FDFS_STORAGE_STATUS_WAIT_SYNC && \
			pStorage->status != FDFS_STORAGE_STATUS_SYNCING)
		{
			close(storage_server.sock);
			sleep(5);
			continue;
		}

		if ((result=storage_reader_init(pStorage, &reader)) != 0)
		{
			logCrit("file: "__FILE__", line: %d, " \
				"storage_reader_init fail, errno=%d, " \
				"program exit!", \
				__LINE__, result);
			g_continue_flag = false;
			break;
		}

		if (!reader.need_sync_old)
		{
			while (g_continue_flag && \
			(pStorage->status != FDFS_STORAGE_STATUS_ACTIVE && \
			 pStorage->status != FDFS_STORAGE_STATUS_DELETED && \
			 pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED && \
			 pStorage->status != FDFS_STORAGE_STATUS_NONE))
			{
				sleep(1);
			}

			if (pStorage->status != FDFS_STORAGE_STATUS_ACTIVE)
			{
				close(storage_server.sock);
				storage_reader_destroy(&reader);
				continue;
			}
		}

		getSockIpaddr(storage_server.sock, \
			local_ip_addr, IP_ADDRESS_SIZE);
		insert_into_local_host_ip(local_ip_addr);

		/*
		//printf("file: "__FILE__", line: %d, " \
			"storage_server.ip_addr=%s, " \
			"local_ip_addr: %s\n", \
			__LINE__, pStorage->ip_addr, local_ip_addr);
		*/

		if (is_local_host_ip(pStorage->ip_addr))
		{  //can't self sync to self
			logError("file: "__FILE__", line: %d, " \
				"ip_addr %s belong to the local host," \
				" sync thread exit.", \
				__LINE__, pStorage->ip_addr);
			fdfs_quit(&storage_server);
			close(storage_server.sock);
			break;
		}

		if (storage_report_my_server_id(&storage_server) != 0)
		{
			close(storage_server.sock);
			storage_reader_destroy(&reader);
			sleep(1);
			continue;
		}

		if (pStorage->status == FDFS_STORAGE_STATUS_WAIT_SYNC)
		{
			pStorage->status = FDFS_STORAGE_STATUS_SYNCING;
			storage_report_storage_status(pStorage->id, \
				pStorage->ip_addr, pStorage->status);
		}

		if (pStorage->status == FDFS_STORAGE_STATUS_SYNCING)
		{
			if (reader.need_sync_old && reader.sync_old_done)
			{
				pStorage->status = FDFS_STORAGE_STATUS_OFFLINE;
				storage_report_storage_status(pStorage->id, \
					pStorage->ip_addr, \
					pStorage->status);
			}
		}

		if (g_sync_part_time)
		{
			current_time = g_current_time;
			storage_sync_get_start_end_times(current_time, \
				&g_sync_start_time, &g_sync_end_time, \
				&start_time, &end_time);
		}

		sync_result = 0;
		while (g_continue_flag && (!g_sync_part_time || \
			(current_time >= start_time && \
			current_time <= end_time)) && \
			(pStorage->status == FDFS_STORAGE_STATUS_ACTIVE || \
			pStorage->status == FDFS_STORAGE_STATUS_SYNCING))
		{
			read_result = storage_binlog_read(&reader, \
					&record, &record_len);
			if (read_result == ENOENT)
			{
				if (reader.need_sync_old && \
					!reader.sync_old_done)
				{
				reader.sync_old_done = true;
				if (storage_write_to_mark_file(&reader) != 0)
				{
					logCrit("file: "__FILE__", line: %d, " \
						"storage_write_to_mark_file " \
						"fail, program exit!", \
						__LINE__);
					g_continue_flag = false;
					break;
				}

				if (pStorage->status == \
					FDFS_STORAGE_STATUS_SYNCING)
				{
					pStorage->status = \
						FDFS_STORAGE_STATUS_OFFLINE;
					storage_report_storage_status( \
						pStorage->id, \
						pStorage->ip_addr, \
						pStorage->status);
				}
				}


				if (reader.last_scan_rows!=reader.scan_row_count)
				{
					if (storage_write_to_mark_file(&reader)!=0)
					{
					logCrit("file: "__FILE__", line: %d, " \
						"storage_write_to_mark_file fail, " \
						"program exit!", __LINE__);
					g_continue_flag = false;
					break;
					}
				}

				current_time = g_current_time;
				if (current_time - last_keep_alive_time >= \
					g_heart_beat_interval)
				{
					if (fdfs_active_test(&storage_server)!=0)
					{
						break;
					}

					last_keep_alive_time = current_time;
				}

				usleep(g_sync_wait_usec);
				continue;
			}

			if (g_sync_part_time)
			{
				current_time = g_current_time;
			}

			if (read_result != 0)
			{
			if (read_result == EINVAL && \
				g_file_sync_skip_invalid_record)
			{
				logWarning("file: "__FILE__", line: %d, " \
					"skip invalid record, binlog index: " \
					"%d, offset: %"PRId64, \
					__LINE__, reader.binlog_index, \
					reader.binlog_offset);
			}
			else
			{
				sleep(5);
				break;
			}
			}
			else if ((sync_result=storage_sync_data(&reader, \
				&storage_server, &record)) != 0)
			{
				logDebug("file: "__FILE__", line: %d, " \
					"binlog index: %d, current record " \
					"offset: %"PRId64", next " \
					"record offset: %"PRId64, \
					__LINE__, reader.binlog_index, \
					reader.binlog_offset, \
					reader.binlog_offset + record_len);
				if (rewind_to_prev_rec_end(&reader) != 0)
				{
					logCrit("file: "__FILE__", line: %d, " \
						"rewind_to_prev_rec_end fail, "\
						"program exit!", __LINE__);
					g_continue_flag = false;
				}

				break;
			}

			reader.binlog_offset += record_len;
			reader.scan_row_count++;

			if (g_sync_interval > 0)
			{
				usleep(g_sync_interval);
			}
		}

		if (reader.last_scan_rows != reader.scan_row_count)
		{
			if (storage_write_to_mark_file(&reader) != 0)
			{
				logCrit("file: "__FILE__", line: %d, " \
					"storage_write_to_mark_file fail, " \
					"program exit!", __LINE__);
				g_continue_flag = false;
				break;
			}
		}

		close(storage_server.sock);
		storage_server.sock = -1;
		storage_reader_destroy(&reader);

		if (!g_continue_flag)
		{
			break;
		}

		if (!(sync_result == ENOTCONN || sync_result == EIO))
		{
			sleep(1);
		}
	}

	if (storage_server.sock >= 0)
	{
		close(storage_server.sock);
	}
	storage_reader_destroy(&reader);

	if (pStorage->status == FDFS_STORAGE_STATUS_DELETED
	 || pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED)
	{
		storage_changelog_req();
		sleep(2 * g_heart_beat_interval + 1);
		pStorage->status = FDFS_STORAGE_STATUS_NONE;
	}

	storage_sync_thread_exit(&storage_server);

	return NULL;
}

int storage_sync_thread_start(const FDFSStorageBrief *pStorage)
{
	int result;
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

	if (storage_server_is_myself(pStorage) || \
		is_local_host_ip(pStorage->ip_addr)) //can't self sync to self
	{
		logWarning("file: "__FILE__", line: %d, " \
			"storage id: %s is myself, can't start sync thread!", \
			__LINE__, pStorage->id);
		return 0;
	}

	if ((result=init_pthread_attr(&pattr, g_thread_stack_size)) != 0)
	{
		return result;
	}

	/*
	//printf("start storage ip_addr: %s, g_storage_sync_thread_count=%d\n", 
			pStorage->ip_addr, g_storage_sync_thread_count);
	*/

	if ((result=pthread_create(&tid, &pattr, storage_sync_thread_entrance, \
		(void *)pStorage)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"create thread failed, errno: %d, " \
			"error info: %s", \
			__LINE__, result, STRERROR(result));

		pthread_attr_destroy(&pattr);
		return result;
	}

	if ((result=pthread_mutex_lock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	g_storage_sync_thread_count++;
	sync_tids = (pthread_t *)realloc(sync_tids, sizeof(pthread_t) * \
					g_storage_sync_thread_count);
	if (sync_tids == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(pthread_t) * \
			g_storage_sync_thread_count, \
			errno, STRERROR(errno));
	}
	else
	{
		sync_tids[g_storage_sync_thread_count - 1] = tid;
	}

	if ((result=pthread_mutex_unlock(&sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	pthread_attr_destroy(&pattr);

	return 0;
}

