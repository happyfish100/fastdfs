/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//trunk_sync.c

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
#include "trunk_sync.h"

#define TRUNK_SYNC_BINLOG_FILENAME	"binlog"
#define TRUNK_SYNC_BINLOG_ROLLBACK_EXT	".rollback"
#define TRUNK_SYNC_MARK_FILE_EXT	".mark"
#define TRUNK_DIR_NAME			"trunk"
#define MARK_ITEM_BINLOG_FILE_OFFSET	"binlog_offset"

static int trunk_binlog_fd = -1;

int g_trunk_sync_thread_count = 0;
static pthread_mutex_t trunk_sync_thread_lock;
static char *trunk_binlog_write_cache_buff = NULL;
static int trunk_binlog_write_cache_len = 0;
static int trunk_binlog_write_version = 1;

/* save sync thread ids */
static pthread_t *trunk_sync_tids = NULL;

static int trunk_write_to_mark_file(TrunkBinLogReader *pReader);
static int trunk_binlog_fsync_ex(const bool bNeedLock, \
		const char *buff, int *length);
static int trunk_binlog_preread(TrunkBinLogReader *pReader);

#define trunk_binlog_fsync(bNeedLock) trunk_binlog_fsync_ex(bNeedLock, \
	trunk_binlog_write_cache_buff, (&trunk_binlog_write_cache_len))

char *get_trunk_binlog_filename(char *full_filename)
{
	snprintf(full_filename, MAX_PATH_SIZE, \
		"%s/data/"TRUNK_DIR_NAME"/"TRUNK_SYNC_BINLOG_FILENAME, \
		g_fdfs_base_path);
	return full_filename;
}

static char *get_trunk_rollback_filename(char *full_filename)
{
	get_trunk_binlog_filename(full_filename);
	if (strlen(full_filename) + sizeof(TRUNK_SYNC_BINLOG_ROLLBACK_EXT) >
		MAX_PATH_SIZE)
	{
		return NULL;
	}
	strcat(full_filename, TRUNK_SYNC_BINLOG_ROLLBACK_EXT);
	return full_filename;
}

static int trunk_binlog_open_writer(const char *binlog_filename)
{
	trunk_binlog_fd = open(binlog_filename, O_WRONLY | O_CREAT |
				O_APPEND, 0644);
	if (trunk_binlog_fd < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, binlog_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EACCES;
	}

	return 0;
}

static int trunk_binlog_close_writer(const bool needLock)
{
	int result;
	if (trunk_binlog_write_cache_len > 0)
	{
		if ((result=trunk_binlog_fsync(needLock)) != 0)
		{
			return result;
		}
	}
	close(trunk_binlog_fd);
	trunk_binlog_fd = -1;
	return 0;
}

int trunk_sync_init()
{
	char data_path[MAX_PATH_SIZE];
	char sync_path[MAX_PATH_SIZE];
	char binlog_filename[MAX_PATH_SIZE];
	int result;

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
			"%s/"TRUNK_DIR_NAME, data_path);
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

	trunk_binlog_write_cache_buff = (char *)malloc( \
					TRUNK_BINLOG_BUFFER_SIZE);
	if (trunk_binlog_write_cache_buff == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, TRUNK_BINLOG_BUFFER_SIZE, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}

	get_trunk_binlog_filename(binlog_filename);
	if ((result=trunk_binlog_open_writer(binlog_filename)) != 0)
	{
		return result;
	}

	if ((result=init_pthread_lock(&trunk_sync_thread_lock)) != 0)
	{
		return result;
	}

	STORAGE_FCHOWN(trunk_binlog_fd, binlog_filename, geteuid(), getegid())

	return 0;
}

int trunk_sync_destroy()
{
	if (trunk_binlog_fd >= 0)
	{
		trunk_binlog_fsync(true);
		close(trunk_binlog_fd);
		trunk_binlog_fd = -1;
	}

	return 0;
}

int kill_trunk_sync_threads()
{
	int result;
	int kill_res;

	if (trunk_sync_tids == NULL)
	{
		return 0;
	}

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	kill_res = kill_work_threads(trunk_sync_tids, g_trunk_sync_thread_count);

	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	while (g_trunk_sync_thread_count > 0)
	{
		usleep(50000);
	}

	return kill_res;
}

int trunk_binlog_sync_func(void *args)
{
	if (trunk_binlog_write_cache_len > 0)
	{
		return trunk_binlog_fsync(true);
	}
	else
	{
		return 0;
	}
}

int trunk_binlog_truncate()
{
	int result;

	if (trunk_binlog_write_cache_len > 0)
	{
		if ((result=trunk_binlog_fsync(true)) != 0)
		{
			return result;
		}
	}

	if (ftruncate(trunk_binlog_fd, 0) != 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"call ftruncate fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	return 0;
}

int trunk_binlog_compress_apply()
{
	int result;
	char binlog_filename[MAX_PATH_SIZE];
	char rollback_filename[MAX_PATH_SIZE];

	get_trunk_binlog_filename(binlog_filename);
	if (get_trunk_rollback_filename(rollback_filename) == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"filename: %s is too long",
			__LINE__, binlog_filename);
		return ENAMETOOLONG;
	}

	if (trunk_binlog_fd < 0)
	{
		if (access(binlog_filename, F_OK) == 0)
		{
			if (rename(binlog_filename, rollback_filename) != 0)
			{
				result = errno != 0 ? errno : EIO;
				logError("file: "__FILE__", line: %d, " \
					"rename %s to %s fail, " \
					"errno: %d, error info: %s",
					__LINE__, binlog_filename,
					rollback_filename, result,
					STRERROR(result));
				return result;
			}
		}
		else if (errno != ENOENT)
		{
			result = errno != 0 ? errno : EIO;
			logError("file: "__FILE__", line: %d, " \
				"call access %s fail, " \
				"errno: %d, error info: %s",
				__LINE__, binlog_filename,
				result, STRERROR(result));
			return result;
		}

		return 0;
	}

	if ((result=trunk_binlog_close_writer(true)) != 0)
	{
		return result;
	}

	if (rename(binlog_filename, rollback_filename) != 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"rename %s to %s fail, " \
			"errno: %d, error info: %s",
			__LINE__, binlog_filename, rollback_filename,
			result, STRERROR(result));
		return result;
	}

	if ((result=trunk_binlog_open_writer(binlog_filename)) != 0)
	{
		rename(rollback_filename, binlog_filename);  //rollback
		return result;
	}

	return 0;
}

static int trunk_binlog_open_read(const char *filename,
	const bool skipFirstLine)
{
	int result;
	int fd;
	char buff[32];

	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		result = errno != 0 ? errno : EACCES;
		logError("file: "__FILE__", line: %d, " \
			"open file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, filename, result, STRERROR(result));
		return -1;
	}

	if (skipFirstLine)
	{
		if (fd_gets(fd, buff, sizeof(buff), 16) <= 0)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"skip first line fail!", __LINE__);
		}
	}

	return fd;
}

static int trunk_binlog_merge_file(int old_fd)
{
	int result;
	int tmp_fd;
	int bytes;
	char binlog_filename[MAX_PATH_SIZE];
	char tmp_filename[MAX_PATH_SIZE];
	char buff[64 * 1024];

	get_trunk_binlog_filename(binlog_filename);
	sprintf(tmp_filename, "%s.tmp", binlog_filename);
	tmp_fd = open(tmp_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (tmp_fd < 0)
	{
		result = errno != 0 ? errno : EACCES;
		logError("file: "__FILE__", line: %d, " \
			"open file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, tmp_filename, result, STRERROR(result));
		return result;
	}

	while ((bytes=read(old_fd, buff, sizeof(buff))) > 0)
	{
		if (write(tmp_fd, buff, bytes) != bytes)
		{
			result = errno != 0 ? errno : EACCES;
			logError("file: "__FILE__", line: %d, " \
				"write to file \"%s\" fail, " \
				"errno: %d, error info: %s", \
				__LINE__, tmp_filename,
				result, STRERROR(result));
			close(tmp_fd);
			return result;
		}
	}

	if (access(binlog_filename, F_OK) == 0)
	{
		int binlog_fd;
		if ((binlog_fd=trunk_binlog_open_read(binlog_filename,
			false)) < 0)
		{
			close(tmp_fd);
			return errno != 0 ? errno : EPERM;
		}

		while ((bytes=read(binlog_fd, buff, sizeof(buff))) > 0)
		{
			if (write(tmp_fd, buff, bytes) != bytes)
			{
				result = errno != 0 ? errno : EACCES;
				logError("file: "__FILE__", line: %d, " \
					"write to file \"%s\" fail, " \
					"errno: %d, error info: %s", \
					__LINE__, tmp_filename,
					result, STRERROR(result));
				close(tmp_fd);
				close(binlog_fd);
				return result;
			}
		}
		close(binlog_fd);
	}

	if (fsync(tmp_fd) != 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"sync file \"%s\" fail, " \
			"errno: %d, error info: %s",  \
			__LINE__, tmp_filename, \
			errno, STRERROR(errno));
		close(tmp_fd);
		return result;
	}
	close(tmp_fd);

	if (rename(tmp_filename, binlog_filename) != 0)
	{
		result = errno != 0 ? errno : EPERM;
		logError("file: "__FILE__", line: %d, " \
			"rename %s to %s fail, " \
			"errno: %d, error info: %s",
			__LINE__, tmp_filename, binlog_filename,
			result, STRERROR(result));
		return result;
	}

	return 0;
}

int trunk_binlog_compress_commit()
{
	int result;
	int data_fd;
	bool need_open_binlog;
	char binlog_filename[MAX_PATH_SIZE];
	char data_filename[MAX_PATH_SIZE];
	char rollback_filename[MAX_PATH_SIZE];

	need_open_binlog = trunk_binlog_fd >= 0;
	get_trunk_binlog_filename(binlog_filename);
	storage_trunk_get_data_filename(data_filename);

	if ((data_fd=trunk_binlog_open_read(data_filename, true)) < 0)
	{
		return errno != 0 ? errno : ENOENT;
	}

	if (need_open_binlog)
	{
		trunk_binlog_close_writer(true);
	}

	result = trunk_binlog_merge_file(data_fd);
	close(data_fd);
	if (result != 0)
	{
		return result;
	}
	if (unlink(data_filename) != 0)
	{
		result = errno != 0 ? errno : EPERM;
		logError("file: "__FILE__", line: %d, " \
			"unlink %s fail, errno: %d, error info: %s",
			__LINE__, data_filename,
			result, STRERROR(result));
		return result;
	}

	get_trunk_rollback_filename(rollback_filename);
	if (access(rollback_filename, F_OK) == 0)
	{
		if (unlink(rollback_filename) != 0)
		{
			result = errno != 0 ? errno : EPERM;
			logWarning("file: "__FILE__", line: %d, " \
				"unlink %s fail, errno: %d, error info: %s",
				__LINE__, rollback_filename,
				result, STRERROR(result));
		}
	}

	if (need_open_binlog)
	{
		return trunk_binlog_open_writer(binlog_filename);
	}

	return 0;
}

int trunk_binlog_compress_rollback()
{
	int result;
	int rollback_fd;
	char binlog_filename[MAX_PATH_SIZE];
	char rollback_filename[MAX_PATH_SIZE];
	struct stat fs;

	get_trunk_binlog_filename(binlog_filename);
	get_trunk_rollback_filename(rollback_filename);
	if (trunk_binlog_fd < 0)
	{
		if (access(rollback_filename, F_OK) == 0)
		{
			if (rename(rollback_filename, binlog_filename) != 0)
			{
				result = errno != 0 ? errno : EPERM;
				logError("file: "__FILE__", line: %d, "\
					"rename %s to %s fail, " \
					"errno: %d, error info: %s",
					__LINE__, rollback_filename,
					binlog_filename, result,
					STRERROR(result));
				return result;
			}
		}

		return 0;
	}

	if (stat(rollback_filename, &fs) != 0)
	{
		result = errno != 0 ? errno : ENOENT;
		if (result == ENOENT)
		{
			return 0;
		}
		logError("file: "__FILE__", line: %d, " \
			"stat file %s fail, errno: %d, error info: %s",
			__LINE__, rollback_filename,
			result, STRERROR(result));
		return result;
	}

	if (fs.st_size == 0)
	{
		unlink(rollback_filename);  //delete zero file directly
		return 0;
	}

	if ((result=trunk_binlog_close_writer(true)) != 0)
	{
		return result;
	}

	if ((rollback_fd=trunk_binlog_open_read(rollback_filename,
		false)) < 0)
	{
		return errno != 0 ? errno : ENOENT;
	}

	result = trunk_binlog_merge_file(rollback_fd);
	close(rollback_fd);
	if (result == 0)
	{
		if (unlink(rollback_filename) != 0)
		{
			result = errno != 0 ? errno : EPERM;
			logWarning("file: "__FILE__", line: %d, " \
				"unlink %s fail, " \
				"errno: %d, error info: %s",
				__LINE__, rollback_filename,
				result, STRERROR(result));
		}

		return trunk_binlog_open_writer(binlog_filename);
	}
	else
	{
		trunk_binlog_open_writer(binlog_filename);
		return result;
	}
}

static int trunk_binlog_fsync_ex(const bool bNeedLock, \
		const char *buff, int *length)
{
	int result;
	int write_ret;
	char full_filename[MAX_PATH_SIZE];

	if (bNeedLock && (result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if (*length == 0) //ignore
	{
		write_ret = 0;  //skip
	}
	else if (write(trunk_binlog_fd, buff, *length) != *length)
	{
		logError("file: "__FILE__", line: %d, " \
			"write to binlog file \"%s\" fail, fd=%d, " \
			"errno: %d, error info: %s",  \
			__LINE__, get_trunk_binlog_filename(full_filename), \
			trunk_binlog_fd, errno, STRERROR(errno));
		write_ret = errno != 0 ? errno : EIO;
	}
	else if (fsync(trunk_binlog_fd) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"sync to binlog file \"%s\" fail, " \
			"errno: %d, error info: %s",  \
			__LINE__, get_trunk_binlog_filename(full_filename), \
			errno, STRERROR(errno));
		write_ret = errno != 0 ? errno : EIO;
	}
	else
	{
		write_ret = 0;
	}

	if (write_ret == 0)
	{
		trunk_binlog_write_version++;
		*length = 0;  //reset cache buff
	}

	if (bNeedLock && (result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return write_ret;
}

int trunk_binlog_write(const int timestamp, const char op_type, \
		const FDFSTrunkFullInfo *pTrunk)
{
	int result;
	int write_ret;

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	trunk_binlog_write_cache_len += sprintf(trunk_binlog_write_cache_buff + \
					trunk_binlog_write_cache_len, \
					"%d %c %d %d %d %d %d %d\n", \
					timestamp, op_type, \
					pTrunk->path.store_path_index, \
					pTrunk->path.sub_path_high, \
					pTrunk->path.sub_path_low, \
					pTrunk->file.id, \
					pTrunk->file.offset, \
					pTrunk->file.size);

	//check if buff full
	if (TRUNK_BINLOG_BUFFER_SIZE - trunk_binlog_write_cache_len < 128)
	{
		write_ret = trunk_binlog_fsync(false);  //sync to disk
	}
	else
	{
		write_ret = 0;
	}

	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return write_ret;
}

int trunk_binlog_write_buffer(const char *buff, const int length)
{
	int result;
	int write_ret;

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	//check if buff full
	if (TRUNK_BINLOG_BUFFER_SIZE - (trunk_binlog_write_cache_len + \
			length) < 128)
	{
		write_ret = trunk_binlog_fsync(false);  //sync to disk
	}
	else
	{
		write_ret = 0;
	}

	if (write_ret == 0)
	{
		if (length >= TRUNK_BINLOG_BUFFER_SIZE)
		{
			if (trunk_binlog_write_cache_len > 0)
			{
				write_ret = trunk_binlog_fsync(false);
			}

			if (write_ret == 0)
			{
				int len;
				len = length;
				write_ret = trunk_binlog_fsync_ex(false, \
					buff, &len);
			}
		}
		else
		{
			memcpy(trunk_binlog_write_cache_buff + \
				trunk_binlog_write_cache_len, buff, length);
			trunk_binlog_write_cache_len += length;
		}
	}
	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
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
	static char buff[MAX_PATH_SIZE];

	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	snprintf(full_filename, MAX_PATH_SIZE, 
		"%s/data/"TRUNK_DIR_NAME"/"TRUNK_SYNC_BINLOG_FILENAME, \
		g_fdfs_base_path);
	return full_filename;
}

int trunk_open_readable_binlog(TrunkBinLogReader *pReader, \
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

static char *trunk_get_mark_filename_by_id_and_port(const char *storage_id, \
		const int port, char *full_filename, const int filename_size)
{
	if (g_use_storage_id)
	{
		snprintf(full_filename, filename_size, \
			"%s/data/"TRUNK_DIR_NAME"/%s%s", g_fdfs_base_path, \
			storage_id, TRUNK_SYNC_MARK_FILE_EXT);
	}
	else
	{
		snprintf(full_filename, filename_size, \
			"%s/data/"TRUNK_DIR_NAME"/%s_%d%s", g_fdfs_base_path, \
			storage_id, port, TRUNK_SYNC_MARK_FILE_EXT);
	}

	return full_filename;
}

static char *trunk_get_mark_filename_by_ip_and_port(const char *ip_addr, \
		const int port, char *full_filename, const int filename_size)
{
	snprintf(full_filename, filename_size, \
		"%s/data/"TRUNK_DIR_NAME"/%s_%d%s", g_fdfs_base_path, \
		ip_addr, port, TRUNK_SYNC_MARK_FILE_EXT);

	return full_filename;
}

char *trunk_mark_filename_by_reader(const void *pArg, char *full_filename)
{
	const TrunkBinLogReader *pReader;
	static char buff[MAX_PATH_SIZE];

	pReader = (const TrunkBinLogReader *)pArg;
	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	return trunk_get_mark_filename_by_id_and_port(pReader->storage_id, \
			g_server_port, full_filename, MAX_PATH_SIZE);
}

static char *trunk_get_mark_filename_by_id(const char *storage_id, 
	char *full_filename, const int filename_size)
{
	return trunk_get_mark_filename_by_id_and_port(storage_id, g_server_port, \
				full_filename, filename_size);
}

int trunk_reader_init(FDFSStorageBrief *pStorage, TrunkBinLogReader *pReader)
{
	char full_filename[MAX_PATH_SIZE];
	IniContext iniContext;
	int result;
	int64_t saved_binlog_offset;
	bool bFileExist;

	saved_binlog_offset = pReader->binlog_offset;

	memset(pReader, 0, sizeof(TrunkBinLogReader));
	pReader->mark_fd = -1;
	pReader->binlog_fd = -1;

	pReader->binlog_buff.buffer = (char *)malloc( \
				TRUNK_BINLOG_BUFFER_SIZE);
	if (pReader->binlog_buff.buffer == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, TRUNK_BINLOG_BUFFER_SIZE, \
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
	trunk_mark_filename_by_reader(pReader, full_filename);

	if (pStorage == NULL)
	{
		bFileExist = false;
		pReader->binlog_offset = saved_binlog_offset;
	}
	else
	{
		bFileExist = fileExists(full_filename);
		if (!bFileExist && (g_use_storage_id && pStorage != NULL))
		{
			char old_mark_filename[MAX_PATH_SIZE];
			trunk_get_mark_filename_by_ip_and_port( \
				pStorage->ip_addr, g_server_port, \
				old_mark_filename, sizeof(old_mark_filename));
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

		if (iniContext.global.count < 1)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, " \
				"in mark file \"%s\", item count: %d < 7", \
				__LINE__, full_filename, iniContext.global.count);
			return ENOENT;
		}

		pReader->binlog_offset = iniGetInt64Value(NULL, \
					MARK_ITEM_BINLOG_FILE_OFFSET, \
					&iniContext, -1);
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

		iniFreeContext(&iniContext);
	}

	pReader->last_binlog_offset = pReader->binlog_offset;

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

	if (!bFileExist && pStorage != NULL)
	{
		if ((result=trunk_write_to_mark_file(pReader)) != 0)
		{
			return result;
		}
	}

	if ((result=trunk_open_readable_binlog(pReader, \
			get_binlog_readable_filename, pReader)) != 0)
	{
		return result;
	}

	result = trunk_binlog_preread(pReader);
	if (result != 0 && result != ENOENT)
	{
		return result;
	}

	return 0;
}

void trunk_reader_destroy(TrunkBinLogReader *pReader)
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

static int trunk_write_to_mark_file(TrunkBinLogReader *pReader)
{
	char buff[128];
	int len;
	int result;

	len = sprintf(buff, \
		"%s=%"PRId64"\n",  \
		MARK_ITEM_BINLOG_FILE_OFFSET, pReader->binlog_offset);

	if ((result=storage_write_to_fd(pReader->mark_fd, \
		trunk_mark_filename_by_reader, pReader, buff, len)) == 0)
	{
		pReader->last_binlog_offset = pReader->binlog_offset;
	}

	return result;
}

static int trunk_binlog_preread(TrunkBinLogReader *pReader)
{
	int bytes_read;
	int saved_trunk_binlog_write_version;

	if (pReader->binlog_buff.version == trunk_binlog_write_version && \
		pReader->binlog_buff.length == 0)
	{
		return ENOENT;
	}

	if (pReader->binlog_buff.length == TRUNK_BINLOG_BUFFER_SIZE) //buff full
	{
		return 0;
	}

	saved_trunk_binlog_write_version = trunk_binlog_write_version;
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
		TRUNK_BINLOG_BUFFER_SIZE - pReader->binlog_buff.length);
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
		pReader->binlog_buff.version = saved_trunk_binlog_write_version;
		return (pReader->binlog_buff.length == 0) ? ENOENT : 0;
	}

	pReader->binlog_buff.length += bytes_read;
	return 0;
}

static int trunk_binlog_do_line_read(TrunkBinLogReader *pReader, \
		char *line, const int line_size, int *line_length)
{
	char *pLineEnd;

	if (pReader->binlog_buff.length == 0)
	{
		return ENOENT;
	}

	pLineEnd = (char *)memchr(pReader->binlog_buff.current, '\n', \
			pReader->binlog_buff.length);
	if (pLineEnd == NULL)
	{
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

static int trunk_binlog_read_line(TrunkBinLogReader *pReader, \
		char *line, const int line_size, int *line_length)
{
	int result;

	result = trunk_binlog_do_line_read(pReader, line, \
			line_size, line_length);
	if (result != ENOENT)
	{
		return result;
	}

	result = trunk_binlog_preread(pReader);
	if (result != 0)
	{
		return result;
	}

	return trunk_binlog_do_line_read(pReader, line, \
			line_size, line_length);
}

int trunk_binlog_read(TrunkBinLogReader *pReader, \
			TrunkBinLogRecord *pRecord, int *record_length)
{
#define COL_COUNT  8
	char line[TRUNK_BINLOG_LINE_SIZE];
	char *cols[COL_COUNT];
	int result;

	result = trunk_binlog_read_line(pReader, line, \
				sizeof(line), record_length);
	if (result != 0)
	{
		return result;
	}

	if ((result=splitEx(line, ' ', cols, COL_COUNT)) < COL_COUNT)
	{
		logError("file: "__FILE__", line: %d, " \
			"read data from binlog file \"%s\" fail, " \
			"file offset: %"PRId64", " \
			"read item count: %d < %d", \
			__LINE__, get_binlog_readable_filename(pReader, NULL), \
			pReader->binlog_offset, result, COL_COUNT);
		return ENOENT;
	}

	pRecord->timestamp = atoi(cols[0]);
	pRecord->op_type = *(cols[1]);
	pRecord->trunk.path.store_path_index = atoi(cols[2]);
	pRecord->trunk.path.sub_path_high = atoi(cols[3]);
	pRecord->trunk.path.sub_path_low = atoi(cols[4]);
	pRecord->trunk.file.id = atoi(cols[5]);
	pRecord->trunk.file.offset = atoi(cols[6]);
	pRecord->trunk.file.size = atoi(cols[7]);

	return 0;
}

int trunk_unlink_mark_file(const char *storage_id)
{
	char old_filename[MAX_PATH_SIZE];
	char new_filename[MAX_PATH_SIZE];
	time_t t;
	struct tm tm;

	t = g_current_time;
	localtime_r(&t, &tm);

	trunk_get_mark_filename_by_id(storage_id, old_filename, \
		sizeof(old_filename));
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

int trunk_rename_mark_file(const char *old_ip_addr, const int old_port, \
		const char *new_ip_addr, const int new_port)
{
	char old_filename[MAX_PATH_SIZE];
	char new_filename[MAX_PATH_SIZE];

	trunk_get_mark_filename_by_id_and_port(old_ip_addr, old_port, \
			old_filename, sizeof(old_filename));
	if (!fileExists(old_filename))
	{
		return ENOENT;
	}

	trunk_get_mark_filename_by_id_and_port(new_ip_addr, new_port, \
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

static void trunk_sync_thread_exit(ConnectionInfo *pStorage)
{
	int result;
	int i;
	pthread_t tid;

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	tid = pthread_self();
	for (i=0; i<g_trunk_sync_thread_count; i++)
	{
		if (pthread_equal(trunk_sync_tids[i], tid))
		{
			break;
		}
	}

	while (i < g_trunk_sync_thread_count - 1)
	{
		trunk_sync_tids[i] = trunk_sync_tids[i + 1];
		i++;
	}
	
	g_trunk_sync_thread_count--;

	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	logInfo("file: "__FILE__", line: %d, " \
		"trunk sync thread to storage server %s:%d exit", 
		__LINE__, pStorage->ip_addr, pStorage->port);
}

static int trunk_sync_data(TrunkBinLogReader *pReader, \
		ConnectionInfo *pStorage)
{
	int length;
	char *p;
	int result;
	TrackerHeader header;
	char in_buff[1];
	char *pBuff;
	int64_t in_bytes;

	p = pReader->binlog_buff.buffer + pReader->binlog_buff.length - 1;
	while (p != pReader->binlog_buff.buffer && *p != '\n')
	{
		p--;
	}

	length = p - pReader->binlog_buff.buffer;
	if (length == 0)
	{
		logWarning("FILE: "__FILE__", line: %d, " \
			"no buffer to sync, buffer length: %d, " \
			"should try again later", __LINE__, \
			pReader->binlog_buff.length);
		return ENOENT;
	}
	length++;

	memset(&header, 0, sizeof(header));
	long2buff(length, header.pkg_len);
	header.cmd = STORAGE_PROTO_CMD_TRUNK_SYNC_BINLOG;
	if ((result=tcpsenddata_nb(pStorage->sock, &header, \
		sizeof(TrackerHeader), g_fdfs_network_timeout)) != 0)
	{
		logError("FILE: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pStorage->ip_addr, pStorage->port, \
			result, STRERROR(result));
		return result;
	}

	if ((result=tcpsenddata_nb(pStorage->sock, pReader->binlog_buff.buffer,\
		length, g_fdfs_network_timeout)) != 0)
	{
		logError("FILE: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pStorage->ip_addr, pStorage->port, \
			result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	if ((result=fdfs_recv_response(pStorage, &pBuff, 0, &in_bytes)) != 0)
	{
		return result;
	}

	pReader->binlog_offset += length;
	pReader->binlog_buff.length -= length;
	if (pReader->binlog_buff.length > 0)
	{
		pReader->binlog_buff.current = pReader->binlog_buff.buffer + length;
	}

	return 0;
}

static void* trunk_sync_thread_entrance(void* arg)
{
	FDFSStorageBrief *pStorage;
	TrunkBinLogReader reader;
	ConnectionInfo storage_server;
	char local_ip_addr[IP_ADDRESS_SIZE];
	int read_result;
	int sync_result;
	int conn_result;
	int result;
	int previousCode;
	int nContinuousFail;
	time_t current_time;
	time_t last_keep_alive_time;
	
	memset(local_ip_addr, 0, sizeof(local_ip_addr));
	memset(&reader, 0, sizeof(reader));
	reader.mark_fd = -1;
	reader.binlog_fd = -1;

	current_time =  g_current_time;
	last_keep_alive_time = 0;

	pStorage = (FDFSStorageBrief *)arg;

	strcpy(storage_server.ip_addr, pStorage->ip_addr);
	storage_server.port = g_server_port;
	storage_server.sock = -1;

	logInfo("file: "__FILE__", line: %d, " \
		"trunk sync thread to storage server %s:%d started", \
		__LINE__, storage_server.ip_addr, storage_server.port);

	while (g_continue_flag && g_if_trunker_self && \
		pStorage->status != FDFS_STORAGE_STATUS_DELETED && \
		pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED && \
		pStorage->status != FDFS_STORAGE_STATUS_NONE)
	{
		previousCode = 0;
		nContinuousFail = 0;
		conn_result = 0;
		while (g_continue_flag && g_if_trunker_self && \
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
					pStorage->ip_addr, g_server_port, \
					szFailPrompt);
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

		if ((!g_continue_flag) || (!g_if_trunker_self) || \
			pStorage->status == FDFS_STORAGE_STATUS_DELETED || \
			pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED || \
			pStorage->status == FDFS_STORAGE_STATUS_NONE)
		{
			logError("file: "__FILE__", line: %d, break loop." \
				"g_continue_flag: %d, g_if_trunker_self: %d, " \
				"dest storage status: %d", __LINE__, \
				g_continue_flag, g_if_trunker_self, \
				pStorage->status);
			break;
		}

		if ((result=trunk_reader_init(pStorage, &reader)) != 0)
		{
			logCrit("file: "__FILE__", line: %d, " \
				"trunk_reader_init fail, errno=%d, " \
				"program exit!", \
				__LINE__, result);
			g_continue_flag = false;
			break;
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
				" trunk sync thread exit.", \
				__LINE__, pStorage->ip_addr);
			fdfs_quit(&storage_server);
			close(storage_server.sock);
			break;
		}

		if (reader.binlog_offset == 0)
		{
			if (fdfs_deal_no_body_cmd(&storage_server, \
				STORAGE_PROTO_CMD_TRUNK_TRUNCATE_BINLOG_FILE) != 0)
			{
				close(storage_server.sock);
				trunk_reader_destroy(&reader);
				sleep(5);
				continue;
			}
		}

		sync_result = 0;
		while (g_continue_flag && \
			pStorage->status != FDFS_STORAGE_STATUS_DELETED && \
			pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED && \
			pStorage->status != FDFS_STORAGE_STATUS_NONE)
		{
			read_result = trunk_binlog_preread(&reader);
			if (read_result == ENOENT)
			{
				if (reader.last_binlog_offset != \
					reader.binlog_offset)
				{
					if (trunk_write_to_mark_file(&reader)!=0)
					{
					logCrit("file: "__FILE__", line: %d, " \
						"trunk_write_to_mark_file fail, " \
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

				if (!g_if_trunker_self)
				{
					break;
				}

				usleep(g_sync_wait_usec);
				continue;
			}

			if (read_result != 0)
			{
				sleep(5);
				continue;
			}

			if ((sync_result=trunk_sync_data(&reader, \
				&storage_server)) != 0)
			{
				break;
			}

			if (g_sync_interval > 0)
			{
				usleep(g_sync_interval);
			}
		}

		if (reader.last_binlog_offset != reader.binlog_offset)
		{
			if (trunk_write_to_mark_file(&reader) != 0)
			{
				logCrit("file: "__FILE__", line: %d, " \
					"trunk_write_to_mark_file fail, " \
					"program exit!", __LINE__);
				g_continue_flag = false;
				break;
			}
		}

		close(storage_server.sock);
		storage_server.sock = -1;
		trunk_reader_destroy(&reader);

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
	trunk_reader_destroy(&reader);

	trunk_sync_thread_exit(&storage_server);

	return NULL;
}

int trunk_sync_thread_start_all()
{
	FDFSStorageServer *pServer;
	FDFSStorageServer *pEnd;
	int result;
	int ret;

	result = 0;
	pEnd = g_storage_servers + g_storage_count;
	for (pServer=g_storage_servers; pServer<pEnd; pServer++)
	{
		ret = trunk_sync_thread_start(&(pServer->server));
		if (ret != 0)
		{
			result = ret;
		}
	}

	return result;
}

int trunk_sync_thread_start(const FDFSStorageBrief *pStorage)
{
	int result;
	pthread_attr_t pattr;
	pthread_t tid;

	if (pStorage->status == FDFS_STORAGE_STATUS_DELETED || \
	    pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED || \
	    pStorage->status == FDFS_STORAGE_STATUS_NONE)
	{
		return 0;
	}

	if (is_local_host_ip(pStorage->ip_addr)) //can't self sync to self
	{
		return 0;
	}

	if ((result=init_pthread_attr(&pattr, g_thread_stack_size)) != 0)
	{
		return result;
	}

	/*
	//printf("start storage ip_addr: %s, g_trunk_sync_thread_count=%d\n", 
			pStorage->ip_addr, g_trunk_sync_thread_count);
	*/

	if ((result=pthread_create(&tid, &pattr, trunk_sync_thread_entrance, \
		(void *)pStorage)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"create thread failed, errno: %d, " \
			"error info: %s", \
			__LINE__, result, STRERROR(result));

		pthread_attr_destroy(&pattr);
		return result;
	}

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	g_trunk_sync_thread_count++;
	trunk_sync_tids = (pthread_t *)realloc(trunk_sync_tids, sizeof(pthread_t) * \
					g_trunk_sync_thread_count);
	if (trunk_sync_tids == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, (int)sizeof(pthread_t) * \
			g_trunk_sync_thread_count, \
			errno, STRERROR(errno));
	}
	else
	{
		trunk_sync_tids[g_trunk_sync_thread_count - 1] = tid;
	}

	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	pthread_attr_destroy(&pattr);

	return 0;
}

int trunk_unlink_all_mark_files()
{
	FDFSStorageServer *pStorageServer;
	FDFSStorageServer *pServerEnd;
	int result;

	pServerEnd = g_storage_servers + g_storage_count;
	for (pStorageServer=g_storage_servers; pStorageServer<pServerEnd; 
		pStorageServer++)
	{
		if (storage_server_is_myself(&(pStorageServer->server)))
		{
			continue;
		}

		if ((result=trunk_unlink_mark_file( \
			pStorageServer->server.id)) != 0)
		{
			if (result != ENOENT)
			{
				return result;
			}
		}
	}

	return 0;
}

