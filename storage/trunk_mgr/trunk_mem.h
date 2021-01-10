/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//trunk_mem.h

#ifndef _TRUNK_MEM_H_
#define _TRUNK_MEM_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "fastcommon/common_define.h"
#include "fdfs_global.h"
#include "fastcommon/fast_mblock.h"
#include "trunk_shared.h"
#include "fdfs_shared_func.h"

#define STORAGE_TRUNK_COMPRESS_STAGE_NONE                0
#define STORAGE_TRUNK_COMPRESS_STAGE_COMPRESS_BEGIN      1
#define STORAGE_TRUNK_COMPRESS_STAGE_APPLY_DONE          2
#define STORAGE_TRUNK_COMPRESS_STAGE_SAVE_DONE           3
#define STORAGE_TRUNK_COMPRESS_STAGE_COMMIT_MERGING      4
#define STORAGE_TRUNK_COMPRESS_STAGE_COMMIT_MERGE_DONE   5
#define STORAGE_TRUNK_COMPRESS_STAGE_COMPRESS_SUCCESS    6
#define STORAGE_TRUNK_COMPRESS_STAGE_ROLLBACK_MERGING    7
#define STORAGE_TRUNK_COMPRESS_STAGE_ROLLBACK_MERGE_DONE 8
#define STORAGE_TRUNK_COMPRESS_STAGE_FINISHED            9

#ifdef __cplusplus
extern "C" {
#endif

extern int g_slot_min_size;    //slot min size, such as 256 bytes
extern int g_slot_max_size;    //slot max size
extern int g_trunk_alloc_alignment_size;  //the alignment size for trunk alloc
extern int g_trunk_file_size;  //the trunk file size, such as 64MB
extern int g_store_path_mode;  //store which path mode, fetch from tracker
extern FDFSStorageReservedSpace g_storage_reserved_space;  //fetch from tracker
extern int g_avg_storage_reserved_mb;  //calc by above var: g_storage_reserved_mb
extern int g_store_path_index;  //store to which path
extern volatile int g_current_trunk_file_id;  //current trunk file id
extern TimeInfo g_trunk_create_file_time_base;
extern TimeInfo g_trunk_compress_binlog_time_base;
extern int g_trunk_create_file_interval;
extern int g_trunk_compress_binlog_min_interval;
extern int g_trunk_compress_binlog_interval;
extern int g_trunk_binlog_max_backups;
extern TrackerServerInfo g_trunk_server;  //the trunk server
extern bool g_if_use_trunk_file;   //if use trunk file
extern bool g_trunk_create_file_advance;
extern bool g_trunk_init_check_occupying;
extern bool g_trunk_init_reload_from_binlog;
extern bool g_trunk_free_space_merge;
extern bool g_delete_unused_trunk_files;
extern int g_trunk_binlog_compress_stage;
extern bool g_if_trunker_self;   //if am i trunk server
extern int64_t g_trunk_create_file_space_threshold;
extern volatile int64_t g_trunk_total_free_space;  //trunk total free space in bytes
extern time_t g_trunk_last_compress_time;

typedef struct tagFDFSTrunkNode {
	FDFSTrunkFullInfo trunk;    //trunk info
	struct fast_mblock_node *pMblockNode;   //for free
	struct tagFDFSTrunkNode *next;
} FDFSTrunkNode;

typedef struct {
	int size;
	FDFSTrunkNode *head;
	struct fast_mblock_node *pMblockNode;   //for free
} FDFSTrunkSlot;

int storage_trunk_init();
int storage_trunk_destroy_ex(const bool bNeedSleep,
        const bool bSaveData);

#define storage_trunk_destroy() storage_trunk_destroy_ex(false, true)

int trunk_alloc_space(const int size, FDFSTrunkFullInfo *pResult);
int trunk_alloc_confirm(const FDFSTrunkFullInfo *pTrunkInfo, const int status);

int trunk_free_space(const FDFSTrunkFullInfo *pTrunkInfo, \
		const bool bWriteBinLog);

bool trunk_check_size(const int64_t file_size);

#define trunk_init_file(filename) \
	trunk_init_file_ex(filename, g_trunk_file_size)

#define trunk_check_and_init_file(filename) \
	trunk_check_and_init_file_ex(filename, g_trunk_file_size)

int trunk_init_file_ex(const char *filename, const int64_t file_size);

int trunk_check_and_init_file_ex(const char *filename, const int64_t file_size);

int trunk_file_delete(const char *trunk_filename, \
		const FDFSTrunkFullInfo *pTrunkInfo);

int trunk_create_trunk_file_advance(void *args);

int trunk_binlog_compress_func(void *args);

int storage_trunk_binlog_compress_check_recovery();

char *storage_trunk_get_data_filename(char *full_filename);

#define storage_check_reserved_space(pGroup) \
        fdfs_check_reserved_space(pGroup, &g_storage_reserved_space)

#define storage_check_reserved_space_trunk(pGroup) \
        fdfs_check_reserved_space_trunk(pGroup, &g_storage_reserved_space)

#define storage_check_reserved_space_path(total_mb, free_mb, avg_mb) \
        fdfs_check_reserved_space_path(total_mb, free_mb, avg_mb, \
                                &g_storage_reserved_space)

#define storage_get_storage_reserved_space_mb(total_mb) \
	fdfs_get_storage_reserved_space_mb(total_mb, &g_storage_reserved_space)

#ifdef __cplusplus
}
#endif

#endif

