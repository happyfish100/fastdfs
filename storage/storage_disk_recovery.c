/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include "fdfs_define.h"
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/avl_tree.h"
#include "fastcommon/shared_func.h"
#include "fdfs_global.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "trunk_mgr/trunk_shared.h"
#include "storage_func.h"
#include "storage_sync.h"
#include "tracker_client.h"
#include "storage_disk_recovery.h"
#include "storage_client.h"

typedef struct {
	char line[128];
	FDFSTrunkPathInfo path;  //trunk file path
	int id;                  //trunk file id
} FDFSTrunkFileIdInfo;

typedef struct recovery_thread_data {
	int thread_index;    //-1 for global
    int result;
    volatile int alive;
    bool done;
    const char *base_path;
    pthread_t tid;
} RecoveryThreadData;

#define RECOVERY_BINLOG_FILENAME	".binlog.recovery"
#define RECOVERY_FLAG_FILENAME		".recovery.flag"
#define RECOVERY_MARK_FILENAME		".recovery.mark"

#define FLAG_ITEM_RECOVERY_THREADS  "recovery_threads"
#define FLAG_ITEM_SAVED_STORAGE_STATUS	"saved_storage_status"
#define FLAG_ITEM_FETCH_BINLOG_DONE    	"fetch_binlog_done"

#define MARK_ITEM_BINLOG_OFFSET    	"binlog_offset"

static int last_recovery_threads = -1;  //for rebalance binlog data
static volatile int current_recovery_thread_count = 0;
static int saved_storage_status = FDFS_STORAGE_STATUS_NONE;

static char *recovery_get_binlog_filename(const void *pArg,
        char *full_filename);

static int disk_recovery_write_to_binlog(FILE *fp,
        const char *binlog_filename, StorageBinLogRecord *pRecord);

static char *recovery_get_full_filename_ex(const char *pBasePath,
        const int thread_index, const char *filename, char *full_filename)
{
	static char buff[MAX_PATH_SIZE];
    int len;

	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	len = snprintf(full_filename, MAX_PATH_SIZE,
		"%s/data/%s", pBasePath, filename);
    if (thread_index >= 0)
    {
        snprintf(full_filename + len, MAX_PATH_SIZE - len,
                ".%d", thread_index);
    }
	return full_filename;
}

static inline char *recovery_get_full_filename(const RecoveryThreadData
        *pThreadData, const char *filename, char *full_filename)
{
    return recovery_get_full_filename_ex(pThreadData->base_path,
            pThreadData->thread_index, filename, full_filename);
}

static inline char *recovery_get_global_full_filename(const char *pBasePath,
        const char *filename, char *full_filename)
{
    return recovery_get_full_filename_ex(pBasePath, -1,
            RECOVERY_FLAG_FILENAME, full_filename);
}

static inline char *recovery_get_global_binlog_filename(const char *pBasePath,
        char *full_filename)
{
	return recovery_get_global_full_filename(pBasePath,
            RECOVERY_BINLOG_FILENAME, full_filename);
}

static int storage_do_fetch_binlog(ConnectionInfo *pSrcStorage, \
		const int store_path_index)
{
	char out_buff[sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + 1];
	char full_binlog_filename[MAX_PATH_SIZE];
	TrackerHeader *pHeader;
	char *pBasePath;
	int64_t in_bytes;
	int64_t file_bytes;
	int result;
    int network_timeout;

	pBasePath = g_fdfs_store_paths.paths[store_path_index].path;
    recovery_get_full_filename_ex(pBasePath, 0,
            RECOVERY_BINLOG_FILENAME, full_binlog_filename);

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;

	long2buff(FDFS_GROUP_NAME_MAX_LEN + 1, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_FETCH_ONE_PATH_BINLOG;
	strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
	*(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN) =
			store_path_index;

	if((result=tcpsenddata_nb(pSrcStorage->sock, out_buff,
		sizeof(out_buff), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"storage server %s:%d, send data fail, "
			"errno: %d, error info: %s.",
			__LINE__, pSrcStorage->ip_addr, pSrcStorage->port,
			result, STRERROR(result));
		return result;
	}

    if (g_fdfs_network_timeout >= 600)
    {
        network_timeout = g_fdfs_network_timeout;
    }
    else
    {
        network_timeout = 600;
    }
	if ((result=fdfs_recv_header_ex(pSrcStorage, network_timeout,
                    &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
                "fdfs_recv_header fail, result: %d",
                __LINE__, result);
		return result;
	}

	if ((result=tcprecvfile(pSrcStorage->sock, full_binlog_filename,
				in_bytes, 0, network_timeout, &file_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"storage server %s:%d, tcprecvfile fail, "
			"errno: %d, error info: %s.",
			__LINE__, pSrcStorage->ip_addr, pSrcStorage->port,
			result, STRERROR(result));
		return result;
	}

	logInfo("file: "__FILE__", line: %d, "
		"recovery binlog from %s:%d, file size: %"PRId64, __LINE__,
        pSrcStorage->ip_addr, pSrcStorage->port, file_bytes);

	return 0;
}

static int recovery_get_src_storage_server(ConnectionInfo *pSrcStorage)
{
	int result;
	int storage_count;
    int i;
    static unsigned int current_index = 0;
	TrackerServerInfo trackerServer;
	ConnectionInfo *pTrackerConn;
	FDFSGroupStat groupStat;
	FDFSStorageInfo storageStats[FDFS_MAX_SERVERS_EACH_GROUP];
	FDFSStorageInfo *pStorageStat;
    bool found;

	memset(pSrcStorage, 0, sizeof(ConnectionInfo));
	pSrcStorage->sock = -1;

	logDebug("file: "__FILE__", line: %d, " \
		"disk recovery: get source storage server", \
		__LINE__);
	while (g_continue_flag)
	{
		result = tracker_get_storage_max_status(&g_tracker_group,
                g_group_name, g_tracker_client_ip.ips[0].address,
                g_my_server_id_str, &saved_storage_status);
		if (result == ENOENT)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"current storage: %s does not exist " \
				"in tracker server", __LINE__, \
				g_tracker_client_ip.ips[0].address);
			return ENOENT;
		}

		if (result == 0)
		{
			if (saved_storage_status == FDFS_STORAGE_STATUS_INIT)
			{
				logInfo("file: "__FILE__", line: %d, " \
					"current storage: %s 's status is %d" \
					", does not need recovery", __LINE__, \
					g_tracker_client_ip.ips[0].address, \
					saved_storage_status);
				return ENOENT;
			}

			if (saved_storage_status == FDFS_STORAGE_STATUS_IP_CHANGED || \
			    saved_storage_status == FDFS_STORAGE_STATUS_DELETED)
			{
				logWarning("file: "__FILE__", line: %d, " \
					"current storage: %s 's status is %d" \
					", does not need recovery", __LINE__, \
					g_tracker_client_ip.ips[0].address,
                    saved_storage_status);
				return ENOENT;
			}

			break;
		}

		sleep(1);
	}

    found = false;
	while (g_continue_flag)
	{
		if ((pTrackerConn=tracker_get_connection_r(&trackerServer, \
				&result)) == NULL)
		{
			sleep(5);
			continue;
		}

		result = tracker_list_one_group(pTrackerConn, \
				g_group_name, &groupStat);
		if (result != 0)
		{
			tracker_close_connection_ex(pTrackerConn, true);
			sleep(1);
			continue;
		}

		if (groupStat.count <= 0)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"storage server count: %d in the group <= 0!",\
				__LINE__, groupStat.count);

			tracker_close_connection(pTrackerConn);
			sleep(1);
			continue;
		}

		if (groupStat.count == 1)
		{
			logInfo("file: "__FILE__", line: %d, " \
				"storage server count in the group = 1, " \
				"does not need recovery", __LINE__);

			tracker_close_connection(pTrackerConn);
			return ENOENT;
		}

		if (g_fdfs_store_paths.count > groupStat.store_path_count)
		{
			logInfo("file: "__FILE__", line: %d, " \
				"storage store path count: %d > " \
				"which of the group: %d, " \
				"does not need recovery", __LINE__, \
				g_fdfs_store_paths.count, groupStat.store_path_count);

			tracker_close_connection(pTrackerConn);
			return ENOENT;
		}

		if (groupStat.active_count <= 0)
		{
			tracker_close_connection(pTrackerConn);
			sleep(5);
			continue;
		}

		result = tracker_list_servers(pTrackerConn,
                		g_group_name, NULL, storageStats,
				FDFS_MAX_SERVERS_EACH_GROUP, &storage_count);
		tracker_close_connection_ex(pTrackerConn, result != 0);
		if (result != 0)
		{
			sleep(5);
			continue;
		}

		if (storage_count <= 1)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"storage server count: %d in the group <= 1!",\
				__LINE__, storage_count);

			sleep(5);
			continue;
		}

		for (i=0; i<storage_count; i++)
		{
            pStorageStat = storageStats + (current_index++ % storage_count);
			if (strcmp(pStorageStat->id, g_my_server_id_str) == 0)
			{
				continue;
			}

			if (pStorageStat->status == FDFS_STORAGE_STATUS_ACTIVE)
			{
                found = true;
				strcpy(pSrcStorage->ip_addr, pStorageStat->ip_addr);
				pSrcStorage->port = pStorageStat->storage_port;
				break;
			}
		}

		if (found)  //found src storage server
		{
			break;
		}

		sleep(5);
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	logDebug("file: "__FILE__", line: %d, "
		"disk recovery: get source storage server %s:%d",
		__LINE__, pSrcStorage->ip_addr, pSrcStorage->port);
	return 0;
}

static char *recovery_get_binlog_filename(const void *pArg,
        char *full_filename)
{
	return recovery_get_full_filename((const RecoveryThreadData *)pArg,
			RECOVERY_BINLOG_FILENAME, full_filename);
}

static char *recovery_get_flag_filename(const char *pBasePath,
        char *full_filename)
{
    return recovery_get_global_full_filename(pBasePath,
            RECOVERY_FLAG_FILENAME, full_filename);
}

static char *recovery_get_mark_filename(const RecoveryThreadData *pThreadData,
        char *full_filename)
{
	return recovery_get_full_filename(pThreadData,
			RECOVERY_MARK_FILENAME, full_filename);
}

static int storage_disk_recovery_delete_thread_files(const char *pBasePath,
        const int index_start, const int index_end)
{
    int i;
	char mark_filename[MAX_PATH_SIZE];
	char binlog_filename[MAX_PATH_SIZE];

    for (i=index_start; i<index_end; i++)
    {
        recovery_get_full_filename_ex(pBasePath, i,
                RECOVERY_BINLOG_FILENAME, binlog_filename);
        if (fileExists(binlog_filename))
        {
            if (unlink(binlog_filename) != 0)
            {
                logError("file: "__FILE__", line: %d, "
                        "delete recovery binlog file: %s fail, "
                        "errno: %d, error info: %s",
                        __LINE__, binlog_filename,
                        errno, STRERROR(errno));
                return errno != 0 ? errno : EPERM;
            }
        }

        recovery_get_full_filename_ex(pBasePath, i,
                RECOVERY_MARK_FILENAME, mark_filename);
        if (fileExists(mark_filename))
        {
            if (unlink(mark_filename) != 0)
            {
                logError("file: "__FILE__", line: %d, "
                        "delete recovery mark file: %s fail, "
                        "errno: %d, error info: %s",
                        __LINE__, mark_filename,
                        errno, STRERROR(errno));
                return errno != 0 ? errno : EPERM;
            }
        }
    }

    return 0;
}

static int storage_disk_recovery_finish(const char *pBasePath)
{
	char flag_filename[MAX_PATH_SIZE];

    recovery_get_flag_filename(pBasePath, flag_filename);
	if (fileExists(flag_filename))
    {
		if (unlink(flag_filename) != 0)
		{
			logError("file: "__FILE__", line: %d, "
				"delete recovery flag file: %s fail, "
				"errno: %d, error info: %s",
				 __LINE__, flag_filename,
				errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}
    }

	return storage_disk_recovery_delete_thread_files(
            pBasePath, 0, g_disk_recovery_threads);
}

static int do_write_to_flag_file(const char *flag_filename,
        const bool fetch_binlog_done, const int recovery_threads)
{
	char buff[256];
	int len;

	len = sprintf(buff,
		"%s=%d\n"
		"%s=%d\n"
		"%s=%d\n",
		FLAG_ITEM_SAVED_STORAGE_STATUS, saved_storage_status,
		FLAG_ITEM_FETCH_BINLOG_DONE, fetch_binlog_done ? 1 : 0,
        FLAG_ITEM_RECOVERY_THREADS, recovery_threads);

	return safeWriteToFile(flag_filename, buff, len);
}

static int do_write_to_mark_file(const char *mark_filename,
        const int64_t binlog_offset)
{
	char buff[128];
	int len;

	len = sprintf(buff,
		"%s=%"PRId64"\n",
		MARK_ITEM_BINLOG_OFFSET, binlog_offset);

	return safeWriteToFile(mark_filename, buff, len);
}

static inline int recovery_write_to_mark_file(StorageBinLogReader *pReader)
{
    return do_write_to_mark_file(pReader->mark_filename,
            pReader->binlog_offset);
}

static int recovery_init_global_binlog_file(const char *pBasePath)
{
	char full_binlog_filename[MAX_PATH_SIZE];
	char buff[1];

	*buff = '\0';
    recovery_get_full_filename_ex(pBasePath, 0,
            RECOVERY_BINLOG_FILENAME, full_binlog_filename);
	return writeToFile(full_binlog_filename, buff, 0);
}

static int recovery_init_flag_file_ex(const char *pBasePath,
		const bool fetch_binlog_done, const int recovery_threads)
{
	char full_filename[MAX_PATH_SIZE];

    recovery_get_flag_filename(pBasePath, full_filename);
    return do_write_to_flag_file(full_filename,
            fetch_binlog_done, recovery_threads);
}

static inline int recovery_init_flag_file(const char *pBasePath,
		const bool fetch_binlog_done, const int recovery_threads)
{
    return recovery_init_flag_file_ex(pBasePath,
		fetch_binlog_done, recovery_threads);
}

static int recovery_load_params_from_flag_file(const char *full_flag_filename)
{
	IniContext iniContext;
	int result;

	memset(&iniContext, 0, sizeof(IniContext));
	if ((result=iniLoadFromFile(full_flag_filename,
                    &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"load from flag file \"%s\" fail, "
			"error code: %d", __LINE__,
			full_flag_filename, result);
		return result;
	}

	if (!iniGetBoolValue(NULL, FLAG_ITEM_FETCH_BINLOG_DONE,
			&iniContext, false))
	{
		iniFreeContext(&iniContext);

		logInfo("file: "__FILE__", line: %d, "
			"flag file \"%s\", %s=0, "
			"need to fetch binlog again", __LINE__,
			full_flag_filename, FLAG_ITEM_FETCH_BINLOG_DONE);
		return EAGAIN;
	}

	saved_storage_status = iniGetIntValue(NULL,
			FLAG_ITEM_SAVED_STORAGE_STATUS, &iniContext, -1);
	if (saved_storage_status < 0)
	{
		iniFreeContext(&iniContext);

		logError("file: "__FILE__", line: %d, "
			"in flag file \"%s\", %s: %d < 0", __LINE__,
			full_flag_filename, FLAG_ITEM_SAVED_STORAGE_STATUS,
			saved_storage_status);
		return EINVAL;
	}

    last_recovery_threads = iniGetIntValue(NULL,
			FLAG_ITEM_RECOVERY_THREADS, &iniContext, -1);

	iniFreeContext(&iniContext);
	return 0;
}

static int recovery_reader_init(const RecoveryThreadData *pThreadData,
                        StorageBinLogReader *pReader)
{
	IniContext iniContext;
	int result;

	memset(pReader, 0, sizeof(StorageBinLogReader));
	pReader->binlog_fd = -1;
	pReader->binlog_index = g_binlog_index + 1;

	pReader->binlog_buff.buffer = (char *)malloc(
			STORAGE_BINLOG_BUFFER_SIZE);
	if (pReader->binlog_buff.buffer == NULL)
	{
		logError("file: "__FILE__", line: %d, "
			"malloc %d bytes fail, "
			"errno: %d, error info: %s",
			__LINE__, STORAGE_BINLOG_BUFFER_SIZE,
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	pReader->binlog_buff.current = pReader->binlog_buff.buffer;

	recovery_get_mark_filename(pThreadData, pReader->mark_filename);
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

	pReader->binlog_offset = iniGetInt64Value(NULL,
			MARK_ITEM_BINLOG_OFFSET, &iniContext, -1);
	if (pReader->binlog_offset < 0)
	{
		iniFreeContext(&iniContext);

		logError("file: "__FILE__", line: %d, " \
			"in mark file \"%s\", %s: "\
			"%"PRId64" < 0", __LINE__, \
			pReader->mark_filename, MARK_ITEM_BINLOG_OFFSET, \
			pReader->binlog_offset);
		return EINVAL;
	}

	iniFreeContext(&iniContext);

	if ((result=storage_open_readable_binlog(pReader,
			recovery_get_binlog_filename, pThreadData)) != 0)
	{
		return result;
	}

	return 0;
}

static int recovery_reader_check_init(const RecoveryThreadData *pThreadData,
        StorageBinLogReader *pReader)
{
    if (pReader->binlog_fd >= 0 && pReader->binlog_buff.buffer != NULL)
    {
        return 0;
    }

    return recovery_reader_init(pThreadData, pReader);
}

static int recovery_download_file_to_local(StorageBinLogRecord *pRecord,
		ConnectionInfo *pTrackerServer, ConnectionInfo *pStorageConn)
{
	int result;
    bool bTrunkFile;
	char local_filename[MAX_PATH_SIZE];
	char tmp_filename[MAX_PATH_SIZE + 32];
    char *download_filename;
	int64_t file_size;

    if (fdfs_is_trunk_file(pRecord->filename, pRecord->filename_len))
    {
        FDFSTrunkFullInfo trunk_info;
        char *pTrunkPathEnd;
        char *pLocalFilename;

        bTrunkFile = true;
        if (fdfs_decode_trunk_info(pRecord->store_path_index,
                    pRecord->true_filename, pRecord->true_filename_len,
                    &trunk_info) != 0)
        {
            return -EINVAL;
        }

        trunk_get_full_filename(&trunk_info,
                local_filename, sizeof(local_filename));

        pTrunkPathEnd = strrchr(pRecord->filename, '/');
        pLocalFilename = strrchr(local_filename, '/');
        if (pTrunkPathEnd == NULL || pLocalFilename == NULL)
        {
            return -EINVAL;
        }
        sprintf(pTrunkPathEnd + 1, "%s", pLocalFilename + 1);
    }
    else
    {
        bTrunkFile = false;
        sprintf(local_filename, "%s/data/%s",
                g_fdfs_store_paths.paths[pRecord->store_path_index].path,
                pRecord->true_filename);
    }

    if (access(local_filename, F_OK) == 0)
    {
        sprintf(tmp_filename, "%s.recovery.tmp", local_filename);
        download_filename = tmp_filename;
    }
    else
    {
        download_filename = local_filename;
    }

    result = storage_download_file_to_file(pTrackerServer,
            pStorageConn, g_group_name, pRecord->filename,
            download_filename, &file_size);
    if (result == 0)
    {
        if (download_filename != local_filename)
        {
            if (rename(download_filename, local_filename) != 0)
            {
                logError("file: "__FILE__", line: %d, "
                        "rename file %s to %s fail, "
                        "errno: %d, error info: %s", __LINE__,
                        download_filename, local_filename,
                        errno, STRERROR(errno));
                return errno != 0 ? errno : EPERM;
            }
        }
        if (!bTrunkFile)
        {
            set_file_utimes(local_filename, pRecord->timestamp);
        }
    }

    return result;
}

static int storage_do_recovery(RecoveryThreadData *pThreadData,
        StorageBinLogReader *pReader, ConnectionInfo *pSrcStorage)
{
	TrackerServerInfo trackerServer;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageConn;
	StorageBinLogRecord record;
	int record_length;
	int result;
	int log_level;
	int count;
	int store_path_index;
	int64_t total_count;
	int64_t success_count;
	int64_t noent_count;
	bool bContinueFlag;
	char local_filename[MAX_PATH_SIZE];
	char src_filename[MAX_PATH_SIZE];

	pTrackerServer = tracker_get_connection_r(&trackerServer, &result);
    if (pTrackerServer == NULL)
    {
        logError("file: "__FILE__", line: %d, "
                "get tracker connection fail, result: %d",
                __LINE__, result);
        return result;
    }

	count = 0;
	total_count = 0;
	success_count = 0;
    noent_count = 0;
	result = 0;

	logInfo("file: "__FILE__", line: %d, "
		"disk recovery thread #%d, src storage server %s:%d, "
        "recovering files of data path: %s ...", __LINE__,
        pThreadData->thread_index, pSrcStorage->ip_addr,
        pSrcStorage->port, pThreadData->base_path);

	bContinueFlag = true;
	while (bContinueFlag)
    {
        if ((result=recovery_reader_check_init(pThreadData, pReader)) != 0)
        {
            break;
        }
        if ((pStorageConn=tracker_make_connection(pSrcStorage,
                        &result)) == NULL)
        {
            sleep(5);
            continue;
        }

        while (g_continue_flag)
        {
            result = storage_binlog_read(pReader, &record, &record_length);
            if (result != 0)
            {
                if (result == ENOENT)
                {
                    pThreadData->done = true;
                    result = 0;
                }
                bContinueFlag = false;
                break;
            }

            total_count++;
            if (record.op_type == STORAGE_OP_TYPE_SOURCE_CREATE_FILE
                    || record.op_type == STORAGE_OP_TYPE_REPLICA_CREATE_FILE)
            {
                result = recovery_download_file_to_local(&record,
                        pTrackerServer, pStorageConn);
                if (result == 0)
                {
                    success_count++;
                }
                else if (result == -EINVAL)
                {
                    result = 0;
                }
                else if (result == ENOENT)
                {
                    result = 0;
                    noent_count++;
                }
                else
                {
                    break;
                }
            }
            else if (record.op_type == STORAGE_OP_TYPE_SOURCE_CREATE_LINK
                    || record.op_type == STORAGE_OP_TYPE_REPLICA_CREATE_LINK)
            {
                if (record.src_filename_len == 0)
                {
                    logError("file: "__FILE__", line: %d, " \
                            "invalid binlog line, filename: %s, " \
                            "expect src filename", __LINE__, \
                            record.filename);
                    result = EINVAL;
                    bContinueFlag = false;
                    break;
                }

                if ((result=storage_split_filename_ex(record.filename, \
                                &record.filename_len, record.true_filename, \
                                &store_path_index)) != 0)
                {
                    bContinueFlag = false;
                    break;
                }
                sprintf(local_filename, "%s/data/%s", \
                        g_fdfs_store_paths.paths[store_path_index].path, \
                        record.true_filename);

                if ((result=storage_split_filename_ex( \
                                record.src_filename, &record.src_filename_len,\
                                record.true_filename, &store_path_index)) != 0)
                {
                    bContinueFlag = false;
                    break;
                }
                sprintf(src_filename, "%s/data/%s", \
                        g_fdfs_store_paths.paths[store_path_index].path, \
                        record.true_filename);
                if (symlink(src_filename, local_filename) == 0)
                {
                    success_count++;
                }
                else
                {
                    result = errno != 0 ? errno : ENOENT;
                    if (result == ENOENT || result == EEXIST)
                    {
                        log_level = LOG_DEBUG;
                    }
                    else
                    {
                        log_level = LOG_ERR;
                    }

                    log_it_ex(&g_log_context, log_level, \
                            "file: "__FILE__", line: %d, " \
                            "link file %s to %s fail, " \
                            "errno: %d, error info: %s", __LINE__,\
                            src_filename, local_filename, \
                            result, STRERROR(result));

                    if (result != ENOENT && result != EEXIST)
                    {
                        bContinueFlag = false;
                        break;
                    }
                    else
                    {
                        result = 0;
                    }
                }
            }
            else
            {
                logError("file: "__FILE__", line: %d, " \
                        "invalid file op type: %d", \
                        __LINE__, record.op_type);
                result = EINVAL;
                bContinueFlag = false;
                break;
            }

            pReader->binlog_offset += record_length;
            count++;
            if (count == 1000)
            {
                logDebug("file: "__FILE__", line: %d, "
                        "disk recovery thread #%d recover path: %s, "
                        "file count: %"PRId64", success count: %"PRId64
                        ", noent_count: %"PRId64, __LINE__,
                        pThreadData->thread_index,
                        pThreadData->base_path, total_count,
                        success_count, noent_count);
                recovery_write_to_mark_file(pReader);
                count = 0;
            }
        }

        tracker_close_connection_ex(pStorageConn, result != 0);
        recovery_write_to_mark_file(pReader);

        if (!g_continue_flag)
        {
            bContinueFlag = false;
        }
        else if (bContinueFlag)
        {
            storage_reader_destroy(pReader);
        }

        if (count > 0)
        {
            logInfo("file: "__FILE__", line: %d, "
                    "disk recovery thread #%d, recover path: %s, "
                    "file count: %"PRId64", success count: "
                    "%"PRId64", noent_count: %"PRId64,
                    __LINE__, pThreadData->thread_index,
                    pThreadData->base_path, total_count,
                    success_count, noent_count);
            count = 0;
        }

        if (bContinueFlag)
        {
            sleep(5);
        }
    }

    tracker_close_connection_ex(pTrackerServer, true);

	if (pThreadData->done)
	{
		logInfo("file: "__FILE__", line: %d, "
			"disk recovery thread #%d, src storage server %s:%d, "
            "recover files of data path: %s done", __LINE__,
            pThreadData->thread_index, pSrcStorage->ip_addr,
            pSrcStorage->port, pThreadData->base_path);
	}

	return g_continue_flag ? result :EINTR;
}

static void *storage_disk_recovery_restore_entrance(void *arg)
{
	StorageBinLogReader reader;
    RecoveryThreadData *pThreadData;
	ConnectionInfo srcStorage;

    pThreadData = (RecoveryThreadData *)arg;
    pThreadData->tid = pthread_self();
    __sync_add_and_fetch(&pThreadData->alive, 1);
    __sync_add_and_fetch(&current_recovery_thread_count, 1);

    do
    {
        if ((pThreadData->result=recovery_get_src_storage_server(&srcStorage)) != 0)
        {
            if (pThreadData->result == ENOENT)
            {
                logWarning("file: "__FILE__", line: %d, "
                        "no source storage server, "
                        "disk recovery finished!", __LINE__);
                pThreadData->result = 0;
            }
            break;
        }

        if ((pThreadData->result=recovery_reader_init(pThreadData, &reader)) != 0)
        {
            storage_reader_destroy(&reader);
            break;
        }

        pThreadData->result = storage_do_recovery(pThreadData, &reader, &srcStorage);

        recovery_write_to_mark_file(&reader);
        storage_reader_destroy(&reader);

    } while (0);

    __sync_sub_and_fetch(&current_recovery_thread_count, 1);
    __sync_sub_and_fetch(&pThreadData->alive, 1);
    sleep(1);

    return NULL;
}

static int storage_disk_recovery_old_version_migrate(const char *pBasePath)
{
	char old_binlog_filename[MAX_PATH_SIZE];
	char old_mark_filename[MAX_PATH_SIZE];
	char new_binlog_filename[MAX_PATH_SIZE];
	char new_mark_filename[MAX_PATH_SIZE];
	int result;

    recovery_get_global_binlog_filename(pBasePath, old_binlog_filename);
	recovery_get_global_full_filename(pBasePath,
            RECOVERY_MARK_FILENAME, old_mark_filename);

	if (!(fileExists(old_mark_filename) &&
	      fileExists(old_binlog_filename)))
	{
		return ENOENT;
	}

	logInfo("file: "__FILE__", line: %d, "
            "try to migrate data from old version ...", __LINE__);

    result = recovery_load_params_from_flag_file(old_mark_filename);
    if (result != 0)
    {
        if (result == EAGAIN)
        {
            unlink(old_mark_filename);
        }
        return result;
    }

    if ((result=recovery_init_flag_file_ex(pBasePath, true, 1)) != 0)
    {
        return result;
    }

    recovery_get_full_filename_ex(pBasePath, 0,
            RECOVERY_MARK_FILENAME, new_mark_filename);
	if (rename(old_mark_filename, new_mark_filename) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"rename file %s to %s fail, "
			"errno: %d, error info: %s", __LINE__,
			old_mark_filename, new_mark_filename,
			errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

    recovery_get_full_filename_ex(pBasePath, 0,
            RECOVERY_BINLOG_FILENAME, new_binlog_filename);
	if (rename(old_binlog_filename, new_binlog_filename) != 0)
	{
		logError("file: "__FILE__", line: %d, "
			"rename file %s to %s fail, "
			"errno: %d, error info: %s", __LINE__,
			old_binlog_filename, new_binlog_filename,
			errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	logInfo("file: "__FILE__", line: %d, "
            "migrate data from old version successfully.", __LINE__);
    return 0;
}

static int do_dispatch_binlog_for_threads(const char *pBasePath)
{
    typedef struct {
        FILE *fp;
        int64_t count;
        char binlog_filename[MAX_PATH_SIZE];
        char temp_filename[MAX_PATH_SIZE];
    } RecoveryDispatchInfo;

    char mark_filename[MAX_PATH_SIZE];
    string_t log_buff;
    char buff[2 * 1024];
    struct stat file_stat;
    RecoveryThreadData thread_data;
    StorageBinLogReader reader;
	StorageBinLogRecord record;
	int record_length;
    RecoveryDispatchInfo *dispatchs;
    RecoveryDispatchInfo *disp;
    int64_t total_count;
    int bytes;
    int result;
    int i;

    bytes = sizeof(RecoveryDispatchInfo) * g_disk_recovery_threads;
    dispatchs = (RecoveryDispatchInfo *)malloc(bytes);
    if (dispatchs == NULL)
    {
        logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail",
                __LINE__, bytes);
        return ENOMEM;
    }
    memset(dispatchs, 0, bytes);

    result = 0;
    for (i=0; i<g_disk_recovery_threads; i++)
    {
        recovery_get_full_filename_ex(pBasePath, i,
                RECOVERY_BINLOG_FILENAME, dispatchs[i].binlog_filename);
        snprintf(dispatchs[i].temp_filename,
                sizeof(dispatchs[i].temp_filename),
                "%s.tmp", dispatchs[i].binlog_filename);
        dispatchs[i].fp = fopen(dispatchs[i].temp_filename, "w");
        if (dispatchs[i].fp == NULL)
        {
            result = errno != 0 ? errno : EPERM;
            logError("file: "__FILE__", line: %d, "
                    "open file: %s to write fail, "
                    "errno: %d, error info: %s.",
                    __LINE__, dispatchs[i].temp_filename,
                    result, STRERROR(result));
            return result;
        }
    }

    thread_data.base_path = pBasePath;
    for (i=0; i<last_recovery_threads; i++)
    {
        thread_data.thread_index = i;
        if ((result=recovery_reader_init(&thread_data, &reader)) != 0)
        {
            break;
        }

        while (g_continue_flag)
        {
            result = storage_binlog_read(&reader, &record, &record_length);
            if (result != 0)
            {
                if (result == ENOENT)
                {
                    result = 0;
                }
                break;
            }

            disp = dispatchs + (unsigned int)(Time33Hash(record.filename,
                    record.filename_len)) % g_disk_recovery_threads;
            if ((result=disk_recovery_write_to_binlog(disp->fp,
                            disp->temp_filename, &record)) != 0)
            {
                break;
            }
            disp->count++;
        }

        storage_reader_destroy(&reader);
        if (result != 0)
        {
            break;
        }
    }

    total_count = 0;
    *buff = '\0';
    log_buff.str = buff;
    log_buff.len = 0;
    for (i=0; i<g_disk_recovery_threads; i++)
    {
        fclose(dispatchs[i].fp);
        if (rename(dispatchs[i].temp_filename,
                    dispatchs[i].binlog_filename) != 0)
        {
            logError("file: "__FILE__", line: %d, "
                    "rename file %s to %s fail, "
                    "errno: %d, error info: %s", __LINE__,
                    dispatchs[i].temp_filename,
                    dispatchs[i].binlog_filename,
                    errno, STRERROR(errno));
            return errno != 0 ? errno : EPERM;
        }

        recovery_get_full_filename_ex(pBasePath, i,
                RECOVERY_MARK_FILENAME, mark_filename);
        if ((result=do_write_to_mark_file(mark_filename, 0)) != 0)
        {
            break;
        }

        total_count += dispatchs[i].count;
        stat(dispatchs[i].binlog_filename, &file_stat);
        log_buff.len += snprintf(log_buff.str + log_buff.len,
                sizeof(buff) - log_buff.len,
                ", {thread: #%d, lines: %"PRId64
                ", size: %"PRId64"}",
                i, dispatchs[i].count,
                (int64_t)file_stat.st_size);
    }
    free(dispatchs);

    logInfo("file: "__FILE__", line: %d, "
            "dispatch stats => total lines: %"PRId64"%s",
            __LINE__, total_count, log_buff.str);
    return result;
}

static int storage_disk_recovery_dispatch_binlog_for_threads(
        const char *pBasePath)
{
    int result;
    int i;
    char binlog_filename[MAX_PATH_SIZE];

    if (last_recovery_threads <= 0)
    {
        logError("file: "__FILE__", line: %d, "
                "invalid last recovery threads: %d, "
                "retry restore data for %s again ...",
                __LINE__, last_recovery_threads, pBasePath);
        return EAGAIN;
    }

    for (i=0; i<last_recovery_threads; i++)
    {
        recovery_get_full_filename_ex(pBasePath, i,
                RECOVERY_BINLOG_FILENAME, binlog_filename);
        if (!fileExists(binlog_filename))
        {
            logError("file: "__FILE__", line: %d, "
                    "binlog file %s not exist, "
                    "try to restart recovery ...",
                    __LINE__, binlog_filename);
            return EAGAIN;
        }
    }

    if (g_disk_recovery_threads == last_recovery_threads)
    {
        return 0;
    }

    logInfo("file: "__FILE__", line: %d, "
            "try to dispatch binlog from %d to %d threads, "
            "data path: %s ...", __LINE__, last_recovery_threads,
            g_disk_recovery_threads, pBasePath);

    result = do_dispatch_binlog_for_threads(pBasePath);
    if (result == 0)
    {
        char flag_filename[MAX_PATH_SIZE];

        logInfo("file: "__FILE__", line: %d, "
                "dispatch binlog for %d threads successfully, "
                "data path: %s.", __LINE__,
                g_disk_recovery_threads, pBasePath);

        if (g_disk_recovery_threads < last_recovery_threads)
        {
            storage_disk_recovery_delete_thread_files(
                    pBasePath, g_disk_recovery_threads,
                    last_recovery_threads);
        }

        recovery_get_flag_filename(pBasePath, flag_filename);
        return do_write_to_flag_file(flag_filename,
                true, g_disk_recovery_threads);
    }
    else
    {
        logError("file: "__FILE__", line: %d, "
                "dispatch binlog for %d threads fail, data path: %s.",
                __LINE__, g_disk_recovery_threads, pBasePath);
    }

    return result;
}

static int storage_disk_recovery_do_restore(const char *pBasePath)
{
    int result;
    int thread_count;
    int bytes;
    int i;
    int k;
    pthread_t *recovery_tids;
    void **args;
    RecoveryThreadData *thread_data;

    logInfo("file: "__FILE__", line: %d, "
            "disk recovery: begin recovery data path: %s, "
            "thread count: %d ...", __LINE__, pBasePath,
            g_disk_recovery_threads);

    bytes = sizeof(RecoveryThreadData) * g_disk_recovery_threads;
    thread_data = (RecoveryThreadData *)malloc(bytes);
    if (thread_data == NULL)
    {
        logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail", __LINE__, bytes);
        return ENOMEM;
    }

    bytes = sizeof(void *) * g_disk_recovery_threads;
    args = (void **)malloc(bytes);
    if (args == NULL)
    {
        logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail", __LINE__, bytes);
        return ENOMEM;
    }


    bytes = sizeof(pthread_t) * g_disk_recovery_threads;
    recovery_tids = (pthread_t *)malloc(bytes);
    if (recovery_tids == NULL)
    {
        logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail", __LINE__, bytes);
        return ENOMEM;
    }

    for (i=0; i<g_disk_recovery_threads; i++)
    {
        thread_data[i].base_path = pBasePath;
        thread_data[i].thread_index = i;
        thread_data[i].result = EINTR;
        thread_data[i].done = false;
        thread_data[i].alive = 0;
        args[i] = thread_data + i;
    }

    thread_count = g_disk_recovery_threads;
    if ((result=create_work_threads(&thread_count,
                    storage_disk_recovery_restore_entrance,
                    args, recovery_tids, g_thread_stack_size)) != 0)
    {
        return result;
    }

    do
    {
        sleep(5);
    } while (g_continue_flag && __sync_fetch_and_add(
                &current_recovery_thread_count, 0) > 0);

    if (__sync_fetch_and_add(&current_recovery_thread_count, 0) > 0)
    {
        for (i=0; i<30; i++)
        {
            if ((thread_count=__sync_fetch_and_add(
                &current_recovery_thread_count, 0)) == 0)
            {
                break;
            }

            for (k=0; k<g_disk_recovery_threads; k++)
            {
                if (__sync_fetch_and_add(&thread_data[k].alive, 0) > 0)
                {
                    pthread_kill(thread_data[k].tid, SIGINT);
                }
            }

            logInfo("file: "__FILE__", line: %d, "
                    "waiting for recovery threads exit, "
                    "waiting count: %d, current thread count: %d",
                    __LINE__, i+1, thread_count);
            sleep(1);
        }
    }

    sleep(1);  //wait for thread exit
    free(thread_data);
    free(args);
    free(recovery_tids);

    if (!g_continue_flag)
    {
        return EINTR;
    }

    while (g_continue_flag)
    {
        if (storage_report_storage_status(g_my_server_id_str,
                    g_tracker_client_ip.ips[0].address,
                    saved_storage_status) == 0)
        {
            break;
        }

        sleep(5);
    }

    if (!g_continue_flag)
    {
        return EINTR;
    }

    for (i=0; i<g_disk_recovery_threads; i++)
    {
        if (!thread_data[i].done)
        {
            return thread_data[i].result != 0 ?
                thread_data[i].result : EINTR;
        }
    }

    logInfo("file: "__FILE__", line: %d, "
            "disk recovery: end of recovery data path: %s",
            __LINE__, pBasePath);

    return storage_disk_recovery_finish(pBasePath);
}

int storage_disk_recovery_check_restore(const char *pBasePath)
{
	char flag_filename[MAX_PATH_SIZE];
	int result;

    recovery_get_flag_filename(pBasePath, flag_filename);
	if (!fileExists(flag_filename))
    {
        result = storage_disk_recovery_old_version_migrate(pBasePath);
        if (result != 0)
        {
            return (result == ENOENT) ? 0 : result;
        }
    }

    result = recovery_load_params_from_flag_file(flag_filename);
    if (result != 0)
    {
        return result;
    }

    if ((result=storage_disk_recovery_dispatch_binlog_for_threads(
                    pBasePath)) != 0)
    {
        return result;
    }

    return storage_disk_recovery_do_restore(pBasePath);
}

static int storage_compare_trunk_id_info(void *p1, void *p2)
{
	int result;
	result = memcmp(&(((FDFSTrunkFileIdInfo *)p1)->path), \
			&(((FDFSTrunkFileIdInfo *)p2)->path), \
			sizeof(FDFSTrunkPathInfo));
	if (result != 0)
	{
		return result;
	}

	return ((FDFSTrunkFileIdInfo *)p1)->id - ((FDFSTrunkFileIdInfo *)p2)->id;
}

static int tree_write_file_walk_callback(void *data, void *args)
{
	int result;

	if (fprintf((FILE *)args, "%s\n",
		((FDFSTrunkFileIdInfo *)data)->line) > 0)
	{
		return 0;
	}
	else
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, "
			"write to binlog file fail, "
			"errno: %d, error info: %s.",
			__LINE__, result, STRERROR(result));
		return EIO;
	}
}

static int disk_recovery_write_to_binlog(FILE *fp,
        const char *binlog_filename, StorageBinLogRecord *pRecord)
{
    int result;
    if (pRecord->op_type == STORAGE_OP_TYPE_SOURCE_CREATE_FILE
            || pRecord->op_type == STORAGE_OP_TYPE_REPLICA_CREATE_FILE)
    {
        if (fprintf(fp, "%d %c %s\n",
                    (int)pRecord->timestamp,
                    pRecord->op_type, pRecord->filename) < 0)
        {
            result = errno != 0 ? errno : EIO;
            logError("file: "__FILE__", line: %d, "
                    "write to file: %s fail, "
                    "errno: %d, error info: %s.",
                    __LINE__, binlog_filename,
                    result, STRERROR(result));
            return result;
        }
    }
    else
    {
        if (fprintf(fp, "%d %c %s %s\n",
                    (int)pRecord->timestamp,
                    pRecord->op_type, pRecord->filename,
                    pRecord->src_filename) < 0)
        {
            result = errno != 0 ? errno : EIO;
            logError("file: "__FILE__", line: %d, "
                    "write to file: %s fail, "
                    "errno: %d, error info: %s.",
                    __LINE__, binlog_filename,
                    result, STRERROR(result));
            return result;
        }
    }

    return 0;
}

static int storage_do_split_trunk_binlog(const int store_path_index, 
		StorageBinLogReader *pReader)
{
	FILE *fp;
	char *pBasePath;
	FDFSTrunkFileIdInfo *pFound;
	char binlogFullFilename[MAX_PATH_SIZE];
	char tmpFullFilename[MAX_PATH_SIZE];
	FDFSTrunkFullInfo trunk_info;
	FDFSTrunkFileIdInfo trunkFileId;
	StorageBinLogRecord record;
	AVLTreeInfo tree_unique_trunks;
	int record_length;
	int result;
	
	pBasePath = g_fdfs_store_paths.paths[store_path_index].path;
	recovery_get_full_filename_ex(pBasePath, -1,
		RECOVERY_BINLOG_FILENAME".tmp", tmpFullFilename);
	fp = fopen(tmpFullFilename, "w");
	if (fp == NULL)
	{
		result = errno != 0 ? errno : EPERM;
		logError("file: "__FILE__", line: %d, " \
			"open file: %s fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, tmpFullFilename,
			result, STRERROR(result));
		return result;
	}

	if ((result=avl_tree_init(&tree_unique_trunks, free, \
			storage_compare_trunk_id_info)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"avl_tree_init fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		fclose(fp);
		return result;
	}

	memset(&trunk_info, 0, sizeof(trunk_info));
	memset(&trunkFileId, 0, sizeof(trunkFileId));
	result = 0;
	while (g_continue_flag)
	{
		result=storage_binlog_read(pReader, &record, &record_length);
		if (result != 0)
		{
			if (result == ENOENT)
			{
				result = 0;
			}
			break;
		}

		if (fdfs_is_trunk_file(record.filename, record.filename_len))
		{
			if (fdfs_decode_trunk_info(store_path_index, \
				record.true_filename, record.true_filename_len,\
				&trunk_info) != 0)
			{
				continue;
			}

			trunkFileId.path = trunk_info.path;
			trunkFileId.id = trunk_info.file.id;
			pFound = (FDFSTrunkFileIdInfo *)avl_tree_find( \
					&tree_unique_trunks, &trunkFileId);
			if (pFound != NULL)
			{
				continue;
			}

			pFound = (FDFSTrunkFileIdInfo *)malloc( \
					sizeof(FDFSTrunkFileIdInfo));
			if (pFound == NULL)
			{
				result = errno != 0 ? errno : ENOMEM;
				logError("file: "__FILE__", line: %d, " \
					"malloc %d bytes fail, " \
					"errno: %d, error info: %s", __LINE__,\
					(int)sizeof(FDFSTrunkFileIdInfo), \
					result, STRERROR(result));
				break;
			}

			sprintf(trunkFileId.line, "%d %c %s", \
				(int)record.timestamp, \
				record.op_type, record.filename);
			memcpy(pFound, &trunkFileId, sizeof(FDFSTrunkFileIdInfo));
			if (avl_tree_insert(&tree_unique_trunks, pFound) != 1)
			{
				result = errno != 0 ? errno : ENOMEM;
				logError("file: "__FILE__", line: %d, " \
					"avl_tree_insert fail, " \
					"errno: %d, error info: %s", \
					__LINE__, result, STRERROR(result));
				break;
			}
		}
		else
		{
            if ((result=disk_recovery_write_to_binlog(fp,
                            tmpFullFilename, &record)) != 0)
            {
                break;
            }
		}
	}

	if (result == 0)
	{
		int tree_node_count;
		tree_node_count = avl_tree_count(&tree_unique_trunks);
		if (tree_node_count > 0)
		{
			logInfo("file: "__FILE__", line: %d, " \
				"recovering trunk file count: %d", __LINE__, \
				tree_node_count);

			result = avl_tree_walk(&tree_unique_trunks, \
				tree_write_file_walk_callback, fp);
		}
	}

	avl_tree_destroy(&tree_unique_trunks);
	fclose(fp);
	if (!g_continue_flag)
	{
		return EINTR;
	}

	if (result != 0)
	{
		return result;
	}

	recovery_get_full_filename_ex(pBasePath, 0,
		RECOVERY_BINLOG_FILENAME, binlogFullFilename);
	if (rename(tmpFullFilename, binlogFullFilename) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"rename file %s to %s fail, " \
			"errno: %d, error info: %s", __LINE__, \
			tmpFullFilename, binlogFullFilename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	return 0;
}

static int storage_disk_recovery_split_trunk_binlog(const int store_path_index)
{
	char mark_filename[MAX_PATH_SIZE];
    RecoveryThreadData thread_data;
	StorageBinLogReader reader;
	int result;

	thread_data.base_path = g_fdfs_store_paths.paths[store_path_index].path;
	thread_data.thread_index = 0;

    recovery_get_mark_filename(&thread_data, mark_filename);
    if ((result=do_write_to_mark_file(mark_filename, 0)) != 0)
    {
        return result;
    }

	if ((result=recovery_reader_init(&thread_data, &reader)) != 0)
	{
		storage_reader_destroy(&reader);
		return result;
	}

	result = storage_do_split_trunk_binlog(store_path_index, &reader);
	storage_reader_destroy(&reader);
	return result;
}

int storage_disk_recovery_prepare(const int store_path_index)
{
	ConnectionInfo srcStorage;
	ConnectionInfo *pStorageConn;
	int result;
	char *pBasePath;

	pBasePath = g_fdfs_store_paths.paths[store_path_index].path;
	if ((result=recovery_init_flag_file(pBasePath, false, -1)) != 0)
	{
		return result;
	}

	if ((result=recovery_init_global_binlog_file(pBasePath)) != 0)
	{
		return result;
	}

	if ((result=recovery_get_src_storage_server(&srcStorage)) != 0)
	{
		if (result == ENOENT)
		{
			return storage_disk_recovery_finish(pBasePath);
		}
		else
		{
			return result;
		}
	}

	while (g_continue_flag)
	{
		if (storage_report_storage_status(g_my_server_id_str, \
			g_tracker_client_ip.ips[0].address,
            FDFS_STORAGE_STATUS_RECOVERY) == 0)
		{
			break;
		}
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	if ((pStorageConn=tracker_make_connection(&srcStorage, &result)) == NULL)
	{
		return result;
	}

	logInfo("file: "__FILE__", line: %d, "
            "try to fetch binlog from %s:%d ...", __LINE__,
            pStorageConn->ip_addr, pStorageConn->port);

	result = storage_do_fetch_binlog(pStorageConn, store_path_index);
	tracker_close_connection_ex(pStorageConn, true);
	if (result != 0)
	{
		return result;
	}

    logInfo("file: "__FILE__", line: %d, "
            "fetch binlog from %s:%d successfully.", __LINE__,
            pStorageConn->ip_addr, pStorageConn->port);

	if ((result=storage_disk_recovery_split_trunk_binlog(
			store_path_index)) != 0)
	{
		char flagFullFilename[MAX_PATH_SIZE];
		unlink(recovery_get_flag_filename(pBasePath, flagFullFilename));
		return result;
	}

	//set fetch binlog done
	if ((result=recovery_init_flag_file(pBasePath, true, 1)) != 0)
	{
		return result;
	}

	return 0;
}
