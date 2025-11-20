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
#include "fastdfs/fdfs_client.h"
#include "test_types.h"
#include "common_func.h"
#include "dfs_func.h"

#define PROCESS_COUNT	5

typedef struct {
	int bytes;  //file size
	char *filename;
	int count;   //total file count
	int metadata_count;
	int success_count;  //success count
	int fd;   //file description
	int64_t time_used;  //unit: ms
	char *file_buff; //file content
} TestFileInfo;

#ifdef DEBUG  //for debug

static TestFileInfo files[FILE_TYPE_COUNT] = {
	{5 * 1024, "5K",        100 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{50 * 1024, "50K",      100 / PROCESS_COUNT, 0, 0, -1, 0, NULL}, 
	{200 * 1024, "200K",     50 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{1 * 1024 * 1024, "1M",   20 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{10 * 1024 * 1024, "10M",  5 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{100 * 1024 * 1024, "100M", 2 / PROCESS_COUNT, 0, 0, -1, 0, NULL}
};

#else

static TestFileInfo files[FILE_TYPE_COUNT] = {
	{5 * 1024, "5K",         10000 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{50 * 1024, "50K",       10000 / PROCESS_COUNT, 0, 0, -1, 0, NULL}, 
	{200 * 1024, "200K",     5000 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{1 * 1024 * 1024, "1M",   1000 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{10 * 1024 * 1024, "10M",  100 / PROCESS_COUNT, 0, 0, -1, 0, NULL},
	{100 * 1024 * 1024, "100M", 50 / PROCESS_COUNT, 0, 0, -1, 0, NULL}
};

#endif

static StorageStat storages[MAX_STORAGE_COUNT];
static int storage_count = 0;
static time_t start_time;
static int total_count = 0;
static int success_count = 0;
static FILE *fpSuccess = NULL;
static FILE *fpFail = NULL;

static int proccess_index;
static int load_file_contents();
static int test_init();
static int save_stats_by_overall();
static int save_stats_by_file_type();
static int save_stats_by_storage_ip();
static int add_to_storage_stat(const char *storage_ip, const int result, const int time_used);
static int set_metadata_test(const char *file_id, const char *storage_ip);
static int get_metadata_test(const char *file_id, const char *storage_ip);

int main(int argc, char **argv)
{
	int result;
	int metadata_count;
	int rand_num;
	int file_index;
	char *conf_filename;
	char file_id[128];
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

	if ((result = load_file_contents()) != 0)
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
	metadata_count = 0;
	for (i=0; i<FILE_TYPE_COUNT; i++)
	{
		metadata_count += files[i].count;
		count_sums[i] = metadata_count;
	}

	if (metadata_count == 0)
	{
		return EINVAL;
	}

	memset(file_id, 0, sizeof(file_id));
	memset(storage_ip, 0, sizeof(storage_ip));

	start_time = time(NULL);
	srand(SRAND_SEED);
	result = 0;
	total_count = 0;
	success_count = 0;
	while (total_count < metadata_count)
	{
		rand_num = (int)(metadata_count * ((double)rand() / RAND_MAX));
		for (file_index=0; file_index<FILE_TYPE_COUNT; file_index++)
		{
			if (rand_num < count_sums[file_index])
			{
				break;
			}
		}

		if (file_index >= FILE_TYPE_COUNT || files[file_index].metadata_count >= files[file_index].count)
		{
			continue;
		}

		files[file_index].metadata_count++;
		total_count++;

		// First upload a file
		gettimeofday(&tv_start, NULL);
		*storage_ip = '\0';
		result = upload_file(files[file_index].file_buff, files[file_index].bytes, file_id, storage_ip);
		gettimeofday(&tv_end, NULL);
		time_used = TIME_SUB_MS(tv_end, tv_start);

		if (result == 0) //upload success
		{
			// Test set metadata
			gettimeofday(&tv_start, NULL);
			result = set_metadata_test(file_id, storage_ip);
			gettimeofday(&tv_end, NULL);
			time_used += TIME_SUB_MS(tv_end, tv_start);

			if (result == 0)
			{
				// Test get metadata
				gettimeofday(&tv_start, NULL);
				result = get_metadata_test(file_id, storage_ip);
				gettimeofday(&tv_end, NULL);
				time_used += TIME_SUB_MS(tv_end, tv_start);
			}

			// Delete the test file
			delete_file(file_id, storage_ip);
		}

		files[file_index].time_used += time_used;
		add_to_storage_stat(storage_ip, result, time_used);
		if (result == 0) //success
		{
			success_count++;
			files[file_index].success_count++;

			fprintf(fpSuccess, "%d %d %s %s %d\n", 
				(int)tv_end.tv_sec, files[file_index].bytes, 
				file_id, storage_ip, time_used);
		}
		else //fail
		{
			fprintf(fpFail, "%d %d %d %d\n", (int)tv_end.tv_sec, 
				files[file_index].bytes, result, time_used);
			fflush(fpFail);
		}

		if (total_count % 100 == 0)
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

	fclose(fpSuccess);
	fclose(fpFail);

	dfs_destroy();

	printf("process %d, time used: %ds\n", proccess_index, (int)(time(NULL) - start_time));
	return result;
}

static int set_metadata_test(const char *file_id, const char *storage_ip)
{
	int result;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	ConnectionInfo storageServer;
	FDFSMetaData meta_list[3];

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		return errno != 0 ? errno : ECONNREFUSED;
	}

	if ((result=tracker_query_storage_update1(pTrackerServer,
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

	// Parse file_id to get group_name and filename
	FDFS_SPLIT_GROUP_NAME_AND_FILENAME(file_id)

	// Set up metadata
	snprintf(meta_list[0].name, sizeof(meta_list[0].name), "width");
	snprintf(meta_list[0].value, sizeof(meta_list[0].value), "1920");
	snprintf(meta_list[1].name, sizeof(meta_list[1].name), "height");
	snprintf(meta_list[1].value, sizeof(meta_list[1].value), "1080");
	snprintf(meta_list[2].name, sizeof(meta_list[2].name), "test_index");
	char index_str[32];
	snprintf(index_str, sizeof(index_str), "%d", proccess_index);
	snprintf(meta_list[2].value, sizeof(meta_list[2].value), "%s", index_str);

	result = storage_set_metadata1(pTrackerServer, pStorageServer,
		file_id, meta_list, 3, STORAGE_SET_METADATA_FLAG_OVERWRITE);

	tracker_close_connection(pTrackerServer);
	tracker_close_connection(pStorageServer);

	return result;
}

static int get_metadata_test(const char *file_id, const char *storage_ip)
{
	int result;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	ConnectionInfo storageServer;
	FDFSMetaData *meta_list = NULL;
	int meta_count = 0;

	pTrackerServer = tracker_get_connection();
	if (pTrackerServer == NULL)
	{
		return errno != 0 ? errno : ECONNREFUSED;
	}

	if ((result=tracker_query_storage_update1(pTrackerServer,
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

	result = storage_get_metadata1(pTrackerServer, pStorageServer,
		file_id, &meta_list, &meta_count);

	if (result == 0 && meta_list != NULL)
	{
		// Free metadata list
		if (meta_list != NULL)
		{
			free(meta_list);
		}
	}

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
		fprintf(fp, "%s %d %d %"PRId64"\n", \
			files[k].filename, files[k].metadata_count, \
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

static int load_file_contents()
{
	int i;
	int64_t file_size;

	for (i=0; i<FILE_TYPE_COUNT; i++)
	{
		files[i].fd = open(files[i].filename, O_RDONLY);
		if (files[i].fd < 0)
		{
			fprintf(stderr, "file: "__FILE__", line: %d, " \
				"open file %s fail, " \
				"errno: %d, error info: %s\n", __LINE__, \
				files[i].filename, errno, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}

		if ((file_size=lseek(files[i].fd, 0, SEEK_END)) < 0)
		{
			fprintf(stderr, "file: "__FILE__", line: %d, " \
				"lseek file %s fail, " \
				"errno: %d, error info: %s\n", __LINE__, \
				files[i].filename, errno, STRERROR(errno));
			return errno != 0 ? errno : EIO;
		}

		if (file_size != files[i].bytes)
		{
			fprintf(stderr, "file: "__FILE__", line: %d, " 
				"%s file size: %d != %d\n", __LINE__, 
				files[i].filename, (int)file_size, \
				files[i].bytes);

			return EINVAL;
		}

		files[i].file_buff = mmap(NULL, file_size, PROT_READ, \
					MAP_SHARED, files[i].fd, 0);
		if (files[i].file_buff == MAP_FAILED)
		{
			fprintf(stderr, "file: "__FILE__", line: %d, " \
				"mmap file %s fail, " \
				"errno: %d, error info: %s\n", \
				__LINE__, files[i].filename, \
				errno, STRERROR(errno));
			return errno != 0 ? errno : ENOENT;
		}
	}

	return 0;
}

static int test_init()
{
	char filename[64];

	if (access("metadata", 0) != 0 && mkdir("metadata", 0755) != 0)
	{
	}

	if (chdir("metadata") != 0)
	{
		printf("chdir fail, errno: %d, error info: %s\n", errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	sprintf(filename, "%s.%d", FILENAME_FILE_ID, proccess_index);
	if ((fpSuccess=fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
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

