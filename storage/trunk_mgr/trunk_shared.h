/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//trunk_shared.h

#ifndef _TRUNK_SHARED_H_
#define _TRUNK_SHARED_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "fastcommon/base64.h"
#include "fastcommon/common_define.h"
#include "fastcommon/ini_file_reader.h"
#include "fdfs_global.h"
#include "tracker_types.h"

#define FDFS_TRUNK_STATUS_FREE  0
#define FDFS_TRUNK_STATUS_HOLD  1

#define FDFS_TRUNK_FILE_TYPE_NONE     '\0'
#define FDFS_TRUNK_FILE_TYPE_REGULAR  'F'
#define FDFS_TRUNK_FILE_TYPE_LINK     'L'

#define FDFS_STAT_FUNC_STAT     0
#define FDFS_STAT_FUNC_LSTAT    1

#define FDFS_TRUNK_FILE_FILE_TYPE_OFFSET	0
#define FDFS_TRUNK_FILE_ALLOC_SIZE_OFFSET	1
#define FDFS_TRUNK_FILE_FILE_SIZE_OFFSET	5
#define FDFS_TRUNK_FILE_FILE_CRC32_OFFSET	9
#define FDFS_TRUNK_FILE_FILE_MTIME_OFFSET  	13
#define FDFS_TRUNK_FILE_FILE_EXT_NAME_OFFSET	17
#define FDFS_TRUNK_FILE_HEADER_SIZE	(17 + FDFS_FILE_EXT_NAME_MAX_LEN + 1)

#define TRUNK_CALC_SIZE(file_size) (FDFS_TRUNK_FILE_HEADER_SIZE + file_size)
#define TRUNK_FILE_START_OFFSET(trunkInfo) \
		(FDFS_TRUNK_FILE_HEADER_SIZE + trunkInfo.file.offset)

#define IS_TRUNK_FILE_BY_ID(trunkInfo) (trunkInfo.file.id > 0)

#define TRUNK_GET_FILENAME(file_id, filename) \
	sprintf(filename, "%06u", file_id)

typedef struct
{
	int total_mb;  //total spaces
	int free_mb;   //free spaces
    int path_len;  //the length of store path
	char *path;    //file store path
    char *mark;    //path mark to avoid confusion
} FDFSStorePathInfo;

typedef struct {
	int count;   //store path count
	FDFSStorePathInfo *paths; //file store paths
} FDFSStorePaths;

#ifdef __cplusplus
extern "C" {
#endif

extern FDFSStorePaths g_fdfs_store_paths;  //file store paths
extern struct base64_context g_fdfs_base64_context;   //base64 context
extern BufferInfo g_zero_buffer;   //zero buffer for reset

typedef int (*stat_func)(const char *filename, struct stat *buf);

typedef struct tagFDFSTrunkHeader {
	char file_type;
	char formatted_ext_name[FDFS_FILE_EXT_NAME_MAX_LEN + 2];
	int alloc_size;
	int file_size;
	int crc32;
	int mtime;
} FDFSTrunkHeader;

typedef struct tagFDFSTrunkPathInfo {
	unsigned char store_path_index;   //store which path as Mxx
	unsigned char sub_path_high;      //high sub dir index, front part of HH/HH
	unsigned char sub_path_low;       //low sub dir index, tail part of HH/HH
} FDFSTrunkPathInfo;

typedef struct tagFDFSTrunkFileInfo {
	int id;      //trunk file id
	int offset;  //file offset
	int size;    //space size
} FDFSTrunkFileInfo;

typedef struct tagFDFSTrunkFullInfo {
	char status;  //normal or hold
	FDFSTrunkPathInfo path;
	FDFSTrunkFileInfo file;
} FDFSTrunkFullInfo;

FDFSStorePathInfo *storage_load_paths_from_conf_file_ex(
        IniContext *pItemContext, const char *szSectionName,
        const bool bUseBasePath, int *path_count, int *err_no);
int storage_load_paths_from_conf_file(IniContext *pItemContext);
int trunk_shared_init();

int storage_split_filename(const char *logic_filename, \
		int *filename_len, char *true_filename, char **ppStorePath);
int storage_split_filename_ex(const char *logic_filename, \
		int *filename_len, char *true_filename, int *store_path_index);
int storage_split_filename_no_check(const char *logic_filename, \
		int *filename_len, char *true_filename, int *store_path_index);

void trunk_file_info_encode(const FDFSTrunkFileInfo *pTrunkFile, char *str);
void trunk_file_info_decode(const char *str, FDFSTrunkFileInfo *pTrunkFile);

char *trunk_info_dump(const FDFSTrunkFullInfo *pTrunkInfo, char *buff, \
				const int buff_size);

char *trunk_header_dump(const FDFSTrunkHeader *pTrunkHeader, char *buff, \
				const int buff_size);

#define trunk_get_full_filename(pTrunkInfo, full_filename, buff_size) \
	trunk_get_full_filename_ex(&g_fdfs_store_paths, pTrunkInfo, \
		full_filename, buff_size)

char *trunk_get_full_filename_ex(const FDFSStorePaths *pStorePaths, \
		const FDFSTrunkFullInfo *pTrunkInfo, \
		char *full_filename, const int buff_size);

void trunk_pack_header(const FDFSTrunkHeader *pTrunkHeader, char *buff);
void trunk_unpack_header(const char *buff, FDFSTrunkHeader *pTrunkHeader);

#define trunk_file_get_content(pTrunkInfo, file_size, pfd, buff, buff_size) \
	trunk_file_get_content_ex(&g_fdfs_store_paths, pTrunkInfo, \
		file_size, pfd, buff, buff_size)

int trunk_file_get_content_ex(const FDFSStorePaths *pStorePaths, \
		const FDFSTrunkFullInfo *pTrunkInfo, const int file_size, \
		int *pfd, char *buff, const int buff_size);

#define trunk_file_do_lstat_func(store_path_index, true_filename, \
		filename_len, stat_func, pStat, pTrunkInfo, pTrunkHeader, pfd) \
	trunk_file_do_lstat_func_ex(&g_fdfs_store_paths, store_path_index, \
		true_filename, filename_len, stat_func, pStat, pTrunkInfo, \
		pTrunkHeader, pfd)

#define trunk_file_stat_func(store_path_index, true_filename, filename_len, \
		stat_func, pStat, pTrunkInfo, pTrunkHeader, pfd) \
	trunk_file_stat_func_ex(&g_fdfs_store_paths, store_path_index, \
		true_filename, filename_len, stat_func, pStat, pTrunkInfo, \
		pTrunkHeader, pfd)

#define trunk_file_stat(store_path_index, true_filename, filename_len, \
		pStat, pTrunkInfo, pTrunkHeader) \
	trunk_file_stat_func(store_path_index, true_filename, filename_len, \
		FDFS_STAT_FUNC_STAT, pStat, pTrunkInfo, pTrunkHeader, NULL)

#define trunk_file_lstat(store_path_index, true_filename, filename_len, \
		pStat, pTrunkInfo, pTrunkHeader) \
	trunk_file_do_lstat_func(store_path_index, true_filename, filename_len, \
		FDFS_STAT_FUNC_LSTAT, pStat, pTrunkInfo, pTrunkHeader, NULL)

#define trunk_file_lstat_ex(store_path_index, true_filename, filename_len, \
		pStat, pTrunkInfo, pTrunkHeader, pfd) \
	trunk_file_do_lstat_func(store_path_index, true_filename, filename_len, \
		FDFS_STAT_FUNC_LSTAT, pStat, pTrunkInfo, pTrunkHeader, pfd)

#define trunk_file_stat_ex(store_path_index, true_filename, filename_len, \
		pStat, pTrunkInfo, pTrunkHeader, pfd) \
	trunk_file_stat_func(store_path_index, true_filename, filename_len, \
		FDFS_STAT_FUNC_STAT, pStat, pTrunkInfo, pTrunkHeader, pfd)

#define trunk_file_stat_ex1(pStorePaths, store_path_index, true_filename, \
		filename_len, pStat, pTrunkInfo, pTrunkHeader, pfd) \
	trunk_file_stat_func_ex(pStorePaths, store_path_index, true_filename, \
		filename_len, FDFS_STAT_FUNC_STAT, pStat, pTrunkInfo, \
		pTrunkHeader, pfd)

int trunk_file_stat_func_ex(const FDFSStorePaths *pStorePaths, \
	const int store_path_index, const char *true_filename, \
	const int filename_len, const int stat_func, \
	struct stat *pStat, FDFSTrunkFullInfo *pTrunkInfo, \
	FDFSTrunkHeader *pTrunkHeader, int *pfd);

int trunk_file_do_lstat_func_ex(const FDFSStorePaths *pStorePaths, \
	const int store_path_index, const char *true_filename, \
	const int filename_len, const int stat_func, \
	struct stat *pStat, FDFSTrunkFullInfo *pTrunkInfo, \
	FDFSTrunkHeader *pTrunkHeader, int *pfd);

bool fdfs_is_trunk_file(const char *remote_filename, const int filename_len);

int fdfs_decode_trunk_info(const int store_path_index, \
		const char *true_filename, const int filename_len, \
		FDFSTrunkFullInfo *pTrunkInfo);

#ifdef __cplusplus
}
#endif

#endif

