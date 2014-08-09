/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//trunk_mem.c

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
#include "fdfs_define.h"
#include "logger.h"
#include "sockopt.h"
#include "shared_func.h"
#include "pthread_func.h"
#include "sched_thread.h"
#include "avl_tree.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_service.h"
#include "trunk_sync.h"
#include "storage_dio.h"
#include "trunk_free_block_checker.h"
#include "trunk_mem.h"

#define STORAGE_TRUNK_DATA_FILENAME  "storage_trunk.dat"

#define STORAGE_TRUNK_INIT_FLAG_NONE        0
#define STORAGE_TRUNK_INIT_FLAG_DESTROYING  1
#define STORAGE_TRUNK_INIT_FLAG_DONE        2

int g_slot_min_size;
int g_trunk_file_size;
int g_slot_max_size;
int g_store_path_mode = FDFS_STORE_PATH_ROUND_ROBIN;
FDFSStorageReservedSpace g_storage_reserved_space = {
			TRACKER_STORAGE_RESERVED_SPACE_FLAG_MB};
int g_avg_storage_reserved_mb = FDFS_DEF_STORAGE_RESERVED_MB;
int g_store_path_index = 0;
int g_current_trunk_file_id = 0;
TimeInfo g_trunk_create_file_time_base = {0, 0};
int g_trunk_create_file_interval = 86400;
int g_trunk_compress_binlog_min_interval = 0;
ConnectionInfo g_trunk_server = {-1, 0};
bool g_if_use_trunk_file = false;
bool g_if_trunker_self = false;
bool g_trunk_create_file_advance = false;
bool g_trunk_init_check_occupying = false;
bool g_trunk_init_reload_from_binlog = false;
static byte trunk_init_flag = STORAGE_TRUNK_INIT_FLAG_NONE;
int64_t g_trunk_total_free_space = 0;
int64_t g_trunk_create_file_space_threshold = 0;
time_t g_trunk_last_compress_time = 0;

static pthread_mutex_t trunk_file_lock;
static pthread_mutex_t trunk_mem_lock;
static struct fast_mblock_man free_blocks_man;
static struct fast_mblock_man tree_nodes_man;

static AVLTreeInfo *tree_info_by_sizes = NULL; //for block alloc

static int trunk_create_next_file(FDFSTrunkFullInfo *pTrunkInfo);
static int trunk_add_free_block(FDFSTrunkNode *pNode, const bool bWriteBinLog);

static int trunk_restore_node(const FDFSTrunkFullInfo *pTrunkInfo);
static int trunk_delete_space(const FDFSTrunkFullInfo *pTrunkInfo, \
		const bool bWriteBinLog);

static int storage_trunk_save();
static int storage_trunk_load();

static int trunk_mem_binlog_write(const int timestamp, const char op_type, \
		const FDFSTrunkFullInfo *pTrunk)
{
	pthread_mutex_lock(&trunk_file_lock);
	if (op_type == TRUNK_OP_TYPE_ADD_SPACE)
	{
		g_trunk_total_free_space += pTrunk->file.size;
	}
	else if (op_type == TRUNK_OP_TYPE_DEL_SPACE)
	{
		g_trunk_total_free_space -= pTrunk->file.size;
	}
	pthread_mutex_unlock(&trunk_file_lock);

	return trunk_binlog_write(timestamp, op_type, pTrunk);
}

static int storage_trunk_node_compare_size(void *p1, void *p2)
{
	return ((FDFSTrunkSlot *)p1)->size - ((FDFSTrunkSlot *)p2)->size;
}

static int storage_trunk_node_compare_offset(void *p1, void *p2)
{
	FDFSTrunkFullInfo *pTrunkInfo1;
	FDFSTrunkFullInfo *pTrunkInfo2;
	int result;

	pTrunkInfo1 = &(((FDFSTrunkNode *)p1)->trunk);
	pTrunkInfo2 = &(((FDFSTrunkNode *)p2)->trunk);

	result = memcmp(&(pTrunkInfo1->path), &(pTrunkInfo2->path), \
			sizeof(FDFSTrunkPathInfo));
	if (result != 0)
	{
		return result;
	}

	result = pTrunkInfo1->file.id - pTrunkInfo2->file.id;
	if (result != 0)
	{
		return result;
	}

	return pTrunkInfo1->file.offset - pTrunkInfo2->file.offset;
}

char *storage_trunk_get_data_filename(char *full_filename)
{
	snprintf(full_filename, MAX_PATH_SIZE, "%s/data/%s", \
		g_fdfs_base_path, STORAGE_TRUNK_DATA_FILENAME);
	return full_filename;
}

#define STORAGE_TRUNK_CHECK_STATUS() \
	do \
	{  \
		if (!g_if_trunker_self) \
		{ \
			logError("file: "__FILE__", line: %d, " \
				"I am not trunk server!", __LINE__); \
			return EINVAL; \
		} \
		if (trunk_init_flag != STORAGE_TRUNK_INIT_FLAG_DONE) \
		{ \
			logError("file: "__FILE__", line: %d, " \
				"I am not inited!", __LINE__);  \
			return EINVAL; \
		} \
	} while (0)

int storage_trunk_init()
{
	int result;
	int i;
	int count;

	if (!g_if_trunker_self)
	{
		logError("file: "__FILE__", line: %d, " \
			"I am not trunk server!", __LINE__);
		return 0;
	}

	if (trunk_init_flag != STORAGE_TRUNK_INIT_FLAG_NONE)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"trunk already inited!", __LINE__);
		return 0;
	}

	logDebug("file: "__FILE__", line: %d, " \
		"storage trunk init ...", __LINE__);

	g_trunk_server.sock = -1;
	g_trunk_server.port = g_server_port;

	if ((result=init_pthread_lock(&trunk_file_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"init_pthread_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	if ((result=init_pthread_lock(&trunk_mem_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"init_pthread_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	if ((result=fast_mblock_init(&free_blocks_man, \
			sizeof(FDFSTrunkNode), 0)) != 0)
	{
		return result;
	}

	if ((result=fast_mblock_init(&tree_nodes_man, \
			sizeof(FDFSTrunkSlot), 0)) != 0)
	{
		return result;
	}

	tree_info_by_sizes = (AVLTreeInfo *)malloc(sizeof(AVLTreeInfo) * \
				g_fdfs_store_paths.count);
	if (tree_info_by_sizes == NULL)
	{
		result = errno != 0 ? errno : ENOMEM;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, errno: %d, error info: %s", \
			__LINE__, (int)(sizeof(AVLTreeInfo) * \
			g_fdfs_store_paths.count), result, STRERROR(result));
		return result;
	}

	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		if ((result=avl_tree_init(tree_info_by_sizes + i, NULL, \
			storage_trunk_node_compare_size)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"avl_tree_init fail, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
			return result;
		}
	}

	if ((result=trunk_free_block_checker_init()) != 0)
	{
		return result;
	}

	if ((result=storage_trunk_load()) != 0)
	{
		return result;
	}

	count = 0;
	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		count += avl_tree_count(tree_info_by_sizes + i);
	}

	logInfo("file: "__FILE__", line: %d, " \
		"tree by space size node count: %d, tree by trunk file id " \
		"node count: %d, free block count: %d, " \
		"trunk_total_free_space: %"PRId64, __LINE__, \
		count, trunk_free_block_tree_node_count(), \
		trunk_free_block_total_count(), \
		g_trunk_total_free_space);

	/*
	{
	char filename[MAX_PATH_SIZE];
	sprintf(filename, "%s/logs/tttt.dat", g_fdfs_base_path);
	trunk_free_block_tree_print(filename);
	}
	*/

	trunk_init_flag = STORAGE_TRUNK_INIT_FLAG_DONE;
	return 0;
}

int storage_trunk_destroy_ex(const bool bNeedSleep)
{
	int result;
	int i;

	if (trunk_init_flag != STORAGE_TRUNK_INIT_FLAG_DONE)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"trunk not inited!", __LINE__);
		return 0;
	}

	trunk_init_flag = STORAGE_TRUNK_INIT_FLAG_DESTROYING;
	if (bNeedSleep)
	{
		sleep(1);
	}

	logDebug("file: "__FILE__", line: %d, " \
		"storage trunk destroy", __LINE__);
	result = storage_trunk_save();

	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		avl_tree_destroy(tree_info_by_sizes + i);
	}
	free(tree_info_by_sizes);
	tree_info_by_sizes = NULL;

	trunk_free_block_checker_destroy();

	fast_mblock_destroy(&free_blocks_man);
	fast_mblock_destroy(&tree_nodes_man);
	pthread_mutex_destroy(&trunk_file_lock);
	pthread_mutex_destroy(&trunk_mem_lock);

	trunk_init_flag = STORAGE_TRUNK_INIT_FLAG_NONE;
	return result;
}

static int64_t storage_trunk_get_binlog_size()
{
	char full_filename[MAX_PATH_SIZE];
	struct stat stat_buf;

	get_trunk_binlog_filename(full_filename);
	if (stat(full_filename, &stat_buf) != 0)
	{
		if (errno == ENOENT)
		{
			return 0;
		}

		logError("file: "__FILE__", line: %d, " \
			"stat file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		return -1;
	}

	return stat_buf.st_size;
}

struct walk_callback_args {
	int fd;
	char buff[16 * 1024];
	char temp_trunk_filename[MAX_PATH_SIZE];
	char *pCurrent;
};

static int tree_walk_callback(void *data, void *args)
{
	struct walk_callback_args *pCallbackArgs;
	FDFSTrunkFullInfo *pTrunkInfo;
	FDFSTrunkNode *pCurrent;
	int len;
	int result;

	pCallbackArgs = (struct walk_callback_args *)args;
	pCurrent = ((FDFSTrunkSlot *)data)->head;
	while (pCurrent != NULL)
	{
		pTrunkInfo = &pCurrent->trunk;
		len = sprintf(pCallbackArgs->pCurrent, \
			"%d %c %d %d %d %d %d %d\n", \
			(int)g_current_time, TRUNK_OP_TYPE_ADD_SPACE, \
			pTrunkInfo->path.store_path_index, \
			pTrunkInfo->path.sub_path_high, \
			pTrunkInfo->path.sub_path_low,  \
			pTrunkInfo->file.id, \
			pTrunkInfo->file.offset, \
			pTrunkInfo->file.size);
		pCallbackArgs->pCurrent += len;
		if (pCallbackArgs->pCurrent - pCallbackArgs->buff > \
				sizeof(pCallbackArgs->buff) - 128)
		{
			if (write(pCallbackArgs->fd, pCallbackArgs->buff, \
			    pCallbackArgs->pCurrent - pCallbackArgs->buff) \
			      != pCallbackArgs->pCurrent - pCallbackArgs->buff)
			{
				result = errno != 0 ? errno : EIO;
				logError("file: "__FILE__", line: %d, "\
					"write to file %s fail, " \
					"errno: %d, error info: %s", __LINE__, \
					pCallbackArgs->temp_trunk_filename, \
					result, STRERROR(result));
				return result;
			}

			pCallbackArgs->pCurrent = pCallbackArgs->buff;
		}

		pCurrent = pCurrent->next;
	}

	return 0;
}

static int storage_trunk_do_save()
{
	int64_t trunk_binlog_size;
	char trunk_data_filename[MAX_PATH_SIZE];
	struct walk_callback_args callback_args;
	int len;
	int result;
	int i;

	trunk_binlog_size = storage_trunk_get_binlog_size();
	if (trunk_binlog_size < 0)
	{
		return errno != 0 ? errno : EPERM;
	}

	memset(&callback_args, 0, sizeof(callback_args));
	callback_args.pCurrent = callback_args.buff;

	sprintf(callback_args.temp_trunk_filename, "%s/data/.%s.tmp", \
		g_fdfs_base_path, STORAGE_TRUNK_DATA_FILENAME);
	callback_args.fd = open(callback_args.temp_trunk_filename, \
				O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (callback_args.fd < 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"open file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, callback_args.temp_trunk_filename, \
			result, STRERROR(result));
		return result;
	}

	len = sprintf(callback_args.pCurrent, "%"PRId64"\n", \
			trunk_binlog_size);
	callback_args.pCurrent += len;

	result = 0;
	pthread_mutex_lock(&trunk_mem_lock);
	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		result = avl_tree_walk(tree_info_by_sizes + i, \
				tree_walk_callback, &callback_args);
		if (result != 0)
		{
			break;
		}
	}

	len = callback_args.pCurrent - callback_args.buff;
	if (len > 0 && result == 0)
	{
		if (write(callback_args.fd, callback_args.buff, len) != len)
		{
			result = errno != 0 ? errno : EIO;
			logError("file: "__FILE__", line: %d, "\
				"write to file %s fail, " \
				"errno: %d, error info: %s", \
				__LINE__, callback_args.temp_trunk_filename, \
				result, STRERROR(result));
		}
	}

	if (result == 0 && fsync(callback_args.fd) != 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, "\
			"fsync file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, callback_args.temp_trunk_filename, \
			result, STRERROR(result));
	}

	if (close(callback_args.fd) != 0)
	{
		if (result == 0)
		{
			result = errno != 0 ? errno : EIO;
		}
		logError("file: "__FILE__", line: %d, "\
			"close file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, callback_args.temp_trunk_filename, \
			errno, STRERROR(errno));
	}
	pthread_mutex_unlock(&trunk_mem_lock);

	if (result != 0)
	{
		return result;
	}

	storage_trunk_get_data_filename(trunk_data_filename);
	if (rename(callback_args.temp_trunk_filename, trunk_data_filename) != 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, "\
			"rename file %s to %s fail, " \
			"errno: %d, error info: %s", __LINE__, \
			callback_args.temp_trunk_filename, trunk_data_filename, \
			result, STRERROR(result));
	}

	return result;
}

static int storage_trunk_save()
{
	int result;

	if (!(g_trunk_compress_binlog_min_interval > 0 && \
		g_current_time - g_trunk_last_compress_time >
		g_trunk_compress_binlog_min_interval))
	{
		return storage_trunk_do_save();
	}

	logInfo("start compress trunk binlog ...");
	if ((result=trunk_binlog_compress_apply()) != 0)
	{
		return result;
	}

	if ((result=storage_trunk_do_save()) != 0)
	{
		trunk_binlog_compress_rollback();
		return result;
	}

	if ((result=trunk_binlog_compress_commit()) != 0)
	{
		trunk_binlog_compress_rollback();
		return result;
	}

	g_trunk_last_compress_time = g_current_time;
	storage_write_to_sync_ini_file();

	logInfo("compress trunk binlog done.");
	return trunk_unlink_all_mark_files();  //because the binlog file be compressed
}

static bool storage_trunk_is_space_occupied(const FDFSTrunkFullInfo *pTrunkInfo)
{
	int result;
	int fd;
	char full_filename[MAX_PATH_SIZE+64];

	trunk_get_full_filename(pTrunkInfo, full_filename, sizeof(full_filename));
	fd = open(full_filename, O_RDONLY, 0644);
	if (fd < 0)
	{
		result = errno != 0 ? errno : ENOENT;
		logWarning("file: " __FILE__ ", line: %d, "
			"open file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			result, STRERROR(result));
		return false;
	}

	if (pTrunkInfo->file.offset > 0 && lseek(fd, pTrunkInfo->file.offset, \
			SEEK_SET) < 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"lseek file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			result, STRERROR(result));
		close(fd);
		return false;
	}

	/*
	logInfo("fd: %d, trunk filename: %s, offset: %d", fd, full_filename, \
			pTrunkInfo->file.offset);
	*/
	result = dio_check_trunk_file_ex(fd, full_filename, \
			pTrunkInfo->file.offset);
	close(fd);
	return (result == EEXIST);
}

static int trunk_add_space_by_trunk(const FDFSTrunkFullInfo *pTrunkInfo)
{
	int result;

	result = trunk_free_space(pTrunkInfo, false);
	if (result == 0 || result == EEXIST)
	{
		return 0;
	}
	else
	{
		return result;
	}
}

static int trunk_add_space_by_node(FDFSTrunkNode *pTrunkNode)
{
	int result;

	if (pTrunkNode->trunk.file.size < g_slot_min_size)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"space: %d is too small, do not need recycle!", \
			__LINE__, pTrunkNode->trunk.file.size);
		fast_mblock_free(&free_blocks_man, pTrunkNode->pMblockNode);
		return 0;
	}

	result = trunk_add_free_block(pTrunkNode, false);
	if (result == 0)
	{
		return 0;
	}
	else
	{
		fast_mblock_free(&free_blocks_man, pTrunkNode->pMblockNode);
		return (result == EEXIST) ? 0 : result;
	}
}

static int storage_trunk_do_add_space(const FDFSTrunkFullInfo *pTrunkInfo)
{
	if (g_trunk_init_check_occupying)
	{
		if (storage_trunk_is_space_occupied(pTrunkInfo))
		{
			return 0;
		}
	}

	/*	
	{
	char buff[256];
	trunk_info_dump(pTrunkInfo, buff, sizeof(buff));
	logInfo("add trunk info: %s", buff);
	}
	*/

	return trunk_add_space_by_trunk(pTrunkInfo);
}

static void storage_trunk_free_node(void *ptr)
{
	fast_mblock_free(&free_blocks_man, \
		((FDFSTrunkNode *)ptr)->pMblockNode);
}

static int storage_trunk_add_free_blocks_callback(void *data, void *args)
{
	/*
	char buff[256];
	logInfo("file: "__FILE__", line: %d"\
		", adding trunk info: %s", __LINE__, \
		trunk_info_dump(&(((FDFSTrunkNode *)data)->trunk), \
		buff, sizeof(buff)));
	*/
	return trunk_add_space_by_node((FDFSTrunkNode *)data);
}

static int storage_trunk_restore(const int64_t restore_offset)
{
	int64_t trunk_binlog_size;
	int64_t line_count;
	TrunkBinLogReader reader;
	TrunkBinLogRecord record;
	char trunk_mark_filename[MAX_PATH_SIZE];
	char buff[256];
	int record_length;
	int result;
	AVLTreeInfo tree_info_by_offset;
	struct fast_mblock_node *pMblockNode;
	FDFSTrunkNode *pTrunkNode;
	FDFSTrunkNode trunkNode;
	bool trunk_init_reload_from_binlog;

	trunk_binlog_size = storage_trunk_get_binlog_size();
	if (trunk_binlog_size < 0)
	{
		return errno != 0 ? errno : EPERM;
	}

	if (restore_offset == trunk_binlog_size)
	{
		return 0;
	}

	if (restore_offset > trunk_binlog_size)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"restore_offset: %"PRId64 \
			" > trunk_binlog_size: %"PRId64, \
			__LINE__, restore_offset, trunk_binlog_size);
		return storage_trunk_save();
	}

	logDebug("file: "__FILE__", line: %d, " \
		"trunk metadata recovering, start offset: " \
		"%"PRId64", need recovery binlog bytes: " \
		"%"PRId64, __LINE__, \
		restore_offset, trunk_binlog_size - restore_offset);

	trunk_init_reload_from_binlog = (restore_offset == 0);
	if (trunk_init_reload_from_binlog)
	{
		memset(&trunkNode, 0, sizeof(trunkNode));
		if ((result=avl_tree_init(&tree_info_by_offset, \
			storage_trunk_free_node, \
			storage_trunk_node_compare_offset)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"avl_tree_init fail, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
			return result;
		}
	}

	memset(&record, 0, sizeof(record));
	memset(&reader, 0, sizeof(reader));
	reader.binlog_offset = restore_offset;
	if ((result=trunk_reader_init(NULL, &reader)) != 0)
	{
		return result;
	}

	line_count = 0;
	while (1)
	{
		record_length = 0;
		result = trunk_binlog_read(&reader, &record, &record_length);
		if (result != 0)
		{
			if (result == ENOENT)
			{
				if (record_length > 0)  //skip incorrect record
				{
					line_count++;
					reader.binlog_offset += record_length;
					continue;
				}

				result = (reader.binlog_offset >= \
					trunk_binlog_size) ? 0 : EINVAL;
				if (result != 0)
				{
				logError("file: "__FILE__", line: %d, " \
					"binlog offset: %"PRId64 \
					" < binlog size: %"PRId64 \
					", please check the end of trunk " \
					"binlog", __LINE__, \
					reader.binlog_offset, trunk_binlog_size);
				}
			}
		
			break;
		}

		line_count++;
		if (record.op_type == TRUNK_OP_TYPE_ADD_SPACE)
		{
			record.trunk.status = FDFS_TRUNK_STATUS_FREE;

			if (trunk_init_reload_from_binlog)
			{
				pMblockNode = fast_mblock_alloc(&free_blocks_man);
				if (pMblockNode == NULL)
				{
					result = errno != 0 ? errno : EIO;
					logError("file: "__FILE__", line: %d, "\
						"malloc %d bytes fail, " \
						"errno: %d, error info: %s", \
						__LINE__, \
						(int)sizeof(FDFSTrunkNode), \
						result, STRERROR(result));
					return result;
				}

				pTrunkNode = (FDFSTrunkNode *)pMblockNode->data;
				memcpy(&pTrunkNode->trunk, &(record.trunk), \
					sizeof(FDFSTrunkFullInfo));

				pTrunkNode->pMblockNode = pMblockNode;
				pTrunkNode->next = NULL;
				result = avl_tree_insert(&tree_info_by_offset,\
							pTrunkNode);
				if (result < 0) //error
				{
					result *= -1;
					logError("file: "__FILE__", line: %d, "\
						"avl_tree_insert fail, " \
						"errno: %d, error info: %s", \
						__LINE__, result, \
						STRERROR(result));
					return result;
				}
				else if (result == 0)
				{
					trunk_info_dump(&(record.trunk), \
							buff, sizeof(buff));
					logWarning("file: "__FILE__", line: %d"\
						", trunk data line: " \
						"%"PRId64", trunk "\
						"space already exist, "\
						"trunk info: %s", \
						__LINE__, line_count, buff);
				}
			}
			else if ((result=trunk_add_space_by_trunk( \
						&record.trunk)) != 0)
			{
				break;
			}
		}
		else if (record.op_type == TRUNK_OP_TYPE_DEL_SPACE)
		{
			record.trunk.status = FDFS_TRUNK_STATUS_FREE;
			if (trunk_init_reload_from_binlog)
			{
				memcpy(&trunkNode.trunk, &record.trunk, \
					sizeof(FDFSTrunkFullInfo));
				if (avl_tree_delete(&tree_info_by_offset,\
							&trunkNode) != 1)
				{
				trunk_info_dump(&(record.trunk), \
						buff, sizeof(buff));
				logWarning("file: "__FILE__", line: %d"\
					", binlog offset: %"PRId64 \
					", trunk data line: %"PRId64 \
					" trunk node not exist, " \
					"trunk info: %s", __LINE__, \
					reader.binlog_offset, \
					line_count, buff);
				}
			}
			else if ((result=trunk_delete_space( \
						&record.trunk, false)) != 0)
			{
				if (result == ENOENT)
				{
				logDebug("file: "__FILE__", line: %d, "\
					"binlog offset: %"PRId64 \
					", trunk data line: %"PRId64,\
					__LINE__, reader.binlog_offset, \
					line_count);

					result = 0;
				}
				else
				{
					break;
				}
			}
		}

		reader.binlog_offset += record_length;
	}

	trunk_reader_destroy(&reader);
	trunk_mark_filename_by_reader(&reader, trunk_mark_filename);
	if (unlink(trunk_mark_filename) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"unlink file %s fail, " \
			"errno: %d, error info: %s", __LINE__, \
			trunk_mark_filename, errno, STRERROR(errno));
	}

	if (result != 0)
	{
		if (trunk_init_reload_from_binlog)
		{
			avl_tree_destroy(&tree_info_by_offset);
		}

		logError("file: "__FILE__", line: %d, " \
			"trunk load fail, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	if (trunk_init_reload_from_binlog)
	{
		logInfo("file: "__FILE__", line: %d, " \
			"free tree node count: %d", \
			__LINE__, avl_tree_count(&tree_info_by_offset));

		result = avl_tree_walk(&tree_info_by_offset, \
				storage_trunk_add_free_blocks_callback, NULL);

		tree_info_by_offset.free_data_func = NULL;
		avl_tree_destroy(&tree_info_by_offset);
	}

	if (result == 0)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"trunk metadata recovery done. start offset: " \
			"%"PRId64", recovery file size: " \
			"%"PRId64, __LINE__, \
			restore_offset, trunk_binlog_size - restore_offset);
		return storage_trunk_save();
	}

	return result;
}

int storage_delete_trunk_data_file()
{
	char trunk_data_filename[MAX_PATH_SIZE];
	int result;

	storage_trunk_get_data_filename(trunk_data_filename);
	if (unlink(trunk_data_filename) == 0)
	{
		return 0;
	}

	result = errno != 0 ? errno : ENOENT;
	if (result != ENOENT)
	{
		logError("file: "__FILE__", line: %d, " \
			"unlink trunk data file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, trunk_data_filename, \
			result, STRERROR(result));
	}

	return result;
}

static int storage_trunk_load()
{
#define TRUNK_DATA_NEW_FIELD_COUNT  8  // >= v5.01
#define TRUNK_DATA_OLD_FIELD_COUNT  6  // < V5.01
#define TRUNK_LINE_MAX_LENGHT  64

	int64_t restore_offset;
	char trunk_data_filename[MAX_PATH_SIZE];
	char buff[4 * 1024 + 1];
	int line_count;
	int col_count;
	int index;
	char *pLineStart;
	char *pLineEnd;
	char *cols[TRUNK_DATA_NEW_FIELD_COUNT];
	FDFSTrunkFullInfo trunkInfo;
	int result;
	int fd;
	int bytes;
	int len;

	storage_trunk_get_data_filename(trunk_data_filename);
	if (g_trunk_init_reload_from_binlog)
	{
		if (unlink(trunk_data_filename) != 0)
		{
			result = errno != 0 ? errno : ENOENT;
			if (result != ENOENT)
			{
				logError("file: "__FILE__", line: %d, " \
					"unlink file %s fail, " \
					"errno: %d, error info: %s", __LINE__, \
					trunk_data_filename, result, \
					STRERROR(result));
				return result;
			}
		}

		restore_offset = 0;
		return storage_trunk_restore(restore_offset);
	}

	fd = open(trunk_data_filename, O_RDONLY);
	if (fd < 0)
	{
		result = errno != 0 ? errno : EIO;
		if (result == ENOENT)
		{
			restore_offset = 0;
			return storage_trunk_restore(restore_offset);
		}

		logError("file: "__FILE__", line: %d, " \
			"open file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, trunk_data_filename, \
			result, STRERROR(result));
		return result;
	}

	if ((bytes=read(fd, buff, sizeof(buff) - 1)) < 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"read from file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, trunk_data_filename, \
			result, STRERROR(result));
		close(fd);
		return result;
	}

	*(buff + bytes) = '\0';
	pLineEnd = strchr(buff, '\n');
	if (pLineEnd == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"read offset from file %s fail", \
			__LINE__, trunk_data_filename);
		close(fd);
		return EINVAL;
	}

	*pLineEnd = '\0';
	restore_offset = strtoll(buff, NULL, 10);
	pLineStart = pLineEnd + 1;  //skip \n
	line_count = 0;
	while (1)
	{
		pLineEnd = strchr(pLineStart, '\n');
		if (pLineEnd == NULL)
		{
			if (bytes < sizeof(buff) - 1) //EOF
			{
				break;
			}

			len = strlen(pLineStart);
			if (len > TRUNK_LINE_MAX_LENGHT)
			{
				logError("file: "__FILE__", line: %d, " \
					"file %s, line length: %d too long", \
					__LINE__, trunk_data_filename, len);
				close(fd);
				return EINVAL;
			}

			memcpy(buff, pLineStart, len);
			if ((bytes=read(fd, buff + len, sizeof(buff) \
					- len - 1)) < 0)
			{
				result = errno != 0 ? errno : EIO;
				logError("file: "__FILE__", line: %d, " \
					"read from file %s fail, " \
					"errno: %d, error info: %s", \
					__LINE__, trunk_data_filename, \
					result, STRERROR(result));
				close(fd);
				return result;
			}

			if (bytes == 0)
			{
				result = ENOENT;
				logError("file: "__FILE__", line: %d, " \
					"file: %s, end of file, expect " \
					"end line", __LINE__, \
					trunk_data_filename);
				close(fd);
				return result;
			}

			bytes += len;
			*(buff + bytes) = '\0';
			pLineStart = buff;
			continue;
		}

		++line_count;
		*pLineEnd = '\0';
		col_count = splitEx(pLineStart, ' ', cols,
				TRUNK_DATA_NEW_FIELD_COUNT);
		if (col_count != TRUNK_DATA_NEW_FIELD_COUNT && \
			col_count != TRUNK_DATA_OLD_FIELD_COUNT)
		{
			logError("file: "__FILE__", line: %d, " \
				"file %s, line: %d is invalid", \
				__LINE__, trunk_data_filename, line_count);
			close(fd);
			return EINVAL;
		}

		if (col_count == TRUNK_DATA_OLD_FIELD_COUNT)
		{
			index = 0;
		}
		else
		{
			index = 2;
		}
		trunkInfo.path.store_path_index = atoi(cols[index++]);
		trunkInfo.path.sub_path_high = atoi(cols[index++]);
		trunkInfo.path.sub_path_low = atoi(cols[index++]);
		trunkInfo.file.id = atoi(cols[index++]);
		trunkInfo.file.offset = atoi(cols[index++]);
		trunkInfo.file.size = atoi(cols[index++]);
		if ((result=storage_trunk_do_add_space(&trunkInfo)) != 0)
		{
			close(fd);
			return result;
		}

		pLineStart = pLineEnd + 1;  //next line
	}

	close(fd);

	if (*pLineStart != '\0')
	{
		logError("file: "__FILE__", line: %d, " \
			"file %s does not end correctly", \
			__LINE__, trunk_data_filename);
		return EINVAL;
	}

	logDebug("file: "__FILE__", line: %d, " \
		"file %s, line count: %d", \
		__LINE__, trunk_data_filename, line_count);

	return storage_trunk_restore(restore_offset);
}

int trunk_free_space(const FDFSTrunkFullInfo *pTrunkInfo, \
		const bool bWriteBinLog)
{
	int result;
	struct fast_mblock_node *pMblockNode;
	FDFSTrunkNode *pTrunkNode;

	if (!g_if_trunker_self)
	{
		logError("file: "__FILE__", line: %d, " \
			"I am not trunk server!", __LINE__);
		return EINVAL;
	}

	if (trunk_init_flag != STORAGE_TRUNK_INIT_FLAG_DONE)
	{
		if (bWriteBinLog)
		{
			logError("file: "__FILE__", line: %d, " \
				"I am not inited!", __LINE__);
			return EINVAL;
		}
	}

	if (pTrunkInfo->file.size < g_slot_min_size)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"space: %d is too small, do not need recycle!", \
			__LINE__, pTrunkInfo->file.size);
		return 0;
	}

	pMblockNode = fast_mblock_alloc(&free_blocks_man);
	if (pMblockNode == NULL)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(FDFSTrunkNode), \
			result, STRERROR(result));
		return result;
	}

	pTrunkNode = (FDFSTrunkNode *)pMblockNode->data;
	memcpy(&pTrunkNode->trunk, pTrunkInfo, sizeof(FDFSTrunkFullInfo));

	pTrunkNode->pMblockNode = pMblockNode;
	pTrunkNode->trunk.status = FDFS_TRUNK_STATUS_FREE;
	pTrunkNode->next = NULL;
	return trunk_add_free_block(pTrunkNode, bWriteBinLog);
}

static int trunk_add_free_block(FDFSTrunkNode *pNode, const bool bWriteBinLog)
{
	int result;
	struct fast_mblock_node *pMblockNode;
	FDFSTrunkSlot target_slot;
	FDFSTrunkSlot *chain;

	pthread_mutex_lock(&trunk_mem_lock);

	if ((result=trunk_free_block_check_duplicate(&(pNode->trunk))) != 0)
	{
		pthread_mutex_unlock(&trunk_mem_lock);
		return result;
	}

	target_slot.size = pNode->trunk.file.size;
	target_slot.head = NULL;
	chain = (FDFSTrunkSlot *)avl_tree_find(tree_info_by_sizes +  \
			pNode->trunk.path.store_path_index, &target_slot);
	if (chain == NULL)
	{
		pMblockNode = fast_mblock_alloc(&tree_nodes_man);
		if (pMblockNode == NULL)
		{
			result = errno != 0 ? errno : EIO;
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", \
				__LINE__, (int)sizeof(FDFSTrunkSlot), \
				result, STRERROR(result));
			pthread_mutex_unlock(&trunk_mem_lock);
			return result;
		}

		chain = (FDFSTrunkSlot *)pMblockNode->data;
		chain->pMblockNode = pMblockNode;
		chain->size = pNode->trunk.file.size;
		pNode->next = NULL;
		chain->head = pNode;

		if (avl_tree_insert(tree_info_by_sizes + pNode->trunk. \
				path.store_path_index, chain) != 1)
		{
			result = errno != 0 ? errno : ENOMEM;
			logError("file: "__FILE__", line: %d, " \
				"avl_tree_insert fail, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
			pthread_mutex_unlock(&trunk_mem_lock);
			return result;
		}
	}
	else
	{
		pNode->next = chain->head;
		chain->head = pNode;
	}

	if (bWriteBinLog)
	{
		result = trunk_mem_binlog_write(g_current_time, \
				TRUNK_OP_TYPE_ADD_SPACE, &(pNode->trunk));
	}
	else
	{
		pthread_mutex_lock(&trunk_file_lock);
		g_trunk_total_free_space += pNode->trunk.file.size;
		pthread_mutex_unlock(&trunk_file_lock);
		result = 0;
	}

	if (result == 0)
	{
		result = trunk_free_block_insert(&(pNode->trunk));
	}
	else
	{
		trunk_free_block_insert(&(pNode->trunk));
	}

	pthread_mutex_unlock(&trunk_mem_lock);

	return result;
}

static void trunk_delete_size_tree_entry(const int store_path_index, \
		FDFSTrunkSlot *pSlot)
{
	if (avl_tree_delete(tree_info_by_sizes + store_path_index, pSlot) == 1)
	{
		fast_mblock_free(&tree_nodes_man, \
				pSlot->pMblockNode);
	}
	else
	{
		logWarning("file: "__FILE__", line: %d, " \
			"can't delete slot entry, size: %d", \
			__LINE__, pSlot->size);
	}
}

static int trunk_delete_space(const FDFSTrunkFullInfo *pTrunkInfo, \
		const bool bWriteBinLog)
{
	int result;
	FDFSTrunkSlot target_slot;
	char buff[256];
	FDFSTrunkSlot *pSlot;
	FDFSTrunkNode *pPrevious;
	FDFSTrunkNode *pCurrent;

	target_slot.size = pTrunkInfo->file.size;
	target_slot.head = NULL;

	pthread_mutex_lock(&trunk_mem_lock);
	pSlot = (FDFSTrunkSlot *)avl_tree_find(tree_info_by_sizes + \
			pTrunkInfo->path.store_path_index, &target_slot);
	if (pSlot == NULL)
	{
		pthread_mutex_unlock(&trunk_mem_lock);
		logError("file: "__FILE__", line: %d, " \
			"can't find trunk entry: %s", __LINE__, \
			trunk_info_dump(pTrunkInfo, buff, sizeof(buff)));
		return ENOENT;
	}

	pPrevious = NULL;
	pCurrent = pSlot->head;
	while (pCurrent != NULL && memcmp(&(pCurrent->trunk), pTrunkInfo, \
		sizeof(FDFSTrunkFullInfo)) != 0)
	{
		pPrevious = pCurrent;
		pCurrent = pCurrent->next;
	}

	if (pCurrent == NULL)
	{
		pthread_mutex_unlock(&trunk_mem_lock);
		logError("file: "__FILE__", line: %d, " \
			"can't find trunk entry: %s", __LINE__, \
			trunk_info_dump(pTrunkInfo, buff, sizeof(buff)));
		return ENOENT;
	}

	if (pPrevious == NULL)
	{
		pSlot->head = pCurrent->next;
		if (pSlot->head == NULL)
		{
			trunk_delete_size_tree_entry(pTrunkInfo->path. \
					store_path_index, pSlot);
		}
	}
	else
	{
		pPrevious->next = pCurrent->next;
	}

	trunk_free_block_delete(&(pCurrent->trunk));
	pthread_mutex_unlock(&trunk_mem_lock);

	if (bWriteBinLog)
	{
		result = trunk_mem_binlog_write(g_current_time, \
				TRUNK_OP_TYPE_DEL_SPACE, &(pCurrent->trunk));
	}
	else
	{
		pthread_mutex_lock(&trunk_file_lock);
		g_trunk_total_free_space -= pCurrent->trunk.file.size;
		pthread_mutex_unlock(&trunk_file_lock);
		result = 0;
	}

	fast_mblock_free(&free_blocks_man, pCurrent->pMblockNode);
	return result;
}

static int trunk_restore_node(const FDFSTrunkFullInfo *pTrunkInfo)
{
	FDFSTrunkSlot target_slot;
	char buff[256];
	FDFSTrunkSlot *pSlot;
	FDFSTrunkNode *pCurrent;

	target_slot.size = pTrunkInfo->file.size;
	target_slot.head = NULL;

	pthread_mutex_lock(&trunk_mem_lock);
	pSlot = (FDFSTrunkSlot *)avl_tree_find(tree_info_by_sizes + \
			pTrunkInfo->path.store_path_index, &target_slot);
	if (pSlot == NULL)
	{
		pthread_mutex_unlock(&trunk_mem_lock);
		logError("file: "__FILE__", line: %d, " \
			"can't find trunk entry: %s", __LINE__, \
			trunk_info_dump(pTrunkInfo, buff, sizeof(buff)));
		return ENOENT;
	}

	pCurrent = pSlot->head;
	while (pCurrent != NULL && memcmp(&(pCurrent->trunk), \
		pTrunkInfo, sizeof(FDFSTrunkFullInfo)) != 0)
	{
		pCurrent = pCurrent->next;
	}

	if (pCurrent == NULL)
	{
		pthread_mutex_unlock(&trunk_mem_lock);

		logError("file: "__FILE__", line: %d, " \
			"can't find trunk entry: %s", __LINE__, \
			trunk_info_dump(pTrunkInfo, buff, sizeof(buff)));
		return ENOENT;
	}

	pCurrent->trunk.status = FDFS_TRUNK_STATUS_FREE;
	pthread_mutex_unlock(&trunk_mem_lock);

	return 0;
}

static int trunk_split(FDFSTrunkNode *pNode, const int size)
{
	int result;
	struct fast_mblock_node *pMblockNode;
	FDFSTrunkNode *pTrunkNode;

	if (pNode->trunk.file.size - size < g_slot_min_size)
	{
		return trunk_mem_binlog_write(g_current_time, \
			TRUNK_OP_TYPE_DEL_SPACE, &(pNode->trunk));
	}

	pMblockNode = fast_mblock_alloc(&free_blocks_man);
	if (pMblockNode == NULL)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(FDFSTrunkNode), \
			result, STRERROR(result));
		return result;
	}

	result = trunk_mem_binlog_write(g_current_time, \
			TRUNK_OP_TYPE_DEL_SPACE, &(pNode->trunk));
	if (result != 0)
	{
		fast_mblock_free(&free_blocks_man, pMblockNode);
		return result;
	}

	pTrunkNode = (FDFSTrunkNode *)pMblockNode->data;
	memcpy(pTrunkNode, pNode, sizeof(FDFSTrunkNode));

	pTrunkNode->pMblockNode = pMblockNode;
	pTrunkNode->trunk.file.offset = pNode->trunk.file.offset + size;
	pTrunkNode->trunk.file.size = pNode->trunk.file.size - size;
	pTrunkNode->trunk.status = FDFS_TRUNK_STATUS_FREE;
	pTrunkNode->next = NULL;

	result = trunk_add_free_block(pTrunkNode, true);
	if (result != 0)
	{
		return result;
	}

	pNode->trunk.file.size = size;
	return 0;
}

static FDFSTrunkNode *trunk_create_trunk_file(const int store_path_index, \
			int *err_no)
{
	FDFSTrunkNode *pTrunkNode;
	struct fast_mblock_node *pMblockNode;

	pMblockNode = fast_mblock_alloc(&free_blocks_man);
	if (pMblockNode == NULL)
	{
		*err_no = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(FDFSTrunkNode), \
			*err_no, STRERROR(*err_no));
		return NULL;
	}

	pTrunkNode = (FDFSTrunkNode *)pMblockNode->data;
	pTrunkNode->pMblockNode = pMblockNode;

	if (store_path_index >= 0)
	{
		pTrunkNode->trunk.path.store_path_index = store_path_index;
	}
	else
	{
		int result;
		int new_store_path_index;
		if ((result=storage_get_storage_path_index( \
				&new_store_path_index)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"get_storage_path_index fail, " \
				"errno: %d, error info: %s", __LINE__, \
				result, STRERROR(result));
			return NULL;
		}
		pTrunkNode->trunk.path.store_path_index = new_store_path_index;
	}

	pTrunkNode->trunk.file.offset = 0;
	pTrunkNode->trunk.file.size = g_trunk_file_size;
	pTrunkNode->trunk.status = FDFS_TRUNK_STATUS_FREE;
	pTrunkNode->next = NULL;

	*err_no = trunk_create_next_file(&(pTrunkNode->trunk));
	if (*err_no != 0)
	{
		fast_mblock_free(&free_blocks_man, pMblockNode);
		return NULL;
	}

	*err_no = trunk_mem_binlog_write(g_current_time, \
			TRUNK_OP_TYPE_ADD_SPACE, &(pTrunkNode->trunk));
	return pTrunkNode;
}

int trunk_alloc_space(const int size, FDFSTrunkFullInfo *pResult)
{
	FDFSTrunkSlot target_slot;
	FDFSTrunkSlot *pSlot;
	FDFSTrunkNode *pPreviousNode;
	FDFSTrunkNode *pTrunkNode;
	int result;

	STORAGE_TRUNK_CHECK_STATUS();

	target_slot.size = (size > g_slot_min_size) ? size : g_slot_min_size;
	target_slot.head = NULL;

	pPreviousNode = NULL;
	pTrunkNode = NULL;
	pthread_mutex_lock(&trunk_mem_lock);
	while (1)
	{
		pSlot = (FDFSTrunkSlot *)avl_tree_find_ge(tree_info_by_sizes \
			 + pResult->path.store_path_index, &target_slot);
		if (pSlot == NULL)
		{
			break;
		}

		pPreviousNode = NULL;
		pTrunkNode = pSlot->head;
		while (pTrunkNode != NULL && \
			pTrunkNode->trunk.status == FDFS_TRUNK_STATUS_HOLD)
		{
			pPreviousNode = pTrunkNode;
			pTrunkNode = pTrunkNode->next;
		}

		if (pTrunkNode != NULL)
		{
			break;
		}

		target_slot.size = pSlot->size + 1;
	}

	if (pTrunkNode != NULL)
	{
		if (pPreviousNode == NULL)
		{
			pSlot->head = pTrunkNode->next;
			if (pSlot->head == NULL)
			{
				trunk_delete_size_tree_entry(pResult->path. \
					store_path_index, pSlot);
			}
		}
		else
		{
			pPreviousNode->next = pTrunkNode->next;
		}

		trunk_free_block_delete(&(pTrunkNode->trunk));
	}
	else
	{
		pTrunkNode = trunk_create_trunk_file(pResult->path. \
					store_path_index, &result);
		if (pTrunkNode == NULL)
		{
			pthread_mutex_unlock(&trunk_mem_lock);
			return result;
		}
	}
	pthread_mutex_unlock(&trunk_mem_lock);

	result = trunk_split(pTrunkNode, size);
	if (result != 0)
	{
		return result;
	}

	pTrunkNode->trunk.status = FDFS_TRUNK_STATUS_HOLD;
	result = trunk_add_free_block(pTrunkNode, true);
	if (result == 0)
	{
		memcpy(pResult, &(pTrunkNode->trunk), \
			sizeof(FDFSTrunkFullInfo));
	}

	return result;
}

int trunk_alloc_confirm(const FDFSTrunkFullInfo *pTrunkInfo, const int status)
{
	FDFSTrunkFullInfo target_trunk_info;

	STORAGE_TRUNK_CHECK_STATUS();

	memset(&target_trunk_info, 0, sizeof(FDFSTrunkFullInfo));
	target_trunk_info.status = FDFS_TRUNK_STATUS_HOLD;
	target_trunk_info.path.store_path_index = \
			pTrunkInfo->path.store_path_index;
	target_trunk_info.path.sub_path_high = pTrunkInfo->path.sub_path_high;
	target_trunk_info.path.sub_path_low = pTrunkInfo->path.sub_path_low;
	target_trunk_info.file.id = pTrunkInfo->file.id;
	target_trunk_info.file.offset = pTrunkInfo->file.offset;
	target_trunk_info.file.size = pTrunkInfo->file.size;

	if (status == 0)
	{
		return trunk_delete_space(&target_trunk_info, true);
	}
	else if (status == EEXIST)
	{
		char buff[256];
		trunk_info_dump(&target_trunk_info, buff, sizeof(buff));

		logWarning("file: "__FILE__", line: %d, " \
			"trunk space already be occupied, " \
			"delete this trunk space, trunk info: %s", \
			__LINE__, buff);
		return trunk_delete_space(&target_trunk_info, true);
	}
	else
	{
		return trunk_restore_node(&target_trunk_info);
	}
}

static int trunk_create_next_file(FDFSTrunkFullInfo *pTrunkInfo)
{
	char buff[32];
	int result;
	int filename_len;
	char short_filename[64];
	char full_filename[MAX_PATH_SIZE];
	int sub_path_high;
	int sub_path_low;

	while (1)
	{
		pthread_mutex_lock(&trunk_file_lock);
		pTrunkInfo->file.id = ++g_current_trunk_file_id;
		result = storage_write_to_sync_ini_file();
		pthread_mutex_unlock(&trunk_file_lock);
		if (result != 0)
		{
			return result;
		}

		int2buff(pTrunkInfo->file.id, buff);
		base64_encode_ex(&g_fdfs_base64_context, buff, sizeof(int), \
				short_filename, &filename_len, false);

		storage_get_store_path(short_filename, filename_len, \
					&sub_path_high, &sub_path_low);

		pTrunkInfo->path.sub_path_high = sub_path_high;
		pTrunkInfo->path.sub_path_low = sub_path_low;

		trunk_get_full_filename(pTrunkInfo, full_filename, \
			sizeof(full_filename));
		if (!fileExists(full_filename))
		{
			break;
		}
	}

	if ((result=trunk_init_file(full_filename)) != 0)
	{
		return result;
	}

	return 0;
}

static int trunk_wait_file_ready(const char *filename, const int64_t file_size, 
		const bool log_when_no_ent)
{
	struct stat file_stat;
	time_t file_mtime;
	int result;

	if (stat(filename, &file_stat) != 0)
	{
		result = errno != 0 ? errno : ENOENT;
		if (log_when_no_ent || result != ENOENT)
		{
			logError("file: "__FILE__", line: %d, " \
				"stat file %s fail, " \
				"errno: %d, error info: %s", \
				__LINE__, filename, \
				result, STRERROR(result));
		}
		return result;
	}

	file_mtime = file_stat.st_mtime;
	while (1)
	{
		if (file_stat.st_size >= file_size)
		{
			return 0;
		}

		if (abs(g_current_time - file_mtime) > 10)
		{
			return ETIMEDOUT;
		}

		usleep(5 * 1000);

		if (stat(filename, &file_stat) != 0)
		{
			result = errno != 0 ? errno : ENOENT;
			if (log_when_no_ent || result != ENOENT)
			{
				logError("file: "__FILE__", line: %d, " \
					"stat file %s fail, " \
					"errno: %d, error info: %s", \
					__LINE__, filename, \
					result, STRERROR(result));
			}
			return result;
		}
	}

	return 0;
}

int trunk_init_file_ex(const char *filename, const int64_t file_size)
{
	int fd;
	int result;

	fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (fd < 0)
	{
		result = errno != 0 ? errno : EEXIST;
		if (result == EEXIST) //already created by another dio thread
		{
			logDebug("file: "__FILE__", line: %d, " \
				"waiting for trunk file: %s " \
				"ready ...", __LINE__, filename);

			result = trunk_wait_file_ready(filename, file_size, true);
			if (result == ETIMEDOUT)
			{
				logError("file: "__FILE__", line: %d, " \
					"waiting for trunk file: %s " \
					"ready timeout!", __LINE__, filename);
			}

			logDebug("file: "__FILE__", line: %d, " \
				"waiting for trunk file: %s " \
				"done.", __LINE__, filename);
			return result;
		}

		logError("file: "__FILE__", line: %d, " \
			"open file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, filename, \
			result, STRERROR(result));
		return result;
	}

	if (ftruncate(fd, file_size) == 0)
	{
		result = 0;
	}
	else
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"ftruncate file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, filename, \
			result, STRERROR(result));
	}

	close(fd);
	return result;
}

int trunk_check_and_init_file_ex(const char *filename, const int64_t file_size)
{
	struct stat file_stat;
	int fd;
	int result;
	
	result = trunk_wait_file_ready(filename, file_size, false);
	if (result == 0)
	{
		return 0;
	}
	if (result == ENOENT)
	{
		return trunk_init_file_ex(filename, file_size);
	}
	if (result != ETIMEDOUT)
	{
		return result;
	}

	if (stat(filename, &file_stat) != 0)
	{
		result = errno != 0 ? errno : ENOENT;
		logError("file: "__FILE__", line: %d, " \
			"stat file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, filename, \
			result, STRERROR(result));
		return result;
	}

	logWarning("file: "__FILE__", line: %d, " \
		"file: %s, file size: %"PRId64 \
		" < %"PRId64", should be resize", \
		__LINE__, filename, (int64_t)file_stat.st_size, file_size);

	fd = open(filename, O_WRONLY, 0644);
	if (fd < 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"open file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, filename, \
			result, STRERROR(result));
		return result;
	}

	if (ftruncate(fd, file_size) == 0)
	{
		result = 0;
	}
	else
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"ftruncate file %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, filename, \
			result, STRERROR(result));
	}

	close(fd);
	return result;
}

bool trunk_check_size(const int64_t file_size)
{
	return file_size <= g_slot_max_size;
}

int trunk_file_delete(const char *trunk_filename, \
		const FDFSTrunkFullInfo *pTrunkInfo)
{
	char pack_buff[FDFS_TRUNK_FILE_HEADER_SIZE];
	char buff[64 * 1024];
	int fd;
	int write_bytes;
	int result;
	int remain_bytes;
	FDFSTrunkHeader trunkHeader;

	fd = open(trunk_filename, O_WRONLY);
	if (fd < 0)
	{
		return errno != 0 ? errno : EIO;
	}

	if (lseek(fd, pTrunkInfo->file.offset, SEEK_SET) < 0)
	{
		result = errno != 0 ? errno : EIO;
		close(fd);
		return result;
	}

	memset(&trunkHeader, 0, sizeof(trunkHeader));
	trunkHeader.alloc_size = pTrunkInfo->file.size;
	trunkHeader.file_type = FDFS_TRUNK_FILE_TYPE_NONE;
	trunk_pack_header(&trunkHeader, pack_buff);

	write_bytes = write(fd, pack_buff, FDFS_TRUNK_FILE_HEADER_SIZE);
	if (write_bytes != FDFS_TRUNK_FILE_HEADER_SIZE)
	{
		result = errno != 0 ? errno : EIO;
		close(fd);
		return result;
	}

	memset(buff, 0, sizeof(buff));
	result = 0;
	remain_bytes = pTrunkInfo->file.size - FDFS_TRUNK_FILE_HEADER_SIZE;
	while (remain_bytes > 0)
	{
		write_bytes = remain_bytes > sizeof(buff) ? \
				sizeof(buff) : remain_bytes;
		if (write(fd, buff, write_bytes) != write_bytes)
		{
			result = errno != 0 ? errno : EIO;
			break;
		}

		remain_bytes -= write_bytes;
	}

	close(fd);
	return result;
}

int trunk_create_trunk_file_advance(void *args)
{
	int64_t total_mb_sum;
	int64_t free_mb_sum;
	int64_t alloc_space;
	FDFSTrunkNode *pTrunkNode;
	int result;
	int i;
	int file_count;

	if (!g_trunk_create_file_advance)
	{
		logError("file: "__FILE__", line: %d, " \
			"do not need create trunk file advancely!", __LINE__);
		return EINVAL;
	}

	if (!g_if_trunker_self)
	{
		logError("file: "__FILE__", line: %d, " \
			"I am not trunk server!", __LINE__);
		return ENOENT;
	}

	alloc_space = g_trunk_create_file_space_threshold - \
			g_trunk_total_free_space;
	if (alloc_space <= 0)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"do not need create trunk file!", __LINE__);
		return 0;
	}

	total_mb_sum = 0;
	free_mb_sum = 0;
	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		total_mb_sum += g_path_space_list[i].total_mb;
		free_mb_sum += g_path_space_list[i].free_mb;
	}

	if (!storage_check_reserved_space_path(total_mb_sum, free_mb_sum \
		- (alloc_space / FDFS_ONE_MB), g_storage_reserved_space.rs.mb))
	{
		logError("file: "__FILE__", line: %d, " \
			"free space is not enough!", __LINE__);
		return ENOSPC;
	}

	result = 0;
	file_count = alloc_space / g_trunk_file_size;
	for (i=0; i<file_count; i++)
	{
		pTrunkNode = trunk_create_trunk_file(-1, &result);
		if (pTrunkNode != NULL)
		{
			result = trunk_add_free_block(pTrunkNode, false);
			if (result != 0)
			{
				break;
			}
		}
	}

	if (result == 0)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"create trunk file count: %d", __LINE__, file_count);
	}
 
	return result;
}

