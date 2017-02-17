/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//trunk_shared.c

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
#include "logger.h"
#include "shared_func.h"
#include "trunk_shared.h"
#include "tracker_proto.h"

FDFSStorePaths g_fdfs_store_paths = {0, NULL};
struct base64_context g_fdfs_base64_context;

void trunk_shared_init()
{
	base64_init_ex(&g_fdfs_base64_context, 0, '-', '_', '.');
}

char **storage_load_paths_from_conf_file_ex(IniContext *pItemContext, \
	const char *szSectionName, const bool bUseBasePath, \
	int *path_count, int *err_no)
{
	char item_name[64];
	char **store_paths;
	char *pPath;
	int i;

	*path_count = iniGetIntValue(szSectionName, "store_path_count", 
					pItemContext, 1);
	if (*path_count <= 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"store_path_count: %d is invalid!", \
			__LINE__, *path_count);
		*err_no = EINVAL;
		return NULL;
	}

	store_paths = (char **)malloc(sizeof(char *) * (*path_count));
	if (store_paths == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(char *) * (*path_count), \
			errno, STRERROR(errno));
		*err_no = errno != 0 ? errno : ENOMEM;
		return NULL;
	}
	memset(store_paths, 0, sizeof(char *) * (*path_count));

	pPath = iniGetStrValue(szSectionName, "store_path0", pItemContext);
	if (pPath == NULL)
	{
		if (!bUseBasePath)
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file must have item " \
				"\"store_path0\"!", __LINE__);
			*err_no = ENOENT;
			free(store_paths);
			return NULL;
		}

		pPath = g_fdfs_base_path;
	}
	store_paths[0] = strdup(pPath);
	if (store_paths[0] == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)strlen(pPath), \
			errno, STRERROR(errno));
		*err_no = errno != 0 ? errno : ENOMEM;
		free(store_paths);
		return NULL;
	}

	*err_no = 0;
	for (i=1; i<*path_count; i++)
	{
		sprintf(item_name, "store_path%d", i);
		pPath = iniGetStrValue(szSectionName, item_name, \
				pItemContext);
		if (pPath == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file must have item \"%s\"!", \
				__LINE__, item_name);
			*err_no = ENOENT;
			break;
		}

		chopPath(pPath);
		if (!fileExists(pPath))
		{
			logError("file: "__FILE__", line: %d, " \
				"\"%s\" can't be accessed, " \
				"errno: %d, error info: %s", __LINE__, \
				pPath, errno, STRERROR(errno));
			*err_no = errno != 0 ? errno : ENOENT;
			break;
		}
		if (!isDir(pPath))
		{
			logError("file: "__FILE__", line: %d, " \
				"\"%s\" is not a directory!", \
				__LINE__, pPath);
			*err_no = ENOTDIR;
			break;
		}

		store_paths[i] = strdup(pPath);
		if (store_paths[i] == NULL)
		{
			logError("file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s", __LINE__, \
				(int)strlen(pPath), errno, STRERROR(errno));
			*err_no = errno != 0 ? errno : ENOMEM;
			break;
		}
	}

	if (*err_no != 0)
	{
		for (i=0; i<*path_count; i++)
		{
			if (store_paths[i] != NULL)
			{
				free(store_paths[i]);
			}
		}
		free(store_paths);
		return NULL;
	}

	return store_paths;
}

int storage_load_paths_from_conf_file(IniContext *pItemContext)
{
	char *pPath;
	int result;

	pPath = iniGetStrValue(NULL, "base_path", pItemContext);
	if (pPath == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"conf file must have item \"base_path\"!", __LINE__);
		return ENOENT;
	}

	snprintf(g_fdfs_base_path, sizeof(g_fdfs_base_path), "%s", pPath);
	chopPath(g_fdfs_base_path);
	if (!fileExists(g_fdfs_base_path))
	{
		logError("file: "__FILE__", line: %d, " \
			"\"%s\" can't be accessed, error info: %s", \
			__LINE__, STRERROR(errno), g_fdfs_base_path);
		return errno != 0 ? errno : ENOENT;
	}
	if (!isDir(g_fdfs_base_path))
	{
		logError("file: "__FILE__", line: %d, " \
			"\"%s\" is not a directory!", \
			__LINE__, g_fdfs_base_path);
		return ENOTDIR;
	}

	g_fdfs_store_paths.paths = storage_load_paths_from_conf_file_ex( \
		pItemContext, NULL, true, &g_fdfs_store_paths.count, &result);

	return result;
}

#define SPLIT_FILENAME_BODY(logic_filename, filename_len, true_filename, \
	store_path_index, check_path_index) \
	do \
	{ \
	char buff[3]; \
	char *pEnd; \
 \
	if (*filename_len <= FDFS_LOGIC_FILE_PATH_LEN) \
	{ \
		logError("file: "__FILE__", line: %d, " \
			"filename_len: %d is invalid, <= %d", \
			__LINE__, *filename_len, FDFS_LOGIC_FILE_PATH_LEN); \
		return EINVAL; \
	} \
 \
	if (*logic_filename != FDFS_STORAGE_STORE_PATH_PREFIX_CHAR) \
	{ /* version < V1.12 */ \
		store_path_index = 0; \
		memcpy(true_filename, logic_filename, (*filename_len)+1); \
		break; \
	} \
 \
	if (*(logic_filename + 3) != '/') \
	{ \
		logError("file: "__FILE__", line: %d, " \
			"filename: %s is invalid", \
			__LINE__, logic_filename); \
		return EINVAL; \
	} \
 \
	*buff = *(logic_filename+1); \
	*(buff+1) = *(logic_filename+2); \
	*(buff+2) = '\0'; \
 \
	pEnd = NULL; \
	store_path_index = strtol(buff, &pEnd, 16); \
	if (pEnd != NULL && *pEnd != '\0') \
	{ \
		logError("file: "__FILE__", line: %d, " \
			"filename: %s is invalid", \
			__LINE__, logic_filename); \
		return EINVAL; \
	} \
 \
	if (check_path_index && (store_path_index < 0 || \
		store_path_index >= g_fdfs_store_paths.count)) \
	{ \
		logError("file: "__FILE__", line: %d, " \
			"filename: %s is invalid, " \
			"invalid store path index: %d", \
			__LINE__, logic_filename, store_path_index); \
		return EINVAL; \
	} \
 \
	*filename_len -= 4; \
	memcpy(true_filename, logic_filename + 4, (*filename_len) + 1); \
 \
	} while (0)


int storage_split_filename(const char *logic_filename, \
		int *filename_len, char *true_filename, char **ppStorePath)
{
	int store_path_index;

	SPLIT_FILENAME_BODY(logic_filename, filename_len, true_filename, \
		store_path_index, true);

	*ppStorePath = g_fdfs_store_paths.paths[store_path_index];

	return 0;
}

int storage_split_filename_ex(const char *logic_filename, \
		int *filename_len, char *true_filename, int *store_path_index)
{
	SPLIT_FILENAME_BODY(logic_filename, \
		filename_len, true_filename, *store_path_index, true);

	return 0;
}

int storage_split_filename_no_check(const char *logic_filename, \
		int *filename_len, char *true_filename, int *store_path_index)
{
	SPLIT_FILENAME_BODY(logic_filename, \
		filename_len, true_filename, *store_path_index, false);

	return 0;
}

char *trunk_info_dump(const FDFSTrunkFullInfo *pTrunkInfo, char *buff, \
				const int buff_size)
{
	snprintf(buff, buff_size, \
		"store_path_index=%d, " \
		"sub_path_high=%d, " \
		"sub_path_low=%d, " \
		"id=%d, offset=%d, size=%d, status=%d", \
		pTrunkInfo->path.store_path_index, \
		pTrunkInfo->path.sub_path_high, \
		pTrunkInfo->path.sub_path_low,  \
		pTrunkInfo->file.id, pTrunkInfo->file.offset, pTrunkInfo->file.size, \
		pTrunkInfo->status);

	return buff;
}

char *trunk_header_dump(const FDFSTrunkHeader *pTrunkHeader, char *buff, \
				const int buff_size)
{
	snprintf(buff, buff_size, \
		"file_type=%d, " \
		"alloc_size=%d, " \
		"file_size=%d, " \
		"crc32=%d, " \
		"mtime=%d, " \
		"ext_name(%d)=%s", \
		pTrunkHeader->file_type, pTrunkHeader->alloc_size, \
		pTrunkHeader->file_size, pTrunkHeader->crc32, \
		pTrunkHeader->mtime, \
		(int)strlen(pTrunkHeader->formatted_ext_name), \
		pTrunkHeader->formatted_ext_name);

	return buff;
}

char *trunk_get_full_filename_ex(const FDFSStorePaths *pStorePaths, \
		const FDFSTrunkFullInfo *pTrunkInfo, \
		char *full_filename, const int buff_size)
{
	char short_filename[64];
	char *pStorePath;

	pStorePath = pStorePaths->paths[pTrunkInfo->path.store_path_index];
	TRUNK_GET_FILENAME(pTrunkInfo->file.id, short_filename);

	snprintf(full_filename, buff_size, \
			"%s/data/"FDFS_STORAGE_DATA_DIR_FORMAT"/" \
			FDFS_STORAGE_DATA_DIR_FORMAT"/%s", \
			pStorePath, pTrunkInfo->path.sub_path_high, \
			pTrunkInfo->path.sub_path_low, short_filename);

	return full_filename;
}

void trunk_pack_header(const FDFSTrunkHeader *pTrunkHeader, char *buff)
{
	*(buff + FDFS_TRUNK_FILE_FILE_TYPE_OFFSET) = pTrunkHeader->file_type;
	int2buff(pTrunkHeader->alloc_size, \
		buff + FDFS_TRUNK_FILE_ALLOC_SIZE_OFFSET);
	int2buff(pTrunkHeader->file_size, \
		buff + FDFS_TRUNK_FILE_FILE_SIZE_OFFSET);
	int2buff(pTrunkHeader->crc32, \
		buff + FDFS_TRUNK_FILE_FILE_CRC32_OFFSET);
	int2buff(pTrunkHeader->mtime, \
		buff + FDFS_TRUNK_FILE_FILE_MTIME_OFFSET);
	memcpy(buff + FDFS_TRUNK_FILE_FILE_EXT_NAME_OFFSET, \
		pTrunkHeader->formatted_ext_name, \
		FDFS_FILE_EXT_NAME_MAX_LEN + 1);
}

void trunk_unpack_header(const char *buff, FDFSTrunkHeader *pTrunkHeader)
{
	pTrunkHeader->file_type = *(buff + FDFS_TRUNK_FILE_FILE_TYPE_OFFSET);
	pTrunkHeader->alloc_size = buff2int(
			buff + FDFS_TRUNK_FILE_ALLOC_SIZE_OFFSET);
	pTrunkHeader->file_size = buff2int(
			buff + FDFS_TRUNK_FILE_FILE_SIZE_OFFSET);
	pTrunkHeader->crc32 = buff2int(
			buff + FDFS_TRUNK_FILE_FILE_CRC32_OFFSET);
	pTrunkHeader->mtime = buff2int(
			buff + FDFS_TRUNK_FILE_FILE_MTIME_OFFSET);
	memcpy(pTrunkHeader->formatted_ext_name, buff + \
		FDFS_TRUNK_FILE_FILE_EXT_NAME_OFFSET, \
		FDFS_FILE_EXT_NAME_MAX_LEN + 1);
	*(pTrunkHeader->formatted_ext_name+FDFS_FILE_EXT_NAME_MAX_LEN+1)='\0';
}

void trunk_file_info_encode(const FDFSTrunkFileInfo *pTrunkFile, char *str)
{
	char buff[sizeof(int) * 3];
	int len;

	int2buff(pTrunkFile->id, buff);
	int2buff(pTrunkFile->offset, buff + sizeof(int));
	int2buff(pTrunkFile->size, buff + sizeof(int) * 2);
	base64_encode_ex(&g_fdfs_base64_context, buff, sizeof(buff), \
			str, &len, false);
}

void trunk_file_info_decode(const char *str, FDFSTrunkFileInfo *pTrunkFile)
{
	char buff[FDFS_TRUNK_FILE_INFO_LEN];
	int len;

	base64_decode_auto(&g_fdfs_base64_context, str, FDFS_TRUNK_FILE_INFO_LEN, \
		buff, &len);

	pTrunkFile->id = buff2int(buff);
	pTrunkFile->offset = buff2int(buff + sizeof(int));
	pTrunkFile->size = buff2int(buff + sizeof(int) * 2);
}

int trunk_file_get_content_ex(const FDFSStorePaths *pStorePaths, \
		const FDFSTrunkFullInfo *pTrunkInfo, const int file_size, \
		int *pfd, char *buff, const int buff_size)
{
	char full_filename[MAX_PATH_SIZE];
	int fd;
	int result;
	int read_bytes;

	if (file_size > buff_size)
	{
		return ENOSPC;
	}

	if (pfd != NULL)
	{
		fd = *pfd;
	}
	else
	{
		trunk_get_full_filename_ex(pStorePaths, pTrunkInfo, \
			full_filename, sizeof(full_filename));
		fd = open(full_filename, O_RDONLY);
		if (fd < 0)
		{
			return errno != 0 ? errno : EIO;
		}

		if (lseek(fd, pTrunkInfo->file.offset + \
			FDFS_TRUNK_FILE_HEADER_SIZE, SEEK_SET) < 0)
		{
			result = errno != 0 ? errno : EIO;
			close(fd);
			return result;
		}
	}

	read_bytes = fc_safe_read(fd, buff, file_size);
	if (read_bytes == file_size)
	{
		result = 0;
	}
	else
	{
		result = errno != 0 ? errno : EINVAL;
	}

	if (pfd == NULL)
	{
		close(fd);
	}

	return result;
}

int trunk_file_stat_func_ex(const FDFSStorePaths *pStorePaths, \
	const int store_path_index, const char *true_filename, \
	const int filename_len, const int stat_func, \
	struct stat *pStat, FDFSTrunkFullInfo *pTrunkInfo, \
	FDFSTrunkHeader *pTrunkHeader, int *pfd)
{
	int result;
	int src_store_path_index;
	int src_filename_len;
	char src_filename[128];
	char src_true_filename[128];

	result = trunk_file_do_lstat_func_ex(pStorePaths, store_path_index, \
		true_filename, filename_len, stat_func, \
		pStat, pTrunkInfo, pTrunkHeader, pfd);
	if (result != 0)
	{
		return result;
	}

	if (!(stat_func == FDFS_STAT_FUNC_STAT && IS_TRUNK_FILE_BY_ID( \
		(*pTrunkInfo)) && S_ISLNK(pStat->st_mode)))
	{
		return 0;
	}

	do
	{
		result = trunk_file_get_content_ex(pStorePaths, pTrunkInfo, \
				pStat->st_size, pfd, src_filename, \
				sizeof(src_filename) - 1);
		if (result != 0)
		{
			break;
		}

		src_filename_len = pStat->st_size;
		*(src_filename + src_filename_len) = '\0';
		if ((result=storage_split_filename_no_check(src_filename, \
			&src_filename_len, src_true_filename, \
			&src_store_path_index)) != 0)
		{
			break;
		}
		if (src_store_path_index < 0 || \
			src_store_path_index >= pStorePaths->count)
		{
			logError("file: "__FILE__", line: %d, " \
				"filename: %s is invalid, " \
				"invalid store path index: %d, " \
				"which < 0 or >= %d", __LINE__, \
				src_filename, src_store_path_index, \
				pStorePaths->count);
			result = EINVAL;
			break;
		}

		if (pfd != NULL)
		{
			close(*pfd);
			*pfd = -1;
		}

		result = trunk_file_do_lstat_func_ex(pStorePaths, \
				src_store_path_index, src_true_filename, \
				src_filename_len, stat_func, pStat, \
				pTrunkInfo, pTrunkHeader, pfd);
	} while (0);

	if (result != 0 && pfd != NULL && *pfd >= 0)
	{
		close(*pfd);
		*pfd = -1;
	}

	return result;
}

int trunk_file_do_lstat_func_ex(const FDFSStorePaths *pStorePaths, \
	const int store_path_index, const char *true_filename, \
	const int filename_len, const int stat_func, \
	struct stat *pStat, FDFSTrunkFullInfo *pTrunkInfo, \
	FDFSTrunkHeader *pTrunkHeader, int *pfd)
{
	char full_filename[MAX_PATH_SIZE];
	char buff[128];
	char pack_buff[FDFS_TRUNK_FILE_HEADER_SIZE];
	int64_t file_size;
	int buff_len;
	int fd;
	int read_bytes;
	int result;

	pTrunkInfo->file.id = 0;
	if (filename_len != FDFS_TRUNK_FILENAME_LENGTH) //not trunk file
	{
		snprintf(full_filename, sizeof(full_filename), "%s/data/%s", \
			pStorePaths->paths[store_path_index], true_filename);

		if (stat_func == FDFS_STAT_FUNC_STAT)
		{
			result = stat(full_filename, pStat);
		}
		else
		{
			result = lstat(full_filename, pStat);
		}
		if (result == 0)
		{
			return 0;
		}
		else
		{
			return errno != 0 ? errno : ENOENT;
		}
	}

	memset(buff, 0, sizeof(buff));
	base64_decode_auto(&g_fdfs_base64_context, (char *)true_filename + \
		FDFS_TRUE_FILE_PATH_LEN, FDFS_FILENAME_BASE64_LENGTH, \
		buff, &buff_len);

	file_size = buff2long(buff + sizeof(int) * 2);
	if (!IS_TRUNK_FILE(file_size))  //slave file
	{
		snprintf(full_filename, sizeof(full_filename), "%s/data/%s", \
			pStorePaths->paths[store_path_index], true_filename);

		if (stat_func == FDFS_STAT_FUNC_STAT)
		{
			result = stat(full_filename, pStat);
		}
		else
		{
			result = lstat(full_filename, pStat);
		}
		if (result == 0)
		{
			return 0;
		}
		else
		{
			return errno != 0 ? errno : ENOENT;
		}
	}

	trunk_file_info_decode(true_filename + FDFS_TRUE_FILE_PATH_LEN + \
		 FDFS_FILENAME_BASE64_LENGTH, &pTrunkInfo->file);

	pTrunkHeader->file_size = FDFS_TRUNK_FILE_TRUE_SIZE(file_size);
	pTrunkHeader->mtime = buff2int(buff + sizeof(int));
	pTrunkHeader->crc32 = buff2int(buff + sizeof(int) * 4);
	memcpy(pTrunkHeader->formatted_ext_name, true_filename + \
		(filename_len - (FDFS_FILE_EXT_NAME_MAX_LEN + 1)), \
		FDFS_FILE_EXT_NAME_MAX_LEN + 2); //include tailing '\0'
	pTrunkHeader->alloc_size = pTrunkInfo->file.size;

	pTrunkInfo->path.store_path_index = store_path_index;
	pTrunkInfo->path.sub_path_high = strtol(true_filename, NULL, 16);
	pTrunkInfo->path.sub_path_low = strtol(true_filename + 3, NULL, 16);

	trunk_get_full_filename_ex(pStorePaths, pTrunkInfo, full_filename, \
				sizeof(full_filename));
	fd = open(full_filename, O_RDONLY);
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

	read_bytes = fc_safe_read(fd, buff, FDFS_TRUNK_FILE_HEADER_SIZE);
	if (read_bytes == FDFS_TRUNK_FILE_HEADER_SIZE)
	{
		result = 0;
	}
	else
	{
		result = errno;
		close(fd);
		return result != 0 ? result : EINVAL;
	}

	memset(pStat, 0, sizeof(struct stat));
	pTrunkHeader->file_type = *(buff + FDFS_TRUNK_FILE_FILE_TYPE_OFFSET);
	if (pTrunkHeader->file_type == FDFS_TRUNK_FILE_TYPE_REGULAR)
	{
		pStat->st_mode = S_IFREG;
	}
	else if (pTrunkHeader->file_type == FDFS_TRUNK_FILE_TYPE_LINK)
	{
		pStat->st_mode = S_IFLNK;
	}
	else if (pTrunkHeader->file_type == FDFS_TRUNK_FILE_TYPE_NONE)
	{
		close(fd);
		return ENOENT;
	}
	else
	{
		close(fd);
		logError("file: "__FILE__", line: %d, " \
			"Invalid file type: %d", __LINE__, \
			pTrunkHeader->file_type);
		return ENOENT;
	}

	trunk_pack_header(pTrunkHeader, pack_buff);

	/*
	{
	char temp[265];
	char szHexBuff[2 * FDFS_TRUNK_FILE_HEADER_SIZE + 1];
	FDFSTrunkHeader trueTrunkHeader;

	fprintf(stderr, "file: "__FILE__", line: %d, true buff=%s\n", __LINE__, \
		bin2hex(buff+1, FDFS_TRUNK_FILE_HEADER_SIZE - 1, szHexBuff));
	trunk_unpack_header(buff, &trueTrunkHeader);
	fprintf(stderr, "file: "__FILE__", line: %d, true fields=%s\n", __LINE__, \
		trunk_header_dump(&trueTrunkHeader, full_filename, sizeof(full_filename)));

	fprintf(stderr, "file: "__FILE__", line: %d, my buff=%s\n", __LINE__, \
		bin2hex(pack_buff+1, FDFS_TRUNK_FILE_HEADER_SIZE - 1, szHexBuff));
	fprintf(stderr, "file: "__FILE__", line: %d, my trunk=%s, my fields=%s\n", __LINE__, \
		trunk_info_dump(pTrunkInfo, temp, sizeof(temp)), \
		trunk_header_dump(pTrunkHeader, full_filename, sizeof(full_filename)));
	}
	*/

	if (memcmp(pack_buff, buff, FDFS_TRUNK_FILE_HEADER_SIZE) != 0)
	{
		close(fd);
		return ENOENT;
	}

	pStat->st_size = pTrunkHeader->file_size;
	pStat->st_mtime = pTrunkHeader->mtime;

	if (pfd != NULL)
	{
		*pfd = fd;
	}
	else
	{
		close(fd);
	}

	return 0;
}

bool fdfs_is_trunk_file(const char *remote_filename, const int filename_len)
{
	int buff_len;
	char buff[64];
	int64_t file_size;

	if (filename_len != FDFS_TRUNK_LOGIC_FILENAME_LENGTH) //not trunk file
	{
		return false;
	}

	memset(buff, 0, sizeof(buff));
	base64_decode_auto(&g_fdfs_base64_context, (char *)remote_filename + \
		FDFS_LOGIC_FILE_PATH_LEN, FDFS_FILENAME_BASE64_LENGTH, \
		buff, &buff_len);

	file_size = buff2long(buff + sizeof(int) * 2);
	return IS_TRUNK_FILE(file_size);
}

int fdfs_decode_trunk_info(const int store_path_index, \
		const char *true_filename, const int filename_len, \
		FDFSTrunkFullInfo *pTrunkInfo)
{
	if (filename_len != FDFS_TRUNK_FILENAME_LENGTH) //not trunk file
	{
		logWarning("file: "__FILE__", line: %d, " \
			"trunk filename length: %d != %d, filename: %s", \
			__LINE__, filename_len, FDFS_TRUNK_FILENAME_LENGTH, \
			true_filename);
		return EINVAL;
	}

	pTrunkInfo->path.store_path_index = store_path_index;
	pTrunkInfo->path.sub_path_high = strtol(true_filename, NULL, 16);
	pTrunkInfo->path.sub_path_low = strtol(true_filename + 3, NULL, 16);
	trunk_file_info_decode(true_filename + FDFS_TRUE_FILE_PATH_LEN + \
		FDFS_FILENAME_BASE64_LENGTH, &pTrunkInfo->file);
	return 0;
}

