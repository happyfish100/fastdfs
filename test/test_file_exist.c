#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include "fastcommon/common_define.h"
#include "fastcommon/shared_func.h"
#include "fastcommon/logger.h"
#include "fastdfs/fdfs_client.h"
#include "fastdfs/storage_client1.h"
#include "test_types.h"
#include "common_func.h"
#include "dfs_func.h"

#define PROCESS_COUNT 10

typedef struct {
    int file_type;  //index
    char *file_id;
} FileEntry;

typedef struct {
    const char *name;
    bool expect_exist;
    int total_count;
    int success_count;
    int64_t time_used;  //ms
} CaseStat;

static CaseStat case_stats[] = {
    {"exist", true, 0, 0, 0},
    {"not_exist", false, 0, 0, 0},
};

#define CASE_COUNT (sizeof(case_stats) / sizeof(case_stats[0]))

typedef struct {
    int bytes;  //file size
    char *filename;
    int total_count;
    int success_count;
    int64_t time_used;  //unit: ms
} TestFileInfo;

static TestFileInfo files[FILE_TYPE_COUNT] = {
    {5 * 1024, "5K", 0, 0, 0},
    {50 * 1024, "50K", 0, 0, 0}, 
    {200 * 1024, "200K", 0, 0, 0},
    {1 * 1024 * 1024, "1M", 0, 0, 0},
    {10 * 1024 * 1024, "10M", 0, 0, 0},
    {100 * 1024 * 1024, "100M", 0, 0, 0}
};

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
static int save_stats_by_case_type();
static int add_to_storage_stat(const char *storage_ip, const int result, const int time_used);
static int file_exist_test(const char *file_id, bool expect_exist, char *storage_ip);
static int get_file_type_index(const int file_bytes);

int main(int argc, char **argv)
{
    int result;
    int i;
    int file_type;
    char *conf_filename;
    char storage_ip[IP_ADDRESS_SIZE];
    struct timeval tv_start;
    struct timeval tv_end;
    int time_used;
    CaseStat *pCase;

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

    conf_filename = (argc >= 3) ? argv[2] : "/etc/fdfs/client.conf";

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

    for (i=0; i<file_count; i++)
    {
        int case_index;
        file_type = file_entries[i].file_type;
        for (case_index = 0; case_index < CASE_COUNT; case_index++)
        {
            CaseStat *pCase = &case_stats[case_index];
            const char *target_file_id;
            char fake_file_id[256];

            if (pCase->expect_exist)
            {
                target_file_id = file_entries[i].file_id;
            }
            else
            {
                snprintf(fake_file_id, sizeof(fake_file_id), "%s.not_exist.%d",
                    file_entries[i].file_id, proccess_index);
                target_file_id = fake_file_id;
            }

            pCase->total_count++;
            files[file_type].total_count++;
            total_count++;

            gettimeofday(&tv_start, NULL);
            *storage_ip = '\0';
            result = file_exist_test(target_file_id, pCase->expect_exist, storage_ip);
            gettimeofday(&tv_end, NULL);
            time_used = TIME_SUB_MS(tv_end, tv_start);
            pCase->time_used += time_used;
            files[file_type].time_used += time_used;

            add_to_storage_stat(storage_ip, result, time_used);
            if (result == 0)
            {
                success_count++;
                pCase->success_count++;
                files[file_type].success_count++;
            }
            else
            {
                fprintf(fpFail, "%d %s %s %s %d %d\n", (int)tv_end.tv_sec,
                    pCase->name, target_file_id, storage_ip, result, time_used);
                fflush(fpFail);
            }
        }

        if ((i + 1) % 100 == 0)
        {
            if ((result=save_stats_by_overall()) != 0 ||
                (result=save_stats_by_file_type()) != 0 ||
                (result=save_stats_by_storage_ip()) != 0 ||
                (result=save_stats_by_case_type()) != 0)
            {
                break;
            }
        }
    }

    save_stats_by_overall();
    save_stats_by_file_type();
    save_stats_by_storage_ip();
    save_stats_by_case_type();

    fclose(fpFail);
    dfs_destroy();

    printf("process %d, time used: %ds\n", proccess_index, (int)(time(NULL) - start_time));
    return result;
}

static int file_exist_test(const char *file_id, bool expect_exist, char *storage_ip)
{
    int result;
    ConnectionInfo *pTrackerServer;
    ConnectionInfo *pStorageServer;

    pTrackerServer = tracker_get_connection();
    if (pTrackerServer == NULL)
    {
        return errno != 0 ? errno : ECONNREFUSED;
    }

    pStorageServer = NULL;
    if ((result=storage_file_exist1(pTrackerServer, &pStorageServer, file_id)) != 0)
    {
        tracker_close_connection_ex(pTrackerServer, true);
        if (pStorageServer != NULL)
        {
            tracker_close_connection(pStorageServer);
        }

        if (!expect_exist && result == ENOENT)
        {
            return 0;
        }

        return result;
    }

    if (pStorageServer != NULL)
    {
        strcpy(storage_ip, pStorageServer->ip_addr);
        tracker_close_connection(pStorageServer);
    }
    tracker_close_connection(pTrackerServer);

    if (expect_exist)
    {
        return 0;
    }
    else
    {
        return EEXIST;
    }
}

static int save_stats_by_case_type()
{
    char filename[64];
    FILE *fp;
    int i;

    sprintf(filename, "stat_by_case_type.%d", proccess_index);
    if ((fp=fopen(filename, "wb")) == NULL)
    {
        printf("open file %s fail, errno: %d, error info: %s\n",
            filename, errno, STRERROR(errno));
        return errno != 0 ? errno : EPERM;
    }

    fprintf(fp, "#case_type total_count success_count time_used(ms)\n");
    for (i = 0; i < CASE_COUNT; i++)
    {
        fprintf(fp, "%s %d %d %"PRId64"\n",
            case_stats[i].name, case_stats[i].total_count,
            case_stats[i].success_count, case_stats[i].time_used);
    }

    fclose(fp);
    return 0;
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
        fprintf(fp, "%s %d %d %"PRId64"\n",
            files[k].filename, files[k].total_count,
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
        fprintf(fp, "%s %d %d %"PRId64"\n",
            storages[k].ip_addr, storages[k].total_count,
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
    fprintf(fp, "%d %d %d\n", total_count, success_count,
        (int)(time(NULL) - start_time));

    fclose(fp);
    return 0;
}

static int add_to_storage_stat(const char *storage_ip, const int result, const int time_used)
{
    StorageStat *pStorage;
    StorageStat *pEnd;

    if (storage_ip == NULL || *storage_ip == '\0')
    {
        return 0;
    }

    pEnd = storages + storage_count;
    for (pStorage=storages; pStorage<pEnd; pStorage++)
    {
        if (strcmp(storage_ip, pStorage->ip_addr) == 0)
        {
            break;
        }
    }

    if (pStorage == pEnd)
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
            "malloc %d bytes fail\n", __LINE__,
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
                    "malloc %d bytes fail\n", __LINE__,
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

    if (access("file_exist", 0) != 0 && mkdir("file_exist", 0755) != 0)
    {
    }

    if (chdir("file_exist") != 0)
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

