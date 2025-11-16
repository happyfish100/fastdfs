/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include "fastcommon/logger.h"
#include "fdfs_global.h"

Version g_fdfs_version = {6, 15, 2};
bool g_use_connection_pool = false;
ConnectionPool g_connection_pool;
int g_connection_pool_max_idle_time = 3600;
struct base64_context g_fdfs_base64_context;

/*
data filename format:
HH/HH/filename: HH for 2 uppercase hex chars
*/
int fdfs_check_data_filename(const char *filename, const int len)
{
	if (len < 6)
	{
		logError("file: "__FILE__", line: %d, " \
			"the length=%d of filename \"%s\" is too short", \
			__LINE__, len, filename);
		return EINVAL;
	}

	if (!IS_UPPER_HEX(*filename) || !IS_UPPER_HEX(*(filename+1)) || \
	    *(filename+2) != '/' || \
	    !IS_UPPER_HEX(*(filename+3)) || !IS_UPPER_HEX(*(filename+4)) || \
	    *(filename+5) != '/')
	{
		logError("file: "__FILE__", line: %d, " \
			"the format of filename \"%s\" is invalid", \
			__LINE__, filename);
		return EINVAL;
	}

	if (strchr(filename + 6, '/') != NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"the format of filename \"%s\" is invalid", \
			__LINE__, filename);
		return EINVAL;
	}

	return 0;
}

int fdfs_gen_slave_filename(const char *master_filename, \
		const char *prefix_name, const char *ext_name, \
		char *filename, int *filename_len)
{
	char true_ext_name[FDFS_FILE_EXT_NAME_MAX_LEN + 2];
	char *pDot;
	int master_file_len;
    int prefix_name_len;
    int true_ext_name_len;

	master_file_len = strlen(master_filename);
	if (master_file_len < 28 + FDFS_FILE_EXT_NAME_MAX_LEN)
	{
		logError("file: "__FILE__", line: %d, " \
			"master filename \"%s\" is invalid", \
			__LINE__, master_filename);
		return EINVAL;
	}

	pDot = strchr(master_filename + (master_file_len - \
			(FDFS_FILE_EXT_NAME_MAX_LEN + 1)), '.');
	if (ext_name != NULL)
	{
		if (*ext_name == '\0')
		{
			*true_ext_name = '\0';
		}
		else if (*ext_name == '.')
		{
			fc_safe_strcpy(true_ext_name, ext_name);
		}
		else
        {
            *true_ext_name = '.';
            fc_strlcpy(true_ext_name + 1, ext_name,
                    sizeof(true_ext_name) - 1);
        }
	}
	else
	{
		if (pDot == NULL)
		{
			*true_ext_name = '\0';
		}
		else
		{
			strcpy(true_ext_name, pDot);
		}
	}

	if (*true_ext_name == '\0' && strcmp(prefix_name, "-m") == 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"prefix_name \"%s\" is invalid", \
			__LINE__, prefix_name);
		return EINVAL;
	}

	/* when prefix_name is empty, the extension name of master file and 
	   slave file can not be same
	*/
	if ((*prefix_name == '\0') && ((pDot == NULL && *true_ext_name == '\0')
		 || (pDot != NULL && strcmp(pDot, true_ext_name) == 0)))
	{
		logError("file: "__FILE__", line: %d, " \
			"empty prefix_name is not allowed", __LINE__);
		return EINVAL;
	}

	if (pDot == NULL)
	{
        *filename_len = master_file_len;
	}
	else
	{
		*filename_len = pDot - master_filename;
    }
    memcpy(filename, master_filename, *filename_len);

    prefix_name_len = strlen(prefix_name);
    memcpy(filename + *filename_len, prefix_name, prefix_name_len);
    *filename_len += prefix_name_len;

    true_ext_name_len = strlen(true_ext_name);
    memcpy(filename + *filename_len, true_ext_name, true_ext_name_len);
    *filename_len += true_ext_name_len;
    *(filename + *filename_len) = '\0';

	return 0;
}
