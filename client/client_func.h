/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//client_func.h

#include "tracker_types.h"
#include "client_global.h"
#include "fastcommon/ini_file_reader.h"

#ifndef _CLIENT_FUNC_H_
#define _CLIENT_FUNC_H_

typedef struct {
    short file_type;
    bool get_from_server;
	time_t create_timestamp;
	int crc32;
	int source_id;   //source storage id
	int64_t file_size;
	char source_ip_addr[IP_ADDRESS_SIZE];  //source storage ip address
} FDFSFileInfo;

#ifdef __cplusplus
extern "C" {
#endif

#define fdfs_client_init(filename) \
	fdfs_client_init_ex((&g_tracker_group), filename)

#define fdfs_client_init_from_buffer(buffer) \
	fdfs_client_init_from_buffer_ex((&g_tracker_group), buffer)

#define fdfs_client_destroy() \
	fdfs_client_destroy_ex((&g_tracker_group))

/**
* client initial from config file
* params:
*       pTrackerGroup: tracker group
*	conf_filename: client config filename
* return: 0 success, !=0 fail, return the error code
**/
int fdfs_client_init_ex(TrackerServerGroup *pTrackerGroup, \
		const char *conf_filename);


/**
* client initial from buffer
* params:
*       pTrackerGroup: tracker group
*	conf_filename: client config filename
* return: 0 success, !=0 fail, return the error code
**/
int fdfs_client_init_from_buffer_ex(TrackerServerGroup *pTrackerGroup, \
		const char *buffer);

/**
* load tracker server group
* params:
*       pTrackerGroup: tracker group
*	conf_filename: tracker server group config filename
* return: 0 success, !=0 fail, return the error code
**/
int fdfs_load_tracker_group(TrackerServerGroup *pTrackerGroup, \
		const char *conf_filename);

/**
* load tracker server group
* params:
*       pTrackerGroup: tracker group
*	conf_filename: config filename
*       items: ini file items
*       nItemCount: ini file item count
* return: 0 success, !=0 fail, return the error code
**/
int fdfs_load_tracker_group_ex(TrackerServerGroup *pTrackerGroup, \
		const char *conf_filename, IniContext *pIniContext);

/**
* copy tracker server group
* params:
*       pDestTrackerGroup: the dest tracker group
*       pSrcTrackerGroup: the source tracker group
* return: 0 success, !=0 fail, return the error code
**/
int fdfs_copy_tracker_group(TrackerServerGroup *pDestTrackerGroup, \
		TrackerServerGroup *pSrcTrackerGroup);

/**
* client destroy function
* params:
*       pTrackerGroup: tracker group
* return: none
**/
void fdfs_client_destroy_ex(TrackerServerGroup *pTrackerGroup);

/**
* tracker group equals
* params:
*       pGroup1: tracker group 1
*       pGroup2: tracker group 2
* return: true for equals, otherwise false
**/
bool fdfs_tracker_group_equals(TrackerServerGroup *pGroup1, \
        TrackerServerGroup *pGroup2);

/**
* get file ext name from filename, extension name do not include dot
* params:
*       filename:  the filename
* return: file ext name, NULL for no ext name
**/
#define fdfs_get_file_ext_name1(filename) \
	fdfs_get_file_ext_name_ex(filename, false)

/**
* get file ext name from filename, extension name maybe include dot
* params:
*       filename:  the filename
* return: file ext name, NULL for no ext name
**/
#define fdfs_get_file_ext_name2(filename) \
	fdfs_get_file_ext_name_ex(filename, true)

#define fdfs_get_file_ext_name(filename) \
	fdfs_get_file_ext_name_ex(filename, true)

/**
* get file ext name from filename
* params:
*       filename:  the filename
*       twoExtName: two extension name as the extension name
* return: file ext name, NULL for no ext name
**/
const char *fdfs_get_file_ext_name_ex(const char *filename, 
	const bool twoExtName);

#ifdef __cplusplus
}
#endif

#endif
