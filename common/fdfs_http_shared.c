
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
#include "md5.h"
#include "shared_func.h"
#include "mime_file_parser.h"
#include "fdfs_global.h"
#include "fdfs_http_shared.h"

const char *fdfs_http_get_file_extension(const char *filename, \
		const int filename_len, int *ext_len)
{
	const char *pEnd;
	const char *pExtName;
	int i;

	pEnd = filename + filename_len;
	pExtName = pEnd - 1;
	for (i=0; i<FDFS_FILE_EXT_NAME_MAX_LEN && pExtName >= filename; \
		i++, pExtName--)
	{
		if (*pExtName == '.')
		{
			break;
		}
	}

	if (i < FDFS_FILE_EXT_NAME_MAX_LEN) //found
	{
		pExtName++;  //skip .
		*ext_len = pEnd - pExtName;
		return pExtName;
	}
	else
	{
		*ext_len = 0;
		return NULL;
	}
}

int fdfs_http_get_content_type_by_extname(FDFSHTTPParams *pParams, \
	const char *ext_name, const int ext_len, \
	char *content_type, const int content_type_size)
{
	HashData *pHashData;

	if (ext_len == 0)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"extension name is empty, " \
			"set to default content type: %s", \
			__LINE__, pParams->default_content_type);
		strcpy(content_type, pParams->default_content_type);
		return 0;
	}

	pHashData = hash_find_ex(&pParams->content_type_hash, \
				ext_name, ext_len + 1);
	if (pHashData == NULL)
	{
		logWarning("file: "__FILE__", line: %d, " \
			"extension name: %s is not supported, " \
			"set to default content type: %s", \
			__LINE__, ext_name, pParams->default_content_type);
		strcpy(content_type, pParams->default_content_type);
		return 0;
	}

	if (pHashData->value_len >= content_type_size)
	{
		*content_type = '\0';
		logError("file: "__FILE__", line: %d, " \
			"extension name: %s 's content type " \
			"is too long", __LINE__, ext_name);
		return EINVAL;
	}

	memcpy(content_type, pHashData->value, pHashData->value_len);
	return 0;
}

int fdfs_http_params_load(IniContext *pIniContext, \
		const char *conf_filename, FDFSHTTPParams *pParams)
{
	int result;
	int ext_len;
	const char *ext_name;
	char *mime_types_filename;
	char szMimeFilename[256];
	char *anti_steal_secret_key;
	char *token_check_fail_filename;
	char *default_content_type;
	int def_content_type_len;
	int64_t file_size;

	memset(pParams, 0, sizeof(FDFSHTTPParams));

	pParams->disabled = iniGetBoolValue(NULL, "http.disabled", \
					pIniContext, false);
	if (pParams->disabled)
	{
		return 0;
	}

	pParams->need_find_content_type = iniGetBoolValue(NULL, \
			"http.need_find_content_type", \
			pIniContext, true);

	pParams->server_port = iniGetIntValue(NULL, "http.server_port", \
					pIniContext, 80);
	if (pParams->server_port <= 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"invalid param \"http.server_port\": %d", \
			__LINE__, pParams->server_port);
		return EINVAL;
	}

	pParams->anti_steal_token = iniGetBoolValue(NULL, \
				"http.anti_steal.check_token", \
				pIniContext, false);
	if (pParams->need_find_content_type || pParams->anti_steal_token)
	{
	mime_types_filename = iniGetStrValue(NULL, "http.mime_types_filename", \
                                        pIniContext);
	if (mime_types_filename == NULL || *mime_types_filename == '\0')
	{
		logError("file: "__FILE__", line: %d, " \
			"param \"http.mime_types_filename\" not exist " \
			"or is empty", __LINE__);
		return EINVAL;
	}

	if (strncasecmp(mime_types_filename, "http://", 7) != 0 && \
		*mime_types_filename != '/' && \
		strncasecmp(conf_filename, "http://", 7) != 0)
	{
		char *pPathEnd;

		pPathEnd = strrchr(conf_filename, '/');
		if (pPathEnd == NULL)
		{
			snprintf(szMimeFilename, sizeof(szMimeFilename), \
					"%s", mime_types_filename);
		}
		else
		{
			int nPathLen;
			int nFilenameLen;

			nPathLen = (pPathEnd - conf_filename) + 1;
			nFilenameLen = strlen(mime_types_filename);
			if (nPathLen + nFilenameLen >= sizeof(szMimeFilename))
			{
				logError("file: "__FILE__", line: %d, " \
					"filename is too long, length %d >= %d",
					__LINE__, nPathLen + nFilenameLen, \
					(int)sizeof(szMimeFilename));
				return ENOSPC;
			}

			memcpy(szMimeFilename, conf_filename, nPathLen);
			memcpy(szMimeFilename + nPathLen, mime_types_filename, \
				nFilenameLen);
			*(szMimeFilename + nPathLen + nFilenameLen) = '\0';
		}
	}
	else
	{
		snprintf(szMimeFilename, sizeof(szMimeFilename), \
				"%s", mime_types_filename);
	}

	result = load_mime_types_from_file(&pParams->content_type_hash, \
				szMimeFilename);
	if (result != 0)
	{
		return result;
	}

	default_content_type = iniGetStrValue(NULL, \
			"http.default_content_type", \
			pIniContext);
	if (default_content_type == NULL || *default_content_type == '\0')
	{
		logError("file: "__FILE__", line: %d, " \
			"param \"http.default_content_type\" not exist " \
			"or is empty", __LINE__);
		return EINVAL;
	}

	def_content_type_len = strlen(default_content_type);
	if (def_content_type_len >= sizeof(pParams->default_content_type))
	{
		logError("file: "__FILE__", line: %d, " \
			"default content type: %s is too long", \
			__LINE__, default_content_type);
		return EINVAL;
	}
	memcpy(pParams->default_content_type, default_content_type, \
			def_content_type_len);
	}

	if (!pParams->anti_steal_token)
	{
		return 0;
	}

	pParams->token_ttl = iniGetIntValue(NULL, \
				"http.anti_steal.token_ttl", \
				pIniContext, 600);
	if (pParams->token_ttl <= 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"param \"http.anti_steal.token_ttl\" is invalid", \
			__LINE__);
		return EINVAL;
	}

	anti_steal_secret_key = iniGetStrValue(NULL, \
			"http.anti_steal.secret_key", \
			pIniContext);
	if (anti_steal_secret_key == NULL || *anti_steal_secret_key == '\0')
	{
		logError("file: "__FILE__", line: %d, " \
			"param \"http.anti_steal.secret_key\" not exist " \
			"or is empty", __LINE__);
		return EINVAL;
	}

	buffer_strcpy(&pParams->anti_steal_secret_key, anti_steal_secret_key);

	token_check_fail_filename = iniGetStrValue(NULL, \
			"http.anti_steal.token_check_fail", \
			pIniContext);
	if (token_check_fail_filename == NULL || \
		*token_check_fail_filename == '\0')
	{
		return 0;
	}

	if (!fileExists(token_check_fail_filename))
	{
		logError("file: "__FILE__", line: %d, " \
			"token_check_fail file: %s not exists", __LINE__, \
			token_check_fail_filename);
		return ENOENT;
	}

	ext_name = fdfs_http_get_file_extension(token_check_fail_filename, \
		strlen(token_check_fail_filename), &ext_len);
	if ((result=fdfs_http_get_content_type_by_extname(pParams, \
			ext_name, ext_len, \
			pParams->token_check_fail_content_type, \
			sizeof(pParams->token_check_fail_content_type))) != 0)
	{
		return result;
	}

	if (!pParams->need_find_content_type)
	{
		hash_destroy(&pParams->content_type_hash);
	}

	if ((result=getFileContent(token_check_fail_filename, \
		&pParams->token_check_fail_buff.buff, &file_size)) != 0)
	{
		return result;
	}

	pParams->token_check_fail_buff.alloc_size = file_size;
	pParams->token_check_fail_buff.length = file_size;

	return 0;
}

void fdfs_http_params_destroy(FDFSHTTPParams *pParams)
{
	if (pParams->need_find_content_type)
	{
		hash_destroy(&pParams->content_type_hash);
	}
}

int fdfs_http_gen_token(const BufferInfo *secret_key, const char *file_id, \
		const int timestamp, char *token)
{
	char buff[256 + 64];
	unsigned char digit[16];
	int id_len;
	int total_len;

	id_len = strlen(file_id);
	if (id_len + secret_key->length + 12 > sizeof(buff))
	{
		return ENOSPC;
	}

	memcpy(buff, file_id, id_len);
	total_len = id_len;
	memcpy(buff + total_len, secret_key->buff, secret_key->length);
	total_len += secret_key->length;
	total_len += sprintf(buff + total_len, "%d", timestamp);

	my_md5_buffer(buff, total_len, digit);
	bin2hex((char *)digit, 16, token);
	return 0;
}

int fdfs_http_check_token(const BufferInfo *secret_key, const char *file_id, \
		const int timestamp, const char *token, const int ttl)
{
	char true_token[33];
	int result;
	int token_len;

	token_len = strlen(token);
	if (token_len != 32)
	{
		return EINVAL;
	}

	if ((timestamp != 0) && (time(NULL) - timestamp > ttl))
	{
		return ETIMEDOUT;
	}

	if ((result=fdfs_http_gen_token(secret_key, file_id, \
			timestamp, true_token)) != 0)
	{
		return result;
	}

	return (memcmp(token, true_token, 32) == 0) ? 0 : EPERM;
}

char *fdfs_http_get_parameter(const char *param_name, KeyValuePair *params, \
		const int param_count)
{
	KeyValuePair *pCurrent;
	KeyValuePair *pEnd;

	pEnd = params + param_count;
	for (pCurrent=params; pCurrent<pEnd; pCurrent++)
	{
		if (strcmp(pCurrent->key, param_name) == 0)
		{
			return pCurrent->value;
		}
	}

	return NULL;
}

