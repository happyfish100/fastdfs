
/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "logger.h"
#include "http_func.h"
#include "shared_func.h"
#include "mime_file_parser.h"

int load_mime_types_from_file(HashArray *pHash, const char *mime_filename)
{
#define MIME_DELIM_CHARS  " \t"

	int result;
	char *content;
	char *pLine;
	char *pLastEnd;
	char *content_type;
	char *ext_name;
	char *lasts;
	int http_status;
	int content_len;
	int64_t file_size;
	char error_info[512];

	if (strncasecmp(mime_filename, "http://", 7) == 0)
	{
		if ((result=get_url_content(mime_filename, 30, 60, &http_status,\
				&content, &content_len, error_info)) != 0)
		{
			logError("file: "__FILE__", line: %d, " \
				"get_url_content fail, " \
				"url: %s, error info: %s", \
				__LINE__, mime_filename, error_info);
			return result;
		}

		if (http_status != 200)
		{
			free(content);
			logError("file: "__FILE__", line: %d, " \
				"HTTP status code: %d != 200, url: %s", \
				__LINE__, http_status, mime_filename);
			return EINVAL;
		}
	}
	else
	{
		if ((result=getFileContent(mime_filename, &content, \
				&file_size)) != 0)
		{
			return result;
		}
	}

	if ((result=hash_init_ex(pHash, PJWHash, 2 * 1024, 0.75, 0, true)) != 0)
	{
		free(content);
		logError("file: "__FILE__", line: %d, " \
			"hash_init_ex fail, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	pLastEnd = content - 1;
	while (pLastEnd != NULL)
	{
		pLine = pLastEnd + 1;
		pLastEnd = strchr(pLine, '\n');
		if (pLastEnd != NULL)
		{
			*pLastEnd = '\0';
		}

		if (*pLine == '\0' || *pLine == '#')
		{
			continue;
		}

		lasts = NULL;
		content_type = strtok_r(pLine, MIME_DELIM_CHARS, &lasts);
		while (1)
		{
			ext_name = strtok_r(NULL, MIME_DELIM_CHARS, &lasts);
			if (ext_name == NULL)
			{
				break;
			}

			if (*ext_name == '\0')
			{
				continue;
			}

			if ((result=hash_insert_ex(pHash, ext_name, \
				strlen(ext_name)+1, content_type, \
				strlen(content_type)+1, true)) < 0)
			{
				free(content);
				result *= -1;
				logError("file: "__FILE__", line: %d, " \
					"hash_insert_ex fail, errno: %d, " \
					"error info: %s", __LINE__, \
					result, STRERROR(result));
				return result;
			}
		}
	}

	free(content);

	//hash_stat_print(pHash);
	return 0;
}

