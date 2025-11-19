#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "fastcommon/common_define.h"
#include "fastcommon/logger.h"
#include "test_types.h"
#include "common_func.h"
#include "dfs_func.h"

#define PROCESS_COUNT	1

typedef struct {
	int bytes;  // append size
	char *description;
	int count;   // total append count
	int append_count;
	int success_count;  // success append count
	int64_t time_used;  // unit: ms
	char *append_buff; // append content
} TestAppendInfo;

#ifdef DEBUG  // for debug

static TestAppendInfo appends[FILE_TYPE_COUNT] = {
	{1 * 1024, "1K",        100 / PROCESS_COUNT, 0, 0, 0, NULL},
	{5 * 1024, "5K",        100 / PROCESS_COUNT, 0, 0, 0, NULL},
	{10 * 1024, "10K",      100 / PROCESS_COUNT, 0, 0, 0, NULL},
	{50 * 1024, "50K",       50 / PROCESS_COUNT, 0, 0, 0, NULL},
	{100 * 1024, "100K",     50 / PROCESS_COUNT, 0, 0, 0, NULL},
	{500 * 1024, "500K",     20 / PROCESS_COUNT, 0, 0, 0, NULL}
};

#else

static TestAppendInfo appends[FILE_TYPE_COUNT] = {
	{1 * 1024, "1K",        10000 / PROCESS_COUNT, 0, 0, 0, NULL},
	{5 * 1024, "5K",        10000 / PROCESS_COUNT, 0, 0, 0, NULL},
	{10 * 1024, "10K",       5000 / PROCESS_COUNT, 0, 0, 0, NULL},
	{50 * 1024, "50K",       2000 / PROCESS_COUNT, 0, 0, 0, NULL},
	{100 * 1024, "100K",     1000 / PROCESS_COUNT, 0, 0, 0, NULL},
	{500 * 1024, "500K",      500 / PROCESS_COUNT, 0, 0, 0, NULL}
};

#endif

static StorageStat storages[MAX_STORAGE_COUNT];
static int storage_count = 0;
static time_t start_time;
static int total_count = 0;
static int success_count = 0;
static FILE *fpSuccess = NULL;
static FILE *fpFail = NULL;

static int process_index;
static char base_file_id[128];  // base appender file to append to
static char base_group_name[FDFS_GROUP_NAME_MAX_LEN + 1];

static int create_base_appender_file();
static int generate_append_buffers();
static int test_init();
static int save_stats_by_overall();
static int save_stats_by_append_type();
static int save_stats_by_storage_ip();
static int add_to_storage_stat(const char *storage_ip, const int result, const int time_used);

int main(int argc, char **argv)
{
	int result;
	int append_count;
	int rand_num;
	int append_index;
	char *conf_filename;
	char storage_ip[IP_ADDRESS_SIZE];
	int count_sums[FILE_TYPE_COUNT];
	int i;
	struct timeval tv_start;
	struct timeval tv_end;
	int time_used;

	if (argc < 2)
	{
		printf("Usage: %s <process_index> [config_filename]\n", argv[0]);
		return EINVAL;
	}

	log_init();
	process_index = atoi(argv[1]);
	if (process_index < 0 || process_index >= PROCESS_COUNT)
	{
		printf("Invalid process index: %d\n", process_index);
		return EINVAL;
	}

	if (argc >= 3)
	{
		conf_filename = argv[2];
	}
	else
	{
		conf_filename = "/etc/fdfs/client.conf";
	}

	if ((result = generate_append_buffers()) != 0)
	{
		return result;
	}

	if ((result = test_init()) != 0)
	{
		return result;
	}

	if ((result = dfs_init(process_index, conf_filename)) != 0)
	{
		return result;
	}

	if ((result = my_daemon_init()) != 0)
	{
		return result;
	}

	// Create base appender file for testing
	if ((result = create_base_appender_file()) != 0)
	{
		printf("Failed to create base appender file, error: %d\n", result);
		return result;
	}

	printf("Base appender file created: %s/%s\n", base_group_name, base_file_id);

	memset(&storages, 0, sizeof(storages));
	append_count = 0;
	for (i = 0; i < FILE_TYPE_COUNT; i++)
	{
		append_count += appends[i].count;
		count_sums[i] = append_count;
	}

	if (append_count == 0)
	{
		return EINVAL;
	}

	memset(storage_ip, 0, sizeof(storage_ip));

	start_time = time(NULL);
	srand(SRAND_SEED);
	result = 0;
	total_count = 0;
	success_count = 0;

	while (total_count < append_count)
	{
		rand_num = (int)(append_count * ((double)rand() / RAND_MAX));
		for (append_index = 0; append_index < FILE_TYPE_COUNT; append_index++)
		{
			if (rand_num < count_sums[append_index])
			{
				break;
			}
		}

		if (append_index >= FILE_TYPE_COUNT || 
			appends[append_index].append_count >= appends[append_index].count)
		{
			continue;
		}

		appends[append_index].append_count++;
		total_count++;

		gettimeofday(&tv_start, NULL);
		*storage_ip = '\0';

		result = append_file_by_buff(appends[append_index].append_buff, 
			appends[append_index].bytes, base_group_name, base_file_id, storage_ip);

		gettimeofday(&tv_end, NULL);
		time_used = TIME_SUB_MS(tv_end, tv_start);
		appends[append_index].time_used += time_used;

		if (result == 0)
		{
			appends[append_index].success_count++;
			success_count++;
			fprintf(fpSuccess, "%d %d %s %s\n", (int)tv_end.tv_sec, 
				time_used, base_group_name, base_file_id);
		}
		else
		{
			fprintf(fpFail, "%d %d %d %s %s\n", (int)tv_end.tv_sec, 
				time_used, result, base_group_name, base_file_id);
		}

		if (*storage_ip != '\0')
		{
			add_to_storage_stat(storage_ip, result, time_used);
		}

		if (total_count % 10000 == 0)
		{
			printf("Total append: %d, success: %d\n", total_count, success_count);
		}
	}

	fclose(fpSuccess);
	fclose(fpFail);

	save_stats_by_overall();
	save_stats_by_append_type();
	save_stats_by_storage_ip();

	printf("\nTotal append operations: %d\n", total_count);
	printf("Success count: %d\n", success_count);
	printf("Fail count: %d\n", total_count - success_count);
	printf("Time elapsed: %d seconds\n", (int)(time(NULL) - start_time));

	dfs_destroy();

	return result;
}

static int create_base_appender_file()
{
	int result;
	char initial_content[1024];
	char storage_ip[IP_ADDRESS_SIZE];

	// Generate initial content
	memset(initial_content, 'A', sizeof(initial_content));
	memset(base_file_id, 0, sizeof(base_file_id));
	memset(base_group_name, 0, sizeof(base_group_name));
	memset(storage_ip, 0, sizeof(storage_ip));

	// Upload as appender file
	result = upload_appender_file_by_buff(initial_content, sizeof(initial_content),
		"txt", NULL, 0, base_group_name, base_file_id, storage_ip);

	if (result != 0)
	{
		printf("Failed to upload base appender file, error: %d\n", result);
		return result;
	}

	return 0;
}

static int generate_append_buffers()
{
	int i;
	int j;

	for (i = 0; i < FILE_TYPE_COUNT; i++)
	{
		appends[i].append_buff = (char *)malloc(appends[i].bytes);
		if (appends[i].append_buff == NULL)
		{
			fprintf(stderr, "file: "__FILE__", line: %d, " \
				"malloc %d bytes fail, " \
				"errno: %d, error info: %s\n", __LINE__, \
				appends[i].bytes, errno, STRERROR(errno));
			return errno != 0 ? errno : ENOMEM;
		}

		// Fill with pattern data
		for (j = 0; j < appends[i].bytes; j++)
		{
			appends[i].append_buff[j] = 'B' + (j % 26);
		}
	}

	return 0;
}

static int save_stats_by_append_type()
{
	int k;
	char filename[64];
	FILE *fp;

	sprintf(filename, "%s.%d", STAT_FILENAME_BY_FILE_TYPE, process_index);
	if ((fp = fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	fprintf(fp, "#append_size total_count success_count time_used(ms)\n");
	for (k = 0; k < FILE_TYPE_COUNT; k++)
	{
		fprintf(fp, "%s %d %d %"PRId64"\n", \
			appends[k].description, appends[k].append_count, \
			appends[k].success_count, appends[k].time_used);
	}

	fclose(fp);
	return 0;
}

static int save_stats_by_storage_ip()
{
	int k;
	char filename[64];
	FILE *fp;

	sprintf(filename, "%s.%d", STAT_FILENAME_BY_STORAGE_IP, process_index);
	if ((fp = fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	fprintf(fp, "#ip_addr total_count success_count time_used(ms)\n");
	for (k = 0; k < storage_count; k++)
	{
		fprintf(fp, "%s %d %d %"PRId64"\n", \
			storages[k].ip_addr, storages[k].total_count, \
			storages[k].success_count, storages[k].time_used);
	}

	fclose(fp);
	return 0;
}

static int save_stats_by_overall()
{
	char filename[64];
	FILE *fp;

	sprintf(filename, "%s.%d", STAT_FILENAME_BY_OVERALL, process_index);
	if ((fp = fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	fprintf(fp, "#total_count success_count  time_used(s)\n");
	fprintf(fp, "%d %d %d\n", total_count, success_count, (int)(time(NULL) - start_time));

	fclose(fp);
	return 0;
}

static int add_to_storage_stat(const char *storage_ip, const int result, const int time_used)
{
	StorageStat *pStorage;
	StorageStat *pEnd;

	pEnd = storages + storage_count;
	for (pStorage = storages; pStorage < pEnd; pStorage++)
	{
		if (strcmp(storage_ip, pStorage->ip_addr) == 0)
		{
			break;
		}
	}

	if (pStorage == pEnd) // not found
	{
		if (storage_count >= MAX_STORAGE_COUNT)
		{
			printf("storage_count %d >= %d\n", storage_count, MAX_STORAGE_COUNT);
			return ENOSPC;
		}

		strcpy(pStorage->ip_addr, storage_ip);
		storage_count++;
	}

	pStorage->time_used += time_used;
	pStorage->total_count++;
	if (result == 0)
	{
		pStorage->success_count++;
	}

	return 0;
}

static int test_init()
{
	char filename[64];

	if (access("append", 0) != 0 && mkdir("append", 0755) != 0)
	{
		// Directory creation failed, but continue
	}

	if (chdir("append") != 0)
	{
		printf("chdir fail, errno: %d, error info: %s\n", errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	sprintf(filename, "%s.%d", FILENAME_FILE_ID, process_index);
	if ((fpSuccess = fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	sprintf(filename, "%s.%d", FILENAME_FAIL, process_index);
	if ((fpFail = fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	return 0;
}
