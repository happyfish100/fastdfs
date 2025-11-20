//dfs_func.h

#ifndef _DFS_FUNC_H
#define _DFS_FUNC_H

#include "fastdfs/fdfs_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
*init function
* param proccess_index the process index based 0
* param conf_filename the config filename
* return 0 if success, none zero for error
*/
int dfs_init(const int proccess_index, const char *conf_filename);

/*
*destroy function
* return void
*/
void dfs_destroy();

/*
* upload file to the storage server
* param file_buff the file content
* param file_size the file size (bytes)
* param file_id return the file id (max length 63)
* param storage_ip return the storage server ip address (max length 15)
* return 0 if success, none zero for error
*/
int upload_file(const char *file_buff, const int file_size, char *file_id, char *storage_ip);

/*
* download file from the storage server
* param file_id the file id
* param file_size return the file size (bytes)
* param storage_ip return the storage server ip address (max length 15)
* return 0 if success, none zero for error
*/
int download_file(const char *file_id, int *file_size, char *storage_ip);

/*
* delete file from the storage server
* param file_id the file id
* param storage_ip return the storage server ip address (max length 15)
* return 0 if success, none zero for error
*/
int delete_file(const char *file_id, char *storage_ip);

/*
* upload appender file to the storage server
* param file_buff the file content
* param file_size the file size (bytes)
* param file_ext_name the file extension name
* param meta_list the metadata list
* param meta_count the metadata count
* param group_name return the group name
* param file_id return the file id (max length 63)
* param storage_ip return the storage server ip address (max length 15)
* return 0 if success, none zero for error
*/
int upload_appender_file_by_buff(const char *file_buff, const int file_size,
	const char *file_ext_name, const FDFSMetaData *meta_list,
	const int meta_count, char *group_name, char *file_id, char *storage_ip);

/*
* append file content to appender file
* param append_buff the content to append
* param append_size the append size (bytes)
* param group_name the group name
* param appender_file_id the appender file id
* param storage_ip return the storage server ip address (max length 15)
* return 0 if success, none zero for error
*/
int append_file_by_buff(const char *append_buff, const int append_size,
	const char *group_name, const char *appender_file_id, char *storage_ip);

#ifdef __cplusplus
}
#endif

#endif
