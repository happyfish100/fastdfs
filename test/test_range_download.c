#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "fastcommon/common_define.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/logger.h"
#include "fastdfs/fdfs_client.h"
#include "test_types.h"
#include "common_func.h"
#include "dfs_func.h"

#define PROCESS_COUNT	10

typedef enum {
	RANGE_START = 0,      // Download from start with length
	RANGE_MIDDLE = 1,     // Download from middle
	RANGE_END = 2,        // Download from near end
	RANGE_FULL = 3,       // Download entire file (offset=0, length=0)
	RANGE_LAST_PART = 4,  // Download last portion
	RANGE_COUNT = 5
} RangeType;

typedef struct {
	int file_type;  //index
	char *file_id;
} FileEntry;

typedef struct {
	int bytes;  //file size
	char *filename;
	int count;   //total file count
	int range_count[RANGE_COUNT];
	int success_count[RANGE_COUNT];  //success count per range type
	int64_t time_used[RANGE_COUNT];  //unit: ms
} TestFileInfo;

#ifdef DEBUG  //for debug

static TestFileInfo files[FILE_TYPE_COUNT] = {
	{5 * 1024, "5K",         500 / PROCESS_COUNT, {0}, {0}, {0}},
	{50 * 1024, "50K",       1000 / PROCESS_COUNT, {0}, {0}, {0}}, 
	{200 * 1024, "200K",     500 / PROCESS_COUNT, {0}, {0}, {0}},
	{1 * 1024 * 1024, "1M",   100 / PROCESS_COUNT, {0}, {0}, {0}},
	{10 * 1024 * 1024, "10M",  20 / PROCESS_COUNT, {0}, {0}, {0}},
	{100 * 1024 * 1024, "100M", 10 / PROCESS_COUNT, {0}, {0}, {0}}
};

#else

static TestFileInfo files[FILE_TYPE_COUNT] = {
	{5 * 1024, "5K",         50000 / PROCESS_COUNT, {0}, {0}, {0}},
	{50 * 1024, "50K",       100000 / PROCESS_COUNT, {0}, {0}, {0}}, 
	{200 * 1024, "200K",     50000 / PROCESS_COUNT, {0}, {0}, {0}},
	{1 * 1024 * 1024, "1M",   10000 / PROCESS_COUNT, {0}, {0}, {0}},
	{10 * 1024 * 1024, "10M",  1000 / PROCESS_COUNT, {0}, {0}, {0}},
	{100 * 1024 * 1024, "100M", 100 / PROCESS_COUNT, {0}, {0}, {0}}
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
static int save_stats_by_range_type();
static int add_to_storage_stat(const char *storage_ip, const int result, const int time_used);
static int download_range_test(const char *file_id, int file_size, RangeType range_type, char *storage_ip);

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
	RangeType range_type;

	if (argc < 2)
	{
		printf("Usage: %s <process_index> [config_filename]\n", argv[0]);
		return EINVAL;
	}

	log_init();
	proccess_index = atoi(argv[1]);
	if (proccess_index < 0 || proccess_index >= PROCESS_COUNT)
	{
		printf("Invalid process index: %d\n", proccess_index);
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

    if ((result=my_daemon_init()) != 0)
	{
		return result;
	}

	memset(&storages, 0, sizeof(storages));
	memset(storage_ip, 0, sizeof(storage_ip));

	start_time = time(NULL);
	srand(SRAND_SEED + proccess_index);
	result = 0;
	total_count = 0;
	success_count = 0;
	while (total_count < file_count * RANGE_COUNT)
	{
		file_index = (int)(file_count * ((double)rand() / RAND_MAX));
		if (file_index >= file_count)
		{
			continue;
		}

		file_type = file_entries[file_index].file_type;
		range_type = (RangeType)(rand() % RANGE_COUNT);

		// Check if we've done enough of this range type for this file type
		if (files[file_type].range_count[range_type] >= files[file_type].count)
		{
			continue;
		}

		files[file_type].range_count[range_type]++;
		total_count++;

		gettimeofday(&tv_start, NULL);
		*storage_ip = '\0';
		result = download_range_test(file_entries[file_index].file_id, 
			files[file_type].bytes, range_type, storage_ip);
		gettimeofday(&tv_end, NULL);
		time_used = TIME_SUB_MS(tv_end, tv_start);
		files[file_type].time_used[range_type] += time_used;

		add_to_storage_stat(storage_ip, result, time_used);
		if (result == 0) //success
		{
			success_count++;
			files[file_type].success_count[range_type]++;
		}
		else //fail
		{
			fprintf(fpFail, "%d %d %d %s %s %d %d\n", (int)tv_end.tv_sec, 
				files[file_type].bytes, range_type, 
				file_entries[file_index].file_id, 
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

			if ((result=save_stats_by_range_type()) != 0)
			{
				break;
			}
		}
	}

	save_stats_by_overall();
	save_stats_by_file_type();
	save_stats_by_storage_ip();
	save_stats_by_range_type();

	fclose(fpFail);

	dfs_destroy();

	printf("process %d, time used: %ds\n", proccess_index, (int)(time(NULL) - start_time));
	return result;
}

static int download_range_test(const char *file_id, int file_size, RangeType range_type, char *storage_ip)
{
	int result;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	ConnectionInfo storageServer;
	char *file_buff = NULL;
	int64_t file_offset;
	int64_t download_bytes;
	int64_t downloaded_size;
	int64_t expected_size;

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		return errno != 0 ? errno : ECONNREFUSED;
	}

	if ((result=tracker_query_storage_fetch1(pTrackerServer,
		&storageServer, file_id)) != 0)
	{
		tracker_close_connection_ex(pTrackerServer, true);
		return result;
	}

	if ((pStorageServer=tracker_make_connection(&storageServer, &result))
			 == NULL)
	{
		tracker_close_connection(pTrackerServer);
		return result;
	}

	strcpy(storage_ip, storageServer.ip_addr);

	// Calculate offset and bytes based on range type
	switch (range_type)
	{
	case RANGE_START:
		// Download first 10% of file
		file_offset = 0;
		download_bytes = file_size / 10;
		if (download_bytes == 0) download_bytes = 1;
		expected_size = download_bytes;
		break;

	case RANGE_MIDDLE:
		// Download middle 20% of file
		file_offset = file_size / 3;
		download_bytes = file_size / 5;
		if (download_bytes == 0) download_bytes = 1;
		expected_size = download_bytes;
		break;

	case RANGE_END:
		// Download last 10% of file
		file_offset = file_size - (file_size / 10);
		if (file_offset < 0) file_offset = 0;
		download_bytes = file_size / 10;
		if (download_bytes == 0) download_bytes = 1;
		expected_size = download_bytes;
		break;

	case RANGE_FULL:
		// Download entire file (offset=0, length=0 means full file)
		file_offset = 0;
		download_bytes = 0;  // 0 means to end of file
		expected_size = file_size;
		break;

	case RANGE_LAST_PART:
		// Download last portion (offset near end, length=0)
		file_offset = file_size / 2;
		download_bytes = 0;  // 0 means to end of file
		expected_size = file_size - file_offset;
		break;

	default:
		result = EINVAL;
		goto cleanup;
	}

	result = storage_do_download_file1_ex(pTrackerServer, pStorageServer,
		FDFS_DOWNLOAD_TO_BUFF, file_id, file_offset, download_bytes,
		&file_buff, NULL, &downloaded_size);

	if (result == 0)
	{
		// Validate downloaded size
		if (downloaded_size != expected_size)
		{
			result = EINVAL;
		}

		// Free the downloaded buffer
		if (file_buff != NULL)
		{
			free(file_buff);
			file_buff = NULL;
		}
	}

cleanup:
	tracker_close_connection(pTrackerServer);
	tracker_close_connection(pStorageServer);

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
		int total = 0;
		int success = 0;
		int64_t time_total = 0;
		int r;
		for (r = 0; r < RANGE_COUNT; r++)
		{
			total += files[k].range_count[r];
			success += files[k].success_count[r];
			time_total += files[k].time_used[r];
		}
		fprintf(fp, "%s %d %d %"PRId64"\n", \
			files[k].filename, total, success, time_total);
	}

	fclose(fp);
	return 0;
}

static int save_stats_by_range_type()
{
	int r;
	char filename[64];
	FILE *fp;

	sprintf(filename, "stat_by_range_type.%d", proccess_index);
	if ((fp=fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	fprintf(fp, "#range_type total_count success_count time_used(ms)\n");
	const char *range_names[] = {"start", "middle", "end", "full", "last_part"};
	for (r=0; r<RANGE_COUNT; r++)
	{
		int total = 0;
		int success = 0;
		int64_t time_total = 0;
		int k;
		for (k = 0; k < FILE_TYPE_COUNT; k++)
		{
			total += files[k].range_count[r];
			success += files[k].success_count[r];
			time_total += files[k].time_used[r];
		}
		fprintf(fp, "%s %d %d %"PRId64"\n", \
			range_names[r], total, success, time_total);
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

	if (access("range_download", 0) != 0 && mkdir("range_download", 0755) != 0)
	{
	}

	if (chdir("range_download") != 0)
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

