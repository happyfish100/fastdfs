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

int dfs_init(const int proccess_index, const char *conf_filename)
{
	return fdfs_client_init(conf_filename);
}

void dfs_destroy()
{
	fdfs_client_destroy();
}

static int downloadFileCallback(void *arg, const int64_t file_size, const char *data, \
                const int current_size)
{
	return 0;
}

int upload_file(const char *file_buff, const int file_size, char *file_id, char *storage_ip)
{
	int result;
	int store_path_index;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	ConnectionInfo storageServer;

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		return errno != 0 ? errno : ECONNREFUSED;
	}

	*group_name = '\0';
	if ((result=tracker_query_storage_store(pTrackerServer, &storageServer,
			 group_name, &store_path_index)) != 0)
	{
		tracker_disconnect_server_ex(pTrackerServer, true);
		return result;
	}

	if ((pStorageServer=tracker_connect_server(&storageServer, &result)) \
			 == NULL)
	{
		tracker_disconnect_server(pTrackerServer);
		return result;
	}

	strcpy(storage_ip, storageServer.ip_addr);
	result = storage_upload_by_filebuff1(pTrackerServer, pStorageServer, 
		store_path_index, file_buff, file_size, NULL, NULL, 0, "", file_id);

	tracker_disconnect_server(pTrackerServer);
	tracker_disconnect_server(pStorageServer);

	return result;
}

int download_file(const char *file_id, int *file_size, char *storage_ip)
{
	int result;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	ConnectionInfo storageServer;
	int64_t file_bytes;

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		return errno != 0 ? errno : ECONNREFUSED;
	}

	if ((result=tracker_query_storage_fetch1(pTrackerServer, \
			&storageServer, file_id)) != 0)
	{
		tracker_disconnect_server_ex(pTrackerServer, true);
		return result;
	}

	if ((pStorageServer=tracker_connect_server(&storageServer, &result)) \
			 == NULL)
	{
		tracker_disconnect_server(pTrackerServer);
		return result;
	}

	strcpy(storage_ip, storageServer.ip_addr);
	result = storage_download_file_ex1(pTrackerServer, pStorageServer, \
		file_id, 0, 0, downloadFileCallback, NULL, &file_bytes);
	*file_size = file_bytes;

	tracker_disconnect_server(pTrackerServer);
	tracker_disconnect_server(pStorageServer);

	return result;
}

int delete_file(const char *file_id, char *storage_ip)
{
	int result;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	ConnectionInfo storageServer;

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		return errno != 0 ? errno : ECONNREFUSED;
	}

	if ((result=tracker_query_storage_update1(pTrackerServer, \
		&storageServer, file_id)) != 0)
	{
		tracker_disconnect_server_ex(pTrackerServer, true);
		return result;
	}

	if ((pStorageServer=tracker_connect_server(&storageServer, &result)) \
			 == NULL)
	{
		tracker_disconnect_server(pTrackerServer);
		return result;
	}

	strcpy(storage_ip, storageServer.ip_addr);
	result = storage_delete_file1(pTrackerServer, pStorageServer, file_id);

	tracker_disconnect_server(pTrackerServer);
	tracker_disconnect_server(pStorageServer);

	return result;
}

