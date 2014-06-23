//test_types.h

#ifndef _TEST_TYPES_H
#define _TEST_TYPES_H

#define FILE_TYPE_COUNT  6
#define MAX_STORAGE_COUNT  5

#define STAT_FILENAME_BY_FILE_TYPE  "stat_by_file_type"
#define STAT_FILENAME_BY_STORAGE_IP "stat_by_storage_ip"
#define STAT_FILENAME_BY_OVERALL    "stat_by_overall"

#define FILENAME_FILE_ID     "file_id"
#define FILENAME_FAIL        "fail"

#define IP_ADDRESS_SIZE		16
#define SRAND_SEED		1225420780

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

typedef struct {
	char ip_addr[IP_ADDRESS_SIZE];
	int total_count;
	int success_count;
	int64_t time_used;
} StorageStat;

typedef struct {
	char id[64];
	int total_count;
	int success_count;
	int64_t time_used;
} EntryStat;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
