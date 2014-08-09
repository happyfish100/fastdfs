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
#include "fdfs_http_shared.h"
#include "sockopt.h"
#include "logger.h"

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
	FDFSMetaData meta_list[32];
	int meta_count;
	int i;
	FDFSMetaData *pMetaList;
	char token[32 + 1];
	char file_id[128];
	char master_file_id[128];
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

	local_filename = NULL;
	if (strcmp(operation, "upload") == 0)
	{
		int upload_type;
		char *prefix_name;
		const char *file_ext_name;
		char slave_file_id[256];
		int slave_file_id_len;

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

		{
		ConnectionInfo storageServers[FDFS_MAX_SERVERS_EACH_GROUP];
		ConnectionInfo *pServer;
		ConnectionInfo *pServerEnd;
		int storage_count;

		strcpy(group_name, "group1");
		if ((result=tracker_query_storage_store_list_with_group( \
			pTrackerServer, group_name, storageServers, \
			FDFS_MAX_SERVERS_EACH_GROUP, &storage_count, \
			&store_path_index)) == 0)
		{
			printf("tracker_query_storage_store_list_with_group: \n");
			pServerEnd = storageServers + storage_count;
			for (pServer=storageServers; pServer<pServerEnd; pServer++)
			{
				printf("\tserver %d. group_name=%s, " \
				       "ip_addr=%s, port=%d\n", \
					(int)(pServer - storageServers) + 1, \
					group_name, pServer->ip_addr, \
					pServer->port);
			}
			printf("\n");
		}
		}

		*group_name = '\0';
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
		strcpy(group_name, "");

		if (upload_type == FDFS_UPLOAD_BY_FILE)
		{
			printf("storage_upload_by_filename\n");
			result = storage_upload_by_filename1(pTrackerServer, \
				pStorageServer, store_path_index, \
				local_filename, file_ext_name, \
				meta_list, meta_count, \
				group_name, file_id);
		}
		else if (upload_type == FDFS_UPLOAD_BY_BUFF)
		{
			char *file_content;
			printf("storage_upload_by_filebuff\n");
			if ((result=getFileContent(local_filename, \
					&file_content, &file_size)) == 0)
			{
			result = storage_upload_by_filebuff1(pTrackerServer, \
				pStorageServer, store_path_index, \
				file_content, file_size, file_ext_name, \
				meta_list, meta_count, \
				group_name, file_id);
			free(file_content);
			}
		}
		else
		{
			struct stat stat_buf;

			printf("storage_upload_by_callback\n");
			if (stat(local_filename, &stat_buf) == 0 && \
				S_ISREG(stat_buf.st_mode))
			{
			file_size = stat_buf.st_size;
			result = storage_upload_by_callback1(pTrackerServer, \
				pStorageServer, store_path_index, \
				uploadFileCallback, local_filename, \
				file_size, file_ext_name, \
				meta_list, meta_count, \
				group_name, file_id);
			}
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

		url_len = sprintf(file_url, "http://%s%s/%s", \
				pStorageServer->ip_addr, szPortPart, file_id);
		if (g_anti_steal_token)
		{
			ts = time(NULL);
			fdfs_http_gen_token(&g_anti_steal_secret_key, \
				file_id, ts, token);
			sprintf(file_url + url_len, "?token=%s&ts=%d", \
				token, (int)ts);
		}

		fdfs_get_file_info1(file_id, &file_info);
		printf("source ip address: %s\n", file_info.source_ip_addr);
		printf("file timestamp=%s\n", formatDatetime(
			file_info.create_timestamp, "%Y-%m-%d %H:%M:%S", \
			szDatetime, sizeof(szDatetime)));
		printf("file size=%"PRId64"\n", file_info.file_size);
		printf("file crc32=%u\n", file_info.crc32);
		printf("example file url: %s\n", file_url);

		strcpy(master_file_id, file_id);
		*file_id = '\0';

		if (upload_type == FDFS_UPLOAD_BY_FILE)
		{
			prefix_name = "_big";
			printf("storage_upload_slave_by_filename\n");
			result = storage_upload_slave_by_filename1( \
				pTrackerServer, NULL, \
				local_filename, master_file_id, \
				prefix_name, file_ext_name, \
				meta_list, meta_count, file_id);
		}
		else if (upload_type == FDFS_UPLOAD_BY_BUFF)
		{
			char *file_content;
			prefix_name = "1024x1024";
			printf("storage_upload_slave_by_filebuff\n");
			if ((result=getFileContent(local_filename, \
					&file_content, &file_size)) == 0)
			{
			result = storage_upload_slave_by_filebuff1( \
				pTrackerServer, NULL, file_content, file_size, \
				master_file_id, prefix_name, file_ext_name, \
				meta_list, meta_count, file_id);
			free(file_content);
			}
		}
		else
		{
			struct stat stat_buf;

			prefix_name = "_small";
			printf("storage_upload_slave_by_callback\n");
			if (stat(local_filename, &stat_buf) == 0 && \
				S_ISREG(stat_buf.st_mode))
			{
			file_size = stat_buf.st_size;
			result = storage_upload_slave_by_callback1( \
				pTrackerServer, NULL, \
				uploadFileCallback, local_filename, \
				file_size, master_file_id, \
				prefix_name, file_ext_name, \
				meta_list, meta_count, file_id);
			}
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
		url_len = sprintf(file_url, "http://%s%s/%s", \
				pStorageServer->ip_addr, szPortPart, file_id);
		if (g_anti_steal_token)
		{
			ts = time(NULL);
			fdfs_http_gen_token(&g_anti_steal_secret_key, \
				file_id, ts, token);
			sprintf(file_url + url_len, "?token=%s&ts=%d", \
				token, (int)ts);
		}

		fdfs_get_file_info1(file_id, &file_info);
		printf("source ip address: %s\n", file_info.source_ip_addr);
		printf("file timestamp=%s\n", formatDatetime(
			file_info.create_timestamp, "%Y-%m-%d %H:%M:%S", \
			szDatetime, sizeof(szDatetime)));
		printf("file size=%"PRId64"\n", file_info.file_size);
		printf("file crc32=%u\n", file_info.crc32);
		printf("example file url: %s\n", file_url);

		if (fdfs_gen_slave_filename(master_file_id, \
               		prefix_name, file_ext_name, \
                	slave_file_id, &slave_file_id_len) == 0)
		{
			if (strcmp(file_id, slave_file_id) != 0)
			{
				printf("slave_file_id=%s\n" \
					"file_id=%s\n" \
					"not equal!\n", \
					slave_file_id, file_id);
			}
		}
	}
	else if (strcmp(operation, "download") == 0 || 
		strcmp(operation, "getmeta") == 0 ||
		strcmp(operation, "setmeta") == 0 ||
		strcmp(operation, "query_servers") == 0 ||
		strcmp(operation, "delete") == 0)
	{
		if (argc < 4)
		{
			printf("Usage: %s <config_file> %s " \
				"<file_id>\n", \
				argv[0], operation);
			fdfs_client_destroy();
			return EINVAL;
		}

		snprintf(file_id, sizeof(file_id), "%s", argv[3]);
		if (strcmp(operation, "query_servers") == 0)
		{
			ConnectionInfo storageServers[FDFS_MAX_SERVERS_EACH_GROUP];
			int server_count;

			result = tracker_query_storage_list1(pTrackerServer, \
                		storageServers, FDFS_MAX_SERVERS_EACH_GROUP, \
                		&server_count, file_id);

			if (result != 0)
			{
				printf("tracker_query_storage_list1 fail, "\
					"file_id=%s, " \
					"error no: %d, error info: %s\n", \
					file_id, result, STRERROR(result));
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

			tracker_disconnect_server_ex(pTrackerServer, true);
			fdfs_client_destroy();
			return result;
		}

		if ((result=tracker_query_storage_fetch1(pTrackerServer, \
       	       		&storageServer, file_id)) != 0)
		{
			fdfs_client_destroy();
			printf("tracker_query_storage_fetch fail, " \
				"file_id=%s, " \
				"error no: %d, error info: %s\n", \
				file_id, result, STRERROR(result));
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
			if (argc >= 5)
			{
				local_filename = argv[4];
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
				result = storage_download_file_ex1( \
					pTrackerServer, pStorageServer, \
					file_id, 0, 0, \
					writeToFileCallback, fp, &file_size);
				fclose(fp);
				}
				}
				else
				{
				result = storage_download_file_to_file1( \
					pTrackerServer, pStorageServer, \
					file_id, \
					local_filename, &file_size);
				}
			}
			else
			{
				file_buff = NULL;
				if ((result=storage_download_file_to_buff1( \
					pTrackerServer, pStorageServer, \
					file_id, \
					&file_buff, &file_size)) == 0)
				{
					local_filename = strrchr( \
							file_id, '/');
					if (local_filename != NULL)
					{
						local_filename++;  //skip /
					}
					else
					{
						local_filename=file_id;
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
			if ((result=storage_get_metadata1(pTrackerServer, \
				NULL, file_id, \
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
			if (argc < 6)
			{
				printf("Usage: %s <config_file> %s " \
					"<file_id> " \
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

			meta_buff = strdup(argv[5]);
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

			if ((result=storage_set_metadata1(pTrackerServer, \
				NULL, file_id, \
				pMetaList, meta_count, *argv[4])) == 0)
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
			if ((result=storage_delete_file1(pTrackerServer, \
			NULL, file_id)) == 0)
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

