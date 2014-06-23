/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//tracker_nio.h

#ifndef _TRACKER_NIO_H
#define _TRACKER_NIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "tracker_types.h"
#include "storage_func.h"
#include "fast_task_queue.h"
#include "storage_global.h"
#include "fdht_types.h"
#include "trunk_mem.h"
#include "md5.h"

#define FDFS_STORAGE_STAGE_NIO_INIT   0
#define FDFS_STORAGE_STAGE_NIO_RECV   1
#define FDFS_STORAGE_STAGE_NIO_SEND   2
#define FDFS_STORAGE_STAGE_NIO_CLOSE  4  //close socket
#define FDFS_STORAGE_STAGE_DIO_THREAD 8

#define FDFS_STORAGE_FILE_OP_READ     'R'
#define FDFS_STORAGE_FILE_OP_WRITE    'W'
#define FDFS_STORAGE_FILE_OP_APPEND   'A'
#define FDFS_STORAGE_FILE_OP_DELETE   'D'
#define FDFS_STORAGE_FILE_OP_DISCARD  'd'

typedef int (*TaskDealFunc)(struct fast_task_info *pTask);

/* this clean func will be called when connection disconnected */
typedef void (*DisconnectCleanFunc)(struct fast_task_info *pTask);

typedef void (*DeleteFileLogCallback)(struct fast_task_info *pTask, \
		const int err_no);

typedef void (*FileDealDoneCallback)(struct fast_task_info *pTask, \
		const int err_no);

typedef int (*FileBeforeOpenCallback)(struct fast_task_info *pTask);
typedef int (*FileBeforeCloseCallback)(struct fast_task_info *pTask);

#define _FILE_TYPE_APPENDER  1
#define _FILE_TYPE_TRUNK     2   //if trunk file, since V3.0
#define _FILE_TYPE_SLAVE     4
#define _FILE_TYPE_REGULAR   8
#define _FILE_TYPE_LINK     16

typedef struct
{
	bool if_gen_filename;	  //if upload generate filename
	char file_type;           //regular or link file
	bool if_sub_path_alloced; //if sub path alloced since V3.0
	char master_filename[128];
	char file_ext_name[FDFS_FILE_EXT_NAME_MAX_LEN + 1];
	char formatted_ext_name[FDFS_FILE_EXT_NAME_MAX_LEN + 2];
	char prefix_name[FDFS_FILE_PREFIX_MAX_LEN + 1];
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];  	//the upload group name
	int start_time;		//upload start timestamp
	FDFSTrunkFullInfo trunk_info;
	FileBeforeOpenCallback before_open_callback;
	FileBeforeCloseCallback before_close_callback;
} StorageUploadInfo;

typedef struct
{
	char op_flag;
	char *meta_buff;
	int meta_bytes;
} StorageSetMetaInfo;

typedef struct
{
	char filename[MAX_PATH_SIZE + 128];  	//full filename

	/* FDFS logic filename to log not including group name */
	char fname2log[128+sizeof(FDFS_STORAGE_META_FILE_EXT)];

	char op;            //w for writing, r for reading, d for deleting etc.
	char sync_flag;     //sync flag log to binlog
	bool calc_crc32;    //if calculate file content hash code
	bool calc_file_hash;      //if calculate file content hash code
	int open_flags;           //open file flags
	int file_hash_codes[4];   //file hash code
	int crc32;   //file content crc32 signature
	MD5_CTX md5_context;

	union
	{
		StorageUploadInfo upload;
		StorageSetMetaInfo setmeta;
	} extra_info;

	int dio_thread_index;		//dio thread index
	int timestamp2log;		//timestamp to log
	int delete_flag;     //delete file flag
	int create_flag;    //create file flag
	int buff_offset;    //buffer offset after recv to write to file
	int fd;         //file description no
	int64_t start;  //the start offset of file
	int64_t end;    //the end offset of file
	int64_t offset; //the current offset of file
	FileDealDoneCallback done_callback;
	DeleteFileLogCallback log_callback;

	struct timeval tv_deal_start; //task deal start tv for access log
} StorageFileContext;

typedef struct
{
	int nio_thread_index;  //nio thread index
	bool canceled;
	char stage;  //nio stage, send or recv
	char storage_server_id[FDFS_STORAGE_ID_MAX_SIZE];

	StorageFileContext file_context;

	int64_t total_length;   //pkg total length for req and request
	int64_t total_offset;   //pkg current offset for req and request

	int64_t request_length;   //request pkg length for access log

	FDFSStorageServer *pSrcStorage;
	TaskDealFunc deal_func;  //function pointer to deal this task
	void *extra_arg;   //store extra arg, such as (BinLogReader *)
	DisconnectCleanFunc clean_func;  //clean function pointer when finished
} StorageClientInfo;

struct storage_nio_thread_data
{
	struct nio_thread_data thread_data;
	GroupArray group_array;  //FastDHT group array
};

#ifdef __cplusplus
extern "C" {
#endif

void storage_recv_notify_read(int sock, short event, void *arg);
int storage_send_add_event(struct fast_task_info *pTask);

void task_finish_clean_up(struct fast_task_info *pTask);
void add_to_deleted_list(struct fast_task_info *pTask);

#ifdef __cplusplus
}
#endif

#endif

