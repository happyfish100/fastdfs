/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include "logger.h"
#include "sockopt.h"
#include "shared_func.h"
#include "local_ip_func.h"

int g_local_host_ip_count = 0;
char g_local_host_ip_addrs[FAST_MAX_LOCAL_IP_ADDRS * \
				IP_ADDRESS_SIZE];
char g_if_alias_prefix[FAST_IF_ALIAS_PREFIX_MAX_SIZE] = {0};

bool is_local_host_ip(const char *client_ip)
{
	char *p;
	char *pEnd;

	pEnd = g_local_host_ip_addrs + \
		IP_ADDRESS_SIZE * g_local_host_ip_count;
	for (p=g_local_host_ip_addrs; p<pEnd; p+=IP_ADDRESS_SIZE)
	{
		if (strcmp(client_ip, p) == 0)
		{
			return true;
		}
	}

	return false;
}

int insert_into_local_host_ip(const char *client_ip)
{
	if (is_local_host_ip(client_ip))
	{
		return 0;
	}

	if (g_local_host_ip_count >= FAST_MAX_LOCAL_IP_ADDRS)
	{
		return -1;
	}

	strcpy(g_local_host_ip_addrs + \
		IP_ADDRESS_SIZE * g_local_host_ip_count, \
		client_ip);
	g_local_host_ip_count++;
	return 1;
}

static void log_local_host_ip_addrs()
{
	char *p;
	char *pEnd;
	char buff[512];
	int len;

	len = sprintf(buff, "local_host_ip_count: %d,", g_local_host_ip_count);
	pEnd = g_local_host_ip_addrs + \
		IP_ADDRESS_SIZE * g_local_host_ip_count;
	for (p=g_local_host_ip_addrs; p<pEnd; p+=IP_ADDRESS_SIZE)
	{
		len += sprintf(buff + len, "  %s", p);
	}

	logInfo("%s", buff);
}

void load_local_host_ip_addrs()
{
#define STORAGE_MAX_ALIAS_PREFIX_COUNT   4
	char ip_addresses[FAST_MAX_LOCAL_IP_ADDRS][IP_ADDRESS_SIZE];
	int count;
	int k;
	char *if_alias_prefixes[STORAGE_MAX_ALIAS_PREFIX_COUNT];
	int alias_count;

	insert_into_local_host_ip("127.0.0.1");

	memset(if_alias_prefixes, 0, sizeof(if_alias_prefixes));
	if (*g_if_alias_prefix == '\0')
	{
		alias_count = 0;
	}
	else
	{
		alias_count = splitEx(g_if_alias_prefix, ',', \
			if_alias_prefixes, STORAGE_MAX_ALIAS_PREFIX_COUNT);
		for (k=0; k<alias_count; k++)
		{
			trim(if_alias_prefixes[k]);
		}
	}

	if (gethostaddrs(if_alias_prefixes, alias_count, ip_addresses, \
			FAST_MAX_LOCAL_IP_ADDRS, &count) != 0)
	{
		return;
	}

	for (k=0; k<count; k++)
	{
		insert_into_local_host_ip(ip_addresses[k]);
	}

	log_local_host_ip_addrs();
	//print_local_host_ip_addrs();
}

void print_local_host_ip_addrs()
{
	char *p;
	char *pEnd;

	printf("local_host_ip_count=%d\n", g_local_host_ip_count);
	pEnd = g_local_host_ip_addrs + \
		IP_ADDRESS_SIZE * g_local_host_ip_count;
	for (p=g_local_host_ip_addrs; p<pEnd; p+=IP_ADDRESS_SIZE)
	{
		printf("%d. %s\n", (int)((p-g_local_host_ip_addrs)/ \
				IP_ADDRESS_SIZE)+1, p);
	}

	printf("\n");
}

