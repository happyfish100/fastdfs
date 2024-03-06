/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//trunk_sync.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "fastcommon/sockopt.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/pthread_func.h"
#include "fastcommon/sched_thread.h"
#include "fastcommon/ini_file_reader.h"
#include "fastcommon/fc_atomic.h"
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_ip_changed_dealer.h"
#include "tracker_client_thread.h"
#include "storage_client.h"
#include "storage_sync_func.h"
#include "trunk_sync.h"

#define TRUNK_SYNC_BINLOG_FILENAME_STR	"binlog"
#define TRUNK_SYNC_BINLOG_FILENAME_LEN  (sizeof(TRUNK_SYNC_BINLOG_FILENAME_STR) - 1)
#define TRUNK_SYNC_BINLOG_ROLLBACK_EXT	".rollback"
#define TRUNK_SYNC_MARK_FILE_EXT_STR	".mark"
#define TRUNK_SYNC_MARK_FILE_EXT_LEN	(sizeof(TRUNK_SYNC_MARK_FILE_EXT_STR) - 1)
#define TRUNK_DIR_NAME			        "trunk"
#define MARK_ITEM_BINLOG_FILE_OFFSET	"binlog_offset"

static int trunk_binlog_fd = -1;

volatile int g_trunk_sync_thread_count = 0;
static pthread_mutex_t trunk_sync_thread_lock;
static char *trunk_binlog_write_cache_buff = NULL;
static int trunk_binlog_write_cache_len = 0;
static int trunk_binlog_write_version = 1;

typedef struct
{
    bool running;
    bool reset_binlog_offset;
    int thread_index;
    const FDFSStorageBrief *pStorage;
    pthread_t tid;
} TrunkSyncThreadInfo;

typedef struct
{
    TrunkSyncThreadInfo **thread_data;
    int alloc_count;
} TrunkSyncThreadInfoArray;

/* save sync thread ids */
static TrunkSyncThreadInfoArray sync_thread_info_array = {NULL, 0};

static int trunk_write_to_mark_file(TrunkBinLogReader *pReader);
static int trunk_binlog_fsync_ex(const bool bNeedLock, \
		const char *buff, int *length);
static int trunk_binlog_preread(TrunkBinLogReader *pReader);

#define trunk_binlog_fsync(bNeedLock) trunk_binlog_fsync_ex(bNeedLock, \
	trunk_binlog_write_cache_buff, (&trunk_binlog_write_cache_len))

char *get_trunk_binlog_filename(char *full_filename)
{
	snprintf(full_filename, MAX_PATH_SIZE, \
		"%s/data/"TRUNK_DIR_NAME"/"TRUNK_SYNC_BINLOG_FILENAME_STR, \
		SF_G_BASE_PATH_STR);
	return full_filename;
}

static char *get_trunk_binlog_rollback_filename(char *full_filename)
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

static char *get_trunk_data_rollback_filename(char *full_filename)
{
	storage_trunk_get_data_filename(full_filename);
	if (strlen(full_filename) + sizeof(TRUNK_SYNC_BINLOG_ROLLBACK_EXT) >
		MAX_PATH_SIZE)
	{
		return NULL;
	}
	strcat(full_filename, TRUNK_SYNC_BINLOG_ROLLBACK_EXT);
	return full_filename;
}

char *get_trunk_binlog_tmp_filename_ex(const char *binlog_filename,
        char *tmp_filename)
{
    const char *true_binlog_filename;
	char filename[MAX_PATH_SIZE];

    if (binlog_filename == NULL)
    {
        get_trunk_binlog_filename(filename);
        true_binlog_filename = filename;
    }
    else
    {
        true_binlog_filename = binlog_filename;
    }

	sprintf(tmp_filename, "%s.tmp", true_binlog_filename);
    return tmp_filename;
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

	snprintf(data_path, sizeof(data_path), "%s/data", SF_G_BASE_PATH_STR);
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

		SF_CHOWN_TO_RUNBY_RETURN_ON_ERROR(sync_path);
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

	SF_FCHOWN_TO_RUNBY_RETURN_ON_ERROR(trunk_binlog_fd, binlog_filename);

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
    TrunkSyncThreadInfo **thread_info;
    TrunkSyncThreadInfo **info_end;

	if (sync_thread_info_array.thread_data == NULL)
	{
		return 0;
	}

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_lock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

    kill_res = 0;
    info_end = sync_thread_info_array.thread_data +
        sync_thread_info_array.alloc_count;
    for (thread_info=sync_thread_info_array.thread_data;
            thread_info<info_end; thread_info++)
    {
        if ((*thread_info)->running && (kill_res=pthread_kill(
                        (*thread_info)->tid, SIGINT)) != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "kill thread failed, "
                    "errno: %d, error info: %s",
                    __LINE__, kill_res, STRERROR(kill_res));
        }
    }

	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_unlock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

	while (FC_ATOMIC_GET(g_trunk_sync_thread_count) > 0)
	{
		usleep(50000);
	}

	return kill_res;
}

int trunk_sync_notify_thread_reset_offset()
{
	int result;
    int i;
    int count;
    bool done;
    TrunkSyncThreadInfo **thread_info;
    TrunkSyncThreadInfo **info_end;

	if (sync_thread_info_array.thread_data == NULL)
	{
		return EINVAL;
	}

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_lock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

    count = 0;
    info_end = sync_thread_info_array.thread_data +
        sync_thread_info_array.alloc_count;
    for (thread_info=sync_thread_info_array.thread_data;
            thread_info<info_end; thread_info++)
    {
        if ((*thread_info)->running)
        {
            (*thread_info)->reset_binlog_offset = true;
            count++;
        }
    }

	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_unlock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

	logInfo("file: "__FILE__", line: %d, "
            "notify %d trunk sync threads to reset offset.",
            __LINE__, count);

    done = false;
    for (i=0; i<300 && SF_G_CONTINUE_FLAG; i++)
    {
        info_end = sync_thread_info_array.thread_data +
            sync_thread_info_array.alloc_count;
        for (thread_info=sync_thread_info_array.thread_data;
                thread_info<info_end; thread_info++)
        {
            if ((*thread_info)->running && (*thread_info)->reset_binlog_offset)
            {
                break;
            }
        }

        if (thread_info == info_end)
        {
            done = true;
            break;
        }

        sleep(1);
    }

    if (done)
    {
        logInfo("file: "__FILE__", line: %d, "
                "trunk sync threads reset binlog offset done.",
                __LINE__);
        return 0;
    }
    else
    {
        count = 0;
        info_end = sync_thread_info_array.thread_data +
            sync_thread_info_array.alloc_count;
        for (thread_info=sync_thread_info_array.thread_data;
                thread_info<info_end; thread_info++)
        {
            if ((*thread_info)->running && (*thread_info)->reset_binlog_offset)
            {
                count++;
            }
        }

        logWarning("file: "__FILE__", line: %d, "
                "%d trunk sync threads reset binlog offset timeout.",
                __LINE__, count);
        return EBUSY;
    }
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

#define BACKUP_FILENAME_LEN  (TRUNK_SYNC_BINLOG_FILENAME_LEN + 15)

typedef struct
{
    char filename[BACKUP_FILENAME_LEN + 1];
} TrunkBinlogBackupFileInfo;

typedef struct
{
    TrunkBinlogBackupFileInfo *files;
    int count;
    int alloc;
} TrunkBinlogBackupFileArray;

static int trunk_binlog_check_alloc_filename_array(
        TrunkBinlogBackupFileArray *file_array)
{
    int bytes;
    TrunkBinlogBackupFileInfo *files;
    int alloc;

    if (file_array->count < file_array->alloc)
    {
        return 0;
    }

    if (file_array->alloc == 0)
    {
        alloc = g_trunk_binlog_max_backups + 1;
    }
    else
    {
        alloc = file_array->alloc * 2;
    }

    bytes = sizeof(TrunkBinlogBackupFileInfo) * alloc;
    files = (TrunkBinlogBackupFileInfo *)malloc(bytes);
    if (files == NULL)
    {
		logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail", __LINE__, bytes);
        return ENOMEM;
    }

    if (file_array->count > 0)
    {
        memcpy(files, file_array->files, sizeof(TrunkBinlogBackupFileInfo) *
                file_array->count);
    }

    if (file_array->files != NULL)
    {
        free(file_array->files);
    }

    file_array->files = files;
    file_array->alloc = alloc;
    return 0;
}

static int trunk_binlog_compare_filename(const void *p1, const void *p2)
{
    return strcmp(((TrunkBinlogBackupFileInfo *)p1)->filename,
            ((TrunkBinlogBackupFileInfo *)p2)->filename);
}

static int trunk_binlog_delete_overflow_backups()
{
#define	BACKUP_FILENAME_PREFIX_STR TRUNK_SYNC_BINLOG_FILENAME_STR"."
#define	BACKUP_FILENAME_PREFIX_LEN (sizeof(BACKUP_FILENAME_PREFIX_STR) - 1)

    int result;
    int i;
    int over_count;
	char file_path[MAX_PATH_SIZE];
	char full_filename[MAX_PATH_SIZE];
    DIR *dir;
    struct dirent *ent;
    TrunkBinlogBackupFileArray file_array;

	snprintf(file_path, sizeof(file_path),
		"%s/data/%s", SF_G_BASE_PATH_STR, TRUNK_DIR_NAME);
    if ((dir=opendir(file_path)) == NULL)
    {
        result = errno != 0 ? errno : EPERM;
		logError("file: "__FILE__", line: %d, "
                "call opendir %s fail, errno: %d, error info: %s",
                __LINE__, file_path, result, STRERROR(result));
        return result;
    }

    result = 0;
    file_array.files = NULL;
    file_array.count = 0;
    file_array.alloc = 0;
    while ((ent=readdir(dir)) != NULL)
    {
        if (strlen(ent->d_name) == BACKUP_FILENAME_LEN &&
                memcmp(ent->d_name, BACKUP_FILENAME_PREFIX_STR,
                    BACKUP_FILENAME_PREFIX_LEN) == 0)
        {
            if ((result=trunk_binlog_check_alloc_filename_array(
                            &file_array)) != 0)
            {
                break;
            }

            strcpy(file_array.files[file_array.count].
                    filename, ent->d_name);
            file_array.count++;
        }
    }

    closedir(dir);

    over_count = (file_array.count - g_trunk_binlog_max_backups) + 1;
    if (result != 0 || over_count <= 0)
    {
        if (file_array.files != NULL)
        {
            free(file_array.files);
        }
        return result;
    }

    qsort(file_array.files, file_array.count,
            sizeof(TrunkBinlogBackupFileInfo),
            trunk_binlog_compare_filename);
    for (i=0; i<over_count; i++)
    {
        sprintf(full_filename, "%s/%s", file_path,
                file_array.files[i].filename);
        unlink(full_filename);
    }

    free(file_array.files);
    return 0;
}

static int trunk_binlog_backup_and_truncate()
{
    int result;
    int open_res;
	char binlog_filename[MAX_PATH_SIZE];
    char backup_filename[MAX_PATH_SIZE];
    time_t t;
    struct tm tm;

    if ((result=trunk_binlog_delete_overflow_backups()) != 0)
    {
        return result;
    }

    if ((result=trunk_binlog_close_writer(false)) != 0)
    {
        return result;
    }

    do
    {
        t = g_current_time;
        localtime_r(&t, &tm);

        get_trunk_binlog_filename(binlog_filename);
        snprintf(backup_filename, sizeof(backup_filename),
                "%s.%04d%02d%02d%02d%02d%02d", binlog_filename,
                tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
        if (rename(binlog_filename, backup_filename) != 0)
        {
            result = errno != 0 ? errno : EACCES;
            logError("file: "__FILE__", line: %d, "
                    "rename file %s to %s fail, "
                    "errno: %d, error info: %s", __LINE__,
                    binlog_filename, backup_filename,
                    result, STRERROR(result));
            break;
        }
    } while (0);

    open_res = trunk_binlog_open_writer(binlog_filename);
    return (result == 0) ? open_res : result;
}

int storage_delete_trunk_data_file()
{
	char trunk_data_filename[MAX_PATH_SIZE];

	storage_trunk_get_data_filename(trunk_data_filename);
    return fc_delete_file_ex(trunk_data_filename, "trunk data");
}

int trunk_binlog_truncate()
{
	int result;

    result = 0;
    pthread_mutex_lock(&trunk_sync_thread_lock);
    do
    {
        if (g_trunk_binlog_max_backups > 0)
        {
            result = trunk_binlog_backup_and_truncate();
        }
        else
        {
            if (trunk_binlog_write_cache_len > 0)
            {
                if ((result=trunk_binlog_fsync(false)) != 0)
                {
                    break;
                }
            }
            if (ftruncate(trunk_binlog_fd, 0) != 0)
            {
                result = errno != 0 ? errno : EIO;
                logError("file: "__FILE__", line: %d, "
                        "call ftruncate fail, "
                        "errno: %d, error info: %s",
                        __LINE__, result, STRERROR(result));
                break;
            }
        }
    } while (0);

    pthread_mutex_unlock(&trunk_sync_thread_lock);
    if (result == 0)
    {
        result = storage_delete_trunk_data_file();
    }

	return result;
}

static int trunk_binlog_delete_rollback_file(const char *filename,
        const bool silence)
{
	int result;
    if (access(filename, F_OK) == 0)
    {
        if (!silence)
        {
            logWarning("file: "__FILE__", line: %d, "
                    "rollback file %s exist, delete it!",
                    __LINE__, filename);
        }
        if (unlink(filename) != 0)
        {
            result = errno != 0 ? errno : EPERM;
            if (result != ENOENT)
            {
                logError("file: "__FILE__", line: %d, "
                        "unlink file %s fail, errno: %d, error info: %s",
                        __LINE__, filename, result, STRERROR(result));
                return result;
            }
        }
    }
    else
    {
        result = errno != 0 ? errno : EPERM;
        if (result != ENOENT)
        {
            logError("file: "__FILE__", line: %d, "
                    "access file %s fail, errno: %d, error info: %s",
                    __LINE__, filename, result, STRERROR(result));
            return result;
        }
    }

    return 0;
}

int trunk_binlog_compress_delete_binlog_rollback_file(const bool silence)
{
	char binlog_rollback_filename[MAX_PATH_SIZE];

	if (get_trunk_binlog_rollback_filename(binlog_rollback_filename) == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"binlog rollback filename is too long", __LINE__);
		return ENAMETOOLONG;
	}

    return trunk_binlog_delete_rollback_file(binlog_rollback_filename, silence);
}

int trunk_binlog_compress_delete_rollback_files(const bool silence)
{
	int result;
	char data_rollback_filename[MAX_PATH_SIZE];

    if ((result=trunk_binlog_compress_delete_binlog_rollback_file(
                    silence)) != 0)
    {
        return result;
    }

    if (get_trunk_data_rollback_filename(data_rollback_filename) == NULL)
    {
		logError("file: "__FILE__", line: %d, "
			"data rollback filename is too long", __LINE__);
		return ENAMETOOLONG;
    }

    if ((result=trunk_binlog_delete_rollback_file(data_rollback_filename,
                    silence)) != 0)
    {
        return result;
    }

    return 0;
}

static int trunk_binlog_rename_file(const char *src_filename,
        const char *dest_filename, const int log_ignore_errno)
{
	int result;
    if (access(src_filename, F_OK) == 0)
    {
        if (rename(src_filename, dest_filename) != 0)
        {
            result = errno != 0 ? errno : EIO;
            logError("file: "__FILE__", line: %d, "
                    "rename %s to %s fail, "
                    "errno: %d, error info: %s",
                    __LINE__, src_filename,
                    dest_filename, result,
                    STRERROR(result));
            return result;
        }
    }
    else
    {
        result = errno != 0 ? errno : EIO;
        if (result - log_ignore_errno != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "call access %s fail, "
                    "errno: %d, error info: %s",
                    __LINE__, src_filename,
                    result, STRERROR(result));
        }

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

static int trunk_binlog_merge_file(int old_fd, const int stage)
{
	int result;
	int tmp_fd;
	int bytes;
	char binlog_filename[MAX_PATH_SIZE];
	char tmp_filename[MAX_PATH_SIZE];
	char buff[64 * 1024];

    get_trunk_binlog_filename(binlog_filename);
    get_trunk_binlog_tmp_filename_ex(binlog_filename, tmp_filename);
	tmp_fd = open(tmp_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (tmp_fd < 0)
	{
		result = errno != 0 ? errno : EACCES;
		logError("file: "__FILE__", line: %d, "
			"open file \"%s\" fail, "
			"errno: %d, error info: %s",
			__LINE__, tmp_filename, result, STRERROR(result));
		return result;
	}
    
	while ((bytes=fc_safe_read(old_fd, buff, sizeof(buff))) > 0)
	{
		if (fc_safe_write(tmp_fd, buff, bytes) != bytes)
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

		while ((bytes=fc_safe_read(binlog_fd, buff, sizeof(buff))) > 0)
		{
			if (fc_safe_write(tmp_fd, buff, bytes) != bytes)
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

    g_trunk_binlog_compress_stage = stage;
    storage_write_to_sync_ini_file();

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

static int trunk_compress_rollback_data_file()
{
    int result;
	char data_filename[MAX_PATH_SIZE];
    char data_rollback_filename[MAX_PATH_SIZE];
    struct stat fs;

	storage_trunk_get_data_filename(data_filename);
    get_trunk_data_rollback_filename(data_rollback_filename);

    if (stat(data_rollback_filename, &fs) != 0)
    {
        result = errno != 0 ? errno : EPERM;
        if (result == ENOENT)
        {
            return 0;
        }

        logError("file: "__FILE__", line: %d, "
                "stat file %s fail, errno: %d, error info: %s",
                __LINE__, data_rollback_filename,
                result, STRERROR(result));
        return result;
    }

    if (unlink(data_filename) != 0)
    {
        result = errno != 0 ? errno : EPERM;
        if (result != ENOENT)
        {
            logError("file: "__FILE__", line: %d, "
                    "unlink %s fail, errno: %d, error info: %s",
                    __LINE__, data_filename, result, STRERROR(result));
            return result;
        }
    }

    if (fs.st_size == 0)
    {
        unlink(data_rollback_filename);  //delete zero file directly
        return 0;
    }

    if (rename(data_rollback_filename, data_filename) != 0)
    {
        result = errno != 0 ? errno : EPERM;
        if (result == ENOENT)
        {
            return 0;
        }

        logError("file: "__FILE__", line: %d, "
                "rename file %s to %s fail, "
                "errno: %d, error info: %s",
                __LINE__, data_rollback_filename,
                data_filename, result, STRERROR(result));
        return result;
    }

    return 0;
}

static int trunk_compress_rollback_binlog_file(const char *binlog_filename)
{
    int result;
    int rollback_fd;
    char binlog_rollback_filename[MAX_PATH_SIZE];
    struct stat fs;

    get_trunk_binlog_rollback_filename(binlog_rollback_filename);
    if (stat(binlog_rollback_filename, &fs) != 0)
    {
        result = errno != 0 ? errno : ENOENT;
        if (result == ENOENT)
        {
            return 0;
        }
        logError("file: "__FILE__", line: %d, "
                "stat file %s fail, errno: %d, error info: %s",
                __LINE__, binlog_rollback_filename,
                result, STRERROR(result));
        return result;
    }

    if (fs.st_size == 0)
    {
        unlink(binlog_rollback_filename);  //delete zero file directly
        return 0;
    }

    if (access(binlog_filename, F_OK) != 0)
    {
        result = errno != 0 ? errno : EPERM;
        if (result == ENOENT)
        {
            if (rename(binlog_rollback_filename, binlog_filename) != 0)
            {
                result = errno != 0 ? errno : EPERM;
                if (result != ENOENT)
                {
                    logError("file: "__FILE__", line: %d, "
                            "rename file %s to %s fail, "
                            "errno: %d, error info: %s",
                            __LINE__, binlog_rollback_filename,
                            binlog_filename, errno, STRERROR(errno));
                    return result;
                }
            }

            return 0;
        }
        else
        {
            logError("file: "__FILE__", line: %d, "
                    "access file %s fail, errno: %d, error info: %s",
                    __LINE__, binlog_filename, errno, STRERROR(errno));
            return result;
        }
    }

    if ((rollback_fd=trunk_binlog_open_read(binlog_rollback_filename,
                    false)) < 0)
    {
        result = errno != 0 ? errno : EPERM;
        if (result == ENOENT)
        {
            return 0;
        }

        return result;
    }

    result = trunk_binlog_merge_file(rollback_fd,
            STORAGE_TRUNK_COMPRESS_STAGE_ROLLBACK_MERGING);
    close(rollback_fd);

    g_trunk_binlog_compress_stage =
        STORAGE_TRUNK_COMPRESS_STAGE_ROLLBACK_MERGE_DONE;
    storage_write_to_sync_ini_file();

    if (unlink(binlog_rollback_filename) != 0)
    {
        logWarning("file: "__FILE__", line: %d, "
                "unlink %s fail, errno: %d, error info: %s",
                __LINE__, binlog_rollback_filename,
                errno, STRERROR(errno));
    }

    return result;
}

int trunk_binlog_compress_delete_temp_files_after_commit()
{
    int result;
	char data_filename[MAX_PATH_SIZE];

	storage_trunk_get_data_filename(data_filename);
    if (unlink(data_filename) != 0)
    {
        result = errno != 0 ? errno : ENOENT;
        logError("file: "__FILE__", line: %d, "
                "unlink %s fail, errno: %d, error info: %s",
                __LINE__, data_filename,
                result, STRERROR(result));
        if (result != ENOENT)
        {
            return result;
        }
    }

    return trunk_binlog_compress_delete_rollback_files(true);
}

int trunk_binlog_compress_apply()
{
	int result;
	int open_res;
	bool need_open_binlog;
	char binlog_filename[MAX_PATH_SIZE];
	char data_filename[MAX_PATH_SIZE];
	char binlog_rollback_filename[MAX_PATH_SIZE];
	char data_rollback_filename[MAX_PATH_SIZE];

	get_trunk_binlog_filename(binlog_filename);
	if (get_trunk_binlog_rollback_filename(binlog_rollback_filename) == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"filename: %s is too long",
			__LINE__, binlog_filename);
		return ENAMETOOLONG;
	}

	storage_trunk_get_data_filename(data_filename);
    if (get_trunk_data_rollback_filename(data_rollback_filename) == NULL)
    {
		logError("file: "__FILE__", line: %d, "
			"data rollback filename is too long", __LINE__);
		return ENAMETOOLONG;
    }

    if (access(binlog_filename, F_OK) != 0)
    {
        result = errno != 0 ? errno : EPERM;
        logError("file: "__FILE__", line: %d, "
                "access file: %s is fail, "
                "errno: %d, error info: %s",
                __LINE__, binlog_filename,
                result, STRERROR(result));
        return result;
    }

	need_open_binlog = trunk_binlog_fd >= 0;

    pthread_mutex_lock(&trunk_sync_thread_lock);
	if (need_open_binlog)
	{
		trunk_binlog_close_writer(false);
	}

    do
    {
        result = trunk_binlog_rename_file(data_filename,
                data_rollback_filename, ENOENT);
        if (result != 0)
        {
            if (result == ENOENT)
            {
                result = writeToFile(data_rollback_filename, "", 0);
            }

            if (result != 0)
            {
                break;
            }
        }

        if ((result=trunk_binlog_rename_file(binlog_filename,
                        binlog_rollback_filename, 0)) != 0)
        {
            trunk_compress_rollback_data_file();
            break;
        }
    } while (0);

    if (need_open_binlog)
    {
        if ((open_res=trunk_binlog_open_writer(binlog_filename)) != 0)
        {
            trunk_binlog_rename_file(binlog_rollback_filename,
                    binlog_filename, 0);   //rollback
            trunk_compress_rollback_data_file();

            if (result == 0)
            {
                result = open_res;
            }
        }
    }

    pthread_mutex_unlock(&trunk_sync_thread_lock);
    return result;
}

int trunk_binlog_compress_commit()
{
	int result;
	int data_fd;
	bool need_open_binlog;
	char binlog_filename[MAX_PATH_SIZE];
	char data_filename[MAX_PATH_SIZE];

	need_open_binlog = trunk_binlog_fd >= 0;
	get_trunk_binlog_filename(binlog_filename);
	storage_trunk_get_data_filename(data_filename);

	if ((data_fd=trunk_binlog_open_read(data_filename, true)) < 0)
	{
		return errno != 0 ? errno : ENOENT;
	}

    pthread_mutex_lock(&trunk_sync_thread_lock);
	if (need_open_binlog)
	{
		trunk_binlog_close_writer(false);
	}

    do
    {
        result = trunk_binlog_merge_file(data_fd,
                STORAGE_TRUNK_COMPRESS_STAGE_COMMIT_MERGING);
        close(data_fd);
        if (result != 0)
        {
            break;
        }

        g_trunk_binlog_compress_stage =
            STORAGE_TRUNK_COMPRESS_STAGE_COMMIT_MERGE_DONE;
        storage_write_to_sync_ini_file();

        if ((result=trunk_binlog_compress_delete_temp_files_after_commit()) != 0)
        {
            break;
        }

        g_trunk_binlog_compress_stage =
            STORAGE_TRUNK_COMPRESS_STAGE_COMPRESS_SUCCESS;
        storage_write_to_sync_ini_file();

        if (need_open_binlog)
        {
            result = trunk_binlog_open_writer(binlog_filename);
        }
    } while (0);

    pthread_mutex_unlock(&trunk_sync_thread_lock);

    return result;
}

static int do_compress_rollback()
{
	int result;
	bool need_open_binlog;
	char binlog_filename[MAX_PATH_SIZE];

	need_open_binlog = trunk_binlog_fd >= 0;
	get_trunk_binlog_filename(binlog_filename);

    pthread_mutex_lock(&trunk_sync_thread_lock);
	if (need_open_binlog)
	{
		trunk_binlog_close_writer(false);
	}

    do
    {
        if ((result=trunk_compress_rollback_binlog_file(binlog_filename)) != 0)
        {
            break;
        }

        if ((result=trunk_compress_rollback_data_file()) != 0)
        {
            break;
        }

        if (need_open_binlog)
        {
            result = trunk_binlog_open_writer(binlog_filename);
        }
    } while (0);

    pthread_mutex_unlock(&trunk_sync_thread_lock);
    return result;
}

int trunk_binlog_compress_rollback()
{
    int result;

    if ((result=do_compress_rollback()) == 0)
    {
        g_trunk_binlog_compress_stage =
            STORAGE_TRUNK_COMPRESS_STAGE_FINISHED;
        storage_write_to_sync_ini_file();
    }

    return result;
}

static int trunk_binlog_fsync_ex(const bool bNeedLock,
		const char *buff, int *length)
{
	int result;
	int write_ret;
	char full_filename[MAX_PATH_SIZE];

	if (bNeedLock && (result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_lock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

	if (*length == 0) //ignore
	{
		write_ret = 0;  //skip
	}
	else if (fc_safe_write(trunk_binlog_fd, buff, *length) != *length)
	{
		write_ret = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, "
			"write to binlog file \"%s\" fail, fd=%d, "
			"errno: %d, error info: %s",
			__LINE__, get_trunk_binlog_filename(full_filename),
			trunk_binlog_fd, errno, STRERROR(errno));
	}
	else if (fsync(trunk_binlog_fd) != 0)
	{
		write_ret = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, "
			"sync to binlog file \"%s\" fail, "
			"errno: %d, error info: %s",
			__LINE__, get_trunk_binlog_filename(full_filename),
			errno, STRERROR(errno));
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
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_unlock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

	return write_ret;
}

int trunk_binlog_flush(const bool bNeedLock)
{
    return trunk_binlog_fsync_ex(bNeedLock,
            trunk_binlog_write_cache_buff,
            (&trunk_binlog_write_cache_len));
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
					"%d %c %d %d %d %u %d %d\n", \
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

static char *get_binlog_readable_filename(const void *pArg,
		char *full_filename)
{
	static char buff[MAX_PATH_SIZE];

	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	snprintf(full_filename, MAX_PATH_SIZE, 
		"%s/data/"TRUNK_DIR_NAME"/"TRUNK_SYNC_BINLOG_FILENAME_STR,
		SF_G_BASE_PATH_STR);
	return full_filename;
}

int trunk_open_readable_binlog(TrunkBinLogReader *pReader, \
		get_filename_func filename_func, const void *pArg)
{
	char full_filename[MAX_PATH_SIZE];
    struct stat file_stat;

	if (pReader->binlog_fd >= 0)
	{
		close(pReader->binlog_fd);
	}

	filename_func(pArg, full_filename);
	pReader->binlog_fd = open(full_filename, O_RDONLY);
	if (pReader->binlog_fd < 0)
	{
		logError("file: "__FILE__", line: %d, "
			"open binlog file \"%s\" fail, "
			"errno: %d, error info: %s",
			__LINE__, full_filename,
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

    if (fstat(pReader->binlog_fd, &file_stat) != 0)
    {
		logError("file: "__FILE__", line: %d, "
			"stat binlog file \"%s\" fail, "
			"errno: %d, error info: %s",
			__LINE__, full_filename,
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
    }

    if (pReader->binlog_offset > file_stat.st_size)
    {
        logWarning("file: "__FILE__", line: %d, "
                "binlog file \"%s\", binlog_offset: %"PRId64
                " > file size: %"PRId64", set binlog_offset to 0",
                __LINE__, full_filename, pReader->binlog_offset,
                (int64_t)file_stat.st_size);
        pReader->binlog_offset = 0;
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
			"%s/data/"TRUNK_DIR_NAME"/%s%s", SF_G_BASE_PATH_STR, \
			storage_id, TRUNK_SYNC_MARK_FILE_EXT_STR);
	}
	else
	{
		snprintf(full_filename, filename_size, \
			"%s/data/"TRUNK_DIR_NAME"/%s_%d%s", SF_G_BASE_PATH_STR, \
			storage_id, port, TRUNK_SYNC_MARK_FILE_EXT_STR);
	}

	return full_filename;
}

static char *trunk_get_mark_filename_by_ip_and_port(const char *ip_addr, \
		const int port, char *full_filename, const int filename_size)
{
	snprintf(full_filename, filename_size, \
		"%s/data/"TRUNK_DIR_NAME"/%s_%d%s", SF_G_BASE_PATH_STR, \
		ip_addr, port, TRUNK_SYNC_MARK_FILE_EXT_STR);

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
			SF_G_INNER_PORT, full_filename, MAX_PATH_SIZE);
}

static char *trunk_get_mark_filename_by_id(const char *storage_id, 
	char *full_filename, const int filename_size)
{
	return trunk_get_mark_filename_by_id_and_port(storage_id, SF_G_INNER_PORT, \
				full_filename, filename_size);
}

int trunk_reader_init(const FDFSStorageBrief *pStorage,
        TrunkBinLogReader *pReader, const bool reset_binlog_offset)
{
	IniContext iniContext;
	int result;
	int64_t saved_binlog_offset;
	bool bFileExist;

	saved_binlog_offset = pReader->binlog_offset;

	memset(pReader, 0, sizeof(TrunkBinLogReader));
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
	trunk_mark_filename_by_reader(pReader, pReader->mark_filename);

	if (pStorage == NULL)
	{
		bFileExist = false;
		pReader->binlog_offset = saved_binlog_offset;
	}
	else
	{
		bFileExist = fileExists(pReader->mark_filename);
		if (!bFileExist && (g_use_storage_id && pStorage != NULL))
		{
			char old_mark_filename[MAX_PATH_SIZE];
			trunk_get_mark_filename_by_ip_and_port(
				pStorage->ip_addr, SF_G_INNER_PORT,
				old_mark_filename, sizeof(old_mark_filename));
			if (fileExists(old_mark_filename))
			{
				if (rename(old_mark_filename,
                            pReader->mark_filename) != 0)
				{
					logError("file: "__FILE__", line: %d, "
						"rename file %s to %s fail, "
						"errno: %d, error info: %s",
						__LINE__, old_mark_filename,
						pReader->mark_filename, errno,
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
		if ((result=iniLoadFromFile(pReader->mark_filename,
                        &iniContext)) != 0)
		{
			logError("file: "__FILE__", line: %d, "
				"load from mark file \"%s\" fail, "
				"error code: %d", __LINE__,
                pReader->mark_filename, result);
			return result;
		}

		if (iniContext.global.count < 1)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, "
				"in mark file \"%s\", item count: %d < 1",
				__LINE__, pReader->mark_filename,
                iniContext.global.count);
			return ENOENT;
		}

		pReader->binlog_offset = iniGetInt64Value(NULL,
					MARK_ITEM_BINLOG_FILE_OFFSET,
					&iniContext, -1);
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

		iniFreeContext(&iniContext);
	}

	pReader->last_binlog_offset = pReader->binlog_offset;
	if (!bFileExist && pStorage != NULL)
	{
		if ((result=trunk_write_to_mark_file(pReader)) != 0)
		{
			return result;
		}
	}

    if (reset_binlog_offset && pReader->binlog_offset > 0)
    {
        pReader->binlog_offset = 0;
        trunk_write_to_mark_file(pReader);
    }

    if ((result=trunk_open_readable_binlog(pReader,
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

	len = sprintf(buff,
		"%s=%"PRId64"\n",
		MARK_ITEM_BINLOG_FILE_OFFSET, pReader->binlog_offset);

    if ((result=safeWriteToFile(pReader->mark_filename, buff, len)) == 0)
    {
        SF_CHOWN_TO_RUNBY_RETURN_ON_ERROR(pReader->mark_filename);
		pReader->last_binlog_offset = pReader->binlog_offset;
    }

	return result;
}

static int trunk_binlog_preread(TrunkBinLogReader *pReader)
{
	int bytes_read;
	int saved_trunk_binlog_write_version;

	if (pReader->binlog_buff.version == trunk_binlog_write_version &&
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

	bytes_read = fc_safe_read(pReader->binlog_fd, pReader->binlog_buff.buffer \
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

	trunk_get_mark_filename_by_id(storage_id, old_filename,
		sizeof(old_filename));
	if (!fileExists(old_filename))
	{
		return ENOENT;
	}

	snprintf(new_filename, sizeof(new_filename),
		"%s.%04d%02d%02d%02d%02d%02d", old_filename,
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (rename(old_filename, new_filename) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"rename file %s to %s fail, "
			"errno: %d, error info: %s",
			__LINE__, old_filename, new_filename,
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

static void trunk_sync_thread_exit(TrunkSyncThreadInfo *thread_data,
        const int port)
{
	int result;
    char formatted_ip[FORMATTED_IP_SIZE];

	if ((result=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_lock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}
	
    thread_data->running = false;
	FC_ATOMIC_DEC(g_trunk_sync_thread_count);

	if ((result=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_unlock fail, "
			"errno: %d, error info: %s",
			__LINE__, result, STRERROR(result));
	}

    format_ip_address(thread_data->pStorage->ip_addr, formatted_ip);
	logInfo("file: "__FILE__", line: %d, "
		"trunk sync thread to storage server %s:%u exit",
		__LINE__, formatted_ip, port);
}

static int trunk_sync_data(TrunkBinLogReader *pReader, \
		ConnectionInfo *pStorage)
{
	int length;
	char *p;
	int result;
	TrackerHeader header;
    char formatted_ip[FORMATTED_IP_SIZE];
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
	if ((result=tcpsenddata_nb(pStorage->sock, &header,
		sizeof(TrackerHeader), SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pStorage->ip_addr, formatted_ip);
		logError("FILE: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip,
            pStorage->port, result, STRERROR(result));
		return result;
	}

	if ((result=tcpsenddata_nb(pStorage->sock, pReader->binlog_buff.buffer,
		length, SF_G_NETWORK_TIMEOUT)) != 0)
	{
        format_ip_address(pStorage->ip_addr, formatted_ip);
		logError("FILE: "__FILE__", line: %d, "
			"send data to storage server %s:%u fail, errno: %d, "
			"error info: %s", __LINE__, formatted_ip, 
            pStorage->port, result, STRERROR(result));
		return result;
	}

	pBuff = in_buff;
	if ((result=fdfs_recv_response(pStorage, &pBuff, 0, &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response fail, result: %d",
                __LINE__, result);
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

static void *trunk_sync_thread_entrance(void* arg)
{
    TrunkSyncThreadInfo *thread_data;
	const FDFSStorageBrief *pStorage;
	TrunkBinLogReader reader;
	ConnectionInfo storage_server;
	char local_ip_addr[IP_ADDRESS_SIZE];
    char formatted_ip[FORMATTED_IP_SIZE];
	int read_result;
	int sync_result;
	int result;
	time_t current_time;
	time_t last_keep_alive_time;

    thread_data = (TrunkSyncThreadInfo *)arg;
#ifdef OS_LINUX
    {
        char thread_name[32];
        snprintf(thread_name, sizeof(thread_name),
                "trunk-sync[%d]", thread_data->thread_index);
        prctl(PR_SET_NAME, thread_name);
    }
#endif

	memset(local_ip_addr, 0, sizeof(local_ip_addr));
	memset(&reader, 0, sizeof(reader));
	reader.binlog_fd = -1;

	current_time =  g_current_time;
	last_keep_alive_time = 0;

	pStorage = thread_data->pStorage;
	strcpy(storage_server.ip_addr, pStorage->ip_addr);
	storage_server.port = SF_G_INNER_PORT;
	storage_server.sock = -1;

    format_ip_address(storage_server.ip_addr, formatted_ip);
	logInfo("file: "__FILE__", line: %d, "
		"trunk sync thread to storage server %s:%u started",
		__LINE__, formatted_ip, storage_server.port);

	while (SF_G_CONTINUE_FLAG && g_if_trunker_self && \
		pStorage->status != FDFS_STORAGE_STATUS_DELETED && \
		pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED && \
		pStorage->status != FDFS_STORAGE_STATUS_NONE)
	{
        storage_sync_connect_storage_server_ex(pStorage,
                &storage_server, &g_if_trunker_self);

		if ((!SF_G_CONTINUE_FLAG) || (!g_if_trunker_self) || \
			pStorage->status == FDFS_STORAGE_STATUS_DELETED || \
			pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED || \
			pStorage->status == FDFS_STORAGE_STATUS_NONE)
		{
			logError("file: "__FILE__", line: %d, break loop." \
				"SF_G_CONTINUE_FLAG: %d, g_if_trunker_self: %d, " \
				"dest storage status: %d", __LINE__, \
				SF_G_CONTINUE_FLAG, g_if_trunker_self, \
				pStorage->status);
			break;
		}

		if ((result=trunk_reader_init(pStorage, &reader,
                        thread_data->reset_binlog_offset)) != 0)
		{
			logCrit("file: "__FILE__", line: %d, "
				"trunk_reader_init fail, errno=%d, "
				"program exit!", __LINE__, result);
			SF_G_CONTINUE_FLAG = false;
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

		if ((strcmp(pStorage->id, g_my_server_id_str) == 0) ||
                is_local_host_ip(pStorage->ip_addr))
		{  //can't self sync to self
			logError("file: "__FILE__", line: %d, " \
				"ip_addr %s belong to the local host," \
				" trunk sync thread exit.", \
				__LINE__, pStorage->ip_addr);
			fdfs_quit(&storage_server);
			close(storage_server.sock);
			break;
		}

        if (thread_data->reset_binlog_offset)
        {
            thread_data->reset_binlog_offset = false;
            if (reader.binlog_offset > 0)
            {
                reader.binlog_offset = 0;
                trunk_write_to_mark_file(&reader);
            }
        }

		if (reader.binlog_offset == 0)
		{
			if ((result=fdfs_deal_no_body_cmd(&storage_server, \
				STORAGE_PROTO_CMD_TRUNK_TRUNCATE_BINLOG_FILE)) != 0)
			{
                logError("file: "__FILE__", line: %d, "
                        "fdfs_deal_no_body_cmd fail, result: %d",
                        __LINE__, result);

				close(storage_server.sock);
				trunk_reader_destroy(&reader);
				sleep(5);
				continue;
			}
		}

		sync_result = 0;
		while (SF_G_CONTINUE_FLAG && !thread_data->reset_binlog_offset &&
			pStorage->status != FDFS_STORAGE_STATUS_DELETED &&
			pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED &&
			pStorage->status != FDFS_STORAGE_STATUS_NONE)
		{
			read_result = trunk_binlog_preread(&reader);
			if (read_result == ENOENT)
			{
				if (reader.last_binlog_offset !=
					reader.binlog_offset)
				{
					if (trunk_write_to_mark_file(&reader)!=0)
					{
					logCrit("file: "__FILE__", line: %d, "
						"trunk_write_to_mark_file fail, "
						"program exit!", __LINE__);
					SF_G_CONTINUE_FLAG = false;
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
				SF_G_CONTINUE_FLAG = false;
				break;
			}
		}

		close(storage_server.sock);
		storage_server.sock = -1;
		trunk_reader_destroy(&reader);

		if (!SF_G_CONTINUE_FLAG)
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

	trunk_sync_thread_exit(thread_data, storage_server.port);

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

TrunkSyncThreadInfo *trunk_sync_alloc_thread_data()
{
    TrunkSyncThreadInfo **thread_info;
    TrunkSyncThreadInfo **info_end;
    TrunkSyncThreadInfo **old_thread_data;
    TrunkSyncThreadInfo **new_thread_data;
    TrunkSyncThreadInfo **new_data_start;
    int alloc_count;
    int bytes;

    if (FC_ATOMIC_GET(g_trunk_sync_thread_count) + 1 <
            sync_thread_info_array.alloc_count)
    {
        info_end = sync_thread_info_array.thread_data +
            sync_thread_info_array.alloc_count;
        for (thread_info=sync_thread_info_array.thread_data;
                thread_info<info_end; thread_info++)
        {
            if (!(*thread_info)->running)
            {
                return *thread_info;
            }
        }
    }

    if (sync_thread_info_array.alloc_count == 0)
    {
        alloc_count = 1;
    }
    else
    {
        alloc_count = sync_thread_info_array.alloc_count * 2;
    }

    bytes = sizeof(TrunkSyncThreadInfo *) * alloc_count;
    new_thread_data = (TrunkSyncThreadInfo **)malloc(bytes);
    if (new_thread_data == NULL)
    {
        logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail, "
                "errno: %d, error info: %s",
                __LINE__, bytes, errno, STRERROR(errno));
        return NULL;
    }

    logInfo("file: "__FILE__", line: %d, "
            "alloc %d thread data entries",
            __LINE__, alloc_count);

    if (sync_thread_info_array.alloc_count > 0)
    {
        memcpy(new_thread_data, sync_thread_info_array.thread_data,
                sizeof(TrunkSyncThreadInfo *) *
                sync_thread_info_array.alloc_count);
    }

    new_data_start = new_thread_data + sync_thread_info_array.alloc_count;
    info_end = new_thread_data + alloc_count;
    for (thread_info=new_data_start; thread_info<info_end; thread_info++)
    {
        *thread_info = (TrunkSyncThreadInfo *)malloc(
                sizeof(TrunkSyncThreadInfo));
        if (*thread_info == NULL)
        {
            logError("file: "__FILE__", line: %d, "
                    "malloc %d bytes fail, "
                    "errno: %d, error info: %s",
                    __LINE__, (int)sizeof(TrunkSyncThreadInfo),
                    errno, STRERROR(errno));
            return NULL;
        }

        memset(*thread_info, 0, sizeof(TrunkSyncThreadInfo));
        (*thread_info)->thread_index = thread_info - new_thread_data;
    }

    old_thread_data = sync_thread_info_array.thread_data;
    sync_thread_info_array.thread_data = new_thread_data;
    sync_thread_info_array.alloc_count = alloc_count;
    if (old_thread_data != NULL)
    {
        free(old_thread_data);
    }

    return *new_data_start;
}

int trunk_sync_thread_start(const FDFSStorageBrief *pStorage)
{
	int result;
	int lock_res;
	pthread_attr_t pattr;
    TrunkSyncThreadInfo *thread_data;

	if (pStorage->status == FDFS_STORAGE_STATUS_DELETED ||
	    pStorage->status == FDFS_STORAGE_STATUS_IP_CHANGED ||
	    pStorage->status == FDFS_STORAGE_STATUS_NONE)
	{
		return 0;
	}

	if ((strcmp(pStorage->id, g_my_server_id_str) == 0) ||
            is_local_host_ip(pStorage->ip_addr)) //can't self sync to self
	{
		return 0;
	}

	if ((result=init_pthread_attr(&pattr, SF_G_THREAD_STACK_SIZE)) != 0)
	{
		return result;
	}

	if ((lock_res=pthread_mutex_lock(&trunk_sync_thread_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"call pthread_mutex_lock fail, "
			"errno: %d, error info: %s",
			__LINE__, lock_res, STRERROR(lock_res));
	}

    do
    {
        thread_data = trunk_sync_alloc_thread_data();
        if (thread_data == NULL)
        {
            result = ENOMEM;
            break;
        }

        thread_data->running = true;
        thread_data->pStorage = pStorage;
        if ((result=pthread_create(&thread_data->tid, &pattr,
                        trunk_sync_thread_entrance,
                        (void *)thread_data)) != 0)
        {
            thread_data->running = false;
            logError("file: "__FILE__", line: %d, "
                    "create thread failed, errno: %d, "
                    "error info: %s",
                    __LINE__, result, STRERROR(result));
            break;
        }

        FC_ATOMIC_INC(g_trunk_sync_thread_count);
    } while (0);

	if ((lock_res=pthread_mutex_unlock(&trunk_sync_thread_lock)) != 0)
    {
        logError("file: "__FILE__", line: %d, "
                "call pthread_mutex_unlock fail, "
                "errno: %d, error info: %s",
                __LINE__, lock_res, STRERROR(lock_res));
    }

	pthread_attr_destroy(&pattr);
	return result;
}

void trunk_waiting_sync_thread_exit()
{
    int saved_trunk_sync_thread_count;
    int count;

    saved_trunk_sync_thread_count = FC_ATOMIC_GET(g_trunk_sync_thread_count);
    if (saved_trunk_sync_thread_count > 0)
    {
        logInfo("file: "__FILE__", line: %d, "
                "waiting %d trunk sync threads exit ...",
                __LINE__, saved_trunk_sync_thread_count);
    }

    count = 0;
    while (FC_ATOMIC_GET(g_trunk_sync_thread_count) > 0 && count < 60)
    {
        usleep(50000);
        count++;
    }

    if (FC_ATOMIC_GET(g_trunk_sync_thread_count) > 0)
    {
        logWarning("file: "__FILE__", line: %d, "
                "kill %d trunk sync threads.",
                __LINE__, FC_ATOMIC_GET(g_trunk_sync_thread_count));
        kill_trunk_sync_threads();
    }

    if (saved_trunk_sync_thread_count > 0)
    {
        logInfo("file: "__FILE__", line: %d, "
                "%d trunk sync threads exited",
                __LINE__, saved_trunk_sync_thread_count);
    }
}

int trunk_unlink_all_mark_files()
{
	char file_path[MAX_PATH_SIZE];
	char full_filename[MAX_PATH_SIZE];
    DIR *dir;
    struct dirent *ent;
	int result;
    int name_len;
	time_t t;
	struct tm tm;

	t = g_current_time;
	localtime_r(&t, &tm);

	snprintf(file_path, sizeof(file_path),
		"%s/data/%s", SF_G_BASE_PATH_STR, TRUNK_DIR_NAME);

    if ((dir=opendir(file_path)) == NULL)
    {
        result = errno != 0 ? errno : EPERM;
		logError("file: "__FILE__", line: %d, "
                "call opendir %s fail, errno: %d, error info: %s",
                __LINE__, file_path, result, STRERROR(result));
        return result;
    }

    result = 0;
    while ((ent=readdir(dir)) != NULL)
    {
        name_len = strlen(ent->d_name);
        if (name_len <= TRUNK_SYNC_MARK_FILE_EXT_LEN)
        {
            continue;
        }
        if (memcmp(ent->d_name + (name_len -
                        TRUNK_SYNC_MARK_FILE_EXT_LEN),
                    TRUNK_SYNC_MARK_FILE_EXT_STR,
                    TRUNK_SYNC_MARK_FILE_EXT_LEN) != 0)
        {
            continue;
        }

        snprintf(full_filename, sizeof(full_filename), "%s/%s",
                file_path, ent->d_name);
        if (unlink(full_filename) != 0)
        {
            result = errno != 0 ? errno : EPERM;
            if (result == ENOENT)
            {
                result = 0;
            }
            else
            {
                logError("file: "__FILE__", line: %d, "
                        "unlink %s fail, errno: %d, error info: %s",
                        __LINE__, full_filename,
                        result, STRERROR(result));
                break;
            }
        }
    }

    closedir(dir);
	return result;
}

int trunk_binlog_get_write_version()
{
    return trunk_binlog_write_version;
}
