/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include "tracker_types.h"
#include "tracker_proto.h"
#include "client_func.h"
#include "tracker_client.h"
#include "storage_client.h"
#include "storage_client1.h"
#include "client_global.h"
#include "base64.h"

static struct base64_context the_base64_context;
static int the_base64_context_inited = 0;

#define FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id) \
	char new_file_id[FDFS_GROUP_NAME_MAX_LEN + 128]; \
	char *group_name; \
	char *filename; \
	char *pSeperator; \
	\
	snprintf(new_file_id, sizeof(new_file_id), "%s", file_id); \
	pSeperator = strchr(new_file_id, FDFS_FILE_ID_SEPERATOR); \
	if (pSeperator == NULL) \
	{ \
		return EINVAL; \
	} \
	\
	*pSeperator = '\0'; \
	group_name = new_file_id; \
	filename =  pSeperator + 1; \

#define storage_get_read_connection(pTrackerServer, \
		ppStorageServer, group_name, filename, \
		pNewStorage, new_connection) \
	storage_get_connection(pTrackerServer, \
		ppStorageServer, TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE, \
		group_name, filename, pNewStorage, new_connection)

#define storage_get_update_connection(pTrackerServer, \
		ppStorageServer, group_name, filename, \
		pNewStorage, new_connection) \
	storage_get_connection(pTrackerServer, \
		ppStorageServer, TRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE, \
		group_name, filename, pNewStorage, new_connection)

static int storage_get_connection(ConnectionInfo *pTrackerServer, \
		ConnectionInfo **ppStorageServer, const byte cmd, \
		const char *group_name, const char *filename, \
		ConnectionInfo *pNewStorage, bool *new_connection)
{
	int result;
	bool new_tracker_connection;
	ConnectionInfo *pNewTracker;
	if (*ppStorageServer == NULL)
	{
		CHECK_CONNECTION(pTrackerServer, pNewTracker, result, \
			new_tracker_connection);
		if (cmd == TRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE)
		{
			result = tracker_query_storage_fetch(pNewTracker, \
		                pNewStorage, group_name, filename);
		}
		else
		{
			result = tracker_query_storage_update(pNewTracker, \
		                pNewStorage, group_name, filename);
		}

		if (new_tracker_connection)
		{
			tracker_disconnect_server_ex(pNewTracker, result != 0);
		}

		if (result != 0)
		{
			return result;
		}

		if ((*ppStorageServer=tracker_connect_server(pNewStorage, \
			&result)) == NULL)
		{
			return result;
		}

		*new_connection = true;
	}
	else
	{
		if ((*ppStorageServer)->sock >= 0)
		{
			*new_connection = false;
		}
		else
		{
			if ((*ppStorageServer=tracker_connect_server( \
				*ppStorageServer, &result)) == NULL)
			{
				return result;
			}

			*new_connection = true;
		}
	}

	return 0;
}

static int storage_get_upload_connection(ConnectionInfo *pTrackerServer, \
		ConnectionInfo **ppStorageServer, char *group_name, \
		ConnectionInfo *pNewStorage, int *store_path_index, \
		bool *new_connection)
{
	int result;
	bool new_tracker_connection;
	ConnectionInfo *pNewTracker;

	if (*ppStorageServer == NULL)
	{
		CHECK_CONNECTION(pTrackerServer, pNewTracker, result, \
			new_tracker_connection);
		if (*group_name == '\0')
		{
			result = tracker_query_storage_store_without_group( \
				pNewTracker, pNewStorage, group_name, \
				store_path_index);
		}
		else
		{
			result = tracker_query_storage_store_with_group( \
				pNewTracker, group_name, pNewStorage, \
				store_path_index);
		}

		if (new_tracker_connection)
		{
			tracker_disconnect_server_ex(pNewTracker, result != 0);
		}

		if (result != 0)
		{
			return result;
		}

		if ((*ppStorageServer=tracker_connect_server(pNewStorage, \
			&result)) == NULL)
		{
			return result;
		}

		*new_connection = true;
	}
	else
	{
		if ((*ppStorageServer)->sock >= 0)
		{
			*new_connection = false;
		}
		else
		{
			if ((*ppStorageServer=tracker_connect_server( \
				*ppStorageServer, &result)) == NULL)
			{
				return result;
			}

			*new_connection = true;
		}
	}

	return 0;
}

int storage_get_metadata1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer,  \
		const char *file_id, \
		FDFSMetaData **meta_list, int *meta_count)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return storage_get_metadata(pTrackerServer, pStorageServer, \
			group_name, filename, meta_list, meta_count);
}

int storage_get_metadata(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer,  \
			const char *group_name, const char *filename, \
			FDFSMetaData **meta_list, \
			int *meta_count)
{
	TrackerHeader *pHeader;
	int result;
	ConnectionInfo storageServer;
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+128];
	int64_t in_bytes;
	int filename_len;
	char *file_buff;
	int64_t file_size;
	bool new_connection;

	file_buff = NULL;
	*meta_list = NULL;
	*meta_count = 0;

	if ((result=storage_get_update_connection(pTrackerServer, \
		&pStorageServer, group_name, filename, \
		&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	do
	{
	/**
	send pkg format:
	FDFS_GROUP_NAME_MAX_LEN bytes: group_name
	remain bytes: filename
	**/

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	snprintf(out_buff + sizeof(TrackerHeader), sizeof(out_buff) - \
		sizeof(TrackerHeader),  "%s", group_name);
	filename_len = snprintf(out_buff + sizeof(TrackerHeader) + \
			FDFS_GROUP_NAME_MAX_LEN, \
			sizeof(out_buff) - sizeof(TrackerHeader) - \
			FDFS_GROUP_NAME_MAX_LEN,  "%s", filename);

	long2buff(FDFS_GROUP_NAME_MAX_LEN + filename_len, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_GET_METADATA;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
			sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			filename_len, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));

		break;
	}

	if ((result=fdfs_recv_response(pStorageServer, \
		&file_buff, 0, &in_bytes)) != 0)
	{
		break;
	}

	file_size = in_bytes;
	if (file_size == 0)
	{
		break;
	}

	file_buff[in_bytes] = '\0';
	*meta_list = fdfs_split_metadata(file_buff, meta_count, &result);
	} while (0);

	if (file_buff != NULL)
	{
		free(file_buff);
	}

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int storage_query_file_info_ex1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer,  const char *file_id, \
		FDFSFileInfo *pFileInfo, const bool bSilence)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)
	return storage_query_file_info_ex(pTrackerServer, pStorageServer,  \
			group_name, filename, pFileInfo, bSilence);
}

int storage_query_file_info_ex(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer,  \
			const char *group_name, const char *filename, \
			FDFSFileInfo *pFileInfo, const bool bSilence)
{
	TrackerHeader *pHeader;
	int result;
	ConnectionInfo storageServer;
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+128];
	char in_buff[3 * FDFS_PROTO_PKG_LEN_SIZE + IP_ADDRESS_SIZE];
	char buff[64];
	int64_t in_bytes;
	int filename_len;
	int buff_len;
	char *pInBuff;
	char *p;
	bool new_connection;

	if ((result=storage_get_read_connection(pTrackerServer, \
		&pStorageServer, group_name, filename, \
		&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	do
	{
	/**
	send pkg format:
	FDFS_GROUP_NAME_MAX_LEN bytes: group_name
	remain bytes: filename
	**/

	pHeader = (TrackerHeader *)out_buff;
	memset(out_buff, 0, sizeof(out_buff));
	snprintf(out_buff + sizeof(TrackerHeader), sizeof(out_buff) - \
		sizeof(TrackerHeader),  "%s", group_name);
	filename_len = snprintf(out_buff + sizeof(TrackerHeader) + \
			FDFS_GROUP_NAME_MAX_LEN, \
			sizeof(out_buff) - sizeof(TrackerHeader) - \
			FDFS_GROUP_NAME_MAX_LEN,  "%s", filename);

	long2buff(FDFS_GROUP_NAME_MAX_LEN + filename_len, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_QUERY_FILE_INFO;
	pHeader->status = bSilence ? ENOENT : 0;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
			sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
			filename_len, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));

		break;
	}

	pInBuff = in_buff;
	if ((result=fdfs_recv_response(pStorageServer, \
		&pInBuff, sizeof(in_buff), &in_bytes)) != 0)
	{
		break;
	}

	if (in_bytes != sizeof(in_buff))
	{
		logError("file: "__FILE__", line: %d, " \
			"recv data from storage server %s:%d fail, " \
			"recv bytes: %"PRId64" != %d", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			in_bytes, (int)sizeof(in_buff));
		result = EINVAL;
	}

	if (!the_base64_context_inited)
	{
		the_base64_context_inited = 1;
		base64_init_ex(&the_base64_context, 0, '-', '_', '.');
	}

	memset(buff, 0, sizeof(buff));
	if (filename_len >= FDFS_LOGIC_FILE_PATH_LEN \
		+ FDFS_FILENAME_BASE64_LENGTH + FDFS_FILE_EXT_NAME_MAX_LEN + 1)
	{
		base64_decode_auto(&the_base64_context, (char *)filename + \
			FDFS_LOGIC_FILE_PATH_LEN, FDFS_FILENAME_BASE64_LENGTH, \
			buff, &buff_len);
	}

	p = in_buff;
        pFileInfo->file_size = buff2long(p);
	p += FDFS_PROTO_PKG_LEN_SIZE;
	pFileInfo->create_timestamp = buff2long(p);
	p += FDFS_PROTO_PKG_LEN_SIZE;
	pFileInfo->crc32 = buff2long(p);
	p += FDFS_PROTO_PKG_LEN_SIZE;
	memcpy(pFileInfo->source_ip_addr, p, IP_ADDRESS_SIZE);
	*(pFileInfo->source_ip_addr + IP_ADDRESS_SIZE - 1) = '\0';
	} while (0);

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int storage_delete_file1(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer, \
			const char *file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return storage_delete_file(pTrackerServer, \
			pStorageServer, group_name, filename);
}

int storage_truncate_file1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, 
		const char *appender_file_id, \
		const int64_t truncated_file_size)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(appender_file_id)

	return storage_truncate_file(pTrackerServer, \
			pStorageServer, group_name, filename, \
			truncated_file_size);
}

int storage_delete_file(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer, \
			const char *group_name, const char *filename)
{
	TrackerHeader *pHeader;
	int result;
	ConnectionInfo storageServer;
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+128];
	char in_buff[1];
	char *pBuff;
	int64_t in_bytes;
	int filename_len;
	bool new_connection;

	if ((result=storage_get_update_connection(pTrackerServer, \
		&pStorageServer, group_name, filename, \
		&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	do
	{
	/**
	send pkg format:
	FDFS_GROUP_NAME_MAX_LEN bytes: group_name
	remain bytes: filename
	**/

	memset(out_buff, 0, sizeof(out_buff));
	snprintf(out_buff + sizeof(TrackerHeader), sizeof(out_buff) - \
		sizeof(TrackerHeader),  "%s", group_name);
	filename_len = snprintf(out_buff + sizeof(TrackerHeader) + \
			FDFS_GROUP_NAME_MAX_LEN, \
			sizeof(out_buff) - sizeof(TrackerHeader) - \
			FDFS_GROUP_NAME_MAX_LEN,  "%s", filename);

	pHeader = (TrackerHeader *)out_buff;
	long2buff(FDFS_GROUP_NAME_MAX_LEN + filename_len, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_DELETE_FILE;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		sizeof(TrackerHeader) + FDFS_GROUP_NAME_MAX_LEN + \
		filename_len, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	pBuff = in_buff;
	if ((result=fdfs_recv_response(pStorageServer, \
		&pBuff, 0, &in_bytes)) != 0)
	{
		break;
	}

	} while (0);

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int storage_do_download_file1_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const int download_type, const char *file_id, \
		const int64_t file_offset, const int64_t download_bytes, \
		char **file_buff, void *arg, int64_t *file_size)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return storage_do_download_file_ex(pTrackerServer, pStorageServer, \
		download_type, group_name, filename, \
		file_offset, download_bytes, file_buff, arg, file_size);
}

int storage_do_download_file_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const int download_type, \
		const char *group_name, const char *remote_filename, \
		const int64_t file_offset, const int64_t download_bytes, \
		char **file_buff, void *arg, int64_t *file_size)
{
	TrackerHeader *pHeader;
	int result;
	ConnectionInfo storageServer;
	char out_buff[sizeof(TrackerHeader)+FDFS_GROUP_NAME_MAX_LEN+128];
	char *p;
	int out_bytes;
	int64_t in_bytes;
	int64_t total_recv_bytes;
	int filename_len;
	bool new_connection;

	*file_size = 0;
	if ((result=storage_get_read_connection(pTrackerServer, \
		&pStorageServer, group_name, remote_filename, \
		&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	do
	{
	/**
	send pkg format:
	8 bytes: file offset
	8 bytes: download file bytes
	FDFS_GROUP_NAME_MAX_LEN bytes: group_name
	remain bytes: filename
	**/

	memset(out_buff, 0, sizeof(out_buff));
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
	long2buff(file_offset, p);
	p += 8;
	long2buff(download_bytes, p);
	p += 8;
	snprintf(p, sizeof(out_buff) - (p - out_buff), "%s", group_name);
	p += FDFS_GROUP_NAME_MAX_LEN;
	filename_len = snprintf(p, sizeof(out_buff) - (p - out_buff), \
				"%s", remote_filename);
	p += filename_len;
	out_bytes = p - out_buff;
	long2buff(out_bytes - sizeof(TrackerHeader), pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_DOWNLOAD_FILE;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		out_bytes, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	if (download_type == FDFS_DOWNLOAD_TO_FILE)
	{
		if ((result=fdfs_recv_header(pStorageServer, \
			&in_bytes)) != 0)
		{
			break;
		}

		if ((result=tcprecvfile(pStorageServer->sock, \
				*file_buff, in_bytes, 0, \
				g_fdfs_network_timeout, \
				&total_recv_bytes)) != 0)
		{
			break;
		}
	}
	else if (download_type == FDFS_DOWNLOAD_TO_BUFF)
	{
		*file_buff = NULL;
		if ((result=fdfs_recv_response(pStorageServer, \
			file_buff, 0, &in_bytes)) != 0)
		{
			break;
		}
	}
	else
	{
		DownloadCallback callback;
		char buff[2048];
		int recv_bytes;
		int64_t remain_bytes;

		if ((result=fdfs_recv_header(pStorageServer, \
			&in_bytes)) != 0)
		{
			break;
		}

		callback = (DownloadCallback)*file_buff;
		remain_bytes = in_bytes;
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

			if ((result=tcprecvdata_nb(pStorageServer->sock, buff, \
				recv_bytes, g_fdfs_network_timeout)) != 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"recv data from storage server " \
					"%s:%d fail, " \
					"errno: %d, error info: %s", __LINE__, \
					pStorageServer->ip_addr, \
					pStorageServer->port, \
					result, STRERROR(result));
				break;
			}

			result = callback(arg, in_bytes, buff, recv_bytes);
			if (result != 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"call callback function fail, " \
					"error code: %d", __LINE__, result);
				break;
			}

			remain_bytes -= recv_bytes;
		}

		if (remain_bytes != 0)
		{
			break;
		}
	}

	*file_size = in_bytes;
	} while (0);

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int storage_download_file_to_file1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id, \
		const char *local_filename, int64_t *file_size)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return storage_download_file_to_file(pTrackerServer, \
			pStorageServer, group_name, filename, \
			local_filename, file_size);
}

int storage_download_file_to_file(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer, \
			const char *group_name, const char *remote_filename, \
			const char *local_filename, int64_t *file_size)
{
	char *pLocalFilename;
	pLocalFilename = (char *)local_filename;
	return storage_do_download_file(pTrackerServer, pStorageServer, \
			FDFS_DOWNLOAD_TO_FILE, group_name, remote_filename, \
			&pLocalFilename, NULL, file_size);
}

int storage_upload_by_filename1_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int store_path_index, \
		const char cmd, const char *local_filename, \
		const char *file_ext_name, const FDFSMetaData *meta_list, \
		const int meta_count, const char *group_name, char *file_id)
{
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];
	int result;

	if (group_name == NULL)
	{
		*new_group_name = '\0';
	}
	else
	{
		snprintf(new_group_name, sizeof(new_group_name), \
			"%s", group_name);
	}

	result = storage_upload_by_filename_ex(pTrackerServer, \
			pStorageServer, store_path_index, cmd, \
			local_filename, file_ext_name, \
			meta_list, meta_count, \
			new_group_name, remote_filename);
	if (result == 0)
	{
		sprintf(file_id, "%s%c%s", new_group_name, \
			FDFS_FILE_ID_SEPERATOR, remote_filename);
	}
	else
	{
		file_id[0] = '\0';
	}

	return result;
}

int storage_do_upload_file1(ConnectionInfo *pTrackerServer, \
	ConnectionInfo *pStorageServer, const int store_path_index, \
	const char cmd, const int upload_type, \
	const char *file_buff, void *arg, const int64_t file_size, \
	const char *file_ext_name, const FDFSMetaData *meta_list, \
	const int meta_count, const char *group_name, char *file_id)
{
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];
	int result;

	if (group_name == NULL)
	{
		*new_group_name = '\0';
	}
	else
	{
		snprintf(new_group_name, sizeof(new_group_name), \
			"%s", group_name);
	}

	result = storage_do_upload_file(pTrackerServer, \
			pStorageServer, store_path_index, cmd, upload_type, \
			file_buff, arg, file_size, NULL, NULL, file_ext_name, \
			meta_list, meta_count, new_group_name, remote_filename);
	if (result == 0)
	{
		sprintf(file_id, "%s%c%s", new_group_name, \
			FDFS_FILE_ID_SEPERATOR, remote_filename);
	}
	else
	{
		file_id[0] = '\0';
	}

	return result;
}

/**
STORAGE_PROTO_CMD_UPLOAD_FILE and
STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE:
1 byte: store path index
8 bytes: meta data bytes
8 bytes: file size
FDFS_FILE_EXT_NAME_MAX_LEN bytes: file ext name
meta data bytes: each meta data seperated by \x01,
                 name and value seperated by \x02
file size bytes: file content

STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE:
8 bytes: master filename length
8 bytes: meta data bytes
8 bytes: file size
FDFS_FILE_PREFIX_MAX_LEN bytes  : filename prefix
FDFS_FILE_EXT_NAME_MAX_LEN bytes: file ext name, do not include dot (.)
master filename bytes: master filename
meta data bytes: each meta data seperated by \x01,
                 name and value seperated by \x02
file size bytes: file content
**/
int storage_do_upload_file(ConnectionInfo *pTrackerServer, \
	ConnectionInfo *pStorageServer, const int store_path_index, \
	const char cmd, const int upload_type, const char *file_buff, \
	void *arg, const int64_t file_size, const char *master_filename, \
	const char *prefix_name, const char *file_ext_name, \
	const FDFSMetaData *meta_list, const int meta_count, \
	char *group_name, char *remote_filename)
{
	TrackerHeader *pHeader;
	int result;
	char out_buff[512];
	char *p;
	int64_t in_bytes;
	int64_t total_send_bytes;
	char in_buff[128];
	char *pInBuff;
	ConnectionInfo storageServer;
	bool new_connection;
	bool bUploadSlave;
	int new_store_path;
	int master_filename_len;
	int prefix_len;

	*remote_filename = '\0';
	new_store_path = store_path_index;
	if (master_filename != NULL)
	{
		master_filename_len = strlen(master_filename);
	}
	else
	{
		master_filename_len = 0;
	}

	if (prefix_name != NULL)
	{
		prefix_len = strlen(prefix_name);
	}
	else
	{
		prefix_len = 0;
	}

	bUploadSlave = (strlen(group_name) > 0 && master_filename_len > 0);
	if (bUploadSlave)
	{
		if ((result=storage_get_update_connection(pTrackerServer, \
			&pStorageServer, group_name, master_filename, \
			&storageServer, &new_connection)) != 0)
		{
			return result;
		}
	}
	else if ((result=storage_get_upload_connection(pTrackerServer, \
		&pStorageServer, group_name, &storageServer, \
		&new_store_path, &new_connection)) != 0)
	{
		*group_name = '\0';
		return result;
	}

	*group_name = '\0';

	/*
	//logInfo("upload to storage %s:%d\n", \
		pStorageServer->ip_addr, pStorageServer->port);
	*/

	do
	{
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
	if (bUploadSlave)
	{
		long2buff(master_filename_len, p);
		p += FDFS_PROTO_PKG_LEN_SIZE;
	}
	else
	{
		*p++ = (char)new_store_path;
	}

	long2buff(file_size, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	if (bUploadSlave)
	{
		memset(p, 0, FDFS_FILE_PREFIX_MAX_LEN + \
				FDFS_FILE_EXT_NAME_MAX_LEN);
		if (prefix_len > FDFS_FILE_PREFIX_MAX_LEN)
		{
			prefix_len = FDFS_FILE_PREFIX_MAX_LEN;
		}
		if (prefix_len > 0)
		{
			memcpy(p, prefix_name, prefix_len);
		}
		p += FDFS_FILE_PREFIX_MAX_LEN;
	}
	else
	{
		memset(p, 0, FDFS_FILE_EXT_NAME_MAX_LEN);
	}

	if (file_ext_name != NULL)
	{
		int file_ext_len;

		file_ext_len = strlen(file_ext_name);
		if (file_ext_len > FDFS_FILE_EXT_NAME_MAX_LEN)
		{
			file_ext_len = FDFS_FILE_EXT_NAME_MAX_LEN;
		}
		if (file_ext_len > 0)
		{
			memcpy(p, file_ext_name, file_ext_len);
		}
	}
	p += FDFS_FILE_EXT_NAME_MAX_LEN;

	if (bUploadSlave)
	{
		memcpy(p, master_filename, master_filename_len);
		p += master_filename_len;
	}

	long2buff((p - out_buff) + file_size - sizeof(TrackerHeader), \
		pHeader->pkg_len);
	pHeader->cmd = cmd;
	pHeader->status = 0;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		p - out_buff, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	if (upload_type == FDFS_UPLOAD_BY_FILE)
	{
		if ((result=tcpsendfile(pStorageServer->sock, file_buff, \
			file_size, g_fdfs_network_timeout, \
			&total_send_bytes)) != 0)
		{
			break;
		}
	}
	else if (upload_type == FDFS_UPLOAD_BY_BUFF)
	{
		if ((result=tcpsenddata_nb(pStorageServer->sock, \
			(char *)file_buff, file_size, \
			g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"send data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pStorageServer->ip_addr, pStorageServer->port, \
				result, STRERROR(result));
			break;
		}
	}
	else //FDFS_UPLOAD_BY_CALLBACK
	{
		UploadCallback callback;
		callback = (UploadCallback)file_buff;
		if ((result=callback(arg, file_size, pStorageServer->sock))!=0)
		{
			break;
		}
	}

	pInBuff = in_buff;
	if ((result=fdfs_recv_response(pStorageServer, \
		&pInBuff, sizeof(in_buff), &in_bytes)) != 0)
	{
		break;
	}

	if (in_bytes <= FDFS_GROUP_NAME_MAX_LEN)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage server %s:%d response data " \
			"length: %"PRId64" is invalid, " \
			"should > %d", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			in_bytes, FDFS_GROUP_NAME_MAX_LEN);
		result = EINVAL;
		break;
	}

	in_buff[in_bytes] = '\0';
	memcpy(group_name, in_buff, FDFS_GROUP_NAME_MAX_LEN);
	group_name[FDFS_GROUP_NAME_MAX_LEN] = '\0';

	memcpy(remote_filename, in_buff + FDFS_GROUP_NAME_MAX_LEN, \
		in_bytes - FDFS_GROUP_NAME_MAX_LEN + 1);

	} while (0);

	if (result == 0 && meta_count > 0)
	{
		result = storage_set_metadata(pTrackerServer, \
			pStorageServer, group_name, remote_filename, \
			meta_list, meta_count, \
			STORAGE_SET_METADATA_FLAG_OVERWRITE);
		if (result != 0)  //rollback
		{
			storage_delete_file(pTrackerServer, pStorageServer, \
				group_name, remote_filename);
			*group_name = '\0';
			*remote_filename = '\0';
		}
	}

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int storage_upload_by_callback_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int store_path_index, \
		const char cmd, UploadCallback callback, void *arg, \
		const int64_t file_size, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *group_name, char *remote_filename)
{
	return storage_do_upload_file(pTrackerServer, pStorageServer, \
		store_path_index, cmd, FDFS_UPLOAD_BY_CALLBACK, \
		(char *)callback, arg, file_size, NULL, NULL, \
		file_ext_name, meta_list, meta_count, \
		group_name, remote_filename);
}

int storage_upload_by_callback1_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int store_path_index, \
		const char cmd, UploadCallback callback, void *arg, \
		const int64_t file_size, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		const char *group_name, char *file_id)
{
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];
	int result;

	if (group_name == NULL)
	{
		*new_group_name = '\0';
	}
	else
	{
		snprintf(new_group_name, sizeof(new_group_name), \
				"%s", group_name);
	}

	result = storage_do_upload_file(pTrackerServer, \
			pStorageServer, store_path_index, \
			cmd, FDFS_UPLOAD_BY_CALLBACK, (char *)callback, arg, \
			file_size, NULL, NULL, file_ext_name, \
			meta_list, meta_count, \
			new_group_name, remote_filename);
	if (result == 0)
	{
		sprintf(file_id, "%s%c%s", new_group_name, \
			FDFS_FILE_ID_SEPERATOR, remote_filename);
	}
	else
	{
		file_id[0] = '\0';
	}

	return result;
}

int storage_upload_by_filename_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int store_path_index, \
		const char cmd, const char *local_filename, \
		const char *file_ext_name, const FDFSMetaData *meta_list, \
		const int meta_count, char *group_name, char *remote_filename)
{
	struct stat stat_buf;

	if (stat(local_filename, &stat_buf) != 0)
	{
		group_name[0] = '\0';
		remote_filename[0] = '\0';
		return errno;
	}

	if (!S_ISREG(stat_buf.st_mode))
	{
		group_name[0] = '\0';
		remote_filename[0] = '\0';
		return EINVAL;
	}

	if (file_ext_name == NULL)
	{
		file_ext_name = fdfs_get_file_ext_name(local_filename);
	}

	return storage_do_upload_file(pTrackerServer, pStorageServer, \
			store_path_index, cmd, \
			FDFS_UPLOAD_BY_FILE, local_filename, \
			NULL, stat_buf.st_size, NULL, NULL, file_ext_name, \
			meta_list, meta_count, group_name, remote_filename);
}

int storage_set_metadata1(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer, \
			const char *file_id, \
			const FDFSMetaData *meta_list, const int meta_count, \
			const char op_flag)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return storage_set_metadata(pTrackerServer, pStorageServer, \
			group_name, filename, \
			meta_list, meta_count, op_flag);
}

/**
8 bytes: filename length
8 bytes: meta data size
1 bytes: operation flag,
     'O' for overwrite all old metadata
     'M' for merge, insert when the meta item not exist, otherwise update it
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
filename
meta data bytes: each meta data seperated by \x01,
                 name and value seperated by \x02
**/
int storage_set_metadata(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer, \
			const char *group_name, const char *filename, \
			const FDFSMetaData *meta_list, const int meta_count, \
			const char op_flag)
{
	TrackerHeader *pHeader;
	int result;
	ConnectionInfo storageServer;
	char out_buff[sizeof(TrackerHeader)+2*FDFS_PROTO_PKG_LEN_SIZE+\
			FDFS_GROUP_NAME_MAX_LEN+128];
	char in_buff[1];
	int64_t in_bytes;
	char *pBuff;
	int filename_len;
	char *meta_buff;
	int meta_bytes;
	char *p;
	char *pEnd;
	bool new_connection;

	if ((result=storage_get_update_connection(pTrackerServer, \
		&pStorageServer, group_name, filename, \
		&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	meta_buff = NULL;
	do
	{
	memset(out_buff, 0, sizeof(out_buff));
	filename_len = strlen(filename);

	if (meta_count > 0)
	{
		meta_buff = fdfs_pack_metadata(meta_list, meta_count, \
                        NULL, &meta_bytes);
		if (meta_buff == NULL)
		{
			result = ENOMEM;
			break;
		}
	}
	else
	{
		meta_bytes = 0;
	}

	pEnd = out_buff + sizeof(out_buff);
	p = out_buff + sizeof(TrackerHeader);

	long2buff(filename_len, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	long2buff(meta_bytes, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	*p++ = op_flag;

	snprintf(p, pEnd - p,  "%s", group_name);
	p += FDFS_GROUP_NAME_MAX_LEN;

	filename_len = snprintf(p, pEnd - p, "%s", filename);
	p += filename_len;

	pHeader = (TrackerHeader *)out_buff;
	long2buff((int)(p - (out_buff + sizeof(TrackerHeader))) + \
		meta_bytes, pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_SET_METADATA;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
			p - out_buff, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));

		break;
	}

	if (meta_bytes > 0 && (result=tcpsenddata_nb(pStorageServer->sock, \
			meta_buff, meta_bytes, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	pBuff = in_buff;
	result = fdfs_recv_response(pStorageServer, \
		&pBuff, 0, &in_bytes);
	} while (0);

	if (meta_buff != NULL)
	{
		free(meta_buff);
	}

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int storage_download_file_ex1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id, \
		const int64_t file_offset, const int64_t download_bytes, \
		DownloadCallback callback, void *arg, int64_t *file_size)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return storage_download_file_ex(pTrackerServer, pStorageServer, \
		group_name, filename, file_offset, download_bytes, \
		callback, arg, file_size);
}

int storage_download_file_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *group_name, const char *remote_filename, \
		const int64_t file_offset, const int64_t download_bytes, \
		DownloadCallback callback, void *arg, int64_t *file_size)
{
	char *pCallback;
	pCallback = (char *)callback;
	return storage_do_download_file_ex(pTrackerServer, pStorageServer, \
		FDFS_DOWNLOAD_TO_CALLBACK, group_name, remote_filename, \
		file_offset, download_bytes, &pCallback, arg, file_size);
}

int tracker_query_storage_fetch1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return tracker_query_storage_fetch(pTrackerServer, \
		pStorageServer, group_name, filename);
}

int tracker_query_storage_update1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return tracker_query_storage_update(pTrackerServer, \
		pStorageServer, group_name, filename);
}

/**
pkg format:
Header
8 bytes: master filename len
8 bytes: source filename len
8 bytes: source file signature len
FDFS_GROUP_NAME_MAX_LEN bytes: group_name
FDFS_FILE_PREFIX_MAX_LEN bytes  : filename prefix, can be empty
FDFS_FILE_EXT_NAME_MAX_LEN bytes: file ext name, do not include dot (.)
master filename len: master filename
source filename len: source filename without group name
source file signature len: source file signature
**/
int storage_client_create_link(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *master_filename,\
		const char *src_filename, const int src_filename_len, \
		const char *src_file_sig, const int src_file_sig_len, \
		const char *group_name, const char *prefix_name, \
		const char *file_ext_name, \
		char *remote_filename, int *filename_len)
{
	TrackerHeader *pHeader;
	int result;
	char out_buff[sizeof(TrackerHeader) + 4 * FDFS_PROTO_PKG_LEN_SIZE + \
		FDFS_GROUP_NAME_MAX_LEN + FDFS_FILE_PREFIX_MAX_LEN + \
		FDFS_FILE_EXT_NAME_MAX_LEN + 256];
	char in_buff[128];
	char *p;
	int group_name_len;
	int master_filename_len;
	int64_t in_bytes;
	char *pInBuff;
	ConnectionInfo storageServer;
	bool new_connection;

	*remote_filename = '\0';
	if (master_filename != NULL)
	{
		master_filename_len = strlen(master_filename);
	}
	else
	{
		master_filename_len = 0;
	}
	if (src_filename_len >= 128 || src_file_sig_len > 64 || \
		master_filename_len >= 128)
	{
		return EINVAL;
	}

	if ((result=storage_get_update_connection(pTrackerServer, \
		&pStorageServer, group_name, src_filename, \
		&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	do
	{
	memset(out_buff, 0, sizeof(out_buff));
	p = out_buff + sizeof(TrackerHeader);
	long2buff(master_filename_len, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;
	long2buff(src_filename_len, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;
	long2buff(src_file_sig_len, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	group_name_len = strlen(group_name);
	if (group_name_len > FDFS_GROUP_NAME_MAX_LEN)
	{
		group_name_len = FDFS_GROUP_NAME_MAX_LEN;
	}
	memcpy(p, group_name, group_name_len);
	p += FDFS_GROUP_NAME_MAX_LEN;

	if (prefix_name != NULL)
	{
		int prefix_len;

		prefix_len = strlen(prefix_name);
		if (prefix_len > FDFS_FILE_PREFIX_MAX_LEN)
		{
			prefix_len = FDFS_FILE_PREFIX_MAX_LEN;
		}
		if (prefix_len > 0)
		{
			memcpy(p, prefix_name, prefix_len);
		}
	}
	p += FDFS_FILE_PREFIX_MAX_LEN;

	if (file_ext_name != NULL)
	{
		int file_ext_len;

		file_ext_len = strlen(file_ext_name);
		if (file_ext_len > FDFS_FILE_EXT_NAME_MAX_LEN)
		{
			file_ext_len = FDFS_FILE_EXT_NAME_MAX_LEN;
		}
		if (file_ext_len > 0)
		{
			memcpy(p, file_ext_name, file_ext_len);
		}
	}
	p += FDFS_FILE_EXT_NAME_MAX_LEN;

	if (master_filename_len > 0)
	{
		memcpy(p, master_filename, master_filename_len);
		p += master_filename_len;
	}
	memcpy(p, src_filename, src_filename_len);
	p += src_filename_len;
	memcpy(p, src_file_sig, src_file_sig_len);
	p += src_file_sig_len;

	pHeader = (TrackerHeader *)out_buff;
	long2buff(p - out_buff - sizeof(TrackerHeader), pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_CREATE_LINK;
	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		p - out_buff, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	pInBuff = in_buff;
	if ((result=fdfs_recv_response(pStorageServer, \
		&pInBuff, sizeof(in_buff), &in_bytes)) != 0)
	{
		break;
	}

	if (in_bytes <= FDFS_GROUP_NAME_MAX_LEN)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage server %s:%d response data " \
			"length: %"PRId64" is invalid, " \
			"should > %d", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			in_bytes, FDFS_GROUP_NAME_MAX_LEN);
		result = EINVAL;
		break;
	}

	*(in_buff + in_bytes) = '\0';
	*filename_len = in_bytes - FDFS_GROUP_NAME_MAX_LEN;
	memcpy(remote_filename, in_buff + FDFS_GROUP_NAME_MAX_LEN, \
		(*filename_len) + 1);

	} while (0);

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int tracker_query_storage_list1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int nMaxServerCount, \
		int *server_count, const char *file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)
	return tracker_query_storage_list(pTrackerServer, \
		pStorageServer, nMaxServerCount, \
		server_count, group_name, filename);
}

int storage_upload_slave_by_filename(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const char *master_filename, const char *prefix_name, \
		const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *group_name, char *remote_filename)
{
	struct stat stat_buf;

	if (master_filename == NULL || *master_filename == '\0' || \
	prefix_name == NULL || group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	if (stat(local_filename, &stat_buf) != 0)
	{
		*group_name = '\0';
		*remote_filename = '\0';
		return errno != 0 ? errno : EPERM;
	}

	if (!S_ISREG(stat_buf.st_mode))
	{
		*group_name = '\0';
		*remote_filename = '\0';
		return EINVAL;
	}

	if (file_ext_name == NULL)
	{
		file_ext_name = fdfs_get_file_ext_name(local_filename);
	}

	return storage_do_upload_file(pTrackerServer, pStorageServer, \
			0, STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE, \
			FDFS_UPLOAD_BY_FILE, local_filename, \
			NULL, stat_buf.st_size, master_filename, prefix_name, \
			file_ext_name, meta_list, meta_count, \
			group_name, remote_filename);
}

int storage_upload_slave_by_callback(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, \
		const int64_t file_size, const char *master_filename, \
		const char *prefix_name, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *group_name, char *remote_filename)
{
	if (master_filename == NULL || *master_filename == '\0' || \
		prefix_name == NULL || *prefix_name == '\0' || \
		group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	return storage_do_upload_file(pTrackerServer, pStorageServer, \
			0, STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE, \
			FDFS_UPLOAD_BY_CALLBACK, (char *)callback, arg, \
			file_size, master_filename, prefix_name, \
			file_ext_name, meta_list, meta_count, \
			group_name, remote_filename);
}

int storage_upload_slave_by_filebuff(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_size, const char *master_filename, \
		const char *prefix_name, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *group_name, char *remote_filename)
{
	if (master_filename == NULL || *master_filename == '\0' || \
		prefix_name == NULL || *prefix_name == '\0' || \
		group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	return storage_do_upload_file(pTrackerServer, pStorageServer, \
			0, STORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE, \
			FDFS_UPLOAD_BY_BUFF, file_buff, NULL, \
			file_size, master_filename, prefix_name, \
			file_ext_name, meta_list, meta_count, \
			group_name, remote_filename);
}

int storage_upload_slave_by_filename1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const char *master_file_id, const char *prefix_name, \
		const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *file_id)
{
	int result;
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];

	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(master_file_id)

	strcpy(new_group_name, group_name);
	result = storage_upload_slave_by_filename(pTrackerServer, \
			pStorageServer, local_filename, filename, \
			prefix_name, file_ext_name, \
			meta_list, meta_count, \
			new_group_name, remote_filename);
	if (result == 0)
	{
		sprintf(file_id, "%s%c%s", new_group_name, \
			FDFS_FILE_ID_SEPERATOR, remote_filename);
	}
	else
	{
		*file_id = '\0';
	}

	return result;
}

int storage_upload_slave_by_filebuff1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_size, const char *master_file_id, \
		const char *prefix_name, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *file_id)
{
	int result;
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];

	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(master_file_id)

	strcpy(new_group_name, group_name);
	result = storage_upload_slave_by_filebuff(pTrackerServer, \
			pStorageServer, file_buff, file_size, \
			filename, prefix_name, file_ext_name, \
			meta_list, meta_count, \
			new_group_name, remote_filename);
	if (result == 0)
	{
		sprintf(file_id, "%s%c%s", new_group_name, \
			FDFS_FILE_ID_SEPERATOR, remote_filename);
	}
	else
	{
		*file_id = '\0';
	}

	return result;
}

int storage_upload_slave_by_callback1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, \
		const int64_t file_size, const char *master_file_id, \
		const char *prefix_name, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *file_id)
{
	int result;
	char new_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[128];

	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(master_file_id)

	strcpy(new_group_name, group_name);
	result = storage_upload_slave_by_callback(pTrackerServer, \
			pStorageServer, callback, arg, file_size, \
			filename, prefix_name, file_ext_name, \
			meta_list, meta_count, \
			new_group_name, remote_filename);
	if (result == 0)
	{
		sprintf(file_id, "%s%c%s", new_group_name, \
			FDFS_FILE_ID_SEPERATOR, remote_filename);
	}
	else
	{
		*file_id = '\0';
	}

	return result;
}

/**
STORAGE_PROTO_CMD_APPEND_FILE:
8 bytes: appender filename length
8 bytes: file size
master filename bytes: appender filename
file size bytes: file content
**/
int storage_do_append_file(ConnectionInfo *pTrackerServer, \
	ConnectionInfo *pStorageServer, const int upload_type, \
	const char *file_buff, void *arg, const int64_t file_size, \
	const char *group_name, const char *appender_filename)
{
	TrackerHeader *pHeader;
	int result;
	char out_buff[512];
	char *p;
	int64_t in_bytes;
	int64_t total_send_bytes;
	ConnectionInfo storageServer;
	bool new_connection;
	int appender_filename_len;

	appender_filename_len = strlen(appender_filename);

	if ((result=storage_get_update_connection(pTrackerServer, \
			&pStorageServer, group_name, appender_filename, \
			&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	/*
	//printf("upload to storage %s:%d\n", \
		pStorageServer->ip_addr, pStorageServer->port);
	*/

	do
	{
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
	long2buff(appender_filename_len, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	long2buff(file_size, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	memcpy(p, appender_filename, appender_filename_len);
	p += appender_filename_len;

	long2buff((p - out_buff) + file_size - sizeof(TrackerHeader), \
		pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_APPEND_FILE;
	pHeader->status = 0;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		p - out_buff, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	if (upload_type == FDFS_UPLOAD_BY_FILE)
	{
		if ((result=tcpsendfile(pStorageServer->sock, file_buff, \
			file_size, g_fdfs_network_timeout, \
			&total_send_bytes)) != 0)
		{
			break;
		}
	}
	else if (upload_type == FDFS_UPLOAD_BY_BUFF)
	{
		if ((result=tcpsenddata_nb(pStorageServer->sock, \
			(char *)file_buff, file_size, \
			g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"send data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pStorageServer->ip_addr, pStorageServer->port, \
				result, STRERROR(result));
			break;
		}
	}
	else //FDFS_UPLOAD_BY_CALLBACK
	{
		UploadCallback callback;
		callback = (UploadCallback)file_buff;
		if ((result=callback(arg, file_size, pStorageServer->sock))!=0)
		{
			break;
		}
	}

	if ((result=fdfs_recv_header(pStorageServer, &in_bytes)) != 0)
	{
		break;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage server %s:%d response data " \
			"length: %"PRId64" is invalid, " \
			"should == 0", __LINE__, pStorageServer->ip_addr, \
			pStorageServer->port, in_bytes);
		result = EINVAL;
		break;
	}

	} while (0);

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

/**
STORAGE_PROTO_CMD_APPEND_FILE:
8 bytes: appender filename length
8 bytes: file offset
8 bytes: file size
master filename bytes: appender filename
file size bytes: file content
**/
int storage_do_modify_file(ConnectionInfo *pTrackerServer, \
	ConnectionInfo *pStorageServer, const int upload_type, \
	const char *file_buff, void *arg, const int64_t file_offset, \
	const int64_t file_size, const char *group_name, \
	const char *appender_filename)
{
	TrackerHeader *pHeader;
	int result;
	char out_buff[512];
	char *p;
	int64_t in_bytes;
	int64_t total_send_bytes;
	ConnectionInfo storageServer;
	bool new_connection;
	int appender_filename_len;

	appender_filename_len = strlen(appender_filename);
	if ((result=storage_get_update_connection(pTrackerServer, \
			&pStorageServer, group_name, appender_filename, \
			&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	/*
	//printf("upload to storage %s:%d\n", \
		pStorageServer->ip_addr, pStorageServer->port);
	*/

	do
	{
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
	long2buff(appender_filename_len, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	long2buff(file_offset, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	long2buff(file_size, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	memcpy(p, appender_filename, appender_filename_len);
	p += appender_filename_len;

	long2buff((p - out_buff) + file_size - sizeof(TrackerHeader), \
		pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_MODIFY_FILE;
	pHeader->status = 0;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		p - out_buff, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	if (upload_type == FDFS_UPLOAD_BY_FILE)
	{
		if ((result=tcpsendfile(pStorageServer->sock, file_buff, \
			file_size, g_fdfs_network_timeout, \
			&total_send_bytes)) != 0)
		{
			break;
		}
	}
	else if (upload_type == FDFS_UPLOAD_BY_BUFF)
	{
		if ((result=tcpsenddata_nb(pStorageServer->sock, \
			(char *)file_buff, file_size, \
			g_fdfs_network_timeout)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"send data to storage server %s:%d fail, " \
				"errno: %d, error info: %s", __LINE__, \
				pStorageServer->ip_addr, pStorageServer->port, \
				result, STRERROR(result));
			break;
		}
	}
	else //FDFS_UPLOAD_BY_CALLBACK
	{
		UploadCallback callback;
		callback = (UploadCallback)file_buff;
		if ((result=callback(arg, file_size, pStorageServer->sock))!=0)
		{
			break;
		}
	}

	if ((result=fdfs_recv_header(pStorageServer, &in_bytes)) != 0)
	{
		break;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage server %s:%d response data " \
			"length: %"PRId64" is invalid, " \
			"should == 0", __LINE__, pStorageServer->ip_addr, \
			pStorageServer->port, in_bytes);
		result = EINVAL;
		break;
	}

	} while (0);

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

int storage_append_by_filename(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const char *group_name, const char *appender_filename)
{
	struct stat stat_buf;

	if (appender_filename == NULL || *appender_filename == '\0' \
	 || group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	if (stat(local_filename, &stat_buf) != 0)
	{
		return errno != 0 ? errno : EPERM;
	}

	if (!S_ISREG(stat_buf.st_mode))
	{
		return EINVAL;
	}
	return storage_do_append_file(pTrackerServer, pStorageServer, \
		FDFS_UPLOAD_BY_FILE, local_filename, \
		NULL, stat_buf.st_size, group_name, appender_filename);
}

int storage_append_by_callback(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, const int64_t file_size, \
		const char *group_name, const char *appender_filename)
{
	if (appender_filename == NULL || *appender_filename == '\0' \
	 || group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	return storage_do_append_file(pTrackerServer, pStorageServer, \
			FDFS_UPLOAD_BY_CALLBACK, (char *)callback, arg, \
			file_size, group_name, appender_filename);
}

int storage_append_by_filebuff(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_size, const char *group_name, \
		const char *appender_filename)
{
	if (appender_filename == NULL || *appender_filename == '\0' \
	 || group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	return storage_do_append_file(pTrackerServer, pStorageServer, \
			FDFS_UPLOAD_BY_BUFF, file_buff, NULL, \
			file_size, group_name, appender_filename);
}

int storage_append_by_filename1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const char *appender_file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(appender_file_id)

	return storage_append_by_filename(pTrackerServer, \
			pStorageServer, local_filename, group_name, filename);
}

int storage_append_by_filebuff1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_size, const char *appender_file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(appender_file_id)

	return storage_append_by_filebuff(pTrackerServer, \
			pStorageServer, file_buff, file_size, \
			group_name, filename);
}

int storage_append_by_callback1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, \
		const int64_t file_size, const char *appender_file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(appender_file_id)

	return storage_append_by_callback(pTrackerServer, \
			pStorageServer, callback, arg, file_size, \
			group_name, filename);
}

int storage_modify_by_filename(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const int64_t file_offset, const char *group_name, \
		const char *appender_filename)
{
	struct stat stat_buf;

	if (appender_filename == NULL || *appender_filename == '\0' \
	 || group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	if (stat(local_filename, &stat_buf) != 0)
	{
		return errno != 0 ? errno : EPERM;
	}

	if (!S_ISREG(stat_buf.st_mode))
	{
		return EINVAL;
	}
	return storage_do_modify_file(pTrackerServer, pStorageServer, \
		FDFS_UPLOAD_BY_FILE, local_filename, \
		NULL, file_offset, stat_buf.st_size, \
		group_name, appender_filename);
}

int storage_modify_by_callback(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, const int64_t file_offset,\
		const int64_t file_size, const char *group_name, \
		const char *appender_filename)
{
	if (appender_filename == NULL || *appender_filename == '\0' \
	 || group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	return storage_do_modify_file(pTrackerServer, pStorageServer, \
			FDFS_UPLOAD_BY_CALLBACK, (char *)callback, arg, \
			file_offset, file_size, group_name, appender_filename);
}

int storage_modify_by_filebuff(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_offset, const int64_t file_size, \
		const char *group_name, const char *appender_filename)
{
	if (appender_filename == NULL || *appender_filename == '\0' \
	 || group_name == NULL || *group_name == '\0')
	{
		return EINVAL;
	}

	return storage_do_modify_file(pTrackerServer, pStorageServer, \
			FDFS_UPLOAD_BY_BUFF, file_buff, NULL, \
			file_offset, file_size, group_name, appender_filename);
}

int storage_modify_by_filename1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const int64_t file_offset, const char *appender_file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(appender_file_id)

	return storage_modify_by_filename(pTrackerServer, \
			pStorageServer, local_filename, file_offset, \
			group_name, filename);
}

int storage_modify_by_filebuff1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_offset, const int64_t file_size, \
		const char *appender_file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(appender_file_id)

	return storage_modify_by_filebuff(pTrackerServer, \
			pStorageServer, file_buff, file_offset, file_size, \
			group_name, filename);
}

int storage_modify_by_callback1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, \
		const int64_t file_offset, const int64_t file_size, \
		const char *appender_file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(appender_file_id)

	return storage_modify_by_callback(pTrackerServer, \
			pStorageServer, callback, arg, file_offset, file_size, \
			group_name, filename);
}

int fdfs_get_file_info_ex1(const char *file_id, const bool get_from_server, \
			FDFSFileInfo *pFileInfo)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	return fdfs_get_file_info_ex(group_name, filename, get_from_server, \
			pFileInfo);
}

int fdfs_get_file_info_ex(const char *group_name, const char *remote_filename, \
	const bool get_from_server, FDFSFileInfo *pFileInfo)
{
	struct in_addr ip_addr;
	int filename_len;
	int buff_len;
	int result;
	char buff[64];

	memset(pFileInfo, 0, sizeof(FDFSFileInfo));
	if (!the_base64_context_inited)
	{
		the_base64_context_inited = 1;
		base64_init_ex(&the_base64_context, 0, '-', '_', '.');
	}

	filename_len = strlen(remote_filename);
	if (filename_len < FDFS_NORMAL_LOGIC_FILENAME_LENGTH)
	{
		logError("file: "__FILE__", line: %d, " \
			"filename is too short, length: %d < %d", \
			__LINE__, filename_len, \
			FDFS_NORMAL_LOGIC_FILENAME_LENGTH);
		return EINVAL;
	}

	memset(buff, 0, sizeof(buff));
	base64_decode_auto(&the_base64_context, (char *)remote_filename + \
		FDFS_LOGIC_FILE_PATH_LEN, FDFS_FILENAME_BASE64_LENGTH, \
		buff, &buff_len);

	memset(&ip_addr, 0, sizeof(ip_addr));
	ip_addr.s_addr = ntohl(buff2int(buff));
	if (fdfs_get_server_id_type(ip_addr.s_addr) == FDFS_ID_TYPE_SERVER_ID)
	{
		pFileInfo->source_id = ip_addr.s_addr;
		if (g_storage_ids_by_id != NULL && g_storage_id_count > 0)
		{
			char id[16];
			FDFSStorageIdInfo *pStorageId;

			sprintf(id, "%d", pFileInfo->source_id);
			pStorageId = fdfs_get_storage_by_id(id);
			if (pStorageId != NULL)
			{
				strcpy(pFileInfo->source_ip_addr, \
					pStorageId->ip_addr);
			}
			else
			{
				*(pFileInfo->source_ip_addr) = '\0';
			}
		}
		else
		{
			*(pFileInfo->source_ip_addr) = '\0';
		}
	}
	else
	{
		pFileInfo->source_id = 0;
		inet_ntop(AF_INET, &ip_addr, pFileInfo->source_ip_addr, \
				IP_ADDRESS_SIZE);
	}

	pFileInfo->create_timestamp = buff2int(buff + sizeof(int));
	pFileInfo->file_size = buff2long(buff + sizeof(int) * 2);

	if (IS_SLAVE_FILE(filename_len, pFileInfo->file_size) || \
	    IS_APPENDER_FILE(pFileInfo->file_size) || \
	    (*(pFileInfo->source_ip_addr) == '\0' && get_from_server))
	{ //slave file or appender file
		if (get_from_server)
		{
			ConnectionInfo *conn;
			ConnectionInfo trackerServer;

			conn = tracker_get_connection_r(&trackerServer, &result);
			if (result != 0)
			{
				return result;
			}

			result = storage_query_file_info(conn, \
				NULL,  group_name, remote_filename, pFileInfo);
			tracker_disconnect_server_ex(conn, result != 0 && \
							result != ENOENT);

			return result;
		}
		else
		{
			pFileInfo->file_size = -1;
			return 0;
		}
	}
	else  //master file (normal file)
	{
		if ((pFileInfo->file_size >> 63) != 0)
		{
			pFileInfo->file_size &= 0xFFFFFFFF;  //low 32 bits is file size
		}
		else if (IS_TRUNK_FILE(pFileInfo->file_size))
		{
			pFileInfo->file_size = FDFS_TRUNK_FILE_TRUE_SIZE( \
							pFileInfo->file_size);
		}

		pFileInfo->crc32 = buff2int(buff+sizeof(int)*4);
	}

	return 0;
}

int storage_file_exist(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer,  \
			const char *group_name, const char *remote_filename)
{
	FDFSFileInfo file_info;
	return storage_query_file_info_ex(pTrackerServer, \
			pStorageServer, group_name, remote_filename, \
			&file_info, true);
}

int storage_file_exist1(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer,  \
			const char *file_id)
{
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)
	return storage_file_exist(pTrackerServer, pStorageServer,  \
			group_name, filename);
}

int storage_truncate_file(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, 
		const char *group_name, const char *appender_filename, \
		const int64_t truncated_file_size)
{
	TrackerHeader *pHeader;
	int result;
	char out_buff[512];
	char *p;
	int64_t in_bytes;
	ConnectionInfo storageServer;
	bool new_connection;
	int appender_filename_len;

	appender_filename_len = strlen(appender_filename);
	if ((result=storage_get_update_connection(pTrackerServer, \
			&pStorageServer, group_name, appender_filename, \
			&storageServer, &new_connection)) != 0)
	{
		return result;
	}

	/*
	//printf("upload to storage %s:%d\n", \
		pStorageServer->ip_addr, pStorageServer->port);
	*/

	do
	{
	pHeader = (TrackerHeader *)out_buff;
	p = out_buff + sizeof(TrackerHeader);
	long2buff(appender_filename_len, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	long2buff(truncated_file_size, p);
	p += FDFS_PROTO_PKG_LEN_SIZE;

	memcpy(p, appender_filename, appender_filename_len);
	p += appender_filename_len;

	long2buff((p - out_buff) - sizeof(TrackerHeader), \
		pHeader->pkg_len);
	pHeader->cmd = STORAGE_PROTO_CMD_TRUNCATE_FILE;
	pHeader->status = 0;

	if ((result=tcpsenddata_nb(pStorageServer->sock, out_buff, \
		p - out_buff, g_fdfs_network_timeout)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"send data to storage server %s:%d fail, " \
			"errno: %d, error info: %s", __LINE__, \
			pStorageServer->ip_addr, pStorageServer->port, \
			result, STRERROR(result));
		break;
	}

	if ((result=fdfs_recv_header(pStorageServer, &in_bytes)) != 0)
	{
		break;
	}

	if (in_bytes != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"storage server %s:%d response data " \
			"length: %"PRId64" is invalid, " \
			"should == 0", __LINE__, pStorageServer->ip_addr, \
			pStorageServer->port, in_bytes);
		result = EINVAL;
		break;
	}
	} while (0);

	if (new_connection)
	{
		tracker_disconnect_server_ex(pStorageServer, result != 0);
	}

	return result;
}

