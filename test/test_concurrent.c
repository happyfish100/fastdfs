#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "fastcommon/common_define.h"
#include "fastcommon/logger.h"
#include "test_types.h"
#include "common_func.h"
#include "dfs_func.h"

#define PROCESS_COUNT	10
#define OPERATION_COUNT	1000

typedef enum {
	OP_UPLOAD = 0,
	OP_DOWNLOAD = 1,
	OP_DELETE = 2,
	OP_APPEND = 3,
	OP_COUNT = 4
} OperationType;

typedef struct {
	int bytes;
	char *filename;
	int fd;
	char *file_buff;
} TestFileInfo;

static TestFileInfo test_file = {
	50 * 1024, "50K", -1, NULL
};

static time_t start_time;
static int total_count = 0;
static int success_count = 0;
static int op_count[OP_COUNT];
static int op_success[OP_COUNT];
static FILE *fpLog = NULL;

static int proccess_index;
static int load_file_contents();
static int test_init();
static int perform_operation(OperationType op_type, char *file_id, char *storage_ip);
static void save_stats();

int main(int argc, char **argv)
{
	int result;
	char *conf_filename;
	char file_id[128];
	char storage_ip[IP_ADDRESS_SIZE];
	int i;
	OperationType op_type;

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

	memset(file_id, 0, sizeof(file_id));
	memset(storage_ip, 0, sizeof(storage_ip));
	memset(op_count, 0, sizeof(op_count));
	memset(op_success, 0, sizeof(op_success));

	start_time = time(NULL);
	srand(SRAND_SEED + proccess_index);
	result = 0;
	total_count = 0;
	success_count = 0;

	// Perform mixed operations
	for (i = 0; i < OPERATION_COUNT; i++)
	{
		op_type = (OperationType)(rand() % OP_COUNT);
		op_count[op_type]++;

		result = perform_operation(op_type, file_id, storage_ip);
		total_count++;

		if (result == 0)
		{
			success_count++;
			op_success[op_type]++;
		}

		if (total_count % 100 == 0)
		{
			save_stats();
		}
	}

	save_stats();
	fclose(fpLog);

	dfs_destroy();

	printf("process %d, time used: %ds, total: %d, success: %d\n", 
		proccess_index, (int)(time(NULL) - start_time), total_count, success_count);
	return result;
}

static int perform_operation(OperationType op_type, char *file_id, char *storage_ip)
{
	int result = 0;
	char appender_file_id[128];
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1];
	static char last_file_id[128] = {0};
	static int has_file = 0;

	switch (op_type)
	{
	case OP_UPLOAD:
		*file_id = '\0';
		*storage_ip = '\0';
		result = upload_file(test_file.file_buff, test_file.bytes, file_id, storage_ip);
		if (result == 0)
		{
			strcpy(last_file_id, file_id);
			has_file = 1;
		}
		break;

	case OP_DOWNLOAD:
		if (!has_file || *last_file_id == '\0')
		{
			// Upload first if no file available
			*file_id = '\0';
			*storage_ip = '\0';
			result = upload_file(test_file.file_buff, test_file.bytes, file_id, storage_ip);
			if (result == 0)
			{
				strcpy(last_file_id, file_id);
				has_file = 1;
			}
		}
		else
		{
			int file_size;
			*storage_ip = '\0';
			result = download_file(last_file_id, &file_size, storage_ip);
		}
		break;

	case OP_DELETE:
		if (!has_file || *last_file_id == '\0')
		{
			// Upload first if no file available
			*file_id = '\0';
			*storage_ip = '\0';
			result = upload_file(test_file.file_buff, test_file.bytes, file_id, storage_ip);
			if (result == 0)
			{
				strcpy(last_file_id, file_id);
				has_file = 1;
			}
		}
		else
		{
			*storage_ip = '\0';
			result = delete_file(last_file_id, storage_ip);
			if (result == 0)
			{
				has_file = 0;
				*last_file_id = '\0';
			}
		}
		break;

	case OP_APPEND:
		if (!has_file || *last_file_id == '\0')
		{
			// Create appender file first
			*group_name = '\0';
			*appender_file_id = '\0';
			*storage_ip = '\0';
			result = upload_appender_file_by_buff(test_file.file_buff, test_file.bytes,
				"txt", NULL, 0, group_name, appender_file_id, storage_ip);
			if (result == 0)
			{
				strcpy(last_file_id, appender_file_id);
				has_file = 1;
			}
		}
		else
		{
			// Append to existing appender file
			char append_data[1024];
			memset(append_data, 'A', sizeof(append_data));
			// Extract group_name from file_id (macro declares group_name and filename)
			FDFS_SPLIT_GROUP_NAME_AND_FILENAME(last_file_id);
			*storage_ip = '\0';
			// append_file_by_buff needs group_name and full file_id
			result = append_file_by_buff(append_data, sizeof(append_data),
				group_name, last_file_id, storage_ip);
		}
		break;

	default:
		result = EINVAL;
		break;
	}

	return result;
}

static void save_stats()
{
	char filename[64];
	FILE *fp;

	sprintf(filename, "concurrent_stats.%d", proccess_index);
	if ((fp=fopen(filename, "wb")) == NULL)
	{
		return;
	}

	fprintf(fp, "#total_count success_count time_used(s)\n");
	fprintf(fp, "%d %d %d\n", total_count, success_count, (int)(time(NULL) - start_time));
	fprintf(fp, "\n#operation_type count success\n");
	fprintf(fp, "upload %d %d\n", op_count[OP_UPLOAD], op_success[OP_UPLOAD]);
	fprintf(fp, "download %d %d\n", op_count[OP_DOWNLOAD], op_success[OP_DOWNLOAD]);
	fprintf(fp, "delete %d %d\n", op_count[OP_DELETE], op_success[OP_DELETE]);
	fprintf(fp, "append %d %d\n", op_count[OP_APPEND], op_success[OP_APPEND]);

	fclose(fp);
}

static int load_file_contents()
{
	int64_t file_size;

	test_file.fd = open(test_file.filename, O_RDONLY);
	if (test_file.fd < 0)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " 
			"open file %s fail, " 
			"errno: %d, error info: %s\n", __LINE__, 
			test_file.filename, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	if ((file_size=lseek(test_file.fd, 0, SEEK_END)) < 0)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " 
			"lseek file %s fail, " 
			"errno: %d, error info: %s\n", __LINE__, 
			test_file.filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EIO;
	}

	if (file_size != test_file.bytes)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " 
			"%s file size: %d != %d\n", __LINE__, 
			test_file.filename, (int)file_size, test_file.bytes);
		return EINVAL;
	}

	test_file.file_buff = mmap(NULL, file_size, PROT_READ, 
				MAP_SHARED, test_file.fd, 0);
	if (test_file.file_buff == MAP_FAILED)
	{
		fprintf(stderr, "file: "__FILE__", line: %d, " 
			"mmap file %s fail, " 
			"errno: %d, error info: %s\n", 
			__LINE__, test_file.filename, 
			errno, STRERROR(errno));
		return errno != 0 ? errno : ENOENT;
	}

	return 0;
}

static int test_init()
{
	char filename[64];

	if (access("concurrent", 0) != 0 && mkdir("concurrent", 0755) != 0)
	{
	}

	if (chdir("concurrent") != 0)
	{
		printf("chdir fail, errno: %d, error info: %s\n", errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	sprintf(filename, "concurrent_log.%d", proccess_index);
	if ((fpLog=fopen(filename, "wb")) == NULL)
	{
		printf("open file %s fail, errno: %d, error info: %s\n", 
			filename, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

	return 0;
}

