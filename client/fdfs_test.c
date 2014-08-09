/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fdfs_client.h"
#include "fdfs_global.h"
#include "base64.h"
#include "sockopt.h"
#include "logger.h"
#include "fdfs_http_shared.h"

int writeToFileCallback(void *arg, const int64_t file_size, const char *data, \
                const int current_size)
{
	if (arg == NULL)
	{
		return EINVAL;
	}

	if (fwrite(data, current_size, 1, (FILE *)arg) != 1)
	{
		return errno != 0 ? errno : EIO;
	}

	return 0;
}

int uploadFileCallback(void *arg, const int64_t file_size, int sock)
{
	int64_t total_send_bytes;
	char *filename;

	if (arg == NULL)
	{
		return EINVAL;
	}

	filename = (char *)arg;
	return tcpsendfile(sock, filename, file_size, \
		g_fdfs_network_timeout, &total_send_bytes);
}

int main(int argc, char *argv[])
{
	char *conf_filename;
	char *local_filename;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	ConnectionInfo storageServer;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	char remote_filename[256];
	char master_filename[256];
	FDFSMetaData meta_list[32];
	int meta_count;
	int i;
	FDFSMetaData *pMetaList;
	char token[32 + 1];
	char file_id[128];
	char file_url[256];
	char szDatetime[20];
	char szPortPart[16];
	int url_len;
	time_t ts;
        char *file_buff;
	int64_t file_size;
	char *operation;
	char *meta_buff;
	int store_path_index;
	FDFSFileInfo file_info;

	printf("This is FastDFS client test program v%d.%02d\n" \
"\nCopyright (C) 2008, Happy Fish / YuQing\n" \
"\nFastDFS may be copied only under the terms of the GNU General\n" \
"Public License V3, which may be found in the FastDFS source kit.\n" \
"Please visit the FastDFS Home Page http://www.csource.org/ \n" \
"for more detail.\n\n" \
, g_fdfs_version.major, g_fdfs_version.minor);

	if (argc < 3)
	{
		printf("Usage: %s <config_file> <operation>\n" \
			"\toperation: upload, download, getmeta, setmeta, " \
			"delete and query_servers\n", argv[0]);
		return 1;
	}

	log_init();
	g_log_context.log_level = LOG_DEBUG;

	conf_filename = argv[1];
	operation = argv[2];
	if ((result=fdfs_client_init(conf_filename)) != 0)
	{
		return result;
	}

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		fdfs_client_destroy();
		return errno != 0 ? errno : ECONNREFUSED;
	}

	pStorageServer = NULL;
	*group_name = '\0';
	local_filename = NULL;
	if (strcmp(operation, "upload") == 0)
	{
		int upload_type;
		char *prefix_name;
		const char *file_ext_name;
		char slave_filename[256];
		int slave_filename_len;

		if (argc < 4)
		{
			printf("Usage: %s <config_file> upload " \
				"<local_filename> [FILE | BUFF | CALLBACK] \n",\
				argv[0]);
			fdfs_client_destroy();
			return EINVAL;
		}

		local_filename = argv[3];
		if (argc == 4)
		{
			upload_type = FDFS_UPLOAD_BY_FILE;
		}
		else
		{
			if (strcmp(argv[4], "BUFF") == 0)
			{
				upload_type = FDFS_UPLOAD_BY_BUFF;
			}
			else if (strcmp(argv[4], "CALLBACK") == 0)
			{
				upload_type = FDFS_UPLOAD_BY_CALLBACK;
			}
			else
			{
				upload_type = FDFS_UPLOAD_BY_FILE;
			}
		}

		store_path_index = 0;

		{
		ConnectionInfo storageServers[FDFS_MAX_SERVERS_EACH_GROUP];
		ConnectionInfo *pServer;
		ConnectionInfo *pServerEnd;
		int storage_count;

		if ((result=tracker_query_storage_store_list_without_group( \
			pTrackerServer, storageServers, \
			FDFS_MAX_SERVERS_EACH_GROUP, &storage_count, \
			group_name, &store_path_index)) == 0)
		{
			printf("tracker_query_storage_store_list_without_group: \n");
			pServerEnd = storageServers + storage_count;
			for (pServer=storageServers; pServer<pServerEnd; pServer++)
			{
				printf("\tserver %d. group_name=%s, " \
				       "ip_addr=%s, port=%d\n", \
					(int)(pServer - storageServers) + 1, \
					group_name, pServer->ip_addr, pServer->port);
			}
			printf("\n");
		}
		}

		if ((result=tracker_query_storage_store(pTrackerServer, \
		                &storageServer, group_name, &store_path_index)) != 0)
		{
			fdfs_client_destroy();
			printf("tracker_query_storage fail, " \
				"error no: %d, error info: %s\n", \
				result, STRERROR(result));
			return result;
		}

		printf("group_name=%s, ip_addr=%s, port=%d\n", \
			group_name, storageServer.ip_addr, \
			storageServer.port);

		if ((pStorageServer=tracker_connect_server(&storageServer, \
			&result)) == NULL)
		{
			fdfs_client_destroy();
			return result;
		}

		memset(&meta_list, 0, sizeof(meta_list));
		meta_count = 0;
		strcpy(meta_list[meta_count].name, "ext_name");
		strcpy(meta_list[meta_count].value, "jpg");
		meta_count++;
		strcpy(meta_list[meta_count].name, "width");
		strcpy(meta_list[meta_count].value, "160");
		meta_count++;
		strcpy(meta_list[meta_count].name, "height");
		strcpy(meta_list[meta_count].value, "80");
		meta_count++;
		strcpy(meta_list[meta_count].name, "file_size");
		strcpy(meta_list[meta_count].value, "115120");
		meta_count++;

		file_ext_name = fdfs_get_file_ext_name(local_filename);
		*group_name = '\0';

		if (upload_type == FDFS_UPLOAD_BY_FILE)
		{
			result = storage_upload_by_filename(pTrackerServer, \
				pStorageServer, store_path_index, \
				local_filename, file_ext_name, \
				meta_list, meta_count, \
				group_name, remote_filename);

			printf("storage_upload_by_filename\n");
		}
		else if (upload_type == FDFS_UPLOAD_BY_BUFF)
		{
			char *file_content;
			if ((result=getFileContent(local_filename, \
					&file_content, &file_size)) == 0)
			{
			result = storage_upload_by_filebuff(pTrackerServer, \
				pStorageServer, store_path_index, \
				file_content, file_size, file_ext_name, \
				meta_list, meta_count, \
				group_name, remote_filename);
			free(file_content);
			}

			printf("storage_upload_by_filebuff\n");
		}
		else
		{
			struct stat stat_buf;

			if (stat(local_filename, &stat_buf) == 0 && \
				S_ISREG(stat_buf.st_mode))
			{
			file_size = stat_buf.st_size;
			result = storage_upload_by_callback(pTrackerServer, \
				pStorageServer, store_path_index, \
				uploadFileCallback, local_filename, \
				file_size, file_ext_name, \
				meta_list, meta_count, \
				group_name, remote_filename);
			}

			printf("storage_upload_by_callback\n");
		}

		if (result != 0)
		{
			printf("upload file fail, " \
				"error no: %d, error info: %s\n", \
				result, STRERROR(result));
			tracker_disconnect_server_ex(pStorageServer, true);
			fdfs_client_destroy();
			return result;
		}

		if (g_tracker_server_http_port == 80)
		{
			*szPortPart = '\0';
		}
		else
		{
			sprintf(szPortPart, ":%d", g_tracker_server_http_port);
		}

		sprintf(file_id, "%s/%s", group_name, remote_filename);
		url_len = sprintf(file_url, "http://%s%s/%s", \
				pStorageServer->ip_addr, szPortPart, file_id);
		if (g_anti_steal_token)
		{
			ts = time(NULL);
			fdfs_http_gen_token(&g_anti_steal_secret_key, file_id, \
                		ts, token);
			sprintf(file_url + url_len, "?token=%s&ts=%d", \
				token, (int)ts);
		}

		printf("group_name=%s, remote_filename=%s\n", \
			group_name, remote_filename);

		fdfs_get_file_info(group_name, remote_filename, &file_info);
		printf("source ip address: %s\n", file_info.source_ip_addr);
		printf("file timestamp=%s\n", formatDatetime(
			file_info.create_timestamp, "%Y-%m-%d %H:%M:%S", \
			szDatetime, sizeof(szDatetime)));
		printf("file size=%"PRId64"\n", file_info.file_size);
		printf("file crc32=%u\n", file_info.crc32);
		printf("example file url: %s\n", file_url);

		strcpy(master_filename, remote_filename);
		*remote_filename = '\0';
		if (upload_type == FDFS_UPLOAD_BY_FILE)
		{
			prefix_name = "_big";
			result = storage_upload_slave_by_filename(pTrackerServer,
				NULL, local_filename, master_filename, \
				prefix_name, file_ext_name, \
				meta_list, meta_count, \
				group_name, remote_filename);

			printf("storage_upload_slave_by_filename\n");
		}
		else if (upload_type == FDFS_UPLOAD_BY_BUFF)
		{
			char *file_content;
			prefix_name = "1024x1024";
			if ((result=getFileContent(local_filename, \
					&file_content, &file_size)) == 0)
			{
			result = storage_upload_slave_by_filebuff(pTrackerServer, \
				NULL, file_content, file_size, master_filename,
				prefix_name, file_ext_name, \
				meta_list, meta_count, \
				group_name, remote_filename);
			free(file_content);
			}

			printf("storage_upload_slave_by_filebuff\n");
		}
		else
		{
			struct stat stat_buf;

			prefix_name = "-small";
			if (stat(local_filename, &stat_buf) == 0 && \
				S_ISREG(stat_buf.st_mode))
			{
			file_size = stat_buf.st_size;
			result = storage_upload_slave_by_callback(pTrackerServer, \
				NULL, uploadFileCallback, local_filename, \
				file_size, master_filename, prefix_name, \
				file_ext_name, meta_list, meta_count, \
				group_name, remote_filename);
			}

			printf("storage_upload_slave_by_callback\n");
		}

		if (result != 0)
		{
			printf("upload slave file fail, " \
				"error no: %d, error info: %s\n", \
				result, STRERROR(result));
			tracker_disconnect_server_ex(pStorageServer, true);
			fdfs_client_destroy();
			return result;
		}

		if (g_tracker_server_http_port == 80)
		{
			*szPortPart = '\0';
		}
		else
		{
			sprintf(szPortPart, ":%d", g_tracker_server_http_port);
		}

		sprintf(file_id, "%s/%s", group_name, remote_filename);
		url_len = sprintf(file_url, "http://%s%s/%s", \
				pStorageServer->ip_addr, szPortPart, file_id);
		if (g_anti_steal_token)
		{
			ts = time(NULL);
			fdfs_http_gen_token(&g_anti_steal_secret_key, file_id, \
                		ts, token);
			sprintf(file_url + url_len, "?token=%s&ts=%d", \
				token, (int)ts);
		}

		printf("group_name=%s, remote_filename=%s\n", \
			group_name, remote_filename);

		fdfs_get_file_info(group_name, remote_filename, &file_info);

		printf("source ip address: %s\n", file_info.source_ip_addr);
		printf("file timestamp=%s\n", formatDatetime(
			file_info.create_timestamp, "%Y-%m-%d %H:%M:%S", \
			szDatetime, sizeof(szDatetime)));
		printf("file size=%"PRId64"\n", file_info.file_size);
		printf("file crc32=%u\n", file_info.crc32);
		printf("example file url: %s\n", file_url);

		if (fdfs_gen_slave_filename(master_filename, \
               		prefix_name, file_ext_name, \
                	slave_filename, &slave_filename_len) == 0)
		{

			if (strcmp(remote_filename, slave_filename) != 0)
			{
				printf("slave_filename=%s\n" \
					"remote_filename=%s\n" \
					"not equal!\n", \
					slave_filename, remote_filename);
			}
		}
	}
	else if (strcmp(operation, "download") == 0 || 
		strcmp(operation, "getmeta") == 0 ||
		strcmp(operation, "setmeta") == 0 ||
		strcmp(operation, "query_servers") == 0 ||
		strcmp(operation, "delete") == 0)
	{
		if (argc < 5)
		{
			printf("Usage: %s <config_file> %s " \
				"<group_name> <remote_filename>\n", \
				argv[0], operation);
			fdfs_client_destroy();
			return EINVAL;
		}

		snprintf(group_name, sizeof(group_name), "%s", argv[3]);
		snprintf(remote_filename, sizeof(remote_filename), \
				"%s", argv[4]);
		if (strcmp(operation, "setmeta") == 0 ||
	 	    strcmp(operation, "delete") == 0)
		{
			result = tracker_query_storage_update(pTrackerServer, \
       	       			&storageServer, group_name, remote_filename);
		}
		else if (strcmp(operation, "query_servers") == 0)
		{
			ConnectionInfo storageServers[FDFS_MAX_SERVERS_EACH_GROUP];
			int server_count;

			result = tracker_query_storage_list(pTrackerServer, \
                		storageServers, FDFS_MAX_SERVERS_EACH_GROUP, \
                		&server_count, group_name, remote_filename);

			if (result != 0)
			{
				printf("tracker_query_storage_list fail, "\
					"group_name=%s, filename=%s, " \
					"error no: %d, error info: %s\n", \
					group_name, remote_filename, \
					result, STRERROR(result));
			}
			else
			{
				printf("server list (%d):\n", server_count);
				for (i=0; i<server_count; i++)
				{
					printf("\t%s:%d\n", \
						storageServers[i].ip_addr, \
						storageServers[i].port);
				}
				printf("\n");
			}

			tracker_disconnect_server_ex(pTrackerServer, result != 0);
			fdfs_client_destroy();
			return result;
		}
		else
		{
			result = tracker_query_storage_fetch(pTrackerServer, \
       	       			&storageServer, group_name, remote_filename);
		}

		if (result != 0)
		{
			fdfs_client_destroy();
			printf("tracker_query_storage_fetch fail, " \
				"group_name=%s, filename=%s, " \
				"error no: %d, error info: %s\n", \
				group_name, remote_filename, \
				result, STRERROR(result));
			return result;
		}

		printf("storage=%s:%d\n", storageServer.ip_addr, \
			storageServer.port);

		if ((pStorageServer=tracker_connect_server(&storageServer, \
			&result)) == NULL)
		{
			fdfs_client_destroy();
			return result;
		}

		if (strcmp(operation, "download") == 0)
		{
			if (argc >= 6)
			{
				local_filename = argv[5];
				if (strcmp(local_filename, "CALLBACK") == 0)
				{
				FILE *fp;
				fp = fopen(local_filename, "wb");
				if (fp == NULL)
				{
					result = errno != 0 ? errno : EPERM;
					printf("open file \"%s\" fail, " \
						"errno: %d, error info: %s", \
						local_filename, result, \
						STRERROR(result));
				}
				else
				{
				result = storage_download_file_ex( \
					pTrackerServer, pStorageServer, \
					group_name, remote_filename, 0, 0, \
					writeToFileCallback, fp, &file_size);
				fclose(fp);
				}
				}
				else
				{
				result = storage_download_file_to_file( \
					pTrackerServer, pStorageServer, \
					group_name, remote_filename, \
					local_filename, &file_size);
				}
			}
			else
			{
				file_buff = NULL;
				if ((result=storage_download_file_to_buff( \
					pTrackerServer, pStorageServer, \
					group_name, remote_filename, \
					&file_buff, &file_size)) == 0)
				{
					local_filename = strrchr( \
							remote_filename, '/');
					if (local_filename != NULL)
					{
						local_filename++;  //skip /
					}
					else
					{
						local_filename=remote_filename;
					}

					result = writeToFile(local_filename, \
						file_buff, file_size);

					free(file_buff);
				}
			}

			if (result == 0)
			{
				printf("download file success, " \
					"file size=%"PRId64", file save to %s\n", \
					 file_size, local_filename);
			}
			else
			{
				printf("download file fail, " \
					"error no: %d, error info: %s\n", \
					result, STRERROR(result));
			}
		}
		else if (strcmp(operation, "getmeta") == 0)
		{
			if ((result=storage_get_metadata(pTrackerServer, \
				pStorageServer, group_name, remote_filename, \
				&pMetaList, &meta_count)) == 0)
			{
				printf("get meta data success, " \
					"meta count=%d\n", meta_count);
				for (i=0; i<meta_count; i++)
				{
					printf("%s=%s\n", \
						pMetaList[i].name, \
						pMetaList[i].value);
				}

				free(pMetaList);
			}
			else
			{
				printf("getmeta fail, " \
					"error no: %d, error info: %s\n", \
					result, STRERROR(result));
			}
		}
		else if (strcmp(operation, "setmeta") == 0)
		{
			if (argc < 7)
			{
				printf("Usage: %s <config_file> %s " \
					"<group_name> <remote_filename> " \
					"<op_flag> <metadata_list>\n" \
					"\top_flag: %c for overwrite, " \
					"%c for merge\n" \
					"\tmetadata_list: name1=value1," \
					"name2=value2,...\n", \
					argv[0], operation, \
					STORAGE_SET_METADATA_FLAG_OVERWRITE, \
					STORAGE_SET_METADATA_FLAG_MERGE);
				fdfs_client_destroy();
				return EINVAL;
			}

			meta_buff = strdup(argv[6]);
			if (meta_buff == NULL)
			{
				printf("Out of memory!\n");
				return ENOMEM;
			}

			pMetaList = fdfs_split_metadata_ex(meta_buff, \
					',', '=', &meta_count, &result);
			if (pMetaList == NULL)
			{
				printf("Out of memory!\n");
				free(meta_buff);
				return ENOMEM;
			}

			if ((result=storage_set_metadata(pTrackerServer, \
				NULL, group_name, remote_filename, \
				pMetaList, meta_count, *argv[5])) == 0)
			{
				printf("set meta data success\n");
			}
			else
			{
				printf("setmeta fail, " \
					"error no: %d, error info: %s\n", \
					result, STRERROR(result));
			}

			free(meta_buff);
			free(pMetaList);
		}
		else if(strcmp(operation, "delete") == 0)
		{
			if ((result=storage_delete_file(pTrackerServer, \
			NULL, group_name, remote_filename)) == 0)
			{
				printf("delete file success\n");
			}
			else
			{
				printf("delete file fail, " \
					"error no: %d, error info: %s\n", \
					result, STRERROR(result));
			}
		}
	}
	else
	{
		fdfs_client_destroy();
		printf("invalid operation: %s\n", operation);
		return EINVAL;
	}

	/* for test only */
	if ((result=fdfs_active_test(pTrackerServer)) != 0)
	{
		printf("active_test to tracker server %s:%d fail, errno: %d\n", \
			pTrackerServer->ip_addr, pTrackerServer->port, result);
	}

	/* for test only */
	if ((result=fdfs_active_test(pStorageServer)) != 0)
	{
		printf("active_test to storage server %s:%d fail, errno: %d\n", \
			pStorageServer->ip_addr, pStorageServer->port, result);
	}

	tracker_disconnect_server_ex(pStorageServer, true);
	tracker_disconnect_server_ex(pTrackerServer, true);

	fdfs_client_destroy();

	return result;
}

