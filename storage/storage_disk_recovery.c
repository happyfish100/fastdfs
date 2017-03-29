/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
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
#include "logger.h"
#include "fdfs_global.h"
#include "sockopt.h"
#include "avl_tree.h"
#include "shared_func.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
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

#define RECOVERY_BINLOG_FILENAME	".binlog.recovery"
#define RECOVERY_MARK_FILENAME		".recovery.mark"

#define MARK_ITEM_BINLOG_OFFSET    	"binlog_offset"
#define MARK_ITEM_FETCH_BINLOG_DONE    	"fetch_binlog_done"
#define MARK_ITEM_SAVED_STORAGE_STATUS	"saved_storage_status"

static int saved_storage_status = FDFS_STORAGE_STATUS_NONE;

static char *recovery_get_binlog_filename(const void *pArg, \
                        char *full_filename);

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

	pBasePath = g_fdfs_store_paths.paths[store_path_index];
	recovery_get_binlog_filename(pBasePath, full_binlog_filename);

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;

	long2buff(FDFS_GROUP_NAME_MAX_LEN + 1, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_FETCH_ONE_PATH_BINLOG;
	strcpy(out_buff + sizeof(TrackerHeader), g_group_name);
	*(out_buff + sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN) = \
			store_path_index;

	if((result=tcpsenddata_nb(pSrcStorage->sock, out_buff, \
		sizeof(out_buff), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pSrcStorage->ip_addr, pSrcStorage->port, \
			result, STRERROR(result));
		return result;
	}

	if ((result=fdfs_recv_header(pSrcStorage, &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
                "fdfs_recv_header fail, result: %d",
                __LINE__, result);
		return result;
	}

	if ((result=tcprecvfile(pSrcStorage->sock, full_binlog_filename, \
				in_bytes, 0, g_fdfs_network_timeout, \
				&file_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, tcprecvfile fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pSrcStorage->ip_addr, pSrcStorage->port, \
			result, STRERROR(result));
		return result;
	}

	logInfo("file: "__FILE__", line: %d, " \
		"recovery binlog file size: %"PRId64, \
		__LINE__, file_bytes);

	return 0;
}

static int recovery_get_src_storage_server(ConnectionInfo *pSrcStorage)
{
	int result;
	int storage_count;
	ConnectionInfo trackerServer;
	ConnectionInfo *pTrackerConn;
	FDFSGroupStat groupStat;
	FDFSStorageInfo storageStats[FDFS_MAX_SERVERS_EACH_GROUP];
	FDFSStorageInfo *pStorageStat;
	FDFSStorageInfo *pStorageEnd;

	memset(pSrcStorage, 0, sizeof(ConnectionInfo));
	pSrcStorage->sock = -1;

	logDebug("file: "__FILE__", line: %d, " \
		"disk recovery: get source storage server", \
		__LINE__);
	while (g_continue_flag)
	{
		result = tracker_get_storage_max_status(&g_tracker_group, \
                		g_group_name, g_tracker_client_ip, \
				g_my_server_id_str, &saved_storage_status);
		if (result == ENOENT)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"current storage: %s does not exist " \
				"in tracker server", __LINE__, \
				g_tracker_client_ip);
			return ENOENT;
		}

		if (result == 0)
		{
			if (saved_storage_status == FDFS_STORAGE_STATUS_INIT)
			{
				logInfo("file: "__FILE__", line: %d, " \
					"current storage: %s 's status is %d" \
					", does not need recovery", __LINE__, \
					g_tracker_client_ip, \
					saved_storage_status);
				return ENOENT;
			}

			if (saved_storage_status == FDFS_STORAGE_STATUS_IP_CHANGED || \
			    saved_storage_status == FDFS_STORAGE_STATUS_DELETED)
			{
				logWarning("file: "__FILE__", line: %d, " \
					"current storage: %s 's status is %d" \
					", does not need recovery", __LINE__, \
					g_tracker_client_ip, saved_storage_status);
				return ENOENT;
			}

			break;
		}

		sleep(1);
	}

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
			tracker_disconnect_server_ex(pTrackerConn, true);
			sleep(1);
			continue;
		}

		if (groupStat.count <= 0)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"storage server count: %d in the group <= 0!",\
				__LINE__, groupStat.count);

			tracker_disconnect_server(pTrackerConn);
			sleep(1);
			continue;
		}

		if (groupStat.count == 1)
		{
			logInfo("file: "__FILE__", line: %d, " \
				"storage server count in the group = 1, " \
				"does not need recovery", __LINE__);

			tracker_disconnect_server(pTrackerConn);
			return ENOENT;
		}

		if (g_fdfs_store_paths.count > groupStat.store_path_count)
		{
			logInfo("file: "__FILE__", line: %d, " \
				"storage store path count: %d > " \
				"which of the group: %d, " \
				"does not need recovery", __LINE__, \
				g_fdfs_store_paths.count, groupStat.store_path_count);

			tracker_disconnect_server(pTrackerConn);
			return ENOENT;
		}

		if (groupStat.active_count <= 0)
		{
			tracker_disconnect_server(pTrackerConn);
			sleep(5);
			continue;
		}

		result = tracker_list_servers(pTrackerConn, \
                		g_group_name, NULL, storageStats, \
				FDFS_MAX_SERVERS_EACH_GROUP, &storage_count);
		tracker_disconnect_server_ex(pTrackerConn, result != 0);
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

		pStorageEnd = storageStats + storage_count;
		for (pStorageStat=storageStats; pStorageStat<pStorageEnd; \
			pStorageStat++)
		{
			if (strcmp(pStorageStat->id, g_my_server_id_str) == 0)
			{
				continue;
			}

			if (pStorageStat->status == FDFS_STORAGE_STATUS_ACTIVE)
			{
				strcpy(pSrcStorage->ip_addr, \
					pStorageStat->ip_addr);
				pSrcStorage->port = pStorageStat->storage_port;
				break;
			}
		}

		if (pStorageStat < pStorageEnd)  //found src storage server
		{
			break;
		}

		sleep(5);
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	logDebug("file: "__FILE__", line: %d, " \
		"disk recovery: get source storage server %s:%d", \
		__LINE__, pSrcStorage->ip_addr, pSrcStorage->port);
	return 0;
}

static char *recovery_get_full_filename(const void *pArg, \
		const char *filename, char *full_filename)
{
	const char *pBasePath;
	static char buff[MAX_PATH_SIZE];

	pBasePath = (const char *)pArg;
	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	snprintf(full_filename, MAX_PATH_SIZE, \
		"%s/data/%s", pBasePath, filename);

	return full_filename;
}

static char *recovery_get_binlog_filename(const void *pArg, \
                        char *full_filename)
{
	return recovery_get_full_filename(pArg, \
			RECOVERY_BINLOG_FILENAME, full_filename);
}

static char *recovery_get_mark_filename(const void *pArg, \
                        char *full_filename)
{
	return recovery_get_full_filename(pArg, \
			RECOVERY_MARK_FILENAME, full_filename);
}

static int storage_disk_recovery_finish(const char *pBasePath)
{
	char full_filename[MAX_PATH_SIZE];

	recovery_get_binlog_filename(pBasePath, full_filename);
	if (fileExists(full_filename))
	{
		if (unlink(full_filename) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"delete recovery binlog file: %s fail, " \
				"errno: %d, error info: %s", \
				 __LINE__, full_filename, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}
	}

	recovery_get_mark_filename(pBasePath, full_filename);
	if (fileExists(full_filename))
	{
		if (unlink(full_filename) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"delete recovery mark file: %s fail, " \
				"errno: %d, error info: %s", \
				 __LINE__, full_filename, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}
	}

	return 0;
}

static int recovery_write_to_mark_file(const char *pBasePath, \
			StorageBinLogReader *pReader)
{
	char buff[128];
	int len;

	len = sprintf(buff, \
		"%s=%d\n" \
		"%s=%"PRId64"\n"  \
		"%s=1\n",  \
		MARK_ITEM_SAVED_STORAGE_STATUS, saved_storage_status, \
		MARK_ITEM_BINLOG_OFFSET, pReader->binlog_offset, \
		MARK_ITEM_FETCH_BINLOG_DONE);

	return storage_write_to_fd(pReader->mark_fd, \
		recovery_get_mark_filename, pBasePath, buff, len);
}

static int recovery_init_binlog_file(const char *pBasePath)
{
	char full_binlog_filename[MAX_PATH_SIZE];
	char buff[1];

	*buff = '\0';
	recovery_get_binlog_filename(pBasePath, full_binlog_filename);
	return writeToFile(full_binlog_filename, buff, 0);
}

static int recovery_init_mark_file(const char *pBasePath, \
		const bool fetch_binlog_done)
{
	char full_filename[MAX_PATH_SIZE];
	char buff[128];
	int len;

	recovery_get_mark_filename(pBasePath, full_filename);

	len = sprintf(buff, \
		"%s=%d\n" \
		"%s=0\n" \
		"%s=%d\n", \
		MARK_ITEM_SAVED_STORAGE_STATUS, saved_storage_status, \
		MARK_ITEM_BINLOG_OFFSET, \
		MARK_ITEM_FETCH_BINLOG_DONE, fetch_binlog_done);
	return writeToFile(full_filename, buff, len);
}

static int recovery_reader_init(const char *pBasePath, \
                        StorageBinLogReader *pReader)
{
	char full_mark_filename[MAX_PATH_SIZE];
	IniContext iniContext;
	int result;

	memset(pReader, 0, sizeof(StorageBinLogReader));
	pReader->mark_fd = -1;
	pReader->binlog_fd = -1;
	pReader->binlog_index = g_binlog_index + 1;

	pReader->binlog_buff.buffer = (char *)malloc( \
			STORAGE_BINLOG_BUFFER_SIZE);
	if (pReader->binlog_buff.buffer == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", \
			__LINE__, STORAGE_BINLOG_BUFFER_SIZE, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	pReader->binlog_buff.current = pReader->binlog_buff.buffer;

	recovery_get_mark_filename(pBasePath, full_mark_filename);
	memset(&iniContext, 0, sizeof(IniContext));
	if ((result=iniLoadFromFile(full_mark_filename, &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"load from mark file \"%s\" fail, " \
			"error code: %d", __LINE__, \
			full_mark_filename, result);
		return result;
	}

	if (!iniGetBoolValue(NULL, MARK_ITEM_FETCH_BINLOG_DONE, \
			&iniContext, false))
	{
		iniFreeContext(&iniContext);

		logInfo("file: "__FILE__", line: %d, " \
			"mark file \"%s\", %s=0, " \
			"need to fetch binlog again", __LINE__, \
			full_mark_filename, MARK_ITEM_FETCH_BINLOG_DONE);
		return EAGAIN;
	}

	saved_storage_status = iniGetIntValue(NULL, \
			MARK_ITEM_SAVED_STORAGE_STATUS, &iniContext, -1);
	if (saved_storage_status < 0)
	{
		iniFreeContext(&iniContext);

		logError("file: "__FILE__", line: %d, " \
			"in mark file \"%s\", %s: %d < 0", __LINE__, \
			full_mark_filename, MARK_ITEM_SAVED_STORAGE_STATUS, \
			saved_storage_status);
		return EINVAL;
	}

	pReader->binlog_offset = iniGetInt64Value(NULL, \
			MARK_ITEM_BINLOG_OFFSET, &iniContext, -1);
	if (pReader->binlog_offset < 0)
	{
		iniFreeContext(&iniContext);

		logError("file: "__FILE__", line: %d, " \
			"in mark file \"%s\", %s: "\
			"%"PRId64" < 0", __LINE__, \
			full_mark_filename, MARK_ITEM_BINLOG_OFFSET, \
			pReader->binlog_offset);
		return EINVAL;
	}

	iniFreeContext(&iniContext);

	pReader->mark_fd = open(full_mark_filename, O_WRONLY | O_CREAT, 0644);
	if (pReader->mark_fd < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open mark file \"%s\" fail, " \
			"error no: %d, error info: %s", \
			__LINE__, full_mark_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	if ((result=storage_open_readable_binlog(pReader, \
			recovery_get_binlog_filename, pBasePath)) != 0)
	{
		return result;
	}

	return 0;
}

static int recovery_reader_check_init(const char *pBasePath, \
                        StorageBinLogReader *pReader)
{
    if (pReader->binlog_fd >= 0 && pReader->binlog_buff.buffer != NULL)
    {
        return 0;
    }

    return recovery_reader_init(pBasePath, pReader);
}

static int storage_do_recovery(const char *pBasePath, StorageBinLogReader *pReader, \
		ConnectionInfo *pSrcStorage)
{
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageConn;
	FDFSTrunkFullInfo trunk_info;
	StorageBinLogRecord record;
	int record_length;
	int result;
	int log_level;
	int count;
	int store_path_index;
	int64_t file_size;
	int64_t total_count;
	int64_t success_count;
	int64_t noent_count;
	bool bContinueFlag;
	char local_filename[MAX_PATH_SIZE];
	char src_filename[MAX_PATH_SIZE];

	pTrackerServer = g_tracker_group.servers;
	count = 0;
	total_count = 0;
	success_count = 0;
    noent_count = 0;
	result = 0;

	logInfo("file: "__FILE__", line: %d, " \
		"disk recovery: recovering files of data path: %s ...", \
		__LINE__, pBasePath);

	bContinueFlag = true;
	while (bContinueFlag)
	{
    if ((result=recovery_reader_check_init(pBasePath, pReader)) != 0)
    {
        break;
    }
	if ((pStorageConn=tracker_connect_server(pSrcStorage, &result)) == NULL)
	{
		sleep(5);
		continue;
	}

	while (g_continue_flag)
	{
		result=storage_binlog_read(pReader, &record, &record_length);
		if (result != 0)
		{
			if (result == ENOENT)
			{
				result = 0;
			}
			bContinueFlag = false;
			break;
		}

		total_count++;
		if (record.op_type == STORAGE_OP_TYPE_SOURCE_CREATE_FILE
		 || record.op_type == STORAGE_OP_TYPE_REPLICA_CREATE_FILE)
		{
			bool bTrunkFile;

			if (fdfs_is_trunk_file(record.filename, \
					record.filename_len))
			{
			char *pTrunkPathEnd;
			char *pLocalFilename;

			bTrunkFile = true;
			if (fdfs_decode_trunk_info(record.store_path_index, \
				record.true_filename, record.true_filename_len,\
				&trunk_info) != 0)
			{
				pReader->binlog_offset += record_length;
				count++;
				continue;
			}

			trunk_get_full_filename(&trunk_info, \
                		local_filename, sizeof(local_filename));

			pTrunkPathEnd = strrchr(record.filename, '/');
			pLocalFilename = strrchr(local_filename, '/');
			if (pTrunkPathEnd == NULL || pLocalFilename == NULL)
			{
				pReader->binlog_offset += record_length;
				count++;
				continue;
			}
			sprintf(pTrunkPathEnd + 1, "%s", pLocalFilename + 1);
			}
			else
			{
			bTrunkFile = false;
			sprintf(local_filename, "%s/data/%s", \
				g_fdfs_store_paths.paths[record.store_path_index], \
				record.true_filename);
			}

			result = storage_download_file_to_file(pTrackerServer, \
					pStorageConn, g_group_name, \
					record.filename, local_filename, \
					&file_size);
			if (result == 0)
			{
				if (!bTrunkFile)
				{
					set_file_utimes(local_filename, \
						record.timestamp);
				}

				success_count++;
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
				g_fdfs_store_paths.paths[store_path_index], \
				record.true_filename);

			if ((result=storage_split_filename_ex( \
				record.src_filename, &record.src_filename_len,\
				record.true_filename, &store_path_index)) != 0)
			{
				bContinueFlag = false;
				break;
			}
			sprintf(src_filename, "%s/data/%s", \
				g_fdfs_store_paths.paths[store_path_index], \
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
			logDebug("file: "__FILE__", line: %d, " \
				"disk recovery: recover path: %s, " \
				"file count: %"PRId64 \
				", success count: %"PRId64", noent_count: %"PRId64, \
				__LINE__, pBasePath, total_count, \
				success_count, noent_count);
			recovery_write_to_mark_file(pBasePath, pReader);
			count = 0;
		}
	}

	tracker_disconnect_server_ex(pStorageConn, result != 0);
    recovery_write_to_mark_file(pBasePath, pReader);
    if (bContinueFlag)
    {
	    storage_reader_destroy(pReader);
    }

	if (count > 0)
	{
		count = 0;

		logInfo("file: "__FILE__", line: %d, " \
			"disk recovery: recover path: %s, " \
			"file count: %"PRId64 \
            ", success count: %"PRId64", noent_count: %"PRId64, \
			__LINE__, pBasePath, total_count, success_count, noent_count);
	}
	else
	{
		sleep(5);
	}
	}

	if (result == 0)
	{
		logInfo("file: "__FILE__", line: %d, " \
			"disk recovery: recover files of data path: %s done", \
			__LINE__, pBasePath);
	}

	return result;
}

int storage_disk_recovery_restore(const char *pBasePath)
{
	char full_binlog_filename[MAX_PATH_SIZE];
	char full_mark_filename[MAX_PATH_SIZE];
	ConnectionInfo srcStorage;
	int result;
	StorageBinLogReader reader;

	recovery_get_binlog_filename(pBasePath, full_binlog_filename);
	recovery_get_mark_filename(pBasePath, full_mark_filename);

	if (!(fileExists(full_mark_filename) && \
	      fileExists(full_binlog_filename)))
	{
		return 0;
	}

	logInfo("file: "__FILE__", line: %d, " \
		"disk recovery: begin recovery data path: %s ...", \
		__LINE__, pBasePath);

	if ((result=recovery_get_src_storage_server(&srcStorage)) != 0)
	{
		if (result == ENOENT)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"no source storage server, " \
				"disk recovery finished!", __LINE__);
			return storage_disk_recovery_finish(pBasePath);
		}
		else
		{
			return result;
		}
	}

	if ((result=recovery_reader_init(pBasePath, &reader)) != 0)
	{
		storage_reader_destroy(&reader);
		return result;
	}

	result = storage_do_recovery(pBasePath, &reader, &srcStorage);

	recovery_write_to_mark_file(pBasePath, &reader);
	storage_reader_destroy(&reader);

	if (result != 0)
	{
		return result;
	}

	while (g_continue_flag)
	{
		if (storage_report_storage_status(g_my_server_id_str, \
			g_tracker_client_ip, saved_storage_status) == 0)
		{
			break;
		}

		sleep(5);
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	logInfo("file: "__FILE__", line: %d, " \
		"disk recovery: end of recovery data path: %s", \
		__LINE__, pBasePath);

	return storage_disk_recovery_finish(pBasePath);
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

	if (fprintf((FILE *)args, "%s\n", \
		((FDFSTrunkFileIdInfo *)data)->line) > 0)
	{
		return 0;
	}
	else
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"write to binlog file fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, result, STRERROR(result));
		return EIO;
	}
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
	
	pBasePath = g_fdfs_store_paths.paths[store_path_index];
	recovery_get_full_filename(pBasePath, \
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
			if (record.op_type == STORAGE_OP_TYPE_SOURCE_CREATE_FILE
		 	|| record.op_type == STORAGE_OP_TYPE_REPLICA_CREATE_FILE)
			{
				if (fprintf(fp, "%d %c %s\n", \
					(int)record.timestamp, \
					record.op_type, record.filename) < 0)
				{
					result = errno != 0 ? errno : EIO;
					logError("file: "__FILE__", line: %d, " \
						"write to file: %s fail, " \
						"errno: %d, error info: %s.", \
						__LINE__, tmpFullFilename,
						result, STRERROR(result));
					break;
				}
			}
			else
			{
				if (fprintf(fp, "%d %c %s %s\n", \
					(int)record.timestamp, \
					record.op_type, record.filename, \
					record.src_filename) < 0)
				{
					result = errno != 0 ? errno : EIO;
					logError("file: "__FILE__", line: %d, " \
						"write to file: %s fail, " \
						"errno: %d, error info: %s.", \
						__LINE__, tmpFullFilename,
						result, STRERROR(result));
					break;
				}
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

	recovery_get_full_filename(pBasePath, \
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
	char *pBasePath;
	StorageBinLogReader reader;
	int result;

	pBasePath = g_fdfs_store_paths.paths[store_path_index];
	if ((result=recovery_reader_init(pBasePath, &reader)) != 0)
	{
		storage_reader_destroy(&reader);
		return result;
	}

	result = storage_do_split_trunk_binlog(store_path_index, &reader);

	storage_reader_destroy(&reader);
	return result;
}

int storage_disk_recovery_start(const int store_path_index)
{
	ConnectionInfo srcStorage;
	ConnectionInfo *pStorageConn;
	int result;
	char *pBasePath;

	pBasePath = g_fdfs_store_paths.paths[store_path_index];
	if ((result=recovery_init_mark_file(pBasePath, false)) != 0)
	{
		return result;
	}

	if ((result=recovery_init_binlog_file(pBasePath)) != 0)
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
			g_tracker_client_ip, FDFS_STORAGE_STATUS_RECOVERY) == 0)
		{
			break;
		}
	}

	if (!g_continue_flag)
	{
		return EINTR;
	}

	if ((pStorageConn=tracker_connect_server(&srcStorage, &result)) == NULL)
	{
		return result;
	}

	result = storage_do_fetch_binlog(pStorageConn, store_path_index);
	tracker_disconnect_server_ex(pStorageConn, true);
	if (result != 0)
	{
		return result;
	}

	//set fetch binlog done
	if ((result=recovery_init_mark_file(pBasePath, true)) != 0)
	{
		return result;
	}

	if ((result=storage_disk_recovery_split_trunk_binlog( \
			store_path_index)) != 0)
	{
		char markFullFilename[MAX_PATH_SIZE];
		unlink(recovery_get_mark_filename(pBasePath, markFullFilename));
		return result;
	}

	return 0;
}

