/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//trunk_free_block_checker.c

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
#include "shared_func.h"
#include "avl_tree.h"
#include "tracker_types.h"
#include "storage_global.h"
#include "trunk_free_block_checker.h"

#define TRUNK_FREE_BLOCK_ARRAY_INIT_SIZE  32

static AVLTreeInfo tree_info_by_id = {NULL, NULL, NULL}; //for unique block nodes

static int storage_trunk_node_compare_entry(void *p1, void *p2)
{
	return memcmp(&(((FDFSTrunksById *)p1)->trunk_file_id), \
			&(((FDFSTrunksById *)p2)->trunk_file_id), \
			sizeof(FDFSTrunkFileIdentifier));
}

int trunk_free_block_checker_init()
{
	int result;
	if ((result=avl_tree_init(&tree_info_by_id, free, \
			storage_trunk_node_compare_entry)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"avl_tree_init fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	return 0;
}

void trunk_free_block_checker_destroy()
{
	avl_tree_destroy(&tree_info_by_id);
}

int trunk_free_block_tree_node_count()
{
	return avl_tree_count(&tree_info_by_id);
}

static int block_tree_count_walk_callback(void *data, void *args)
{
	int *pcount;
	pcount = (int *)args;

	*pcount += ((FDFSTrunksById *)data)->block_array.count;
	return 0;
}

int trunk_free_block_total_count()
{
	int count;
	count = 0;
	avl_tree_walk(&tree_info_by_id, block_tree_count_walk_callback, &count);
	return count;
}

#define FILL_FILE_IDENTIFIER(target, pTrunkInfo) \
	memset(&target, 0, sizeof(target)); \
	memcpy(&(target.path), &(pTrunkInfo->path), sizeof(FDFSTrunkPathInfo));\
	target.id = pTrunkInfo->file.id;

int trunk_free_block_check_duplicate(FDFSTrunkFullInfo *pTrunkInfo)
{
	FDFSTrunkFileIdentifier target;
	FDFSTrunksById *pFound;
	FDFSTrunkFullInfo **blocks;
	int end_offset;
	int left;
	int right;
	int mid;
	int result;

	/*
	char buff[256];

	logWarning("file: "__FILE__", line: %d, " \
		"trunk entry: %s", __LINE__, \
		trunk_info_dump(pTrunkInfo, buff, sizeof(buff)));
	*/

	FILL_FILE_IDENTIFIER(target, pTrunkInfo);

	pFound = (FDFSTrunksById *)avl_tree_find(&tree_info_by_id, &target);
	if (pFound == NULL)
	{
		return 0;
	}

	/*
	{
		logWarning("file: "__FILE__", line: %d, " \
			"ARRAY COUNT: %d, trunk entry: %s", \
			__LINE__, pFound->block_array.count, \
			trunk_info_dump(pTrunkInfo, buff, sizeof(buff)));
	}
	*/

	if (pFound->block_array.count == 0)
	{
		return 0;
	}

	blocks = pFound->block_array.blocks;
	end_offset = pTrunkInfo->file.offset + pTrunkInfo->file.size;
	if (end_offset <= blocks[0]->file.offset)
	{
		return 0;
	}

	right = pFound->block_array.count - 1;
	if (pTrunkInfo->file.offset >= blocks[right]->file.offset + \
				blocks[right]->file.size)
	{
		return 0;
	}


	result = 0;
	mid = 0;
	left = 0;
	while (left <= right)
	{
		mid = (left + right) / 2;
		if (pTrunkInfo->file.offset < blocks[mid]->file.offset)
		{
			if (blocks[mid]->file.offset < end_offset)
			{
				result = EEXIST;
				break;
			}

			right = mid - 1;
		}
		else if (pTrunkInfo->file.offset == blocks[mid]->file.offset)
		{
			if (pTrunkInfo->file.size == blocks[mid]->file.size)
			{
				char buff[256];
				logWarning("file: "__FILE__", line: %d, " \
					"node already exist, trunk entry: %s", \
					__LINE__, trunk_info_dump(pTrunkInfo, \
					buff, sizeof(buff)));
				return EEXIST;
			}

			result = EEXIST;
			break;
		}
		else
		{
			if (pTrunkInfo->file.offset < (blocks[mid]->file.offset + \
						blocks[mid]->file.size))
			{
				result = EEXIST;
				break;
			}

			left = mid + 1;
		}
	}

	if (result != 0)
	{
		char buff1[256];
		char buff2[256];

		logWarning("file: "__FILE__", line: %d, " \
			"node overlap, current trunk entry: %s, " \
			"existed trunk entry: %s", __LINE__, \
			trunk_info_dump(pTrunkInfo, buff1, sizeof(buff1)), \
			trunk_info_dump(blocks[mid], buff2, sizeof(buff2)));
	}

	return result;
}

static int trunk_free_block_realloc(FDFSBlockArray *pArray, const int new_alloc)
{
	FDFSTrunkFullInfo **blocks;
	int result;

	blocks = (FDFSTrunkFullInfo **)realloc(pArray->blocks, \
			new_alloc * sizeof(FDFSTrunkFullInfo *));
	if (blocks == NULL)
	{
		result = errno != 0 ? errno : ENOMEM;
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", __LINE__, \
			(int)(new_alloc * sizeof(FDFSTrunkFullInfo *)),
			result, STRERROR(result));
		return result;
	}

	pArray->alloc = new_alloc;
	pArray->blocks = blocks;
	return 0;
}

static int trunk_free_block_do_insert(FDFSTrunkFullInfo *pTrunkInfo, \
		FDFSBlockArray *pArray)
{
	int left;
	int right;
	int mid;
	int pos;
	int result;

	if (pArray->count >= pArray->alloc)
	{
		if ((result=trunk_free_block_realloc(pArray, \
			pArray->alloc == 0 ? TRUNK_FREE_BLOCK_ARRAY_INIT_SIZE \
						 : 2 * pArray->alloc)) != 0)
		{
			return result;
		}
	}

	if (pArray->count == 0)
	{
		pArray->blocks[pArray->count++] = pTrunkInfo;
		return 0;
	}

	if (pTrunkInfo->file.offset < pArray->blocks[0]->file.offset)
	{
		memmove(&(pArray->blocks[1]), &(pArray->blocks[0]), \
			pArray->count * sizeof(FDFSTrunkFullInfo *));
		pArray->blocks[0] = pTrunkInfo;
		pArray->count++;
		return 0;
	}

	right = pArray->count - 1;
	if (pTrunkInfo->file.offset > pArray->blocks[right]->file.offset)
	{
		pArray->blocks[pArray->count++] = pTrunkInfo;
		return 0;
	}

	left = 0;
	mid = 0;
	while (left <= right)
	{
		mid = (left + right) / 2;
		if (pArray->blocks[mid]->file.offset > pTrunkInfo->file.offset)
		{
			right = mid - 1;
		}
		else if (pArray->blocks[mid]->file.offset == \
			pTrunkInfo->file.offset)
		{
			char buff[256];
			logWarning("file: "__FILE__", line: %d, " \
				"node already exist, trunk entry: %s", \
				__LINE__, trunk_info_dump(pTrunkInfo, \
				buff, sizeof(buff)));
			return EEXIST;
		}
		else
		{
			left = mid + 1;
		}
	}

	if (pTrunkInfo->file.offset < pArray->blocks[mid]->file.offset)
	{
		pos = mid;
	}
	else
	{
		pos = mid + 1;
	}

	memmove(&(pArray->blocks[pos + 1]), &(pArray->blocks[pos]), \
			(pArray->count - pos) * sizeof(FDFSTrunkFullInfo *));
	pArray->blocks[pos] = pTrunkInfo;
	pArray->count++;
	return 0;
}

int trunk_free_block_insert(FDFSTrunkFullInfo *pTrunkInfo)
{
	int result;
	FDFSTrunkFileIdentifier target;
	FDFSTrunksById *pTrunksById;

	FILL_FILE_IDENTIFIER(target, pTrunkInfo);

	pTrunksById = (FDFSTrunksById *)avl_tree_find(&tree_info_by_id, &target);
	if (pTrunksById == NULL)
	{
		pTrunksById = (FDFSTrunksById *)malloc(sizeof(FDFSTrunksById));
		if (pTrunksById == NULL)
		{
			result = errno != 0 ? errno : ENOMEM;
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", \
				__LINE__, (int)sizeof(FDFSTrunksById), \
				result, STRERROR(result));
			return result;
		}

		memset(pTrunksById, 0, sizeof(FDFSTrunksById));
		memcpy(&(pTrunksById->trunk_file_id), &target, \
			sizeof(FDFSTrunkFileIdentifier));
		if (avl_tree_insert(&tree_info_by_id, pTrunksById) != 1)
		{
			result = errno != 0 ? errno : ENOMEM;
			logError("file: "__FILE__", line: %d, " \
				"avl_tree_insert fail, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
			return result;
		}
	}

	return trunk_free_block_do_insert(pTrunkInfo, \
				&(pTrunksById->block_array));
}

int trunk_free_block_delete(FDFSTrunkFullInfo *pTrunkInfo)
{
	int result;
	int left;
	int right;
	int mid;
	int move_count;
	FDFSTrunkFileIdentifier target;
	FDFSTrunksById *pTrunksById;
	char buff[256];

	FILL_FILE_IDENTIFIER(target, pTrunkInfo);

	pTrunksById = (FDFSTrunksById *)avl_tree_find(&tree_info_by_id, &target);
	if (pTrunksById == NULL)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"node NOT exist, trunk entry: %s", \
			__LINE__, trunk_info_dump(pTrunkInfo, \
			buff, sizeof(buff)));
		return ENOENT;
	}

	result = ENOENT;
	mid = 0;
	left = 0;
	right = pTrunksById->block_array.count - 1;
	while (left <= right)
	{
		mid = (left + right) / 2;
		if (pTrunksById->block_array.blocks[mid]->file.offset > \
				pTrunkInfo->file.offset)
		{
			right = mid - 1;
		}
		else if (pTrunksById->block_array.blocks[mid]->file.offset == \
				pTrunkInfo->file.offset)
		{
			result = 0;
			break;
		}
		else
		{
			left = mid + 1;
		}
	}

	if (result == ENOENT)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"trunk node NOT exist, trunk entry: %s", \
			__LINE__, trunk_info_dump(pTrunkInfo, \
			buff, sizeof(buff)));
		return result;
	}

	move_count = pTrunksById->block_array.count - (mid + 1);
	if (move_count > 0)
	{
		memmove(&(pTrunksById->block_array.blocks[mid]), \
			&(pTrunksById->block_array.blocks[mid + 1]), \
			move_count * sizeof(FDFSTrunkFullInfo *));
	}
	pTrunksById->block_array.count--;

	if (pTrunksById->block_array.count == 0)
	{
		free(pTrunksById->block_array.blocks);
		if (avl_tree_delete(&tree_info_by_id, pTrunksById) != 1)
		{
			memset(&(pTrunksById->block_array), 0, \
				sizeof(FDFSBlockArray));
			logWarning("file: "__FILE__", line: %d, " \
				"can't delete block node, trunk info: %s", \
				__LINE__, trunk_info_dump(pTrunkInfo, buff, \
				sizeof(buff)));
			return ENOENT;
		}
	}
	else
	{
		if ((pTrunksById->block_array.count < \
			pTrunksById->block_array.alloc / 2) && \
		    (pTrunksById->block_array.count > \
			TRUNK_FREE_BLOCK_ARRAY_INIT_SIZE / 2)) //small the array
		{
			if ((result=trunk_free_block_realloc( \
				&(pTrunksById->block_array), \
				pTrunksById->block_array.alloc / 2)) != 0)
			{
				return result;
			}
		}
	}

	return 0;
}

static int block_tree_print_walk_callback(void *data, void *args)
{
	FILE *fp;
	FDFSBlockArray *pArray;
	FDFSTrunkFullInfo **pp;
	FDFSTrunkFullInfo **ppEnd;

	fp = (FILE *)args;
	pArray = &(((FDFSTrunksById *)data)->block_array);

	/*
	{
	FDFSTrunkFileIdentifier *pFileIdentifier;
	pFileIdentifier = &(((FDFSTrunksById *)data)->trunk_file_id);

	fprintf(fp, "%d %d %d %d %d", \
			pFileIdentifier->path.store_path_index, \
			pFileIdentifier->path.sub_path_high, \
			pFileIdentifier->path.sub_path_low, \
			pFileIdentifier->id, pArray->count);
	if (pArray->count > 0)
	{
		fprintf(fp, " %d", pArray->blocks[0]->file.offset);
		if (pArray->count > 1)
		{
			fprintf(fp, " %d", pArray->blocks[pArray->count-1]-> \
				file.offset + pArray->blocks[pArray->count-1]->\
				file.size);
		}
	}
	fprintf(fp, "\n");
	return 0;
	}
	*/

	/*
	{
	FDFSTrunkFullInfo **ppPrevious;
	if (pArray->count <= 1)
	{
		return 0;
	}
	ppPrevious = pArray->blocks;
	ppEnd = pArray->blocks + pArray->count;
	for (pp=pArray->blocks + 1; pp<ppEnd; pp++)
	{
		if ((*ppPrevious)->file.offset >= (*pp)->file.offset)
		{
			fprintf(fp, "%d %d %d %d %d %d\n", \
				(*ppPrevious)->path.store_path_index, \
				(*ppPrevious)->path.sub_path_high, (*ppPrevious)->path.sub_path_low, \
				(*ppPrevious)->file.id, (*ppPrevious)->file.offset, (*ppPrevious)->file.size);

			fprintf(fp, "%d %d %d %d %d %d\n", \
				(*pp)->path.store_path_index, \
				(*pp)->path.sub_path_high, (*pp)->path.sub_path_low, \
				(*pp)->file.id, (*pp)->file.offset, (*pp)->file.size);
		}
		ppPrevious = pp;
	}
	return 0;
	}
	*/

	ppEnd = pArray->blocks + pArray->count;
	for (pp=pArray->blocks; pp<ppEnd; pp++)
	{
		fprintf(fp, "%d %d %d %d %d %d\n", \
			(*pp)->path.store_path_index, \
			(*pp)->path.sub_path_high, (*pp)->path.sub_path_low, \
			(*pp)->file.id, (*pp)->file.offset, (*pp)->file.size);
	}

	return 0;
}

int trunk_free_block_tree_print(const char *filename)
{
	FILE *fp;
	int result;

	fp = fopen(filename, "w");
	if (fp == NULL)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"open file %s fail, " \
			"errno: %d, error info: %s", __LINE__, \
			filename, result, STRERROR(result));
		return result;
	}

	avl_tree_walk(&tree_info_by_id, block_tree_print_walk_callback, fp);
	fclose(fp);
	return 0;
}

