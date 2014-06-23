
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
#include "sockopt.h"
#include "logger.h"
#include "shared_func.h"

int get_url_content(const char *url, const int connect_timeout, \
	const int network_timeout, int *http_status, \
	char **content, int *content_len, char *error_info)
{
	char domain_name[256];
	char ip_addr[IP_ADDRESS_SIZE];
	char out_buff[4096];
	int domain_len;
	int url_len;
	int out_len;
	int alloc_size;
	int recv_bytes;
	int result;
	int sock;
	int port;
	const char *pDomain;
	const char *pContent;
	const char *pURI;
	char *pPort;
	char *pSpace;

	*http_status = 0;
	*content_len = 0;
	*content = NULL;

	url_len = strlen(url);
	if (url_len <= 7 || strncasecmp(url, "http://", 7) != 0)
	{
		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"invalid url.", __LINE__);
		return EINVAL;
	}

	pDomain = url + 7;
	pURI = strchr(pDomain, '/');
	if (pURI == NULL)
	{
		domain_len = url_len - 7;
		pURI = "/";
	}
	else
	{
		domain_len = pURI - pDomain;
	}

	if (domain_len >= sizeof(domain_name))
	{
		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"domain is too large, exceed %d.", \
			__LINE__, (int)sizeof(domain_name));
		return EINVAL;
	}

	memcpy(domain_name, pDomain, domain_len);
	*(domain_name + domain_len) = '\0';
	pPort = strchr(domain_name, ':');
	if (pPort == NULL)
	{
		port = 80;
	}
	else
	{
		*pPort = '\0';
		port = atoi(pPort + 1);
	}

	if (getIpaddrByName(domain_name, ip_addr, \
		sizeof(ip_addr)) == INADDR_NONE)
	{
		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"resolve domain \"%s\" fail.", \
			__LINE__, domain_name);
		return EINVAL;
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"socket create failed, errno: %d, " \
			"error info: %s", __LINE__, \
			errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	if ((result=connectserverbyip_nb_auto(sock, ip_addr, port, \
			connect_timeout)) != 0)
	{
		close(sock);

		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"connect to %s:%d fail, errno: %d, " \
			"error info: %s", __LINE__, domain_name, \
			port, result, STRERROR(result));

		return result;
	}

	out_len = snprintf(out_buff, sizeof(out_buff), \
		"GET %s HTTP/1.0\r\n" \
		"Host: %s:%d\r\n" \
		"Connection: close\r\n" \
		"\r\n", pURI, domain_name, port);
	if ((result=tcpsenddata(sock, out_buff, out_len, network_timeout)) != 0)
	{
		close(sock);

		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"send data to %s:%d fail, errno: %d, " \
			"error info: %s", __LINE__, domain_name, \
			port, result, STRERROR(result));

		return result;
	}

	alloc_size = 64 * 1024;
	*content = (char *)malloc(alloc_size + 1);
	if (*content == NULL)
	{
		close(sock);
		result = errno != 0 ? errno : ENOMEM;

		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, errno: %d, " \
			"error info: %s", __LINE__, alloc_size + 1, \
			result, STRERROR(result));

		return result;
	}

	do
	{
		recv_bytes = alloc_size - *content_len;
		if (recv_bytes <= 0)
		{
			alloc_size *= 2;
			*content = (char *)realloc(*content, alloc_size + 1);
			if (*content == NULL)
			{
				close(sock);
				result = errno != 0 ? errno : ENOMEM;

				sprintf(error_info, "file: "__FILE__", line: %d, " \
					"realloc %d bytes fail, errno: %d, " \
					"error info: %s", __LINE__, \
					alloc_size + 1, \
					result, STRERROR(result));

				return result;
			}

			recv_bytes = alloc_size - *content_len;
		}

		result = tcprecvdata_ex(sock, *content + *content_len, \
				recv_bytes, network_timeout, &recv_bytes);

		*content_len += recv_bytes;
	} while (result == 0);

	if (result != ENOTCONN)
	{
		close(sock);
		free(*content);
		*content = NULL;
		*content_len = 0;

		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"recv data from %s:%d fail, errno: %d, " \
			"error info: %s", __LINE__, domain_name, \
			port, result, STRERROR(result));

		return result;
	}

	*(*content + *content_len) = '\0';
	pContent = strstr(*content, "\r\n\r\n");
	if (pContent == NULL)
	{
		close(sock);
		free(*content);
		*content = NULL;
		*content_len = 0;

		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"response data from %s:%d is invalid", \
			__LINE__, domain_name, port);

		return EINVAL;
	}

	pContent += 4;
	pSpace = strchr(*content, ' ');
	if (pSpace == NULL || pSpace >= pContent)
	{
		close(sock);
		free(*content);
		*content = NULL;
		*content_len = 0;

		sprintf(error_info, "file: "__FILE__", line: %d, " \
			"response data from %s:%d is invalid", \
			__LINE__, domain_name, port);

		return EINVAL;
	}

	*http_status = atoi(pSpace + 1);
	*content_len -= pContent - *content;
	memcpy(*content, pContent, *content_len);
	*(*content + *content_len) = '\0';

	close(sock);

	*error_info = '\0';
	return 0;
}

int http_parse_query(char *url, KeyValuePair *params, const int max_count)
{
	KeyValuePair *pCurrent;
	KeyValuePair *pEnd;
	char *pParamStart;
	char *p;
	char *pStrEnd;
	int value_len;

	pParamStart = strchr(url, '?');
	if (pParamStart == NULL)
	{
		return 0;
	}

	*pParamStart = '\0';

	pEnd = params + max_count;
	pCurrent = params;
	p = pParamStart + 1;
	while (p != NULL && *p != '\0')
	{
		if (pCurrent >= pEnd)
		{
			return pCurrent - params;
		}

		pCurrent->key = p;
		pStrEnd = strchr(p, '&');
		if (pStrEnd == NULL)
		{
			p = NULL;
		}
		else
		{
			*pStrEnd = '\0';
			p = pStrEnd + 1;
		}

		pStrEnd = strchr(pCurrent->key, '=');
		if (pStrEnd == NULL)
		{
			continue;
		}

		*pStrEnd = '\0';
		pCurrent->value = pStrEnd + 1;
		if (*pCurrent->key == '\0')
		{
			continue;
		}

		urldecode(pCurrent->value, strlen(pCurrent->value), \
			pCurrent->value, &value_len);
		pCurrent++;
	}

	return pCurrent - params;
}

