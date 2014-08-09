#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logger.h"
#include "common_define.h"
#include "test_types.h"
#include "common_func.h"
#include "dfs_func.h"

#define PROCESS_COUNT	20

#ifdef DEBUG  //for debug
#define TOTAL_SECONDS	300
#else
#define TOTAL_SECONDS	8 * 3600
#endif

typedef struct {
	int file_type;  //index
	char *file_id;
} FileEntry;

typedef struct {
	int bytes;  //file size
	char *filename;
	int count;   //total file count
	int download_count;
	int success_count;  //success upload count
	int64_t time_used;  //unit: ms
} TestFileInfo;

#ifdef DEBUG  //for debug
static TestFileInfo files[FILE_TYPE_COUNT] = {
	{5 * 1024, "5K",         1000 / PROCESS_COUNT, 0, 0, 0},
	{50 * 1024, "50K",       2000 / PROCESS_COUNT, 0, 0, 0}, 
	{200 * 1024, "200K",     1000 / PROCESS_COUNT, 0, 0, 0},
	{1 * 1024 * 1024, "1M",   200 / PROCESS_COUNT, 0, 0, 0},
	{10 * 1024 * 1024, "10M",  20 / PROCESS_COUNT, 0, 0, 0},
	{100 * 1024 * 1024, "100M", 10 / PROCESS_COUNT, 0, 0, 0}
};

#else

static TestFileInfo files[FILE_TYPE_COUNT] = {
	{5 * 1024, "5K",         1000000 / PROCESS_COUNT, 0, 0, 0},
	{50 * 1024, "50K",       2000000 / PROCESS_COUNT, 0, 0, 0}, 
	{200 * 1024, "200K",     1000000 / PROCESS_COUNT, 0, 0, 0},
	{1 * 1024 * 1024, "1M",   200000 / PROCESS_COUNT, 0, 0, 0},
	{10 * 1024 * 1024, "10M",  20000 / PROCESS_COUNT, 0, 0, 0},
	{100 * 1024 * 1024, "100M", 1000 / PROCESS_COUNT, 0, 0, 0}
};

#endif

static StorageStat storages[MAX_STORAGE_COUNT];
static int storage_count = 0;
static time_t start_time;
static int total_count = 0;
static int success_count = 0;
static FILE *fpFail = NULL;

static int proccess_index = 0;
static int file_count = 0;
static FileEntry *file_entries = NULL;

static int load_file_ids();
static int test_init();
static int save_stats_by_overall();
static int save_stats_by_file_type();
static int save_stats_by_storage_ip();
static int add_to_storage_stat(const char *storage_ip, const int result, const int time_used);

int main(int argc, char **argv)
{
	int result;
	int file_index;
	int file_type;
	int file_size;
	char *conf_filename;
	char storage_ip[IP_ADDRESS_SIZE];
	struct timeval tv_start;
	struct timeval tv_end;
	int time_used;

	if (argc < 2)
	{
		printf("Usage: %s <process_index> [config_filename]\n", argv[0]);
		return EINVAL;
	}

	log_init();
	proccess_index = atoi(argv[1]);
	if (proccess_index < 0 || proccess_index >= PROCESS_COUNT)
	{
		printf("Invalid proccess index: %d\n", proccess_index);
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

	if ((result = load_file_ids()) != 0)
	{
		return result;
	}

	if ((result=test_init()) != 0)
	{
		return result;
	}

	if ((result=dfs_init(proccess_index, conf_filename)) != 0)
	{
		return result;
	}

#ifndef WIN32
	if (daemon(1, 1) != 0)
	{
		return errno != 0 ? errno : EFAULT;
	}
#endif

	/*
	printf("file_count = %d\n", file_count);
	printf("file_entries[0]=%s\n", file_entries[0].file_id);
	printf("file_entries[%d]=%s\n", file_count-1, file_entries[file_count-1].file_id);
	*/

	memset(&storages, 0, sizeof(storages));
	memset(storage_ip, 0, sizeof(storage_ip));

	start_time = time(NULL);
	srand(SRAND_SEED);
	result = 0;
	total_count = 0;
	success_count = 0;
	while (time(NULL) - start_time < TOTAL_SECONDS)
	{
		file_index = (int)(file_count * ((double)rand() / RAND_MAX));
		if (file_index >= file_count)
		{
			printf("file_index=%d!!!!\n", file_index);
			continue;
		}

		file_type = file_entries[file_index].file_type;
		files[file_type].download_count++;
		total_count++;

		gettimeofday(&tv_start, NULL);
		*storage_ip = '\0';
		result = download_file(file_entries[file_index].file_id, &file_size, storage_ip);
		gettimeofday(&tv_end, NULL);
		time_used = TIME_SUB_MS(tv_end, tv_start);
		files[file_type].time_used += time_used;

		add_to_storage_stat(storage_ip, result, time_used);
		if (result == 0) //success
		{
			if (file_size != files[file_type].bytes)
			{
				result = EINVAL;
			}
		}

		if (result == 0) //success
		{
			success_count++;
			files[file_type].success_count++;
		}
		else //fail
		{
			fprintf(fpFail, "%d %d %s %s %d %d\n", (int)tv_end.tv_sec, 
				files[file_type].bytes, file_entries[file_index].file_id, 
				storage_ip, result, time_used);
			fflush(fpFail);
		}

		if (total_count % 10000 == 0)
		{
			if ((result=save_stats_by_overall()) != 0)
			{
				break;
			}
			if ((result=save_stats_by_file_type()) != 0)
			{
				break;
			}

			if ((result=save_stats_by_storage_ip()) != 0)
			{
				break;
			}
		}
	}

	save_stats_by_overall();
	save_stats_by_file_type();
	save_stats_by_storage_ip();

	fclose(fpFail);

	dfs_destroy();

	printf("proccess %d, time used: %ds\n", proccess_index, (int)(time(NULL) - start_time));
	return result;
}

static int save_stats_by_file_type()
{
	int k;
	char filename[64];
	FILE *fp;

	sprintf(filename, "%s.%d", STAT_FILENAME_BY_FILE_TYPE, proccess_index);
	if ((fp=fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	fprintf(fp, "#file_type total_count success_count time_used(ms)\n");
	for (k=0; k<FILE_TYPE_COUNT; k++)
	{
		fprintf(fp, "%s %d %d %"PRId64"\n", \
			files[k].filename, files[k].download_count, \
			files[k].success_count, files[k].time_used);
	}

	fclose(fp);
	return 0;
}

static int save_stats_by_storage_ip()
{
	int k;
	char filename[64];
	FILE *fp;

	sprintf(filename, "%s.%d", STAT_FILENAME_BY_STORAGE_IP, proccess_index);
	if ((fp=fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	fprintf(fp, "#ip_addr total_count success_count time_used(ms)\n");
	for (k=0; k<storage_count; k++)
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

	sprintf(filename, "%s.%d", STAT_FILENAME_BY_OVERALL, proccess_index);
	if ((fp=fopen(filename, "wb")) == NULL)
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
	for (pStorage=storages; pStorage<pEnd; pStorage++)
	{
		if (strcmp(storage_ip, pStorage->ip_addr) == 0)
		{
			break;
		}
	}

	if (pStorage == pEnd) //not found
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

static int get_file_type_index(const int file_bytes)
{
	TestFileInfo *pFile;
	TestFileInfo *pEnd;

	pEnd = files + FILE_TYPE_COUNT;
	for (pFile=files; pFile<pEnd; pFile++)
	{
		if (file_bytes == pFile->bytes)
		{
			return pFile - files;
		}
	}

	return -1;
}

static int load_file_ids()
{
	int i;
	int result;
	int64_t file_size;
	int bytes;
	char filename[64];
	char *file_buff;
	char *p;
	int nLineCount;
	int nSkipLines;
	char *pStart;
	char *pEnd;
	char *pFind;

	sprintf(filename, "upload/%s.%d", FILENAME_FILE_ID, proccess_index / 2);
	if ((result=getFileContent(filename, &file_buff, &file_size)) != 0)
	{
		printf("file: "__FILE__", line: %d, " 
			"getFileContent %s fail, errno: %d, error info: %s\n", __LINE__, 
			filename, errno, STRERROR(errno));

		return result;
	}

	nLineCount = 0;
	p = file_buff;
	while (*p != '\0')
	{
		if (*p == '\n')
		{
			nLineCount++;
		}

		p++;
	}

	file_count = nLineCount / 2;
	if (file_count == 0)
	{
		printf("file: "__FILE__", line: %d, " 
			"file count == 0 in file %s\n", __LINE__, filename);
		free(file_buff);
		return EINVAL;
	}

	file_entries = (FileEntry *)malloc(sizeof(FileEntry) * file_count);
	if (file_entries == NULL)
	{
		printf("file: "__FILE__", line: %d, " 
			"malloc %d bytes fail\n", __LINE__, \
			(int)sizeof(FileEntry) * file_count);
		free(file_buff);
		return ENOMEM;
	}
	memset(file_entries, 0, sizeof(FileEntry) * file_count);

	nSkipLines = (proccess_index % 2) * file_count;
	i = 0;
	p = file_buff;
	while (i < nSkipLines)
	{
		if (*p == '\n')
		{
			i++;
		}

		p++;
	}

	pStart = p;
	i = 0;
	while (i < file_count)
	{
		if (*p == '\n')
		{
			*p = '\0';
			pFind = strchr(pStart, ' ');
			if (pFind == NULL)
			{
				printf("file: "__FILE__", line: %d, " 
					"can't find ' ' in file %s\n", __LINE__, filename);
				result = EINVAL;
				break;
			}

			pFind++;
			pEnd = strchr(pFind, ' ');
			if (pEnd == NULL)
			{
				printf("file: "__FILE__", line: %d, " 
					"can't find ' ' in file %s\n", __LINE__, filename);
				result = EINVAL;
				break;
			}
			*pEnd = '\0';
			bytes = atoi(pFind);

			pFind = pEnd + 1;  //skip space
			pEnd = strchr(pFind, ' ');
			if (pEnd == NULL)
			{
				printf("file: "__FILE__", line: %d, " 
					"can't find ' ' in file %s\n", __LINE__, filename);
				result = EINVAL;
				break;
			}
			*pEnd = '\0';

			file_entries[i].file_type = get_file_type_index(bytes);
			if (file_entries[i].file_type < 0)
			{
				printf("file: "__FILE__", line: %d, " 
					"invalid file bytes: %d in file %s\n", __LINE__, bytes, filename);
				result = EINVAL;
				break;
			}

			file_entries[i].file_id = strdup(pFind);
			if (file_entries[i].file_id == NULL)
			{
				printf("file: "__FILE__", line: %d, " 
					"malloc %d bytes fail\n", __LINE__, \
					(int)strlen(pFind) + 1);
				result = ENOMEM;
				break;
			}

			i++;
			pStart = ++p;
		}
		else
		{
			p++;
		}
	}

	free(file_buff);

	return result;
}

static int test_init()
{
	char filename[64];

	if (access("download", 0) != 0 && mkdir("download", 0755) != 0)
	{
	}

	if (chdir("download") != 0)
	{
		printf("chdir fail, errno: %d, error info: %s\n", errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	sprintf(filename, "%s.%d", FILENAME_FAIL, proccess_index);
	if ((fpFail=fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	return 0;
}

