/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "fastcommon/shared_func.h"
#include "fastcommon/pthread_func.h"
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/ioevent_loop.h"
#include "fastcommon/fc_atomic.h"
#include "sf/sf_service.h"
#include "storage_global.h"
#include "storage_service.h"
#include "trunk_mem.h"
#include "storage_dio.h"

static struct storage_dio_context *g_dio_contexts = NULL;

volatile int g_dio_thread_count = 0;

static void *dio_thread_entrance(void* arg);
 
int storage_dio_init()
{
	int result;
	int bytes;
	int threads_count_per_path;
	int context_count;
	struct storage_dio_thread_data *pThreadData;
	struct storage_dio_thread_data *pDataEnd;
	struct storage_dio_context *pContext;
	struct storage_dio_context *pContextEnd;
	pthread_t tid;
	pthread_attr_t thread_attr;

	if ((result=init_pthread_attr(&thread_attr, SF_G_THREAD_STACK_SIZE)) != 0)
	{
		logError("file: "__FILE__", line: %d, " \
			"init_pthread_attr fail, program exit!", __LINE__);
		return result;
	}

	bytes = sizeof(struct storage_dio_thread_data) * g_fdfs_store_paths.count;
	g_dio_thread_data = (struct storage_dio_thread_data *)malloc(bytes);
	if (g_dio_thread_data == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, errno: %d, error info: %s", \
			__LINE__, bytes, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	memset(g_dio_thread_data, 0, bytes);

	threads_count_per_path = g_disk_reader_threads + g_disk_writer_threads;
	context_count = threads_count_per_path * g_fdfs_store_paths.count;
	bytes = sizeof(struct storage_dio_context) * context_count;
	g_dio_contexts = (struct storage_dio_context *)malloc(bytes);
	if (g_dio_contexts == NULL)
	{
		logError("file: "__FILE__", line: %d, " \
			"malloc %d bytes fail, " \
			"errno: %d, error info: %s", __LINE__, \
			bytes, errno, STRERROR(errno));
		return errno != 0 ? errno : ENOMEM;
	}
	memset(g_dio_contexts, 0, bytes);

	g_dio_thread_count = 0;
	pDataEnd = g_dio_thread_data + g_fdfs_store_paths.count;
	for (pThreadData=g_dio_thread_data; pThreadData<pDataEnd; pThreadData++)
	{
		pThreadData->count = threads_count_per_path;
		pThreadData->contexts = g_dio_contexts + (pThreadData -
				g_dio_thread_data) * threads_count_per_path;
		pThreadData->reader = pThreadData->contexts;
		pThreadData->writer = pThreadData->contexts+g_disk_reader_threads;

		pContextEnd = pThreadData->contexts + pThreadData->count;
		for (pContext=pThreadData->contexts; pContext<pContextEnd;
			pContext++)
		{
			if ((result=blocked_queue_init(&(pContext->queue))) != 0)
			{
				return result;
			}

            pContext->path_index = pThreadData - g_dio_thread_data;
            pContext->thread_index = pContext - pThreadData->contexts;
            if (g_disk_rw_separated)
            {
                if (pContext->thread_index < g_disk_reader_threads)
                {
                    pContext->rw = "r";
                }
                else
                {
                    pContext->rw = "w";
                    pContext->thread_index -= g_disk_reader_threads;
                }
            }
            else
            {
                pContext->rw = "rw";
            }
			if ((result=pthread_create(&tid, &thread_attr,
					dio_thread_entrance, pContext)) != 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"create thread failed, " \
					"startup threads: %d, " \
					"errno: %d, error info: %s", \
					__LINE__, g_dio_thread_count, \
					result, STRERROR(result));
				return result;
			}
			else
			{
                __sync_add_and_fetch(&g_dio_thread_count, 1);
			}
		}
	}

	pthread_attr_destroy(&thread_attr);

	return result;
}

void storage_dio_terminate()
{
	struct storage_dio_context *pContext;
	struct storage_dio_context *pContextEnd;

	pContextEnd = g_dio_contexts + g_dio_thread_count;
	for (pContext=g_dio_contexts; pContext<pContextEnd; pContext++)
	{
		blocked_queue_terminate(&(pContext->queue));
	}
}

int storage_dio_queue_push(struct fast_task_info *pTask)
{
    StorageClientInfo *pClientInfo;
	StorageFileContext *pFileContext;
	struct storage_dio_context *pContext;
	int result;

    pClientInfo = (StorageClientInfo *)pTask->arg;
	pFileContext = &(pClientInfo->file_context);
	pContext = g_dio_contexts + pFileContext->dio_thread_index;
    sf_hold_task(pTask);
	if ((result=blocked_queue_push(&(pContext->queue), pTask)) != 0)
	{
		ioevent_add_to_deleted_list(pTask);
        sf_release_task(pTask);
		return result;
	}

	return 0;
}

int storage_dio_get_thread_index(struct fast_task_info *pTask, \
		const int store_path_index, const char file_op)
{
	struct storage_dio_thread_data *pThreadData;
	struct storage_dio_context *contexts;
	struct storage_dio_context *pContext;
	int count;

	pThreadData = g_dio_thread_data + store_path_index;
	if (g_disk_rw_separated)
	{
		if (file_op == FDFS_STORAGE_FILE_OP_READ)
		{
			contexts = pThreadData->reader;
			count = g_disk_reader_threads;
		}
		else
		{
			contexts = pThreadData->writer;
			count = g_disk_writer_threads;
		}
	}
	else
	{
		contexts = pThreadData->contexts;
		count = pThreadData->count;
	}

	pContext = contexts + (((unsigned int)pTask->event.fd) % count);
	return pContext - g_dio_contexts;
}

int dio_delete_normal_file(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;
	int result;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	if (unlink(pFileContext->filename) != 0)
	{
		result = errno != 0 ? errno : EACCES;
		pFileContext->log_callback(pTask, result);
	}
	else
	{
		result = 0;
	}

	pFileContext->done_callback(pTask, result);
	return result;
}

int dio_delete_trunk_file(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;
	int result;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);

	if ((result=trunk_file_delete(pFileContext->filename,
		&(pFileContext->extra_info.upload.trunk_info))) != 0)
	{
		pFileContext->log_callback(pTask, result);
	}

	pFileContext->done_callback(pTask, result);
	return result;
}

int dio_discard_file(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	pFileContext->offset += pTask->length - pFileContext->buff_offset;
	if (pFileContext->offset >= pFileContext->end)
	{
		pFileContext->done_callback(pTask, 0);
	}
	else
	{
		pFileContext->buff_offset = 0;
        pFileContext->continue_callback(pTask, SF_NIO_STAGE_RECV);
	}

	return 0;
}

int dio_open_file(StorageFileContext *pFileContext)
{
	int result;

	if (pFileContext->fd < 0)
    {
        pFileContext->fd = open(pFileContext->filename,
                pFileContext->open_flags, 0644);
        if (pFileContext->fd < 0)
        {
            result = errno != 0 ? errno : EACCES;
            logError("file: "__FILE__", line: %d, " \
                    "open file: %s fail, " \
                    "errno: %d, error info: %s", \
                    __LINE__, pFileContext->filename, \
                    result, STRERROR(result));
        }
        else
        {
            result = 0;
        }

        __sync_add_and_fetch(&g_storage_stat.total_file_open_count, 1);
        if (result == 0)
        {
            __sync_add_and_fetch(&g_storage_stat.success_file_open_count, 1);
        }

        if (result != 0)
        {
            return result;
        }
    }

	if (pFileContext->offset > 0 && lseek(pFileContext->fd,
		pFileContext->offset, SEEK_SET) < 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"lseek file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pFileContext->filename, \
			result, STRERROR(result));
		return result;
	}

	return 0;
}

int dio_read_file(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;
	int result;
	int64_t remain_bytes;
	int capacity_bytes;
	int read_bytes;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);

	do
	{
	if ((result=dio_open_file(pFileContext)) != 0)
	{
		break;
	}

	remain_bytes = pFileContext->end - pFileContext->offset;
	capacity_bytes = pTask->size - pTask->length;
	read_bytes = (capacity_bytes < remain_bytes) ? \
				capacity_bytes : remain_bytes;

	/*
	logInfo("###before dio read bytes: %d, pTask->length=%d, file offset=%ld", \
		read_bytes, pTask->length, pFileContext->offset);
	*/

	if (fc_safe_read(pFileContext->fd, pTask->data + pTask->length, \
		read_bytes) != read_bytes)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"read from file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pFileContext->filename, \
			result, STRERROR(result));
	}

    __sync_add_and_fetch(&g_storage_stat.total_file_read_count, 1);
	if (result == 0)
	{
        __sync_add_and_fetch(&g_storage_stat.success_file_read_count, 1);
	}

	if (result != 0)
	{
		break;
	}

	if (pFileContext->calc_crc32)
	{
		pFileContext->crc32 = CRC32_ex(pTask->data + pTask->length,
                read_bytes, pFileContext->crc32);
	}

	pTask->length += read_bytes;
	pFileContext->offset += read_bytes;

    /*
	logInfo("###after dio read bytes: %d, pTask->length=%d, "
            "file offset=%"PRId64", file size: %"PRId64, read_bytes,
            pTask->length, pFileContext->offset, pFileContext->end);
            */

	if (pFileContext->offset < pFileContext->end)
	{
        pFileContext->continue_callback(pTask, SF_NIO_STAGE_SEND);
	}
	else
	{
		/* file read done, close it */
		close(pFileContext->fd);
		pFileContext->fd = -1;

		if (pFileContext->calc_crc32)
		{
			pFileContext->crc32 = CRC32_FINAL( \
						pFileContext->crc32);
		}
		pFileContext->done_callback(pTask, result);
	}

	return 0;

	} while (0);

	/* file read error, close it */
	if (pFileContext->fd > 0)
	{
		close(pFileContext->fd);
		pFileContext->fd = -1;
	}

	pFileContext->done_callback(pTask, result);
	return result;
}

int dio_write_file(struct fast_task_info *pTask)
{
	StorageClientInfo *pClientInfo;
	StorageFileContext *pFileContext;
	int result;
	int write_bytes;
	char *pDataBuff;

	pClientInfo = (StorageClientInfo *)pTask->arg;
	pFileContext = &(pClientInfo->file_context);
	result = 0;
	do
	{
	if (pFileContext->fd < 0)
	{
		if (pFileContext->extra_info.upload.before_open_callback!=NULL)
		{
			result = pFileContext->extra_info.upload. \
					before_open_callback(pTask);
			if (result != 0)
			{
				break;
			}
		}

		if ((result=dio_open_file(pFileContext)) != 0)
		{
			break;
		}
	}

	pDataBuff = pTask->data + pFileContext->buff_offset;
	write_bytes = pTask->length - pFileContext->buff_offset;
	if (fc_safe_write(pFileContext->fd, pDataBuff, write_bytes) != write_bytes)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"write to file: %s fail, fd=%d, write_bytes=%d, " \
			"errno: %d, error info: %s", \
			__LINE__, pFileContext->filename, \
			pFileContext->fd, write_bytes, \
			result, STRERROR(result));
	}

    __sync_add_and_fetch(&g_storage_stat.total_file_write_count, 1);
	if (result == 0)
	{
        __sync_add_and_fetch(&g_storage_stat.success_file_write_count, 1);
	}

	if (result != 0)
	{
		break;
	}

	if (pFileContext->calc_crc32)
	{
		pFileContext->crc32 = CRC32_ex(pDataBuff, write_bytes, \
					pFileContext->crc32);
	}

	if (pFileContext->calc_file_hash)
	{
		if (g_file_signature_method == STORAGE_FILE_SIGNATURE_METHOD_HASH)
		{
			CALC_HASH_CODES4(pDataBuff, write_bytes, \
					pFileContext->file_hash_codes)
		}
		else
		{
			my_md5_update(&pFileContext->md5_context, \
				(unsigned char *)pDataBuff, write_bytes);
		}
	}

    /*
	logInfo("###dio fd: %d, write bytes: %d, pTask->length=%d, "
            "buff_offset=%d", pFileContext->fd, write_bytes,
            pTask->length, pFileContext->buff_offset);
            */

	pFileContext->offset += write_bytes;
	if (pFileContext->offset < pFileContext->end)
	{
		pFileContext->buff_offset = 0;
        pFileContext->continue_callback(pTask, SF_NIO_STAGE_RECV);
	}
	else
	{
		if (pFileContext->calc_crc32)
		{
			pFileContext->crc32 = CRC32_FINAL( \
						pFileContext->crc32);
		}

		if (pFileContext->calc_file_hash)
		{
			if (g_file_signature_method == STORAGE_FILE_SIGNATURE_METHOD_HASH)
			{
				FINISH_HASH_CODES4(pFileContext->file_hash_codes)
			}
			else
			{
				my_md5_final((unsigned char *)(pFileContext-> \
				file_hash_codes), &pFileContext->md5_context);
			}
		}

		if (pFileContext->extra_info.upload.before_close_callback != NULL)
		{
			result = pFileContext->extra_info.upload. \
					before_close_callback(pTask);
		}

		/* file write done, close it */
		close(pFileContext->fd);
		pFileContext->fd = -1;

		if (pFileContext->done_callback != NULL)
		{
			pFileContext->done_callback(pTask, result);
		}
	}

	return 0;
	} while (0);

	pClientInfo->clean_func(pTask);

	if (pFileContext->done_callback != NULL)
	{
		pFileContext->done_callback(pTask, result);
	}
	return result;
}

int dio_truncate_file(struct fast_task_info *pTask)
{
	StorageClientInfo *pClientInfo;
	StorageFileContext *pFileContext;
	int result;

	pClientInfo = (StorageClientInfo *)pTask->arg;
	pFileContext = &(pClientInfo->file_context);
	result = 0;
	do
	{
	if (pFileContext->fd < 0)
	{
		if (pFileContext->extra_info.upload.before_open_callback!=NULL)
		{
			result = pFileContext->extra_info.upload. \
					before_open_callback(pTask);
			if (result != 0)
			{
				break;
			}
		}

		if ((result=dio_open_file(pFileContext)) != 0)
		{
			break;
		}
	}

	if (ftruncate(pFileContext->fd, pFileContext->offset) != 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"truncate file: %s fail, fd=%d, " \
			"remain_bytes=%"PRId64", " \
			"errno: %d, error info: %s", \
			__LINE__, pFileContext->filename, \
			pFileContext->fd, pFileContext->offset, \
			result, STRERROR(result));
		break;
	}

	if (pFileContext->extra_info.upload.before_close_callback != NULL)
	{
		result = pFileContext->extra_info.upload. \
				before_close_callback(pTask);
	}

	/* file write done, close it */
	close(pFileContext->fd);
	pFileContext->fd = -1;

	if (pFileContext->done_callback != NULL)
	{
		pFileContext->done_callback(pTask, result);
	}

	return 0;
	} while (0);

	pClientInfo->clean_func(pTask);

	if (pFileContext->done_callback != NULL)
	{
		pFileContext->done_callback(pTask, result);
	}
	return result;
}

void dio_read_finish_clean_up(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	if (pFileContext->fd > 0)
	{
		close(pFileContext->fd);
		pFileContext->fd = -1;
	}
}

void dio_write_finish_clean_up(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	if (pFileContext->fd > 0)
	{
		close(pFileContext->fd);
		pFileContext->fd = -1;

		/* if file does not write to the end, delete it */
		if (pFileContext->offset < pFileContext->end)
		{
			if (unlink(pFileContext->filename) != 0)
			{
				logError("file: "__FILE__", line: %d, " \
					"client ip: %s, " \
					"delete useless file %s fail," \
					"errno: %d, error info: %s", \
					__LINE__, pTask->client_ip, \
					pFileContext->filename, \
					errno, STRERROR(errno));
			}
		}
	}
}

void dio_append_finish_clean_up(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	if (pFileContext->fd > 0)
	{
		/* if file does not write to the end, 
                   delete the appended contents 
                */
		if (pFileContext->offset > pFileContext->start && \
		    pFileContext->offset < pFileContext->end)
		{
			if (ftruncate(pFileContext->fd,pFileContext->start)!=0)
			{
				logError("file: "__FILE__", line: %d, " \
					"client ip: %s, " \
					"call ftruncate of file %s fail," \
					"errno: %d, error info: %s", \
					__LINE__, pTask->client_ip, \
					pFileContext->filename, \
					errno, STRERROR(errno));
			}
			else
			{
				logDebug("file: "__FILE__", line: %d, " \
					"client ip: %s, append file fail, " \
					"call ftruncate of file %s to size: "\
					"%"PRId64, \
					__LINE__, pTask->client_ip, \
					pFileContext->filename, \
					pFileContext->start);
			}
		}

		close(pFileContext->fd);
		pFileContext->fd = -1;
	}
}

void dio_modify_finish_clean_up(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	if (pFileContext->fd > 0)
	{
		/* if file does not write to the end, log error info
                */
		if (pFileContext->offset >= pFileContext->start && \
		    pFileContext->offset < pFileContext->end)
		{
			logError("file: "__FILE__", line: %d, " \
				"client ip: %s, modify file: %s fail", \
				__LINE__, pTask->client_ip, \
				pFileContext->filename);
		}

		close(pFileContext->fd);
		pFileContext->fd = -1;
	}
}

void dio_trunk_write_finish_clean_up(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;
	int result;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	if (pFileContext->fd > 0)
	{
		close(pFileContext->fd);
		pFileContext->fd = -1;

		/* if file does not write to the end, 
                   delete the appended contents 
                */
		if (pFileContext->offset > pFileContext->start && \
		    pFileContext->offset < pFileContext->end)
		{
			if ((result=trunk_file_delete(pFileContext->filename, \
			&(pFileContext->extra_info.upload.trunk_info))) != 0)
			{
			}
		}
	}
}

static void *dio_thread_entrance(void* arg)
{
	struct storage_dio_context *pContext; 
	struct fast_task_info *pTask;

	pContext = (struct storage_dio_context *)arg; 

#ifdef OS_LINUX
    {
        char thread_name[32];
        snprintf(thread_name, sizeof(thread_name), "dio-p%02d-%s[%d]",
                pContext->path_index, pContext->rw, pContext->thread_index);
        prctl(PR_SET_NAME, thread_name);
    }
#endif

	while (SF_G_CONTINUE_FLAG)
	{
		while ((pTask=blocked_queue_pop(&(pContext->queue))) != NULL)
        {
            ((StorageClientInfo *)pTask->arg)->deal_func(pTask);
            storage_release_task(pTask);
        }
	}

    __sync_sub_and_fetch(&g_dio_thread_count, 1);

	logDebug("file: "__FILE__", line: %d, "
		"dio thread exited, thread count: %d", __LINE__,
        FC_ATOMIC_GET(g_dio_thread_count));

	return NULL;
}

int dio_check_trunk_file_when_upload(struct fast_task_info *pTask)
{
	int result;
	StorageFileContext *pFileContext;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	if ((result=trunk_check_and_init_file(pFileContext->filename)) != 0)
	{
		return result;
	}

	if ((result=dio_open_file(pFileContext)) != 0)
	{
		return result;
	}

	if (lseek(pFileContext->fd, -FDFS_TRUNK_FILE_HEADER_SIZE, SEEK_CUR) < 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"lseek file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pFileContext->filename, \
			result, STRERROR(result));
		return result;
	}

	return dio_check_trunk_file_ex(pFileContext->fd, pFileContext->filename,
		 pFileContext->start - FDFS_TRUNK_FILE_HEADER_SIZE);
}

int dio_check_trunk_file_when_sync(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);
	return trunk_check_and_init_file(pFileContext->filename);
}

int dio_check_trunk_file_ex(int fd, const char *filename, const int64_t offset)
{
	int result;
	char old_header[FDFS_TRUNK_FILE_HEADER_SIZE];
	static char expect_header[FDFS_TRUNK_FILE_HEADER_SIZE] = {'\0'};

	if (fc_safe_read(fd, old_header, FDFS_TRUNK_FILE_HEADER_SIZE) !=
		FDFS_TRUNK_FILE_HEADER_SIZE)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, "
			"read trunk header of file: %s fail, "
			"errno: %d, error info: %s",
			__LINE__, filename,
			result, STRERROR(result));
		return result;
	}

	if (memcmp(old_header, expect_header,
		FDFS_TRUNK_FILE_HEADER_SIZE) != 0)
	{
		FDFSTrunkHeader srcOldTrunkHeader;
		FDFSTrunkHeader newOldTrunkHeader;

		trunk_unpack_header(old_header, &srcOldTrunkHeader);
		memcpy(&newOldTrunkHeader, &srcOldTrunkHeader,
			sizeof(FDFSTrunkHeader));
		newOldTrunkHeader.alloc_size = 0;
		newOldTrunkHeader.file_size = 0;
		newOldTrunkHeader.file_type = 0;
		trunk_pack_header(&newOldTrunkHeader, old_header);
		if (memcmp(old_header, expect_header,
			FDFS_TRUNK_FILE_HEADER_SIZE) != 0)
		{
			char buff[256];
			trunk_header_dump(&srcOldTrunkHeader, \
				buff, sizeof(buff));

			logError("file: "__FILE__", line: %d, " \
				"trunk file: %s, offset: " \
				"%"PRId64" already occupied" \
				" by other file, trunk header info: %s"\
				, __LINE__, filename, offset, buff);
			return EEXIST;
		}
	}

	return 0;
}

int dio_write_chunk_header(struct fast_task_info *pTask)
{
	StorageFileContext *pFileContext;
	char header[FDFS_TRUNK_FILE_HEADER_SIZE];
	FDFSTrunkHeader trunkHeader;
	int result;

	pFileContext = &(((StorageClientInfo *)pTask->arg)->file_context);

	if (pFileContext->extra_info.upload.file_type & _FILE_TYPE_LINK)
	{
		trunkHeader.file_type = FDFS_TRUNK_FILE_TYPE_LINK;
	}
	else
	{
		trunkHeader.file_type = FDFS_TRUNK_FILE_TYPE_REGULAR;
	}
	
	trunkHeader.alloc_size = pFileContext->extra_info.upload.trunk_info.file.size;
	trunkHeader.file_size = pFileContext->end - pFileContext->start;
	trunkHeader.crc32 = pFileContext->crc32;
	trunkHeader.mtime = pFileContext->extra_info.upload.start_time;
	snprintf(trunkHeader.formatted_ext_name, \
		sizeof(trunkHeader.formatted_ext_name), "%s", \
		pFileContext->extra_info.upload.formatted_ext_name);

	if (lseek(pFileContext->fd, pFileContext->start - \
		FDFS_TRUNK_FILE_HEADER_SIZE, SEEK_SET) < 0)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"lseek file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pFileContext->filename, \
			result, STRERROR(result));
		return result;
	}

	trunk_pack_header(&trunkHeader, header);
	/*
	{
	char buff1[256];
	char buff2[256];
	char buff3[1024];
	trunk_header_dump(&trunkHeader, buff3, sizeof(buff3));
	logInfo("file: "__FILE__", line: %d, my trunk=%s, my fields=%s", __LINE__, \
                trunk_info_dump(&pFileContext->extra_info.upload.trunk_info, buff1, sizeof(buff1)), \
                trunk_header_dump(&trunkHeader, buff2, sizeof(buff2)));
	}
	*/

	if (fc_safe_write(pFileContext->fd, header, FDFS_TRUNK_FILE_HEADER_SIZE) != \
		FDFS_TRUNK_FILE_HEADER_SIZE)
	{
		result = errno != 0 ? errno : EIO;
		logError("file: "__FILE__", line: %d, " \
			"write to file: %s fail, " \
			"errno: %d, error info: %s", \
			__LINE__, pFileContext->filename, \
			result, STRERROR(result));
		return result;
	}

	return 0;
}

