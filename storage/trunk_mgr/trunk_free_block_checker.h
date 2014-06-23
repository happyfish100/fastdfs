/**
* Copyright (C) 2012 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//trunk_free_block_checker.h

#ifndef _TRUNK_FREE_BLOCK_CHECKER_H_
#define _TRUNK_FREE_BLOCK_CHECKER_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "common_define.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "trunk_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	FDFSTrunkPathInfo path;  //trunk file path
	int id;                  //trunk file id
} FDFSTrunkFileIdentifier;

typedef struct {
	int alloc;  //alloc block count
	int count;  //block count
	FDFSTrunkFullInfo **blocks;   //sort by FDFSTrunkFullInfo.file.offset
} FDFSBlockArray;

typedef struct {
	FDFSTrunkFileIdentifier trunk_file_id;
	FDFSBlockArray block_array;
} FDFSTrunksById;

int trunk_free_block_checker_init();
void trunk_free_block_checker_destroy();

int trunk_free_block_tree_node_count();
int trunk_free_block_total_count();

int trunk_free_block_check_duplicate(FDFSTrunkFullInfo *pTrunkInfo);
int trunk_free_block_insert(FDFSTrunkFullInfo *pTrunkInfo);
int trunk_free_block_delete(FDFSTrunkFullInfo *pTrunkInfo);

int trunk_free_block_tree_print(const char *filename);

#ifdef __cplusplus
}
#endif

#endif

