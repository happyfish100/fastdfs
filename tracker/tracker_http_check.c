#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "logger.h"
#include "fdfs_global.h"
#include "tracker_global.h"
#include "tracker_mem.h"
#include "tracker_proto.h"
#include "http_func.h"
#include "sockopt.h"
#include "tracker_http_check.h"

static pthread_t http_check_tid;
bool g_http_check_flag = false;

static void *http_check_entrance(void *arg)
{
	FDFSGroupInfo **ppGroup;
	FDFSGroupInfo **ppGroupEnd;
	FDFSStorageDetail **ppServer;
	FDFSStorageDetail **ppServerEnd;
	char url[512];
	char error_info[512];
	char *content;
	int content_len;
	int http_status;
	int sock;
	int server_count;
	int result;

	g_http_check_flag = true;
	g_http_servers_dirty = false;
	while (g_continue_flag)
	{
	if (g_http_servers_dirty)
	{
		g_http_servers_dirty = false;
	}
	else
	{
		sleep(g_http_check_interval);
	}

	ppGroupEnd = g_groups.groups + g_groups.count;
	for (ppGroup=g_groups.groups; g_continue_flag && (!g_http_servers_dirty)\
		&& ppGroup<ppGroupEnd; ppGroup++)
        {

	if ((*ppGroup)->storage_http_port <= 0)
	{
		continue;
	}

	server_count = 0;
	ppServerEnd = (*ppGroup)->active_servers + (*ppGroup)->active_count;
	for (ppServer=(*ppGroup)->active_servers; g_continue_flag && \
		(!g_http_servers_dirty) && ppServer<ppServerEnd; ppServer++)
	{
		if (g_http_check_type == FDFS_HTTP_CHECK_ALIVE_TYPE_TCP)
		{
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if(sock < 0)
			{
				result = errno != 0 ? errno : EPERM;
				logError("file: "__FILE__", line: %d, " \
					"socket create failed, errno: %d, " \
					"error info: %s.", \
					__LINE__, result, STRERROR(result));
				sleep(1);
				continue;
			}

			result = connectserverbyip_nb_auto(sock, \
					(*ppServer)->ip_addr, \
					(*ppGroup)->storage_http_port, \
					g_fdfs_connect_timeout);
			close(sock);

			if (g_http_servers_dirty)
			{
				break;
			}

			if (result == 0)
			{
				*((*ppGroup)->http_servers+server_count)=*ppServer;
				server_count++;
				if ((*ppServer)->http_check_fail_count > 0)
				{
					logInfo("file: "__FILE__", line: %d, " \
						"http check alive success " \
						"after %d times, server: %s:%d",
						__LINE__, \
						(*ppServer)->http_check_fail_count, 
						(*ppServer)->ip_addr, \
						(*ppGroup)->storage_http_port);
					(*ppServer)->http_check_fail_count = 0;
				}
			}
			else
			{
				if (result != (*ppServer)->http_check_last_errno)
				{
				if ((*ppServer)->http_check_fail_count > 1)
				{
					logError("file: "__FILE__", line: %d, "\
						"http check alive fail after " \
						"%d times, storage server: " \
						"%s:%d, error info: %s", \
						__LINE__, \
						(*ppServer)->http_check_fail_count, \
						(*ppServer)->ip_addr, \
						(*ppGroup)->storage_http_port, \
						(*ppServer)->http_check_error_info);
				}

				sprintf((*ppServer)->http_check_error_info, 
					"http check alive, connect to http " \
					"server %s:%d fail, " \
					"errno: %d, error info: %s", \
					(*ppServer)->ip_addr, \
					(*ppGroup)->storage_http_port, result, \
					STRERROR(result));

				logError("file: "__FILE__", line: %d, %s", \
					__LINE__, \
					(*ppServer)->http_check_error_info);
				(*ppServer)->http_check_last_errno = result;
				(*ppServer)->http_check_fail_count = 1;
				}
				else
				{
					(*ppServer)->http_check_fail_count++;
				}
			}
		}
		else  //http
		{
		sprintf(url, "http://%s:%d%s", (*ppServer)->ip_addr, \
			(*ppGroup)->storage_http_port, g_http_check_uri);

		result = get_url_content(url, g_fdfs_connect_timeout, \
				g_fdfs_network_timeout, &http_status, \
        			&content, &content_len, error_info);

		if (g_http_servers_dirty)
		{
			if (result == 0)
			{
				free(content);
			}

			break;
		}

		if (result == 0)
		{
			if (http_status == 200)
			{
				*((*ppGroup)->http_servers+server_count)=*ppServer;
				server_count++;

				if ((*ppServer)->http_check_fail_count > 0)
				{
					logInfo("file: "__FILE__", line: %d, " \
						"http check alive success " \
						"after %d times, url: %s",\
						__LINE__, \
						(*ppServer)->http_check_fail_count, 
						url);
					(*ppServer)->http_check_fail_count = 0;
				}
			}
			else
			{
			if (http_status != (*ppServer)->http_check_last_status)
			{
				if ((*ppServer)->http_check_fail_count > 1)
				{
					logError("file: "__FILE__", line: %d, "\
						"http check alive fail after " \
						"%d times, storage server: " \
						"%s:%d, error info: %s", \
						__LINE__, \
						(*ppServer)->http_check_fail_count, \
						(*ppServer)->ip_addr, \
						(*ppGroup)->storage_http_port, \
						(*ppServer)->http_check_error_info);
				}

				sprintf((*ppServer)->http_check_error_info, \
					"http check alive fail, url: %s, " \
					"http_status=%d", url, http_status);

				logError("file: "__FILE__", line: %d, %s", \
					__LINE__, \
					(*ppServer)->http_check_error_info);
				(*ppServer)->http_check_last_status = http_status;
				(*ppServer)->http_check_fail_count = 1;
			}
			else
			{
				(*ppServer)->http_check_fail_count++;
			}
			}

			free(content);
		}
		else
		{
			if (result != (*ppServer)->http_check_last_errno)
			{
				if ((*ppServer)->http_check_fail_count > 1)
				{
					logError("file: "__FILE__", line: %d, "\
						"http check alive fail after " \
						"%d times, storage server: " \
						"%s:%d, error info: %s", \
						__LINE__, \
						(*ppServer)->http_check_fail_count, \
						(*ppServer)->ip_addr, \
						(*ppGroup)->storage_http_port, \
						(*ppServer)->http_check_error_info);
				}

				sprintf((*ppServer)->http_check_error_info, \
					"http check alive fail, " \
					"error info: %s", error_info);

				logError("file: "__FILE__", line: %d, %s", \
					__LINE__, \
					(*ppServer)->http_check_error_info);
				(*ppServer)->http_check_last_errno = result;
				(*ppServer)->http_check_fail_count = 1;
			}
			else
			{
				(*ppServer)->http_check_fail_count++;
			}
		}
		}
	}

	if (g_http_servers_dirty)
	{
		break;
	}

	if ((*ppGroup)->http_server_count != server_count)
	{
		logDebug("file: "__FILE__", line: %d, " \
			"group: %s, HTTP server count change from %d to %d", \
			__LINE__, (*ppGroup)->group_name, \
			(*ppGroup)->http_server_count, server_count);

		(*ppGroup)->http_server_count = server_count;
	}
	}
	}

	ppGroupEnd = g_groups.groups + g_groups.count;
	for (ppGroup=g_groups.groups; ppGroup<ppGroupEnd; ppGroup++)
	{
	ppServerEnd = (*ppGroup)->all_servers + (*ppGroup)->count;
	for (ppServer=(*ppGroup)->all_servers; ppServer<ppServerEnd; ppServer++)
	{
		if ((*ppServer)->http_check_fail_count > 1)
		{
			logError("file: "__FILE__", line: %d, " \
				"http check alive fail " \
				"after %d times, storage server: %s:%d, " \
				"error info: %s", \
				__LINE__, (*ppServer)->http_check_fail_count, \
				(*ppServer)->ip_addr, \
				(*ppGroup)->storage_http_port,\
				(*ppServer)->http_check_error_info);
		}
	}
	}

	g_http_check_flag = false;
	return NULL;
}

int tracker_http_check_start()
{
	int result;

	if (g_http_check_interval <= 0)
	{
		return 0;
	}

	if ((result=pthread_create(&http_check_tid, NULL, \
			http_check_entrance, NULL)) != 0)
	{
		logCrit("file: "__FILE__", line: %d, " \
			"create thread failed, errno: %d, error info: %s", \
			__LINE__, result, STRERROR(result));
		return result;
	}

	return 0;
}

int tracker_http_check_stop()
{
	if (g_http_check_interval <= 0)
	{
		return 0;
	}

	return pthread_kill(http_check_tid, SIGINT);
}

