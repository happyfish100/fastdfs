#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "fdfs_global.h"
#include "dfs_func.h"
#include "fdfs_client.h"

static ConnectionInfo *pTrackerServer;
static ConnectionInfo storage_servers[FDFS_MAX_SERVERS_EACH_GROUP];
static int storage_server_count = 0;

static ConnectionInfo *getConnectedStorageServer(
		ConnectionInfo *pStorageServer, int *err_no)
{
	ConnectionInfo *pEnd;
	ConnectionInfo *pServer;

	pEnd = storage_servers + storage_server_count;
	for (pServer=storage_servers; pServer<pEnd; pServer++)
	{
		if (strcmp(pStorageServer->ip_addr, pServer->ip_addr) == 0)
		{
			if (pServer->sock < 0)
			{
				*err_no = conn_pool_connect_server(pServer, \
					g_fdfs_connect_timeout);
				if (*err_no != 0)
				{
					return NULL;
				}
			}
			else
			{
				*err_no = 0;
			}

			return pServer;
		}
	}

	pServer = pEnd;
	memcpy(pServer, pStorageServer, sizeof(ConnectionInfo));
	pServer->sock = -1;
	if ((*err_no=conn_pool_connect_server(pServer, \
		g_fdfs_connect_timeout)) != 0)
	{
		return NULL;
	}

	storage_server_count++;

	*err_no = 0;
	return pServer;
}

int dfs_init(const int proccess_index, const char *conf_filename)
{
	int result;
	if ((result=fdfs_client_init(conf_filename)) != 0)
	{
		return result;
	}

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		return errno != 0 ? errno : ECONNREFUSED;
	}

	return 0;
}

void dfs_destroy()
{
	ConnectionInfo *pEnd;
	ConnectionInfo *pServer;

	tracker_disconnect_server(pTrackerServer);

	pEnd = storage_servers + storage_server_count;
	for (pServer=storage_servers; pServer<pEnd; pServer++)
	{
		conn_pool_disconnect_server(pServer);
	}

	fdfs_client_destroy();
}

static int downloadFileCallback(void *arg, const int64_t file_size, 
		const char *data, const int current_size)
{
	return 0;
}

int upload_file(const char *file_buff, const int file_size, char *file_id, 
		char *storage_ip)
{
	int result;
	int store_path_index;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	ConnectionInfo storageServer;
	ConnectionInfo *pStorageServer;

	*group_name = '\0';
	if ((result=tracker_query_storage_store(pTrackerServer, &storageServer,
			 group_name, &store_path_index)) != 0)
	{
		return result;
	}


	if ((pStorageServer=getConnectedStorageServer(&storageServer, 
			&result)) == NULL)
	{
		return result;
	}

	strcpy(storage_ip, storageServer.ip_addr);
	result = storage_upload_by_filebuff1(pTrackerServer, pStorageServer, 
		store_path_index, file_buff, file_size, NULL, NULL, 0, "", file_id);

	return result;
}

int download_file(const char *file_id, int *file_size, char *storage_ip)
{
	int result;
	ConnectionInfo storageServer;
	ConnectionInfo *pStorageServer;
	int64_t file_bytes;

	if ((result=tracker_query_storage_fetch1(pTrackerServer, 
			&storageServer, file_id)) != 0)
	{
		return result;
	}

	if ((pStorageServer=getConnectedStorageServer(&storageServer, 
			&result)) == NULL)
	{
		return result;
	}

	strcpy(storage_ip, storageServer.ip_addr);
	result = storage_download_file_ex1(pTrackerServer, pStorageServer, 
			file_id, 0, 0, downloadFileCallback, NULL, &file_bytes);
	*file_size = file_bytes;

	return result;
}

int delete_file(const char *file_id, char *storage_ip)
{
	int result;
	ConnectionInfo storageServer;
	ConnectionInfo *pStorageServer;

	if ((result=tracker_query_storage_update1(pTrackerServer, 
			&storageServer, file_id)) != 0)
	{
		return result;
	}

	if ((pStorageServer=getConnectedStorageServer(&storageServer, 
			&result)) == NULL)
	{
		return result;
	}

	strcpy(storage_ip, storageServer.ip_addr);
	result = storage_delete_file1(pTrackerServer, pStorageServer, file_id);

	return result;
}

