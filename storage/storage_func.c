/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//storage_func.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
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
#include "connection_pool.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "fdfs_shared_func.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_param_getter.h"
#include "storage_ip_changed_dealer.h"
#include "fdht_global.h"
#include "fdht_func.h"
#include "fdht_client.h"
#include "client_func.h"
#include "trunk_mem.h"
#include "trunk_sync.h"
#include "storage_disk_recovery.h"
#include "tracker_client.h"

#ifdef WITH_HTTPD
#include "fdfs_http_shared.h"
#endif

#define DATA_DIR_INITED_FILENAME	".data_init_flag"
#define STORAGE_STAT_FILENAME		"storage_stat.dat"

#define INIT_ITEM_STORAGE_JOIN_TIME	"storage_join_time"
#define INIT_ITEM_SYNC_OLD_DONE		"sync_old_done"
#define INIT_ITEM_SYNC_SRC_SERVER	"sync_src_server"
#define INIT_ITEM_SYNC_UNTIL_TIMESTAMP	"sync_until_timestamp"
#define INIT_ITEM_LAST_IP_ADDRESS	"last_ip_addr"
#define INIT_ITEM_LAST_SERVER_PORT	"last_server_port"
#define INIT_ITEM_LAST_HTTP_PORT	"last_http_port"
#define INIT_ITEM_CURRENT_TRUNK_FILE_ID "current_trunk_file_id"
#define INIT_ITEM_TRUNK_LAST_COMPRESS_TIME "trunk_last_compress_time"

#define STAT_ITEM_TOTAL_UPLOAD		"total_upload_count"
#define STAT_ITEM_SUCCESS_UPLOAD	"success_upload_count"
#define STAT_ITEM_TOTAL_APPEND		"total_append_count"
#define STAT_ITEM_SUCCESS_APPEND	"success_append_count"
#define STAT_ITEM_TOTAL_MODIFY		"total_modify_count"
#define STAT_ITEM_SUCCESS_MODIFY	"success_modify_count"
#define STAT_ITEM_TOTAL_TRUNCATE	"total_truncate_count"
#define STAT_ITEM_SUCCESS_TRUNCATE	"success_truncate_count"
#define STAT_ITEM_TOTAL_DOWNLOAD	"total_download_count"
#define STAT_ITEM_SUCCESS_DOWNLOAD	"success_download_count"
#define STAT_ITEM_LAST_SOURCE_UPD	"last_source_update"
#define STAT_ITEM_LAST_SYNC_UPD		"last_sync_update"
#define STAT_ITEM_TOTAL_SET_META	"total_set_meta_count"
#define STAT_ITEM_SUCCESS_SET_META	"success_set_meta_count"
#define STAT_ITEM_TOTAL_DELETE		"total_delete_count"
#define STAT_ITEM_SUCCESS_DELETE	"success_delete_count"
#define STAT_ITEM_TOTAL_GET_META	"total_get_meta_count"
#define STAT_ITEM_SUCCESS_GET_META	"success_get_meta_count"
#define STAT_ITEM_TOTAL_CREATE_LINK	"total_create_link_count"
#define STAT_ITEM_SUCCESS_CREATE_LINK	"success_create_link_count"
#define STAT_ITEM_TOTAL_DELETE_LINK	"total_delete_link_count"
#define STAT_ITEM_SUCCESS_DELETE_LINK	"success_delete_link_count"
#define STAT_ITEM_TOTAL_UPLOAD_BYTES	"total_upload_bytes"
#define STAT_ITEM_SUCCESS_UPLOAD_BYTES	"success_upload_bytes"
#define STAT_ITEM_TOTAL_APPEND_BYTES	"total_append_bytes"
#define STAT_ITEM_SUCCESS_APPEND_BYTES	"success_append_bytes"
#define STAT_ITEM_TOTAL_MODIFY_BYTES	"total_modify_bytes"
#define STAT_ITEM_SUCCESS_MODIFY_BYTES	"success_modify_bytes"
#define STAT_ITEM_TOTAL_DOWNLOAD_BYTES	"total_download_bytes"
#define STAT_ITEM_SUCCESS_DOWNLOAD_BYTES "success_download_bytes"
#define STAT_ITEM_TOTAL_SYNC_IN_BYTES      "total_sync_in_bytes"
#define STAT_ITEM_SUCCESS_SYNC_IN_BYTES    "success_sync_in_bytes"
#define STAT_ITEM_TOTAL_SYNC_OUT_BYTES     "total_sync_out_bytes"
#define STAT_ITEM_SUCCESS_SYNC_OUT_BYTES   "success_sync_out_bytes"
#define STAT_ITEM_TOTAL_FILE_OPEN_COUNT    "total_file_open_count"
#define STAT_ITEM_SUCCESS_FILE_OPEN_COUNT  "success_file_open_count"
#define STAT_ITEM_TOTAL_FILE_READ_COUNT    "total_file_read_count"
#define STAT_ITEM_SUCCESS_FILE_READ_COUNT  "success_file_read_count"
#define STAT_ITEM_TOTAL_FILE_WRITE_COUNT   "total_file_write_count"
#define STAT_ITEM_SUCCESS_FILE_WRITE_COUNT "success_file_write_count"

#define STAT_ITEM_DIST_PATH_INDEX_HIGH	"dist_path_index_high"
#define STAT_ITEM_DIST_PATH_INDEX_LOW	"dist_path_index_low"
#define STAT_ITEM_DIST_WRITE_FILE_COUNT	"dist_write_file_count"

static int storage_stat_fd = -1;

/*
static pthread_mutex_t fsync_thread_mutex;
static pthread_cond_t fsync_thread_cond;
static int fsync_thread_count = 0;
*/

static pthread_mutex_t sync_stat_file_lock;

static int storage_open_stat_file();
static int storage_close_stat_file();
static int storage_make_data_dirs(const char *pBasePath, bool *pathCreated);
static int storage_check_and_make_data_dirs();

static int storage_do_get_group_name(ConnectionInfo *pTrackerServer)
{
	char out_buff[sizeof(TrackerHeader) + 4];
	TrackerHeader *pHeader;
	char *pInBuff;
	int64_t in_bytes;
	int result;

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	long2buff(4, pHeader->pkg_len);
    int2buff(g_server_port, out_buff + sizeof(TrackerHeader));
	pHeader->cmd = TRACKER_PROTO_CMD_STORAGE_GET_GROUP_NAME;
	if ((result=tcpsenddata_nb(pTrackerServer->sock, out_buff, \
			sizeof(out_buff), g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, send data fail, " \
			"errno: %d, error info: %s.", \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, \
			result, STRERROR(result));
		return result;
	}

	pInBuff = g_group_name;
	if ((result=fdfs_recv_response(pTrackerServer, \
		&pInBuff, FDFS_GROUP_NAME_MAX_LEN, &in_bytes)) != 0)
	{
		logError("file: "__FILE__", line: %d, "
                "fdfs_recv_response fail, result: %d",
                __LINE__, result);
		return result;
	}

	if (in_bytes != FDFS_GROUP_NAME_MAX_LEN)
	{
		logError("file: "__FILE__", line: %d, " \
			"tracker server %s:%d, recv body length: " \
			"%"PRId64" != %d",  \
			__LINE__, pTrackerServer->ip_addr, \
			pTrackerServer->port, in_bytes, FDFS_GROUP_NAME_MAX_LEN);
		return EINVAL;
	}

	return 0;
}

static int storage_get_group_name_from_tracker()
{
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pServerEnd;
	ConnectionInfo *pTrackerConn;
	ConnectionInfo tracker_server;
	int result;

	result = ENOENT;
	pServerEnd = g_tracker_group.servers + g_tracker_group.server_count;
	for (pTrackerServer=g_tracker_group.servers; \
		pTrackerServer<pServerEnd; pTrackerServer++)
	{
		memcpy(&tracker_server, pTrackerServer, \
			sizeof(ConnectionInfo));
		tracker_server.sock = -1;
        if ((pTrackerConn=tracker_connect_server(&tracker_server, \
			&result)) == NULL)
		{
			continue;
		}

        result = storage_do_get_group_name(pTrackerConn);
		tracker_disconnect_server_ex(pTrackerConn, \
			result != 0 && result != ENOENT);
		if (result == 0)
		{
			return 0;
		}
	}

	return result;
}

static int tracker_get_my_server_id()
{
	struct in_addr ip_addr;

	if (inet_pton(AF_INET, g_tracker_client_ip, &ip_addr) == 1)
	{
		g_server_id_in_filename = ip_addr.s_addr;
	}
	else
	{
		logError("file: "__FILE__", line: %d, " \
			"call inet_pton for ip: %s fail", \
		__LINE__,g_tracker_client_ip);
		g_server_id_in_filename = INADDR_NONE;
	}

	if (g_use_storage_id)
	{
		ConnectionInfo *pTrackerServer;
		int result;

		pTrackerServer = tracker_get_connection();
		if (pTrackerServer == NULL)
		{
			return errno != 0 ? errno : ECONNREFUSED;
		}

		result = tracker_get_storage_id(pTrackerServer, \
			g_group_name, g_tracker_client_ip, g_my_server_id_str);
		tracker_disconnect_server_ex(pTrackerServer, result != 0);
		if (result != 0)
		{
			return result;
		}

		if (g_id_type_in_filename == FDFS_ID_TYPE_SERVER_ID)
		{
			g_server_id_in_filename = atoi(g_my_server_id_str);
		}
	}
	else
	{
		snprintf(g_my_server_id_str, sizeof(g_my_server_id_str), "%s", \
			g_tracker_client_ip);
	}

	logInfo("file: "__FILE__", line: %d, " \
		"tracker_client_ip: %s, my_server_id_str: %s, " \
		"g_server_id_in_filename: %d", __LINE__, \
		g_tracker_client_ip, g_my_server_id_str, g_server_id_in_filename);
	return 0;
}

static char *get_storage_stat_filename(const void *pArg, char *full_filename)
{
	static char buff[MAX_PATH_SIZE];

	if (full_filename == NULL)
	{
		full_filename = buff;
	}

	snprintf(full_filename, MAX_PATH_SIZE, \
			"%s/data/%s", g_fdfs_base_path, STORAGE_STAT_FILENAME);
	return full_filename;
}

int storage_write_to_fd(int fd, get_filename_func filename_func, \
		const void *pArg, const char *buff, const int len)
{
	if (ftruncate(fd, 0) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"truncate file \"%s\" to empty fail, " \
			"error no: %d, error info: %s", \
			__LINE__, filename_func(pArg, NULL), \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	if (lseek(fd, 0, SEEK_SET) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"rewind file \"%s\" to start fail, " \
			"error no: %d, error info: %s", \
			__LINE__, filename_func(pArg, NULL), \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	if (fc_safe_write(fd, buff, len) != len)
	{
		logError("file: "__FILE__", line: %d, " \
			"write to file \"%s\" fail, " \
			"error no: %d, error info: %s", \
			__LINE__, filename_func(pArg, NULL), \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	if (fsync(fd) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"sync file \"%s\" to disk fail, " \
			"error no: %d, error info: %s", \
			__LINE__, filename_func(pArg, NULL), \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	return 0;
}

static int storage_open_stat_file()
{
	char full_filename[MAX_PATH_SIZE];
	IniContext iniContext;
	int result;

	get_storage_stat_filename(NULL, full_filename);
	if (fileExists(full_filename))
	{
		if ((result=iniLoadFromFile(full_filename, &iniContext)) \
			 != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"load from stat file \"%s\" fail, " \
				"error code: %d", \
				__LINE__, full_filename, result);
			return result;
		}

		if (iniContext.global.count < 12)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, " \
				"in stat file \"%s\", item count: %d < 12", \
				__LINE__, full_filename, iniContext.global.count);
			return ENOENT;
		}

		g_storage_stat.total_upload_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_UPLOAD, &iniContext, 0);
		g_storage_stat.success_upload_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_UPLOAD, &iniContext, 0);
		g_storage_stat.total_append_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_APPEND, &iniContext, 0);
		g_storage_stat.success_append_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_APPEND, &iniContext, 0);
		g_storage_stat.total_modify_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_MODIFY, &iniContext, 0);
		g_storage_stat.success_modify_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_MODIFY, &iniContext, 0);
		g_storage_stat.total_truncate_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_TRUNCATE, &iniContext, 0);
		g_storage_stat.success_truncate_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_TRUNCATE, &iniContext, 0);
		g_storage_stat.total_download_count = iniGetInt64Value(NULL,  \
				STAT_ITEM_TOTAL_DOWNLOAD, &iniContext, 0);
		g_storage_stat.success_download_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_DOWNLOAD, &iniContext, 0);
		g_storage_stat.last_source_update = iniGetIntValue(NULL, \
				STAT_ITEM_LAST_SOURCE_UPD, &iniContext, 0);
		g_storage_stat.last_sync_update = iniGetIntValue(NULL, \
				STAT_ITEM_LAST_SYNC_UPD, &iniContext, 0);
		g_storage_stat.total_set_meta_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_SET_META, &iniContext, 0);
		g_storage_stat.success_set_meta_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_SET_META, &iniContext, 0);
		g_storage_stat.total_delete_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_DELETE, &iniContext, 0);
		g_storage_stat.success_delete_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_DELETE, &iniContext, 0);
		g_storage_stat.total_get_meta_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_GET_META, &iniContext, 0);
		g_storage_stat.success_get_meta_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_GET_META, &iniContext, 0);
		g_storage_stat.total_create_link_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_CREATE_LINK, &iniContext, 0);
		g_storage_stat.success_create_link_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_CREATE_LINK, &iniContext, 0);
		g_storage_stat.total_delete_link_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_DELETE_LINK, &iniContext, 0);
		g_storage_stat.success_delete_link_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_DELETE_LINK, &iniContext, 0);
		g_storage_stat.total_upload_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_UPLOAD_BYTES, &iniContext, 0);
		g_storage_stat.success_upload_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_UPLOAD_BYTES, &iniContext, 0);
		g_storage_stat.total_append_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_APPEND_BYTES, &iniContext, 0);
		g_storage_stat.success_append_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_APPEND_BYTES, &iniContext, 0);
		g_storage_stat.total_modify_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_MODIFY_BYTES, &iniContext, 0);
		g_storage_stat.success_modify_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_MODIFY_BYTES, &iniContext, 0);
		g_storage_stat.total_download_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_DOWNLOAD_BYTES, &iniContext, 0);
		g_storage_stat.success_download_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_DOWNLOAD_BYTES, &iniContext, 0);
		g_storage_stat.total_sync_in_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_SYNC_IN_BYTES, &iniContext, 0);
		g_storage_stat.success_sync_in_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_SYNC_IN_BYTES, &iniContext, 0);
		g_storage_stat.total_sync_out_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_SYNC_OUT_BYTES, &iniContext, 0);
		g_storage_stat.success_sync_out_bytes = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_SYNC_OUT_BYTES, &iniContext, 0);
		g_storage_stat.total_file_open_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_FILE_OPEN_COUNT, &iniContext, 0);
		g_storage_stat.success_file_open_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_FILE_OPEN_COUNT, &iniContext, 0);
		g_storage_stat.total_file_read_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_FILE_READ_COUNT, &iniContext, 0);
		g_storage_stat.success_file_read_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_FILE_READ_COUNT, &iniContext, 0);
		g_storage_stat.total_file_write_count = iniGetInt64Value(NULL, \
				STAT_ITEM_TOTAL_FILE_WRITE_COUNT, &iniContext, 0);
		g_storage_stat.success_file_write_count = iniGetInt64Value(NULL, \
				STAT_ITEM_SUCCESS_FILE_WRITE_COUNT, &iniContext, 0);
		g_dist_path_index_high = iniGetIntValue(NULL, \
				STAT_ITEM_DIST_PATH_INDEX_HIGH, &iniContext, 0);
		g_dist_path_index_low = iniGetIntValue(NULL, \
				STAT_ITEM_DIST_PATH_INDEX_LOW, &iniContext, 0);
		g_dist_write_file_count = iniGetIntValue(NULL, \
				STAT_ITEM_DIST_WRITE_FILE_COUNT, &iniContext, 0);

		iniFreeContext(&iniContext);
	}
	else
	{
		memset(&g_storage_stat, 0, sizeof(g_storage_stat));
	}

	storage_stat_fd = open(full_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (storage_stat_fd < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open stat file \"%s\" fail, " \
			"error no: %d, error info: %s", \
			__LINE__, full_filename, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	if ((result=storage_write_to_stat_file()) != 0)
	{
		return result;
	}

	STORAGE_FCHOWN(storage_stat_fd, full_filename, geteuid(), getegid())
	return 0;
}

static int storage_close_stat_file()
{
	int result;

	result = 0;
	if (storage_stat_fd >= 0)
	{
		result = storage_write_to_stat_file();
		if (close(storage_stat_fd) != 0)
		{
			result += errno != 0 ? errno : ENOENT;
		}
		storage_stat_fd = -1;
	}

	return result;
}

int storage_write_to_stat_file()
{
	char buff[2048];
	int len;
	int result;
	int write_ret;

	len = sprintf(buff, 
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%"PRId64"\n"  \
		"%s=%d\n"  \
		"%s=%d\n"  \
		"%s=%d\n"  \
		"%s=%d\n"  \
		"%s=%d\n", \
		STAT_ITEM_TOTAL_UPLOAD, g_storage_stat.total_upload_count, \
		STAT_ITEM_SUCCESS_UPLOAD, g_storage_stat.success_upload_count, \
		STAT_ITEM_TOTAL_APPEND, g_storage_stat.total_append_count, \
		STAT_ITEM_SUCCESS_APPEND, g_storage_stat.success_append_count, \
		STAT_ITEM_TOTAL_MODIFY, g_storage_stat.total_modify_count, \
		STAT_ITEM_SUCCESS_MODIFY, g_storage_stat.success_modify_count, \
		STAT_ITEM_TOTAL_TRUNCATE, g_storage_stat.total_truncate_count, \
		STAT_ITEM_SUCCESS_TRUNCATE, g_storage_stat.success_truncate_count, \
		STAT_ITEM_TOTAL_DOWNLOAD, g_storage_stat.total_download_count, \
		STAT_ITEM_SUCCESS_DOWNLOAD, \
		g_storage_stat.success_download_count, \
		STAT_ITEM_TOTAL_SET_META, g_storage_stat.total_set_meta_count, \
		STAT_ITEM_SUCCESS_SET_META, \
		g_storage_stat.success_set_meta_count, \
		STAT_ITEM_TOTAL_DELETE, g_storage_stat.total_delete_count, \
		STAT_ITEM_SUCCESS_DELETE, g_storage_stat.success_delete_count, \
		STAT_ITEM_TOTAL_GET_META, g_storage_stat.total_get_meta_count, \
		STAT_ITEM_SUCCESS_GET_META, \
		g_storage_stat.success_get_meta_count,  \
		STAT_ITEM_TOTAL_CREATE_LINK, \
		g_storage_stat.total_create_link_count,  \
		STAT_ITEM_SUCCESS_CREATE_LINK, \
		g_storage_stat.success_create_link_count,  \
		STAT_ITEM_TOTAL_DELETE_LINK, \
		g_storage_stat.total_delete_link_count,  \
		STAT_ITEM_SUCCESS_DELETE_LINK, \
		g_storage_stat.success_delete_link_count,  \
		STAT_ITEM_TOTAL_UPLOAD_BYTES, \
		g_storage_stat.total_upload_bytes,
		STAT_ITEM_SUCCESS_UPLOAD_BYTES, \
		g_storage_stat.success_upload_bytes, \
		STAT_ITEM_TOTAL_APPEND_BYTES, \
		g_storage_stat.total_append_bytes,
		STAT_ITEM_SUCCESS_APPEND_BYTES, \
		g_storage_stat.success_append_bytes, \
		STAT_ITEM_TOTAL_MODIFY_BYTES, \
		g_storage_stat.total_modify_bytes,
		STAT_ITEM_SUCCESS_MODIFY_BYTES, \
		g_storage_stat.success_modify_bytes, \
		STAT_ITEM_TOTAL_DOWNLOAD_BYTES, \
		g_storage_stat.total_download_bytes, \
		STAT_ITEM_SUCCESS_DOWNLOAD_BYTES, \
		g_storage_stat.success_download_bytes, \
		STAT_ITEM_TOTAL_SYNC_IN_BYTES, \
		g_storage_stat.total_sync_in_bytes, \
		STAT_ITEM_SUCCESS_SYNC_IN_BYTES, \
		g_storage_stat.success_sync_in_bytes, \
		STAT_ITEM_TOTAL_SYNC_OUT_BYTES, \
		g_storage_stat.total_sync_out_bytes, \
		STAT_ITEM_SUCCESS_SYNC_OUT_BYTES, \
		g_storage_stat.success_sync_out_bytes, \
		STAT_ITEM_TOTAL_FILE_OPEN_COUNT, \
		g_storage_stat.total_file_open_count, \
		STAT_ITEM_SUCCESS_FILE_OPEN_COUNT, \
		g_storage_stat.success_file_open_count, \
		STAT_ITEM_TOTAL_FILE_READ_COUNT, \
		g_storage_stat.total_file_read_count, \
		STAT_ITEM_SUCCESS_FILE_READ_COUNT, \
		g_storage_stat.success_file_read_count, \
		STAT_ITEM_TOTAL_FILE_WRITE_COUNT, \
		g_storage_stat.total_file_write_count, \
		STAT_ITEM_SUCCESS_FILE_WRITE_COUNT, \
		g_storage_stat.success_file_write_count, \
		STAT_ITEM_LAST_SOURCE_UPD, \
		(int)g_storage_stat.last_source_update, \
		STAT_ITEM_LAST_SYNC_UPD, (int)g_storage_stat.last_sync_update,\
		STAT_ITEM_DIST_PATH_INDEX_HIGH, g_dist_path_index_high, \
		STAT_ITEM_DIST_PATH_INDEX_LOW, g_dist_path_index_low, \
		STAT_ITEM_DIST_WRITE_FILE_COUNT, g_dist_write_file_count
	    );

        if ((result=pthread_mutex_lock(&sync_stat_file_lock)) != 0)
        {
                logError("file: "__FILE__", line: %d, " \
                        "call pthread_mutex_lock fail, " \
                        "errno: %d, error info: %s", \
                        __LINE__, result, STRERROR(result));
        }

	write_ret = storage_write_to_fd(storage_stat_fd, \
			get_storage_stat_filename, NULL, buff, len);

        if ((result=pthread_mutex_unlock(&sync_stat_file_lock)) != 0)
        {
                logError("file: "__FILE__", line: %d, " \
                        "call pthread_mutex_unlock fail, " \
                        "errno: %d, error info: %s", \
                        __LINE__, result, STRERROR(result));
        }

	return write_ret;
}

int storage_write_to_sync_ini_file()
{
	char full_filename[MAX_PATH_SIZE];
	char buff[512];
	int fd;
	int len;

	snprintf(full_filename, sizeof(full_filename), \
		"%s/data/%s", g_fdfs_base_path, DATA_DIR_INITED_FILENAME);
	if ((fd=open(full_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"open file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	len = sprintf(buff, "%s=%d\n" \
		"%s=%d\n"  \
		"%s=%s\n"  \
		"%s=%d\n"  \
		"%s=%s\n"  \
		"%s=%d\n"  \
		"%s=%d\n"  \
		"%s=%d\n"  \
		"%s=%d\n", \
		INIT_ITEM_STORAGE_JOIN_TIME, g_storage_join_time, \
		INIT_ITEM_SYNC_OLD_DONE, g_sync_old_done, \
		INIT_ITEM_SYNC_SRC_SERVER, g_sync_src_id, \
		INIT_ITEM_SYNC_UNTIL_TIMESTAMP, g_sync_until_timestamp, \
		INIT_ITEM_LAST_IP_ADDRESS, g_tracker_client_ip, \
		INIT_ITEM_LAST_SERVER_PORT, g_last_server_port, \
		INIT_ITEM_LAST_HTTP_PORT, g_last_http_port,
		INIT_ITEM_CURRENT_TRUNK_FILE_ID, g_current_trunk_file_id, \
		INIT_ITEM_TRUNK_LAST_COMPRESS_TIME, (int)g_trunk_last_compress_time
	    );
	if (fc_safe_write(fd, buff, len) != len)
	{
		logError("file: "__FILE__", line: %d, " \
			"write to file \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, full_filename, \
			errno, STRERROR(errno));
		close(fd);
		return errno != 0 ? errno : EIO;
	}

	close(fd);

	STORAGE_CHOWN(full_filename, geteuid(), getegid())

	return 0;
}

static int storage_check_and_make_data_dirs()
{
	int result;
	int i;
	char data_path[MAX_PATH_SIZE];
	char full_filename[MAX_PATH_SIZE];
	bool pathCreated;

	snprintf(data_path, sizeof(data_path), "%s/data", \
			g_fdfs_base_path);
	snprintf(full_filename, sizeof(full_filename), "%s/%s", \
			data_path, DATA_DIR_INITED_FILENAME);
	if (fileExists(full_filename))
	{
		IniContext iniContext;
		char *pValue;

		if ((result=iniLoadFromFile(full_filename, &iniContext)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"load from file \"%s/%s\" fail, " \
				"error code: %d", \
				__LINE__, data_path, \
				full_filename, result);
			return result;
		}
		
		pValue = iniGetStrValue(NULL, INIT_ITEM_STORAGE_JOIN_TIME, \
				&iniContext);
		if (pValue == NULL)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, " \
				"in file \"%s/%s\", item \"%s\" not exists", \
				__LINE__, data_path, full_filename, \
				INIT_ITEM_STORAGE_JOIN_TIME);
			return ENOENT;
		}
		g_storage_join_time = atoi(pValue);

		pValue = iniGetStrValue(NULL, INIT_ITEM_SYNC_OLD_DONE, \
				&iniContext);
		if (pValue == NULL)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, " \
				"in file \"%s/%s\", item \"%s\" not exists", \
				__LINE__, data_path, full_filename, \
				INIT_ITEM_SYNC_OLD_DONE);
			return ENOENT;
		}
		g_sync_old_done = atoi(pValue);

		pValue = iniGetStrValue(NULL, INIT_ITEM_SYNC_SRC_SERVER, \
				&iniContext);
		if (pValue == NULL)
		{
			iniFreeContext(&iniContext);
			logError("file: "__FILE__", line: %d, " \
				"in file \"%s/%s\", item \"%s\" not exists", \
				__LINE__, data_path, full_filename, \
				INIT_ITEM_SYNC_SRC_SERVER);
			return ENOENT;
		}
		snprintf(g_sync_src_id, sizeof(g_sync_src_id), \
				"%s", pValue);

		g_sync_until_timestamp = iniGetIntValue(NULL, \
				INIT_ITEM_SYNC_UNTIL_TIMESTAMP, \
				&iniContext, 0);

		pValue = iniGetStrValue(NULL, INIT_ITEM_LAST_IP_ADDRESS, \
				&iniContext);
		if (pValue != NULL)
		{
			snprintf(g_last_storage_ip, sizeof(g_last_storage_ip), \
				"%s", pValue);
		}

		pValue = iniGetStrValue(NULL, INIT_ITEM_LAST_SERVER_PORT, \
				&iniContext);
		if (pValue != NULL)
		{
			g_last_server_port = atoi(pValue);
		}

		pValue = iniGetStrValue(NULL, INIT_ITEM_LAST_HTTP_PORT, \
				&iniContext);
		if (pValue != NULL)
		{
			g_last_http_port = atoi(pValue);
		}

		g_current_trunk_file_id = iniGetIntValue(NULL, \
			INIT_ITEM_CURRENT_TRUNK_FILE_ID, &iniContext, 0);
		g_trunk_last_compress_time = iniGetIntValue(NULL, \
			INIT_ITEM_TRUNK_LAST_COMPRESS_TIME , &iniContext, 0);

		iniFreeContext(&iniContext);

		if (g_last_server_port == 0 || g_last_http_port == 0)
		{
			if (g_last_server_port == 0)
			{
				g_last_server_port = g_server_port;
			}

			if (g_last_http_port == 0)
			{
				g_last_http_port = g_http_port;
			}

			if ((result=storage_write_to_sync_ini_file()) != 0)
			{
				return result;
			}
		}

		/*
		logInfo("g_sync_old_done = %d, "
			"g_sync_src_id = %s, "
			"g_sync_until_timestamp = %d, "
			"g_last_storage_ip = %s, "
			"g_last_server_port = %d, "
			"g_last_http_port = %d, "
			"g_current_trunk_file_id = %d, "
			"g_trunk_last_compress_time = %d",
			g_sync_old_done, g_sync_src_id, g_sync_until_timestamp,
			g_last_storage_ip, g_last_server_port, g_last_http_port,
			g_current_trunk_file_id, (int)g_trunk_last_compress_time
			);
		*/
	}
	else
	{
		if (!fileExists(data_path))
		{
			if (mkdir(data_path, 0755) != 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"mkdir \"%s\" fail, " \
					"errno: %d, error info: %s", \
					__LINE__, data_path, \
					errno, STRERROR(errno));
				return errno != 0 ? errno : EPERM;
			}

			STORAGE_CHOWN(data_path, geteuid(), getegid())
		}

		g_last_server_port = g_server_port;
		g_last_http_port = g_http_port;
		g_storage_join_time = g_current_time;
		if ((result=storage_write_to_sync_ini_file()) != 0)
		{
			return result;
		}
	}

	for (i=0; i<g_fdfs_store_paths.count; i++)
	{
		if ((result=storage_make_data_dirs(g_fdfs_store_paths.paths[i], \
				&pathCreated)) != 0)
		{
			return result;
		}

		if (g_sync_old_done && pathCreated)  //repair damaged disk
		{
			if ((result=storage_disk_recovery_start(i)) != 0)
			{
				return result;
			}
		}

		result = storage_disk_recovery_restore(g_fdfs_store_paths.paths[i]);
		if (result == EAGAIN) //need to re-fetch binlog
		{
			if ((result=storage_disk_recovery_start(i)) != 0)
			{
				return result;
			}

			result=storage_disk_recovery_restore(g_fdfs_store_paths.paths[i]);
		}

		if (result != 0)
		{
			return result;
		}
	}

	return 0;
}

static int storage_make_data_dirs(const char *pBasePath, bool *pathCreated)
{
	char data_path[MAX_PATH_SIZE];
	char dir_name[9];
	char sub_name[9];
	char min_sub_path[16];
	char max_sub_path[16];
	int i, k;
	uid_t current_uid;
	gid_t current_gid;

	current_uid = geteuid();
	current_gid = getegid();

	*pathCreated = false;
	snprintf(data_path, sizeof(data_path), "%s/data", pBasePath);
	if (!fileExists(data_path))
	{
		if (mkdir(data_path, 0755) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"mkdir \"%s\" fail, " \
				"errno: %d, error info: %s", \
				__LINE__, data_path, errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}

		STORAGE_CHOWN(data_path, current_uid, current_gid)
	}

	if (chdir(data_path) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"chdir \"%s\" fail, " \
			"errno: %d, error info: %s", \
			__LINE__, data_path, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	sprintf(min_sub_path, FDFS_STORAGE_DATA_DIR_FORMAT"/"FDFS_STORAGE_DATA_DIR_FORMAT,
			0, 0);
	sprintf(max_sub_path, FDFS_STORAGE_DATA_DIR_FORMAT"/"FDFS_STORAGE_DATA_DIR_FORMAT,
			g_subdir_count_per_path-1, g_subdir_count_per_path-1);
	if (fileExists(min_sub_path) && fileExists(max_sub_path))
	{
		return 0;
	}

	fprintf(stderr, "data path: %s, mkdir sub dir...\n", data_path);
	for (i=0; i<g_subdir_count_per_path; i++)
	{
		sprintf(dir_name, FDFS_STORAGE_DATA_DIR_FORMAT, i);

		fprintf(stderr, "mkdir data path: %s ...\n", dir_name);
		if (mkdir(dir_name, 0755) != 0)
		{
			if (!(errno == EEXIST && isDir(dir_name)))
			{
				logError("file: "__FILE__", line: %d, " \
					"mkdir \"%s/%s\" fail, " \
					"errno: %d, error info: %s", \
					__LINE__, data_path, dir_name, \
					errno, STRERROR(errno));
				return errno != 0 ? errno : ENOENT;
			}
		}

		STORAGE_CHOWN(dir_name, current_uid, current_gid)

		if (chdir(dir_name) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"chdir \"%s/%s\" fail, " \
				"errno: %d, error info: %s", \
				__LINE__, data_path, dir_name, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}

		for (k=0; k<g_subdir_count_per_path; k++)
		{
			sprintf(sub_name, FDFS_STORAGE_DATA_DIR_FORMAT, k);
			if (mkdir(sub_name, 0755) != 0)
			{
				if (!(errno == EEXIST && isDir(sub_name)))
				{
					logError("file: "__FILE__", line: %d," \
						" mkdir \"%s/%s/%s\" fail, " \
						"errno: %d, error info: %s", \
						__LINE__, data_path, \
						dir_name, sub_name, \
						errno, STRERROR(errno));
					return errno != 0 ? errno : ENOENT;
				}
			}

			STORAGE_CHOWN(sub_name, current_uid, current_gid)
		}

		if (chdir("..") != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"chdir \"%s\" fail, " \
				"errno: %d, error info: %s", \
				__LINE__, data_path, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}
	}

	fprintf(stderr, "data path: %s, mkdir sub dir done.\n", data_path);

	*pathCreated = true;
	return 0;
}

/*
static int init_fsync_pthread_cond()
{
	int result;
	pthread_condattr_t thread_condattr;
	if ((result=pthread_condattr_init(&thread_condattr)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"pthread_condattr_init failed, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	if ((result=pthread_cond_init(&fsync_thread_cond, &thread_condattr)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"pthread_cond_init failed, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	pthread_condattr_destroy(&thread_condattr);
	return 0;
}
*/

static int storage_load_paths(IniContext *pItemContext)
{
	int result;
	int bytes;

	result = storage_load_paths_from_conf_file(pItemContext);
	if (result != 0)
	{
		return result;
	}

	bytes = sizeof(FDFSStorePathInfo) * g_fdfs_store_paths.count;
	g_path_space_list = (FDFSStorePathInfo *)malloc(bytes);
	if (g_path_space_list == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, errno: %d, error info: %s", \
			__LINE__, bytes, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	memset(g_path_space_list, 0, bytes);
	return 0;
}

void storage_set_access_log_header(struct log_context *pContext)
{
#define STORAGE_ACCESS_HEADER_STR "client_ip action filename status time_used_ms req_len resp_len"
#define STORAGE_ACCESS_HEADER_LEN (sizeof(STORAGE_ACCESS_HEADER_STR) - 1)

    log_header(pContext, STORAGE_ACCESS_HEADER_STR, STORAGE_ACCESS_HEADER_LEN);
}

int storage_func_init(const char *filename, \
		char *bind_addr, const int addr_size)
{
	char *pBindAddr;
	char *pGroupName;
	char *pRunByGroup;
	char *pRunByUser;
	char *pFsyncAfterWrittenBytes;
	char *pThreadStackSize;
	char *pBuffSize;
	char *pIfAliasPrefix;
	char *pHttpDomain;
	char *pRotateAccessLogSize;
	char *pRotateErrorLogSize;
	IniContext iniContext;
	int result;
	int64_t fsync_after_written_bytes;
	int64_t thread_stack_size;
	int64_t buff_size;
	int64_t rotate_access_log_size;
	int64_t rotate_error_log_size;
	ConnectionInfo *pServer;
	ConnectionInfo *pEnd;

	/*
	while (nThreadCount > 0)
	{
		sleep(1);
	}
	*/

	if ((result=iniLoadFromFile(filename, &iniContext)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"load conf file \"%s\" fail, ret code: %d", \
			__LINE__, filename, result);
		return result;
	}

	do
	{
		if (iniGetBoolValue(NULL, "disabled", &iniContext, false))
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file \"%s\" disabled=true, exit", \
				__LINE__, filename);
			result = ECANCELED;
			break;
		}

		g_subdir_count_per_path=iniGetIntValue(NULL, \
				"subdir_count_per_path", &iniContext, \
				DEFAULT_DATA_DIR_COUNT_PER_PATH);
		if (g_subdir_count_per_path <= 0 || \
		    g_subdir_count_per_path > 256)
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file \"%s\", invalid subdir_count: %d", \
				__LINE__, filename, g_subdir_count_per_path);
			result = EINVAL;
			break;
		}

		if ((result=storage_load_paths(&iniContext)) != 0)
		{
			break;
		}

		load_log_level(&iniContext);
		if ((result=log_set_prefix(g_fdfs_base_path, \
				STORAGE_ERROR_LOG_FILENAME)) != 0)
		{
			break;
		}

		g_fdfs_connect_timeout = iniGetIntValue(NULL, "connect_timeout", \
				&iniContext, DEFAULT_CONNECT_TIMEOUT);
		if (g_fdfs_connect_timeout <= 0)
		{
			g_fdfs_connect_timeout = DEFAULT_CONNECT_TIMEOUT;
		}

		g_fdfs_network_timeout = iniGetIntValue(NULL, "network_timeout", \
				&iniContext, DEFAULT_NETWORK_TIMEOUT);
		if (g_fdfs_network_timeout <= 0)
		{
			g_fdfs_network_timeout = DEFAULT_NETWORK_TIMEOUT;
		}

		g_server_port = iniGetIntValue(NULL, "port", &iniContext, \
					FDFS_STORAGE_SERVER_DEF_PORT);
		if (g_server_port <= 0)
		{
			g_server_port = FDFS_STORAGE_SERVER_DEF_PORT;
		}

		g_heart_beat_interval = iniGetIntValue(NULL, \
				"heart_beat_interval", &iniContext, \
				STORAGE_BEAT_DEF_INTERVAL);
		if (g_heart_beat_interval <= 0)
		{
			g_heart_beat_interval = STORAGE_BEAT_DEF_INTERVAL;
		}

		g_stat_report_interval = iniGetIntValue(NULL, \
				"stat_report_interval", &iniContext, \
				STORAGE_REPORT_DEF_INTERVAL);
		if (g_stat_report_interval <= 0)
		{
			g_stat_report_interval = STORAGE_REPORT_DEF_INTERVAL;
		}

		pBindAddr = iniGetStrValue(NULL, "bind_addr", &iniContext);
		if (pBindAddr == NULL)
		{
			*bind_addr = '\0';
		}
		else
		{
			snprintf(bind_addr, addr_size, "%s", pBindAddr);
		}

		g_client_bind_addr = iniGetBoolValue(NULL, "client_bind", \
					&iniContext, true);

		result = fdfs_load_tracker_group_ex(&g_tracker_group, \
				filename, &iniContext);
		if (result != 0)
		{
			break;
		}

		pEnd = g_tracker_group.servers + g_tracker_group.server_count;
		for (pServer=g_tracker_group.servers; pServer<pEnd; pServer++)
		{
			//printf("server=%s:%d\n", pServer->ip_addr, pServer->port);
			if (strcmp(pServer->ip_addr, "127.0.0.1") == 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"conf file \"%s\", " \
					"tracker: \"%s:%d\" is invalid, " \
					"tracker server ip can't be 127.0.0.1",\
					__LINE__, filename, pServer->ip_addr, \
					pServer->port);
				result = EINVAL;
				break;
			}
		}
		if (result != 0)
		{
			break;
		}

		pGroupName = iniGetStrValue(NULL, "group_name", &iniContext);
		if (pGroupName == NULL)
		{
            result = storage_get_group_name_from_tracker();
            if (result == 0)
            {
                logInfo("file: "__FILE__", line: %d, " \
                        "get group name from tracker server, group_name: %s",
                        __LINE__, g_group_name);
            }
            else
            {
                logError("file: "__FILE__", line: %d, " \
                        "conf file \"%s\" must have item " \
                        "\"group_name\"!", \
                        __LINE__, filename);
                result = ENOENT;
                break;
            }
		}
        else if (pGroupName[0] == '\0')
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file \"%s\", " \
				"group_name is empty!", \
				__LINE__, filename);
			result = EINVAL;
			break;
		}
        else
        {
		    snprintf(g_group_name, sizeof(g_group_name), "%s", pGroupName);
        }

		if ((result=fdfs_validate_group_name(g_group_name)) != 0) \
		{
			logError("file: "__FILE__", line: %d, " \
				"conf file \"%s\", " \
				"the group name \"%s\" is invalid!", \
				__LINE__, filename, g_group_name);
			result = EINVAL;
			break;
		}

		g_sync_wait_usec = iniGetIntValue(NULL, "sync_wait_msec",\
			 &iniContext, STORAGE_DEF_SYNC_WAIT_MSEC);
		if (g_sync_wait_usec <= 0)
		{
			g_sync_wait_usec = STORAGE_DEF_SYNC_WAIT_MSEC;
		}
		g_sync_wait_usec *= 1000;

		g_sync_interval = iniGetIntValue(NULL, "sync_interval",\
			 &iniContext, 0);
		if (g_sync_interval < 0)
		{
			g_sync_interval = 0;
		}
		g_sync_interval *= 1000;

		if ((result=get_time_item_from_conf(&iniContext, \
			"sync_start_time", &g_sync_start_time, 0, 0)) != 0)
		{
			break;
		}
		if ((result=get_time_item_from_conf(&iniContext, \
			"sync_end_time", &g_sync_end_time, 23, 59)) != 0)
		{
			break;
		}

		g_sync_part_time = !((g_sync_start_time.hour == 0 && \
				g_sync_start_time.minute == 0) && \
				(g_sync_end_time.hour == 23 && \
				g_sync_end_time.minute == 59));

		g_max_connections = iniGetIntValue(NULL, "max_connections", \
				&iniContext, DEFAULT_MAX_CONNECTONS);
		if (g_max_connections <= 0)
		{
			g_max_connections = DEFAULT_MAX_CONNECTONS;
		}
		if ((result=set_rlimit(RLIMIT_NOFILE, g_max_connections)) != 0)
		{
			break;
		}

		g_accept_threads = iniGetIntValue(NULL, "accept_threads", \
				&iniContext, 1);
		if (g_accept_threads <= 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"accept_threads\" is invalid, " \
				"value: %d <= 0!", __LINE__, g_accept_threads);
			result = EINVAL;
                        break;
		}

		g_work_threads = iniGetIntValue(NULL, "work_threads", \
				&iniContext, DEFAULT_WORK_THREADS);
		if (g_work_threads <= 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"work_threads\" is invalid, " \
				"value: %d <= 0!", __LINE__, g_work_threads);
			result = EINVAL;
                        break;
		}

		pBuffSize = iniGetStrValue(NULL, \
			"buff_size", &iniContext);
		if (pBuffSize == NULL)
		{
			buff_size = STORAGE_DEFAULT_BUFF_SIZE;
		}
		else if ((result=parse_bytes(pBuffSize, 1, &buff_size)) != 0)
		{
			break;
		}
		g_buff_size = buff_size;
		if (g_buff_size < 4 * 1024 || \
			g_buff_size < sizeof(TrackerHeader) + \
					TRUNK_BINLOG_BUFFER_SIZE)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"buff_size\" is too small, " \
				"value: %d < %d or < %d!", __LINE__, \
				g_buff_size, 4 * 1024, \
				(int)sizeof(TrackerHeader) + \
				TRUNK_BINLOG_BUFFER_SIZE);
			result = EINVAL;
                        break;
		}

		g_disk_rw_separated = iniGetBoolValue(NULL, \
				"disk_rw_separated", &iniContext, true);

		g_disk_reader_threads = iniGetIntValue(NULL, \
				"disk_reader_threads", \
				&iniContext, DEFAULT_DISK_READER_THREADS);
		if (g_disk_reader_threads < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"disk_reader_threads\" is invalid, " \
				"value: %d < 0!", __LINE__, \
				g_disk_reader_threads);
			result = EINVAL;
                        break;
		}

		g_disk_writer_threads = iniGetIntValue(NULL, \
				"disk_writer_threads", \
				&iniContext, DEFAULT_DISK_WRITER_THREADS);
		if (g_disk_writer_threads < 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"disk_writer_threads\" is invalid, " \
				"value: %d < 0!", __LINE__, \
				g_disk_writer_threads);
			result = EINVAL;
                        break;
		}

		if (g_disk_rw_separated)
		{
			if (g_disk_reader_threads == 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"item \"disk_reader_threads\" is " \
					"invalid, value = 0!", __LINE__);
				result = EINVAL;
				break;
			}

			if (g_disk_writer_threads == 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"item \"disk_writer_threads\" is " \
					"invalid, value = 0!", __LINE__);
				result = EINVAL;
				break;
			}
		}
		else if (g_disk_reader_threads + g_disk_writer_threads == 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"disk_reader_threads\" and " \
				"\"disk_writer_threads\" are " \
				"invalid, both value = 0!", __LINE__);
			result = EINVAL;
			break;
		}

		/*
		g_disk_rw_direct = iniGetBoolValue(NULL, \
				"disk_rw_direct", &iniContext, false);
		*/

		pRunByGroup = iniGetStrValue(NULL, "run_by_group", &iniContext);
		pRunByUser = iniGetStrValue(NULL, "run_by_user", &iniContext);
		if (pRunByGroup == NULL)
		{
			*g_run_by_group = '\0';
		}
		else
		{
			snprintf(g_run_by_group, sizeof(g_run_by_group), \
				"%s", pRunByGroup);
		}
		if (*g_run_by_group == '\0')
		{
			g_run_by_gid = getegid();
		}
		else
		{
			struct group *pGroup;

     			pGroup = getgrnam(g_run_by_group);
			if (pGroup == NULL)
			{
				result = errno != 0 ? errno : ENOENT;
				logError("file: "__FILE__", line: %d, " \
					"getgrnam fail, errno: %d, " \
					"error info: %s", __LINE__, \
					result, STRERROR(result));
				return result;
			}

			g_run_by_gid = pGroup->gr_gid;
		}

		if (pRunByUser == NULL)
		{
			*g_run_by_user = '\0';
		}
		else
		{
			snprintf(g_run_by_user, sizeof(g_run_by_user), \
				"%s", pRunByUser);
		}
		if (*g_run_by_user == '\0')
		{
			g_run_by_uid = geteuid();
		}
		else
		{
			struct passwd *pUser;

     			pUser = getpwnam(g_run_by_user);
			if (pUser == NULL)
			{
				result = errno != 0 ? errno : ENOENT;
				logError("file: "__FILE__", line: %d, " \
					"getpwnam fail, errno: %d, " \
					"error info: %s", __LINE__, \
					result, STRERROR(result));
				return result;
			}

			g_run_by_uid = pUser->pw_uid;
		}

		if ((result=load_allow_hosts(&iniContext, \
                	 &g_allow_ip_addrs, &g_allow_ip_count)) != 0)
		{
			return result;
		}

		g_file_distribute_path_mode = iniGetIntValue(NULL, \
			"file_distribute_path_mode", &iniContext, \
			FDFS_FILE_DIST_PATH_ROUND_ROBIN);
		g_file_distribute_rotate_count = iniGetIntValue(NULL, \
			"file_distribute_rotate_count", &iniContext, \
			FDFS_FILE_DIST_DEFAULT_ROTATE_COUNT);
		if (g_file_distribute_rotate_count <= 0)
		{
			g_file_distribute_rotate_count = \
				FDFS_FILE_DIST_DEFAULT_ROTATE_COUNT;
		}

		pFsyncAfterWrittenBytes = iniGetStrValue(NULL, \
			"fsync_after_written_bytes", &iniContext);
		if (pFsyncAfterWrittenBytes == NULL)
		{
			fsync_after_written_bytes = 0;
		}
		else if ((result=parse_bytes(pFsyncAfterWrittenBytes, 1, \
				&fsync_after_written_bytes)) != 0)
		{
			break;
		}
		g_fsync_after_written_bytes = fsync_after_written_bytes;

		g_sync_log_buff_interval = iniGetIntValue(NULL, \
				"sync_log_buff_interval", &iniContext, \
				SYNC_LOG_BUFF_DEF_INTERVAL);
		if (g_sync_log_buff_interval <= 0)
		{
			g_sync_log_buff_interval = SYNC_LOG_BUFF_DEF_INTERVAL;
		}

		g_sync_binlog_buff_interval = iniGetIntValue(NULL, \
				"sync_binlog_buff_interval", &iniContext,\
				SYNC_BINLOG_BUFF_DEF_INTERVAL);
		if (g_sync_binlog_buff_interval <= 0)
		{
			g_sync_binlog_buff_interval=SYNC_BINLOG_BUFF_DEF_INTERVAL;
		}

		g_write_mark_file_freq = iniGetIntValue(NULL, \
				"write_mark_file_freq", &iniContext, \
				FDFS_DEFAULT_SYNC_MARK_FILE_FREQ);
		if (g_write_mark_file_freq <= 0)
		{
			g_write_mark_file_freq = FDFS_DEFAULT_SYNC_MARK_FILE_FREQ;
		}


		g_sync_stat_file_interval = iniGetIntValue(NULL, \
				"sync_stat_file_interval", &iniContext, \
				DEFAULT_SYNC_STAT_FILE_INTERVAL);
		if (g_sync_stat_file_interval <= 0)
		{
			g_sync_stat_file_interval=DEFAULT_SYNC_STAT_FILE_INTERVAL;
		}

		pThreadStackSize = iniGetStrValue(NULL, \
			"thread_stack_size", &iniContext);
		if (pThreadStackSize == NULL)
		{
			thread_stack_size = 512 * 1024;
		}
		else if ((result=parse_bytes(pThreadStackSize, 1, \
				&thread_stack_size)) != 0)
		{
			break;
		}
		g_thread_stack_size = (int)thread_stack_size;

		if (g_thread_stack_size < FAST_WRITE_BUFF_SIZE + 64 * 1024)
		{
			logError("file: "__FILE__", line: %d, " \
				"item \"thread_stack_size\" %d is invalid, " \
				"which < %d", __LINE__, g_thread_stack_size, \
				FAST_WRITE_BUFF_SIZE + 64 * 1024);
			result = EINVAL;
			break;
		}

		g_upload_priority = iniGetIntValue(NULL, \
				"upload_priority", &iniContext, \
				DEFAULT_UPLOAD_PRIORITY);

		pIfAliasPrefix = iniGetStrValue(NULL, \
			"if_alias_prefix", &iniContext);
		if (pIfAliasPrefix == NULL)
		{
			*g_if_alias_prefix = '\0';
		}
		else
		{
			snprintf(g_if_alias_prefix, sizeof(g_if_alias_prefix), 
				"%s", pIfAliasPrefix);
		}

		g_check_file_duplicate = iniGetBoolValue(NULL, \
				"check_file_duplicate", &iniContext, false);
		if (g_check_file_duplicate)
		{
			char *pKeyNamespace;
			char *pFileSignatureMethod;

			pFileSignatureMethod = iniGetStrValue(NULL, \
				"file_signature_method", &iniContext);
			if (pFileSignatureMethod != NULL && strcasecmp( \
				pFileSignatureMethod, "md5") == 0)
			{
				g_file_signature_method = \
					STORAGE_FILE_SIGNATURE_METHOD_MD5;
			}
			else
			{
				g_file_signature_method = \
					STORAGE_FILE_SIGNATURE_METHOD_HASH;
			}

			strcpy(g_fdht_base_path, g_fdfs_base_path);
			g_fdht_connect_timeout = g_fdfs_connect_timeout;
			g_fdht_network_timeout = g_fdfs_network_timeout;

			pKeyNamespace = iniGetStrValue(NULL, \
				"key_namespace", &iniContext);
			if (pKeyNamespace == NULL || *pKeyNamespace == '\0')
			{
				logError("file: "__FILE__", line: %d, " \
					"item \"key_namespace\" does not " \
					"exist or is empty", __LINE__);
				result = EINVAL;
				break;
			}

			g_namespace_len = strlen(pKeyNamespace);
			if (g_namespace_len >= sizeof(g_key_namespace))
			{
				g_namespace_len = sizeof(g_key_namespace) - 1;
			}
			memcpy(g_key_namespace, pKeyNamespace, g_namespace_len);
			*(g_key_namespace + g_namespace_len) = '\0';

			if ((result=fdht_load_groups(&iniContext, \
					&g_group_array)) != 0)
			{
				break;
			}

			g_keep_alive = iniGetBoolValue(NULL, "keep_alive", \
					&iniContext, false);
		}

		g_http_port = iniGetIntValue(NULL, "http.server_port", \
                                        &iniContext, 80);
		if (g_http_port <= 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"invalid param \"http.server_port\": %d", \
				__LINE__, g_http_port);
			result = EINVAL;
			break;
		}
 
		pHttpDomain = iniGetStrValue(NULL, \
			"http.domain_name", &iniContext);
		if (pHttpDomain == NULL)
		{
			*g_http_domain = '\0';
		}
		else
		{
			snprintf(g_http_domain, sizeof(g_http_domain), \
				"%s", pHttpDomain);
		}

		g_use_access_log = iniGetBoolValue(NULL, "use_access_log", \
					&iniContext, false);
		if (g_use_access_log)
		{
			result = log_init_ex(&g_access_log_context);
			if (result != 0)
			{
				break;
			}

			log_set_time_precision(&g_access_log_context, \
				LOG_TIME_PRECISION_MSECOND);
			log_set_cache_ex(&g_access_log_context, true);
			result = log_set_prefix_ex(&g_access_log_context, \
				g_fdfs_base_path, "storage_access");
			if (result != 0)
			{
				break;
			}
            log_set_header_callback(&g_access_log_context, storage_set_access_log_header);
		}
	
		g_rotate_access_log = iniGetBoolValue(NULL, "rotate_access_log",\
					&iniContext, false);
		if ((result=get_time_item_from_conf(&iniContext, \
			"access_log_rotate_time", &g_access_log_rotate_time, \
			0, 0)) != 0)
		{
			break;
		}

		g_rotate_error_log = iniGetBoolValue(NULL, "rotate_error_log",\
					&iniContext, false);
		if ((result=get_time_item_from_conf(&iniContext, \
			"error_log_rotate_time", &g_error_log_rotate_time, \
			0, 0)) != 0)
		{
			break;
		}

		pRotateAccessLogSize = iniGetStrValue(NULL, \
			"rotate_access_log_size", &iniContext);
		if (pRotateAccessLogSize == NULL)
		{
			rotate_access_log_size = 0;
		}
		else if ((result=parse_bytes(pRotateAccessLogSize, 1, \
				&rotate_access_log_size)) != 0)
		{
			break;
		}
		if (rotate_access_log_size > 0 && \
			rotate_access_log_size < FDFS_ONE_MB)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"item \"rotate_access_log_size\": " \
				"%"PRId64" is too small, " \
				"change to 1 MB", __LINE__, \
				rotate_access_log_size);
			rotate_access_log_size = FDFS_ONE_MB;
		}
		fdfs_set_log_rotate_size(&g_access_log_context, rotate_access_log_size);

		pRotateErrorLogSize = iniGetStrValue(NULL, \
			"rotate_error_log_size", &iniContext);
		if (pRotateErrorLogSize == NULL)
		{
			rotate_error_log_size = 0;
		}
		else if ((result=parse_bytes(pRotateErrorLogSize, 1, \
				&rotate_error_log_size)) != 0)
		{
			break;
		}
		if (rotate_error_log_size > 0 && \
			rotate_error_log_size < FDFS_ONE_MB)
		{
			logWarning("file: "__FILE__", line: %d, " \
				"item \"rotate_error_log_size\": " \
				"%"PRId64" is too small, " \
				"change to 1 MB", __LINE__, \
				rotate_error_log_size);
			rotate_error_log_size = FDFS_ONE_MB;
		}
		fdfs_set_log_rotate_size(&g_log_context, rotate_error_log_size);

		g_log_file_keep_days = iniGetIntValue(NULL, \
				"log_file_keep_days", &iniContext, 0);

		g_file_sync_skip_invalid_record = iniGetBoolValue(NULL, \
			"file_sync_skip_invalid_record", &iniContext, false);

		if ((result=fdfs_connection_pool_init(filename, &iniContext)) != 0)
		{
			break;
		}

#ifdef WITH_HTTPD
		{
		char *pHttpTrunkSize;
		int64_t http_trunk_size;

		if ((result=fdfs_http_params_load(&iniContext, \
				filename, &g_http_params)) != 0)
		{
			break;
		}

		pHttpTrunkSize = iniGetStrValue(NULL, \
			"http.trunk_size", &iniContext);
		if (pHttpTrunkSize == NULL)
		{
			http_trunk_size = 64 * 1024;
		}
		else if ((result=parse_bytes(pHttpTrunkSize, 1, \
				&http_trunk_size)) != 0)
		{
			break;
		}

		g_http_trunk_size = (int)http_trunk_size;
		}
#endif

		logInfo("FastDFS v%d.%02d, base_path=%s, store_path_count=%d, " \
			"subdir_count_per_path=%d, group_name=%s, " \
			"run_by_group=%s, run_by_user=%s, " \
			"connect_timeout=%ds, network_timeout=%ds, "\
			"port=%d, bind_addr=%s, client_bind=%d, " \
			"max_connections=%d, accept_threads=%d, " \
			"work_threads=%d, "    \
			"disk_rw_separated=%d, disk_reader_threads=%d, " \
			"disk_writer_threads=%d, " \
			"buff_size=%dKB, heart_beat_interval=%ds, " \
			"stat_report_interval=%ds, tracker_server_count=%d, " \
			"sync_wait_msec=%dms, sync_interval=%dms, " \
			"sync_start_time=%02d:%02d, sync_end_time=%02d:%02d, "\
			"write_mark_file_freq=%d, " \
			"allow_ip_count=%d, " \
			"file_distribute_path_mode=%d, " \
			"file_distribute_rotate_count=%d, " \
			"fsync_after_written_bytes=%d, " \
			"sync_log_buff_interval=%ds, " \
			"sync_binlog_buff_interval=%ds, " \
			"sync_stat_file_interval=%ds, " \
			"thread_stack_size=%d KB, upload_priority=%d, " \
			"if_alias_prefix=%s, " \
			"check_file_duplicate=%d, file_signature_method=%s, " \
			"FDHT group count=%d, FDHT server count=%d, " \
			"FDHT key_namespace=%s, FDHT keep_alive=%d, " \
			"HTTP server port=%d, domain name=%s, " \
			"use_access_log=%d, rotate_access_log=%d, " \
			"access_log_rotate_time=%02d:%02d, " \
			"rotate_error_log=%d, " \
			"error_log_rotate_time=%02d:%02d, " \
			"rotate_access_log_size=%"PRId64", " \
			"rotate_error_log_size=%"PRId64", " \
			"log_file_keep_days=%d, " \
			"file_sync_skip_invalid_record=%d, " \
			"use_connection_pool=%d, " \
			"g_connection_pool_max_idle_time=%ds", \
			g_fdfs_version.major, g_fdfs_version.minor, \
			g_fdfs_base_path, g_fdfs_store_paths.count, \
			g_subdir_count_per_path, \
			g_group_name, g_run_by_group, g_run_by_user, \
			g_fdfs_connect_timeout, g_fdfs_network_timeout, \
			g_server_port, bind_addr, \
			g_client_bind_addr, g_max_connections, \
			g_accept_threads, g_work_threads, g_disk_rw_separated, \
			g_disk_reader_threads, g_disk_writer_threads, \
			g_buff_size / 1024, \
			g_heart_beat_interval, g_stat_report_interval, \
			g_tracker_group.server_count, g_sync_wait_usec / 1000, \
			g_sync_interval / 1000, \
			g_sync_start_time.hour, g_sync_start_time.minute, \
			g_sync_end_time.hour, g_sync_end_time.minute, \
			g_write_mark_file_freq, \
			g_allow_ip_count, g_file_distribute_path_mode, \
			g_file_distribute_rotate_count, \
			g_fsync_after_written_bytes, g_sync_log_buff_interval, \
			g_sync_binlog_buff_interval, g_sync_stat_file_interval, \
			g_thread_stack_size/1024, g_upload_priority, \
			g_if_alias_prefix, g_check_file_duplicate, \
			g_file_signature_method == STORAGE_FILE_SIGNATURE_METHOD_HASH \
				? "hash" : "md5", 
			g_group_array.group_count, g_group_array.server_count, \
			g_key_namespace, g_keep_alive, \
			g_http_port, g_http_domain, g_use_access_log, \
			g_rotate_access_log, g_access_log_rotate_time.hour, \
			g_access_log_rotate_time.minute, \
			g_rotate_error_log, g_error_log_rotate_time.hour, \
			g_error_log_rotate_time.minute, \
			g_access_log_context.rotate_size, \
			g_log_context.rotate_size, g_log_file_keep_days, \
			g_file_sync_skip_invalid_record, \
			g_use_connection_pool, g_connection_pool_max_idle_time);

#ifdef WITH_HTTPD
		if (!g_http_params.disabled)
		{
			logInfo("HTTP supported: " \
				"server_port=%d, " \
				"http_trunk_size=%d, " \
				"default_content_type=%s, " \
				"anti_steal_token=%d, " \
				"token_ttl=%ds, " \
				"anti_steal_secret_key length=%d, "  \
				"token_check_fail content_type=%s, " \
				"token_check_fail buff length=%d",  \
				g_http_params.server_port, \
				g_http_trunk_size, \
				g_http_params.default_content_type, \
				g_http_params.anti_steal_token, \
				g_http_params.token_ttl, \
				g_http_params.anti_steal_secret_key.length, \
				g_http_params.token_check_fail_content_type, \
				g_http_params.token_check_fail_buff.length);
		}
#endif

	} while (0);

	iniFreeContext(&iniContext);

	if (result != 0)
	{
		return result;
	}

	if ((result=storage_get_my_tracker_client_ip()) != 0)
	{
		return result;
	}

	if ((result=storage_check_and_make_data_dirs()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"storage_check_and_make_data_dirs fail, " \
			"program exit!", __LINE__);
		return result;
	}

	if ((result=storage_get_params_from_tracker()) != 0)
	{
		return result;
	}

	if ((result=tracker_get_my_server_id()) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"get my server id from tracker server fail, " \
			"errno: %d, error info: %s", __LINE__, \
			result, STRERROR(result));
		return result;
	}

	if (g_use_storage_id)
	{
		if ((result=fdfs_get_storage_ids_from_tracker_group( \
				&g_tracker_group)) != 0)
		{
			return result;
		}
	}

	if ((result=storage_check_ip_changed()) != 0)
	{
		return result;
	}

	if ((result=init_pthread_lock(&sync_stat_file_lock)) != 0)
	{
		return result;
	}

	return storage_open_stat_file();
}

int storage_func_destroy()
{
	int i;
	int result;
	int close_ret;

	if (g_fdfs_store_paths.paths != NULL)
	{
		for (i=0; i<g_fdfs_store_paths.count; i++)
		{
			if (g_fdfs_store_paths.paths[i] != NULL)
			{
				free(g_fdfs_store_paths.paths[i]);
				g_fdfs_store_paths.paths[i] = NULL;
			}
		}

		g_fdfs_store_paths.paths = NULL;
	}

	if (g_tracker_group.servers != NULL)
	{
		free(g_tracker_group.servers);
		g_tracker_group.servers = NULL;
		g_tracker_group.server_count = 0;
		g_tracker_group.server_index = 0;
	}

	close_ret = storage_close_stat_file();

	if (g_use_access_log)
	{
		log_destroy_ex(&g_access_log_context);
	}

	if ((result=pthread_mutex_destroy(&sync_stat_file_lock)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_destroy fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	return close_ret;
}

bool storage_server_is_myself(const FDFSStorageBrief *pStorageBrief)
{
	if (g_use_storage_id)
	{
		return strcmp(pStorageBrief->id, g_my_server_id_str) == 0;
	}
	else
	{
		return is_local_host_ip(pStorageBrief->ip_addr);
	}
}

bool storage_id_is_myself(const char *storage_id)
{
	if (g_use_storage_id)
	{
		return strcmp(storage_id, g_my_server_id_str) == 0;
	}
	else
	{
		return is_local_host_ip(storage_id);
	}
}

/*
int write_serialized(int fd, const char *buff, size_t count, const bool bSync)
{
	int result;
	int fsync_ret;

	if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	while (fsync_thread_count >= g_max_write_thread_count)
	{
		if ((result=pthread_cond_wait(&fsync_thread_cond, \
				&fsync_thread_mutex)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"pthread_cond_wait failed, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
			return result;
		}
	}

	fsync_thread_count++;

	if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if (fc_safe_write(fd, buff, count) == count)
	{
		if (bSync && fsync(fd) != 0)
		{
			fsync_ret = errno != 0 ? errno : EIO;
			logError("file: "__FILE__", line: %d, " \
				"call fsync fail, " \
				"errno: %d, error info: %s", \
				__LINE__, fsync_ret, STRERROR(fsync_ret));
		}
		else
		{
			fsync_ret = 0;
		}
	}
	else
	{
		fsync_ret = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"call write fail, " \
			"errno: %d, error info: %s", \
			__LINE__, fsync_ret, STRERROR(fsync_ret));
	}

	if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	fsync_thread_count--;

	if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if ((result=pthread_cond_signal(&fsync_thread_cond)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"pthread_cond_signal failed, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return fsync_ret;
}

int fsync_serialized(int fd)
{
	int result;
	int fsync_ret;

	if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	while (fsync_thread_count >= g_max_write_thread_count)
	{
		if ((result=pthread_cond_wait(&fsync_thread_cond, \
				&fsync_thread_mutex)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"pthread_cond_wait failed, " \
				"errno: %d, error info: %s", \
				__LINE__, result, STRERROR(result));
			return result;
		}
	}

	fsync_thread_count++;

	if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if (fsync(fd) == 0)
	{
		fsync_ret = 0;
	}
	else
	{
		fsync_ret = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"call fsync fail, " \
			"errno: %d, error info: %s", \
			__LINE__, fsync_ret, STRERROR(fsync_ret));
	}

	if ((result=pthread_mutex_lock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_lock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	fsync_thread_count--;

	if ((result=pthread_mutex_unlock(&fsync_thread_mutex)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"call pthread_mutex_unlock fail, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	if ((result=pthread_cond_signal(&fsync_thread_cond)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"pthread_cond_signal failed, " \
			"errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
	}

	return fsync_ret;
}

int recv_file_serialized(int sock, const char *filename, \
		const int64_t file_bytes)
{
	int fd;
	char buff[FAST_WRITE_BUFF_SIZE];
	int64_t remain_bytes;
	int recv_bytes;
	int result;

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
	{
		return errno != 0 ? errno : EACCES;
	}

	remain_bytes = file_bytes;
	while (remain_bytes > 0)
	{
		if (remain_bytes > sizeof(buff))
		{
			recv_bytes = sizeof(buff);
		}
		else
		{
			recv_bytes = remain_bytes;
		}

		if ((result=tcprecvdata_nb(sock, buff, recv_bytes, \
				g_fdfs_network_timeout)) != 0)
		{
			close(fd);
			unlink(filename);
			return result;
		}

		if (recv_bytes == remain_bytes)  //last buff
		{
			if (write_serialized(fd, buff, recv_bytes, true) != 0)
			{
				result = errno != 0 ? errno : EIO;
				close(fd);
				unlink(filename);
				return result;
			}
		}
		else
		{
			if (write_serialized(fd, buff, recv_bytes, false) != 0)
			{
				result = errno != 0 ? errno : EIO;
				close(fd);
				unlink(filename);
				return result;
			}

			if ((result=fsync_serialized(fd)) != 0)
			{
				close(fd);
				unlink(filename);
				return result;
			}
		}

		remain_bytes -= recv_bytes;
	}

	close(fd);
	return 0;
}
*/
