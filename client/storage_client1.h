/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#ifndef STORAGE_CLIENT1_H
#define STORAGE_CLIENT1_H

#include "tracker_types.h"
#include "storage_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* upload file to storage server (by file name)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       store_path_index: the index of path on the storage server
*       local_filename: local filename to upload
*       file_ext_name: file ext name, not include dot(.), 
*                      if be NULL will abstract ext name from the local filename
*	meta_list: meta info array
*       meta_count: meta item count
*       group_name: specify the group name to upload file to, can be NULL or emtpy
*	file_id: return the new created file id (including group name and filename)
* return: 0 success, !=0 fail, return the error code
**/
#define storage_upload_by_filename1(pTrackerServer, pStorageServer, \
		store_path_index, local_filename, file_ext_name, \
		meta_list, meta_count, group_name, file_id) \
	storage_upload_by_filename1_ex(pTrackerServer, pStorageServer, \
		store_path_index, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		local_filename, file_ext_name, meta_list, meta_count, \
		group_name, file_id)

#define storage_upload_appender_by_filename1(pTrackerServer, pStorageServer, \
		store_path_index, local_filename, file_ext_name, \
		meta_list, meta_count, group_name, file_id) \
	storage_upload_by_filename1_ex(pTrackerServer, pStorageServer, \
		store_path_index, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		local_filename, file_ext_name, meta_list, meta_count, \
		group_name, file_id)

int storage_upload_by_filename1_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int store_path_index, \
		const char cmd, const char *local_filename, \
		const char *file_ext_name, const FDFSMetaData *meta_list, \
		const int meta_count, const char *group_name, char *file_id);

/**
* upload file to storage server (by file buff)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       store_path_index: the index of path on the storage server
*       file_buff: file content/buff
*       file_size: file size (bytes)
*       file_ext_name: file ext name, not include dot(.), can be NULL
*	meta_list: meta info array
*       meta_count: meta item count
*       group_name: specify the group name to upload file to, can be NULL or emtpy
*	file_id: return the new created file id (including group name and filename)
* return: 0 success, !=0 fail, return the error code
**/
#define storage_upload_by_filebuff1(pTrackerServer, pStorageServer, \
		store_path_index, file_buff, file_size, file_ext_name, \
		meta_list, meta_count, group_name, file_id) \
	storage_do_upload_file1(pTrackerServer, pStorageServer, \
		store_path_index, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		FDFS_UPLOAD_BY_BUFF, file_buff, NULL, \
		file_size, file_ext_name, meta_list, meta_count, \
		group_name, file_id)

#define storage_upload_appender_by_filebuff1(pTrackerServer, pStorageServer, \
		store_path_index, file_buff, file_size, file_ext_name, \
		meta_list, meta_count, group_name, file_id) \
	storage_do_upload_file1(pTrackerServer, pStorageServer, \
		store_path_index, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		FDFS_UPLOAD_BY_BUFF, file_buff, NULL, \
		file_size, file_ext_name, meta_list, meta_count, \
		group_name, file_id)

int storage_do_upload_file1(ConnectionInfo *pTrackerServer, \
	ConnectionInfo *pStorageServer, const int store_path_index, \
	const char cmd, const int upload_type, \
	const char *file_buff, void *arg, const int64_t file_size, \
	const char *file_ext_name, const FDFSMetaData *meta_list, \
	const int meta_count, const char *group_name, char *file_id);

/**
* upload file to storage server (by callback)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       store_path_index: the index of path on the storage server
*       file_size: the file size
*       file_ext_name: file ext name, not include dot(.), can be NULL
*       callback: callback function to send file content to storage server
*       arg: callback extra arguement
*	meta_list: meta info array
*       meta_count: meta item count
*       group_name: specify the group name to upload file to, can be NULL or emtpy
*	file_id: return the new created file id (including group name and filename)
* return: 0 success, !=0 fail, return the error code
**/
#define storage_upload_by_callback1(pTrackerServer, pStorageServer, \
		store_path_index, callback, arg, \
		file_size, file_ext_name, meta_list, meta_count, \
		group_name, file_id) \
	storage_upload_by_callback1_ex(pTrackerServer, pStorageServer, \
		store_path_index, STORAGE_PROTO_CMD_UPLOAD_FILE, \
		callback, arg, file_size, file_ext_name, meta_list, \
		meta_count, group_name, file_id)

#define storage_upload_appender_by_callback1(pTrackerServer, pStorageServer, \
		store_path_index, callback, arg, \
		file_size, file_ext_name, meta_list, meta_count, \
		group_name, file_id) \
	storage_upload_by_callback1_ex(pTrackerServer, pStorageServer, \
		store_path_index, STORAGE_PROTO_CMD_UPLOAD_APPENDER_FILE, \
		callback, arg, file_size, file_ext_name, meta_list, \
		meta_count, group_name, file_id)

int storage_upload_by_callback1_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int store_path_index, \
		const char cmd, UploadCallback callback, void *arg, \
		const int64_t file_size, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		const char *group_name, char *file_id);

/**
* delete file from storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*	file_id: the file id to deleted (including group name and filename)
* return: 0 success, !=0 fail, return the error code
**/
int storage_delete_file1(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer, \
			const char *file_id);

/**
* delete file from storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       appender_file_id: the appender file id
*	truncated_file_size: the truncated file size
* return: 0 success, !=0 fail, return the error code
**/
int storage_truncate_file1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, 
		const char *appender_file_id, \
		const int64_t truncated_file_size);

/**
* set metadata items to storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*	file_id: the file id (including group name and filename)
*	meta_list: meta item array
*       meta_count: meta item count
*       op_flag:
*            # STORAGE_SET_METADATA_FLAG_OVERWRITE('O'): overwrite all old 
*				metadata items
*            # STORAGE_SET_METADATA_FLAG_MERGE ('M'): merge, insert when
*				the metadata item not exist, otherwise update it
* return: 0 success, !=0 fail, return the error code
**/
int storage_set_metadata1(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer, \
			const char *file_id, \
			const FDFSMetaData *meta_list, const int meta_count, \
			const char op_flag);

/**
* download file from storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*	file_id: the file id (including group name and filename)
*       file_buff: return file content/buff, must be freed
*       file_size: return file size (bytes)
* return: 0 success, !=0 fail, return the error code
**/
#define storage_download_file1(pTrackerServer, pStorageServer, file_id, \
			file_buff, file_size)  \
	storage_do_download_file1_ex(pTrackerServer, pStorageServer, \
			FDFS_DOWNLOAD_TO_BUFF, file_id, 0, 0, \
			file_buff, NULL, file_size)

#define storage_download_file_to_buff1(pTrackerServer, pStorageServer, \
			file_id, file_buff, file_size)  \
	storage_do_download_file1_ex(pTrackerServer, pStorageServer, \
			FDFS_DOWNLOAD_TO_BUFF, file_id, 0, 0, \
			file_buff, NULL, file_size)

#define storage_do_download_file1(pTrackerServer, pStorageServer, \
			download_type, file_id, file_buff, file_size) \
	storage_do_download_file1_ex(pTrackerServer, pStorageServer, \
			download_type, file_id, \
			0, 0, file_buff, NULL, file_size)

/**
* download file from storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*	file_id: the file id (including group name and filename)
*       file_offset: the start offset to download
*       download_bytes: download bytes, 0 means from start offset to the file end
*       file_buff: return file content/buff, must be freed
*       file_size: return file size (bytes)
* return: 0 success, !=0 fail, return the error code
**/
int storage_do_download_file1_ex(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const int download_type, const char *file_id, \
		const int64_t file_offset, const int64_t download_bytes, \
		char **file_buff, void *arg, int64_t *file_size);

/**
* download file from storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*	file_id: the file id (including group name and filename)
*	local_filename: local filename to write
*       file_size: return file size (bytes)
* return: 0 success, !=0 fail, return the error code
**/
int storage_download_file_to_file1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id, \
		const char *local_filename, int64_t *file_size);

/**
* get all metadata items from storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*	file_id: the file id (including group name and filename)
*	meta_list: return meta info array, must be freed
*       meta_count: return meta item count
* return: 0 success, !=0 fail, return the error code
**/
int storage_get_metadata1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer,  \
		const char *file_id, \
		FDFSMetaData **meta_list, int *meta_count);


/**
* download file from storage server
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*	file_id: the file id (including group name and filename)
*       file_offset: the start offset to download
*       download_bytes: download bytes, 0 means from start offset to the file end
*	callback: callback function
*	arg: callback extra arguement
*       file_size: return file size (bytes)
* return: 0 success, !=0 fail, return the error code
**/
int storage_download_file_ex1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id, \
		const int64_t file_offset, const int64_t download_bytes, \
		DownloadCallback callback, void *arg, int64_t *file_size);

/**
* query storage server to download file
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*	file_id: the file id (including group name and filename)
* return: 0 success, !=0 fail, return the error code
**/
int tracker_query_storage_fetch1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id);
		
/**
* query storage server to update (delete file and set metadata)
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*	file_id: the file id (including group name and filename)
* return: 0 success, !=0 fail, return the error code
**/
int tracker_query_storage_update1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		const char *file_id);

/**
* query storage server list to fetch file
* params:
*	pTrackerServer: tracker server
*	pStorageServer: return storage server
*       nMaxServerCount: max storage server count
*       server_count:  return storage server count
*       group_name: the group name of storage server
*       filename: filename on storage server
* return: 0 success, !=0 fail, return the error code
**/
int tracker_query_storage_list1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const int nMaxServerCount, \
		int *server_count, const char *file_id);

/**
* upload slave file to storage server (by file name)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       local_filename: local filename to upload
*       master_file_id: the mater file id to generate the slave file id
*       prefix_name: the prefix name to generate the file id
*       file_ext_name: file ext name, not include dot(.), 
*                      if be NULL will abstract ext name from the local filename
*	meta_list: meta info array
*       meta_count: meta item count
*	file_id: return the slave file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_upload_slave_by_filename1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const char *master_file_id, const char *prefix_name, \
		const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *file_id);

/**
* upload slave file to storage server (by file buff)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       file_buff: file content/buff
*       file_size: file size (bytes)
*       master_file_id: the mater file id to generate the slave file id
*       prefix_name: the prefix name to generate the file id
*       file_ext_name: file ext name, not include dot(.), can be NULL
*	meta_list: meta info array
*       meta_count: meta item count
*	file_id: return the slave file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_upload_slave_by_filebuff1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_size, const char *master_file_id, \
		const char *prefix_name, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *file_id);

/**
* upload slave file to storage server (by callback)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       callback: callback function to send file content to storage server
*       arg: callback extra arguement
*       file_size: the file size
*       master_file_id: the mater file id to generate the slave file id
*       prefix_name: the prefix name to generate the file id
*       file_ext_name: file ext name, not include dot(.), can be NULL
*	meta_list: meta info array
*       meta_count: meta item count
*	file_id: return the slave file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_upload_slave_by_callback1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, \
		const int64_t file_size, const char *master_file_id, \
		const char *prefix_name, const char *file_ext_name, \
		const FDFSMetaData *meta_list, const int meta_count, \
		char *file_id);

/**
* append file to storage server (by filename)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       local_filename: local filename to upload
*       appender_file_id: the appender file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_append_by_filename1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const char *appender_file_id);


/**
* append file to storage server (by file buff)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       file_buff: file content/buff
*       file_size: file size (bytes)
*       appender_file_id: the appender file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_append_by_filebuff1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_size, const char *appender_file_id);


/**
* append file to storage server (by callback)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       callback: callback function to send file content to storage server
*       arg: callback extra arguement
*       file_size: the file size
*       appender_file_id: the appender file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_append_by_callback1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, \
		const int64_t file_size, const char *appender_file_id);

/**
* modify file to storage server (by local filename)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       local_filename: local filename to upload
*       file_offset: the start offset to modify appender file
*       appender_file_id: the appender file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_modify_by_filename1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *local_filename,\
		const int64_t file_offset, const char *appender_file_id);


/**
* modify file to storage server (by callback)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       callback: callback function to send file content to storage server
*       arg: callback extra arguement
*       file_offset: the start offset to modify appender file
*       file_size: the file size
*       appender_file_id: the appender file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_modify_by_callback1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, \
		UploadCallback callback, void *arg, \
		const int64_t file_offset, const int64_t file_size, \
		const char *appender_file_id);


/**
* modify file to storage server (by file buff)
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       file_buff: file content/buff
*       file_offset: the start offset to modify appender file
*       file_size: file size (bytes)
*       appender_file_id: the appender file id
* return: 0 success, !=0 fail, return the error code
**/
int storage_modify_by_filebuff1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer, const char *file_buff, \
		const int64_t file_offset, const int64_t file_size, \
		const char *appender_file_id);


#define storage_query_file_info1(pTrackerServer, pStorageServer, file_id, \
		pFileInfo) \
	storage_query_file_info_ex1(pTrackerServer, pStorageServer, file_id, \
		pFileInfo, false)

/**
* query file info
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       file_id: the file id
*	pFileInfo: return the file info (file size and create timestamp)
*	bSilence: when this file not exist, do not log error on storage server
* return: 0 success, !=0 fail, return the error code
**/
int storage_query_file_info_ex1(ConnectionInfo *pTrackerServer, \
		ConnectionInfo *pStorageServer,  const char *file_id, \
		FDFSFileInfo *pFileInfo, const bool bSilence);

#define fdfs_get_file_info1(file_id, pFileInfo) \
	fdfs_get_file_info_ex1(file_id, true, pFileInfo)

/**
* get file info from the filename return by storage server
* params:
*       file_id: the file id return by storage server
*       get_from_server: if get slave file info from storage server
*       pFileInfo: return the file info
* return: 0 success, !=0 fail, return the error code
**/
int fdfs_get_file_info_ex1(const char *file_id, const bool get_from_server, \
		FDFSFileInfo *pFileInfo);

/**
* check if file exist
* params:
*       pTrackerServer: tracker server
*       pStorageServer: storage server
*       file_id: the file id return by storage server
* return: 0 file exist, !=0 not exist, return the error code
**/
int storage_file_exist1(ConnectionInfo *pTrackerServer, \
			ConnectionInfo *pStorageServer,  \
			const char *file_id);

/**
* regenerate normal filename for appender file
* Note: the appender file will change to normal file
* params:
*       pTrackerServer: the tracker server
*       pStorageServer: the storage server
*	    group_name: the group name 
*	    appender_file_id: the appender file id
*       file_id: regenerated file id return by storage server
* return: 0 success, !=0 fail, return the error code
**/
int storage_regenerate_appender_filename1(ConnectionInfo *pTrackerServer,
		ConnectionInfo *pStorageServer, const char *appender_file_id,
        char *new_file_id);

#ifdef __cplusplus
}
#endif

#endif

