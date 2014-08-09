#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common_define.h"
#include "test_types.h"
#include "common_func.h"

static int proccess_count;

static int combine_stat_overall(int *ptotal_count, int *psuccess_count, int *ptime_used);
static int combine_stat_by(const char *file_prefix, EntryStat *stats, const int max_entries, int *entry_count);
static void print_stat_by(EntryStat *stats, const int entry_count);

int main(int argc, char **argv)
{
	EntryStat stats[FILE_TYPE_COUNT];
	int entry_count;
	int time_used;
	int total_count;
	int success_count;
	int i;
	int bytes;
	int64_t total_bytes;

	if (argc < 2)
	{
		printf("Usage: %s <process_count>\n", argv[0]);
		return EINVAL;
	}

	proccess_count = atoi(argv[1]);
	if (proccess_count <= 0)
	{
		printf("Invalid proccess count: %d\n", proccess_count);
		return EINVAL;
	}

	total_count = 0;
	success_count = 0;
	time_used = 0;
	combine_stat_overall(&total_count, &success_count, &time_used);
	printf("total_count=%d, success_count=%d, success ratio: %.2f%% time_used=%ds, avg time used: %dms, QPS=%.2f\n\n", 
		total_count, success_count, total_count > 0 ? 100.00 * success_count / total_count : 0.00, 
		time_used, total_count > 0 ? time_used * 1000 / total_count : 0, 
		time_used == 0 ? 0 : (double)success_count / time_used);

	if (combine_stat_by(STAT_FILENAME_BY_FILE_TYPE, stats, FILE_TYPE_COUNT, &entry_count) == 0)
	{
		printf("file_type total_count success_count time_used(s) avg(ms) QPS success_ratio\n");
		print_stat_by(stats, entry_count);
		printf("\n");
	}

	total_bytes = 0;
	for (i=0; i<entry_count; i++)
	{
		if (strcmp(stats[i].id, "5K") == 0)
		{
			bytes = 5 * 1024;
		}
		else if (strcmp(stats[i].id, "50K") == 0)
		{
			bytes = 50 * 1024;
		}
		else if (strcmp(stats[i].id, "200K") == 0)
		{
			bytes = 200 * 1024;
		}
		else if (strcmp(stats[i].id, "1M") == 0)
		{
			bytes = 1 * 1024 * 1024;
		}
		else if (strcmp(stats[i].id, "10M") == 0)
		{
			bytes = 10 * 1024 * 1024;
		}
		else if (strcmp(stats[i].id, "100M") == 0)
		{
			bytes = 100 * 1024 * 1024;
		}
		else
		{
			bytes = 0;
		}

		total_bytes += (int64_t)bytes * stats[i].success_count;
	}
	if (time_used > 0)
	{
		printf("IO speed = %d KB\n", (int)(total_bytes / (time_used * 1024)));
	}

	if (combine_stat_by(STAT_FILENAME_BY_STORAGE_IP, stats, FILE_TYPE_COUNT, &entry_count) == 0)
	{
		printf("ip_addr  total_count success_count time_used(s) avg(ms) QPS success_ratio\n");
		print_stat_by(stats, entry_count);
		printf("\n");
	}

	return 0;
}

static void print_stat_by(EntryStat *stats, const int entry_count)
{
	EntryStat *pEntry;
	EntryStat *pEnd;
	int seconds;

	pEnd = stats + entry_count;
	for (pEntry=stats; pEntry<pEnd; pEntry++)
	{
		seconds = pEntry->time_used / 1000;
		printf("%s %d %d %d %d %.2f %.2f\n", pEntry->id, pEntry->total_count, 
			pEntry->success_count, (int)(pEntry->time_used / 1000), 
			pEntry->total_count == 0 ? 0 : (int)(pEntry->time_used / pEntry->total_count), 
			seconds == 0 ? 0 : (double)pEntry->success_count / seconds, 
			pEntry->total_count > 0 ? 100.00 * pEntry->success_count / pEntry->total_count : 0.00);
	}
}

static int combine_stat_by(const char *file_prefix, EntryStat *stats, const int max_entries, int *entry_count)
{
	char filename[64];
	FILE *fp;
	int proccess_index;
	char buff[256];
	char id[64];
	int64_t time_used;
	int total_count;
	int success_count;
	EntryStat *pEntry;
	EntryStat *pEnd;

	*entry_count = 0;
	memset(stats, 0, sizeof(EntryStat) * max_entries);
	for (proccess_index=0; proccess_index<proccess_count; proccess_index++)
	{
		sprintf(filename, "%s.%d", file_prefix, proccess_index);
		if ((fp=fopen(filename, "r")) == NULL)
		{
			printf("open file %s fail, errno: %d, error info: %s\n", 
				filename, errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}

		while (fgets(buff, sizeof(buff), fp) != NULL)
		{
			if (*buff == '#')
			{
				continue;
			}

			if (sscanf(buff, "%s %d %d %"PRId64, id, \
				&total_count, &success_count, &time_used) != 4)
			{
				if (*buff == ' ') //empty id (eg. storage ip)
				{
					*id = '\0';
					if (sscanf(buff+1, "%d %d %"PRId64, \
					&total_count, &success_count, &time_used) != 3)
					{
						printf("sscanf %s fail, errno: %d, error info: %s\n", 
							filename, errno, STRERROR(errno));
		                fclose(fp);
						return errno != 0 ? errno : EINVAL;
					}
				}
				else
				{
					printf("sscanf %s fail, errno: %d, error info: %s\n", 
						filename, errno, STRERROR(errno));
		            fclose(fp);
					return errno != 0 ? errno : EINVAL;
				}
			}

			pEnd = stats + (*entry_count);
			for (pEntry=stats; pEntry<pEnd; pEntry++)
			{
				if (strcmp(id, pEntry->id) == 0)
				{
					break;
				}
			}

			if (pEntry == pEnd) //not found
			{
				if (*entry_count >= max_entries)
				{
					printf("entry count: %d >= max entries: %d\n", *entry_count, max_entries);
		            fclose(fp);
					return ENOSPC;
				}

				strcpy(pEntry->id, id);
				(*entry_count)++;
			}

			pEntry->total_count += total_count;
			pEntry->success_count += success_count;
			pEntry->time_used += time_used;
		}

		fclose(fp);
	}

	pEnd = stats + (*entry_count);
	for (pEntry=stats; pEntry<pEnd; pEntry++)
	{
		pEntry->time_used /= proccess_count;
	}

	return 0;
}

static int combine_stat_overall(int *ptotal_count, int *psuccess_count, int *ptime_used)
{
	char filename[64];
	FILE *fp;
	int proccess_index;
	char buff[256];
	int time_used;
	int total_count;
	int success_count;

	*ptotal_count = 0;
	*psuccess_count = 0; 
	*ptime_used = 0;

	for (proccess_index=0; proccess_index<proccess_count; proccess_index++)
	{
		sprintf(filename, "%s.%d", STAT_FILENAME_BY_OVERALL, proccess_index);
		if ((fp=fopen(filename, "r")) == NULL)
		{
			printf("open file %s fail, errno: %d, error info: %s\n", 
				filename, errno, STRERROR(errno));
			return errno != 0 ? errno : EPERM;
		}

		while (fgets(buff, sizeof(buff), fp) != NULL)
		{
			if (*buff == '#')
			{
				continue;
			}

			if (sscanf(buff, "%d %d %d", &total_count, &success_count, &time_used) != 3)
			{
				printf("sscanf %s fail, errno: %d, error info: %s\n", 
					filename, errno, STRERROR(errno));
				return errno != 0 ? errno : EINVAL;
			}

			break;
		}

		*ptotal_count += total_count;
		*psuccess_count += success_count; 
		*ptime_used += time_used;
		fclose(fp);
	}

	*ptime_used /= proccess_count;

	return 0;
}

